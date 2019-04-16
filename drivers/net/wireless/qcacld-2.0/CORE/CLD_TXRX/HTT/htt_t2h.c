/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * @file htt_t2h.c
 * @brief Provide functions to process target->host HTT messages.
 * @details
 *  This file contains functions related to target->host HTT messages.
 *  There are two categories of functions:
 *  1.  A function that receives a HTT message from HTC, and dispatches it
 *      based on the HTT message type.
 *  2.  functions that provide the info elements from specific HTT messages.
 */

#include <htc_api.h>         /* HTC_PACKET */
#include <htt.h>             /* HTT_T2H_MSG_TYPE, etc. */
#include <adf_nbuf.h>        /* adf_nbuf_t */

#include <ol_htt_rx_api.h>
#include <ol_htt_tx_api.h>
#include <ol_txrx_htt_api.h> /* htt_tx_status */

#include <htt_internal.h>    /* HTT_TX_SCHED, etc. */
#include <pktlog_ac_fmt.h>
#include <wdi_event.h>
#include <ol_htt_tx_api.h>
#include <ol_txrx_stats.h>
#include <wdi_event_api.h>
#include <ol_txrx_ctrl_api.h>
#include <ol_txrx_peer_find.h>
#include <ol_ctrl_txrx_api.h>
/*--- target->host HTT message dispatch function ----------------------------*/

#ifndef DEBUG_CREDIT
#define DEBUG_CREDIT 0
#endif

static u_int8_t *
htt_t2h_mac_addr_deswizzle(u_int8_t *tgt_mac_addr, u_int8_t *buffer)
{
#ifdef BIG_ENDIAN_HOST
    /*
     * The host endianness is opposite of the target endianness.
     * To make u_int32_t elements come out correctly, the target->host
     * upload has swizzled the bytes in each u_int32_t element of the
     * message.
     * For byte-array message fields like the MAC address, this
     * upload swizzling puts the bytes in the wrong order, and needs
     * to be undone.
     */
    buffer[0] = tgt_mac_addr[3];
    buffer[1] = tgt_mac_addr[2];
    buffer[2] = tgt_mac_addr[1];
    buffer[3] = tgt_mac_addr[0];
    buffer[4] = tgt_mac_addr[7];
    buffer[5] = tgt_mac_addr[6];
    return buffer;
#else
    /*
     * The host endianness matches the target endianness -
     * we can use the mac addr directly from the message buffer.
     */
    return tgt_mac_addr;
#endif
}

#if defined(CONFIG_HL_SUPPORT)
#define HTT_RX_FRAG_SET_LAST_MSDU(pdev, msg) /* no-op */
#define HTT_FAIL_NOTIFY_BREAK_CHECK(status) \
	((status) == htt_tx_status_fail_notify)
#else
static void HTT_RX_FRAG_SET_LAST_MSDU(
    struct htt_pdev_t *pdev, adf_nbuf_t msg)
{
    u_int32_t *msg_word;
    unsigned num_msdu_bytes;
    adf_nbuf_t msdu;
    struct htt_host_rx_desc_base *rx_desc;
    int start_idx;
    u_int8_t *p_fw_msdu_rx_desc = 0;

    msg_word = (u_int32_t *) adf_nbuf_data(msg);
    num_msdu_bytes = HTT_RX_FRAG_IND_FW_RX_DESC_BYTES_GET(*(msg_word +
               HTT_RX_FRAG_IND_HDR_PREFIX_SIZE32));
    /*
     * 1 word for the message header,
     * 1 word to specify the number of MSDU bytes,
     * 1 word for every 4 MSDU bytes (round up),
     * 1 word for the MPDU range header
     */
    pdev->rx_mpdu_range_offset_words = 3 + ((num_msdu_bytes + 3) >> 2);
    pdev->rx_ind_msdu_byte_idx = 0;

    p_fw_msdu_rx_desc = ((u_int8_t *)(msg_word) +
               HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_FRAG_IND_FW_DESC_BYTE_OFFSET));

    /*
     * Fix for EV126710, in which BSOD occurs due to last_msdu bit
     * not set while the next pointer is deliberately set to NULL
     * before calling ol_rx_pn_check_base()
     *
     * For fragment frames, the HW may not have set the last_msdu bit
     * in the rx descriptor, but the SW expects this flag to be set,
     * since each fragment is in a separate MPDU. Thus, set the flag here,
     * just in case the HW didn't.
     */
    start_idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
    msdu = pdev->rx_ring.buf.netbufs_ring[start_idx];
    adf_nbuf_set_pktlen(msdu, HTT_RX_BUF_SIZE);
    adf_nbuf_unmap(pdev->osdev, msdu, ADF_OS_DMA_FROM_DEVICE);
    rx_desc = htt_rx_desc(msdu);
    *((u_int8_t *) &rx_desc->fw_desc.u.val) = *p_fw_msdu_rx_desc;
    rx_desc->msdu_end.last_msdu = 1;
    adf_nbuf_map(pdev->osdev, msdu, ADF_OS_DMA_FROM_DEVICE);
}

#define HTT_FAIL_NOTIFY_BREAK_CHECK(status)  0
#endif /* CONFIG_HL_SUPPORT */

#define MAX_TARGET_TX_CREDIT    204800

#define CFR_MAGIC_NUM_HEAD      0xDEADBEAF
#define CFR_MAGIC_NUM_TAIL      0xBEAFDEAD

static void
htt_populate_n_relay_cfr_data(struct htt_cfr_dump_ind_type_1 *cfr_ind,
                           struct htt_rfs_cfr_dump *rfs_cfr_dump)
{
    int i,j;
    u32 *cfr_data;
    u32 msg_len = __le32_to_cpu(cfr_ind->length);

    cfr_data= (u32*)((void *)cfr_ind + sizeof(struct htt_cfr_dump_ind_type_1));

    if (msg_len >  sizeof(rfs_cfr_dump->cfr_dump))
        msg_len = sizeof(rfs_cfr_dump->cfr_dump);

    for (i=0,j=0;i < msg_len/4; i++,j+=2) {
        rfs_cfr_dump->cfr_dump[j] = (u16)(cfr_data[i] & 0xFFFF);
        rfs_cfr_dump->cfr_dump[j+1] = (u16)((cfr_data[i] >> 16) & 0xFFFF);
#ifdef CFR_DATA_DBG
        adf_os_print("%d 0x%4x %d 0x%4x\n",j, rfs_cfr_dump->cfr_dump[j], j+1,
                     rfs_cfr_dump->cfr_dump[j+1]);
#endif
    }
#ifdef WLAN_OPEN_SOURCE
    if (msg_len)
        cfr_dump_to_rfs(rfs_cfr_dump->cfr_dump, msg_len);
#endif /* WLAN_OPEN_SOURCE */
}

static void
htt_populate_rfs_cfr_header(struct htt_rfs_cfr_hdr *cfr_hdr,
                            struct htt_cfr_dump_ind_type_1 *cfr_ind)
{
    cfr_hdr->head_magic_num = CFR_MAGIC_NUM_HEAD;
    memcpy(&cfr_hdr->mac_addr, &cfr_ind->mac_addr, HTT_MAC_ADDR_LEN);
    cfr_hdr->status = cfr_ind->status;
    cfr_hdr->capture_bw = cfr_ind->capture_bw;
    cfr_hdr->channel_bw = cfr_ind->channel_bw;
    cfr_hdr->capture_mode = cfr_ind->mode;
    cfr_hdr->capture_type = cfr_ind->cap_type;
    cfr_hdr->sts_count = cfr_ind->sts_count;

    cfr_hdr->prim20_chan = __le32_to_cpu(cfr_ind->chan.chan_mhz);
    cfr_hdr->center_freq1 =  __le32_to_cpu(cfr_ind->chan.band_center_freq1);
    cfr_hdr->center_freq2 =  __le32_to_cpu(cfr_ind->chan.band_center_freq2);
    cfr_hdr->phy_mode = __le32_to_cpu(cfr_ind->chan.chan_mode);

    cfr_hdr->num_rx_chain = 0; /* TODO */

    cfr_hdr->length = __le32_to_cpu(cfr_ind->length);

    cfr_hdr->timestamp = __le32_to_cpu(cfr_ind->timestamp);

#ifdef WLAN_OPEN_SOURCE
    cfr_dump_to_rfs(cfr_hdr, sizeof(struct htt_rfs_cfr_hdr));
#endif /* WLAN_OPEN_SOURCE */
}

static void htt_peer_cfr_compl_ind(u_int32_t *data)
{
    struct htt_rfs_cfr_dump rfs_cfr_dump = { };
    enum htt_cfr_capture_msg_type cfr_msg_type;
    struct htt_cfr_dump_compl_ind *cfr_dump_ind;
    u32 msg_len = 0;

    cfr_dump_ind = (struct htt_cfr_dump_compl_ind *)data;

    cfr_msg_type = __le32_to_cpu(cfr_dump_ind->msg_type);

    switch (cfr_msg_type) {
    case HTT_PEER_CFR_CAPTURE_MSG_TYPE_LEGACY:
        {
            msg_len = __le32_to_cpu(cfr_dump_ind->cfr_dump_legacy.length);
            /* Skip if msg_len is 0 */
            if (!msg_len)
                break;

#ifdef CFR_DATA_DBG
            /* Discard if no of tones doesnt belong to 20MHz BW */
            if (msg_len != (54*4)) {
                if(msg_len)
                    adf_os_print("WARN: Currently supported 53 tones "
                                 "in 20MHz got %d\n", msg_len/4);
            }
#endif
            htt_populate_rfs_cfr_header(&rfs_cfr_dump.cfr_hdr,
                                        &cfr_dump_ind->cfr_dump_legacy);

            htt_populate_n_relay_cfr_data(&cfr_dump_ind->cfr_dump_legacy,
                                          &rfs_cfr_dump);

            rfs_cfr_dump.tail_magic_num = CFR_MAGIC_NUM_TAIL;

#ifdef WLAN_OPEN_SOURCE
            cfr_dump_to_rfs(&rfs_cfr_dump.tail_magic_num, sizeof(u32));

            cfr_finalalize_relay();
#endif /* WLAN_OPEN_SOURCE */
            break;
        }
    default:
        adf_os_print("Unsupported CFR capture method %d\n",cfr_msg_type);
        break;
    }
}

/* Target to host Msg/event  handler  for low priority messages*/
void
htt_t2h_lp_msg_handler(void *context, adf_nbuf_t htt_t2h_msg )
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;
    u_int32_t payload_present;

    msg_word = (u_int32_t *) adf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    payload_present = HTT_T2H_PAYLOAD_PRESENT_GET(*msg_word);

    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_VERSION_CONF:
        {
            htc_pm_runtime_put(pdev->htc_pdev);
            pdev->tgt_ver.major = HTT_VER_CONF_MAJOR_GET(*msg_word);
            pdev->tgt_ver.minor = HTT_VER_CONF_MINOR_GET(*msg_word);
            adf_os_print("target uses HTT version %d.%d; host uses %d.%d\n",
                pdev->tgt_ver.major, pdev->tgt_ver.minor,
                HTT_CURRENT_VERSION_MAJOR, HTT_CURRENT_VERSION_MINOR);
            if (pdev->tgt_ver.major != HTT_CURRENT_VERSION_MAJOR) {
                adf_os_print("*** Incompatible host/target HTT versions!\n");
            }
            /* abort if the target is incompatible with the host */
            adf_os_assert(pdev->tgt_ver.major == HTT_CURRENT_VERSION_MAJOR);
            if (pdev->tgt_ver.minor != HTT_CURRENT_VERSION_MINOR) {
                adf_os_print(
                    "*** Warning: host/target HTT versions are different, "
                    "though compatible!\n");
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_FLUSH:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int16_t seq_num_start, seq_num_end;
            enum htt_rx_flush_action action;

            peer_id = HTT_RX_FLUSH_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FLUSH_TID_GET(*msg_word);
            seq_num_start = HTT_RX_FLUSH_SEQ_NUM_START_GET(*(msg_word+1));
            seq_num_end = HTT_RX_FLUSH_SEQ_NUM_END_GET(*(msg_word+1));
            action =
                HTT_RX_FLUSH_MPDU_STATUS_GET(*(msg_word+1)) == 1 ?
                htt_rx_flush_release : htt_rx_flush_discard;
            ol_rx_flush_handler(
                pdev->txrx_pdev,
                peer_id, tid,
                seq_num_start,
                seq_num_end,
                action);
            break;
        }
    case  HTT_T2H_MSG_TYPE_RX_OFFLOAD_DELIVER_IND:
        {
            u_int16_t msdu_cnt;
            msdu_cnt = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_GET(*msg_word);
            ol_rx_offload_deliver_ind_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                msdu_cnt);
            if (pdev->cfg.is_high_latency) {
                /* return here for HL to avoid double free on htt_t2h_msg */
                return;
            } else {
                break;
            }
        }
    case  HTT_T2H_MSG_TYPE_RX_FRAG_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_FRAG_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FRAG_IND_EXT_TID_GET(*msg_word);
            HTT_RX_FRAG_SET_LAST_MSDU(pdev, htt_t2h_msg);

            /* If packet len is invalid, will discard this frame. */
            if (pdev->cfg.is_high_latency) {
                u_int32_t rx_pkt_len = 0;

                rx_pkt_len = adf_nbuf_len(htt_t2h_msg);

                if (rx_pkt_len < (HTT_RX_FRAG_IND_BYTES +
                    sizeof(struct hl_htt_rx_ind_base)+
                    sizeof(struct ieee80211_frame))) {

                    adf_os_print("%s: invalid packet len, %u\n",
                                __FUNCTION__,
                                rx_pkt_len);
                    /*
                     * This buf will be freed before
                     * exiting this function.
                     */
                    break;
                }
            }

            ol_rx_frag_indication_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                peer_id,
                tid);

            if (pdev->cfg.is_high_latency) {
               /*
                * For high latency solution, HTT_T2H_MSG_TYPE_RX_FRAG_IND
                * message and RX packet share the same buffer. All buffer will
                * be freed by ol_rx_frag_indication_handler or upper layer to
                * avoid double free issue.
                *
                */
                return;
            }

            break;
        }
    case HTT_T2H_MSG_TYPE_RX_ADDBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t win_sz;
            u_int16_t start_seq_num;

            /*
             * FOR NOW, the host doesn't need to know the initial
             * sequence number for rx aggregation.
             * Thus, any value will do - specify 0.
             */
            start_seq_num = 0;
            peer_id = HTT_RX_ADDBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_ADDBA_TID_GET(*msg_word);
            win_sz = HTT_RX_ADDBA_WIN_SIZE_GET(*msg_word);
            ol_rx_addba_handler(
                pdev->txrx_pdev, peer_id, tid, win_sz, start_seq_num,
                0 /* success */);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_DELBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_DELBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_DELBA_TID_GET(*msg_word);
            ol_rx_delba_handler(pdev->txrx_pdev, peer_id, tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_MAP:
        {
            u_int8_t mac_addr_deswizzle_buf[HTT_MAC_ADDR_LEN];
            u_int8_t *peer_mac_addr;
            u_int16_t peer_id;
            u_int8_t vdev_id;

            peer_id = HTT_RX_PEER_MAP_PEER_ID_GET(*msg_word);
            vdev_id = HTT_RX_PEER_MAP_VDEV_ID_GET(*msg_word);
            peer_mac_addr = htt_t2h_mac_addr_deswizzle(
                (u_int8_t *) (msg_word+1), &mac_addr_deswizzle_buf[0]);

            if (peer_id > ol_cfg_max_peer_id(pdev->ctrl_pdev)) {
                adf_os_print("%s: HTT_T2H_MSG_TYPE_PEER_MAP,"
                            "invalid peer_id, %u\n",
                            __FUNCTION__,
                            peer_id);
                break;
            }

            ol_rx_peer_map_handler(
                pdev->txrx_pdev, peer_id, vdev_id, peer_mac_addr, 1/*can tx*/);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_UNMAP:
        {
            u_int16_t peer_id;
            peer_id = HTT_RX_PEER_UNMAP_PEER_ID_GET(*msg_word);

            if (peer_id > ol_cfg_max_peer_id(pdev->ctrl_pdev)) {
                adf_os_print("%s: HTT_T2H_MSG_TYPE_PEER_UNMAP,"
                            "invalid peer_id, %u\n",
                            __FUNCTION__,
                            peer_id);
                break;
            }

            ol_rx_peer_unmap_handler(pdev->txrx_pdev, peer_id);
            break;
        }
    case HTT_T2H_MSG_TYPE_SEC_IND:
        {
            u_int16_t peer_id;
            enum htt_sec_type sec_type;
            int is_unicast;

            peer_id = HTT_SEC_IND_PEER_ID_GET(*msg_word);
            sec_type = HTT_SEC_IND_SEC_TYPE_GET(*msg_word);
            is_unicast = HTT_SEC_IND_UNICAST_GET(*msg_word);
            msg_word++; /* point to the first part of the Michael key */
            ol_rx_sec_ind_handler(
                pdev->txrx_pdev, peer_id, sec_type, is_unicast, msg_word, msg_word+2);
            break;
        }
    case HTT_T2H_MSG_TYPE_MGMT_TX_COMPL_IND:
        {
            struct htt_mgmt_tx_compl_ind *compl_msg;
            int32_t credit_delta = 1;

            compl_msg = (struct htt_mgmt_tx_compl_ind *)(msg_word + 1);

            if (pdev->cfg.is_high_latency) {
                if (!pdev->cfg.default_tx_comp_req) {
                    HTT_TX_MUTEX_ACQUIRE(&pdev->credit_mutex);
                    adf_os_atomic_add(credit_delta,
                                      &pdev->htt_tx_credit.target_delta);
                    credit_delta = htt_tx_credit_update(pdev);
                    HTT_TX_MUTEX_RELEASE(&pdev->credit_mutex);
                }
                if (credit_delta) {
                    ol_tx_target_credit_update(pdev->txrx_pdev, credit_delta);
                }
            }
            OL_TX_DESC_UPDATE_GROUP_CREDIT(
                pdev->txrx_pdev, compl_msg->desc_id, 1, 0, compl_msg->status);
            ol_tx_single_completion_handler(
                pdev->txrx_pdev, compl_msg->status, compl_msg->desc_id);
            htc_pm_runtime_put(pdev->htc_pdev);
            HTT_TX_SCHED(pdev);
            break;
        }
#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
    case HTT_T2H_MSG_TYPE_STATS_CONF:
        {
            u_int64_t cookie;
            u_int8_t *stats_info_list;

            cookie = *(msg_word + 1);
            cookie |= ((u_int64_t) (*(msg_word + 2))) << 32;

            stats_info_list = (u_int8_t *) (msg_word + 3);
            htc_pm_runtime_put(pdev->htc_pdev);
            ol_txrx_fw_stats_handler(pdev->txrx_pdev, cookie, stats_info_list);
            break;
        }
#endif
#ifndef REMOVE_PKT_LOG
    case HTT_T2H_MSG_TYPE_PKTLOG:
        {
            u_int32_t *pl_hdr;
            u_int32_t log_type;
            uint32_t len = adf_nbuf_len(htt_t2h_msg);
            struct ol_fw_data pl_fw_data;

            pl_hdr = (msg_word + 1);
            pl_fw_data.data = pl_hdr;
            pl_fw_data.len = len - sizeof(*msg_word);

            log_type = (*(pl_hdr + 1) & ATH_PKTLOG_HDR_LOG_TYPE_MASK) >>
                                            ATH_PKTLOG_HDR_LOG_TYPE_SHIFT;
            if (log_type == PKTLOG_TYPE_TX_CTRL ||
               (log_type) == PKTLOG_TYPE_TX_STAT ||
               (log_type) == PKTLOG_TYPE_TX_MSDU_ID ||
               (log_type) == PKTLOG_TYPE_TX_FRM_HDR ||
               (log_type) == PKTLOG_TYPE_TX_VIRT_ADDR) {
                wdi_event_handler(WDI_EVENT_TX_STATUS, pdev->txrx_pdev, &pl_fw_data);
            } else if ((log_type) == PKTLOG_TYPE_RC_FIND) {
                wdi_event_handler(WDI_EVENT_RATE_FIND, pdev->txrx_pdev, &pl_fw_data);
            } else if ((log_type) == PKTLOG_TYPE_RC_UPDATE) {
                wdi_event_handler(
                    WDI_EVENT_RATE_UPDATE, pdev->txrx_pdev, &pl_fw_data);
            } else if ((log_type) == PKTLOG_TYPE_RX_STAT) {
                wdi_event_handler(WDI_EVENT_RX_DESC, pdev->txrx_pdev, &pl_fw_data);
            }
            break;
        }
#endif
    case HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND:
    {
        u_int32_t htt_credit_delta_abs;
        int32_t htt_credit_delta;
        int sign, old_credit;
        int delta2 = 0;

        htt_credit_delta_abs = HTT_TX_CREDIT_DELTA_ABS_GET(*msg_word);
        sign = HTT_TX_CREDIT_SIGN_BIT_GET(*msg_word) ? -1 : 1;
        htt_credit_delta = sign * htt_credit_delta_abs;

        old_credit = adf_os_atomic_read(&pdev->htt_tx_credit.target_delta);
        if (((old_credit + htt_credit_delta) > MAX_TARGET_TX_CREDIT) ||
            ((old_credit + htt_credit_delta) < -MAX_TARGET_TX_CREDIT)) {
            adf_os_print("%s: invalid credit update,old_credit=%d,"
                        "htt_credit_delta=%d\n",
                        __FUNCTION__,
                        old_credit,
                        htt_credit_delta);
            break;
        }

        if (pdev->cfg.is_high_latency &&
            !pdev->cfg.default_tx_comp_req) {
            HTT_TX_MUTEX_ACQUIRE(&pdev->credit_mutex);
            adf_os_atomic_add(htt_credit_delta,
                              &pdev->htt_tx_credit.target_delta);
            htt_credit_delta = htt_tx_credit_update(pdev);


            if (htt_credit_delta >= 0) {
                if ((adf_os_atomic_read(&pdev->txrx_pdev->target_tx_credit) +
                    htt_credit_delta) < HTT_MAX_BUS_CREDIT) {
                    if (adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) > 0) {
                        delta2 = HTT_MAX_BUS_CREDIT -
                                adf_os_atomic_read(&pdev->txrx_pdev->target_tx_credit)
                                - htt_credit_delta;
                        delta2 = (adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) < delta2) ?
                                adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) : delta2;

                        adf_os_atomic_add(-delta2, &pdev->htt_tx_credit.target_delta);
                        adf_os_atomic_add(delta2, &pdev->txrx_pdev->target_tx_credit);
                        adf_os_atomic_add(-delta2, &pdev->htt_tx_credit.bus_delta);
                    }
                }
            } else {
                if (adf_os_atomic_read(&pdev->txrx_pdev->target_tx_credit) < HTT_MAX_BUS_CREDIT) {
                    if (adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) > 0) {
                        delta2 = HTT_MAX_BUS_CREDIT -
                                adf_os_atomic_read(&pdev->txrx_pdev->target_tx_credit);
                        delta2 = (adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) < delta2) ?
                                adf_os_atomic_read(&pdev->htt_tx_credit.target_delta) : delta2;

                        adf_os_atomic_add(-delta2, &pdev->htt_tx_credit.target_delta);
                        adf_os_atomic_add(delta2, &pdev->txrx_pdev->target_tx_credit);
                        adf_os_atomic_add(-delta2, &pdev->htt_tx_credit.bus_delta);
                    }
                }
            }


            HTT_TX_MUTEX_RELEASE(&pdev->credit_mutex);
        }

        HTT_TX_GROUP_CREDIT_PROCESS(pdev, msg_word);
        /*
         * Call ol_tx_credit_completion even if htt_credit_delta is zero,
         * in case there is some global credit already available, but the
         * above group credit updates have removed credit restrictions,
         * possibly allowing the download scheduler to perform a download
         * even if htt_credit_delta == 0.
         */
        ol_tx_credit_completion_handler(pdev->txrx_pdev, htt_credit_delta);
        break;
    }

#ifdef IPA_UC_OFFLOAD
    case HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE:
        {
            u_int8_t op_code;
            u_int16_t len;
            u_int8_t *op_msg_buffer;
            u_int8_t *msg_start_ptr;

            htc_pm_runtime_put(pdev->htc_pdev);
            msg_start_ptr = (u_int8_t *)msg_word;
            op_code = HTT_WDI_IPA_OP_RESPONSE_OP_CODE_GET(*msg_word);
            msg_word++;
            len = HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_GET(*msg_word);

            op_msg_buffer = adf_os_mem_alloc(NULL,
                sizeof(struct htt_wdi_ipa_op_response_t) + len);
            if (!op_msg_buffer) {
                adf_os_print("OPCODE messsage buffer alloc fail");
                break;
            }
            adf_os_mem_copy(op_msg_buffer,
                    msg_start_ptr,
                    sizeof(struct htt_wdi_ipa_op_response_t) + len);
            ol_txrx_ipa_uc_op_response(pdev->txrx_pdev, op_msg_buffer);
            break;
        }
#endif /* IPA_UC_OFFLOAD */
    case HTT_T2H_MSG_TYPE_RX_OFLD_PKT_ERR:
    {
        switch (HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_GET(*msg_word)) {
            case HTT_RX_OFLD_PKT_ERR_TYPE_MIC_ERR:
            {
                struct ol_error_info err_info;
                struct ol_txrx_vdev_t *vdev;
                struct ol_txrx_peer_t *peer;
                u_int8_t * pn_ptr;
                u_int16_t peer_id =
                     HTT_RX_OFLD_PKT_ERR_MIC_ERR_PEER_ID_GET(*(msg_word + 1));

                peer = ol_txrx_peer_find_by_id(pdev->txrx_pdev, peer_id);
                if (!peer) {
                    adf_os_print("%s: invalid peer id %d\n", __FUNCTION__,
                                  peer_id);
                    break;
                }
                vdev = peer->vdev;

                err_info.u.mic_err.vdev_id = vdev->vdev_id;
                err_info.u.mic_err.key_id =
                     HTT_RX_OFLD_PKT_ERR_MIC_ERR_KEYID_GET(*(msg_word + 1));
                adf_os_mem_copy(err_info.u.mic_err.da,
                                (u_int8_t *)(msg_word + 2),
                                 OL_TXRX_MAC_ADDR_LEN);
                adf_os_mem_copy(err_info.u.mic_err.sa,
                                (u_int8_t *)(msg_word + 4),
                                 OL_TXRX_MAC_ADDR_LEN);
                adf_os_mem_copy(err_info.u.mic_err.ta,
                                peer->mac_addr.raw, OL_TXRX_MAC_ADDR_LEN);

                pn_ptr = (u_int8_t *)&err_info.u.mic_err.pn;
                adf_os_mem_copy(pn_ptr, (u_int8_t *)(msg_word + 6), 4);
                adf_os_mem_copy((pn_ptr + 4), (u_int8_t *)(msg_word + 7), 2);

                ol_indicate_err(OL_RX_ERR_TKIP_MIC, &err_info);
                break;
            }

            default:
            {
                adf_os_print("%s: unhandled error type %d\n", __FUNCTION__,
                             HTT_RX_OFLD_PKT_ERR_MSG_SUB_TYPE_GET(*msg_word));
                break;
            }
        }
        break;
    }
    case HTT_T2H_MSG_TYPE_RATE_REPORT:
        {
            u_int16_t peer_cnt = HTT_PEER_RATE_REPORT_MSG_PEER_COUNT_GET(*msg_word);
            u_int16_t i;
            struct rate_report_t *report, *each;
            int max_peers;

            /* Param sanity check */
            if (peer_cnt == 0) {
                adf_os_print("RATE REPORT messsage peer_cnt is 0! \n");
                break;
            }

            max_peers = ol_cfg_max_peer_id(pdev->ctrl_pdev) + 1;
            if (peer_cnt > max_peers) {
                adf_os_print("RATE REPORT msg peer_cnt is larger than %d\n",
                    max_peers);
                break;
            }

            /* At least one peer and no limit apply to peer_cnt here */
            report = adf_os_mem_alloc(NULL,
                sizeof(struct rate_report_t) * peer_cnt);
            if (!report) {
                adf_os_print("RATE REPORT messsage buffer alloc fail. peer_cnt %d\n",
                                peer_cnt);
                break;
            }

            each = report;
            msg_word++; /* point to the payload */
            for (i = 0; i < peer_cnt; i++) {
                each->id  =
                    HTT_PEER_RATE_REPORT_MSG_PEER_ID_GET(*(msg_word + i*2));
                each->phy =
                    HTT_PEER_RATE_REPORT_MSG_PHY_GET(*(msg_word + i*2));
                each->rate = *(msg_word + i*2 + 1);
                each++;
            }
            ol_txrx_peer_link_status_handler(pdev->txrx_pdev, peer_cnt, report);

            adf_os_mem_free(report);
            break;
        }
    case HTT_T2H_MSG_TYPE_CFR_DUMP_COMPL_IND:
        {
#ifdef CFR_DATA_DBG
            int i;
            struct htt_cfr_dump_ind_type_1 *cfr_dump;
#endif
            if (!payload_present)
                return;
            msg_word++;
            htt_peer_cfr_compl_ind(msg_word);
#ifdef CFR_DATA_DBG
            msg_word++;
            cfr_dump = (struct htt_cfr_dump_ind_type_1 *) msg_word;
            adf_os_print("CFR Dump: Length=%d Counter=%d  Mode=%d BW=%d\n",
                          cfr_dump->length, cfr_dump->counter, cfr_dump->mode,
                          cfr_dump->capture_bw);
            msg_word += sizeof(struct htt_cfr_dump_ind_type_1)/4;
            for (i=0; i<cfr_dump->length/4; i++) {
                 adf_os_print("\n%d 0x%04x ",i, (u16)(*(msg_word+i) & 0xFFFF));
                 adf_os_print("0x%04x ", (u16)((*(msg_word+i) >>16) & 0xFFFF));
            }
            adf_os_print("\n");
#endif
        }
    default:
        break;
    };
    /* Free the indication buffer */
    adf_nbuf_free(htt_t2h_msg);
}

/* Generic Target to host Msg/event  handler  for low priority messages
  Low priority message are handler in a different handler called from
  this function . So that the most likely succes path like Rx and
  Tx comp   has little code   foot print
 */
void
htt_t2h_msg_handler(void *context, HTC_PACKET *pkt)
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    adf_nbuf_t htt_t2h_msg = (adf_nbuf_t) pkt->pPktContext;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;

    /* check for successful message reception */
    if (pkt->Status != A_OK) {
        if (pkt->Status != A_ECANCELED) {
            pdev->stats.htc_err_cnt++;
        }
        adf_nbuf_free(htt_t2h_msg);
        return;
    }

#ifdef HTT_RX_RESTORE
if (adf_os_unlikely(pdev->rx_ring.rx_reset)) {
        adf_os_print("rx restore ..\n");
        adf_nbuf_free(htt_t2h_msg);
        return;
    }
#endif

    /* confirm alignment */
    HTT_ASSERT3((((unsigned long) adf_nbuf_data(htt_t2h_msg)) & 0x3) == 0);

    msg_word = (u_int32_t *) adf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_RX_IND:
        {
            unsigned num_mpdu_ranges;
            unsigned num_msdu_bytes;
            u_int16_t peer_id;
            u_int8_t tid;

            if (adf_os_unlikely(pdev->cfg.is_full_reorder_offload)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IND not supported with full "
                             "reorder offload\n");
                break;
            }
            peer_id = HTT_RX_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_IND_EXT_TID_GET(*msg_word);

            if (tid >= OL_TXRX_NUM_EXT_TIDS) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IND, invalid tid %d\n", tid);
                break;
            }

            num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(
                *(msg_word + 2 + HTT_RX_PPDU_DESC_SIZE32));
            /*
             * 1 word for the message header,
             * HTT_RX_PPDU_DESC_SIZE32 words for the FW rx PPDU desc
             * 1 word to specify the number of MSDU bytes,
             * 1 word for every 4 MSDU bytes (round up),
             * 1 word for the MPDU range header
             */
            pdev->rx_mpdu_range_offset_words =
                (HTT_RX_IND_HDR_BYTES + num_msdu_bytes + 3) >> 2;
            num_mpdu_ranges = HTT_RX_IND_NUM_MPDU_RANGES_GET(*(msg_word + 1));
            pdev->rx_ind_msdu_byte_idx = 0;

            if (pdev->cfg.is_high_latency) {
                /*
                 * TODO: remove copy after stopping reuse skb on HIF layer
                 * because SDIO HIF may reuse skb before upper layer release it
                 */
                if (VOS_MONITOR_MODE == vos_get_conparam())
                    ol_rx_mon_indication_handler(
                            pdev->txrx_pdev, htt_t2h_msg, peer_id, tid,
                            num_mpdu_ranges);
                else
                    ol_rx_indication_handler(
                            pdev->txrx_pdev, htt_t2h_msg, peer_id, tid,
                            num_mpdu_ranges);

                return;
            } else {
                ol_rx_indication_handler(
                    pdev->txrx_pdev, htt_t2h_msg, peer_id, tid,
                    num_mpdu_ranges);
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_TX_COMPL_IND:
        {
            int old_credit;
            int num_msdus;
            enum htt_tx_status status;
            int msg_len = adf_nbuf_len(htt_t2h_msg);

            /* status - no enum translation needed */
            status = HTT_TX_COMPL_IND_STATUS_GET(*msg_word);
            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);

            /*
             * each desc id will occupy 2 bytes.
             * the 4 is for htt msg header
             */
            if ((num_msdus * HTT_TX_COMPL_BYTES_PER_MSDU_ID +
                HTT_TX_COMPL_HEAD_SZ) > msg_len) {
                adf_os_print("%s: num_msdus(%d) is invalid,"
                            "adf_nbuf_len = %d\n",
                            __FUNCTION__,
                            num_msdus,
                            msg_len);
                break;
            }

            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }

            /* Indicate failure status to user space */
            ol_tx_failure_indication(pdev->txrx_pdev,
                                     HTT_TX_COMPL_IND_TID_GET(*msg_word),
                                     num_msdus, status);

            if (pdev->cfg.is_high_latency) {
                /*
                 * For regular frms in HL case, frms have already been
                 * freed and tx credit has been updated. FW indicates
                 * special message for failure MSDUs with status type
                 * htt_tx_status_fail_notify. Once such message was
                 * received, just break here.
                 */
                if (ol_cfg_tx_free_at_download(pdev->ctrl_pdev) &&
                    HTT_FAIL_NOTIFY_BREAK_CHECK(status)) {
                    adf_os_print("HTT TX COMPL for failed data frm.\n");
                    break;
                }

                old_credit = adf_os_atomic_read(&pdev->htt_tx_credit.target_delta);
                if (((old_credit + num_msdus) > MAX_TARGET_TX_CREDIT) ||
                    ((old_credit + num_msdus) < -MAX_TARGET_TX_CREDIT)) {
                    adf_os_print("%s: invalid credit update,old_credit=%d,"
                                "num_msdus=%d\n",
                                __FUNCTION__,
                                old_credit,
                                num_msdus);
                } else {
                    if (!pdev->cfg.default_tx_comp_req) {
                        int credit_delta;
                        HTT_TX_MUTEX_ACQUIRE(&pdev->credit_mutex);
                        adf_os_atomic_add(num_msdus,
                            &pdev->htt_tx_credit.target_delta);
                        credit_delta = htt_tx_credit_update(pdev);
                        HTT_TX_MUTEX_RELEASE(&pdev->credit_mutex);
                        if (credit_delta) {
                            ol_tx_target_credit_update(pdev->txrx_pdev,
                                                       credit_delta);
                        }
                    } else {
                        ol_tx_target_credit_update(pdev->txrx_pdev,
                                                   num_msdus);
                    }
                }
            }
            ol_tx_completion_handler(
                pdev->txrx_pdev, num_msdus, status, msg_word);
            HTT_TX_SCHED(pdev);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_PN_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid, pn_ie_cnt, *pn_ie=NULL;
            u_int16_t seq_num_start, seq_num_end;

            /*First dword */
            peer_id = HTT_RX_PN_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_PN_IND_EXT_TID_GET(*msg_word);

            msg_word++;
            /*Second dword */
            seq_num_start = HTT_RX_PN_IND_SEQ_NUM_START_GET(*msg_word);
            seq_num_end = HTT_RX_PN_IND_SEQ_NUM_END_GET(*msg_word);
            pn_ie_cnt = HTT_RX_PN_IND_PN_IE_CNT_GET(*msg_word);

            msg_word++;
            /*Third dword*/
            if (pn_ie_cnt) {
                pn_ie = (u_int8_t *)msg_word;
            }

            ol_rx_pn_ind_handler(
                pdev->txrx_pdev, peer_id, tid, seq_num_start, seq_num_end,
                pn_ie_cnt, pn_ie);

            break;
        }
    case HTT_T2H_MSG_TYPE_TX_INSPECT_IND:
        {
            int num_msdus;
            int msg_len = adf_nbuf_len(htt_t2h_msg);

            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
            /*
             * each desc id will occupy 2 bytes.
             * the 4 is for htt msg header
             */
            if ((num_msdus * HTT_TX_COMPL_BYTES_PER_MSDU_ID +
                HTT_TX_COMPL_HEAD_SZ) > msg_len) {
                adf_os_print("%s: num_msdus(%d) is invalid,"
                            "adf_nbuf_len = %d,inspect\n",
                            __FUNCTION__,
                            num_msdus,
                            msg_len);
                break;
            }

            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }
            ol_tx_inspect_handler(pdev->txrx_pdev, num_msdus, msg_word + 1);
            HTT_TX_SCHED(pdev);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t offload_ind, frag_ind;

            if (adf_os_unlikely(!pdev->cfg.is_full_reorder_offload)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND not supported"
                             " when full reorder offload is disabled\n");
                break;
            }

            if (adf_os_unlikely(pdev->cfg.is_high_latency)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND not supported"
                             " on high latency\n");
                break;
            }

            peer_id = HTT_RX_IN_ORD_PADDR_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_IN_ORD_PADDR_IND_EXT_TID_GET(*msg_word);
            offload_ind = HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_GET(*msg_word);
            frag_ind = HTT_RX_IN_ORD_PADDR_IND_FRAG_GET(*msg_word);

            if (adf_os_unlikely(frag_ind)) {
                ol_rx_frag_indication_handler(pdev->txrx_pdev, htt_t2h_msg,
                                               peer_id, tid);
                break;
            }

            ol_rx_in_order_indication_handler(pdev->txrx_pdev, htt_t2h_msg,
                                               peer_id, tid, offload_ind);
            break;
     }
    case HTT_T2H_MSG_TYPE_MONITOR_MAC_HEADER_IND:
        {
            ol_rx_mon_mac_header_handler(pdev->txrx_pdev, htt_t2h_msg);
            break;
        }

    default:
        htt_t2h_lp_msg_handler(context, htt_t2h_msg);
        return ;

    };

    /* Free the indication buffer */
    adf_nbuf_free(htt_t2h_msg);
}

/*--- target->host HTT message Info Element access methods ------------------*/

/*--- tx completion message ---*/

u_int16_t
htt_tx_compl_desc_id(void *iterator, int num)
{
    /*
     * The MSDU IDs are packed , 2 per 32-bit word.
     * Iterate on them as an array of 16-bit elements.
     * This will work fine if the host endianness matches
     * the target endianness.
     * If the host endianness is opposite of the target's,
     * this iterator will produce descriptor IDs in a different
     * order than the target inserted them into the message -
     * if the target puts in [0, 1, 2, 3, ...] the host will
     * put out [1, 0, 3, 2, ...].
     * This is fine, except for the last ID if there are an
     * odd number of IDs.  But the TX_COMPL_IND handling code
     * in the htt_t2h_msg_handler already added a duplicate
     * of the final ID, if there were an odd number of IDs,
     * so this function can safely treat the IDs as an array
     * of 16-bit elements.
     */
    return *(((u_int16_t *) iterator) + num);
}

/*--- rx indication message ---*/

int
htt_rx_ind_flush(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_FLUSH_VALID_GET(*msg_word);
}

void
htt_rx_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}

int
htt_rx_ind_release(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_REL_VALID_GET(*msg_word);
}

void
htt_rx_ind_release_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_REL_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_REL_SEQ_NUM_END_GET(*msg_word);
}

void
htt_rx_ind_mpdu_range_info(
    struct htt_pdev_t *pdev,
    adf_nbuf_t rx_ind_msg,
    int mpdu_range_num,
    enum htt_rx_status *status,
    int *mpdu_count)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word += pdev->rx_mpdu_range_offset_words + mpdu_range_num;
    *status = HTT_RX_IND_MPDU_STATUS_GET(*msg_word);
    *mpdu_count = HTT_RX_IND_MPDU_COUNT_GET(*msg_word);
}

/**
 * htt_rx_ind_rssi_dbm() - Return the RSSI provided in a rx indication message.
 *
 * @pdev:       the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 *
 * Return the RSSI from an rx indication message in dBm units.
 *
 * Return: RSSI in dBm, or HTT_INVALID_RSSI
 */
int16_t
htt_rx_ind_rssi_dbm(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    int8_t rssi;
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_START_VALID_GET(*msg_word)) {
        return HTT_RSSI_INVALID;
    }

    rssi = HTT_RX_IND_RSSI_CMB_GET(*msg_word);
    return (HTT_TGT_RSSI_INVALID == rssi) ?
        HTT_RSSI_INVALID :
        rssi;
}

/**
 * htt_rx_ind_rssi_dbm_chain() - Return the RSSI for a chain provided in a rx
 *              indication message.
 * @pdev:       the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 * @chain:      the index of the chain (0-4)
 *
 * Return the RSSI for a chain from an rx indication message in dBm units.
 *
 * Return: RSSI in dBm, or HTT_INVALID_RSSI
 */
int16_t
htt_rx_ind_rssi_dbm_chain(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
                          int8_t chain)
{
    int8_t rssi;
    u_int32_t *msg_word;

    if (chain < 0 || chain > 3) {
        return HTT_RSSI_INVALID;
    }

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_START_VALID_GET(*msg_word)) {
        return HTT_RSSI_INVALID;
    }

    msg_word += 1 + chain;

    rssi = HTT_RX_IND_RSSI_PRI20_GET(*msg_word);
    return (HTT_TGT_RSSI_INVALID == rssi) ?
        HTT_RSSI_INVALID :
        rssi;
}

/**
 * htt_rx_ind_noise_floor_chain() - Return the nosie floor for a chain
 *              provided in a rx indication message.
 * @pdev:       the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 * @chain:      the index of the chain (0-1) for DSRC
 *
 * Return the noise floor for a chain from an rx indication message.
 *
 * Return: noise floor, or HTT_NOISE_FLOOR_INVALID
 */
int8_t
htt_rx_ind_noise_floor_chain(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
			     int8_t chain)
{
	int8_t noise_floor;
	u_int32_t *msg_word;

	/* only chain0/1 used with 11p DSRC */
	if (chain < 0 || chain > 1) {
		return HTT_NOISE_FLOOR_INVALID;
	}

	msg_word = (u_int32_t *)
		(adf_nbuf_data(rx_ind_msg) +
		 HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

	/* check if the RX_IND message contains valid rx PPDU start info */
	if (!HTT_RX_IND_START_VALID_GET(*msg_word)) {
		return HTT_NOISE_FLOOR_INVALID;
	}

	msg_word = (u_int32_t *)
		(adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_HDR_SUFFIX_BYTE_OFFSET);

	if (chain == 0)
		noise_floor = HTT_RX_IND_NOISE_FLOOR_CHAIN0_GET(*msg_word);
	else if (chain == 1)
		noise_floor = HTT_RX_IND_NOISE_FLOOR_CHAIN1_GET(*msg_word);

	return noise_floor;
}

/**
 * htt_rx_ind_legacy_rate() - Return the data rate
 * @pdev:        the HTT instance the rx data was received on
 * @rx_ind_msg:  the netbuf containing the rx indication message
 * @legacy_rate: (output) the data rate
 *      The legacy_rate parameter's value depends on the
 *      legacy_rate_sel value.
 *      If legacy_rate_sel is 0:
 *              0x8: OFDM 48 Mbps
 *              0x9: OFDM 24 Mbps
 *              0xA: OFDM 12 Mbps
 *              0xB: OFDM 6 Mbps
 *              0xC: OFDM 54 Mbps
 *              0xD: OFDM 36 Mbps
 *              0xE: OFDM 18 Mbps
 *              0xF: OFDM 9 Mbps
 *      If legacy_rate_sel is 1:
 *              0x8: CCK 11 Mbps long preamble
 *              0x9: CCK 5.5 Mbps long preamble
 *              0xA: CCK 2 Mbps long preamble
 *              0xB: CCK 1 Mbps long preamble
 *              0xC: CCK 11 Mbps short preamble
 *              0xD: CCK 5.5 Mbps short preamble
 *              0xE: CCK 2 Mbps short preamble
 *      -1 on error.
 * @legacy_rate_sel: (output) 0 to indicate OFDM, 1 to indicate CCK.
 *      -1 on error.
 *
 * Return the data rate provided in a rx indication message.
 */
void
htt_rx_ind_legacy_rate(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
    uint8_t *legacy_rate, uint8_t *legacy_rate_sel)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_START_VALID_GET(*msg_word)) {
        *legacy_rate = -1;
        *legacy_rate_sel = -1;
        return;
    }

    *legacy_rate = HTT_RX_IND_LEGACY_RATE_GET(*msg_word);
    *legacy_rate_sel = HTT_RX_IND_LEGACY_RATE_SEL_GET(*msg_word);
}

/**
 * htt_rx_ind_timestamp() - Return the timestamp
 * @pdev:                  the HTT instance the rx data was received on
 * @rx_ind_msg:            the netbuf containing the rx indication message
 * @timestamp_microsec:    (output) the timestamp to microsecond resolution.
 *                         -1 on error.
 * @timestamp_submicrosec: the submicrosecond portion of the
 *                         timestamp. -1 on error.
 *
 * Return the timestamp provided in a rx indication message.
 */
void
htt_rx_ind_timestamp(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
    uint32_t *timestamp_microsec, uint8_t *timestamp_submicrosec)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_END_VALID_GET(*msg_word)) {
        *timestamp_microsec = -1;
        *timestamp_submicrosec = -1;
        return;
    }

    *timestamp_microsec = *(msg_word + 6);
    *timestamp_submicrosec =
            HTT_RX_IND_TIMESTAMP_SUBMICROSEC_GET(*msg_word);
}

/**
 * htt_rx_ind_tsf32() - Return the TSF timestamp
 * @pdev:       the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 *
 * Return the TSF timestamp provided in a rx indication message.
 *
 * Return: TSF timestamp
 */
uint32_t
htt_rx_ind_tsf32(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_END_VALID_GET(*msg_word)) {
        return -1;
    }

    return *(msg_word + 5);
}

/**
 * htt_rx_ind_ext_tid() - Return the extended traffic ID provided in a rx indication message.
 * @pdev:       the HTT instance the rx data was received on
 * @rx_ind_msg: the netbuf containing the rx indication message
 *
 * Return the extended traffic ID in a rx indication message.
 *
 * Return: Extended TID
 */
uint8_t
htt_rx_ind_ext_tid(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg));

    return HTT_RX_IND_EXT_TID_GET(*msg_word);
}

/*--- stats confirmation message ---*/

void
htt_t2h_dbg_stats_hdr_parse(
    u_int8_t *stats_info_list,
    enum htt_dbg_stats_type *type,
    enum htt_dbg_stats_status *status,
    int *length,
    u_int8_t **stats_data)
{
    u_int32_t *msg_word = (u_int32_t *) stats_info_list;
    *type = HTT_T2H_STATS_CONF_TLV_TYPE_GET(*msg_word);
    *status = HTT_T2H_STATS_CONF_TLV_STATUS_GET(*msg_word);
    *length = HTT_T2H_STATS_CONF_TLV_HDR_SIZE +       /* header length */
        HTT_T2H_STATS_CONF_TLV_LENGTH_GET(*msg_word); /* data length */
    *stats_data = stats_info_list + HTT_T2H_STATS_CONF_TLV_HDR_SIZE;
}

void
htt_rx_frag_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_frag_ind_msg,
    u_int16_t *seq_num_start,
    u_int16_t *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_frag_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}

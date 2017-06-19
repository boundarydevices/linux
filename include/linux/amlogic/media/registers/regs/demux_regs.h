/*
 * include/linux/amlogic/media/registers/regs/demux_regs.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef DEMUX_REGS_HEADER_
#define DEMUX_REGS_HEADER_
/*
 * pay attention : the regs range has
 * changed to 0x18xx in txlx, it was
 * converted automatically based on
 * the platform at init.
 * #define FEC_INPUT_CONTROL 0x1802
 */

#define FEC_INPUT_CONTROL 0x1602
#define FEC_INPUT_CONTROL_2 0x1652
#define FEC_INPUT_CONTROL_3 0x16a2
#define FEC_INPUT_DATA 0x1603
#define FEC_INPUT_DATA_2 0x1653
#define FEC_INPUT_DATA_3 0x16a3
#define DEMUX_CONTROL 0x1604
#define DEMUX_CONTROL_2 0x1654
#define DEMUX_CONTROL_3 0x16a4
#define FEC_SYNC_BYTE 0x1605
#define FEC_SYNC_BYTE_2 0x1655
#define FEC_SYNC_BYTE_3 0x16a5
#define FM_WR_DATA 0x1606
#define FM_WR_DATA_2 0x1656
#define FM_WR_DATA_3 0x16a6
#define FM_WR_ADDR 0x1607
#define FM_WR_ADDR_2 0x1657
#define FM_WR_ADDR_3 0x16a7
#define MAX_FM_COMP_ADDR 0x1608
#define MAX_FM_COMP_ADDR_2 0x1658
#define MAX_FM_COMP_ADDR_3 0x16a8
#define TS_HEAD_0 0x1609
#define TS_HEAD_0_2 0x1659
#define TS_HEAD_0_3 0x16a9
#define TS_HEAD_1 0x160a
#define TS_HEAD_1_2 0x165a
#define TS_HEAD_1_3 0x16aa
#define ASSIGN_PID_NUMBER 0x1614
#define ASSIGN_PID_NUMBER_2 0x1664
#define ASSIGN_PID_NUMBER_3 0x16b4
#define VIDEO_STREAM_ID 0x1615
#define VIDEO_STREAM_ID_2 0x1665
#define VIDEO_STREAM_ID_3 0x16b5
#define AUDIO_STREAM_ID 0x1616
#define AUDIO_STREAM_ID_2 0x1666
#define AUDIO_STREAM_ID_3 0x16b6
#define SUB_STREAM_ID 0x1617
#define SUB_STREAM_ID_2 0x1667
#define SUB_STREAM_ID_3 0x16b7
#define OTHER_STREAM_ID 0x1618
#define OTHER_STREAM_ID_2 0x1668
#define OTHER_STREAM_ID_3 0x16b8
#define PCR90K_CTL 0x1619
#define PCR90K_CTL_2 0x1669
#define PCR90K_CTL_3 0x16b9
#define PCR_DEMUX 0x161a
#define PCR_DEMUX_2 0x166a
#define PCR_DEMUX_3 0x16ba
#define VIDEO_PTS_DEMUX 0x161b
#define VIDEO_PTS_DEMUX_2 0x166b
#define VIDEO_PTS_DEMUX_3 0x16bb
#define VIDEO_DTS_DEMUX 0x161c
#define VIDEO_DTS_DEMUX_2 0x166c
#define VIDEO_DTS_DEMUX_3 0x16bc
#define AUDIO_PTS_DEMUX 0x161d
#define AUDIO_PTS_DEMUX_2 0x166d
#define AUDIO_PTS_DEMUX_3 0x16bd
#define SUB_PTS_DEMUX 0x161e
#define SUB_PTS_DEMUX_2 0x166e
#define SUB_PTS_DEMUX_3 0x16be
#define STB_PTS_DTS_STATUS 0x161f
#define STB_PTS_DTS_STATUS_2 0x166f
#define STB_PTS_DTS_STATUS_3 0x16bf
#define STB_DEBUG_INDEX 0x1620
#define STB_DEBUG_INDEX_2 0x1670
#define STB_DEBUG_INDEX_3 0x16c0
#define STB_DEBUG_DATAUT_O 0x1621
#define STB_DEBUG_DATAUT_O_2 0x1671
#define STB_DEBUG_DATAUT_O_3 0x16c1
#define STBM_CTL_O 0x1622
#define STBM_CTL_O_2 0x1672
#define STBM_CTL_O_3 0x16c2
#define STB_INT_STATUS 0x1623
#define STB_INT_STATUS_2 0x1673
#define STB_INT_STATUS_3 0x16c3
#define DEMUX_ENDIAN 0x1624
#define DEMUX_ENDIAN_2 0x1674
#define DEMUX_ENDIAN_3 0x16c4
#define TS_HIU_CTL 0x1625
#define TS_HIU_CTL_2 0x1675
#define TS_HIU_CTL_3 0x16c5
#define SEC_BUFF_BASE 0x1626
#define SEC_BUFF_BASE_2 0x1676
#define SEC_BUFF_BASE_3 0x16c6
#define DEMUX_MEM_REQ_EN 0x1627
#define DEMUX_MEM_REQ_EN_2 0x1677
#define DEMUX_MEM_REQ_EN_3 0x16c7
#define VIDEO_PDTS_WR_PTR 0x1628
#define VIDEO_PDTS_WR_PTR_2 0x1678
#define VIDEO_PDTS_WR_PTR_3 0x16c8
#define AUDIO_PDTS_WR_PTR 0x1629
#define AUDIO_PDTS_WR_PTR_2 0x1679
#define AUDIO_PDTS_WR_PTR_3 0x16c9
#define SUB_WR_PTR 0x162a
#define SUB_WR_PTR_2 0x167a
#define SUB_WR_PTR_3 0x16ca
#define SB_START 0x162b
#define SB_START_2 0x167b
#define SB_START_3 0x16cb
#define SB_LAST_ADDR 0x162c
#define SB_LAST_ADDR_2 0x167c
#define SB_LAST_ADDR_3 0x16cc
#define SB_PES_WR_PTR 0x162d
#define SB_PES_WR_PTR_2 0x167d
#define SB_PES_WR_PTR_3 0x16cd
#define OTHER_WR_PTR 0x162e
#define OTHER_WR_PTR_2 0x167e
#define OTHER_WR_PTR_3 0x16ce
#define OB_START 0x162f
#define OB_START_2 0x167f
#define OB_START_3 0x16cf
#define OB_LAST_ADDR 0x1630
#define OB_LAST_ADDR_2 0x1680
#define OB_LAST_ADDR_3 0x16d0
#define OB_PES_WR_PTR 0x1631
#define OB_PES_WR_PTR_2 0x1681
#define OB_PES_WR_PTR_3 0x16d1
#define STB_INT_MASK 0x1632
#define STB_INT_MASK_2 0x1682
#define STB_INT_MASK_3 0x16d2
#define VIDEO_SPLICING_CTL 0x1633
#define VIDEO_SPLICING_CTL_2 0x1683
#define VIDEO_SPLICING_CTL_3 0x16d3
#define AUDIO_SPLICING_CTL 0x1634
#define AUDIO_SPLICING_CTL_2 0x1684
#define AUDIO_SPLICING_CTL_3 0x16d4
#define TS_PACKAGE_BYTE_COUNT 0x1635
#define TS_PACKAGE_BYTE_COUNT_2 0x1685
#define TS_PACKAGE_BYTE_COUNT_3 0x16d5
#define PES_STRONG_SYNC 0x1636
#define PES_STRONG_SYNC_2 0x1686
#define PES_STRONG_SYNC_3 0x16d6
#define OM_DATA_RD_ADDR 0x1637
#define OM_DATA_RD_ADDR_2 0x1687
#define OM_DATA_RD_ADDR_3 0x16d7
#define OM_DATA_RD 0x1638
#define OM_DATA_RD_2 0x1688
#define OM_DATA_RD_3 0x16d8
#define SECTION_AUTO_STOP_3 0x1639
#define SECTION_AUTO_STOP_3_2 0x1689
#define SECTION_AUTO_STOP_3_3 0x16d9
#define SECTION_AUTO_STOP_2 0x163a
#define SECTION_AUTO_STOP_2_2 0x168a
#define SECTION_AUTO_STOP_2_3 0x16da
#define SECTION_AUTO_STOP_1 0x163b
#define SECTION_AUTO_STOP_1_2 0x168b
#define SECTION_AUTO_STOP_1_3 0x16db
#define SECTION_AUTO_STOP_0 0x163c
#define SECTION_AUTO_STOP_0_2 0x168c
#define SECTION_AUTO_STOP_0_3 0x16dc
#define DEMUX_CHANNEL_RESET 0x163d
#define DEMUX_CHANNEL_RESET_2 0x168d
#define DEMUX_CHANNEL_RESET_3 0x16dd
#define DEMUX_SCRAMBLING_STATE 0x163e
#define DEMUX_SCRAMBLING_STATE_2 0x168e
#define DEMUX_SCRAMBLING_STATE_3 0x16de
#define DEMUX_CHANNEL_ACTIVITY 0x163f
#define DEMUX_CHANNEL_ACTIVITY_2 0x168f
#define DEMUX_CHANNEL_ACTIVITY_3 0x16df
#define DEMUX_STAMP_CTL 0x1640
#define DEMUX_STAMP_CTL_2 0x1690
#define DEMUX_STAMP_CTL_3 0x16e0
#define DEMUX_VIDEO_STAMP_SYNC_0 0x1641
#define DEMUX_VIDEO_STAMP_SYNC_0_2 0x1691
#define DEMUX_VIDEO_STAMP_SYNC_0_3 0x16e1
#define DEMUX_VIDEO_STAMP_SYNC_1 0x1642
#define DEMUX_VIDEO_STAMP_SYNC_1_2 0x1692
#define DEMUX_VIDEO_STAMP_SYNC_1_3 0x16e2
#define DEMUX_AUDIO_STAMP_SYNC_0 0x1643
#define DEMUX_AUDIO_STAMP_SYNC_0_2 0x1693
#define DEMUX_AUDIO_STAMP_SYNC_0_3 0x16e3
#define DEMUX_AUDIO_STAMP_SYNC_1 0x1644
#define DEMUX_AUDIO_STAMP_SYNC_1_2 0x1694
#define DEMUX_AUDIO_STAMP_SYNC_1_3 0x16e4
#define DEMUX_SECTION_RESET 0x1645
#define DEMUX_SECTION_RESET_2 0x1695
#define DEMUX_SECTION_RESET_3 0x16e5


#define STB_TOP_CONFIG 0x16f0
#define TS_TOP_CONFIG 0x16f1
#define TS_FILE_CONFIG 0x16f2
#define TS_PL_PID_INDEX 0x16f3
#define TS_PL_PID_DATA 0x16f4

#endif


/*
 * include/linux/amlogic/media/sound/aiu_regs.h
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

#ifndef _AML_AIU_REGS_H
#define _AML_AIU_REGS_H

/* HIU regs */
#define HHI_AUD_CLK_CNTL	0x5e
#define HHI_AUD_CLK_CNTL2	0x64
#define HHI_MPLL_CNTL		0xa0
#define HHI_MPLL_MP0		0xa6
#define HHI_MPLL_MP1		0xa7
#define HHI_MPLL_MP2		0xa8

/* AIU regs */
#define AIU_958_BPF			0x1500
#define AIU_958_BRST		0x1501
#define AIU_958_LENGTH		0x1502
#define AIU_958_PADDSIZE	0x1503
#define AIU_958_MISC		0x1504
#define AIU_958_FORCE_LEFT	0x1505
#define AIU_958_DISCARD_NUM	0x1506
#define AIU_958_DCU_FF_CTRL	0x1507
#define AIU_958_CHSTAT_L0	0x1508
#define AIU_958_CHSTAT_L1	0x1509
#define AIU_958_CTRL		0x150a
#define AIU_958_RPT			0x150b
#define AIU_I2S_MUTE_SWAP	0x150c
#define AIU_I2S_SOURCE_DESC	0x150d
#define AIU_I2S_MED_CTRL	0x150e
#define AIU_I2S_MED_THRESH	0x150f
#define AIU_I2S_DAC_CFG		0x1510
#define AIU_I2S_SYNC		0x1511
#define AIU_I2S_MISC		0x1512
#define AIU_I2S_OUT_CFG		0x1513
#define AIU_I2S_FF_CTRL		0x1514
#define AIU_RST_SOFT		0x1515
#define AIU_CLK_CTRL		0x1516
#define AIU_MIX_ADCCFG		0x1517
#define AIU_MIX_CTRL		0x1518
#define AIU_CLK_CTRL_MORE	0x1519
#define AIU_958_POP			0x151a
#define AIU_MIX_GAIN		0x151b
#define AIU_958_SYNWORD1	0x151c
#define AIU_958_SYNWORD2	0x151d
#define AIU_958_SYNWORD3	0x151e
#define AIU_958_SYNWORD1_MASK	0x151f
#define AIU_958_SYNWORD2_MASK	0x1520
#define AIU_958_SYNWORD3_MASK	0x1521
#define AIU_958_FFRDOUT_THD		0x1522
#define AIU_958_LENGTH_PER_PAUSE	0x1523
#define AIU_958_PAUSE_NUM		0x1524
#define AIU_958_PAUSE_PAYLOAD	0x1525
#define AIU_958_AUTO_PAUSE		0x1526
#define AIU_958_PAUSE_PD_LENGTH	0x1527
#define AIU_CODEC_DAC_LRCLK_CTRL	0x1528
#define AIU_CODEC_ADC_LRCLK_CTRL	0x1529
#define AIU_HDMI_CLK_DATA_CTRL		0x152a
#define AIU_CODEC_CLK_DATA_CTRL		0x152b
#define AIU_ACODEC_CTRL			0x152c
#define AIU_958_CHSTAT_R0		0x1530
#define AIU_958_CHSTAT_R1		0x1531
#define AIU_958_VALID_CTRL		0x1532
#define AIU_AUDIO_AMP_REG0		0x153c
#define AIU_AUDIO_AMP_REG1		0x153d
#define AIU_AUDIO_AMP_REG2		0x153e
#define AIU_AUDIO_AMP_REG3		0x153f
#define AIU_AIFIFO2_CTRL		0x1540
#define AIU_AIFIFO2_STATUS		0x1541
#define AIU_AIFIFO2_GBIT		0x1542
#define AIU_AIFIFO2_CLB			0x1543
#define AIU_CRC_CTRL			0x1544
#define AIU_CRC_STATUS			0x1545
#define AIU_CRC_SHIFT_REG		0x1546
#define AIU_CRC_IREG			0x1547
#define AIU_CRC_CAL_REG1		0x1548
#define AIU_CRC_CAL_REG0		0x1549
#define AIU_CRC_POLY_COEF1		0x154a
#define AIU_CRC_POLY_COEF0		0x154b
#define AIU_CRC_BIT_SIZE1		0x154c
#define AIU_CRC_BIT_SIZE0		0x154d
#define AIU_CRC_BIT_CNT1		0x154e
#define AIU_CRC_BIT_CNT0		0x154f
#define AIU_AMCLK_GATE_HI		0x1550
#define AIU_AMCLK_GATE_LO		0x1551
#define AIU_AMCLK_MSR			0x1552
#define AIU_AUDAC_CTRL0			0x1553
#define AIU_DELTA_SIGMA0		0x1555
#define AIU_DELTA_SIGMA1		0x1556
#define AIU_DELTA_SIGMA2		0x1557
#define AIU_DELTA_SIGMA3		0x1558
#define AIU_DELTA_SIGMA4		0x1559
#define AIU_DELTA_SIGMA5		0x155a
#define AIU_DELTA_SIGMA6		0x155b
#define AIU_DELTA_SIGMA7		0x155c
#define AIU_DELTA_SIGMA_LCNTS	0x155d
#define AIU_DELTA_SIGMA_RCNTS	0x155e
#define AIU_MEM_I2S_START_PTR	0x1560
#define AIU_MEM_I2S_RD_PTR		0x1561
#define AIU_MEM_I2S_END_PTR		0x1562
#define AIU_MEM_I2S_MASKS		0x1563
#define AIU_MEM_I2S_CONTROL		0x1564
#define AIU_MEM_IEC958_START_PTR	0x1565
#define AIU_MEM_IEC958_RD_PTR		0x1566
#define AIU_MEM_IEC958_END_PTR		0x1567
#define AIU_MEM_IEC958_MASKS		0x1568
#define AIU_MEM_IEC958_CONTROL		0x1569
#define AIU_MEM_AIFIFO2_START_PTR	0x156a
#define AIU_MEM_AIFIFO2_CURR_PTR	0x156b
#define AIU_MEM_AIFIFO2_END_PTR		0x156c
#define AIU_MEM_AIFIFO2_BYTES_AVAIL	0x156d
#define AIU_MEM_AIFIFO2_CONTROL		0x156e
#define AIU_MEM_AIFIFO2_MAN_WP		0x156f
#define AIU_MEM_AIFIFO2_MAN_RP		0x1570
#define AIU_MEM_AIFIFO2_LEVEL		0x1571
#define AIU_MEM_AIFIFO2_BUF_CNTL	0x1572
#define AIU_MEM_I2S_MAN_WP		0x1573
#define AIU_MEM_I2S_MAN_RP		0x1574
#define AIU_MEM_I2S_LEVEL		0x1575
#define AIU_MEM_I2S_BUF_CNTL	0x1576
#define AIU_MEM_I2S_BUF_WRAP_COUNT	0x1577
#define AIU_MEM_I2S_MEM_CTL			0x1578
#define AIU_MEM_IEC958_MEM_CTL		0x1579
#define AIU_MEM_IEC958_WRAP_COUNT	0x157a
#define AIU_MEM_IEC958_IRQ_LEVEL	0x157b
#define AIU_MEM_IEC958_MAN_WP		0x157c
#define AIU_MEM_IEC958_MAN_RP		0x157d
#define AIU_MEM_IEC958_LEVEL		0x157e
#define AIU_MEM_IEC958_BUF_CNTL		0x157f
#define AIU_AIFIFO_CTRL		0x1580
#define AIU_AIFIFO_STATUS	0x1581
#define AIU_AIFIFO_GBIT		0x1582
#define AIU_AIFIFO_CLB		0x1583
#define AIU_MEM_AIFIFO_START_PTR	0x1584
#define AIU_MEM_AIFIFO_CURR_PTR		0x1585
#define AIU_MEM_AIFIFO_END_PTR		0x1586
#define AIU_MEM_AIFIFO_BYTES_AVAIL	0x1587
#define AIU_MEM_AIFIFO_CONTROL		0x1588
#define AIU_MEM_AIFIFO_MAN_WP		0x1589
#define AIU_MEM_AIFIFO_MAN_RP		0x158a
#define AIU_MEM_AIFIFO_LEVEL		0x158b
#define AIU_MEM_AIFIFO_BUF_CNTL		0x158c
#define AIU_MEM_AIFIFO_BUF_WRAP_COUNT	0x158d
#define AIU_MEM_AIFIFO2_BUF_WRAP_COUNT	0x158e
#define AIU_MEM_AIFIFO_MEM_CTL		0x158f
#define AIFIFO_TIME_STAMP_CNTL		0x1590
#define AIFIFO_TIME_STAMP_SYNC_0	0x1591
#define AIFIFO_TIME_STAMP_SYNC_1	0x1592
#define AIFIFO_TIME_STAMP_0			0x1593
#define AIFIFO_TIME_STAMP_1			0x1594
#define AIFIFO_TIME_STAMP_2			0x1595
#define AIFIFO_TIME_STAMP_3			0x1596
#define AIFIFO_TIME_STAMP_LENGTH	0x1597
#define AIFIFO2_TIME_STAMP_CNTL		0x1598
#define AIFIFO2_TIME_STAMP_SYNC_0	0x1599
#define AIFIFO2_TIME_STAMP_SYNC_1	0x159a
#define AIFIFO2_TIME_STAMP_0		0x159b
#define AIFIFO2_TIME_STAMP_1		0x159c
#define AIFIFO2_TIME_STAMP_2		0x159d
#define AIFIFO2_TIME_STAMP_3		0x159e
#define AIFIFO2_TIME_STAMP_LENGTH	0x159f
#define IEC958_TIME_STAMP_CNTL		0x15a0
#define IEC958_TIME_STAMP_SYNC_0	0x15a1
#define IEC958_TIME_STAMP_SYNC_1	0x15a2
#define IEC958_TIME_STAMP_0		0x15a3
#define IEC958_TIME_STAMP_1		0x15a4
#define IEC958_TIME_STAMP_2		0x15a5
#define IEC958_TIME_STAMP_3		0x15a6
#define IEC958_TIME_STAMP_LENGTH	0x15a7
#define AIU_MEM_AIFIFO2_MEM_CTL		0x15a8
#define AIU_I2S_CBUS_DDR_CNTL		0x15a9
#define AIU_I2S_CBUS_DDR_WDATA		0x15aa
#define AIU_I2S_CBUS_DDR_ADDR		0x15ab

#define ISA_TIMER_MUX   0x2650
#define ISA_TIMERA      0x2651
#define ISA_TIMERB      0x2652
#define ISA_TIMERC      0x2653
#define ISA_TIMERD      0x2654
#define ISA_TIMERE      0x2655

#define MEDIA_CPU_CTL				0x2661
#define RESET2_REGISTER				0x1103
#define RESET1_REGISTER				0x1102
#define ACODEC_RESET				0x1D

/* internal EQ regs */
#define   AED_EQ_CH1_COEF00                 0x1400
#define   AED_EQ_CH1_COEF01                 0x1401
#define   AED_EQ_CH1_COEF02                 0x1402
#define   AED_EQ_CH1_COEF03                 0x1403
#define   AED_EQ_CH1_COEF04                 0x1404
#define   AED_EQ_CH1_COEF10                 0x1405
#define   AED_EQ_CH1_COEF11                 0x1406
#define   AED_EQ_CH1_COEF12                 0x1407
#define   AED_EQ_CH1_COEF13                 0x1408
#define   AED_EQ_CH1_COEF14                 0x1409
#define   AED_EQ_CH1_COEF20                 0x140a
#define   AED_EQ_CH1_COEF21                 0x140b
#define   AED_EQ_CH1_COEF22                 0x140c
#define   AED_EQ_CH1_COEF23                 0x140d
#define   AED_EQ_CH1_COEF24                 0x140e
#define   AED_EQ_CH1_COEF30                 0x140f
#define   AED_EQ_CH1_COEF31                 0x1410
#define   AED_EQ_CH1_COEF32                 0x1411
#define   AED_EQ_CH1_COEF33                 0x1412
#define   AED_EQ_CH1_COEF34                 0x1413
#define   AED_EQ_CH1_COEF40                 0x1414
#define   AED_EQ_CH1_COEF41                 0x1415
#define   AED_EQ_CH1_COEF42                 0x1416
#define   AED_EQ_CH1_COEF43                 0x1417
#define   AED_EQ_CH1_COEF44                 0x1418
#define   AED_EQ_CH1_COEF50                 0x1419
#define   AED_EQ_CH1_COEF51                 0x141a
#define   AED_EQ_CH1_COEF52                 0x141b
#define   AED_EQ_CH1_COEF53                 0x141c
#define   AED_EQ_CH1_COEF54                 0x141d
#define   AED_EQ_CH1_COEF60                 0x141e
#define   AED_EQ_CH1_COEF61                 0x141f
#define   AED_EQ_CH1_COEF62                 0x1420
#define   AED_EQ_CH1_COEF63                 0x1421
#define   AED_EQ_CH1_COEF64                 0x1422
#define   AED_EQ_CH1_COEF70                 0x1423
#define   AED_EQ_CH1_COEF71                 0x1424
#define   AED_EQ_CH1_COEF72                 0x1425
#define   AED_EQ_CH1_COEF73                 0x1426
#define   AED_EQ_CH1_COEF74                 0x1427
#define   AED_EQ_CH1_COEF80                 0x1428
#define   AED_EQ_CH1_COEF81                 0x1429
#define   AED_EQ_CH1_COEF82                 0x142a
#define   AED_EQ_CH1_COEF83                 0x142b
#define   AED_EQ_CH1_COEF84                 0x142c
#define   AED_EQ_CH1_COEF90                 0x142d
#define   AED_EQ_CH1_COEF91                 0x142e
#define   AED_EQ_CH1_COEF92                 0x142f
#define   AED_EQ_CH1_COEF93                 0x1430
#define   AED_EQ_CH1_COEF94                 0x1431

#define   AED_EQ_CH2_COEF00                 0x1432
#define   AED_EQ_CH2_COEF01                 0x1433
#define   AED_EQ_CH2_COEF02                 0x1434
#define   AED_EQ_CH2_COEF03                 0x1435
#define   AED_EQ_CH2_COEF04                 0x1436
#define   AED_EQ_CH2_COEF10                 0x1437
#define   AED_EQ_CH2_COEF11                 0x1438
#define   AED_EQ_CH2_COEF12                 0x1439
#define   AED_EQ_CH2_COEF13                 0x143a
#define   AED_EQ_CH2_COEF14                 0x143b
#define   AED_EQ_CH2_COEF20                 0x143c
#define   AED_EQ_CH2_COEF21                 0x143d
#define   AED_EQ_CH2_COEF22                 0x143e
#define   AED_EQ_CH2_COEF23                 0x143f
#define   AED_EQ_CH2_COEF24                 0x1440
#define   AED_EQ_CH2_COEF30                 0x1441
#define   AED_EQ_CH2_COEF31                 0x1442
#define   AED_EQ_CH2_COEF32                 0x1443
#define   AED_EQ_CH2_COEF33                 0x1444
#define   AED_EQ_CH2_COEF34                 0x1445
#define   AED_EQ_CH2_COEF40                 0x1446
#define   AED_EQ_CH2_COEF41                 0x1447
#define   AED_EQ_CH2_COEF42                 0x1448
#define   AED_EQ_CH2_COEF43                 0x1449
#define   AED_EQ_CH2_COEF44                 0x144a
#define   AED_EQ_CH2_COEF50                 0x144b
#define   AED_EQ_CH2_COEF51                 0x144c
#define   AED_EQ_CH2_COEF52                 0x144d
#define   AED_EQ_CH2_COEF53                 0x144e
#define   AED_EQ_CH2_COEF54                 0x144f
#define   AED_EQ_CH2_COEF60                 0x1450
#define   AED_EQ_CH2_COEF61                 0x1451
#define   AED_EQ_CH2_COEF62                 0x1452
#define   AED_EQ_CH2_COEF63                 0x1453
#define   AED_EQ_CH2_COEF64                 0x1454
#define   AED_EQ_CH2_COEF70                 0x1455
#define   AED_EQ_CH2_COEF71                 0x1456
#define   AED_EQ_CH2_COEF72                 0x1457
#define   AED_EQ_CH2_COEF73                 0x1458
#define   AED_EQ_CH2_COEF74                 0x1459
#define   AED_EQ_CH2_COEF80                 0x145a
#define   AED_EQ_CH2_COEF81                 0x145b
#define   AED_EQ_CH2_COEF82                 0x145c
#define   AED_EQ_CH2_COEF83                 0x145d
#define   AED_EQ_CH2_COEF84                 0x145e
#define   AED_EQ_CH2_COEF90                 0x145f
#define   AED_EQ_CH2_COEF91                 0x1460
#define   AED_EQ_CH2_COEF92                 0x1461
#define   AED_EQ_CH2_COEF93                 0x1462
#define   AED_EQ_CH2_COEF94                 0x1463

#define   AED_EQ_EN                         0x1464
#define   AED_EQ_VOLUME                     0x1465
#define   AED_EQ_VOLUME_SLEW_CNT            0x1466
#define   AED_MUTE                          0x1467

/* internal DRC reg*/
#define   AED_DRC_EN                        0x1470
#define   AED_DRC_AE                        0x1471
#define   AED_DRC_AA                        0x1472
#define   AED_DRC_AD                        0x1473
#define   AED_DRC_AE_1M                     0x1474
#define   AED_DRC_AA_1M                     0x1475
#define   AED_DRC_AD_1M                     0x1476
#define   AED_DRC_OFFSET0                   0x1477
#define   AED_DRC_OFFSET1                   0x1478
#define   AED_DRC_THD0                      0x1479
#define   AED_DRC_THD1                      0x147a
#define   AED_DRC_K0                        0x147b
#define   AED_DRC_K1                        0x147c
#define   AED_CLIP_THD                      0x147d

/* internal noise gate regs*/
#define   AED_NG_THD0                       0x147e
#define   AED_NG_THD1                       0x147f
#define   AED_NG_CNT_THD                    0x1480
#define   AED_NG_CTL                        0x1481

#define   AED_ED_CTL                        0x1482
#define   AED_DEBUG0                        0x1483
#define   AED_DEBUG1                        0x1484
#define   AED_DEBUG2                        0x1485
#define   AED_DEBUG3                        0x1486
#define   AED_DEBUG4                        0x1487
#define   AED_DEBUG5                        0x1488
#define   AED_DEBUG6                        0x1489
#define   AED_DRC_AA_H                      0x148a
#define   AED_DRC_AD_H                      0x148b
#define   AED_DRC_AA_1M_H                   0x148c
#define   AED_DRC_AD_1M_H                   0x148d
#define   AED_NG_CNT                        0x148e
#define   AED_NG_STEP                       0x148f
#define   AED_TOP_CTL                       0x1490

#endif /* _AML_AIU_REGS_H */

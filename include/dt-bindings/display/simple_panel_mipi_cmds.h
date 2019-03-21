#define S_DCS_S0P	1
#define S_DCS_S1P	2
#define S_DCS_L2P	3
#define S_DCS_L3P	4
#define S_DCS_L4P	5
#define S_DCS_L5P	6
#define S_DELAY		0x40
#define S_MRPS		0x41
#define S_DCS_BUF	0x42
#define S_DCS_LENGTH	0x48
#define S_DCS_READ	0x49
#define S_DCS_READ1	0x49
#define S_DCS_READ2	0x4a
#define S_DCS_READ3	0x4b
#define S_DCS_READ4	0x4c
#define S_DCS_READ5	0x4d
#define S_DCS_READ6	0x4e
#define S_DCS_READ7	0x4f
#define S_DCS_READ8	0x50
#define S_IF_1_LANE	0x51
#define S_IF_2_LANES	0x52
#define S_IF_3_LANES	0x53
#define S_IF_4_LANES	0x54
#define S_IF_BURST	0x55
#define S_IF_NONBURST	0x56
#define S_CONST		0x57
#define S_HSYNC		0x58
#define S_HBP		0x59
#define S_HACTIVE	0x5a
#define S_HFP		0x5b
#define S_VSYNC		0x5c
#define S_VBP		0x5d
#define S_VACTIVE	0x5e
#define S_VFP		0x5f
#define S_LPTXTIME	0x60
#define S_CLRSIPOCOUNT	0x61

#define S_GEN_S0P	0x81
#define S_GEN_S1P	0x82
#define S_GEN_L2P	0x83
#define S_GEN_L3P	0x84
#define S_GEN_L4P	0x85
#define S_GEN_L5P	0x86
#define S_GEN_BUF	0xc2
#define S_GEN_LENGTH	0xc8
#define S_GEN_READ1	0xc9
#define S_GEN_READ2	0xca
#define S_GEN_READ3	0xcb
#define S_GEN_READ4	0xcc
#define S_GEN_READ5	0xcd
#define S_GEN_READ6	0xce
#define S_GEN_READ7	0xcf
#define S_GEN_READ8	0xd0

#define MRPS(len)			S_MRPS len			/* Set max return packet size */
#define DCS_READ(cmd, c1)		S_DCS_READ1 cmd c1
#define DCS_READ1(cmd, c1)		S_DCS_READ1 cmd c1
#define DCS_READ2(cmd, c1, c2)		S_DCS_READ2 cmd c1 c2
#define DCS_READ3(cmd, c1, c2, c3)	S_DCS_READ3 cmd c1 c2 c3
#define DCS_READ4(cmd, c1, c2, c3, c4)	S_DCS_READ4 cmd c1 c2 c3 c4
#define GEN_READ1(addrl, addrh, c1)	S_GEN_READ1 addrl addrh c1
#define GEN_READ2(addrl, addrh, c1, c2)	S_GEN_READ2 addrl addrh c1 c2
#define GEN_READ3(addrl, addrh, c1, c2, c3)	S_GEN_READ3 addrl addrh c1 c2 c3
#define GEN_READ4(addrl, addrh, c1, c2, c3, c4)	S_GEN_READ4 addrl addrh c1 c2 c3 c4
#define GEN_READ5(addrl, addrh, c1, c2, c3, c4, c5)	S_GEN_READ5 addrl addrh c1 c2 c3 c4 c5
#define GEN_READ6(addrl, addrh, c1, c2, c3, c4, c5, c6)	S_GEN_READ6 addrl addrh c1 c2 c3 c4 c5 c6
#define GEN_READ7(addrl, addrh, c1, c2, c3, c4, c5, c6, c7)	S_GEN_READ7 addrl addrh c1 c2 c3 c4 c5 c6 c7
#define GEN_READ8(addrl, addrh, c1, c2, c3, c4, c5, c6, c7, c8)	S_GEN_READ8 addrl addrh c1 c2 c3 c4 c5 c6 c7 c8
#define DCS_S0P(cmd)			S_DCS_S0P cmd			/* DCS_Short_Write_NP */
#define DCS_S1P(cmd, p1)		S_DCS_S1P cmd p1		/* DCS_Short_Write_1P */
#define DCS_L2P(cmd, p1, p2)		S_DCS_L2P cmd p1 p2		/* DCS Long_Write_2P */
#define DCS_L3P(cmd, p1, p2, p3)	S_DCS_L3P cmd p1 p2 p3		/* DCS_Long_Write_3P */
#define DCS_L4P(cmd, p1, p2, p3, p4)	S_DCS_L4P cmd p1 p2 p3 p4	/* DCS_Long_Write_4P */
#define DCS_L5P(cmd, p1, p2, p3, p4, p5) S_DCS_L5P cmd p1 p2 p3	p4 p5	/* DCS_Long_Write_5P */
#define GEN_S0P(cmd)			S_GEN_S0P cmd			/* Generic_Short_Write_NP */
#define GEN_S1P(cmd, p1)		S_GEN_S1P cmd p1		/* Generic_Short_Write_1P */
#define GEN_L2P(cmd, p1, p2)		S_GEN_L2P cmd p1 p2		/* Generic_Long_Write_2P */
#define GEN_L3P(cmd, p1, p2, p3)	S_GEN_L3P cmd p1 p2 p3		/* Generic_Long_Write_3P */
#define GEN_L4P(cmd, p1, p2, p3, p4)	S_GEN_L4P cmd p1 p2 p3 p4	/* Generic_Long_Write_4P */
#define GEN_L5P(addrl, addrh, p1, p2, p3, p4)	S_GEN_L5P addrl addrh p1 p2 p3 p4
#define GEN_BUF(len)			S_GEN_BUF len
#define DELAY(a)			S_DELAY a
#define S_BUF_CONST(dest_start, dest_len, c1, c2, c3, c4) S_CONST dest_start dest_len c1 c2 c3 c4
#define S_BUF_SET(id, dest_start, dest_len, src_start) id dest_start dest_len src_start

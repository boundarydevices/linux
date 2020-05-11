#define S_DCS_S0P	1
#define S_DCS_S1P	2
#define S_DCS_L2P	3
#define S_DCS_L3P	4
#define S_DCS_L4P	5
#define S_DCS_L5P	6
#define S_DELAY		0x40
#define S_MRPS		0x41
#define S_DCS_LENGTH	0x48
#define S_DCS_READ1	0x49
#define S_DCS_READ2	0x4a
#define S_DCS_READ3	0x4b
#define S_DCS_READ4	0x4c
#define S_IF_1_LANE	0x51
#define S_IF_2_LANES	0x52
#define S_IF_3_LANES	0x53
#define S_IF_4_LANES	0x54

#define S_GEN_S0P	0x81
#define S_GEN_S1P	0x82
#define S_GEN_L2P	0x83
#define S_GEN_L3P	0x84
#define S_GEN_L4P	0x85
#define S_GEN_L5P	0x86
#define S_GEN_LENGTH	0xc8
#define S_GEN_READ1	0xc9
#define S_GEN_READ2	0xca
#define S_GEN_READ3	0xcb
#define S_GEN_READ4	0xcc

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
#define DCS_S0P(cmd)			S_DCS_S0P cmd			/* DCS_Short_Write_NP */
#define DCS_S1P(cmd, p1)		S_DCS_S1P cmd p1		/* DCS_Short_Write_1P */
#define DCS_L2P(cmd, p1, p2)		S_DCS_L2P cmd p1 p2		/* DCS Long_Write_2P */
#define DCS_L3P(cmd, p1, p2, p3)	S_DCS_L3P cmd p1 p2 p3		/* DCS_Long_Write_3P */
#define GEN_S0P(cmd)			S_GEN_S0P cmd			/* Generic_Short_Write_NP */
#define GEN_S1P(cmd, p1)		S_GEN_S1P cmd p1		/* Generic_Short_Write_1P */
#define GEN_L2P(cmd, p1, p2)		S_GEN_L2P cmd p1 p2		/* Generic_Long_Write_2P */
#define GEN_L3P(cmd, p1, p2, p3)	S_GEN_L3P cmd p1 p2 p3		/* Generic_Long_Write_3P */
#define GEN_L5P(addrl, addrh, p1, p2, p3, p4)	S_GEN_L5P addrl addrh p1 p2 p3 p4
#define DELAY(a)			S_DELAY a

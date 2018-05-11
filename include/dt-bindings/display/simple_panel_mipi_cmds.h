#define S_DCS_S0P	1
#define S_DCS_S1P	2
#define S_DCS_L3P	4
#define S_DELAY		5
#define S_GEN_S0P	0x81
#define S_GEN_S1P	0x82
#define S_GEN_L3P	0x84

#define DCS_S0P(cmd)			S_DCS_S0P cmd			/* DCS_Short_Write_NP */
#define DCS_S1P(cmd, p1)		S_DCS_S1P cmd p1		/* DCS_Short_Write_1P */
#define DCS_L3P(cmd, p1, p2, p3)	S_DCS_L3P cmd p1 p2 p3		/* DCS_Long_Write_3P */
#define GEN_S0P(cmd)			S_GEN_S0P cmd			/* Generic_Short_Write_NP */
#define GEN_S1P(cmd, p1)		S_GEN_S1P cmd p1		/* Generic_Short_Write_1P */
#define GEN_L3P(cmd, p1, p2, p3)	S_GEN_L3P cmd p1 p2 p3		/* Generic_Long_Write_3P */
#define DELAY(a)			S_DELAY a

/****************************************************************************
*
*    Copyright (c) 2005 - 2020 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/


/*
** gcSL language definition
*/

#ifndef __gc_vsc_old_gcsl_h_
#define __gc_vsc_old_gcsl_h_

BEGIN_EXTERN_C()

/* always set SHADER_64BITMODE to 0 to unify 32bit and 64bit shader binaries */
#if (PTRDIFF_MAX  == INT32_MAX)
#define SHADER_64BITMODE  0
#else
#define SHADER_64BITMODE  0
#endif

#if gcdRENDER_QUALITY_CHECK
#define TEMP_OPT_CONSTANT_TEXLD_COORD    0
#else
#define TEMP_OPT_CONSTANT_TEXLD_COORD    1
#endif

/******************************* IR VERSION ******************/
#define gcdSL_IR_VERSION gcmCC('\0','\0','\0','\1')

/******************************* SHADER BINARY FILE VERSION ******************/
/* the shader file version before Loadtime Constant optimization */
#define gcdSL_SHADER_BINARY_BEFORE_LTC_FILE_VERSION gcmCC(0, 0, 1, 1)

/* bump up version to 1.2 for Loadtime Constant optimization on 1/3/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_STRUCT_SYMBOL_FILE_VERSION gcmCC(0, 0, 1, 2)

/* bump up version to 1.3 for struct support for variable and uniform on 3/9/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_VARIABLE_TYPE_QUALIFIER_FILE_VERSION gcmCC(0, 0, 1, 3)

/* bump up version to 1.4 for variable type qualifier support on 4/2/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_PRECISION_QUALIFIER_FILE_VERSION gcmCC(0, 0, 1, 4)

/* bump up version to 1.5 for precision qualifier support on 8/23/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_HALTI_FILE_VERSION gcmCC(0, 0, 1, 5)

/* bump up version to 1.6 for halti feature support on 9/10/2012 */
#define gcdSL_SHADER_BINARY_BEFORE_IR_CHANGE_FILE_VERSION gcmCC(0, 0, 1, 6)

/* bump up version to 1.7 for openCL image sampler with TEXLD support on 9/18/2013 */
#define gcdSL_SHADER_BINARY_BEFORE_OPENCL_IMAGE_SAMPLER_BY_TEXLD_FILE_VERSION gcmCC(0, 0, 1, 7)

/* bump up version to 1.8 for IR change support on 9/19/2013 */
#define gcdSL_SHADER_BINARY_VERSION_9_19_2013 gcmCC(0, 0, 1, 8)

/* bump up version to 1.9 for ES31 features support on 4/22/2014 */
#define gcdSL_SHADER_BINARY_BEFORE_ES31_VERSION gcmCC(0, 0, 1, 9)

/* bump up version to 1.10 for OPENCL1.2 features support on 8/22/2014 */
#define gcdSL_SHADER_BINARY_BEFORE_CL12_VERSION gcmCC(0, 0, 1, 10)

/* bump up version to 1.11 for ES31 uniform location support on 8/28/2014 */
#define gcdSL_SHADER_BINARY_BEFORE_ES31_UNIFORM_LOCATION_VERSION gcmCC(0, 0, 1, 11)

/* bump up version to 1.12 for invariant support on 10/20/2014 */
#define gcdSL_SHADER_BINARY_BEFORE_ADD_INVARIANT_VERSION gcmCC(0, 0, 1, 12)

/* bump up version to 1.13 for OCL uniform block support on 11/15/2014 */
#define gcdSL_SHADER_BINARY_BEFORE_OCL_UNIFORM_BLOCK_VERSION gcmCC(0, 0, 1, 13)

/* bump up version to 1.14 for ES31 uniform base binding index support on 04/07/2015 */
#define gcdSL_SHADER_BINARY_BEFORE_UNIFORM_BASE_BINDING_VERSION gcmCC(0, 0, 1, 14)

/* bump up version to 1.15 for enlarge type size from 8 bit to 16 bit on 06/25/2015 */
#define gcdSL_SHADER_BINARY_BEFORE_16BIT_TYPE_VERSION gcmCC(0, 0, 1, 15)

/* bump up version to 1.16 for ltc dummy uniform index on 09/23/2016 */
#define gcdSL_SHADER_BINARY_BEFORE_DUMMY_UNIFORM_INDEX_VERSION gcmCC(0, 0, 1, 16)

/* bump up version to 1.17 for type name var index on 03/16/2017 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVEING_TYPE_NAME_VAR_INDEX gcmCC(0, 0, 1, 17)

/* bump up version to 1.18 for uniform physical addr on 04/13/2017 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVEING_UNIFORM_PHYSICAL_ADDR gcmCC(0, 0, 1, 18)

/* bump up version to 1.19 for uniform physical addr on 09/06/2017 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVEING_UNIFORM_RES_OP_FLAG gcmCC(0, 0, 1, 19)

/* bump up version to 1.20 for new header with chipModel and chpRevision on 11/30/2017 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVEING_CHIPMODEL gcmCC(0, 0, 1, 20)

/* bump up version to 1.22 for new changes merged from dev_129469 branch on 11/21/2017 */
#define gcdSL_SHADER_BINARY_BEFORE_MERGED_FROM_DEV_129469_FLAG gcmCC(0, 0, 1, 22)

/* bump up version to 1.23 for remove VG from shader flags on 03/01/2018 */
#define gcdSL_SHADER_BINARY_BEFORE_MOVE_VG_FROM_SHADER_FLAG gcmCC(0, 0, 1, 23)
/* bump up version to 1.24 for adding transform feedback info on 08/20/2018 */

/* bump up version to 1.26 for adding output's shader mode on 09/28/2018 */
/* bump up version to 1.27 for modify _viv_atan2_float() to comform to CL spec on 11/20/2018 */
/* bump up version to 1.28 for using HALTI5 trig functions for all cases (not just conformance) on 12/3/2018 */
/* bump up version to 1.29 for new header with chipModel and chpRevision on 03/19/2019 */
/* bump up version to 1.30 for saving source string on 03/26/2019 */
/* bump up version to 1.31 for saving some flags in hints on 04/17/2019 */
/* bump up version to 1.32 for saving some flags in hints on 04/25/2019 */
/* bump up version to 1.33 for saving the shader source for OCL on 07/15/2019 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVING_SHADER_SOURCE_FOR_OCL gcmCC(0, 0, 1, 33)

/* bump up version to 1.34 for workGroupSizeFactor into the binary on 07/18/2019 */
/* bump up version to 1.35 for adding isBuiltinArray and builtinArrayIdx in TFBVarying on 07/19/2019 */
/* bump up version to 1.36 for adding bEndOfInterleavedBuffer in TFBVarying on 07/22/2019 */
/* bump up version to 1.37 for adding intrisinc functions for gSampler2DRect on 08/06/2019 */
/* bump up version to 1.38 for saving the full graphics shaders into the binary on 08/08/2019 */
/* bump up version to 1.39 for saving ubo array index into the binary on 08/09/2019 */
/* bump up version to 1.40 for saving local memory size, uniform's swizzle and shader kind into the binary on 11/07/2019 */

/* bump up version to 1.41 for saving the stream number for gcOUTPUT on 11/07/2019 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVING_STREAM_NUMBER_FOR_OUTPUT gcmCC(0, 0, 1, 41)

/* bump up version to 1.42 for saving adding intrinsic functions sin, cos, tan 11/8/2019 */
/* bump up version to 1.43 for supporting textureGather functions have texture2DrectShadow type 11/14/2019 */

/* bump up version to 1.44 for saving cl_program_binary_type for gcSHADER on 03/12/2020 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVING_CL_PROGRAM_BINARY_TYPE gcmCC(0, 0, 1, 44)

/* bump up version to 1.45 for saving the return variable to a argument 03/27/2020 */
#define gcdSL_SHADER_BINARY_BEFORE_SAVING_FUNCTION_RETURN_VARIABLE gcmCC(0, 0, 1, 45)

/* bump up version to 1.46 for adding intrinsic functions vstore_half with rounding modes rtz, rtp and rtn 03/28/2020 */

/* bump up version to 1.47 for adding intrinsic functions noise1, noise2 ... for OGL4.0 05/02/2020 */

/* bump up version to 1.48 for adding intrinsic functions of double type for ldexp, frexp, fma, packDouble2x32 and unpackDouble2x32 for OGL4.0 05/04/2020 */

/* bump up version to 1.49 for supporting textureSize function for more sampler type 5/20/2020 */
/* bump up version to 1.50 for adding a new variable tcsHasNoPerVertexAttribute in hints 06/03/2020 */
/* bump up version to 1.51 for modifying the constant border value 07/14/2020 */
/* bump up version to 1.52 for adding OCL fma intrinsic function when HW support is not available 07/17/2020 */
/* bump up version to 1.53 for adding OCL dual16 check in hints 07/17/2020 */
/* bump up version to 1.54 for adding a new _viv_image_load_uimage_1d_rg32ui, iimage_1d_rg32i, uimage_3d_rg32ui, iimage_3d_rg32i libfunction 24/08/2020 */

/* bump up version to 1.55 for saving shaderKind and swizzle of uniform to gcSHADER on 09/09/2020 */
/* bump up version to 1.56 for refine save and load inputLocation and outputLocation in gcSHADER on 09/22/2020 */

/* current version */
#define gcdSL_SHADER_BINARY_FILE_VERSION gcmCC(SHADER_64BITMODE, 0, 1, 57)
#define gcdSL_PROGRAM_BINARY_FILE_VERSION gcmCC(SHADER_64BITMODE, 0, 1, 57)

typedef union _gcsValue
{
    /* vector 16 value */
    gctFLOAT         f32_v16[16];
    gctINT32         i32_v16[16];
    gctUINT32        u32_v16[16];

    /* vector 4 value */
    gctFLOAT         f32_v4[4];
    gctINT32         i32_v4[4];
    gctUINT32        u32_v4[4];

    /* packed vector 16 value */
    gctINT8          i8_v16[16];
    gctUINT8         u8_v16[16];
    gctINT16         i16_v16[16];
    gctUINT16        u16_v16[16];

    /* scalar value */
    gctFLOAT         f32;
    gctINT32         i32;
    gctUINT32        u32;
} gcsValue;

/******************************************************************************\
|******************************* SHADER LANGUAGE ******************************|
\******************************************************************************/

/* Shader types. */
typedef enum _gcSHADER_KIND {
    gcSHADER_TYPE_UNKNOWN = 0,
    gcSHADER_TYPE_VERTEX,
    gcSHADER_TYPE_FRAGMENT,
    gcSHADER_TYPE_COMPUTE,
    gcSHADER_TYPE_CL,
    gcSHADER_TYPE_PRECOMPILED,
    gcSHADER_TYPE_LIBRARY,
    gcSHADER_TYPE_VERTEX_DEFAULT_UBO,
    gcSHADER_TYPE_FRAGMENT_DEFAULT_UBO,
    gcSHADER_TYPE_TCS,
    gcSHADER_TYPE_TES,
    gcSHADER_TYPE_GEOMETRY,
    gcSHADER_KIND_COUNT
} gcSHADER_KIND;

typedef enum gceFRAGOUT_USAGE
{
    gcvFRAGOUT_USAGE_USER_DEFINED     = 0,
    gcvFRAGOUT_USAGE_FRAGCOLOR        = 1,
    gcvFRAGOUT_USAGE_FRAGDATA         = 2,
}
gceFRAGOUT_USAGE;

#define gcSL_GetShaderKindString(Kind) (((Kind) == gcSHADER_TYPE_VERTEX) ? "VS" :        \
                                        ((Kind) == gcSHADER_TYPE_FRAGMENT) ? "FS" :      \
                                        ((Kind) == gcSHADER_TYPE_TCS) ? "TCS" :          \
                                        ((Kind) == gcSHADER_TYPE_TES) ? "TES" :          \
                                        ((Kind) == gcSHADER_TYPE_GEOMETRY) ? "GEO" :     \
                                        ((Kind) == gcSHADER_TYPE_CL) ? "CL" :            \
                                        ((Kind) == gcSHADER_TYPE_LIBRARY) ? "LIBRARY" :  \
                                        ((Kind) == gcSHADER_TYPE_PRECOMPILED) ? "PRECOMPILED" :  \
                                        ((Kind) == gcSHADER_TYPE_COMPUTE) ? "CS" : "??")

#define _SHADER_GL_LANGUAGE_TYPE  gcmCC('E', 'S', '\0', '\0')
#define _SHADER_DX_LANGUAGE_TYPE  gcmCC('D', 'X', '\0', '\0')
#define _cldLanguageType          gcmCC('C', 'L', '\0', '\0')
#define _SHADER_VG_TYPE           gcmCC('V', 'G', '\0', '\0')
#define _SHADER_OGL_LANGUAGE_TYPE gcmCC('G', 'L', '\0', '\0')

#define _SHADER_GL_VERSION_SIG    1

#define _SHADER_ES11_VERSION      gcmCC('\0', '\0', '\1', '\1')
#define _SHADER_HALTI_VERSION     gcmCC('\0', '\0', '\0', '\3')
#define _SHADER_ES31_VERSION      gcmCC('\0', '\0', '\1', '\3')
#define _SHADER_ES32_VERSION      gcmCC('\0', '\0', '\2', '\3')
#define _SHADER_ES40_VERSION      gcmCC('\0', '\0', '\0', '\4')
#define _SHADER_DX_VERSION_30     gcmCC('\3', '\0', '\0', '\0')
#define _cldCL1Dot1               gcmCC('\0', '\0', '\0', '\1')
#define _cldCL1Dot2               gcmCC('\0', '\0', '\2', '\1')
#define _cldCL3Dot0               gcmCC('\0', '\0', '\0', '\3')
#define _SHADER_GL10_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\0', '\1')
#define _SHADER_GL11_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\1', '\1')
#define _SHADER_GL12_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\2', '\1')
#define _SHADER_GL13_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\3', '\1')
#define _SHADER_GL14_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\4', '\1')
#define _SHADER_GL15_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\5', '\1')
#define _SHADER_GL33_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\3', '\3')
#define _SHADER_GL40_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\0', '\4')
#define _SHADER_GL41_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\1', '\4')
#define _SHADER_GL42_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\2', '\4')
#define _SHADER_GL43_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\3', '\4')
#define _SHADER_GL44_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\4', '\4')
#define _SHADER_GL45_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\5', '\4')
#define _SHADER_GL46_VERSION      gcmCC('\0', _SHADER_GL_VERSION_SIG, '\6', '\4')

#define gcShader_IsCL(S)           (GetShaderType(S) == gcSHADER_TYPE_CL && (((S)->compilerVersion[0] & 0xFFFF) == _cldLanguageType))
#define gcShader_IsGlCompute(S)    (GetShaderType(S) == gcSHADER_TYPE_COMPUTE && (((S)->compilerVersion[0] & 0xFFFF) != _cldLanguageType))

/* Client version. */
typedef enum _gcSL_OPCODE
{
    gcSL_NOP, /* 0x00 */
    gcSL_MOV, /* 0x01 */
    gcSL_SAT, /* 0x02 */
    gcSL_DP3, /* 0x03 */
    gcSL_DP4, /* 0x04 */
    gcSL_ABS, /* 0x05 */
    gcSL_JMP, /* 0x06 */
    gcSL_ADD, /* 0x07 */
    gcSL_MUL, /* 0x08 */
    gcSL_RCP, /* 0x09 */
    gcSL_SUB, /* 0x0A */
    gcSL_KILL, /* 0x0B */
    gcSL_TEXLD, /* 0x0C */
    gcSL_CALL, /* 0x0D */
    gcSL_RET, /* 0x0E */
    gcSL_NORM, /* 0x0F */
    gcSL_MAX, /* 0x10 */
    gcSL_MIN, /* 0x11 */
    gcSL_POW, /* 0x12 */
    gcSL_RSQ, /* 0x13 */
    gcSL_LOG, /* 0x14 */
    gcSL_FRAC, /* 0x15 */
    gcSL_FLOOR, /* 0x16 */
    gcSL_CEIL, /* 0x17 */
    gcSL_CROSS, /* 0x18 */
    gcSL_TEXLDPROJ, /* 0x19 */
    gcSL_TEXBIAS, /* 0x1A */
    gcSL_TEXGRAD, /* 0x1B */
    gcSL_TEXLOD, /* 0x1C */
    gcSL_SIN, /* 0x1D */
    gcSL_COS, /* 0x1E */
    gcSL_TAN, /* 0x1F */
    gcSL_EXP, /* 0x20 */
    gcSL_SIGN, /* 0x21 */
    gcSL_STEP, /* 0x22 */
    gcSL_SQRT, /* 0x23 */
    gcSL_ACOS, /* 0x24 */
    gcSL_ASIN, /* 0x25 */
    gcSL_ATAN, /* 0x26 */
    gcSL_SET, /* 0x27 */
    gcSL_DSX, /* 0x28 */
    gcSL_DSY, /* 0x29 */
    gcSL_FWIDTH, /* 0x2A */
    gcSL_DIV, /* 0x2B */
    gcSL_MOD, /* 0x2C */
    gcSL_AND_BITWISE, /* 0x2D */
    gcSL_OR_BITWISE, /* 0x2E */
    gcSL_XOR_BITWISE, /* 0x2F */
    gcSL_NOT_BITWISE, /* 0x30 */
    gcSL_LSHIFT, /* 0x31 */
    gcSL_RSHIFT, /* 0x32 */
    gcSL_ROTATE, /* 0x33 */
    gcSL_BITSEL, /* 0x34 */
    gcSL_LEADZERO, /* 0x35 */
    gcSL_LOAD, /* 0x36 */
    gcSL_STORE, /* 0x37 */
    gcSL_BARRIER, /* 0x38 */
    gcSL_STORE1, /* 0x39 */
    gcSL_ATOMADD, /* 0x3A */
    gcSL_ATOMSUB, /* 0x3B */
    gcSL_ATOMXCHG, /* 0x3C */
    gcSL_ATOMCMPXCHG, /* 0x3D */
    gcSL_ATOMMIN, /* 0x3E */
    gcSL_ATOMMAX, /* 0x3F */
    gcSL_ATOMOR, /* 0x40 */
    gcSL_ATOMAND, /* 0x41 */
    gcSL_ATOMXOR, /* 0x42 */
    gcSL_TEXLDPCF, /* 0x43 */
    gcSL_TEXLDPCFPROJ, /* 0x44 */
    gcSL_TEXLODQ, /* 0x45  ES31 */
    gcSL_FLUSH, /* 0x46  ES31 */
    gcSL_JMP_ANY, /* 0x47  ES31 */
    gcSL_BITRANGE, /* 0x48  ES31 */
    gcSL_BITRANGE1, /* 0x49  ES31 */
    gcSL_BITEXTRACT, /* 0x4A  ES31 */
    gcSL_BITINSERT, /* 0x4B  ES31 */
    gcSL_FINDLSB, /* 0x4C  ES31 */
    gcSL_FINDMSB, /* 0x4D  ES31 */
    gcSL_IMAGE_OFFSET, /* 0x4E  ES31 */
    gcSL_IMAGE_ADDR, /* 0x4F  ES31 */
    gcSL_SINPI, /* 0x50 */
    gcSL_COSPI, /* 0x51 */
    gcSL_TANPI, /* 0x52 */
    gcSL_ADDLO, /* 0x53 */  /* Float only. */
    gcSL_MULLO, /* 0x54 */  /* Float only. */
    gcSL_CONV, /* 0x55 */
    gcSL_GETEXP, /* 0x56 */
    gcSL_GETMANT, /* 0x57 */
    gcSL_MULHI, /* 0x58 */  /* Integer only. */
    gcSL_CMP, /* 0x59 */
    gcSL_I2F, /* 0x5A */
    gcSL_F2I, /* 0x5B */
    gcSL_ADDSAT, /* 0x5C */  /* Integer only. */
    gcSL_SUBSAT, /* 0x5D */  /* Integer only. */
    gcSL_MULSAT, /* 0x5E */  /* Integer only. */
    gcSL_DP2, /* 0x5F */
    gcSL_UNPACK, /* 0x60 */
    gcSL_IMAGE_WR, /* 0x61 */
    gcSL_SAMPLER_ADD, /* 0x62 */
    gcSL_MOVA, /* 0x63, HW MOVAR/MOVF/MOVI, VIRCG only */
    gcSL_IMAGE_RD, /* 0x64 */
    gcSL_IMAGE_SAMPLER, /* 0x65 */
    gcSL_NORM_MUL, /* 0x66  VIRCG only */
    gcSL_NORM_DP2, /* 0x67  VIRCG only */
    gcSL_NORM_DP3, /* 0x68  VIRCG only */
    gcSL_NORM_DP4, /* 0x69  VIRCG only */
    gcSL_PRE_DIV, /* 0x6A  VIRCG only */
    gcSL_PRE_LOG2, /* 0x6B  VIRCG only */
    gcSL_TEXGATHER, /* 0x6C  ES31 */
    gcSL_TEXFETCH_MS, /* 0x6D  ES31 */
    gcSL_POPCOUNT, /* 0x6E  ES31(OCL1.2)*/
    gcSL_BIT_REVERSAL, /* 0x6F  ES31 */
    gcSL_BYTE_REVERSAL, /* 0x70  ES31 */
    gcSL_TEXPCF, /* 0x71  ES31 */
    gcSL_UCARRY, /* 0x72  ES31 UCARRY is a condition op, while gcSL
                                                 has not enough bits to represent more */

    gcSL_TEXU, /* 0x73  paired with gcSL_TEXLD to implement HW texld_u_plain */
    gcSL_TEXU_LOD, /* 0x74  paired with gcSL_TEXLD to implement HW texld_u_lod */
    gcSL_MEM_BARRIER, /* 0x75  Memory Barrier. */
    gcSL_SAMPLER_ASSIGN, /* 0x76  Sampler assignment as a parameter, only exist on FE. */
    gcSL_GET_SAMPLER_IDX, /* 0x77  Get Image/Sampler index */

    gcSL_IMAGE_RD_3D, /* 0x78 */
    gcSL_IMAGE_WR_3D, /* 0x79 */
    gcSL_CLAMP0MAX, /* 0x7A clamp0max dest, value, max */
    gcSL_FMA_MUL, /* 0x7B FMA first part: MUL */
    gcSL_FMA_ADD, /* 0x7C FMA second part: ADD */
    gcSL_ATTR_ST, /* 0x7D ATTR_ST attribute(0+temp(1).x), InvocationIndex, val */
    gcSL_ATTR_LD, /* 0x7E ATTR_LD dest, attribute(0+temp(1).x), InvocationIndex */
    gcSL_EMIT_VERTEX, /* 0x7F For function "EmitVertex" */
    gcSL_END_PRIMITIVE, /* 0x80 For function "EndPrimitive" */
    gcSL_ARCTRIG0, /* 0x81 For triangle functions */
    gcSL_ARCTRIG1, /* 0x82 For triangle functions */
    gcSL_MUL_Z, /* 0x83 special mul, resulting in 0 from inf * 0 */
    gcSL_NEG, /* 0x84 neg(a) is similar to (0 - (a)) */
    gcSL_LONGLO, /* 0x85 get the lower 4 bytes of a long/ulong integer */
    gcSL_LONGHI, /* 0x86 get the upper 4 bytes of a long/ulong integer */
    gcSL_MOV_LONG, /* 0x87 mov two 4 byte integers to the lower/upper 4 bytes of a long/ulong integer */
    gcSL_MADSAT, /* 0x88 mad with saturation for integer only */
    gcSL_COPY, /* 0x89 copy register contents */
    gcSL_LOAD_L, /* 0x8A load data from local memory */
    gcSL_STORE_L, /* 0x8B store data to local memory */
    gcSL_IMAGE_ADDR_3D, /* 0x8C */
    gcSL_GET_SAMPLER_LMM, /* 0x8D Get sampler's lodminmax */
    gcSL_GET_SAMPLER_LBS, /* 0x8E Get sampler's levelbasesize */
    gcSL_TEXLD_U, /* 0x8F For TEXLD_U, use the format of coord to select FLOAT/INT/UNSIGINED. */
    gcSL_PARAM_CHAIN, /* 0x90 No specific semantic, only used to chain two sources to one dest. */
    gcSL_INTRINSIC, /* 0x91 Instrinsic dest, IntrinsicId, Param */
    gcSL_INTRINSIC_ST, /* 0x92 Instrinsic dest, IntrinsicId, Param; Param is stored implicitly */
    gcSL_CMAD, /* 0x93 Complex number mad. */
    gcSL_CONJ, /* 0x94 Conjugate modifier. */
    gcSL_CMUL, /* 0x95 Complex number mul. */
    gcSL_CMADCJ, /* 0x96 Complex number conjugate mad. */
    gcSL_CMULCJ, /* 0x97 Complex number conjugate mul. */
    gcSL_CADDCJ, /* 0x98 Complex number conjugate add. */
    gcSL_CSUBCJ, /* 0x99 Complex number conjugate sub. */
    gcSL_CADD, /* 0x9A Complex number add. */
    gcSL_GET_IMAGE_TYPE, /* 0x9B get the image type: 0-1d, 1-1dbuffer, 2-1darray, 3-2d, 4-2darray, 5-3d */
    gcSL_CLAMPCOORD, /* 0x9C clamp image 2d cooridate to its width and height */
    gcSL_EMIT_STREAM_VERTEX, /* 0x9D For function "EmitStreamVertex" */
    gcSL_END_STREAM_PRIMITIVE, /* 0x9E For function "EndStreamPrimitive" */
    gcSL_CTZ, /* 0x9F For function "ctz()" */
    gcSL_MAXOPCODE
}
gcSL_OPCODE;

#define gcSL_isOpcodeTexld(Opcode)         ((Opcode) == gcSL_TEXLD         ||    \
                                            (Opcode) == gcSL_TEXLD_U       ||    \
                                            (Opcode) == gcSL_TEXLDPROJ     ||    \
                                            (Opcode) == gcSL_TEXLDPCF      ||    \
                                            (Opcode) == gcSL_TEXLODQ       ||    \
                                            (Opcode) == gcSL_TEXLDPCFPROJ)

#define gcSL_isOpcodeTexldModifier(Opcode) ((Opcode) == gcSL_TEXBIAS ||    \
                                            (Opcode) == gcSL_TEXLOD ||     \
                                            (Opcode) == gcSL_TEXGRAD ||    \
                                            (Opcode) == gcSL_TEXGATHER ||  \
                                            (Opcode) == gcSL_TEXFETCH_MS || \
                                            (Opcode) == gcSL_TEXU || \
                                            (Opcode) == gcSL_TEXU_LOD)

#define gcSL_isOpcodeAtom(Opcode)          ((Opcode) == gcSL_ATOMADD       ||   \
                                            (Opcode) == gcSL_ATOMSUB       ||   \
                                            (Opcode) == gcSL_ATOMXCHG      ||   \
                                            (Opcode) == gcSL_ATOMCMPXCHG   ||   \
                                            (Opcode) == gcSL_ATOMMIN       ||   \
                                            (Opcode) == gcSL_ATOMMAX       ||   \
                                            (Opcode) == gcSL_ATOMOR        ||   \
                                            (Opcode) == gcSL_ATOMAND       ||   \
                                            (Opcode) == gcSL_ATOMXOR)

/* according to decode[] */
#define gcSL_isOpcodeHaveNoTarget(Opcode)       ((Opcode) == gcSL_NOP                 ||  \
                                                 (Opcode) == gcSL_JMP                 ||  \
                                                 (Opcode) == gcSL_JMP_ANY             ||  \
                                                 (Opcode) == gcSL_KILL                ||  \
                                                 (Opcode) == gcSL_CALL                ||  \
                                                 (Opcode) == gcSL_RET                 ||  \
                                                 (Opcode) == gcSL_TEXBIAS             ||  \
                                                 (Opcode) == gcSL_TEXGRAD             ||  \
                                                 (Opcode) == gcSL_TEXLOD              ||  \
                                                 (Opcode) == gcSL_BARRIER             ||  \
                                                 (Opcode) == gcSL_FLUSH               ||  \
                                                 (Opcode) == gcSL_IMAGE_OFFSET        ||  \
                                                 (Opcode) == gcSL_IMAGE_SAMPLER       ||  \
                                                 (Opcode) == gcSL_TEXGATHER           ||  \
                                                 (Opcode) == gcSL_TEXFETCH_MS         ||  \
                                                 (Opcode) == gcSL_TEXPCF              ||  \
                                                 (Opcode) == gcSL_TEXU                ||  \
                                                 (Opcode) == gcSL_TEXU_LOD            ||  \
                                                 (Opcode) == gcSL_MEM_BARRIER         ||  \
                                                 (Opcode) == gcSL_EMIT_VERTEX         ||  \
                                                 (Opcode) == gcSL_END_PRIMITIVE       ||  \
                                                 (Opcode) == gcSL_EMIT_STREAM_VERTEX  ||  \
                                                 (Opcode) == gcSL_END_STREAM_PRIMITIVE)

#define gcSL_isOpcodeUseTargetAsSource(Opcode)  ((Opcode) == gcSL_STORE               ||  \
                                                 (Opcode) == gcSL_IMAGE_WR            ||  \
                                                 (Opcode) == gcSL_IMAGE_WR_3D         ||  \
                                                 (Opcode) == gcSL_ATTR_ST)

#define gcSL_isOpcodeTargetInvalid(Opcode)      ((Opcode) == gcSL_STORE               ||  \
                                                 (Opcode) == gcSL_STORE1              ||  \
                                                 (Opcode) == gcSL_STORE_L             ||  \
                                                 (Opcode) == gcSL_IMAGE_WR            ||  \
                                                 (Opcode) == gcSL_IMAGE_WR_3D         ||  \
                                                 (Opcode) == gcSL_ATTR_ST)

#define gcSL_isOpcodeImageRead(Opcode)          ((Opcode) == gcSL_IMAGE_RD            ||  \
                                                 (Opcode) == gcSL_IMAGE_RD_3D)

#define gcSL_isOpcodeImageWrite(Opcode)         ((Opcode) == gcSL_IMAGE_WR            ||  \
                                                 (Opcode) == gcSL_IMAGE_WR_3D)

#define gcSL_isOpcodeImageAddr(Opcode)          ((Opcode) == gcSL_IMAGE_ADDR          ||  \
                                                 (Opcode) == gcSL_IMAGE_ADDR_3D)

#define gcSL_isOpcodeImageRelated(Opcode)       (gcSL_isOpcodeImageRead(Opcode)       ||  \
                                                 gcSL_isOpcodeImageWrite(Opcode)      ||  \
                                                 gcSL_isOpcodeImageAddr(Opcode))

#define gcSL_isComplexRelated(Opcode)           ((Opcode) == gcSL_CMAD                ||  \
                                                 (Opcode) == gcSL_CMUL                ||  \
                                                 (Opcode) == gcSL_CONJ                ||  \
                                                 (Opcode) == gcSL_CMADCJ              ||  \
                                                 (Opcode) == gcSL_CMULCJ              ||  \
                                                 (Opcode) == gcSL_CADDCJ              ||  \
                                                 (Opcode) == gcSL_CSUBCJ                  \
                                                 )

typedef enum _gcSL_OPCODE_ATTR_RESULT_PRECISION
{
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_INVALID = 0,
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_HIA = 1, /* highest precision in both operands */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_AF = 2, /* as the precision of the first operand */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_AS = 3, /* as the precision of the second operand */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_HP = 4, /* highp */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_MP = 5, /* mediump */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_LP = 6, /* lowp */
    _gcSL_OPCODE_ATTR_RESULT_PRECISION_IGNORE = 7, /* the precision doesn't matter */
} gcSL_OPCODE_ATTR_RESULT_PRECISION;

typedef struct _gcSL_OPCODE_ATTR
{
    gcSL_OPCODE_ATTR_RESULT_PRECISION resultPrecision;
}
gcSL_OPCODE_ATTR;

extern const gcSL_OPCODE_ATTR gcvOpcodeAttr[];

typedef enum _gcSL_MEMORY_SCOPE
{
    gcSL_MEMORY_SCOPE_CROSS_DEVICE      = 0,
    gcSL_MEMORY_SCOPE_DEVICE            = 1,
    gcSL_MEMORY_SCOPE_WORKGROUP         = 2,
    gcSL_MEMORY_SCOPE_SUBGROUP          = 3,
    gcSL_MEMORY_SCOPE_INVOCATION        = 4,
    gcSL_MEMORY_SCOPE_QUEUE_FAMILY      = 5,
}
gcSL_MEMORY_SCOPE;

/* Match SPIR-V spec. */
typedef enum _gcSL_MEMORY_SEMANTIC_FLAG
{
    gcSL_MEMORY_SEMANTIC_RELAXED                = 0x0,
    gcSL_MEMORY_SEMANTIC_ACQUIRE                = 0x2,
    gcSL_MEMORY_SEMANTIC_RELEASE                = 0x4,
    gcSL_MEMORY_SEMANTIC_ACQUIRERELEASE         = 0x8,
    gcSL_MEMORY_SEMANTIC_SEQUENTIALLYCONSISTENT = 0x10,
    gcSL_MEMORY_SEMANTIC_UNIFORMMEMORY          = 0x40,
    gcSL_MEMORY_SEMANTIC_SUBGROUPMEMORY         = 0x80,
    gcSL_MEMORY_SEMANTIC_WORKGROUPMEMORY        = 0x100,
    gcSL_MEMORY_SEMANTIC_CROSSWORKGROUPMEMORY   = 0x200,
    gcSL_MEMORY_SEMANTIC_ATOMICCOUNTERMEMORY    = 0x400,
    gcSL_MEMORY_SEMANTIC_IMAGEMEMORY            = 0x800,
    gcSL_MEMORY_SEMANTIC_OUTPUTMEMORY           = 0x1000,
    gcSL_MEMORY_SEMANTIC_MAKEAVAILABLE          = 0x2000,
    gcSL_MEMORY_SEMANTIC_MAKEVISIBLE            = 0x4000,
    gcSL_MEMORY_SEMANTIC_VOLATILE               = 0x8000,
}gcSL_MEMORY_SEMANTIC_FLAG;

typedef enum _gcSL_FORMAT
{
    gcSL_FLOAT = 0, /* 0 */
    gcSL_INTEGER = 1, /* 1 */
    gcSL_INT32 = 1, /* 1 */
    gcSL_BOOLEAN = 2, /* 2 */
    gcSL_UINT32 = 3, /* 3 */
    gcSL_INT8, /* 4 */
    gcSL_UINT8, /* 5 */
    gcSL_INT16, /* 6 */
    gcSL_UINT16, /* 7 */
    gcSL_INT64, /* 8 */
    gcSL_UINT64, /* 9 */
    gcSL_SNORM8, /* 10 */
    gcSL_UNORM8, /* 11 */
    gcSL_FLOAT16, /* 12 */
    gcSL_FLOAT64, /* 13 */    /* Reserved for future enhancement. */
    gcSL_SNORM16, /* 14 */
    gcSL_UNORM16, /* 15 */
    gcSL_INVALID, /* 16 */
/* the following is extended for OCL 1.2 for type name querying
   the lower 4 bits are defined as above.
   An aggregate format can be formed from or'ing together a value from above
   and another value from below
*/
    gcSL_VOID      = 0x011, /* to support OCL 1.2 for querying */
    gcSL_SAMPLER_T = 0x012, /* OCL sampler_t */
    gcSL_SIZE_T    = 0x013, /* OCL size_t */
    gcSL_EVENT_T   = 0x014, /* OCL event */
    gcSL_PTRDIFF_T = 0x015, /* OCL prtdiff_t */
    gcSL_INTPTR_T  = 0x016, /* OCL intptr_t */
    gcSL_UINTPTR_T = 0x017, /* OCL uintptr_t */
    gcSL_STRUCT    = 0x100, /* OCL struct */
    gcSL_UNION     = 0x200, /* OCL union */
    gcSL_ENUM      = 0x300, /* OCL enum */
    gcSL_TYPEDEF   = 0x400, /* OCL typedef */
}
gcSL_FORMAT;

#define GetFormatBasicType(f)       ((f) & 0xFF)
#define GetFormatSpecialType(f)     ((f) & ~0xFF)
#define isFormatSpecialType(f)      (GetFormatSpecialType(f) != 0)
#define SetFormatSpecialType(f, t)  do {                                                        \
                                        gcmASSERT(isFormatSpecialType(t));                      \
                                        (f) = GetFormatBasicType(f) | GetFormatSpecialType(t);  \
                                    } while (0)

#define isFormat64bit(f)   ((f) == gcSL_INT64 || (f) == gcSL_UINT64)

/* Destination write enable bits. */
typedef enum _gcSL_ENABLE
{
    gcSL_ENABLE_NONE                     = 0x0, /* none is enabled, error/uninitialized state */
    gcSL_ENABLE_X                        = 0x1,
    gcSL_ENABLE_Y                        = 0x2,
    gcSL_ENABLE_Z                        = 0x4,
    gcSL_ENABLE_W                        = 0x8,
    /* Combinations. */
    gcSL_ENABLE_XY                       = gcSL_ENABLE_X | gcSL_ENABLE_Y,
    gcSL_ENABLE_XYZ                      = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_Z,
    gcSL_ENABLE_XYZW                     = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_XYW                      = gcSL_ENABLE_X | gcSL_ENABLE_Y | gcSL_ENABLE_W,
    gcSL_ENABLE_XZ                       = gcSL_ENABLE_X | gcSL_ENABLE_Z,
    gcSL_ENABLE_XZW                      = gcSL_ENABLE_X | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_XW                       = gcSL_ENABLE_X | gcSL_ENABLE_W,
    gcSL_ENABLE_YZ                       = gcSL_ENABLE_Y | gcSL_ENABLE_Z,
    gcSL_ENABLE_YZW                      = gcSL_ENABLE_Y | gcSL_ENABLE_Z | gcSL_ENABLE_W,
    gcSL_ENABLE_YW                       = gcSL_ENABLE_Y | gcSL_ENABLE_W,
    gcSL_ENABLE_ZW                       = gcSL_ENABLE_Z | gcSL_ENABLE_W,
}
gcSL_ENABLE;

#define gcmEnableChannelCount(enable)          \
    (((enable) & 0x1) + (((enable) & 0x2) >> 1) + (((enable) & 0x4) >> 2) + (((enable) & 0x8) >> 3))

/* Possible indices. */
typedef enum _gcSL_INDEXED
{
    gcSL_NOT_INDEXED, /* 0 */
    gcSL_INDEXED_X, /* 1 */
    gcSL_INDEXED_Y, /* 2 */
    gcSL_INDEXED_Z, /* 3 */
    gcSL_INDEXED_W, /* 4 */
}
gcSL_INDEXED;

/* Possible indices. */
typedef enum _gcSL_INDEXED_LEVEL
{
    gcSL_NONE_INDEXED, /* 0 */
    gcSL_LEAF_INDEXED, /* 1 */
    gcSL_NODE_INDEXED, /* 2 */
    gcSL_LEAF_AND_NODE_INDEXED             /* 3 */
}
gcSL_INDEXED_LEVEL;

/* Opcode conditions. */
typedef enum _gcSL_CONDITION
{
    gcSL_ALWAYS, /* 0x0 */
    gcSL_NOT_EQUAL, /* 0x1 */
    gcSL_LESS_OR_EQUAL, /* 0x2 */
    gcSL_LESS, /* 0x3 */
    gcSL_EQUAL, /* 0x4 */
    gcSL_GREATER, /* 0x5 */
    gcSL_GREATER_OR_EQUAL, /* 0x6 */
    gcSL_AND, /* 0x7 */
    gcSL_OR, /* 0x8 */
    gcSL_XOR, /* 0x9 */
    gcSL_NOT_ZERO, /* 0xA */
    gcSL_ZERO, /* 0xB */
    gcSL_GREATER_OR_EQUAL_ZERO, /* 0xC */
    gcSL_GREATER_ZERO, /* 0xD */
    gcSL_LESS_OREQUAL_ZERO, /* 0xE */
    gcSL_LESS_ZERO, /* 0xF */
    gcSL_ALLMSB, /* 0x10*/
    gcSL_ANYMSB, /* 0x11*/
    gcSL_SELMSB, /* 0x12*/
}
gcSL_CONDITION;

/* Possible source operand types. */
typedef enum _gcSL_TYPE
{
    gcSL_NONE, /* 0x0 */
    gcSL_TEMP, /* 0x1 */
    gcSL_ATTRIBUTE, /* 0x2 */
    gcSL_UNIFORM, /* 0x3 */
    gcSL_SAMPLER, /* 0x4 */
    gcSL_CONSTANT, /* 0x5 */
    gcSL_OUTPUT, /* 0x6 */
}
gcSL_TYPE;

/* Swizzle generator macro. */
#define gcmSWIZZLE(Component1, Component2, Component3, Component4) \
(\
    (gcSL_SWIZZLE_ ## Component1 << 0) | \
    (gcSL_SWIZZLE_ ## Component2 << 2) | \
    (gcSL_SWIZZLE_ ## Component3 << 4) | \
    (gcSL_SWIZZLE_ ## Component4 << 6)   \
)

#define gcmExtractSwizzle(Swizzle, Index) \
    ((gcSL_SWIZZLE) ((((Swizzle) >> (Index * 2)) & 0x3)))

#define gcmComposeSwizzle(SwizzleX, SwizzleY, SwizzleZ, SwizzleW) \
(\
    ((SwizzleX) << 0) | \
    ((SwizzleY) << 2) | \
    ((SwizzleZ) << 4) | \
    ((SwizzleW) << 6)   \
)

/* Possible swizzle values. */
typedef enum _gcSL_SWIZZLE
{
    gcSL_SWIZZLE_X, /* 0x0 */
    gcSL_SWIZZLE_Y, /* 0x1 */
    gcSL_SWIZZLE_Z, /* 0x2 */
    gcSL_SWIZZLE_W, /* 0x3 */
    /* Combinations. */
    gcSL_SWIZZLE_XXXX = gcmSWIZZLE(X, X, X, X),
    gcSL_SWIZZLE_YYYY = gcmSWIZZLE(Y, Y, Y, Y),
    gcSL_SWIZZLE_ZZZZ = gcmSWIZZLE(Z, Z, Z, Z),
    gcSL_SWIZZLE_WWWW = gcmSWIZZLE(W, W, W, W),
    gcSL_SWIZZLE_XYYY = gcmSWIZZLE(X, Y, Y, Y),
    gcSL_SWIZZLE_XZZZ = gcmSWIZZLE(X, Z, Z, Z),
    gcSL_SWIZZLE_XWWW = gcmSWIZZLE(X, W, W, W),
    gcSL_SWIZZLE_YZZZ = gcmSWIZZLE(Y, Z, Z, Z),
    gcSL_SWIZZLE_YWWW = gcmSWIZZLE(Y, W, W, W),
    gcSL_SWIZZLE_ZWWW = gcmSWIZZLE(Z, W, W, W),
    gcSL_SWIZZLE_XYZZ = gcmSWIZZLE(X, Y, Z, Z),
    gcSL_SWIZZLE_XYWW = gcmSWIZZLE(X, Y, W, W),
    gcSL_SWIZZLE_XZWW = gcmSWIZZLE(X, Z, W, W),
    gcSL_SWIZZLE_YZWW = gcmSWIZZLE(Y, Z, W, W),
    gcSL_SWIZZLE_XXYZ = gcmSWIZZLE(X, X, Y, Z),
    gcSL_SWIZZLE_XYZW = gcmSWIZZLE(X, Y, Z, W),
    gcSL_SWIZZLE_XYXY = gcmSWIZZLE(X, Y, X, Y),
    gcSL_SWIZZLE_YYZZ = gcmSWIZZLE(Y, Y, Z, Z),
    gcSL_SWIZZLE_YYWW = gcmSWIZZLE(Y, Y, W, W),
    gcSL_SWIZZLE_ZZZW = gcmSWIZZLE(Z, Z, Z, W),
    gcSL_SWIZZLE_XZZW = gcmSWIZZLE(X, Z, Z, W),
    gcSL_SWIZZLE_YYZW = gcmSWIZZLE(Y, Y, Z, W),
    gcSL_SWIZZLE_XXXY = gcmSWIZZLE(X, X, X, Y),
    gcSL_SWIZZLE_XZXZ = gcmSWIZZLE(X, Z, X, Z),
    gcSL_SWIZZLE_YWWY = gcmSWIZZLE(Y, W, W, Y),
    gcSL_SWIZZLE_ZWZW = gcmSWIZZLE(Z, W, Z, W),

    gcSL_SWIZZLE_INVALID = 0x7FFFFFFF
}
gcSL_SWIZZLE;

typedef enum _gcSL_COMPONENT
{
    gcSL_COMPONENT_X, /* 0x0 */
    gcSL_COMPONENT_Y, /* 0x1 */
    gcSL_COMPONENT_Z, /* 0x2 */
    gcSL_COMPONENT_W, /* 0x3 */
    gcSL_COMPONENT_COUNT            /* 0x4 */
} gcSL_COMPONENT;

#define gcmIsComponentEnabled(Enable, Component) (((Enable) & (1 << (Component))) != 0)

/* Rounding modes. */
typedef enum _gcSL_ROUND
{
    gcSL_ROUND_DEFAULT, /* 0x0 */
    gcSL_ROUND_RTZ, /* 0x1 */
    gcSL_ROUND_RTNE, /* 0x2 */
    gcSL_ROUND_RTP, /* 0x3 */
    gcSL_ROUND_RTN                  /* 0x4 */
} gcSL_ROUND;

/* Saturation */
typedef enum _gcSL_MODIFIER_SAT
{
    gcSL_NO_SATURATE, /* 0x0 */
    gcSL_SATURATE            /* 0x1 */
} gcSL_MODIFIER_SAT;

/* Opcode flag modes. */
typedef enum _gcSL_OPCODE_RES_TYPE
{
    gcSL_OPCODE_RES_TYPE_NONE           = 0x0,
    gcSL_OPCODE_RES_TYPE_FETCH          = 0x1,
    gcSL_OPCODE_RES_TYPE_FETCH_MS       = 0x2,
} gcSL_OPCODE_RES_TYPE;

/* Precision setting. */
typedef enum _gcSL_PRECISION
{
    gcSL_PRECISION_DEFAULT, /* 0x0 */
    gcSL_PRECISION_LOW, /* 0x1 */
    gcSL_PRECISION_MEDIUM, /* 0x2 */
    gcSL_PRECISION_HIGH, /* 0x3 */
    gcSL_PRECISION_ANY, /* 0x4 */
} gcSL_PRECISION;

/* gcSHADER_TYPE enumeration. */
typedef enum _gcSHADER_TYPE
{
    gcSHADER_FLOAT_X1   = 0, /* 0x00 */
    gcSHADER_FLOAT_X2, /* 0x01 */
    gcSHADER_FLOAT_X3, /* 0x02 */
    gcSHADER_FLOAT_X4, /* 0x03 */
    gcSHADER_FLOAT_2X2, /* 0x04 */
    gcSHADER_FLOAT_3X3, /* 0x05 */
    gcSHADER_FLOAT_4X4, /* 0x06 */
    gcSHADER_BOOLEAN_X1, /* 0x07 */
    gcSHADER_BOOLEAN_X2, /* 0x08 */
    gcSHADER_BOOLEAN_X3, /* 0x09 */
    gcSHADER_BOOLEAN_X4, /* 0x0A */
    gcSHADER_INTEGER_X1, /* 0x0B */
    gcSHADER_INTEGER_X2, /* 0x0C */
    gcSHADER_INTEGER_X3, /* 0x0D */
    gcSHADER_INTEGER_X4, /* 0x0E */
    gcSHADER_SAMPLER_1D, /* 0x0F */
    gcSHADER_SAMPLER_2D, /* 0x10 */
    gcSHADER_SAMPLER_3D, /* 0x11 */
    gcSHADER_SAMPLER_CUBIC, /* 0x12 */
    gcSHADER_FIXED_X1, /* 0x13 */
    gcSHADER_FIXED_X2, /* 0x14 */
    gcSHADER_FIXED_X3, /* 0x15 */
    gcSHADER_FIXED_X4, /* 0x16 */

    /* For OCL. */
    gcSHADER_IMAGE_1D_T, /* 0x17 */
    gcSHADER_IMAGE_1D_BUFFER_T, /* 0x18 */
    gcSHADER_IMAGE_1D_ARRAY_T, /* 0x19 */
    gcSHADER_IMAGE_2D_T, /* 0x1A */
    gcSHADER_IMAGE_2D_ARRAY_T, /* 0x1B */
    gcSHADER_IMAGE_3D_T, /* 0x1C */
    gcSHADER_VIV_GENERIC_IMAGE_T, /* 0x1D */
    gcSHADER_SAMPLER_T, /* 0x1E */

    gcSHADER_FLOAT_2X3, /* 0x1F */
    gcSHADER_FLOAT_2X4, /* 0x20 */
    gcSHADER_FLOAT_3X2, /* 0x21 */
    gcSHADER_FLOAT_3X4, /* 0x22 */
    gcSHADER_FLOAT_4X2, /* 0x23 */
    gcSHADER_FLOAT_4X3, /* 0x24 */
    gcSHADER_ISAMPLER_2D, /* 0x25 */
    gcSHADER_ISAMPLER_3D, /* 0x26 */
    gcSHADER_ISAMPLER_CUBIC, /* 0x27 */
    gcSHADER_USAMPLER_2D, /* 0x28 */
    gcSHADER_USAMPLER_3D, /* 0x29 */
    gcSHADER_USAMPLER_CUBIC, /* 0x2A */
    gcSHADER_SAMPLER_EXTERNAL_OES, /* 0x2B */

    gcSHADER_UINT_X1, /* 0x2C */
    gcSHADER_UINT_X2, /* 0x2D */
    gcSHADER_UINT_X3, /* 0x2E */
    gcSHADER_UINT_X4, /* 0x2F */

    gcSHADER_SAMPLER_2D_SHADOW, /* 0x30 */
    gcSHADER_SAMPLER_CUBE_SHADOW, /* 0x31 */

    gcSHADER_SAMPLER_1D_ARRAY, /* 0x32 */
    gcSHADER_SAMPLER_1D_ARRAY_SHADOW, /* 0x33 */
    gcSHADER_SAMPLER_2D_ARRAY, /* 0x34 */
    gcSHADER_ISAMPLER_2D_ARRAY, /* 0x35 */
    gcSHADER_USAMPLER_2D_ARRAY, /* 0x36 */
    gcSHADER_SAMPLER_2D_ARRAY_SHADOW, /* 0x37 */

    gcSHADER_SAMPLER_2D_MS, /* 0x38 */
    gcSHADER_ISAMPLER_2D_MS, /* 0x39 */
    gcSHADER_USAMPLER_2D_MS, /* 0x3A */
    gcSHADER_SAMPLER_2D_MS_ARRAY, /* 0x3B */
    gcSHADER_ISAMPLER_2D_MS_ARRAY, /* 0x3C */
    gcSHADER_USAMPLER_2D_MS_ARRAY, /* 0x3D */

    gcSHADER_IMAGE_2D, /* 0x3E */
    gcSHADER_IIMAGE_2D, /* 0x3F */
    gcSHADER_UIMAGE_2D, /* 0x40 */
    gcSHADER_IMAGE_3D, /* 0x41 */
    gcSHADER_IIMAGE_3D, /* 0x42 */
    gcSHADER_UIMAGE_3D, /* 0x43 */
    gcSHADER_IMAGE_CUBE, /* 0x44 */
    gcSHADER_IIMAGE_CUBE, /* 0x45 */
    gcSHADER_UIMAGE_CUBE, /* 0x46 */
    gcSHADER_IMAGE_2D_ARRAY, /* 0x47 */
    gcSHADER_IIMAGE_2D_ARRAY, /* 0x48 */
    gcSHADER_UIMAGE_2D_ARRAY, /* 0x49 */
    gcSHADER_VIV_GENERIC_GL_IMAGE, /* 0x4A */

    gcSHADER_ATOMIC_UINT, /* 0x4B */

    /* GL_EXT_texture_cube_map_array */
    gcSHADER_SAMPLER_CUBEMAP_ARRAY, /* 0x4C */
    gcSHADER_SAMPLER_CUBEMAP_ARRAY_SHADOW, /* 0x4D */
    gcSHADER_ISAMPLER_CUBEMAP_ARRAY, /* 0x4E */
    gcSHADER_USAMPLER_CUBEMAP_ARRAY, /* 0x4F */
    gcSHADER_IMAGE_CUBEMAP_ARRAY, /* 0x50 */
    gcSHADER_IIMAGE_CUBEMAP_ARRAY, /* 0x51 */
    gcSHADER_UIMAGE_CUBEMAP_ARRAY, /* 0x52 */

    gcSHADER_INT64_X1, /* 0x53 */
    gcSHADER_INT64_X2, /* 0x54 */
    gcSHADER_INT64_X3, /* 0x55 */
    gcSHADER_INT64_X4, /* 0x56 */
    gcSHADER_UINT64_X1, /* 0x57 */
    gcSHADER_UINT64_X2, /* 0x58 */
    gcSHADER_UINT64_X3, /* 0x59 */
    gcSHADER_UINT64_X4, /* 0x5A */

    /* texture buffer extension type. */
    gcSHADER_SAMPLER_BUFFER, /* 0x5B */
    gcSHADER_ISAMPLER_BUFFER, /* 0x5C */
    gcSHADER_USAMPLER_BUFFER, /* 0x5D */
    gcSHADER_IMAGE_BUFFER, /* 0x5E */
    gcSHADER_IIMAGE_BUFFER, /* 0x5F */
    gcSHADER_UIMAGE_BUFFER, /* 0x60 */
    gcSHADER_VIV_GENERIC_GL_SAMPLER, /* 0x61 */

    /* float16 */
    gcSHADER_FLOAT16_X1, /* 0x62 half2 */
    gcSHADER_FLOAT16_X2, /* 0x63 half2 */
    gcSHADER_FLOAT16_X3, /* 0x64 half3 */
    gcSHADER_FLOAT16_X4, /* 0x65 half4 */
    gcSHADER_FLOAT16_X8, /* 0x66 half8 */
    gcSHADER_FLOAT16_X16, /* 0x67 half16 */
    gcSHADER_FLOAT16_X32, /* 0x68 half32 */

    /*  boolean  */
    gcSHADER_BOOLEAN_X8, /* 0x69 */
    gcSHADER_BOOLEAN_X16, /* 0x6A */
    gcSHADER_BOOLEAN_X32, /* 0x6B */

    /* uchar vectors  */
    gcSHADER_UINT8_X1, /* 0x6C */
    gcSHADER_UINT8_X2, /* 0x6D */
    gcSHADER_UINT8_X3, /* 0x6E */
    gcSHADER_UINT8_X4, /* 0x6F */
    gcSHADER_UINT8_X8, /* 0x70 */
    gcSHADER_UINT8_X16, /* 0x71 */
    gcSHADER_UINT8_X32, /* 0x72 */

    /* char vectors  */
    gcSHADER_INT8_X1, /* 0x73 */
    gcSHADER_INT8_X2, /* 0x74 */
    gcSHADER_INT8_X3, /* 0x75 */
    gcSHADER_INT8_X4, /* 0x76 */
    gcSHADER_INT8_X8, /* 0x77 */
    gcSHADER_INT8_X16, /* 0x78 */
    gcSHADER_INT8_X32, /* 0x79 */

    /* ushort vectors */
    gcSHADER_UINT16_X1, /* 0x7A */
    gcSHADER_UINT16_X2, /* 0x7B */
    gcSHADER_UINT16_X3, /* 0x7C */
    gcSHADER_UINT16_X4, /* 0x7D */
    gcSHADER_UINT16_X8, /* 0x7E */
    gcSHADER_UINT16_X16, /* 0x7F */
    gcSHADER_UINT16_X32, /* 0x80 */

    /* short vectors */
    gcSHADER_INT16_X1, /* 0x81 */
    gcSHADER_INT16_X2, /* 0x82 */
    gcSHADER_INT16_X3, /* 0x83 */
    gcSHADER_INT16_X4, /* 0x84 */
    gcSHADER_INT16_X8, /* 0x85 */
    gcSHADER_INT16_X16, /* 0x86 */
    gcSHADER_INT16_X32, /* 0x87 */

    /* packed data type */
    /* packed float16 (2 bytes per element) */
    gcSHADER_FLOAT16_P2, /* 0x88 half2 */
    gcSHADER_FLOAT16_P3, /* 0x89 half3 */
    gcSHADER_FLOAT16_P4, /* 0x8A half4 */
    gcSHADER_FLOAT16_P8, /* 0x8B half8 */
    gcSHADER_FLOAT16_P16, /* 0x8C half16 */
    gcSHADER_FLOAT16_P32, /* 0x8D half32 */

    /* packed boolean (1 byte per element) */
    gcSHADER_BOOLEAN_P2, /* 0x8E bool2 bvec2 */
    gcSHADER_BOOLEAN_P3, /* 0x8F */
    gcSHADER_BOOLEAN_P4, /* 0x90 */
    gcSHADER_BOOLEAN_P8, /* 0x91 */
    gcSHADER_BOOLEAN_P16, /* 0x92 */
    gcSHADER_BOOLEAN_P32, /* 0x93 */

    /* uchar vectors (1 byte per element) */
    gcSHADER_UINT8_P2, /* 0x94 */
    gcSHADER_UINT8_P3, /* 0x95 */
    gcSHADER_UINT8_P4, /* 0x96 */
    gcSHADER_UINT8_P8, /* 0x97 */
    gcSHADER_UINT8_P16, /* 0x98 */
    gcSHADER_UINT8_P32, /* 0x99 */

    /* char vectors (1 byte per element) */
    gcSHADER_INT8_P2, /* 0x9A */
    gcSHADER_INT8_P3, /* 0x9B */
    gcSHADER_INT8_P4, /* 0x9C */
    gcSHADER_INT8_P8, /* 0x9D */
    gcSHADER_INT8_P16, /* 0x9E */
    gcSHADER_INT8_P32, /* 0x9F */

    /* ushort vectors (2 bytes per element) */
    gcSHADER_UINT16_P2, /* 0xA0 */
    gcSHADER_UINT16_P3, /* 0xA1 */
    gcSHADER_UINT16_P4, /* 0xA2 */
    gcSHADER_UINT16_P8, /* 0xA3 */
    gcSHADER_UINT16_P16, /* 0xA4 */
    gcSHADER_UINT16_P32, /* 0xA5 */

    /* short vectors (2 bytes per element) */
    gcSHADER_INT16_P2, /* 0xA6 */
    gcSHADER_INT16_P3, /* 0xA7 */
    gcSHADER_INT16_P4, /* 0xA8 */
    gcSHADER_INT16_P8, /* 0xA9 */
    gcSHADER_INT16_P16, /* 0xAA */
    gcSHADER_INT16_P32, /* 0xAB */

    gcSHADER_INTEGER_X8, /* 0xAC */
    gcSHADER_INTEGER_X16, /* 0xAD */
    gcSHADER_UINT_X8, /* 0xAE */
    gcSHADER_UINT_X16, /* 0xAF */
    gcSHADER_FLOAT_X8, /* 0xB0 */
    gcSHADER_FLOAT_X16, /* 0xB1 */
    gcSHADER_INT64_X8, /* 0xB2 */
    gcSHADER_INT64_X16, /* 0xB3 */
    gcSHADER_UINT64_X8, /* 0xB4 */
    gcSHADER_UINT64_X16, /* 0xB5 */

    gcSHADER_FLOAT64_X1, /* 0xB6 */
    gcSHADER_FLOAT64_X2, /* 0xB7 */
    gcSHADER_FLOAT64_X3, /* 0xB8 */
    gcSHADER_FLOAT64_X4, /* 0xB9 */
    gcSHADER_FLOAT64_2X2, /* 0xBA */
    gcSHADER_FLOAT64_3X3, /* 0xBB */
    gcSHADER_FLOAT64_4X4, /* 0xBC */
    gcSHADER_FLOAT64_2X3, /* 0xBD */
    gcSHADER_FLOAT64_2X4, /* 0xBE */
    gcSHADER_FLOAT64_3X2, /* 0xBF */
    gcSHADER_FLOAT64_3X4, /* 0xC0 */
    gcSHADER_FLOAT64_4X2, /* 0xC1 */
    gcSHADER_FLOAT64_4X3, /* 0xC2 */
    gcSHADER_FLOAT64_X8, /* 0xC3 */
    gcSHADER_FLOAT64_X16, /* 0xC4 */

    /* OpenGL 4.0 types */
    gcSHADER_SAMPLER_2D_RECT, /* 0xC5 */
    gcSHADER_ISAMPLER_2D_RECT, /* 0xC6 */
    gcSHADER_USAMPLER_2D_RECT, /* 0xC7 */
    gcSHADER_SAMPLER_2D_RECT_SHADOW, /* 0xC8 */
    gcSHADER_ISAMPLER_1D_ARRAY, /* 0xC9 */
    gcSHADER_USAMPLER_1D_ARRAY, /* 0xCA */
    gcSHADER_ISAMPLER_1D, /* 0xCB */
    gcSHADER_USAMPLER_1D, /* 0xCC */
    gcSHADER_SAMPLER_1D_SHADOW, /* 0xCD */

    gcSHADER_UNKONWN_TYPE, /* do not add type after this */
    gcSHADER_TYPE_COUNT         /* must to change gcvShaderTypeInfo at the
                                 * same time if you add any new type! */
}
gcSHADER_TYPE;

#define isSamplerType(Type)                               ((Type >= gcSHADER_SAMPLER_1D && Type <= gcSHADER_SAMPLER_CUBIC) ||\
                                                           (Type >= gcSHADER_ISAMPLER_2D && Type <= gcSHADER_SAMPLER_EXTERNAL_OES) ||\
                                                           (Type >= gcSHADER_SAMPLER_2D_SHADOW && Type <= gcSHADER_USAMPLER_2D_MS_ARRAY) ||\
                                                           (Type >= gcSHADER_SAMPLER_CUBEMAP_ARRAY && Type <= gcSHADER_USAMPLER_CUBEMAP_ARRAY) || \
                                                           (Type >= gcSHADER_SAMPLER_BUFFER && Type <= gcSHADER_USAMPLER_BUFFER ) || \
                                                           (Type >= gcSHADER_SAMPLER_2D_RECT && Type <= gcSHADER_SAMPLER_1D_SHADOW ))

#define isShadowSampler(Type)                             ((Type == gcSHADER_SAMPLER_2D_SHADOW)            || \
                                                           (Type == gcSHADER_SAMPLER_CUBE_SHADOW)          || \
                                                           (Type == gcSHADER_SAMPLER_1D_ARRAY_SHADOW)      || \
                                                           (Type == gcSHADER_SAMPLER_2D_ARRAY_SHADOW)      || \
                                                           (Type == gcSHADER_SAMPLER_CUBEMAP_ARRAY_SHADOW) || \
                                                           (Type == gcSHADER_SAMPLER_1D_SHADOW)            || \
                                                           (Type == gcSHADER_SAMPLER_2D_RECT_SHADOW))

#define isOCLImageType(Type)                              (Type >= gcSHADER_IMAGE_1D_T && Type <= gcSHADER_VIV_GENERIC_IMAGE_T)

#define isOGLImageType(Type)                              ((Type >= gcSHADER_IMAGE_2D && Type <= gcSHADER_VIV_GENERIC_GL_IMAGE) ||\
                                                           (Type >= gcSHADER_IMAGE_CUBEMAP_ARRAY && Type <= gcSHADER_UIMAGE_CUBEMAP_ARRAY) || \
                                                           (Type >= gcSHADER_IMAGE_BUFFER && Type <= gcSHADER_UIMAGE_BUFFER))

#define isAtomicType(Type)                                (Type == gcSHADER_ATOMIC_UINT)

#define isNormalType(Type)                                ((!isSamplerType(Type)) && \
                                                           (!isOCLImageType(Type)) && \
                                                           (!isOGLImageType(Type)) && \
                                                           (!isAtomicType(Type)))

typedef enum _gcSHADER_VAR_CATEGORY
{
    gcSHADER_VAR_CATEGORY_NORMAL  =  0, /* primitive type and its array */
    gcSHADER_VAR_CATEGORY_STRUCT, /* structure */
    gcSHADER_VAR_CATEGORY_BLOCK, /* interface block - uniform or storage */
    gcSHADER_VAR_CATEGORY_BLOCK_MEMBER, /* member for uniform or storage block */
    gcSHADER_VAR_CATEGORY_BLOCK_ADDRESS, /* address for uniform or storage block */
    gcSHADER_VAR_CATEGORY_LOD_MIN_MAX, /* lod min max */
    gcSHADER_VAR_CATEGORY_LEVEL_BASE_SIZE, /* base level */
    gcSHADER_VAR_CATEGORY_FUNCTION_INPUT_ARGUMENT, /* function input argument */
    gcSHADER_VAR_CATEGORY_FUNCTION_OUTPUT_ARGUMENT, /* function output argument */
    gcSHADER_VAR_CATEGORY_FUNCTION_INOUT_ARGUMENT, /* function inout argument */
    gcSHADER_VAR_CATEGORY_TOP_LEVEL_STRUCT, /* top level struct, only use for ssbo. */
    gcSHADER_VAR_CATEGORY_SAMPLE_LOCATION, /* sample location. */
    gcSHADER_VAR_CATEGORY_ENABLE_MULTISAMPLE_BUFFERS, /* multisample buffers. */
    gcSHADER_VAR_CATEGORY_TYPE_NAME, /* the name of a type, e.g, a structure type name. */
    gcSHADER_VAR_CATEGORY_GL_SAMPLER_FOR_IMAGE_T,
    gcSHADER_VAR_CATEGORY_GL_IMAGE_FOR_IMAGE_T,
    gcSHADER_VAR_CATEGORY_WORK_THREAD_COUNT, /* the number of concurrent work thread. */
    gcSHADER_VAR_CATEGORY_WORK_GROUP_COUNT, /* the number of concurrent work group. */
    gcSHADER_VAR_CATEGORY_WORK_GROUP_ID_OFFSET, /* the workGroupId offset, for multi-GPU only. */
    gcSHADER_VAR_CATEGORY_GLOBAL_INVOCATION_ID_OFFSET, /* the globalId offset, for multi-GPU only. */
    gcSHADER_VAR_CATEGORY_VIEW_INDEX,
    gcSHADER_VAR_CATEGORY_CLIP_DISTANCE_ENABLE,
}
gcSHADER_VAR_CATEGORY;

typedef enum _gceTYPE_QUALIFIER
{
    gcvTYPE_QUALIFIER_NONE         = 0x0, /* unqualified */
    gcvTYPE_QUALIFIER_VOLATILE     = 0x1, /* volatile */
    gcvTYPE_QUALIFIER_RESTRICT     = 0x2, /* restrict */
    gcvTYPE_QUALIFIER_READ_ONLY    = 0x4, /* readonly */
    gcvTYPE_QUALIFIER_WRITE_ONLY   = 0x8, /* writeonly */
    gcvTYPE_QUALIFIER_CONSTANT     = 0x10, /* constant address space */
    gcvTYPE_QUALIFIER_GLOBAL       = 0x20, /* global address space */
    gcvTYPE_QUALIFIER_LOCAL        = 0x40, /* local address space */
    gcvTYPE_QUALIFIER_PRIVATE      = 0x80, /* private address space */
    gcvTYPE_QUALIFIER_CONST        = 0x100, /* const */
}gceTYPE_QUALIFIER;

#define gcd_ADDRESS_SPACE_QUALIFIERS  (gcvTYPE_QUALIFIER_CONSTANT | \
                                       gcvTYPE_QUALIFIER_GLOBAL | \
                                       gcvTYPE_QUALIFIER_LOCAL | \
                                       gcvTYPE_QUALIFIER_PRIVATE)

typedef gctUINT16  gctTYPE_QUALIFIER;

/* gcSHADER_PRECISION enumeration. */
typedef enum _gcSHADER_PRECISION
{
    gcSHADER_PRECISION_DEFAULT, /* 0x00 */
    gcSHADER_PRECISION_LOW, /* 0x01 */
    gcSHADER_PRECISION_MEDIUM, /* 0x02 */
    gcSHADER_PRECISION_HIGH, /* 0x03 */
    gcSHADER_PRECISION_ANY, /* 0x04 */
}
gcSHADER_PRECISION;

/* shader layout qualifiers. */
typedef enum _gceLAYOUT_QUALIFIER
{
    gcvLAYOUT_QUALIFIER_NONE                                 = 0x0,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_MULTIPLY               = 0x1,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_OVERLAY                = 0x2,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_DARKEN                 = 0x4,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_LIGHTEN                = 0x8,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORDODGE             = 0x10,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORBURN              = 0x20,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HARDLIGHT              = 0x40,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_SOFTLIGHT              = 0x80,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_DIFFERENCE             = 0x100,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_EXCLUSION              = 0x200,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_HUE                = 0x400,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_SATURATION         = 0x800,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_COLOR              = 0x1000,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_LUMINOSITY         = 0x2000,
    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_SCREEN                 = 0x4000,

    gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_ALL_EQUATIONS          = (gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_MULTIPLY|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_SCREEN|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_OVERLAY|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_DARKEN|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_LIGHTEN|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORDODGE|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORBURN|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HARDLIGHT|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_SOFTLIGHT|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_DIFFERENCE|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_EXCLUSION|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_HUE|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_SATURATION|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_COLOR|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_LUMINOSITY),

    gcvLAYOUT_QUALIFIER_BLEND_HW_UNSUPPORT_EQUATIONS_ALL     = gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_ALL_EQUATIONS,
    gcvLAYOUT_QUALIFIER_BLEND_HW_UNSUPPORT_EQUATIONS_PART0   = (gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORDODGE|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_COLORBURN|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_SOFTLIGHT|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_HUE|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_SATURATION|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_COLOR|
                                                                gcvLAYOUT_QUALIFIER_BLEND_SUPPORT_HSL_LUMINOSITY),
}
gceLAYOUT_QUALIFIER;

/* shader layout qualifiers. */
typedef enum _gceIMAGE_FORMAT
{
    gcIMAGE_FORMAT_DEFAULT  = 0,
    gcIMAGE_FORMAT_RGBA32F,
    gcIMAGE_FORMAT_RGBA16F,
    gcIMAGE_FORMAT_R32F,
    gcIMAGE_FORMAT_RGBA8,
    gcIMAGE_FORMAT_RGBA8_SNORM,
    gcIMAGE_FORMAT_RGBA32I,
    gcIMAGE_FORMAT_RGBA16I,
    gcIMAGE_FORMAT_RGBA8I,
    gcIMAGE_FORMAT_R32I,
    gcIMAGE_FORMAT_RGBA32UI,
    gcIMAGE_FORMAT_RGBA16UI,
    gcIMAGE_FORMAT_RGBA8UI,
    gcIMAGE_FORMAT_R32UI,
}
gceIMAGE_FORMAT;

/* gcSHADER_SHADERMODE enumeration. */
typedef enum _gcSHADER_SHADERMODE
{
    gcSHADER_SHADER_DEFAULT             = 0x00,
    gcSHADER_SHADER_SMOOTH              = 0x00,
    gcSHADER_SHADER_FLAT                = 0x01,
    gcSHADER_SHADER_NOPERSPECTIVE       = 0x02,
}
gcSHADER_SHADERMODE;

/* Kernel function property flags. */
typedef enum _gcePROPERTY_FLAGS
{
    gcvPROPERTY_REQD_WORK_GRP_SIZE    = 0x00,
    gcvPROPERTY_WORK_GRP_SIZE_HINT    = 0x01,
    gcvPROPERTY_KERNEL_SCALE_HINT     = 0x02,
    gcvPROPERTY_COUNT
}
gceKERNEL_FUNCTION_PROPERTY_FLAGS;

/* HALTI interface block layout qualifiers */
typedef enum _gceINTERFACE_BLOCK_LAYOUT_ID
{
    gcvINTERFACE_BLOCK_NONE      = 0,
    gcvINTERFACE_BLOCK_SHARED    = 0x01,
    gcvINTERFACE_BLOCK_STD140    = 0x02,
    gcvINTERFACE_BLOCK_PACKED    = 0x04,
    gcvINTERFACE_BLOCK_STD430    = 0x08,
    gcvINTERFACE_BLOCK_ROW_MAJOR = 0x10,
}
gceINTERFACE_BLOCK_LAYOUT_ID;

/* Uniform flags. */
typedef enum _gceUNIFORM_FLAGS
{
    /* special uniform kind */
    gcvUNIFORM_KIND_NONE                        = 0,
    gcvUNIFORM_KIND_UBO_ADDRESS                 = 0,
    gcvUNIFORM_KIND_KERNEL_ARG                  = 1,
    gcvUNIFORM_KIND_KERNEL_ARG_LOCAL            = 2,
    gcvUNIFORM_KIND_KERNEL_ARG_SAMPLER          = 3,
    gcvUNIFORM_KIND_KERNEL_ARG_CONSTANT         = 4,
    gcvUNIFORM_KIND_KERNEL_ARG_LOCAL_MEM_SIZE   = 5,
    gcvUNIFORM_KIND_KERNEL_ARG_PRIVATE          = 6,
    gcvUNIFORM_KIND_LOCAL_ADDRESS_SPACE         = 7,
    gcvUNIFORM_KIND_PRIVATE_ADDRESS_SPACE       = 8,
    gcvUNIFORM_KIND_CONSTANT_ADDRESS_SPACE      = 9,
    gcvUNIFORM_KIND_GLOBAL_SIZE                 = 10,
    gcvUNIFORM_KIND_LOCAL_SIZE                  = 11,
    gcvUNIFORM_KIND_NUM_GROUPS                  = 12,
    gcvUNIFORM_KIND_GLOBAL_OFFSET               = 13,
    gcvUNIFORM_KIND_WORK_DIM                    = 14,
    gcvUNIFORM_KIND_TRANSFORM_FEEDBACK_BUFFER   = 15,
    gcvUNIFORM_KIND_TRANSFORM_FEEDBACK_STATE    = 16,
    gcvUNIFORM_KIND_PRINTF_ADDRESS              = 17,
    gcvUNIFORM_KIND_WORKITEM_PRINTF_BUFFER_SIZE = 18,
    gcvUNIFORM_KIND_STORAGE_BLOCK_ADDRESS       = 19,
    gcvUNIFORM_KIND_GENERAL_PATCH               = 20,
    gcvUNIFORM_KIND_IMAGE_EXTRA_LAYER           = 21,
    gcvUNIFORM_KIND_TEMP_REG_SPILL_ADDRESS      = 22,
    gcvUNIFORM_KIND_GLOBAL_WORK_SCALE           = 23,
    gcvUNIFORM_KIND_NUM_GROUPS_FOR_SINGLE_GPU   = 24,
    gcvUNIFORM_KIND_THREAD_ID_MEM_ADDR          = 25,
    gcvUNIFORM_KIND_ENQUEUED_LOCAL_SIZE         = 26,

    /* Use this to check if this flag is a special uniform kind. */
    gcvUNIFORM_FLAG_SPECIAL_KIND_MASK           = 0x1F,

    /* flags */
    gcvUNIFORM_FLAG_COMPILETIME_INITIALIZED     = 0x000020,
    gcvUNIFORM_FLAG_LOADTIME_CONSTANT           = 0x000040,
    gcvUNIFORM_FLAG_IS_ARRAY                    = 0x000080,
    gcvUNIFORM_FLAG_IS_INACTIVE                 = 0x000100,
    gcvUNIFORM_FLAG_USED_IN_SHADER              = 0x000200,
    gcvUNIFORM_FLAG_USED_IN_LTC                 = 0x000400, /* it may be not used in
                                                          shader, but is used in LTC */
    gcvUNIFORM_FLAG_INDIRECTLY_ADDRESSED        = 0x000800,
    gcvUNIFORM_FLAG_MOVED_TO_DUB                = 0x001000,
    gcvUNIFORM_FLAG_STD140_SHARED               = 0x002000,
    gcvUNIFORM_FLAG_KERNEL_ARG_PATCH            = 0x004000,
    gcvUNIFORM_FLAG_SAMPLER_CALCULATE_TEX_SIZE  = 0x008000,
    gcvUNIFORM_FLAG_DIRECTLY_ADDRESSED          = 0x010000,
    gcvUNIFORM_FLAG_MOVED_TO_DUBO               = 0x020000,
    gcvUNIFORM_FLAG_USED_AS_TEXGATHER_SAMPLER   = 0x040000,
    gcvUNIFORM_FLAG_ATOMIC_COUNTER              = 0x080000,
    gcvUNIFORM_FLAG_BUILTIN                     = 0x100000,
    gcvUNIFORM_FLAG_COMPILER_GEN                = 0x200000,
    gcvUNIFORM_FLAG_IS_POINTER                  = 0x400000, /* a true pointer as defined in the shader source */
    gcvUNIFORM_FLAG_IS_DEPTH_COMPARISON         = 0x800000,
    gcvUNIFORM_FLAG_IS_MULTI_LAYER              = 0x1000000,
    gcvUNIFORM_FLAG_STATICALLY_USED             = 0x2000000,
    gcvUNIFORM_FLAG_USED_AS_TEXGATHEROFFSETS_SAMPLER = 0x4000000,
    gcvUNIFORM_FLAG_MOVED_TO_CUBO               = 0x8000000,
    gcvUNIFORM_FLAG_TREAT_SAMPLER_AS_CONST      = 0x10000000,
    gcvUNIFORM_FLAG_WITH_INITIALIZER            = 0x20000000,
    gcvUNIFORM_FLAG_FORCE_ACTIVE                = 0x40000000,
}
gceUNIFORM_FLAGS;

#define GetUniformFlagsSpecialKind(f)     ((f) & gcvUNIFORM_FLAG_SPECIAL_KIND_MASK)
#define isUniformSpecialKind(f)     (((f) & gcvUNIFORM_FLAG_SPECIAL_KIND_MASK) != 0)

/* specific to OPENCL */
#define isUniformKindBuiltin(k)     ((k) == gcvUNIFORM_KIND_LOCAL_ADDRESS_SPACE       || \
                                     (k) == gcvUNIFORM_KIND_PRIVATE_ADDRESS_SPACE     || \
                                     (k) == gcvUNIFORM_KIND_CONSTANT_ADDRESS_SPACE    || \
                                     (k) == gcvUNIFORM_KIND_KERNEL_ARG_LOCAL_MEM_SIZE || \
                                     (k) == gcvUNIFORM_KIND_GLOBAL_SIZE               || \
                                     (k) == gcvUNIFORM_KIND_LOCAL_SIZE                || \
                                     (k) == gcvUNIFORM_KIND_ENQUEUED_LOCAL_SIZE       || \
                                     (k) == gcvUNIFORM_KIND_NUM_GROUPS                || \
                                     (k) == gcvUNIFORM_KIND_GLOBAL_OFFSET             || \
                                     (k) == gcvUNIFORM_KIND_WORK_DIM                  || \
                                     (k) == gcvUNIFORM_KIND_PRINTF_ADDRESS            || \
                                     (k) == gcvUNIFORM_KIND_GLOBAL_WORK_SCALE         || \
                                     (k) == gcvUNIFORM_KIND_NUM_GROUPS_FOR_SINGLE_GPU || \
                                     (k) == gcvUNIFORM_KIND_WORKITEM_PRINTF_BUFFER_SIZE)

#define isUniformKindKernelArg(k)   ((k) == gcvUNIFORM_KIND_KERNEL_ARG                || \
                                     (k) == gcvUNIFORM_KIND_KERNEL_ARG_LOCAL          || \
                                     (k) == gcvUNIFORM_KIND_KERNEL_ARG_SAMPLER        || \
                                     (k) == gcvUNIFORM_KIND_KERNEL_ARG_CONSTANT       || \
                                     (k) == gcvUNIFORM_KIND_KERNEL_ARG_PRIVATE)

#define GetUniformFlags(u)          ((u)->_flags)
#define SetUniformFlags(u, flags)   ((u)->_flags = (flags))

#define GetUniformKind(u)           ((gceUNIFORM_FLAGS)((u)->_flags & gcvUNIFORM_FLAG_SPECIAL_KIND_MASK))
#define SetUniformKind(u, k)        do {                                        \
                                        gceUNIFORM_FLAGS *_f = &(u)->_flags;    \
                                        gcmASSERT(isUniformSpecialKind(k));     \
                                        *_f = (((gctUINT)*_f) & ~gcvUNIFORM_FLAG_SPECIAL_KIND_MASK) | ((k)&gcvUNIFORM_FLAG_SPECIAL_KIND_MASK);       \
                                    } while (0)

#define UniformHasFlag(u, flag)     (((u)->_flags & (flag)) != 0 )
#define SetUniformFlag(u, flag)     do {                                    \
                                        if (isUniformSpecialKind(flag))     \
                                        {                                   \
                                            SetUniformKind((u), flag);      \
                                        } else {                            \
                                            (u)->_flags |= (gctUINT)(flag); \
                                        }                                   \
                                    } while(0)

#define ResetUniformFlag(u, flag)   do {                                \
                                        gcmASSERT(!isUniformSpecialKind(flag));\
                                        ((u)->_flags &= ~((gctUINT)flag));       \
                                    } while (0)

/* kinds */
#define isUniformTransfromFeedbackState(u)   (GetUniformKind(u) == gcvUNIFORM_KIND_TRANSFORM_FEEDBACK_STATE)
#define isUniformTransfromFeedbackBuffer(u)  (GetUniformKind(u) == gcvUNIFORM_KIND_TRANSFORM_FEEDBACK_BUFFER)
#define isUniformImageExtraLayer(u)          (GetUniformKind(u) == gcvUNIFORM_KIND_IMAGE_EXTRA_LAYER)
#define isUniformWorkDim(u)                  (GetUniformKind(u) == gcvUNIFORM_KIND_WORK_DIM)
#define isUniformGlobalSize(u)               (GetUniformKind(u) == gcvUNIFORM_KIND_GLOBAL_SIZE)
#define isUniformLocalSize(u)                (GetUniformKind(u) == gcvUNIFORM_KIND_LOCAL_SIZE)
#define isUniformEnqueuedLocalSize(u)        (GetUniformKind(u) == gcvUNIFORM_KIND_ENQUEUED_LOCAL_SIZE)
#define isUniformNumGroups(u)                (GetUniformKind(u) == gcvUNIFORM_KIND_NUM_GROUPS)
#define isUniformNumGroupsForSingleGPU(u)    (GetUniformKind(u) == gcvUNIFORM_KIND_NUM_GROUPS_FOR_SINGLE_GPU)
#define isUniformGlobalOffset(u)             (GetUniformKind(u) == gcvUNIFORM_KIND_GLOBAL_OFFSET)
#define isUniformLocalAddressSpace(u)        (GetUniformKind(u) == gcvUNIFORM_KIND_LOCAL_ADDRESS_SPACE)
#define isUniformPrivateAddressSpace(u)      (GetUniformKind(u) == gcvUNIFORM_KIND_PRIVATE_ADDRESS_SPACE)
#define isUniformConstantAddressSpace(u)     (GetUniformKind(u) == gcvUNIFORM_KIND_CONSTANT_ADDRESS_SPACE)
#define isUniformKernelArg(u)                (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG)
#define isUniformKernelArgConstant(u)        (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG_CONSTANT)
#define isUniformKernelArgSampler(u)         (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG_SAMPLER)
#define isUniformKernelArgLocal(u)           (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG_LOCAL)
#define isUniformKernelArgLocalMemSize(u)    (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG_LOCAL_MEM_SIZE)
#define isUniformKernelArgPrivate(u)         (GetUniformKind(u) == gcvUNIFORM_KIND_KERNEL_ARG_PRIVATE)
#define isUniformStorageBlockAddress(u)      (GetUniformKind(u) == gcvUNIFORM_KIND_STORAGE_BLOCK_ADDRESS)
#define isUniformUBOAddress(u)               ((u)->_varCategory == gcSHADER_VAR_CATEGORY_BLOCK_ADDRESS && \
                                              GetUniformKind(u) == gcvUNIFORM_KIND_UBO_ADDRESS)
#define isUniformPrintfAddress(u)            (GetUniformKind(u) == gcvUNIFORM_KIND_PRINTF_ADDRESS)
#define isUniformWorkItemPrintfBufferSize(u) (GetUniformKind(u) == gcvUNIFORM_KIND_WORKITEM_PRINTF_BUFFER_SIZE)
#define isUniformTempRegSpillAddress(u)      (GetUniformKind(u) == gcvUNIFORM_KIND_TEMP_REG_SPILL_ADDRESS)
#define isUniformGlobalWorkScale(u)          (GetUniformKind(u) == gcvUNIFORM_KIND_GLOBAL_WORK_SCALE)
#define isUniformThreadIdMemAddr(u)          (GetUniformKind(u) == gcvUNIFORM_KIND_THREAD_ID_MEM_ADDR)

#define hasUniformKernelArgKind(u)          (isUniformKernelArg(u)  ||       \
                                             isUniformKernelArgLocal(u) ||   \
                                             isUniformKernelArgSampler(u) || \
                                             isUniformKernelArgPrivate(u) || \
                                             isUniformKernelArgConstant(u))

/* Whether the uniform was user-defined or built-in */
#define isUniformShaderDefined(u)           (((u)->name[0] != '#') ||                                       \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.near")) || \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.far")) ||  \
                                              gcmIS_SUCCESS(gcoOS_StrCmp((u)->name, "#DepthRange.diff"))    \
                                            )

/* flags */
#define isUniformArray(u)                           (((u)->_flags & gcvUNIFORM_FLAG_IS_ARRAY) != 0)
#define isUniformInactive(u)                        (((u)->_flags & gcvUNIFORM_FLAG_IS_INACTIVE) != 0)
#define isUniformImmediate(u)                       (((u)->_flags & gcvUNIFORM_FLAG_COMPILETIME_INITIALIZED) != 0)
#define isUniformLoadtimeConstant(u)                (((u)->_flags & gcvUNIFORM_FLAG_LOADTIME_CONSTANT) != 0)
#define isUniformUsedInShader(u)                    (((u)->_flags & gcvUNIFORM_FLAG_USED_IN_SHADER) != 0)
#define isUniformUsedInLTC(u)                       (((u)->_flags & gcvUNIFORM_FLAG_USED_IN_LTC) != 0)
#define isUniformSTD140OrShared(u)                  (((u)->_flags & gcvUNIFORM_FLAG_STD140_SHARED) != 0)
#define isUniformKernelArgPatch(u)                  (((u)->_flags & gcvUNIFORM_FLAG_KERNEL_ARG_PATCH) != 0)
#define isUniformCompiletimeInitialized(u)          (((u)->_flags & gcvUNIFORM_FLAG_COMPILETIME_INITIALIZED) != 0)
#define isUniformIndirectlyAddressed(u)             (((u)->_flags & gcvUNIFORM_FLAG_INDIRECTLY_ADDRESSED) != 0)
#define isUniformDirectlyAddressed(u)               (((u)->_flags & gcvUNIFORM_FLAG_DIRECTLY_ADDRESSED) != 0)
#define isUniformMovedToDUB(u)                      (((u)->_flags & gcvUNIFORM_FLAG_MOVED_TO_DUB) != 0)
#define isUniformSamplerCalculateTexSize(u)         (((u)->_flags & gcvUNIFORM_FLAG_SAMPLER_CALCULATE_TEX_SIZE) != 0)
#define isUniformUsedAsTexGatherSampler(u)          (((u)->_flags & gcvUNIFORM_FLAG_USED_AS_TEXGATHER_SAMPLER) != 0)
#define isUniformUsedAsTexGatherOffsetsSampler(u)   (((u)->_flags & gcvUNIFORM_FLAG_USED_AS_TEXGATHEROFFSETS_SAMPLER) != 0)
#define isUniformAtomicCounter(u)                   (((u)->_flags & gcvUNIFORM_FLAG_ATOMIC_COUNTER) != 0)
#define isUniformBuiltin(u)                         (((u)->_flags & gcvUNIFORM_FLAG_BUILTIN) != 0)
#define isUniformCompilerGen(u)                     (((u)->_flags & gcvUNIFORM_FLAG_COMPILER_GEN) != 0)
#define isUniformPointer(u)                         (((u)->_flags & gcvUNIFORM_FLAG_IS_POINTER) != 0)
#define isUniformMLSampler(u)                       (((u)->_flags & gcvUNIFORM_FLAG_IS_MULTI_LAYER) != 0)
#define isUniformStaticallyUsed(u)                  (((u)->_flags & gcvUNIFORM_FLAG_STATICALLY_USED) != 0)
#define isUniformMovedToDUBO(u)                     (((u)->_flags & gcvUNIFORM_FLAG_MOVED_TO_DUBO) != 0)
#define isUniformMovedToCUBO(u)                     (((u)->_flags & gcvUNIFORM_FLAG_MOVED_TO_CUBO) != 0)
#define isUniformMovedToAUBO(u)                     (isUniformMovedToDUBO(u) || isUniformMovedToCUBO(u))
#define isUniformTreatSamplerAsConst(u)             (((u)->_flags & gcvUNIFORM_FLAG_TREAT_SAMPLER_AS_CONST) != 0)
#define isUniformWithInitializer(u)                 (((u)->_flags & gcvUNIFORM_FLAG_WITH_INITIALIZER) != 0)
#define isUniformForceActive(u)                     (((u)->_flags & gcvUNIFORM_FLAG_FORCE_ACTIVE) != 0)

#define SetUniformUsedInShader(u)           ((u)->_flags |= gcvUNIFORM_FLAG_USED_IN_SHADER)
#define ResetUniformUsedInShader(u)         ((u)->_flags &= ~gcvUNIFORM_FLAG_USED_IN_SHADER)
#define SetUniformUsedInLTC(u)              ((u)->_flags |= gcvUNIFORM_FLAG_USED_IN_LTC)
#define ResetUniformUsedInLTC(u)            ((u)->_flags &= ~gcvUNIFORM_FLAG_USED_IN_LTC)

#define isUniformNormal(u)                  ((u)->_varCategory == gcSHADER_VAR_CATEGORY_NORMAL)
#define isUniformStruct(u)                  ((u)->_varCategory == gcSHADER_VAR_CATEGORY_STRUCT)
#define isUniformBlockMember(u)             ((u)->_varCategory == gcSHADER_VAR_CATEGORY_BLOCK_MEMBER)
#define isUniformBlockAddress(u)            ((u)->_varCategory == gcSHADER_VAR_CATEGORY_BLOCK_ADDRESS)
#define isUniformLodMinMax(u)               ((u)->_varCategory == gcSHADER_VAR_CATEGORY_LOD_MIN_MAX)
#define isUniformLevelBaseSize(u)           ((u)->_varCategory == gcSHADER_VAR_CATEGORY_LEVEL_BASE_SIZE)
#define isUniformSampleLocation(u)          ((u)->_varCategory == gcSHADER_VAR_CATEGORY_SAMPLE_LOCATION)
#define isUniformMultiSampleBuffers(u)      ((u)->_varCategory == gcSHADER_VAR_CATEGORY_ENABLE_MULTISAMPLE_BUFFERS)
#define isUniformGlSamplerForImaget(u)      ((u)->_varCategory == gcSHADER_VAR_CATEGORY_GL_SAMPLER_FOR_IMAGE_T)
#define isUniformGlImageForImaget(u)        ((u)->_varCategory == gcSHADER_VAR_CATEGORY_GL_IMAGE_FOR_IMAGE_T)
#define isUniformWorkThreadCount(u)         ((u)->_varCategory == gcSHADER_VAR_CATEGORY_WORK_THREAD_COUNT)
#define isUniformWorkGroupCount(u)          ((u)->_varCategory == gcSHADER_VAR_CATEGORY_WORK_GROUP_COUNT)
#define isUniformWorkGroupIdOffset(u)       ((u)->_varCategory == gcSHADER_VAR_CATEGORY_WORK_GROUP_ID_OFFSET)
#define isUniformGlobalInvocationIdOffset(u)((u)->_varCategory == gcSHADER_VAR_CATEGORY_GLOBAL_INVOCATION_ID_OFFSET)
#define isUniformViewIndex(u)               ((u)->_varCategory == gcSHADER_VAR_CATEGORY_VIEW_INDEX)
#define isUniformClipDistanceEnable(u)      ((u)->_varCategory == gcSHADER_VAR_CATEGORY_CLIP_DISTANCE_ENABLE)

#define isUniformBasicType(u)               (isUniformNormal((u))                   || \
                                             isUniformBlockMember((u))              || \
                                             isUniformBlockAddress((u))             || \
                                             isUniformLodMinMax((u))                || \
                                             isUniformLevelBaseSize((u))            || \
                                             isUniformSampleLocation((u))           || \
                                             isUniformMultiSampleBuffers((u))       || \
                                             isUniformGlSamplerForImaget((u))       || \
                                             isUniformGlImageForImaget((u))         || \
                                             isUniformWorkThreadCount((u))          || \
                                             isUniformWorkGroupCount((u))           || \
                                             isUniformClipDistanceEnable((u))       || \
                                             isUniformWorkGroupIdOffset((u))        || \
                                             isUniformGlobalInvocationIdOffset((u)))

#define isUniformSampler(u)                 (isUniformNormal(u) && (gcmType_Kind(GetUniformType(u)) == gceTK_SAMPLER))

#define isUniformImage(u)                   (isUniformNormal(u) && (gcmType_Kind(GetUniformType(u)) == gceTK_IMAGE))
#define isUniformImage2D(uniform)           (((uniform)->u.type == gcSHADER_IMAGE_2D) || ((uniform)->u.type == gcSHADER_IIMAGE_2D) || ((uniform)->u.type == gcSHADER_UIMAGE_2D))

#define isUniformImageT(u)                  (isUniformNormal(u) && (gcmType_Kind(GetUniformType(u)) == gceTK_IMAGE_T))

#define isUniformSamplerBuffer(uniform)     ((uniform->u.type == gcSHADER_SAMPLER_BUFFER) || \
                                             (uniform->u.type == gcSHADER_ISAMPLER_BUFFER) || \
                                             (uniform->u.type == gcSHADER_USAMPLER_BUFFER))
#define isUniformImageBuffer(uniform)       ((uniform->u.type == gcSHADER_IMAGE_BUFFER) || \
                                             (uniform->u.type == gcSHADER_IIMAGE_BUFFER) || \
                                             (uniform->u.type == gcSHADER_UIMAGE_BUFFER))
#define isUniformBuffer(uniform)            (isUniformSamplerBuffer(uniform) || isUniformImageBuffer(uniform))

#define isUniformMatrix(uniform)            ((uniform->u.type == gcSHADER_FLOAT_2X2) || \
                                             (uniform->u.type == gcSHADER_FLOAT_3X3) || \
                                             (uniform->u.type == gcSHADER_FLOAT_4X4) || \
                                             (uniform->u.type == gcSHADER_FLOAT_2X3) || \
                                             (uniform->u.type == gcSHADER_FLOAT_2X4) || \
                                             (uniform->u.type == gcSHADER_FLOAT_3X2) || \
                                             (uniform->u.type == gcSHADER_FLOAT_3X4) || \
                                             (uniform->u.type == gcSHADER_FLOAT_4X2) || \
                                             (uniform->u.type == gcSHADER_FLOAT_4X3))


typedef enum _gceFEEDBACK_BUFFER_MODE
{
  gcvFEEDBACK_INTERLEAVED        = 0x00,
  gcvFEEDBACK_SEPARATE           = 0x01
} gceFEEDBACK_BUFFER_MODE;

/* Special register indices. */
typedef enum _gceBuiltinNameKind
{
    gcSL_NONBUILTINGNAME =  0,
    gcSL_POSITION        = -1,
    gcSL_POINT_SIZE      = -2,
    gcSL_COLOR           = -3,
    gcSL_FRONT_FACING    = -4,
    gcSL_POINT_COORD     = -5,
    gcSL_POSITION_W      = -6,
    gcSL_DEPTH           = -7,
    gcSL_FOG_FRAG_COORD  = -8,
    gcSL_VERTEX_ID       = -9,
    gcSL_INSTANCE_ID     = -10,
    gcSL_WORK_GROUP_ID          = -11,
    gcSL_LOCAL_INVOCATION_ID    = -12,
    gcSL_GLOBAL_INVOCATION_ID   = -13,
    gcSL_HELPER_INVOCATION      = -14,
    gcSL_FRONT_COLOR            = -15,
    gcSL_BACK_COLOR             = -16,
    gcSL_FRONT_SECONDARY_COLOR  = -17,
    gcSL_BACK_SECONDARY_COLOR   = -18,
    gcSL_TEX_COORD              = -19,
    gcSL_SUBSAMPLE_DEPTH        = -20, /* internal subsample_depth register */
    gcSL_PERVERTEX              = -21, /* gl_PerVertex */
    gcSL_IN                     = -22, /* gl_in */
    gcSL_OUT                    = -23, /* gl_out */
    gcSL_INVOCATION_ID          = -24, /* gl_InvocationID */
    gcSL_PATCH_VERTICES_IN      = -25, /* gl_PatchVerticesIn */
    gcSL_PRIMITIVE_ID           = -26, /* gl_PrimitiveID */
    gcSL_TESS_LEVEL_OUTER       = -27, /* gl_TessLevelOuter */
    gcSL_TESS_LEVEL_INNER       = -28, /* gl_TessLevelInner */
    gcSL_LAYER                  = -29, /* gl_Layer */
    gcSL_PRIMITIVE_ID_IN        = -30, /* gl_PrimitiveIDIn */
    gcSL_TESS_COORD             = -31, /* gl_TessCoord */
    gcSL_SAMPLE_ID              = -32, /* gl_SampleID */
    gcSL_SAMPLE_POSITION        = -33, /* gl_SamplePosition */
    gcSL_SAMPLE_MASK_IN         = -34, /* gl_SampleMaskIn */
    gcSL_SAMPLE_MASK            = -35, /* gl_SampleMask */
    gcSL_IN_POSITION            = -36, /* gl_in.gl_Position */
    gcSL_IN_POINT_SIZE          = -37, /* gl_in.gl_PointSize */
    gcSL_BOUNDING_BOX           = -38, /* gl_BoundingBox */
    gcSL_LAST_FRAG_DATA         = -39, /* gl_LastFragData */
    gcSL_CLUSTER_ID             = -40, /* cluster id */
    gcSL_CLIP_DISTANCE          = -41, /* gl_ClipDistance */
    gcSL_LOCAL_INVOCATION_INDEX  = -42,
    gcSL_GLOBAL_INVOCATION_INDEX = -43,
    gcSL_SECONDARY_COLOR        = -44, /* gl_SecondaryColor */
    gcSL_NORMAL                 = -45, /* gl_Normal */
    gcSL_VERTEX                 = -46, /* gl_Vertex */
    gcSL_FOG_COORD              = -47, /* gl_FogCoord */
    gcSL_MULTI_TEX_COORD_0      = -48, /* gl_MultiTexCoord0 */
    gcSL_MULTI_TEX_COORD_1      = -49, /* gl_MultiTexCoord1 */
    gcSL_MULTI_TEX_COORD_2      = -50, /* gl_MultiTexCoord2 */
    gcSL_MULTI_TEX_COORD_3      = -51, /* gl_MultiTexCoord3 */
    gcSL_MULTI_TEX_COORD_4      = -52, /* gl_MultiTexCoord4 */
    gcSL_MULTI_TEX_COORD_5      = -53, /* gl_MultiTexCoord5 */
    gcSL_MULTI_TEX_COORD_6      = -54, /* gl_MultiTexCoord6 */
    gcSL_MULTI_TEX_COORD_7      = -55, /* gl_MultiTexCoord7 */
    gcSL_CLIP_VERTEX            = -56, /* gl_ClipVertex */
    gcSL_FRONT_COLOR_IN            = -57,
    gcSL_BACK_COLOR_IN             = -58,
    gcSL_FRONT_SECONDARY_COLOR_IN  = -59,
    gcSL_BACK_SECONDARY_COLOR_IN   = -60,
    gcSL_BUILTINNAME_COUNT      = 61
} gceBuiltinNameKind;

/* Special code generation indices. */
#define gcSL_CG_TEMP1_XY_NO_SRC_SHIFT           108
#define gcSL_CG_TEMP1_X_NO_SRC_SHIFT            109
#define gcSL_CG_TEMP2_X_NO_SRC_SHIFT            110
#define gcSL_CG_TEMP3_X_NO_SRC_SHIFT            111
#define gcSL_CG_TEMP1                           112
#define gcSL_CG_TEMP1_X                         113
#define gcSL_CG_TEMP1_XY                        114
#define gcSL_CG_TEMP1_XYZ                       115
#define gcSL_CG_TEMP1_XYZW                      116
#define gcSL_CG_TEMP2                           117
#define gcSL_CG_TEMP2_X                         118
#define gcSL_CG_TEMP2_XY                        119
#define gcSL_CG_TEMP2_XYZ                       120
#define gcSL_CG_TEMP2_XYZW                      121
#define gcSL_CG_TEMP3                           122
#define gcSL_CG_TEMP3_X                         123
#define gcSL_CG_TEMP3_XY                        124
#define gcSL_CG_TEMP3_XYZ                       125
#define gcSL_CG_TEMP3_XYZW                      126
#define gcSL_CG_CONSTANT                        127

/* now opcode is encoded with sat,src[0|1]'s neg and abs modifier */
#define gcdSL_OPCODE_Opcode                  0 : 8
#define gcdSL_OPCODE_Round                   8 : 3  /* rounding mode */
#define gcdSL_OPCODE_Sat                    11 : 1  /* target sat modifier */
#define gcdSL_OPCODE_RES_TYPE               12 : 4

#define gcmSL_OPCODE_GET(Value, Field) \
    gcmGETBITS(Value, gctUINT16, gcdSL_OPCODE_##Field)

#define gcmSL_OPCODE_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctUINT16, gcdSL_OPCODE_##Field, NewValue)

#define gcmSL_OPCODE_UPDATE(Value, Field, NewValue) \
        (Value) = gcmSL_OPCODE_SET(Value, Field, NewValue)

/* 4-bit enable bits. */
#define gcdSL_TARGET_Enable                     0 : 4
/* Indexed addressing mode of type gcSL_INDEXED. */
#define gcdSL_TARGET_Indexed                    4 : 3
/* precision on temp: 1 => high and 0 => medium */
#define gcdSL_TARGET_Precision                  7 : 3
/* 4-bit condition of type gcSL_CONDITION. */
#define gcdSL_TARGET_Condition                  10 : 5
/* Target format of type gcSL_FORMAT. */
#define gcdSL_TARGET_Format                    15 : 4
/* non-zero field to indicate the number of components in target being packed; width of six bits
 allows number of components to 32. */
#define gcdSL_TARGET_PackedComponents          19 : 6
typedef gctUINT32 gctTARGET_t;

#define gcmSL_TARGET_GET(Value, Field) \
    gcmGETBITS(Value, gctTARGET_t, gcdSL_TARGET_##Field)

#define gcmSL_TARGET_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctTARGET_t, gcdSL_TARGET_##Field, NewValue)

#define SOURCE_is_32BIT   1

/* Register type of type gcSL_TYPE. */
#define gcdSL_SOURCE_Type                       0 : 3
/* Indexed register swizzle. */
#define gcdSL_SOURCE_Indexed                    3 : 3
/* Source format of type gcSL_FORMAT. */
#if SOURCE_is_32BIT
typedef gctUINT32 gctSOURCE_t;

#define gcdSL_SOURCE_Format                     6 : 4
/* Swizzle fields of type gcSL_SWIZZLE. */
#define gcdSL_SOURCE_Swizzle                   10 : 8
#define gcdSL_SOURCE_SwizzleX                  10 : 2
#define gcdSL_SOURCE_SwizzleY                  12 : 2
#define gcdSL_SOURCE_SwizzleZ                  14 : 2
#define gcdSL_SOURCE_SwizzleW                  16 : 2

/* source precision : 1 => high and 0 => medium */
#define gcdSL_SOURCE_Precision                 18 : 3

/* source modifier */
#define gcdSL_SOURCE_Neg                       21 : 1
#define gcdSL_SOURCE_Abs                       22 : 1

/* if this source is parent indexed. Right now only implement this for normal uniform. */
#define gcdSL_SOURCE_Indexed_Level             23 : 2

/* non-zero field to indicate the number of components in source being packed; width of six bits
 allows number of components to 32. */
#define gcdSL_SOURCE_PackedComponents         25 : 6

#else  /* source is 16 bit */
typedef gctUINT16 gctSOURCE_t;

#define gcdSL_SOURCE_Format                     6 : 2
/* Swizzle fields of type gcSL_SWIZZLE. */
#define gcdSL_SOURCE_Swizzle                    8 : 8
#define gcdSL_SOURCE_SwizzleX                   8 : 2
#define gcdSL_SOURCE_SwizzleY                  10 : 2
#define gcdSL_SOURCE_SwizzleZ                  12 : 2
#define gcdSL_SOURCE_SwizzleW                  14 : 2
#endif

#define gcmSL_SOURCE_GET(Value, Field) \
    gcmGETBITS(Value, gctSOURCE_t, gcdSL_SOURCE_##Field)

#define gcmSL_SOURCE_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctSOURCE_t, gcdSL_SOURCE_##Field, NewValue)

/* Index of register. */
#define gcdSL_INDEX_Index                      0 : 20
/* Constant value. */
#define gcdSL_INDEX_ConstValue                20 :  2

#define gcmSL_INDEX_GET(Value, Field) \
    gcmGETBITS(Value, gctUINT32, gcdSL_INDEX_##Field)

#define gcmSL_INDEX_SET(Value, Field, NewValue) \
    gcmSETBITS(Value, gctUINT32, gcdSL_INDEX_##Field, NewValue)

#define gcmSL_JMP_TARGET(Value) (Value)->tempIndex
#define gcmSL_CALL_TARGET(Value) (Value)->tempIndex

#define gcmDummySamplerIdBit          0x2000   /* 14th bit is 1 (MSB for the field gcdSL_INDEX_Index) ??? */

#define gcmMakeDummySamplerId(SID)    ((SID) | gcmDummySamplerIdBit)
#define gcmIsDummySamplerId(SID)      ((SID) >= gcmDummySamplerIdBit)
#define gcmGetOriginalSamplerId(SID)  ((SID) & ~gcmDummySamplerIdBit)

#define _MAX_VARYINGS                     32
#define gcdSL_MIN_TEXTURE_LOD_BIAS       -16
#define gcdSL_MAX_TEXTURE_LOD_BIAS         6

/* Structure that defines a gcSL instruction. */
typedef struct _gcSL_INSTRUCTION
{
    /* Opcode of type gcSL_OPCODE. */
    gctUINT16                    opcode;

    /* Indexed register for destination. */
    gctUINT16                    tempIndexed;

    /* Indexed register for source 0 operand. */
    gctUINT16                    source0Indexed;

    /* Indexed register for source 1 operand. */
    gctUINT16                    source1Indexed;

    /* Opcode condition and target write enable bits of type gcSL_TARGET. */
    gctTARGET_t                  temp;

    /* 16-bit temporary register index, or call/branch target inst. index. */
    gctUINT32                    tempIndex;

    /* Type of source 0 operand of type gcSL_SOURCE. */
    gctSOURCE_t                  source0;

    /* 14-bit register index for source 0 operand of type gcSL_INDEX,
     * must accessed by gcmSL_INDEX_GET(source0Index, Index);
     * and 2-bit constant value to the base of the Index, must be
     * accessed by gcmSL_INDEX_GET(source0Index, ConstValue).
     *
     * Now we have to change the temp register index to 32 bit due to
     * the fact that some program may exceed the 14 temp register count
     * limits!
     */
    gctUINT32                    source0Index;

    /* Type of source 1 operand of type gcSL_SOURCE. */
    gctSOURCE_t                  source1;

    /* 14-bit register index for source 1 operand of type gcSL_INDEX,
     * must accessed by gcmSL_INDEX_GET(source1Index, Index);
     * and 2-bit constant value to the base of the Index, must be
     * accessed by gcmSL_INDEX_GET(source1Index, ConstValue).
     */
    gctUINT32                    source1Index;

    gctUINT32                    srcLoc;
}
* gcSL_INSTRUCTION;

#define GCSL_SRC_LOC_LINE_OFFSET 16
#define GCSL_SRC_LOC_COL_MASK 0xffff
#define GCSL_Build_SRC_LOC(LineNo, StringNo) ((StringNo) | ((LineNo) << GCSL_SRC_LOC_LINE_OFFSET))
#define GCSL_SRC_LOC_LINENO(loc) (loc >> GCSL_SRC_LOC_LINE_OFFSET)
#define GCSL_SRC_LOC_COLNO(loc) (loc & GCSL_SRC_LOC_COL_MASK)

#define GetInstOpcode(i)                    ((i)->opcode)
#define GetInstTemp(i)                      ((i)->temp)
#define GetInstTempIndex(i)                 ((i)->tempIndex)
#define GetInstTempIndexed(i)               ((i)->tempIndexed)
#define GetInstSource0(i)                   ((i)->source0)
#define GetInstSource0Index(i)              ((i)->source0Index)
#define GetInstSource0Indexed(i)            ((i)->source0Indexed)
#define GetInstSource1(i)                   ((i)->source1)
#define GetInstSource1Index(i)              ((i)->source1Index)
#define GetInstSource1Indexed(i)            ((i)->source1Indexed)

/******************************************************************************\
|*********************************** SHADERS **********************************|
\******************************************************************************/
#define MAX_LTC_COMPONENTS   4

typedef union _ConstantValueUnion
{
    gctBOOL        b;
    gctUINT32      u32;
    gctINT32       i32;
    gctFLOAT       f32;
} ConstantValueUnion;

typedef struct _LoadtimeConstantValue
{
    gcSL_ENABLE          enable;               /* the components enabled, for target value */
    gctSOURCE_t          sourceInfo;           /* source type, indexed, format and swizzle info */
    gcSL_FORMAT          elementType;          /* the format of element */
    gctINT               instructionIndex;     /* the instruction index to the LTC expression in Shader */
    ConstantValueUnion   v[MAX_LTC_COMPONENTS];
} LTCValue, *PLTCValue;

typedef enum _gceATTRIBUTE_Flag
{
    gcATTRIBUTE_ISTEXTURE           = 0x01,
    /* Flag to indicate the attribute is a varying packed with othe attribute
       and is no longer in use in the shader, but it cannot be removed from
       attribute array due to the shader maybe loaded from saved file and keep
       the index fro the attributes is needed */
    gcATTRIBUTE_PACKEDAWAY          = 0x02,
    /* mark the attribute to always used to avoid be optimized away, it is
     * useful when doing re-compile, recompiled code cannot change attribute
     * mapping done by master shader
     */
    gcATTRIBUTE_ALWAYSUSED          = 0x04,

    /* mark the attribute to not used based on new RA
     */
    gcATTRIBUTE_ISNOTUSED           = 0x08,

    /* Texcoord only for pointsprite */
    gcATTRIBUTE_POINTSPRITE_TC      = 0x10,

    /* the attribute is per-patch input */
    gcATTRIBUTE_ISPERPATCH          = 0x20,
    gcATTRIBUTE_ISZWTEXTURE         = 0x40,
    gcATTRIBUTE_ISPOSITION          = 0x80,
    gcATTRIBUTE_ENABLED             = 0x100,
    gcATTRIBUTE_ISINVARIANT         = 0x200,
    gcATTRIBUTE_ISPERVERTEXARRAY    = 0x400,
    gcATTRIBUTE_ISPRECISE           = 0x800,
    gcATTRIBUTE_ISIOBLOCKMEMBER     = 0x1000,
    gcATTRIBUTE_ISINSTANCEMEMBER    = 0x2000,
    gcATTRIBUTE_ISCENTROID          = 0x4000,
    gcATTRIBUTE_ISSAMPLE            = 0x8000,
    gcATTRIBUTE_ISPERVERTEXNOTARRAY = 0x10000,
    gcATTRIBUTE_ISSTATICALLYUSED    = 0x20000,
    gcATTRIBUTE_ISUSEASINTERPOLATE  = 0x40000,

    /* This attribute is generated by compiler. */
    gcATTRIBUTE_COMPILERGEN         = 0x80000,

    gcATTRIBUTE_ISDIRECTPOSITION    = 0x100000,

    /* The location is set by driver. */
    gcATTRIBUTE_LOC_SET_BY_DRIVER   = 0x200000,
    gcATTRIBUTE_LOC_HAS_ALIAS       = 0x400000, /* aliased with another attribute (same location) */

    gcATTRIBUTE_REG_ALLOCATED       = 0x800000, /* register allocated for this attribute */
} gceATTRIBUTE_Flag;

#define gcmATTRIBUTE_isTexture(att)             (((att)->flags_ & gcATTRIBUTE_ISTEXTURE) != 0)
#define gcmATTRIBUTE_packedAway(att)            (((att)->flags_ & gcATTRIBUTE_PACKEDAWAY) != 0)
#define gcmATTRIBUTE_alwaysUsed(att)            (((att)->flags_ & gcATTRIBUTE_ALWAYSUSED) != 0)
#define gcmATTRIBUTE_isNotUsed(att)             (((att)->flags_ & gcATTRIBUTE_ISNOTUSED) != 0)
#define gcmATTRIBUTE_pointspriteTc(att)         (((att)->flags_ & gcATTRIBUTE_POINTSPRITE_TC) != 0)
#define gcmATTRIBUTE_isPerPatch(att)            (((att)->flags_ & gcATTRIBUTE_ISPERPATCH) != 0)
#define gcmATTRIBUTE_isZWTexture(att)           (((att)->flags_ & gcATTRIBUTE_ISZWTEXTURE) != 0)
#define gcmATTRIBUTE_isPosition(att)            (((att)->flags_ & gcATTRIBUTE_ISPOSITION) != 0)
#define gcmATTRIBUTE_isDirectPosition(att)      (((att)->flags_ & gcATTRIBUTE_ISDIRECTPOSITION) != 0)
#define gcmATTRIBUTE_enabled(att)               (((att)->flags_ & gcATTRIBUTE_ENABLED) != 0)
#define gcmATTRIBUTE_isInvariant(att)           (((att)->flags_ & gcATTRIBUTE_ISINVARIANT) != 0)
#define gcmATTRIBUTE_isPerVertexArray(att)      (((att)->flags_ & gcATTRIBUTE_ISPERVERTEXARRAY) != 0)
#define gcmATTRIBUTE_isPerVertexNotArray(att)   (((att)->flags_ & gcATTRIBUTE_ISPERVERTEXNOTARRAY) != 0)
#define gcmATTRIBUTE_isPrecise(att)             (((att)->flags_ & gcATTRIBUTE_ISPRECISE) != 0)
#define gcmATTRIBUTE_isIOBlockMember(att)       (((att)->flags_ & gcATTRIBUTE_ISIOBLOCKMEMBER) != 0)
#define gcmATTRIBUTE_isInstanceMember(att)      (((att)->flags_ & gcATTRIBUTE_ISINSTANCEMEMBER) != 0)
#define gcmATTRIBUTE_isCentroid(att)            (((att)->flags_ & gcATTRIBUTE_ISCENTROID) != 0)
#define gcmATTRIBUTE_isSample(att)              (((att)->flags_ & gcATTRIBUTE_ISSAMPLE) != 0)
#define gcmATTRIBUTE_isStaticallyUsed(att)      (((att)->flags_ & gcATTRIBUTE_ISSTATICALLYUSED) != 0)
#define gcmATTRIBUTE_isCompilerGen(att)         (((att)->flags_ & gcATTRIBUTE_COMPILERGEN) != 0)
#define gcmATTRIBUTE_isUseAsInterpolate(att)    (((att)->flags_ & gcATTRIBUTE_ISUSEASINTERPOLATE) != 0)
#define gcmATTRIBUTE_isLocSetByDriver(att)      (((att)->flags_ & gcATTRIBUTE_LOC_SET_BY_DRIVER) != 0)
#define gcmATTRIBUTE_hasAlias(att)              (((att)->flags_ & gcATTRIBUTE_LOC_HAS_ALIAS) != 0)
#define gcmATTRIBUTE_isRegAllocated(att)        (((att)->flags_ & gcATTRIBUTE_REG_ALLOCATED) != 0)

#define gcmATTRIBUTE_SetIsTexture(att, v)   \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISTEXTURE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISTEXTURE))
#define gcmATTRIBUTE_SetPackedAway(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_PACKEDAWAY) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_PACKEDAWAY))

#define gcmATTRIBUTE_SetAlwaysUsed(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ALWAYSUSED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ALWAYSUSED))

#define gcmATTRIBUTE_SetNotUsed(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISNOTUSED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISNOTUSED))

#define gcmATTRIBUTE_SetPointspriteTc(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_POINTSPRITE_TC) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_POINTSPRITE_TC))

#define gcmATTRIBUTE_SetIsPerPatch(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISPERPATCH) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISPERPATCH))

#define gcmATTRIBUTE_SetIsZWTexture(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISZWTEXTURE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISZWTEXTURE))

#define gcmATTRIBUTE_SetIsPosition(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISPOSITION) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISPOSITION))

#define gcmATTRIBUTE_SetIsDirectPosition(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISDIRECTPOSITION) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISDIRECTPOSITION))

#define gcmATTRIBUTE_SetEnabled(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ENABLED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ENABLED))

#define gcmATTRIBUTE_SetIsInvariant(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISINVARIANT) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISINVARIANT))

#define gcmATTRIBUTE_SetIsPerVertexArray(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISPERVERTEXARRAY) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISPERVERTEXARRAY))

#define gcmATTRIBUTE_SetIsPerVertexNotArray(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISPERVERTEXNOTARRAY) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISPERVERTEXNOTARRAY))

#define gcmATTRIBUTE_SetIsPrecise(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISPRECISE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISPRECISE))

#define gcmATTRIBUTE_SetIsIOBlockMember(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISIOBLOCKMEMBER) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISIOBLOCKMEMBER))

#define gcmATTRIBUTE_SetIsInstanceMember(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISINSTANCEMEMBER) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISINSTANCEMEMBER))

#define gcmATTRIBUTE_SetIsCentroid(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISCENTROID) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISCENTROID))

#define gcmATTRIBUTE_SetIsSample(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISSAMPLE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISSAMPLE))

#define gcmATTRIBUTE_SetIsStaticallyUsed(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISSTATICALLYUSED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISSTATICALLYUSED))

#define gcmATTRIBUTE_SetIsCompilerGen(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_COMPILERGEN) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_COMPILERGEN))

#define gcmATTRIBUTE_SetIsUseAsInterpolate(att,v )  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_ISUSEASINTERPOLATE) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_ISUSEASINTERPOLATE))

#define gcmATTRIBUTE_SetLocSetByDriver(att, v)  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_LOC_SET_BY_DRIVER) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_LOC_SET_BY_DRIVER))

#define gcmATTRIBUTE_SetLocHasAlias(att, v)  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_LOC_HAS_ALIAS) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_LOC_HAS_ALIAS))

#define gcmATTRIBUTE_SetRegAllocated(att, v)  \
        ((att)->flags_ = ((att)->flags_ & ~gcATTRIBUTE_REG_ALLOCATED) | \
                          ((v) == gcvFALSE ? 0 : gcATTRIBUTE_REG_ALLOCATED))

/* Forwarded declaration */
typedef struct _gcSHADER *              gcSHADER;
typedef struct _gcATTRIBUTE *           gcATTRIBUTE;
typedef struct _gcUNIFORM *             gcUNIFORM;
typedef struct _gcsUNIFORM_BLOCK *      gcsUNIFORM_BLOCK;
typedef struct _gcsSTORAGE_IO_BLOCK *   gcsSTORAGE_BLOCK;
typedef struct _gcsIO_BLOCK *           gcsIO_BLOCK;
typedef struct _gcsINTERFACE_BLOCK_INFO gcsINTERFACE_BLOCK_INFO;
typedef struct _gcsSHADER_VAR_INFO      gcsSHADER_VAR_INFO;
typedef struct _gcOUTPUT *              gcOUTPUT;
typedef struct _gcsFUNCTION *           gcFUNCTION;
typedef struct _gcsKERNEL_FUNCTION *    gcKERNEL_FUNCTION;
typedef struct _gcVARIABLE *            gcVARIABLE;
typedef struct _gcSHADER_LIST *         gcSHADER_LIST;
typedef struct _gcBINARY_LIST *         gcBINARY_LIST;

struct _gcSHADER_LIST
{
    gcSHADER_LIST               next;
    gctINT                      index;
    gctINT                      data0;
    gctINT                      data1;
};

struct _gcBINARY_LIST
{
    gctINT                      index;
    gctINT                      data0;
    gctINT                      data1;
};

/* Structure the defines an attribute (input) for a shader. */
struct _gcATTRIBUTE
{
    /* The object. */
    gcsOBJECT                    object;

    /* Index of the attribute. */
    gctUINT16                    index;

    /* Type of the attribute. */
    gcSHADER_TYPE                type;

    /* Precision of the uniform. */
    gcSHADER_PRECISION           precision;

    /* Number of array elements for this attribute. */
    gctINT                       arraySize;

    /* Number of array level. Right now it can only be 0 or 1. */
    gctINT                       arrayLength;

    /* Flag to indicate this attribute is used as a texture coordinate
       or packedAway. */
    gctUINT32                    flags_;

    /* Assigned input register index. */
    gctINT                       inputIndex;

    /* Flat input or smooth input. */
    gcSHADER_SHADERMODE          shaderMode;

    /* If other attributes packed to this attribute, we need to change the shade mode for each components. */
    gcSHADER_SHADERMODE          componentShadeMode[4];

    /* Location index. */
    gctINT                       location;

    /* Field index. */
    gctINT                       fieldIndex;

    /* IO block index. */
    gctINT                       ioBlockIndex;

    /* IO block array index. */
    gctINT                       ioBlockArrayIndex;

    /* Only used for a IO block member, point to the next element in block. */
    gctINT16                     nextSibling;

    /* Only used for a IO block member, point to the previous element in block. */
    gctINT16                     prevSibling;

    /* The variable index of the type name, it is only enabled for a structure member only. */
    gctINT16                     typeNameVarIndex;

    /* Length of the attribute name. */
    gctINT                       nameLength;

    /* The attribute name. */
    char                         name[1];
};

#define GetATTRObject(a)                    ((a)->object)
#define GetATTRIndex(a)                     ((a)->index)
#define GetATTRType(a)                      ((a)->type)
#define SetATTRType(a, i)                   (GetATTRType(a) = (i))
#define GetATTRPrecision(a)                 ((a)->precision)
#define GetATTRArraySize(a)                 ((a)->arraySize)
#define GetATTRIsArray(a)                   ((a)->arrayLength)
#define GetATTRFlags(a)                     ((a)->flags_)
#define GetATTRInputIndex(a)                ((a)->inputIndex)
#define GetATTRShaderMode(a)                ((a)->shaderMode)
#define GetATTRComponentShaderModes(a)      ((a)->componentShadeMode)
#define GetATTRComponentShaderMode(a, i)    ((a)->componentShadeMode[i])
#define GetATTRLocation(a)                  ((a)->location)
#define SetATTRLocation(a, l)               (GetATTRLocation(a) = (l))
#define GetATTRFieldIndex(a)                ((a)->fieldIndex)
#define GetATTRNameLength(a)                ((a)->nameLength)
#define GetATTRName(a)                      ((a)->name)
#define GetATTRIOBlockIndex(a)              ((a)->ioBlockIndex)
#define SetATTRIOBlockIndex(a, i)           (GetATTRIOBlockIndex(a) = (i))
#define GetATTRIOBlockArrayIndex(a)         ((a)->ioBlockArrayIndex)
#define SetATTRIOBlockArrayIndex(a, i)      (GetATTRIOBlockArrayIndex(a) = (i))
#define GetATTRNextSibling(a)               ((a)->nextSibling)
#define SetATTRNextSibling(a, i)            (GetATTRNextSibling(a) = (i))
#define GetATTRPrevSibling(a)               ((a)->prevSibling)
#define SetATTRPrevSibling(a, i)            (GetATTRPrevSibling(a) = (i))
#define GetATTRTypeNameVarIndex(a)          ((a)->typeNameVarIndex)
#define SetATTRTypeNameVarIndex(a, i)       (GetATTRTypeNameVarIndex(a) = (i))

/* Sampel structure, but inside a binary. */
typedef struct _gcBINARY_ATTRIBUTE
{
    /* Index of the attribute. */
    gctUINT16                     index;

    /* Type for this attribute of type gcATTRIBUTE_TYPE. */
    gctINT16                      type;

    /* Flag to indicate this attribute is used as a texture coordinate
       or packedAway. */
    gctUINT16                     flags1;
    gctUINT16                     flags2;

    /* Number of array elements for this attribute. */
    gctINT16                      arraySize;

    /* Number of array level. Right now it can only be 0 or 1. */
    gctINT16                      arrayLength;

    /* Length of the attribute name. */
    gctINT16                      nameLength;

    /* precision */
    gctINT16                      precision;

    /* IO block index. */
    gctINT16                      ioBlockIndex;

    /* IO block array index. */
    gctINT16                      ioBlockArrayIndex;

    /* Only used for a IO block member, point to the next element in block. */
    gctINT16                      nextSibling;

    /* Only used for a IO block member, point to the previous element in block. */
    gctINT16                      prevSibling;

    /* The variable index of the type name, it is only enabled for a structure member only. */
    gctINT16                      typeNameVarIndex;

    /* shader mode: flat/smooth/... */
    gctINT16                      shaderMode;

    /* The attribute name. */
    char                          name[1];
}
* gcBINARY_ATTRIBUTE;

/* Structure that defines inteface information associated with a variable (maybe a gcUNIFORM, gcVARIABLE) for a shader */
struct _gcsSHADER_VAR_INFO
{
    /* Variable category */
    gcSHADER_VAR_CATEGORY         varCategory;

    /* Data type for this most-inner variable. */
    gcSHADER_TYPE                 type;

    /* Only used for structure, block, or block member;
       When used for structure: it points to either first array element for
       arrayed struct or first struct element if struct is not arrayed;

       When used for block, point to first element in block */
    gctINT16                      firstChild;

    /* Only used for structure or blocks;
       When used for structure: it points to either next array element for
       arrayed struct or next struct element if struct is not arrayed;

       When used for block: it points to the next array element for block, if any */
    gctINT16                      nextSibling;

    /* Only used for structure or blocks;
       When used for structure: it points to either prev array element for
       arrayed struct or prev struct element if struct is not arayed;

       When used for block: it points to the previous array element for block, if any */
    gctINT16                      prevSibling;

    /* Only used for structure element, point to parent _gcUNIFORM */
    gctINT16                      parent;

    union
    {
        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                 numStructureElement;

        /* Number of element in block */
        gctUINT16                 numBlockElement;
    } u;


    /* Default block uniform location */
    gctINT32                      location;

    /* Atomic counter binding */
    gctINT32                      binding;

    /* Atomic counter offset */
    gctINT32                      offset;

    gcSHADER_PRECISION            precision  : 8;    /* Precision of the uniform. */
    gctBOOL                       isArray    : 2;    /* Is variable a true array */
    gctBOOL                       isLocal    : 2;    /* Is local variable */
    gctBOOL                       isOutput   : 2;    /* Is output variable */
    gctBOOL                       isPrecise  : 2;    /* Is precise variable */
    gctBOOL                       isPerVertex: 2;    /* Is per-vertex */
    gctBOOL                       isPointer  : 2;    /* Whether the variable is a pointer. */

    /* Number of array elements for this variable(for an array of array, it saves the array size of the higher array). */
    gctINT                        arraySize;

    /* Number of array level. */
    gctINT                        arrayCount;

    /* Array size for every array level. */
    gctINT *                      arraySizeList;

    /* Format of element of the variable. */
    gcSL_FORMAT                   format;

    /* Format of image */
    gceIMAGE_FORMAT               imageFormat;

};

#define GetSVICategory(s)                   ((s)->varCategory)
#define SetSVICategory(s, i)                (GetSVICategory(s) = (i))
#define GetSVIType(s)                       ((s)->type)
#define SetSVIType(s, i)                    (GetSVIType(s) = (i))
#define GetSVIFirstChild(s)                 ((s)->firstChild)
#define SetSVIFirstChild(s, i)              (GetSVIFirstChild(s) = (i))
#define GetSVINextSibling(s)                ((s)->nextSibling)
#define SetSVINextSibling(s, i)             (GetSVINextSibling(s) = (i))
#define GetSVIPrevSibling(s)                ((s)->prevSibling)
#define SetSVIPrevSibling(s, i)             (GetSVIPrevSibling(s) = (i))
#define GetSVIParent(s)                     ((s)->parent)
#define SetSVIParent(s, i)                  (GetSVIParent(s) = (i))
#define GetSVINumStructureElement(s)        ((s)->u.numStructureElement)
#define SetSVINumStructureElement(s, i)     (GetSVINumStructureElement(s) = (i))
#define GetSVINumBlockElement(s)            ((s)->u.numBlockElement)
#define SetSVINumBlockElement(s, i)         (GetSVINumBlockElement(s) = (i))
#define GetSVIPrecision(s)                  ((s)->precision)
#define SetSVIPrecision(s, i)               (GetSVIPrecision(s) = (i))
#define GetSVIBinding(s)                    ((s)->binding)
#define SETSVIBinding(s, b)                 ((s)->binding = (b))
#define GetSVIIsArray(s)                    ((s)->isArray)
#define SetSVIIsArray(s, i)                 (GetSVIIsArray(s) = (i))
#define GetSVIIsLocal(s)                    ((s)->isLocal)
#define SetSVIIsLocal(s, i)                 (GetSVIIsLocal(s) = (i))
#define GetSVIIsOutput(s)                   ((s)->isOutput)
#define SetSVIIsOutput(s, i)                (GetSVIIsOutput(s) = (i))
#define GetSVIIsPointer(s)                  ((s)->isPointer)
#define SetSVIIsPointer(s, i)               (GetSVIIsPointer(s) = (i))
#define GetSVIIsPerVertex(s)                ((s)->isPerVertex)
#define SetSVIIsPerVertex(s, i)             (GetSVIIsPerVertex(s) = (i))
#define GetSVIArraySize(s)                  ((s)->arraySize)
#define SetSVIArraySize(s, i)               (GetSVIArraySize(s) = (i))
#define GetSVIFormat(s)                     ((s)->format)
#define SetSVIFormat(s, i)                  (GetSVIFormat(s) = (i))
#define GetSVIImageFormat(s)                ((s)->imageFormat)
#define SetSVIImageFormat(s, i)             (GetSVIFormat(s) = (i))
#define GetSVILocation(s)                   ((s)->location)
#define SetSVILocation(s, i)                (GetSVILocation(s) = (i))

#define gcvBLOCK_INDEX_DEFAULT           -1

typedef enum _gceInterfaceBlock_Flag
{
    gceIB_FLAG_NONE                = 0x00,
    gceIB_FLAG_UNSIZED             = 0x01, /* last member of storage block is unsized array */
    gceIB_FLAG_FOR_SHARED_VARIABLE = 0x02, /* block for shared variables */
    gceIB_FLAG_WITH_INSTANCE_NAME  = 0x04, /* block with a instance name. */
    gceIB_FLAG_STATICALLY_USED     = 0x08, /* blcok is statically used */
} gceInterfaceBlock_Flag;

/* Structure that defines a common interface block info for a shader. */
struct _gcsINTERFACE_BLOCK_INFO
{
    /* The object which would denote the block type. */
    gcsOBJECT                       object;

    gcsSHADER_VAR_INFO              variableInfo;

    /* Interface block index */
    gctINT16                        blockIndex;

    /* If interface block is an array, use this field to record the array index of this
    ** block array element
    */
    gctINT16                        arrayIndex;

    /* Index of the uniform to keep the base address. */
    gctINT16                        index;

    /* temp register to contain the base address of shared variable storage block*/
    gctINT                          _sharedVariableBaseAddress;

    /* interface block flag */
    gceInterfaceBlock_Flag          flags;

    /* Memory layout */
    gceINTERFACE_BLOCK_LAYOUT_ID    memoryLayout;

    /* block size in bytes */
    gctUINT32                       blockSize;

    /* binding point */
    gctINT32                        binding;
};

#define GetIBIObject(ibi)                    (&((ibi)->object))
#define SetIBIObject(ibi, i)                 (*GetIBIObject(ibi) = (i))
#define GetIBIShaderVarInfo(ibi)             (&((ibi)->variableInfo))
#define SetIBIShaderVarInfo(ibi, i)          (*GetIBIShaderVarInfo(ibi) = (i))
#define GetIBIBlockIndex(ibi)                ((ibi)->blockIndex)
#define SetIBIBlockIndex(ibi, i)             (GetIBIBlockIndex(ibi) = (i))
#define GetIBIArrayIndex(ibi)                ((ibi)->arrayIndex)
#define SetIBIArrayIndex(ibi, i)             (SetIBIArrayIndex(ibi) = (i))
#define GetIBIIndex(ibi)                     ((ibi)->index)
#define SetIBIIndex(ibi, i)                  (GetIBIIndex(ibi) = (i))
#define GetIBISharedVariableBaseAddress(ibi) ((ibi)->_sharedVariableBaseAddress)
#define SetIBISharedVariableBaseAddress(ibi, i) (GetIBISharedVariableBaseAddress(ibi) = (i))
#define GetIBIFlag(ibi)                      ((ibi)->flags)
#define SetIBIFlag(ibi, i)                   do { (ibi)->flags |= (gceInterfaceBlock_Flag)(i); } while (0)
#define HasIBIFlag(ibi, i)                   ((ibi)->flags & (gceInterfaceBlock_Flag)(i))
#define GetIBIMemoryLayout(ibi)              ((ibi)->memoryLayout)
#define SetIBIMemoryLayout(ibi, i)           (GetIBIMemoryLayout(ibi) = (i))
#define GetIBIBlockSize(ibi)                 ((ibi)->blockSize)
#define SetIBIBlockSize(ibi, i)              (GetIBIBlockSize(ibi) = (i))
#define GetIBIBinding(ibi)                   ((ibi)->binding)
#define SetIBIBinding(ibi, i)                (GetIBIBinding(ibi) = (i))

#define isIBIUniformBlock(ibi)               (GetIBIObject(ibi)->type == gcvOBJ_UNIFORM_BLOCK)
#define isIBIStorageBlock(ibi)               (GetIBIObject(ibi)->type == gcvOBJ_STORAGE_BLOCK)

#define GetIBICategory(s)                   GetSVICategory(GetIBIShaderVarInfo(s))
#define SetIBICategory(s, i)                SetSVICategory(GetIBIShaderVarInfo(s), (i))
#define GetIBIType(s)                       GetSVIType(GetIBIShaderVarInfo(s))
#define SetIBIType(s, i)                    SetSVIType(GetIBIShaderVarInfo(s), (i))
#define GetIBIFirstChild(s)                 GetSVIFirstChild(GetIBIShaderVarInfo(s))
#define SetIBIFirstChild(s, i)              SetSVIFirstChild(GetIBIShaderVarInfo(s), (i))
#define GetIBINextSibling(s)                GetSVINextSibling(GetIBIShaderVarInfo(s))
#define SetIBINextSibling(s, i)             SetSVINextSibling(GetIBIShaderVarInfo(s), (i))
#define GetIBIPrevSibling(s)                GetSVIPrevSibling(GetIBIShaderVarInfo(s))
#define SetIBIPrevSibling(s, i)             SetSVIPrevSibling(GetIBIShaderVarInfo(s), (i))
#define GetIBIParent(s)                     GetSVIParent(GetIBIShaderVarInfo(s))
#define SetIBIParent(s, i)                  SetSVIParent(GetIBIShaderVarInfo(s), (i))
#define GetIBINumStructureElement(s)        GetSVINumStructureElement(GetIBIShaderVarInfo(s))
#define SetIBINumStructureElement(s, i)     SetSVINumStructureElement(GetIBIShaderVarInfo(s), (i))
#define GetIBINumBlockElement(s)            GetSVINumBlockElement(GetIBIShaderVarInfo(s))
#define SetIBINumBlockElement(s, i)         SetSVINumBlockElement(GetIBIShaderVarInfo(s), (i))
#define GetIBIPrecision(s)                  GetSVIPrecision(GetIBIShaderVarInfo(s))
#define SetIBIPrecision(s, i)               SetSVIPrecision(GetIBIShaderVarInfo(s), (i))
#define GetIBIIsArray(s)                    GetSVIIsArray(GetIBIShaderVarInfo(s))
#define SetIBIIsArray(s, i)                 SetSVIIsArray(GetIBIShaderVarInfo(s), (i))
#define GetIBIArraySize(s)                  GetSVIArraySize(GetIBIShaderVarInfo(s))
#define SetIBIArraySize(s, i)               SetSVIArraySize(GetIBIShaderVarInfo(s), (i))
#define GetIBILocation(s)                   GetSVILocation(GetIBIShaderVarInfo(s))
#define SetIBILocation(s, i)                SetSVILocation(GetIBIShaderVarInfo(s), (i))

/* Structure that defines a uniform block for a shader. */
struct _gcsUNIFORM_BLOCK
{
    gcsINTERFACE_BLOCK_INFO         interfaceBlockInfo;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind;

    gctBOOL                         _useLoadInst; /* indicate to use LOAD */
    gctBOOL                         _finished;

    /* number of uniforms in block */
    gctUINT32                       uniformCount;

    /* array of pointers to uniforms in block */
    gcUNIFORM                       *uniforms;

    /* If a uniform is used on both VS and PS,
    ** the index of this uniform on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Length of the uniform block name. */
    gctINT                          nameLength;

    /* The uniform block name. */
    char                            name[1];
};

#define GetUBInterfaceBlockInfo(u)          (&((u)->interfaceBlockInfo))
#define SetUBInterfaceBlockInfo(u, i)       (*GetUBInterfaceBlockInfo(u) = (i))
#define GetUBObject(u)                      (&(GetUBInterfaceBlockInfo(u)->object))
#define SetUBObject(u, i)                   (*GetUBObject(u) = (i))
#define GetUBUseLoadInst(u)                 ((u)->_useLoadInst)
#define SetUBUseLoadInst(u, i)              (GetUBUseLoadInst(u), (i))
#define GetUBFinished(u)                    ((u)->_finished)
#define SetUBFinished(u, i)                 (GetUBFinished(u), (i))
#define GetUBShaderVarInfo(u)               (&(GetUBInterfaceBlockInfo(u)->variableInfo))
#define SetUBShaderVarInfo(u, i)            (*GetUBShaderVarInfo(u) = (i))
#define GetUBBlockIndex(u)                  (GetUBInterfaceBlockInfo(u)->blockIndex)
#define SetUBBlockIndex(u, i)               (GetUBBlockIndex(u) = (i))
#define GetUBIndex(u)                       (GetUBInterfaceBlockInfo(u)->index)
#define SetUBIndex(u, i)                    (GetUBIndex(u) = (i))
#define GetUBMemoryLayout(u)                (GetUBInterfaceBlockInfo(u)->memoryLayout)
#define SetUBMemoryLayout(u, i)             (GetUBMemoryLayout(u) = (i))
#define GetUBBlockSize(u)                   (GetUBInterfaceBlockInfo(u)->blockSize)
#define SetUBBlockSize(u, i)                (GetUBBlockSize(u) = (i))
#define GetUBBinding(u)                     (GetUBInterfaceBlockInfo(u)->binding)
#define SetUBBinding(u, i)                  (GetUBBinding(u) = (i))
#define GetUBUniformCount(u)                ((u)->uniformCount)
#define SetUBUniformCount(u, i)             (GetUBUniformCount(u) = (i))
#define GetUBUniforms(u)                    ((u)->uniforms)
#define SetUBUniforms(u, i)                 (GetUBUniforms(u) = (i))
#define GetUBMatchIndex(u)                  ((u)->matchIndex)
#define SetUBMatchIndex(u, i)               (GetUBMatchIndex(u) = (i))
#define GetUBArrayIndex(u)                  (GetUBInterfaceBlockInfo(u)->arrayIndex)
#define SetUBArrayIndex(u, i)               (GetUBArrayIndex(u) = (i))
#define GetUBNameLength(u)                  ((u)->nameLength)
#define SetUBNameLength(u, i)               (GetUBNameLength(u) = (i))
#define GetUBName(u)                        ((u)->name)
#define SetUBName(u, i)                     (GetUBName(u) = (i))
#define GetUBShaderKind(u)                  ((u)->shaderKind)
#define SetUBShaderKind(u, i)               (GetUBShaderKind(u) = (i))
#define GetUBFlag(u)                        GetIBIFlag(GetUBInterfaceBlockInfo(u))
#define SetUBFlag(u, i)                     SetIBIFlag(GetUBInterfaceBlockInfo(u), (i))

#define GetUBCategory(s)                   GetSVICategory(GetUBShaderVarInfo(s))
#define SetUBCategory(s, i)                SetSVICategory(GetUBShaderVarInfo(s), (i))
#define GetUBType(s)                       GetSVIType(GetUBShaderVarInfo(s))
#define SetUBType(s, i)                    SetSVIType(GetUBShaderVarInfo(s), (i))
#define GetUBFirstChild(s)                 GetSVIFirstChild(GetUBShaderVarInfo(s))
#define SetUBFirstChild(s, i)              SetSVIFirstChild(GetUBShaderVarInfo(s), (i))
#define GetUBNextSibling(s)                GetSVINextSibling(GetUBShaderVarInfo(s))
#define SetUBNextSibling(s, i)             SetSVINextSibling(GetUBShaderVarInfo(s), (i))
#define GetUBPrevSibling(s)                GetSVIPrevSibling(GetUBShaderVarInfo(s))
#define SetUBPrevSibling(s, i)             SetSVIPrevSibling(GetUBShaderVarInfo(s), (i))
#define GetUBParent(s)                     GetSVIParent(GetUBShaderVarInfo(s))
#define SetUBParent(s, i)                  SetSVIParent(GetUBShaderVarInfo(s), (i))
#define GetUBNumStructureElement(s)        GetSVINumStructureElement(GetUBShaderVarInfo(s))
#define SetUBNumStructureElement(s, i)     SetSVINumStructureElement(GetUBShaderVarInfo(s), (i))
#define GetUBNumBlockElement(s)            GetSVINumBlockElement(GetUBShaderVarInfo(s))
#define SetUBNumBlockElement(s, i)         SetSVINumBlockElement(GetUBShaderVarInfo(s), (i))
#define GetUBPrecision(s)                  GetSVIPrecision(GetUBShaderVarInfo(s))
#define SetUBPrecision(s, i)               SetSVIPrecision(GetUBShaderVarInfo(s), (i))
#define GetUBIsArray(s)                    GetSVIIsArray(GetUBShaderVarInfo(s))
#define SetUBIsArray(s, i)                 SetSVIIsArray(GetUBShaderVarInfo(s), (i))
#define GetUBArraySize(s)                  GetSVIArraySize(GetUBShaderVarInfo(s))
#define SetUBArraySize(s, i)               SetSVIArraySize(GetUBShaderVarInfo(s), (i))

/* Structure that defines a uniform block for a shader. */
typedef struct _gcBINARY_UNIFORM_BLOCK
{
    /* Memory layout */
    gctUINT16                       memoryLayout;

    /* block size in bytes */
    gctUINT16                       blockSize;

    /* Number of element in block */
    gctUINT16                       numBlockElement;

    /* points to first element in block */
    gctINT16                        firstChild;

    /* points to the next array element for block, if any */
    gctINT16                        nextSibling;

    /* points to the previous array element for block, if any */
    gctINT16                        prevSibling;

    /* Index of the uniform to keep the base address. */
    gctINT16                        index;

    /* Index of the uniform to keep the base address. */
    gctINT16                        arrayIndex;

    /* Length of the uniform block name. */
    gctINT16                        nameLength;

    /* binding point */
    char                            binding[sizeof(gctINT32)];

    /* The uniform block name. */
    char                            name[1];
}
*gcBINARY_UNIFORM_BLOCK;

/* Structure that defines a storage block for a shader. */
struct _gcsSTORAGE_IO_BLOCK
{
    gcsINTERFACE_BLOCK_INFO         interfaceBlockInfo;

    /* number of variables in block */
    gctUINT32                       variableCount;

    /* array of pointers to variables in block */
    gcVARIABLE                      *variables;

    /* If a storage buffer object is used on both VS and PS,
    ** the index of this SBO on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Length of the block name. */
    gctINT                          nameLength;

    /* The block name. */
    char                            name[1];
};

/* Structure that defines a IO block for a shader. */
struct _gcsIO_BLOCK
{
    gcsINTERFACE_BLOCK_INFO         interfaceBlockInfo;

    /* Length of the block name. */
    gctINT                          nameLength;

    /* Length of the instance name. */
    gctINT                          instanceNameLength;

    /* The name, it would be BlockName{.instanceName} */
    char                            name[1];
};

#define GetSBInterfaceBlockInfo(s)          (&((s)->interfaceBlockInfo))
#define SetSBInterfaceBlockInfo(s, i)       (*GetSBInterfaceBlockInfo(s) = (i))
#define GetSBObject(s)                      (&(GetSBInterfaceBlockInfo(s)->object))
#define SetSBObject(s, i)                   (*GetSBObject(s) = (i))
#define GetSBShaderVarInfo(s)               (&(GetSBInterfaceBlockInfo(s)->variableInfo))
#define SetSBShaderVarInfo(s, i)            (*GetSBShaderVarInfo(s) = (i))
#define GetSBBlockIndex(s)                  (GetSBInterfaceBlockInfo(s)->blockIndex)
#define SetSBBlockIndex(s, i)               (GetSBBlockIndex(s) = (i))
#define GetSBIndex(s)                       (GetSBInterfaceBlockInfo(s)->index)
#define SetSBIndex(s, i)                    (GetSBIndex(s) = (i))
#define GetSBSharedVariableBaseAddress(s)   (GetSBInterfaceBlockInfo(s)->_sharedVariableBaseAddress)
#define SetSBSharedVariableBaseAddress(s, i) (GetSBSharedVariableBaseAddress(s) = (i))

#define GetSBMemoryLayout(s)                (GetSBInterfaceBlockInfo(s)->memoryLayout)
#define SetSBMemoryLayout(s, i)             (GetSBMemoryLayout(s) = (i))
#define GetSBBlockSize(s)                   (GetSBInterfaceBlockInfo(s)->blockSize)
#define SetSBBlockSize(s, i)                (GetSBBlockSize(s) = (i))
#define GetSBBinding(s)                     (GetSBInterfaceBlockInfo(s)->binding)
#define SetSBBinding(s, i)                  (GetSBBinding(s) = (i))
#define GetSBVariableCount(s)               ((s)->variableCount)
#define SetSBVariableCount(s, i)            (GetSBVariableCount(s) = (i))
#define GetSBVariables(s)                   ((s)->variables)
#define GetSBVariable(s, i)                 ((s)->variables[(i)])
#define SetSBVariables(s, i)                (GetSBVariables(s) = (i))
#define GetSBNameLength(s)                  ((s)->nameLength)
#define SetSBNameLength(s, i)               (GetSBNameLength(s) = (i))
#define GetSBName(s)                        ((s)->name)
#define SetSBName(s, i)                     (GetSBName(s) = (i))
#define GetSBInstanceNameLength(s)          ((s)->instanceNameLength)
#define SetSBInstanceNameLength(s, i)       (GetSBInstanceNameLength(s) = (i))
#define GetSBMatchIndex(s)                  ((s)->matchIndex)
#define SetSBMatchIndex(s, i)               (GetSBMatchIndex(s) = (i))
#define GetSBFlag(s)                        GetIBIFlag(GetSBInterfaceBlockInfo(s))
#define SetSBFlag(s, i)                     SetIBIFlag(GetSBInterfaceBlockInfo(s), (i))
#define GetSBCategory(s)                   GetSVICategory(GetSBShaderVarInfo(s))
#define SetSBCategory(s, i)                SetSVICategory(GetSBShaderVarInfo(s), (i))
#define GetSBType(s)                       GetSVIType(GetSBShaderVarInfo(s))
#define SetSBType(s, i)                    SetSVIType(GetSBShaderVarInfo(s), (i))
#define GetSBFirstChild(s)                 GetSVIFirstChild(GetSBShaderVarInfo(s))
#define SetSBFirstChild(s, i)              SetSVIFirstChild(GetSBShaderVarInfo(s), (i))
#define GetSBNextSibling(s)                GetSVINextSibling(GetSBShaderVarInfo(s))
#define SetSBNextSibling(s, i)             SetSVINextSibling(GetSBShaderVarInfo(s), (i))
#define GetSBPrevSibling(s)                GetSVIPrevSibling(GetSBShaderVarInfo(s))
#define SetSBPrevSibling(s, i)             SetSVIPrevSibling(GetSBShaderVarInfo(s), (i))
#define GetSBParent(s)                     GetSVIParent(GetSBShaderVarInfo(s))
#define SetSBParent(s, i)                  SetSVIParent(GetSBShaderVarInfo(s), (i))
#define GetSBNumStructureElement(s)        GetSVINumStructureElement(GetSBShaderVarInfo(s))
#define SetSBNumStructureElement(s, i)     SetSVINumStructureElement(GetSBShaderVarInfo(s), (i))
#define GetSBNumBlockElement(s)            GetSVINumBlockElement(GetSBShaderVarInfo(s))
#define SetSBNumBlockElement(s, i)         SetSVINumBlockElement(GetSBShaderVarInfo(s), (i))
#define GetSBPrecision(s)                  GetSVIPrecision(GetSBShaderVarInfo(s))
#define SetSBPrecision(s, i)               SetSVIPrecision(GetSBShaderVarInfo(s), (i))
#define GetSBIsArray(s)                    GetSVIIsArray(GetSBShaderVarInfo(s))
#define SetSBIsArray(s, i)                 SetSVIIsArray(GetSBShaderVarInfo(s), (i))
#define GetSBArraySize(s)                  GetSVIArraySize(GetSBShaderVarInfo(s))
#define SetSBArraySize(s, i)               SetSVIArraySize(GetSBShaderVarInfo(s), (i))

/* Structure that defines a storage block for a shader. */
typedef struct _gcBINARY_STORAGE_IO_BLOCK
{
    /* Memory layout */
    gctUINT16                       memoryLayout;

    /* flags */
    gctUINT16                       flags;

    /* block size in bytes */
    gctUINT16                       blockSize;

    /* Number of element in block */
    gctUINT16                       numBlockElement;

    /* points to first element in block */
    gctINT16                        firstChild;

    /* points to the next array element for block, if any */
    gctINT16                        nextSibling;

    /* points to the previous array element for block, if any */
    gctINT16                        prevSibling;

    /* Index of the uniform to keep the base address. */
    gctINT16                        index;

    /* Length of the storage block name. */
    gctINT16                        nameLength;

    /* Length of the instance name. */
    gctINT16                        instanceNameLength;

    /* binding point */
    char                            binding[sizeof(gctINT32)];

    /* The storage block name. */
    char                            name[1];
}
*gcBINARY_STORAGE_BLOCK;

typedef struct _gcBINARY_STORAGE_IO_BLOCK * gcBINARY_IO_BLOCK;

typedef enum _gcUNIFORM_RES_OP_FLAG
{
    gcUNIFORM_RES_OP_FLAG_NONE        = 0x0000,
    gcUNIFORM_RES_OP_FLAG_TEXLD       = 0x0001,
    gcUNIFORM_RES_OP_FLAG_TEXLD_BIAS  = 0x0002,
    gcUNIFORM_RES_OP_FLAG_TEXLD_LOD   = 0x0004,
    gcUNIFORM_RES_OP_FLAG_TEXLD_GRAD  = 0x0008,
    gcUNIFORM_RES_OP_FLAG_TEXLDP      = 0x0010,
    gcUNIFORM_RES_OP_FLAG_TEXLDP_GRAD = 0x0020,
    gcUNIFORM_RES_OP_FLAG_TEXLDP_BIAS = 0x0040,
    gcUNIFORM_RES_OP_FLAG_TEXLDP_LOD  = 0x0080,
    gcUNIFORM_RES_OP_FLAG_FETCH       = 0x0100,
    gcUNIFORM_RES_OP_FLAG_FETCH_MS    = 0x0200,
    gcUNIFORM_RES_OP_FLAG_GATHER      = 0x0400,
    gcUNIFORM_RES_OP_FLAG_GATHER_PCF  = 0x0800,
    gcUNIFORM_RES_OP_FLAG_LODQ        = 0x1000,
}gcUNIFORM_RES_OP_FLAG;

/* Structure that defines an uniform (constant register) for a shader. */
struct _gcUNIFORM
{
    /* The object. */
    gcsOBJECT                       object;

    /* Index of the uniform. */
    gctUINT16                       index;

    /* Uniform block index: Default block index = -1 */
    gctINT16                        blockIndex;

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Index to image sampler for OpenCL */
    gctUINT16                       imageSamplerIndex;

    /* If a uniform is used on both VS and PS,
    ** the index of this uniform on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Variable category */
    gcSHADER_VAR_CATEGORY           _varCategory : 8;

    /* Physically assigned values. */
    gctUINT8                        swizzle      : 8;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind : 5;

        /* memory order of matrices */
    gctBOOL                         isRowMajor : 2;

    /* Whether the uniform is a pointer. */
    gctBOOL                         isPointer  : 2;


    /*
    ** 1) If this uniform is a normal constant uniform, save the const index in physical.
    ** 2) If this uniform is a normal texture uniform, save the sampler index in physical.
    ** 3) If this uniform is a sampler_buffer, save the const index in physical,
    **    save the sampler index in samplerPhysical
    */
    gctINT                          physical;
    gctINT                          samplerPhysical;
    gctUINT32                       address;
    gctUINT32                       RAPriority;

    /* Flags. */
    gceUNIFORM_FLAGS                _flags;

    gcUNIFORM_RES_OP_FLAG           resOpFlag;

    /* stride on array or matrix */
    gctINT32                        arrayStride;
    gctINT16                        matrixStride;

    /* Number of array elements for this uniform. */
    gctINT                          arraySize;
    /* Number of array elements that are used in the shader. */
    gctINT                          usedArraySize;

    /* Number of array level. */
    gctINT                          arrayLengthCount;

    /* Array size for every array level. */
    gctINT *                        arrayLengthList;

    /* offset from uniform block's base address */
    gctINT32                        offset;

    union
    {
        /* Data type for this most-inner variable. */
        gcSHADER_TYPE               type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;

        /* Number of elements in block */
        gctUINT16                   numBlockElement;
    }
    u;

    /* type qualifier */
    gctTYPE_QUALIFIER               qualifier;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    /* ES31 explicit binding for opaque uniforms, 0 if not specify. */
    gctINT                          binding;

    /* ES31 explicit location for default block uniforms;  -1 if not specify. */
    gctINT                          location;

    /* Whether the uniform is part of model view projectoin matrix. */
    gctINT                          modelViewProjection;

    /* Format of element of the uniform shaderType. */
    gcSL_FORMAT                     format;

    /* Format of element of the uniform shaderType. */
    gctINT                          vectorSize;

    /* Offset to typeNameBuffer in shader to locate the non basic type name of uniform shaderType.
       For basic type: the value is -1
    */
    gctINT                          typeNameOffset;

    /* Compile-time constant value, */
    gcsValue                        initializer;

    /* ES31 explicit base address uniform for atomic counter;  -1 if not specify. */
    gctINT16                        baseBindingIdx;

    /* Dummy-uniform index for LTC uniform, -1 for non-ltc uniforms. */
    gctINT16                        dummyUniformIndex;

    /* Only used for structure or blocks;
       When used for structure, point to either first array element for
       arrayed struct or first struct element if struct is not arrayed

       When used for block, point to first element in block */
    gctINT16                        firstChild;

    /* Only used for structure or blocks;
       When used for structure, point to either next array element for
       arrayed struct or next struct element if struct is not arrayed;

       When used for block, point to next element in block */
    gctINT16                        nextSibling;

    /* Only used for structure or blocks;
       When used for structure, point to either prev array element for
       arrayed struct or prev struct element if struct is not arrayed;

       When used for block, point to previous element in block */
    gctINT16                        prevSibling;

    /* used for structure, point to parent _gcUNIFORM
       or for associated sampler of LOD_MIN_MAX and LEVEL_BASE_SIZE */
    gctINT16                        parent;

    /* image format */
    gctINT16                        imageFormat;

    gcUNIFORM                       followingAddr;
    gctUINT32                       followingOffset;            /* or sampler_t constant value */

    /* Length of the uniform name. */
    gctINT                          nameLength;

    /* The uniform name. */
    char                            name[1];
};

#define GetUniformCategory(u)                   ((u)->_varCategory)
#define SetUniformCategory(u, c)                ((u)->_varCategory = (c))
#define GetUniformBlockID(u)                    ((u)->blockIndex)
#define SetUniformBlockID(u, b)                 ((u)->blockIndex = (b))
#define GetUniformResOpFlags(u)                 ((u)->resOpFlag)
#define SetUniformResOpFlags(u, a)              ((u)->resOpFlag = (a))
#define SetUniformResOpFlag(u, a)               ((u)->resOpFlag |= (a))
#define GetUniformArrayStride(u)                ((u)->arrayStride)
#define SetUniformArrayStride(u, a)             ((u)->arrayStride = (a))
#define GetUniformMatrixStride(u)               ((u)->matrixStride)
#define SetUniformMatrixStride(u, m)            ((u)->matrixStride = (m))
#define GetUniformIsRowMajor(u)                 ((u)->isRowMajor)
#define GetUniformOffset(u)                     ((u)->offset)
#define GetUniformType(un)                      ((un)->u.type)
#define GetUniformNumStructureElement(un)       ((un)->u.numStructureElement)
#define SetUniformNumStructureElement(un, m)    ((un)->u.numStructureElement = (m))
#define GetUniformNumBlockElement(un)           ((un)->u.numBlockElement)
#define GetUniformIndex(u)                      ((u)->index)
#define GetUniformGlUniformIndex(u)             ((u)->glUniformIndex)
#define SetUniformGlUniformIndex(u, g)          ((u)->glUniformIndex = (g))
#define GetUniformImageSamplerIndex(u)          ((u)->imageSamplerIndex)
#define GetUniformPrecision(u)                  ((u)->precision)
#define GetUniformArraySize(u)                  ((u)->arraySize)
#define GetUniformUsedArraySize(u)              ((u)->usedArraySize)
#define SetUniformUsedArraySize(u, v)           (GetUniformUsedArraySize(u) = (v))
#define GetUniformBinding(u)                    ((u)->binding)
#define SetUniformBinding(u, b)                 ((u)->binding = (b))
#define GetUniformOffset(u)                     ((u)->offset)
#define SetUniformOffset(u, o)                  ((u)->offset = (o))
#define GetUniformLayoutLocation(u)             ((u)->location)
#define SetUniformLayoutLocation(u, l)          ((u)->location = (l))
#define GetUniformModelViewProjection(u)        ((u)->modelViewProjection)
#define GetUniformFormat(u)                     ((u)->format)
#define GetUniformVectorSize(u)                 ((u)->vectorSize)
#define SetUniformVectorSize(u, g)              (GetUniformVectorSize(u) = (g))
#define GetUniformTypeNameOffset(u)             ((u)->typeNameOffset)
#define SetUniformTypeNameOffset(u, g)          (GetUniformTypeNameOffset(u) = (g))
#define GetUniformIsPointer(u)                  ((u)->isPointer)
#define GetUniformPhysical(u)                   ((u)->physical)
#define SetUniformPhysical(u, p)                (GetUniformPhysical(u) = (p))
#define GetUniformSamplerPhysical(u)            ((u)->samplerPhysical)
#define SetUniformSamplerPhysical(u, p)         (GetUniformSamplerPhysical(u) = (p))
#define GetUniformSwizzle(u)                    ((u)->swizzle)
#define SetUniformSwizzle(u, s)                 (GetUniformSwizzle(u) = (s))
#define GetUniformAddress(u)                    ((u)->address)
#define GetUniformInitializer(u)                ((u)->initializer)
#define SetUniformInitializer(u, i)             (GetUniformInitializer(u) = (i))
#define GetUniformDummyUniformIndex(u)          ((u)->dummyUniformIndex)
#define setUniformDummyUniformIndex(u, i)       ((u)->dummyUniformIndex = (i))
#define GetUniformMatchIndex(u)                 ((u)->matchIndex)
#define GetUniformFirstChild(u)                 ((u)->firstChild)
#define GetUniformNextSibling(u)                ((u)->nextSibling)
#define SetUniformNextSibling(u, g)             (GetUniformNextSibling(u) = (g))
#define GetUniformPrevSibling(u)                ((u)->prevSibling)
#define SetUniformPrevSibling(u, g)             (GetUniformPrevSibling(u) = (g))
#define GetUniformParent(u)                     ((u)->parent)
#define SetUniformParent(u, g)                  ((u)->parent = (g))
#define GetUniformFollowingAddr(u)              ((u)->followingAddr)
#define SetUniformFollowingAddr(u, f)           ((u)->followingAddr = (f))
#define GetUniformFollowingOffset(u)            ((u)->followingOffset)
#define SetUniformFollowingOffset(u, f)         ((u)->followingOffset = (f))
#define GetUniformNameLength(u)                 ((u)->nameLength)
#define GetUniformName(u)                       ((u)->name)
#define GetUniformImageFormat(u)                ((u)->imageFormat)
#define SetUniformImageFormat(u, g)             ((u)->imageFormat = (g))
#define GetUniformQualifier(u)                  ((u)->qualifier)
#define SetUniformQualifier(u, g)               ((u)->qualifier |= (g))
#define ClrUniformAddrSpaceQualifier(u)         ((u)->qualifier &= ~gcd_ADDRESS_SPACE_QUALIFIERS)
#define SetUniformAddrSpaceQualifier(u, g)      ((u)->qualifier = ((u)->qualifier & ~gcd_ADDRESS_SPACE_QUALIFIERS) | (g))
#define GetUniformShaderKind(u)                 ((u)->shaderKind)
#define SetUniformShaderKind(u, g)              (GetUniformShaderKind(u) = (g))
#define GetUniformArrayLengthCount(u)           ((u)->arrayLengthCount)
#define GetUniformSingleLevelArraySzie(u, l)    ((u)->arrayLengthList[l])
#define isUniformArraysOfArrays(u)              (GetUniformArrayLengthCount(u) > 1)

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_UNIFORM
{
    union
    {
        /* Data type for this most-inner variable. */
        gctINT16                    type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;
    }
    u;

    /* Number of array elements for this uniform. */
    gctINT16                        arraySize;

    gctUINT16                       arrayLengthCount;

    /* Length of the uniform name. */
    gctINT16                        nameLength;

    /* uniform flags */
    char                            flags[sizeof(gceUNIFORM_FLAGS)];

    /* Physically assigned values. */
    gctUINT8                        swizzle;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind;

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcUNIFORM */
    gctINT16                        parent;

    /* precision */
    gctINT16                        precision;

    /* ES31 explicit location for default block uniforms;  -1 if not specify. */
    gctINT                          location;

    /* ES31 explicit base address uniform for atomic counter;  -1 if not specify. */
    gctINT16                        baseBindingIdx;

    /* HALTI extras begin: */
    /* Uniform block index:
       Default block index = -1 */
    gctINT16                        blockIndex;

    /* stride on array or matrix */
    char                            arrayStride[sizeof(gctINT32)];

    gctINT16                        matrixStride;

    /* memory order of matrices */
    gctINT16                        isRowMajor;

    /* offset from uniform block's base address */
    char                            offset[sizeof(gctINT32)];

    /* compile-time constant value */
    char                            initializer[sizeof(gcsValue)];

    /* Dummy-uniform index for LTC uniform, -1 for non-ltc uniforms. */
    gctINT16                        dummyUniformIndex;

    /* 3.1 explicit binding for opaque uniforms, -1 if not specify. */
    char                            binding[sizeof(gctINT32)];

    /* 3.1 image format */
    gctINT16                        imageFormat;

    /* Number of array elements that are used in the shader. */
    gctINT16                        usedArraySize;

    /* physical and address. */
    gctINT16                        physical;
    gctINT16                        samplerPhysical;
    char                            address[sizeof(gctUINT32)];

    char                            resOpFlag[sizeof(gctUINT32)];

    /* HALTI extras end: */
    /* The uniform arrayLengthList and name. */
    char                            memory[1];
}
* gcBINARY_UNIFORM;

/* Same structure, but inside a binary with more variables. */
typedef struct _gcBINARY_UNIFORM_EX
{
    /* Uniform type of type gcUNIFORM_TYPE. */
    union
    {
        /* Data type for this most-inner variable. */
        gctINT16                    type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;
    }
    u;

    /* Index of the uniform. */
    gctUINT16                       index;

    /* Number of array elements for this uniform. */
    gctINT16                        arraySize;

    gctUINT16                       arrayLengthCount;

    /* Flags. */
    char                            flags[sizeof(gceUNIFORM_FLAGS)];

    /* Format of element of the uniform shaderType. */
    gctUINT16                       format;

    /* Wheter the uniform is a pointer. */
    gctINT16                        isPointer;

    /* precision */
    gctINT16                        precision;

    /* Length of the uniform name. */
    gctINT16                        nameLength;

    /* Corresponding Index of Program's GLUniform */
    gctINT16                        glUniformIndex;

    /* Index to corresponding image sampler */
    gctUINT16                       imageSamplerIndex;

    /* compile-time constant value */
    char                            initializer[sizeof(gcsValue)];

    /* Dummy-uniform index for LTC uniform, -1 for non-ltc uniforms. */
    gctINT16                        dummyUniformIndex;

    /* type qualifier is currently 16bit long.
       If it ever changes to more than 16bits, the alignment has to be adjusted
       when writing out to a shader binary
    */
    gctTYPE_QUALIFIER               qualifier;

    /* Physically assigned values. */
    gctUINT8                        swizzle;

    /* Shader type for this uniform. Set this at the end of link. */
    gcSHADER_KIND                   shaderKind;

    /* companion to format field to denote vector size,
       value of 0 denote the underlying type is scalar */
    gctINT16                        vectorSize;

    /* offset to typeNameBuffer in gcSHADER at which the name of derived type reside */
    gctCHAR                         typeNameOffset[sizeof(gctINT)];

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcUNIFORM */
    gctINT16                        parent;

    /* Uniform block index:
       Default block index = -1 */
    gctINT16                        blockIndex;

    /* stride on array */
    char                            arrayStride[sizeof(gctINT32)];

    /* offset from uniform block's base address */
    char                            offset[sizeof(gctINT32)];

    /* physical and address. */
    gctINT16                        physical;
    gctINT16                        samplerPhysical;
    char                            address[sizeof(gctUINT32)];

    char                            resOpFlag[sizeof(gctUINT32)];

    /* The uniform arrayLengthList and name. */
    char                            memory[1];
}
* gcBINARY_UNIFORM_EX;


typedef enum _gceOUTPUT_Flag
{
    gcOUTPUT_CONVERTEDTOPHYSICAL        = 0x01,
    gcOUTPUT_ALWAYSUSED                 = 0x02,
    gcOUTPUT_ISNOTUSED                  = 0x04,
    gcOUTPUT_PACKEDAWAY                 = 0x08,

    /* per-vertex array for TCS/TES */
    gcOUTPUT_ISPERVERTEXARRAY           = 0x10,
    /* the output is per-patch output */
    gcOUTPUT_ISPERPATCH                 = 0x20,
    gcOUTPUT_ISARRAY                    = 0x40,
    gcOUTPUT_ISPOSITION                 = 0x80,
    gcOUTPUT_ENABLED                    = 0x100,
    gcOUTPUT_ISINVARIANT                = 0x200,
    gcOUTPUT_ISPRECISE                  = 0x400,
    gcOUTPUT_ISIOBLOCKMEMBER            = 0x800,
    gcOUTPUT_ISINSTANCEMEMBER           = 0x1000,
    gcOUTPUT_ISCENTROID                 = 0x2000,
    gcOUTPUT_ISSAMPLE                   = 0x4000,
    gcOUTPUT_ISPERVERTEXNOTARRAY        = 0x8000,
    gcOUTPUT_STATICALLYUSED             = 0x10000,
} gceOUTPUT_Flag;

#define gcmOUTPUT_convertedToPhysical(out)  (((out)->flags_ & gcOUTPUT_CONVERTEDTOPHYSICAL) != 0)
#define gcmOUTPUT_packedAway(out)           (((out)->flags_ & gcOUTPUT_PACKEDAWAY) != 0)
#define gcmOUTPUT_alwaysUsed(out)           (((out)->flags_ & gcOUTPUT_ALWAYSUSED) != 0)
#define gcmOUTPUT_isNotUsed(out)            (((out)->flags_ & gcOUTPUT_ISNOTUSED) != 0)
#define gcmOUTPUT_isPerPatch(out)           (((out)->flags_ & gcOUTPUT_ISPERPATCH) != 0)
#define gcmOUTPUT_isPerVertexArray(out)     (((out)->flags_ & gcOUTPUT_ISPERVERTEXARRAY) != 0)
#define gcmOUTPUT_isPerVertexNotArray(out)  (((out)->flags_ & gcOUTPUT_ISPERVERTEXNOTARRAY) != 0)
#define gcmOUTPUT_isArray(out)              (((out)->flags_ & gcOUTPUT_ISARRAY) != 0)
#define gcmOUTPUT_isPosition(out)           (((out)->flags_ & gcOUTPUT_ISPOSITION) != 0)
#define gcmOUTPUT_enabled(out)              (((out)->flags_ & gcOUTPUT_ENABLED) != 0)
#define gcmOUTPUT_isInvariant(out)          (((out)->flags_ & gcOUTPUT_ISINVARIANT) != 0)
#define gcmOUTPUT_isPrecise(out)            (((out)->flags_ & gcOUTPUT_ISPRECISE) != 0)
#define gcmOUTPUT_isIOBLockMember(out)      (((out)->flags_ & gcOUTPUT_ISIOBLOCKMEMBER) != 0)
#define gcmOUTPUT_isInstanceMember(out)     (((out)->flags_ & gcOUTPUT_ISINSTANCEMEMBER) != 0)
#define gcmOUTPUT_isCentroid(out)           (((out)->flags_ & gcOUTPUT_ISCENTROID) != 0)
#define gcmOUTPUT_isSample(out)             (((out)->flags_ & gcOUTPUT_ISSAMPLE) != 0)
#define gcmOUTPUT_isStaticallyUsed(out)     (((out)->flags_ & gcOUTPUT_STATICALLYUSED) != 0)

#define gcmOUTPUT_SetConvertedToPhysicaly(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_CONVERTEDTOPHYSICAL) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_CONVERTEDTOPHYSICAL))

#define gcmOUTPUT_SetPackedAway(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_PACKEDAWAY) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_PACKEDAWAY))

#define gcmOUTPUT_SetAlwaysUsed(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ALWAYSUSED) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ALWAYSUSED))

#define gcmOUTPUT_SetNotUsed(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISNOTUSED) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISNOTUSED))

#define gcmOUTPUT_SetIsPerPatch(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISPERPATCH) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISPERPATCH))

#define gcmOUTPUT_SetIsPerVertexArray(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISPERVERTEXARRAY) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISPERVERTEXARRAY))

#define gcmOUTPUT_SetIsPerVertexNotArray(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISPERVERTEXNOTARRAY) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISPERVERTEXNOTARRAY))

#define gcmOUTPUT_SetIsArray(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISARRAY) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISARRAY))

#define gcmOUTPUT_SetIsPosition(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISPOSITION) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISPOSITION))

#define gcmOUTPUT_SetEnabled(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ENABLED) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ENABLED))

#define gcmOUTPUT_SetIsInvariant(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISINVARIANT) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISINVARIANT))

#define gcmOUTPUT_SetIsPrecise(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISPRECISE) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISPRECISE))

#define gcmOUTPUT_SetIsIOBlockMember(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISIOBLOCKMEMBER) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISIOBLOCKMEMBER))

#define gcmOUTPUT_SetIsInstanceMember(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISINSTANCEMEMBER) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISINSTANCEMEMBER))

#define gcmOUTPUT_SetIsCentroid(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISCENTROID) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISCENTROID))

#define gcmOUTPUT_SetIsSample(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_ISSAMPLE) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_ISSAMPLE))

#define gcmOUTPUT_SetIsStaticallyUsed(out,v )  \
        ((out)->flags_ = ((out)->flags_ & ~gcOUTPUT_STATICALLYUSED) | \
                          ((v) == gcvFALSE ? 0 : gcOUTPUT_STATICALLYUSED))

/* Structure that defines an output for a shader. */
struct _gcOUTPUT
{
    /* The object. */
    gcsOBJECT                       object;

    /* Index of the output. */
    gctUINT16                       index;

    /* Original type for this output, driver should use this for transform feedback. */
    gcSHADER_TYPE                   origType;

    /* Type for this output, this type may be changed after varying packing. */
    gcSHADER_TYPE                   type;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    /* Temporary register index that holds the output value. */
    gctUINT32                       tempIndex;

    gctUINT32                       flags_;

    /* Number of array elements for this output. */
    gctINT                          arraySize;

    /* array Index for the output */
    gctINT                          arrayIndex;

    /* Flat output or smooth output. */
    gcSHADER_SHADERMODE             shaderMode;

    /* The stream number, for GS only. The default value is 0. */
    gctINT                          streamNumber;

    /* Location index. */
    gctINT                          location;
    gctINT                          output2RTIndex; /* user may specify location 1,3,
                                                     * real RT index are 1->0, 3->1 */

    /* Field Index. */
    gctINT                          fieldIndex;

    /* IO block index. */
    gctINT                          ioBlockIndex;

    /* IO block array index. */
    gctINT                          ioBlockArrayIndex;

    /* Only used for a IO block member, point to the next element in block. */
    gctINT16                        nextSibling;

    /* Only used for a IO block member, point to the previous element in block. */
    gctINT16                        prevSibling;

    /* The variable index of the type name, it is only enabled for a structure member only. */
    gctINT16                        typeNameVarIndex;

    /* layout qualifier */
    gceLAYOUT_QUALIFIER             layoutQualifier;

    /* Length of the output name. */
    gctINT                          nameLength;

    /* The output name. */
    char                            name[1];
};

#define GetOutputObject(o)                  (&(o)->object)
#define GetOutputIndex(o)                   ((o)->index)
#define GetOutputOrigType(o)                ((o)->origType)
#define GetOutputType(o)                    ((o)->type)
#define GetOutputPrecision(o)               ((o)->precision)
#define GetOutputArraySize(o)               ((o)->arraySize)
#define GetOutputArrayIndex(o)              ((o)->arrayIndex)
#define GetOutputTempIndex(o)               ((o)->tempIndex)
#define GetOutputShaderMode(o)              ((o)->shaderMode)
#define GetOutputStreamNumber(o)            ((o)->streamNumber)
#define SetOutputStreamNumber(o, i)         (GetOutputStreamNumber(o) = (i))
#define GetOutputLocation(o)                ((o)->location)
#define GetOutput2RTIndex(o)                ((o)->output2RTIndex)
#define GetOutputFieldIndex(o)              ((o)->fieldIndex)
#define GetOutputNameLength(o)              ((o)->nameLength)
#define GetOutputName(o)                    ((o)->name)
#define GetOutputIOBlockIndex(o)            ((o)->ioBlockIndex)
#define SetOutputIOBlockIndex(o, i)         (GetOutputIOBlockIndex(o) = (i))
#define GetOutputIOBlockArrayIndex(o)       ((o)->ioBlockArrayIndex)
#define SetOutputIOBlockArrayIndex(o, i)    (GetOutputIOBlockArrayIndex(o) = (i))
#define GetOutputNextSibling(o)             ((o)->nextSibling)
#define SetOutputNextSibling(o, i)          (GetOutputNextSibling(o) = (i))
#define GetOutputPrevSibling(o)             ((o)->prevSibling)
#define SetOutputPrevSibling(o, i)          (GetOutputPrevSibling(o) = (i))
#define GetOutputTypeNameVarIndex(o)        ((o)->typeNameVarIndex)
#define SetOutputTypeNameVarIndex(o, i)     (GetOutputTypeNameVarIndex(o) = (i))

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_OUTPUT
{
    /* Index of the output. */
    gctUINT16                       index;

    /* Original type for this output, driver should use this for transform feedback. */
    gctINT16                        origType;

    /* Type for this output. */
    gctINT16                        type;

    /* Number of array elements for this output. */
    gctINT16                        arraySize;

    /* Temporary register index that holds the output value. */
    gctUINT32                       tempIndex;

    gctUINT16                       flags1;

    gctUINT16                       flags2;

    /* Length of the output name. */
    gctINT16                        nameLength;

    /* precision */
    gctINT16                        precision;

    /* IO block index. */
    gctINT16                        ioBlockIndex;

    /* IO block array index. */
    gctINT16                        ioBlockArrayIndex;

    /* Only used for a IO block member, point to the next element in block. */
    gctINT16                        nextSibling;

    /* Only used for a IO block member, point to the previous element in block. */
    gctINT16                        prevSibling;

    /* The variable index of the type name, it is only enabled for a structure member only. */
    gctINT16                        typeNameVarIndex;

    /* shader mode: flat/smooth/... */
    gctINT16                        shaderMode;

    /* The stream number, for GS only. The default value is 0. */
    gctINT16                        streamNumber;

    /* layout qualifier */
    char                            layoutQualifier[sizeof(gceLAYOUT_QUALIFIER)];

    /* The output name. */
    char                            name[1];
}
* gcBINARY_OUTPUT;

/* NOTE - size of gceVARIABLE_FLAG enum is gctUINT16
   If it goes beyond gctUINT16, flags field in gcBINARY_UNIFORM needs to be adjusted
   and gcSHADER_Load* to be updated accordingly
*/
typedef enum _gceVARIABLE_FLAG
{
    gceVARFLAG_NONE                     = 0x00,
    gceVARFLAG_IS_LOCAL                 = 0x01,
    gceVARFLAG_IS_OUTPUT                = 0x02,
    gceVARFLAG_IS_INPUT                 = 0x04,
    gceVARFLAG_IS_ROW_MAJOR             = 0x08,
    gceVARFLAG_IS_COMPILER_GENERATED    = 0x10,
    gceVARFLAG_IS_NOT_USED              = 0x20,
    gceVARFLAG_IS_STATIC                = 0x40,
    gceVARFLAG_IS_EXTERN                = 0x80,
    gceVARFLAG_IS_POINTER               = 0x100,
    gceVARFLAG_IS_PRECISE               = 0x200,
    gceVARFLAG_IS_STATICALLY_USED       = 0x400,
    gceVARFLAG_IS_PERVERTEX             = 0x800,
    gceVARFLAG_IS_HOST_ENDIAN           = 0x1000,
    /* This variable is a function parameter, but the function is deleted. */
    gceVARFLAG_IS_PARAM_FUNC_DELETE     = 0x2000,
    gceVARFLAG_IS_EXTENDED_VECTOR       = 0x4000,
} gceVARIABLE_FLAG;

/* Structure that defines a variable for a shader. */
struct _gcVARIABLE
{
    /* The object. */
    gcsOBJECT                       object;

    /* Index of the variable. */
    gctUINT16                       index;
    /* If a storage block member is used on both VS and PS,
    ** the index of this variable on the other shader would be saved by this.
    */
    gctINT16                        matchIndex;

    /* Storage block index: Default block index = gcvBLOCK_INDEX_DEFAULT */
    gctINT16                        blockIndex;

    /* Variable category */
    gcSHADER_VAR_CATEGORY           _varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcVARIABLE */
    gctINT16                        parent;

    union
    {
        /* Data type for this most-inner variable. */
        gcSHADER_TYPE               type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                   numStructureElement;

        /* Number of elements in block */
        gctUINT16                   numBlockElement;
    }
    u;

    /* type qualifier */
    gctTYPE_QUALIFIER               qualifier;

    /* Precision of the uniform. */
    gcSHADER_PRECISION              precision;

    gceVARIABLE_FLAG                flags;

    /* Number of array elements for this variable, at least 1 */
    gctINT                          arraySize;

    /* Number of array level. */
    gctINT                          arrayLengthCount;

    /* Array size for every array level. */
    gctINT *                        arrayLengthList;

    /* Start temporary register index that holds the variable value. */
    gctUINT32                       tempIndex;

    /* stride on array or matrix */
    gctINT32                        arrayStride;
    gctINT16                        matrixStride;

    /* Top level array data associated with this variable */
    gctINT                          topLevelArraySize;
    gctINT32                        topLevelArrayStride;

    /* offset from storage block's base address */
    gctINT32                        offset;

    /* Length of the variable name. */
    gctINT                          nameLength;

    /* The variable name. */
    char                            name[1];
};

#define GetVariableObject(v)                    (&(v)->object)
#define GetVariableIndex(v)                     ((v)->index)
#define GetVariableCategory(v)                  ((v)->_varCategory)
#define SetVariableCategory(v, c)               ((v)->_varCategory = (c))
#define GetVariableBlockID(v)                   ((v)->blockIndex)
#define GetVariableArrayStride(v)               ((v)->arrayStride)
#define SetVariableArrayStride(v, s)            ((v)->arrayStride = (s))
#define GetVariableTopLevelArrayStride(v)       ((v)->topLevelArrayStride)
#define SetVariableTopLevelArrayStride(v, s)    ((v)->topLevelArrayStride = (s))
#define GetVariableMatrixStride(v)              ((v)->matrixStride)
#define GetVariableOffset(v)                    ((v)->offset)
#define SetVariableOffset(v, s)                 ((v)->offset = (s))
#define GetVariableFirstChild(v)                ((v)->firstChild)
#define GetVariableNextSibling(v)               ((v)->nextSibling)
#define GetVariablePrevSibling(v)               ((v)->prevSibling)
#define GetVariableParent(v)                    ((v)->parent)
#define GetVariableType(v)                      ((v)->u.type)
#define SetVariableType(v, t)                   (GetVariableType(v) = (t))
#define GetVariableNumStructureElement(v)       ((v)->u.numStructureElement)
#define GetVariableNumBlockElement(v)           ((v)->u.numBlockElement)
#define GetVariableQualifier(v)                 ((v)->qualifier)
#define SetVariableQualifier(v, s)              ((v)->qualifier |= (s))
#define GetVariablePrecision(v)                 ((v)->precision)
#define GetVariableFlags(v)                     ((v)->flags)
#define GetVariableIsLocal(v)                   (((v)->flags & gceVARFLAG_IS_LOCAL) != 0)
#define GetVariableIsOtput(v)                   (((v)->flags & gceVARFLAG_IS_OUTPUT) != 0)
#define GetVariableIsRowMajor(v)                (((v)->flags & gceVARFLAG_IS_ROW_MAJOR) != 0)
#define GetVariableIsCompilerGenerated(v)       (((v)->flags & gceVARFLAG_IS_COMPILER_GENERATED) != 0)
#define GetVariableIsPointer(v)                 (((v)->flags & gceVARFLAG_IS_POINTER) != 0)
#define GetVariableIsPrecise(v)                 (((v)->flags & gceVARFLAG_IS_PRECISE) != 0)
#define GetVariableIsPerVertex(v)               (((v)->flags & gceVARFLAG_IS_PERVERTEX) != 0)
#define GetVariableIsHostEndian(v)              (((v)->flags & gceVARFLAG_IS_HOST_ENDIAN) != 0)
#define GetVariableIsParamFuncDelete(v)         (((v)->flags & gceVARFLAG_IS_PARAM_FUNC_DELETE) != 0)
#define GetVariableIsExtendedVector(v)          (((v)->flags & gceVARFLAG_IS_EXTENDED_VECTOR) != 0)

#define GetVariableArraySize(v)                 ((v)->arraySize)
#define SetVariableArraySize(v, s)              ((v)->arraySize = (s))
#define GetVariableTopLevelArraySize(v)         ((v)->topLevelArraySize)
#define SetVariableTopLevelArraySize(v, s)      ((v)->topLevelArraySize = (s))
#define GetVariableArrayLengthCount(v)          ((v)->arrayLengthCount)
#define GetVariableTempIndex(v)                 ((v)->tempIndex)
#define GetVariableNameLength(v)                ((v)->nameLength)
#define GetVariableName(v)                      ((v)->name)

#define GetVariableKnownArraySize(v)            ((v)->arraySize > 0 ? (v)->arraySize : 1)

#define isVariableArray(v)                      ((v)->arrayLengthCount > 0 || (v)->arraySize == -1)
#define isVariableArraysOfArrays(v)             (GetVariableArrayLengthCount(v) > 1)
#define isVariableFunctionArg(v)                ((v)->_varCategory >= gcSHADER_VAR_CATEGORY_FUNCTION_INPUT_ARGUMENT && (v)->_varCategory <= gcSHADER_VAR_CATEGORY_FUNCTION_INOUT_ARGUMENT)
#define isVariableNormal(v)                     (((v)->_varCategory == gcSHADER_VAR_CATEGORY_NORMAL) || (isVariableFunctionArg(v)))
#define isVariableTopLevelStruct(v)             ((v)->_varCategory == gcSHADER_VAR_CATEGORY_TOP_LEVEL_STRUCT)
#define isVariableStruct(v)                     (isVariableTopLevelStruct(v) || ((v)->_varCategory == gcSHADER_VAR_CATEGORY_STRUCT))
#define isVariableBlockMember(v)                ((v)->_varCategory == gcSHADER_VAR_CATEGORY_BLOCK_MEMBER)
#define isVariableInactive(v)                   (gcvFALSE)
#define isVariableSimple(v)                     (isVariableNormal(v) || isVariableBlockMember(v) )
#define isVariableStructMember(v)               (GetVariableParent(v) != -1)
#define isVariableTypeName(v)                   (GetVariableCategory(v) == gcSHADER_VAR_CATEGORY_TYPE_NAME)

#define SetVariableFlag(v, f)                   do { (v)->flags |= f; } while(0)
#define SetVariableFlags(v, f)                  do { (v)->flags = f; } while(0)
#define SetVariableIsLocal(v)                   do { (v)->flags |= gceVARFLAG_IS_LOCAL; } while(0)
#define SetVariableIsOutput(v)                  do { (v)->flags |= gceVARFLAG_IS_OUTPUT; } while(0)
#define SetVariableIsRowMajor(v)                do { (v)->flags |= gceVARFLAG_IS_ROW_MAJOR; } while(0)
#define SetVariableIsCompilerGenerated(v)       do { (v)->flags |= gceVARFLAG_IS_COMPILER_GENERATED; } while(0)
#define SetVariableIsNotUsed(v)                 do { (v)->flags |= gceVARFLAG_IS_NOT_USED; } while(0)
#define SetVariableIsPointer(v)                 do { (v)->flags |= gceVARFLAG_IS_POINTER; } while(0)
#define SetVariableIsExtern(v)                  do { (v)->flags |= gceVARFLAG_IS_EXTERN; } while(0)
#define SetVariableIsStatic(v)                  do { (v)->flags |= gceVARFLAG_IS_STATIC; } while(0)
#define SetVariableIsPrecise(v)                 do { (v)->flags |= gceVARFLAG_IS_PRECISE; } while(0)
#define SetVariableIsPerVertex(v)               do { (v)->flags |= gceVARFLAG_IS_PERVERTEX; } while(0)
#define SetVariableIsHostEndian(v)              do { (v)->flags |= gceVARFLAG_IS_HOST_ENDIAN; } while(0)
#define SetVariableIsStaticallyUsed(v)          do { (v)->flags |= gceVARFLAG_IS_STATICALLY_USED; } while(0)
#define SetVariableIsParamFuncDelete(v)         do { (v)->flags |= gceVARFLAG_IS_PARAM_FUNC_DELETE; } while(0)

#define IsVariableIsNotUsed(v)                  (((v)->flags & gceVARFLAG_IS_NOT_USED) != 0)
#define IsVariablePointer(v)                    (((v)->flags & gceVARFLAG_IS_POINTER) != 0)
#define IsVariableExtern(v)                     (((v)->flags & gceVARFLAG_IS_EXTERN) != 0)
#define IsVariableStatic(v)                     (((v)->flags & gceVARFLAG_IS_STATIC) != 0)
#define IsVariableLocal(v)                      (((v)->flags & gceVARFLAG_IS_LOCAL) != 0)
#define IsVariableGlobal(v)                     (((v)->flags & (gceVARFLAG_IS_STATIC | gceVARFLAG_IS_LOCAL)) == 0)
#define IsVariableOutput(v)                     (((v)->flags & gceVARFLAG_IS_OUTPUT) != 0)
#define IsVariablePrecise(v)                    (((v)->flags & gceVARFLAG_IS_PRECISE) != 0)
#define IsVariableHostEndian(v)                 (((v)->flags & gceVARFLAG_IS_HOST_ENDIAN) != 0)
#define IsVariableStaticallyUsed(v)             (((v)->flags & gceVARFLAG_IS_STATICALLY_USED) != 0)

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_VARIABLE
{
    union
    {
        /* Data type for this most-inner variable. */
        gctUINT16                     type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                    numStructureElement;
    }
    u;

    /* Number of array elements for this variable, at least 1,
           8 bit wide arraySize is not enough and does not match
           definition in struct _gcVARIABLE. This is a potential
           problem - KLC
        */
    gctINT16                         arraySize;

    gctUINT16                       arrayLengthCount;

    /* Start temporary register index that holds the variable value. */
    gctUINT32                       tempIndex;

    /* Length of the variable name. */
    gctINT16                        nameLength;

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcVARIABLE */
    gctINT16                        parent;

    /* type qualifier is currently 16bit long.
       If it ever changes to more than 16bits, the alignment has to be adjusted
       when writing out to a shader binary
    */
    gctTYPE_QUALIFIER               qualifier;

    /* precision */
    gctINT16                        precision;

    /* flags */
    gctINT16                        flags;

    /* Storage block index: Default block index = -1 */
    gctINT16                        blockIndex;

    /* The variable arrayLengthList and name. */
    char                            memory[1];
}
* gcBINARY_VARIABLE;

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_VARIABLE_EX
{
    union
    {
        /* Data type for this most-inner variable. */
        gctUINT16                     type;

        /* Number of element in structure if arraySize of this */
        /* struct is 1, otherwise, set it to 0 */
        gctUINT16                    numStructureElement;
    }
    u;

    /* Number of array elements for this variable, at least 1,
           8 bit wide arraySize is not enough and does not match
           definition in struct _gcVARIABLE. This is a potential
           problem - KLC
        */
    gctINT16                         arraySize;

    gctUINT16                       arrayLengthCount;

    /* Start temporary register index that holds the variable value. */
    gctUINT32                       tempIndex;

    /* Length of the variable name. */
    gctINT16                        nameLength;

    /* Variable category */
    gctINT16                        varCategory;

    /* Only used for structure, point to either first array element for */
    /* arrayed struct or first struct element if struct is not arrayed */
    gctINT16                        firstChild;

    /* Only used for structure, point to either next array element for */
    /* arrayed struct or next struct element if struct is not arrayed */
    gctINT16                        nextSibling;

    /* Only used for structure, point to either prev array element for */
    /* arrayed struct or prev struct element if struct is not arrayed */
    gctINT16                        prevSibling;

    /* Only used for structure, point to parent _gcVARIABLE */
    gctINT16                        parent;

    /* type qualifier is currently 16bit long.
       If it ever changes to more than 16bits, the alignment has to be adjusted
       when writing out to a shader binary
    */
    gctTYPE_QUALIFIER               qualifier;

    /* precision */
    gctINT16                        precision;

    /* flags */
    gctINT16                        flags;

    /* Storage block index: Default block index = -1 */
    gctINT16                        blockIndex;

    /* stride on array. For scalar, it represents its size in bytes */
    char                            arrayStride[sizeof(gctINT32)];

    /* offset from constant memory base address */
    char                            offset[sizeof(gctINT32)];

    /* The variable arrayLengthList and name. */
    char                            memory[1];
}
* gcBINARY_VARIABLE_EX;

typedef enum _gceFUNCTION_ARGUMENT_FLAG
{
    gceFUNCTION_ARGUMENT_FLAG_NONE                  = 0x00,
    gceFUNCTION_ARGUMENT_FLAG_IS_PRECISE            = 0x01,
    gceFUNCTION_ARGUMENT_FLAG_IS_RETURN_VARIABLE    = 0x02,
} gceFUNCTION_ARGUMENT_FLAG;

typedef struct _gcsFUNCTION_ARGUMENT
{
    gctUINT32                       index;
    gctUINT8                        enable;
    gctUINT8                        qualifier;
    gctUINT8                        precision;
    gctUINT16                       variableIndex;
    gctUINT8                        flags;
}
gcsFUNCTION_ARGUMENT,
* gcsFUNCTION_ARGUMENT_PTR;

#define GetFuncArgIndex(f)                  ((f)->index)
#define GetFuncArgEnable(f)                 ((f)->enable)
#define GetFuncArgQualifier(f)              ((f)->qualifier)

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_ARGUMENT
{
    gctUINT32                       index;
    gctUINT8                        enable;
    gctUINT8                        qualifier;
    gctUINT8                        precision;
    gctUINT8                        flags;
    gctUINT16                       variableIndex;
}
* gcBINARY_ARGUMENT;

typedef enum _gceFUNCTION_FLAG
{
  gcvFUNC_NOATTR                = 0x00,
  gcvFUNC_INTRINSICS            = 0x01, /* Function is openCL/OpenGL builtin function */
  gcvFUNC_ALWAYSINLINE          = 0x02, /* Always inline */
  gcvFUNC_NOINLINE              = 0x04, /* Neve inline */
  gcvFUNC_INLINEHINT            = 0x08, /* Inline is desirable */

  gcvFUNC_READNONE              = 0x10, /* Function does not access memory */
  gcvFUNC_READONLY              = 0x20, /* Function only reads from memory */
  gcvFUNC_STRUCTRET             = 0x40, /* Hidden pointer to structure to return */
  gcvFUNC_NORETURN              = 0x80, /* Function is not returning */

  gcvFUNC_INREG                 = 0x100, /* Force argument to be passed in register */
  gcvFUNC_BYVAL                 = 0x200, /* Pass structure by value */

  gcvFUNC_STATIC                = 0x400, /* OPENCL static function */
  gcvFUNC_EXTERN                = 0x800, /* OPENCL extern function with no body */

  gcvFUNC_NAME_MANGLED          = 0x1000, /* name mangled */

  gcvFUNC_RECOMPILER            = 0x2000, /* A recompile function. */
  gcvFUNC_RECOMPILER_STUB       = 0x4000, /* The function to stub a recompile function. */
  gcvFUNC_HAS_SAMPLER_INDEXINED = 0x8000, /* This function has sampler indexing used. */
  gcvFUNC_PARAM_AS_IMG_SOURCE0  = 0x10000, /* Use parameter as source0 of a IMG op. */
  gcvFUNC_USING_SAMPLER_VIRTUAL = 0x20000, /* Use sampler virtual instructions, like get_sampler_lmm or get_sampler_lbs. */
  gcvFUNC_HAS_TEMPREG_BIGGAP    = 0x40000, /* the function has big gap in temp ergister due to called be inlined first */
  gcvFUNC_HAS_INT64             = 0x80000, /* the function has long/ulong types */
  gcvFUNC_NOT_USED              = 0x100000, /* the function is not used the sahder, do not convert to VIR */
} gceFUNCTION_FLAG;

typedef enum _gceINTRINSICS_KIND
{
    gceINTRIN_NONE         = 0, /* not an intrinsics */
    gceINTRIN_UNKNOWN      = 1, /* is an intrinsics, but unknown kind */

    /* common functions */
    gceINTRIN_clamp,
    gceINTRIN_min,
    gceINTRIN_max,
    gceINTRIN_sign,
    gceINTRIN_fmix,
    gceINTRIN_mix,
    /* Angle and Trigonometry Functions */
    gceINTRIN_radians,
    gceINTRIN_degrees,
    gceINTRIN_step,
    gceINTRIN_smoothstep,
    /* Geometric Functions */
    gceINTRIN_cross,
    gceINTRIN_fast_length,
    gceINTRIN_length,
    gceINTRIN_distance,
    gceINTRIN_dot,
    gceINTRIN_normalize,
    gceINTRIN_fast_normalize,
    gceINTRIN_faceforward,
    gceINTRIN_reflect,
    gceINTRIN_refract,
    /* Vector Relational Functions */
    gceINTRIN_isequal,
    gceINTRIN_isnotequal,
    gceINTRIN_isgreater,
    gceINTRIN_isgreaterequal,
    gceINTRIN_isless,
    gceINTRIN_islessequal,
    gceINTRIN_islessgreater,
    gceINTRIN_isordered,
    gceINTRIN_isunordered,
    gceINTRIN_isfinite,
    gceINTRIN_isnan,
    gceINTRIN_isinf,
    gceINTRIN_isnormal,
    gceINTRIN_signbit,
    gceINTRIN_lgamma,
    gceINTRIN_lgamma_r,
    gceINTRIN_shuffle,
    gceINTRIN_shuffle2,
    gceINTRIN_select,
    gceINTRIN_bitselect,
    gceINTRIN_any,
    gceINTRIN_all,
    /* Matrix Functions */
    gceINTRIN_matrixCompMult,
    /* Async copy and prefetch */
    gceINTRIN_async_work_group_copy,
    gceINTRIN_async_work_group_strided_copy,
    gceINTRIN_wait_group_events,
    gceINTRIN_prefetch,
    /* Atomic Functions */
    gceINTRIN_atomic_add,
    gceINTRIN_atomic_sub,
    gceINTRIN_atomic_inc,
    gceINTRIN_atomic_dec,
    gceINTRIN_atomic_xchg,
    gceINTRIN_atomic_cmpxchg,
    gceINTRIN_atomic_min,
    gceINTRIN_atomic_max,
    gceINTRIN_atomic_or,
    gceINTRIN_atomic_and,
    gceINTRIN_atomic_xor,

    /* work-item functions */
    gceINTRIN_get_global_id,
    gceINTRIN_get_local_id,
    gceINTRIN_get_group_id,
    gceINTRIN_get_work_dim,
    gceINTRIN_get_global_size,
    gceINTRIN_get_local_size,
    gceINTRIN_get_global_offset,
    gceINTRIN_get_num_groups,
    /* synchronization functions */
    gceINTRIN_barrier,
    /* intrinsic builtin functions that are written in source,
       which needs compile/link to the shader */
    gceINTRIN_source,
    gceINTRIN_create_size_for_sampler,
    gceINTRIN_texture_gather,
    gceINTRIN_texture_gather_2DRect,
    gceINTRIN_texture_gather_offset,
    gceINTRIN_texture_gather_offset_2DRect,
    gceINTRIN_texture_gather_offsets,
    gceINTRIN_texture_gather_offsets_2DRect,
    gceINTRIN_texelFetch_for_MSAA,
    gceINTRIN_image_size,
    gceINTRIN_image_load,
    gceINTRIN_image_store,
    gceINTRIN_image_atomic,
    gceINTRIN_MS_interpolate_at_centroid,
    gceINTRIN_MS_interpolate_at_sample,
    gceINTRIN_MS_interpolate_at_offset,

} gceINTRINSICS_KIND;

#define IsIntrinsicsKindCreateSamplerSize(Kind)        ((Kind) == gceINTRIN_create_size_for_sampler)
#define IsIntrinsicsKindTextureGather(Kind)            ((Kind) == gceINTRIN_texture_gather)
#define IsIntrinsicsKindTextureGather2DRect(Kind)      ((Kind) == gceINTRIN_texture_gather_2DRect)
#define IsIntrinsicsKindTextureGatherOffset(Kind)      ((Kind) == gceINTRIN_texture_gather_offset)
#define IsIntrinsicsKindTextureGatherOffset2DRect(Kind)      ((Kind) == gceINTRIN_texture_gather_offset_2DRect)
#define IsIntrinsicsKindTextureGatherOffsets(Kind)     ((Kind) == gceINTRIN_texture_gather_offsets)
#define IsIntrinsicsKindTextureGatherOffsets2DRect(Kind)     ((Kind) == gceINTRIN_texture_gather_offsets_2DRect)
#define IsIntrinsicsKindTexelFetchForMSAA(Kind)        ((Kind) == gceINTRIN_texelFetch_for_MSAA)
#define IsIntrinsicsKindImageSize(Kind)                ((Kind) == gceINTRIN_image_size)
#define IsIntrinsicsKindImageLoad(Kind)                ((Kind) == gceINTRIN_image_load)
#define IsIntrinsicsKindImageStore(Kind)               ((Kind) == gceINTRIN_image_store)
#define IsIntrinsicsKindImageAtomic(Kind)              ((Kind) == gceINTRIN_image_atomic)
#define IsIntrinsicsKindInterpolateAtCentroid(Kind)    ((Kind) == gceINTRIN_MS_interpolate_at_centroid)
#define IsIntrinsicsKindInterpolateAtSample(Kind)      ((Kind) == gceINTRIN_MS_interpolate_at_sample)
#define IsIntrinsicsKindInterpolateAtOffset(Kind)      ((Kind) == gceINTRIN_MS_interpolate_at_offset)
#define IsIntrinsicsKindMSInterpolation(Kind)          ((IsIntrinsicsKindInterpolateAtCentroid(Kind)) ||\
                                                        (IsIntrinsicsKindInterpolateAtSample(Kind)) ||\
                                                        (IsIntrinsicsKindInterpolateAtOffset(Kind)))

#define IsIntrinsicsKindNeedTexSizeOnly(Kind)          ((IsIntrinsicsKindTextureGather(Kind)) ||\
                                                        (IsIntrinsicsKindTextureGather2DRect(Kind)) || \
                                                        (IsIntrinsicsKindTextureGatherOffset(Kind)) ||\
                                                        (IsIntrinsicsKindTextureGatherOffset2DRect(Kind)) ||\
                                                        (IsIntrinsicsKindTextureGatherOffsets(Kind)) ||\
                                                        (IsIntrinsicsKindTextureGatherOffsets2DRect(Kind)) ||\
                                                        (IsIntrinsicsKindTexelFetchForMSAA(Kind)))

struct _gcsFUNCTION
{
    gcsOBJECT                       object;

    gctUINT32                       argumentArrayCount;
    gctUINT32                       argumentCount;
    gcsFUNCTION_ARGUMENT_PTR        arguments;
    /* the number of arguments be packed away by _packingArugments() */
    gctUINT32                       packedAwayArgNo;

    gctUINT32                       label;
    gceFUNCTION_FLAG                flags;
    gceINTRINSICS_KIND              intrinsicsKind;

    /* Local variables. */
    gctUINT32                       localVariableCount;
    gcVARIABLE *                    localVariables;

    /* temp register start index, end index and count */
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexEnd;
    gctUINT32                       tempIndexCount;

    gctUINT                         codeStart;
    gctUINT                         codeCount;

    gctBOOL                         isRecursion;
    gctUINT16                       die;

    gctINT                          nameLength;
    char                            name[1];
};

#define GetFunctionObject(f)                        (&(f)->object)
#define GetFunctionArgumentArrayCount(f)            ((f)->argumentArrayCount)
#define GetFunctionArgumentCount(f)                 ((f)->argumentCount)
#define GetFunctionPackedAwayArgNo(f)               ((f)->packedAwayArgNo)
#define GetFunctionArguments(f)                     ((f)->arguments)
#define GetFunctionLable(f)                         ((f)->label)
#define GetFunctionFlags(f)                         ((f)->flags)
#define SetFunctionFlags(f, s)                      ((f)->flags |= (s))
#define GetFunctionIntrinsicsKind(f)                ((f)->intrinsicsKind)
#define GetFunctionLocalVariableCount(f)            ((f)->localVariableCount)
#define GetFunctionLocalVariables(f)                ((f)->localVariables)
#define GetFunctionTempIndexStart(f)                ((f)->tempIndexStart)
#define GetFunctionTempIndexEnd(f)                  ((f)->tempIndexEnd)
#define GetFunctionTempIndexCount(f)                ((f)->tempIndexCount)
#define GetFunctionCodeStart(f)                     ((f)->codeStart)
#define GetFunctionCodeCount(f)                     ((f)->codeCount)
#define GetFunctionIsRecursion(f)                   ((f)->isRecursion)
#define GetFunctionNameLength(f)                    ((f)->nameLenght)
#define GetFunctionName(f)                          ((f)->name)

#define SetFunctionIntrinsicsKind(f, kind)          (GetFunctionIntrinsicsKind(f) = kind)
#define IsFunctionIntrinsicsBuiltIn(f)              (((gcsOBJECT*) (f))->type == gcvOBJ_FUNCTION \
                                                     ? GetFunctionIntrinsicsKind(f) >= gceINTRIN_source \
                                                     : gcvFALSE)

#define IsFunctionExtern(f)                         (((gcsOBJECT*) (f))->type == gcvOBJ_FUNCTION \
                                                     ? (GetFunctionFlags(f) & gcvFUNC_EXTERN) \
                                                     : (((gcsOBJECT*) (f))->type == gcvOBJ_KERNEL_FUNCTION \
                                                        ? (GetKFunctionFlags(f) & gcvFUNC_EXTERN) \
                                                        : gcvFALSE))

#define IsFunctionRecompiler(f)                     (((f)->flags & gcvFUNC_RECOMPILER) != 0)
#define IsFunctionRecompilerStub(f)                 (((f)->flags & gcvFUNC_RECOMPILER_STUB) != 0)
#define IsFunctionHasSamplerIndexing(f)             (((f)->flags & gcvFUNC_HAS_SAMPLER_INDEXINED) != 0)
#define IsFunctionParamAsImgSource0(f)              (((f)->flags & gcvFUNC_PARAM_AS_IMG_SOURCE0) != 0)
#define IsFunctionUsingSamplerVirtual(f)            (((f)->flags & gcvFUNC_USING_SAMPLER_VIRTUAL) != 0)
#define IsFunctionHasBigGapInTempReg(f)             (((f)->flags & gcvFUNC_HAS_TEMPREG_BIGGAP) != 0)
#define IsFunctionHasInt64(f)                       (((f)->flags & gcvFUNC_HAS_INT64) != 0)
#define IsFunctionNotUsed(f)                        (((f)->flags & gcvFUNC_NOT_USED) != 0)

#define SetFunctionRecompiler(f)                    if (f != gcvNULL) { (f)->flags |= gcvFUNC_RECOMPILER; }
#define SetFunctionRecompilerStub(f)                if (f != gcvNULL) { (f)->flags |= gcvFUNC_RECOMPILER_STUB; }
#define SetFunctionHasSamplerIndexing(f)            if (f != gcvNULL) { (f)->flags |= gcvFUNC_HAS_SAMPLER_INDEXINED; }
#define SetFunctionParamAsImgSource0(f)             if (f != gcvNULL) { (f)->flags |= gcvFUNC_PARAM_AS_IMG_SOURCE0; }
#define SetFunctionUsingSamplerVirtual(f)           if (f != gcvNULL) { (f)->flags |= gcvFUNC_USING_SAMPLER_VIRTUAL; }
#define SetFunctionHasInt64(f)                      if (f != gcvNULL) { (f)->flags |= gcvFUNC_HAS_INT64; }
#define SetFunctionNotUsed(f)                       if (f != gcvNULL) { (f)->flags |= gcvFUNC_NOT_USED; }
#define SetFunctionNotUsed(f)                       if (f != gcvNULL) { (f)->flags |= gcvFUNC_NOT_USED; }

/* Same structure, but inside a binary.
   NOTE: to maintain backward compatibility, new fields must be added after before the name field
   and the existing field orders have to be maintained.  */
typedef struct _gcBINARY_FUNCTION
{
    gctINT16                        argumentCount;
    /* the number of arguments be packed away by _packingArugments() */
    gctUINT16                       packedAwayArgNo;
    gctINT16                        localVariableCount;
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexEnd;
    gctUINT32                       tempIndexCount;
    gctUINT32                       codeStart;
    gctUINT32                       codeCount;

    gctINT16                        label;

    gctINT16                        nameLength;
    char                            flags[sizeof(gctUINT32)];
    char                            intrinsicsKind[sizeof(gctUINT32)];
    gctUINT16                       die;

    char                            name[1];
}
* gcBINARY_FUNCTION;

typedef struct _gcsIMAGE_SAMPLER
{
    /* Kernel function argument # associated with the image passed to the kernel function */
    gctUINT8                        imageNum;

    /* Sampler type either passed in as a kernel function argument which will be an argument #
            or
           defined as a constant variable inside the program which will be an unsigend integer value*/
    gctBOOL                         isConstantSamplerType;

    gctUINT32                       samplerType;
}
gcsIMAGE_SAMPLER,
* gcsIMAGE_SAMPLER_PTR;

#define GetImageSamplerImageNum(i)              ((i)->imageNum)
#define GetImageSamplerIsConstantSamplerType(i) ((i)->isConstantSamplerType)
#define GetImageSamplerType(i)                  ((i)->samplerType)

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_IMAGE_SAMPLER
{
    /* index to uniform array associated with the sampler */
    gctUINT16                       uniformIndex;
    gctBOOL                         isConstantSamplerType;

    /* Kernel function argument # associated with the image passed to the kernel function */
    gctUINT8                        imageNum;

    /* Sampler type either passed in as a kernel function argument which will be an argument #
            or
           defined as a constant variable inside the program which will be an unsigend integer value*/
    gctUINT32                       samplerType;
}
* gcBINARY_IMAGE_SAMPLER;

typedef struct _gcsKERNEL_FUNCTION_PROPERTY
{
    gctINT                          propertyType;
    gctUINT32                       propertySize;
}
gcsKERNEL_FUNCTION_PROPERTY,
* gcsKERNEL_FUNCTION_PROPERTY_PTR;

#define GetKFuncPropType(k)             ((k)->propertyType)
#define GetKFuncPropSize(k)             ((k)->propertySize)

/* Same structure, but inside a binary. */
typedef struct _gcBINARY_KERNEL_FUNCTION_PROPERTY
{
    gctINT                          propertyType;
    gctUINT32                       propertySize;
}
* gcBINARY_KERNEL_FUNCTION_PROPERTY;

struct _gcsKERNEL_FUNCTION
{
    gcsOBJECT                       object;

    /* fields common to gcFUNCTION and gcKERNEL_FUNCTION */
    gctUINT32                       argumentArrayCount;
    gctUINT32                       argumentCount;
    gcsFUNCTION_ARGUMENT_PTR        arguments;
    /* the number of arguments be packed away by _packingArugments() */
    gctUINT32                       dummyPackedAwayArgNo; /* not used by kernel, only make it compatible with function*/

    gctUINT32                       label;
    gceFUNCTION_FLAG                flags;
    gceINTRINSICS_KIND              intrinsicsKind; /* not used in kernel */

    /* Local variables. */
    gctUINT32                       localVariableCount;
    gcVARIABLE *                    localVariables;

    /* temp register start index, end index and count */
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexEnd;
    gctUINT32                       tempIndexCount;

    /*
    ** the codeEnd only includes the kernel function declaration code count.
    ** The codeCount includes the kernel function declaration code count and the kernel function call,
    */
    gctUINT                         codeStart;
    gctUINT                         codeEnd;
    gctUINT                         codeCount;

    gctBOOL                         isRecursion;
    gctBOOL                         isCalledByEntryKernel;   /* kernel function can be called
                                                              * by another kernel, cannot remove it
                                                              * if it is called by main */
    /* kernel specific fields */
    gcSHADER                        shader;

    /* kernel info: */
    /* Local address space size */
    gctUINT32                       localMemorySize;

    /* Uniforms Args */
    gctUINT32                       uniformArgumentArrayCount;
    gctUINT32                       uniformArgumentCount;
    gcUNIFORM *                     uniformArguments;
    gctINT                          samplerIndex;

    /* Image-Sampler associations */
    gctUINT32                       imageSamplerArrayCount;
    gctUINT32                       imageSamplerCount;
    gcsIMAGE_SAMPLER_PTR            imageSamplers;

    /* Kernel function properties */
    gctUINT32                       propertyArrayCount;
    gctUINT32                       propertyCount;
    gcsKERNEL_FUNCTION_PROPERTY_PTR properties;
    gctUINT32                       propertyValueArrayCount;
    gctUINT32                       propertyValueCount;
    gctINT_PTR                      propertyValues;

    gctBOOL                         isMain;     /* this kernel is merged with main() */
    gctUINT16                       die;

    gctINT                          nameLength;
    char                            name[1];
};

#define GetKFunctionObject(k)                       (&(k)->object)
#define GetKFunctionShader(k)                       ((k)->shader)
#define GetKFunctionArgArrayCount(k)                ((k)->argumentArrayCount)
#define GetKFunctionArgCount(k)                     ((k)->argumentCount)
#define GetKFunctionArgs(k)                         ((k)->arguments)
#define GetKFunctionLable(k)                        ((k)->lable)
#define GetKFunctionFlags(k)                        ((k)->flags)
#define SetKFunctionFlags(k, s)                     ((k)->flags |= (s))
#define GetKFunctionLocalMemorySize(k)              ((k)->localMemorySize)
#define GetKFunctionUArgArrayCount(k)               ((k)->uniformArgumentArrayCount)
#define GetKFunctionUArgCount(k)                    ((k)->uniformArgumentCount)
#define GetKFunctionUArgs(k)                        ((k)->uniformArguments)
#define GetKFunctionSamplerIndex(k)                 ((k)->samplerIndex)
#define GetKFunctionISamplerArrayCount(k)           ((k)->imageSamplerArrayCount)
#define GetKFunctionISamplerCount(k)                ((k)->imageSamplerCount)
#define SetKFunctionISamplerCount(k, i)             ((k)->imageSamplerCount = i)
#define GetKFunctionISamplers(k)                    ((k)->imageSamplers)
#define GetKFunctionLocalVarCount(k)                ((k)->localVariableCount)
#define GetKFunctionLocalVars(k)                    ((k)->localVariables)
#define GetKFunctionTempIndexStart(k)               ((k)->tempIndexStart)
#define GetKFunctionTempIndexEnd(k)                 ((k)->tempIndexEnd)
#define GetKFunctionTempIndexCount1(k)              ((k)->tempIndexCount)
#define GetKFunctionPropertyArrayCount(k)           ((k)->propertyArrayCount)
#define GetKFunctionPropertyCount(k)                ((k)->propertyCount)
#define GetKFunctionProperties(k)                   ((k)->properties)
#define GetKFunctionPropertyValueArrayCount(k)      ((k)->propertyValueArrayCount)
#define GetKFunctionPropertyValueCount(k)           ((k)->propertyValueCount)
#define GetKFunctionPropertyValues(k)               ((k)->propertyValues)
#define GetKFunctionCodeStart(k)                    ((k)->codeStart)
#define GetKFunctionCodeCount(k)                    ((k)->codeCount)
#define GetKFunctionCodeEnd(k)                      ((k)->codeEnd)
#define GetKFunctionIsMain(k)                       ((k)->isMain)
#define GetKFunctionNameLength(k)                   ((k)->nameLength)
#define GetKFunctionName(k)                         ((k)->name)


/* Same structure, but inside a binary.
   NOTE: to maintain backward compatibility, new fields must be added after before the name field
   and the existing field orders have to be maintained.  */
typedef struct _gcBINARY_KERNEL_FUNCTION
{
    gctINT16                        argumentCount;
    gctINT16                        label;
    char                            localMemorySize[sizeof(gctUINT32)];
    gctINT16                        uniformArgumentCount;
    gctINT16                        samplerIndex;
    gctINT16                        imageSamplerCount;
    gctINT16                        localVariableCount;
    gctUINT32                       tempIndexStart;
    gctUINT32                       tempIndexEnd;
    gctUINT32                       tempIndexCount;
    gctINT16                        propertyCount;
    gctINT16                        propertyValueCount;

    gctUINT32                       codeStart;
    gctUINT32                       codeCount;
    gctUINT32                       codeEnd;

    gctUINT16                       isMain;
    gctUINT16                       die;

    gctINT16                        nameLength;
    char                            flags[sizeof(gctUINT32)];
    char                            name[1];
}
* gcBINARY_KERNEL_FUNCTION;

/* Index into current instruction. */
typedef enum _gcSHADER_INSTRUCTION_INDEX
{
    gcSHADER_OPCODE,
    gcSHADER_SOURCE0,
    gcSHADER_SOURCE1,
}
gcSHADER_INSTRUCTION_INDEX;

typedef struct _gcSHADER_LINK * gcSHADER_LINK;

/* Structure defining a linked references for a label. */
struct _gcSHADER_LINK
{
    gcSHADER_LINK                   next;
    gctUINT                         referenced;
};

#define GetLinkNext(l)              ((l)->next)
#define GetLinkRef(l)               ((l)->referenced)

typedef struct _gcSHADER_LABEL * gcSHADER_LABEL;

/* Structure defining a label. */
struct _gcSHADER_LABEL
{
    gcSHADER_LABEL                  next;
    gctUINT                         label;
    gctUINT                         defined;
    gcSHADER_LINK                   referenced;
    gcFUNCTION                      function;       /* the function that is corresponding to this label */
};

#define GetLabelNext(l)                 ((l)->next)
#define GetLableID(l)                   ((l)->label)
#define GetLableDefined(l)              ((l)->defined)
#define GetLableRef(l)                  ((l)->referenced)
#define GetLableFunction(l)             ((l)->function)

typedef struct _gcsVarTempRegInfo
{
    gcOUTPUT           varying;
    gctUINT32          streamoutSize;  /* size to write on feedback buffer */
    gctINT             tempRegCount;   /* number of temp register assigned
                                          to this variable */
    gctBOOL            isArray;
    gcSHADER_TYPE *    tempRegTypes;   /* the type for each temp reg */
}
gcsVarTempRegInfo;

typedef struct _gcsTFBVarying
{
    gctSTRING name;
    gctINT    arraySize;
    gctBOOL   isWholeTFBed;
    gctBOOL   isArray;
    gcOUTPUT  output;
    gctBOOL   isBuiltinArray;
    gctINT    builtinArrayIdx;
    gctBOOL   bEndOfInterleavedBuffer;
} gcsTFBVarying;

typedef struct _gcBINARY_TFBVarying
{
    gctUINT16                       outputIndex;
    gctINT16                        arraySize;
    gctINT16                        isWholeTFBed;
    gctINT16                        isArray;
    gctINT16                        isBuiltinArray;
    gctINT16                        builtinArrayIdx;
    gctINT16                        bEndOfInterleavedBuffer;
    gctINT16                        nameLength;
    char                            name[1];
}
* gcBINARY_TFBVarying;

typedef struct _gcsTRANSFORM_FEEDBACK
{
    gctUINT32                       varyingCount;
    /* pointer to varyings to be streamed out */
    gcsTFBVarying *                 varyings;

    gceFEEDBACK_BUFFER_MODE         bufferMode;
    /* driver set to 1 if transform feedback is active and not
       paused, 0 if inactive or paused */
    gcUNIFORM                       stateUniform;
    /* the temp register info for each varying */
    gcsVarTempRegInfo *             varRegInfos;
    union {
        /* array of uniform for separate transform feedback buffers */
        gcUNIFORM *                 separateBufUniforms;
        /* transfom feedback buffer for interleaved mode */
        gcUNIFORM                   interleavedBufUniform;
    } feedbackBuffer;
    gctINT                          shaderTempCount;
    /* total size to write to interleaved buffer for one vertex */
    gctUINT32                       totalSize;
}
gcsTRANSFORM_FEEDBACK;

#define GetFeedbackVaryingCount(f)                  ((f)->varyingCount)
#define GetFeedbackVaryings(f)                      ((f)->varyings)
#define GetFeedbackBufferMode(f)                    ((f)->bufferMode)
#define GetFeedbackStateUniform(f)                  ((f)->stateUniform)
#define GetFeedbackVarRegInfos(f)                   ((f)->varRegInfos)
#define GetFeedbackSeparateBufUniforms(f)           ((f)->feedbackBuffer.separateBufUniforms)
#define GetFeedbackInterleavedBufUniform(f)         ((f)->feedbackBuffer.interleavedBufUniform)
#define GetFeedbackShaderTempCount(f)               ((f)->shaderTempCount)
#define GetFeedbackTotalSize(f)                     ((f)->totalSize)

typedef enum _gcSHADER_FLAGS
{
    gcSHADER_FLAG_OLDHEADER                 = 0x01, /* the old header word 5 is gcdSL_IR_VERSION,
                                                         which always be 0x01 */
    gcSHADER_FLAG_HWREG_ALLOCATED           = 0x02, /* the shader is HW Register allocated
                                                         no need to geneated MOVA implicitly */
    gcSHADER_FLAG_CONST_HWREG_ALLOCATED     = 0x04, /* the shader is HW const Register allocated */
    gcSHADER_FLAG_HAS_UNSIZED_SBO           = 0x08, /* the shader has unsized array of Storage Buffer Object */
    gcSHADER_FLAG_HAS_VERTEXID_VAR          = 0x10, /* the shader has vertexId variable */
    gcSHADER_FLAG_HAS_INSTANCEID_VAR        = 0x20, /* the shader has instanceId variable */
    gcSHADER_FLAG_HAS_INTRINSIC_BUILTIN     = 0x40, /* the shader has intrinsic builtins */
    gcSHADER_FLAG_HAS_EXTERN_FUNCTION       = 0x80, /* the shader has extern functions */
    gcSHADER_FLAG_HAS_EXTERN_VARIABLE       = 0x100, /* the shader has extern variables */
    gcSHADER_FLAG_NEED_PATCH_FOR_CENTROID   = 0x200, /* the shader uses centroid varyings as
                                                         the argument of interpolate functions */
    gcSHADER_FLAG_HAS_BASE_MEMORY_ADDR      = 0x400, /* the shader has base memory address, for CL only. */
    gcSHADER_FLAG_HAS_LOCAL_MEMORY_ADDR     = 0x800, /* the shader has local memory address. */
    gcSHADER_FLAG_HAS_INT64                 = 0x1000, /* the shader has 64 bit integer data and operation. */
    gcSHADER_FLAG_HAS_IMAGE_QUERY           = 0x2000, /* the shader has image query */
    gcSHADER_FLAG_HAS_VIV_VX_EXTENSION      = 0x4000, /* the shader has Vivante VX extension */
    gcSHADER_FLAG_USE_LOCAL_MEM             = 0x8000, /* the shader use local memory */
    gcSHADER_FLAG_VP_TWO_SIDE_ENABLE        = 0x10000, /* the shader use two side, for GL fragment shader only. */
    gcSHADER_FLAG_CLAMP_OUTPUT_COLOR        = 0x20000, /* Clamp the output color, for GL shader only. */
    gcSHADER_FLAG_HAS_INT64_PATCH           = 0x40000, /* the shader has 64 bit integer data and operation patch (recompile). */
    gcSHADER_FLAG_AFTER_LINK                = 0x80000, /* the shader is linked(gcLinkProgram/gcLinkShaders/gcLinkKernel). */
    gcSHADER_FLAG_LOADED_KERNEL             = 0x100000, /* shader has loaded a specified kernel function, for OCL shader only. */
    gcSHADER_FLAG_FORCE_ALL_OUTPUT_INVARIANT= 0x200000, /* Force all outputs to be invariant. */
    gcSHADER_FLAG_CONSTANT_MEMORY_REFERENCED= 0x400000, /* constant memory reference in the shader (library) through linking. */
    gcSHADER_FLAG_HAS_DEFINE_MAIN_FUNC      = 0x800000, /* Whether the shader defines a main function, for GL shader only. */
    gcSHADER_FLAG_ENABLE_MULTI_GPU          = 0x1000000, /* whether enable multi-GPU. */
    gcSHADER_FLAG_HAS_VIV_GCSL_DRIVER_IMAGE = 0x2000000, /* the shader has OCL option `-cl-viv-gcsl-driver-image */
    gcSHADER_FLAG_GENERATED_OFFLINE_COMPILER= 0x4000000, /* whether enable offline compile. */
    gcSHADER_FLAG_COMPATIBILITY_PROFILE     = 0x8000000, /* the shader version is compatibility profile for OGL.*/
    gcSHADER_FLAG_USE_CONST_REG_FOR_UBO     = 0x10000000, /* Use constant register to save the UBO.*/
    gcSHADER_FLAG_HAS_CL_KHR_FP16_EXTENSION = 0x20000000, /* the shader has OCL extension cl-khr-fp16 enabled */
} gcSHADER_FLAGS;

#define gcShaderIsOldHeader(Shader)             (((Shader)->flags & gcSHADER_FLAG_OLDHEADER) != 0)
#define gcShaderHwRegAllocated(Shader)          ((Shader)->flags & gcSHADER_FLAG_HWREG_ALLOCATED)
#define gcShaderConstHwRegAllocated(Shader)     ((Shader)->flags & gcSHADER_FLAG_CONST_HWREG_ALLOCATED)
#define gcShaderHasUnsizedSBO(Shader)           (((Shader)->flags & gcSHADER_FLAG_HAS_UNSIZED_SBO) != 0)
#define gcShaderHasVertexIdVar(Shader)          (((Shader)->flags & gcSHADER_FLAG_HAS_VERTEXID_VAR) != 0)
#define gcShaderHasInstanceIdVar(Shader)        (((Shader)->flags & gcSHADER_FLAG_HAS_INSTANCEID_VAR) != 0)
#define gcShaderHasIntrinsicBuiltin(Shader)     (((Shader)->flags & gcSHADER_FLAG_HAS_INTRINSIC_BUILTIN) != 0)
#define gcShaderHasExternFunction(Shader)       (((Shader)->flags & gcSHADER_FLAG_HAS_EXTERN_FUNCTION) != 0)
#define gcShaderHasExternVariable(Shader)       (((Shader)->flags & gcSHADER_FLAG_HAS_EXTERN_VARIABLE) != 0)
#define gcShaderNeedPatchForCentroid(Shader)    (((Shader)->flags & gcSHADER_FLAG_NEED_PATCH_FOR_CENTROID) != 0)
#define gcShaderHasBaseMemoryAddr(Shader)       (((Shader)->flags & gcSHADER_FLAG_HAS_BASE_MEMORY_ADDR) != 0)
#define gcShaderHasLocalMemoryAddr(Shader)      (((Shader)->flags & gcSHADER_FLAG_HAS_LOCAL_MEMORY_ADDR) != 0)
#define gcShaderHasInt64(Shader)                (((Shader)->flags & gcSHADER_FLAG_HAS_INT64) != 0)
#define gcShaderHasInt64Patch(Shader)           (((Shader)->flags & gcSHADER_FLAG_HAS_INT64_PATCH) != 0)
#define gcShaderHasImageQuery(Shader)           (((Shader)->flags & gcSHADER_FLAG_HAS_IMAGE_QUERY) != 0)
#define gcShaderHasVivVxExtension(Shader)       (((Shader)->flags & gcSHADER_FLAG_HAS_VIV_VX_EXTENSION) != 0)
#define gcShaderHasVivGcslDriverImage(Shader)   (((Shader)->flags & gcSHADER_FLAG_HAS_VIV_GCSL_DRIVER_IMAGE) != 0)
#define gcShaderHasClKhrFp16Extension(Shader)   (((Shader)->flags & gcSHADER_FLAG_HAS_CL_KHR_FP16_EXTENSION) != 0)
#define gcShaderUseLocalMem(Shader)             (((Shader)->flags & gcSHADER_FLAG_USE_LOCAL_MEM) != 0)
#define gcShaderVPTwoSideEnable(Shader)         (((Shader)->flags & gcSHADER_FLAG_VP_TWO_SIDE_ENABLE) != 0)
#define gcShaderClampOutputColor(Shader)        (((Shader)->flags & gcSHADER_FLAG_CLAMP_OUTPUT_COLOR) != 0)
#define gcShaderAfterLink(Shader)               (((Shader)->flags & gcSHADER_FLAG_AFTER_LINK) != 0)
#define gcShaderHasLoadedKernel(Shader)         (((Shader)->flags & gcSHADER_FLAG_LOADED_KERNEL) != 0)
#define gcShaderForceAllOutputInvariant(Shader) (((Shader)->flags & gcSHADER_FLAG_FORCE_ALL_OUTPUT_INVARIANT) != 0)
#define gcShaderConstantMemoryReferenced(Shader) (((Shader)->flags & gcSHADER_FLAG_CONSTANT_MEMORY_REFERENCED) != 0)
#define gcShaderHasDefineMainFunc(Shader)       (((Shader)->flags & gcSHADER_FLAG_HAS_DEFINE_MAIN_FUNC) != 0)
#define gcShaderEnableMultiGPU(Shader)          (((Shader)->flags & gcSHADER_FLAG_ENABLE_MULTI_GPU) != 0)
#define gcShaderEnableOfflineCompiler(Shader)   (((Shader)->flags & gcSHADER_FLAG_GENERATED_OFFLINE_COMPILER) != 0)
#define gcShaderIsCompatibilityProfile(Shader)  (((Shader)->flags & gcSHADER_FLAG_COMPATIBILITY_PROFILE) != 0)
#define gcShaderUseConstRegForUBO(Shader)       (((Shader)->flags & gcSHADER_FLAG_USE_CONST_REG_FOR_UBO) != 0)

#define gcShaderGetFlag(Shader)                 (Shader)->flags)

#define gcShaderSetIsOldHeader(Shader)          do { (Shader)->flags |= gcSHADER_FLAG_OLDHEADER; } while (0)
#define gcShaderSetHwRegAllocated(Shader)       do { (Shader)->flags |= gcSHADER_FLAG_HWREG_ALLOCATED; } while (0)
#define gcShaderSetConstHwRegAllocated(Shader)  do { (Shader)->flags |= gcSHADER_FLAG_CONST_HWREG_ALLOCATED; } while (0)
#define gcShaderSetHasUnsizedSBO(Shader)        do { (Shader)->flags |= gcSHADER_FLAG_HAS_UNSIZED_SBO; } while (0)
#define gcShaderSetHasVertexIdVar(Shader)       do { (Shader)->flags |= gcSHADER_FLAG_HAS_VERTEXID_VAR; } while (0)
#define gcShaderSetHasInstanceIdVar(Shader)     do { (Shader)->flags |= gcSHADER_FLAG_HAS_INSTANCEID_VAR; } while (0)
#define gcShaderSetHasIntrinsicBuiltin(Shader)  do { (Shader)->flags |= gcSHADER_FLAG_HAS_INTRINSIC_BUILTIN; } while (0)
#define gcShaderClrHasIntrinsicBuiltin(Shader)  do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_INTRINSIC_BUILTIN; } while (0)
#define gcShaderSetHasExternFunction(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_EXTERN_FUNCTION; } while (0)
#define gcShaderClrHasExternFunction(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_EXTERN_FUNCTION; } while (0)
#define gcShaderSetHasExternVariable(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_EXTERN_VARIABLE; } while (0)
#define gcShaderClrHasExternVariable(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_EXTERN_VARIABLE; } while (0)
#define gcShaderSetNeedPatchForCentroid(Shader) do { (Shader)->flags |= gcSHADER_FLAG_NEED_PATCH_FOR_CENTROID; } while (0)
#define gcShaderClrNeedPatchForCentroid(Shader) do { (Shader)->flags &= ~gcSHADER_FLAG_NEED_PATCH_FOR_CENTROID; } while (0)
#define gcShaderSetHasBaseMemoryAddr(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_BASE_MEMORY_ADDR; } while (0)
#define gcShaderClrHasBaseMemoryAddr(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_BASE_MEMORY_ADDR; } while (0)
#define gcShaderSetHasLocalMemoryAddr(Shader)   do { (Shader)->flags |= gcSHADER_FLAG_HAS_LOCAL_MEMORY_ADDR; } while (0)
#define gcShaderClrHasLocalMemoryAddr(Shader)   do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_LOCAL_MEMORY_ADDR; } while (0)
#define gcShaderSetHasInt64(Shader)             do { (Shader)->flags |= gcSHADER_FLAG_HAS_INT64; } while (0)
#define gcShaderClrHasInt64(Shader)             do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_INT64; } while (0)
#define gcShaderSetHasInt64Patch(Shader)        do { (Shader)->flags |= gcSHADER_FLAG_HAS_INT64_PATCH; } while (0)
#define gcShaderClrHasInt64Patch(Shader)        do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_INT64_PATCH; } while (0)
#define gcShaderSetHasImageQuery(Shader)        do { (Shader)->flags |= gcSHADER_FLAG_HAS_IMAGE_QUERY; } while (0)
#define gcShaderClrHasImageQuery(Shader)        do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_IMAGE_QUERY; } while (0)
#define gcShaderSetHasVivVxExtension(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_VIV_VX_EXTENSION; } while (0)
#define gcShaderClrHasVivVxExtension(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_VIV_VX_EXTENSION; } while (0)
#define gcShaderSetHasVivGcslDriverImage(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_VIV_GCSL_DRIVER_IMAGE; } while (0)
#define gcShaderClrHasVivGcslDriverImage(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_VIV_GCSL_DRIVER_IMAGE; } while (0)
#define gcShaderSetHasClKhrFp16Extension(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_CL_KHR_FP16_EXTENSION; } while (0)
#define gcShaderClrHasClKhrFp16Extension(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_CL_KHR_FP16_EXTENSION; } while (0)
#define gcShaderSetVPTwoSideEnable(Shader)      do { (Shader)->flags |= gcSHADER_FLAG_VP_TWO_SIDE_ENABLE; } while (0)
#define gcShaderClrVPTwoSideEnable(Shader)      do { (Shader)->flags &= ~gcSHADER_FLAG_VP_TWO_SIDE_ENABLE; } while (0)
#define gcShaderSetClampOutputColor(Shader)     do { (Shader)->flags |= gcSHADER_FLAG_CLAMP_OUTPUT_COLOR; } while (0)
#define gcShaderClrClampOutputColor(Shader)     do { (Shader)->flags &= ~gcSHADER_FLAG_CLAMP_OUTPUT_COLOR; } while (0)
#define gcShaderSetAfterLink(Shader)            do { (Shader)->flags |= gcSHADER_FLAG_AFTER_LINK; } while (0)
#define gcShaderClrAfterLink(Shader)            do { (Shader)->flags &= ~gcSHADER_FLAG_AFTER_LINK; } while (0)
#define gcShaderSetLoadedKernel(Shader)         do { (Shader)->flags |= gcSHADER_FLAG_LOADED_KERNEL; } while (0)
#define gcShaderClrLoadedKernel(Shader)         do { (Shader)->flags &= ~gcSHADER_FLAG_LOADED_KERNEL; } while (0)
#define gcShaderSetAllOutputInvariant(Shader)   do { (Shader)->flags |= gcSHADER_FLAG_FORCE_ALL_OUTPUT_INVARIANT; } while (0)
#define gcShaderClrAllOutputInvariant(Shader)   do { (Shader)->flags &= ~gcSHADER_FLAG_FORCE_ALL_OUTPUT_INVARIANT; } while (0)
#define gcShaderSetConstantMemoryReferenced(Shader)   do { (Shader)->flags |= gcSHADER_FLAG_CONSTANT_MEMORY_REFERENCED; } while (0)
#define gcShaderClrConstantMemoryReferenced(Shader)   do { (Shader)->flags &= ~gcSHADER_FLAG_CONSTANT_MEMORY_REFERENCED; } while (0)
#define gcShaderSetHasDefineMainFunc(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_HAS_DEFINE_MAIN_FUNC; } while (0)
#define gcShaderClrHasDefineMainFunc(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_HAS_DEFINE_MAIN_FUNC; } while (0)
#define gcShaderSetEnableMultiGPU(Shader)       do { (Shader)->flags |= gcSHADER_FLAG_ENABLE_MULTI_GPU; } while (0)
#define gcShaderClrEnableMultiGPU(Shader)       do { (Shader)->flags &= ~gcSHADER_FLAG_ENABLE_MULTI_GPU; } while (0)
#define gcShaderSetEnableOfflineCompiler(Shader)do { (Shader)->flags |= gcSHADER_FLAG_GENERATED_OFFLINE_COMPILER; } while (0)
#define gcShaderClrEnableOfflineCompiler(Shader)do { (Shader)->flags &= ~gcSHADER_FLAG_GENERATED_OFFLINE_COMPILER; } while (0)
#define gcShaderSetIsCompatibilityProfile(Shader)     do { (Shader)->flags |= gcSHADER_FLAG_COMPATIBILITY_PROFILE; } while (0)
#define gcShaderClrIsCompatibilityProfile(Shader)     do { (Shader)->flags &= ~gcSHADER_FLAG_COMPATIBILITY_PROFILE; } while (0)
#define gcShaderSetUseConstRegForUBO(Shader)    do { (Shader)->flags |= gcSHADER_FLAG_USE_CONST_REG_FOR_UBO; } while (0)
#define gcShaderClrUseConstRegForUBO(Shader)    do { (Shader)->flags &= ~gcSHADER_FLAG_USE_CONST_REG_FOR_UBO; } while (0)
#define gcShaderSetFlag(Shader, Flag)           do { (Shader)->flags = (Flag); } while (0)

typedef struct _gcLibraryList gcLibraryList;

struct _gcLibraryList
{
    gcSHADER        lib;
    /* temp register mapping table */
    gctUINT32       tempMappingTableEntries;
    gctUINT32 *     tempMappingTable;
    /* uniform mapping table */
    gctUINT32       uniformMappingTableEntries;
    gctUINT16 *     uniformMappingTable;

    gcLibraryList * next;
};

#define gcMAX_SHADERS_IN_LINK_GOURP 6
#define gcMAX_SHADERS_IN_PROGRAM    6

typedef enum _gcsSHADER_GROUP_SHADER_KIND
{
    gceSGSK_VERTEX_SHADER       = 0,
    gceSGSK_CL_SHADER           = 1, /* openCL shader */
    gceSGSK_COMPUTE_SHADER      = 1, /* OGL compute shader */
    gceSGSK_TC_SHADER           = 2, /* Tessellation control shader */
    gceSGSK_TE_SHADER           = 3, /* Tessellation evaluation shader */
    gceSGSK_GEOMETRY_SHADER     = 4,
    gceSGSK_FRAGMENT_SHADER     = 5
} gcsSHADER_GROUP_SHADER_KIND;

typedef struct _gcsSHADERGROUP
{
    gcSHADER    shaderGroup[gcMAX_SHADERS_IN_LINK_GOURP];
    gctBOOL     validGroup;
    gctBOOL     hasComputeOrCLShader;
} gcsShaderGroup;

typedef enum _gcTESSPRIMITIVEMODE
{
    gcTESS_PMODE_TRIANGLE = 0,
    gcTESS_PMODE_QUAD,
    gcTESS_PMODE_ISOLINE
} gcTessPrimitiveMode;

typedef enum _gcTESSVERTEXSPACING
{
    gcTESS_SPACING_EQUAL = 0, /* equal_spacing */
    gcTESS_SPACING_EVEN, /* fractional_even_spacing */
    gcTESS_SPACING_ODD            /* fractional_odd_spacing */
} gcTessVertexSpacing;

typedef enum _gcTESSORDERING
{
    gcTESS_ORDER_CCW = 0, /* Counter Clockwise */
    gcTESS_ORDER_CW               /* Clockwise */
} gcTessOrdering;

typedef enum _gcGEOPRIMITIVE
{
    gcGEO_POINTS = 0,
    gcGEO_LINES,
    gcGEO_LINES_ADJACENCY,
    gcGEO_TRIANGLES,
    gcGEO_TRIANGLES_ADJACENCY,
    gcGEO_LINE_STRIP,
    gcGEO_TRIANGLE_STRIP,
} gcGeoPrimitive;

typedef struct _gcCOMPUTELAYOUT
{
    /* Compute shader layout qualifiers */
    gctUINT32           workGroupSize[3];  /* local group size in the first(0), second(1), and
                                               third(2) dimension */
    /* Is WorkGroupSize fixed? If no, compiler can adjust it. */
    gctBOOL             isWorkGroupSizeFixed;

    /* If WorkGroupSize has been adjusted. */
    gctBOOL             isWorkGroupSizeAdjusted;

    /* Default workGroupSize. */
    gctUINT32           adjustedWorkGroupSize;

    /* The factor of reducing WorkGroupSize, the default value is 0. */
    gctUINT16           workGroupSizeFactor[3];
} gcComputeLayout;

typedef struct _gcTCSLAYOUT
{
    /* Tesselation Control Shader layout */
    gctINT                 tcsPatchOutputVertices;
    gcUNIFORM              tcsInputVerticesUniform;
    gctINT                 tcsPatchInputVertices;
} gcTCSLayout;

typedef struct _gcTESLAYOUT
{
    /* Tessellation Evaluation Shader layout qualifiers*/
    gcTessPrimitiveMode   tessPrimitiveMode;
    gcTessVertexSpacing   tessVertexSpacing;
    gcTessOrdering        tessOrdering;
    gctBOOL               tessPointMode;
    gctINT                tessPatchInputVertices;
} gcTESLayout;

typedef struct _gcGEOLAYOUT
{
    /*  Geometry Shader layout */
    /* times of geometry shader executable is invoked for each input primitive received */
    gctINT                geoInvocations;
    /* the maximum number of vertices the shader will ever emit in a single invocation */
    gctINT                geoMaxVertices;
    /* type of input primitive accepted by geometry shader */
    gcGeoPrimitive        geoInPrimitive;
    /* type of output primitive accepted by geometry shader */
    gcGeoPrimitive        geoOutPrimitive;
} gcGEOLayout;

/* same value as cl_program_binary_type. */
typedef enum _gcCL_PROGRAM_BINARY_TYPE
{
    gcCL_PROGRAM_BINARY_TYPE_NONE                   = 0x0,
    gcCL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT        = 0x1,
    gcCL_PROGRAM_BINARY_TYPE_LIBRARY                = 0x2,
    gcCL_PROGRAM_BINARY_TYPE_EXECUTABLE             = 0x4,
} gcCL_PROGRAM_BINARY_TYPE;

/* The structure that defines the gcSHADER object to the outside world. */
struct _gcSHADER
{
    /* The base object. */
    gcsOBJECT                   object;
    gceAPI                      clientApiVersion;  /* client API version. */
    gctUINT                     _id;                /* unique id used for triage */
    gctUINT                     _stringId;        /* unique id generated by shader source. */
    gctUINT                     _constVectorId;     /* unique constant uniform vector id */
    gctUINT                     _dummyUniformCount;
    gctUINT                     _tempRegCount;      /* the temp register count of the shader */
    gctUINT                     _maxLocalTempRegCount; /* the maximum # of temp register set aside to local use in OCL */

    /* Flag whether enable default UBO.*/
    gctBOOL                     enableDefaultUBO;
    gctINT                      _defaultUniformBlockIndex; /* Default uniform block index */

    /* Index of the uniform block that hold the const uniforms generated by compiler.*/
    gctINT                      constUniformBlockIndex;

    /* The size of the uniform block that hold the const uniforms. */
    gctINT                      constUBOSize;

    /* A memory buffer to store constant uniform. */
    gctUINT32 *                 constUBOData;

    /* Frontend compiler version */
    gctUINT32                   compilerVersion[2];

    /* Type of shader. */
    gcSHADER_KIND               type;

    /* Save the cl_program_binary_type which is set by OCL driver. */
    gcCL_PROGRAM_BINARY_TYPE    clProgramBinaryType;

    /* Flags */
    gcSHADER_FLAGS              flags;

    /* Maximum of kernel function arguments, used to calculation the starting uniform index */
    gctUINT32                   maxKernelFunctionArgs;

    /* Global uniform count, used for cl kernel patching. */
    gctUINT32                   globalUniformCount;

    /* Constant memory address space size for openCL */
    gctUINT32                   constantMemorySize;
    gctCHAR    *                constantMemoryBuffer;

    /* Private memory address space size for openCL */
    gctUINT32                   privateMemorySize;

    /* Local memory address space size for openCL, inherited from chosen kernel function */
    gctUINT32                   localMemorySize;

    /* buffer to keep non basic type names for openCL */
    gctUINT32                   typeNameBufferSize;
    gctCHAR    *                typeNameBuffer;

    /* Attributes. */
    gctUINT32                   attributeArraySize;
    gctUINT32                   attributeCount;
    gcATTRIBUTE *               attributes;

    gctUINT32                   builtinAttributeCount;
    gcATTRIBUTE                 builtinAttributes[2];

    /* Uniforms. */
    gctUINT32                   uniformArraySize;
    gctUINT32                   uniformCount;
    gctUINT32                   uniformVectorCount;  /* the vector count of all uniforms */
    gcUNIFORM *                 uniforms;

    gctINT                      samplerIndex;

    /* HALTI extras: Uniform block. */
    gctUINT32                   uniformBlockArraySize;
    gctUINT32                   uniformBlockCount;
    gcsUNIFORM_BLOCK *          uniformBlocks;

    /* HALTI extras: input buffer locations. */
    gctUINT32                   inputLocationArraySize;
    gctUINT32                   inputLocationCount;
    gctINT *                    inputLocations;

    /* HALTI extras: input buffer locations. */
    gctUINT32                   outputLocationArraySize;
    gctUINT32                   outputLocationCount;
    gctINT *                    outputLocations;

    /* ES30 extras: default block uniform locations. */
    gctUINT32                   uniformLocationArraySize;
    gctUINT32                   uniformLocationCount;
    gctINT *                    uniformLocations;

    /* Outputs. */
    gctUINT32                   outputArraySize;    /* the size of 'outputs' be allocated */
    gctUINT32                   outputCount;        /* the output current be added, each
                                                       item in an array count as one output */
    gcOUTPUT *                  outputs;            /* pointer to the output array */

    /* Global variables. */
    gctUINT32                   variableArraySize;
    gctUINT32                   variableCount;
    gcVARIABLE *                variables;

    /* ES 3.1 : storage block. */
    gctUINT32                   storageBlockArraySize;
    gctUINT32                   storageBlockCount;
    gcsSTORAGE_BLOCK *          storageBlocks;

    gctBOOL                     enableDefaultStorageBlock;
    gctUINT16                   _defaultStorageBlock;

    /* ES 3.1+ : io block. */
    gctUINT32                   ioBlockArraySize;
    gctUINT32                   ioBlockCount;
    gcsIO_BLOCK *               ioBlocks;

    /* Functions. */
    gctUINT32                   functionArraySize;
    gctUINT32                   functionCount;
    gcFUNCTION *                functions;
    gcFUNCTION                  currentFunction;

    /* Kernel Functions. */
    gctUINT32                   kernelFunctionArraySize;
    gctUINT32                   kernelFunctionCount;
    gcKERNEL_FUNCTION *         kernelFunctions;
    gcKERNEL_FUNCTION           currentKernelFunction;

    union {
        gcComputeLayout   compute;
        gcTCSLayout       tcs;
        gcTESLayout       tes;
        gcGEOLayout       geo;
    } shaderLayout;

    /* Code. */
    gctUINT32                   codeCount;
    gctUINT                     lastInstruction;
    gcSHADER_INSTRUCTION_INDEX  instrIndex;
    gcSHADER_LABEL              labels;
    gcSL_INSTRUCTION            code;

    gctINT *                    loadUsers;

    /* load-time optimization uniforms */
    gctINT                      ltcUniformCount;      /* load-time constant uniform count */
    gctUINT                     ltcUniformBegin;      /* the begin offset of ltc in uniforms */
    gcSHADER_LIST               ltcCodeUniformMappingList; /* Map code index to the uniform index. */
    gctUINT                     ltcInstructionCount;  /* the total instruction count of the LTC expressions */
    gcSL_INSTRUCTION            ltcExpressions;       /* the expression array for ltc uniforms, which is a list of instructions */

    PLTCValue                   ltcUniformValues;  /* Save the LTC uniform values that can be evaluated within link time. */

#if gcdUSE_WCLIP_PATCH
    gctBOOL                     vsPositionZDependsOnW;    /* for wClip */
    gcSHADER_LIST               wClipTempIndexList;
    gcSHADER_LIST               wClipUniformIndexList;
#endif

    /* Optimization option. */
    gctUINT                     optimizationOption;

    /* Transform feedback varyings */
    gcsTRANSFORM_FEEDBACK       transformFeedback;

    /* Source code string */
    gctUINT                     sourceLength;            /* including terminating '\0' */
    gctSTRING                   source;

    /* a list of library linked to this shader */
    gcLibraryList *             libraryList;
#if gcdSHADER_SRC_BY_MACHINECODE
    /* It is used to do high level GLSL shader detection, and give a BE a replacement index hint to replace
       automatically compiled machine code with manually written extremely optimized machine code. So it is
       a dynamic info based on driver client code, and supposedly it is not related to gcSL structure. So DO
       NOT CONSIDER IT IN LOAD/SAVE ROUTINES. Putting this info in gcSL structure because we don't want to
       break existed prototype of gcLinkShaders. If later somebody find another better place to pass this info
       into gcLinkShaders, feel free to change it */
    gctUINT32                   replaceIndex;
#endif

    gctBOOL                     isDual16Shader;
    /* for recompile only - whether the recompile's master shader is dual16 or not,
       if yes, we need to force recompile shader to be dual16 if possible */
    gctBOOL                     isMasterDual16Shader;

    gctUINT32                   RARegWaterMark;
    gctUINT32                   RATempReg;
    gctUINT32                   RAHighestPriority;

    /* Disable EarlyZ for this program. */
    gctBOOL                     disableEarlyZ;

    /* all the blend modes the shader needs to do */
    gceLAYOUT_QUALIFIER         outputBlends;

    /* whether the shader enable early fragment test, only affect fragment shader. */
    gctBOOL                     useEarlyFragTest;

    /* Use gl_LastFragData[]. */
    gctBOOL                     useLastFragData;

    /* whether the shader's input vertex count coming from driver, only affect TCS shader.
       during recompilation, this flag is set to be true. */
    gctBOOL                     useDriverTcsPatchInputVertices;

    /* whether this shader has any not-states-related error that can be detected by FE
       but specs requires that those errors are treated as link-time errors. */
    gceSTATUS                   hasNotStagesRelatedLinkError;

    void *                      debugInfo;
    gceFRAGOUT_USAGE            fragOutUsage;

    gctUINT32                   optionsLen;
    gctSTRING                   buildOptions;

#if _SUPPORT_LONG_ULONG_DATA_TYPE
    /* used to modefy the index of instruction when need to insert instruction into the shader when recompile */
    gctUINT                     InsertCount;
    gctUINT                     InstNum;
#endif

};

/* Defines for OCL on reserved temp registers used for memory space addresses */
/* Once add a new entry, please also do the corresponding change in gc_vsc_vir_ir.h. */
#define _gcdOCL_PrivateMemoryAddressRegIndex     1  /*private memory address register index */
#define _gcdOCL_LocalMemoryAddressRegIndex       2  /*local memory address register index for the local variables within kernel function */
#define _gcdOCL_ParmLocalMemoryAddressRegIndex   3  /*local memory address register index for the local parameters */
#define _gcdOCL_ConstantMemoryAddressRegIndex    4  /*constant memory address register index */
#define _gcdOCL_PrintfStartMemoryAddressRegIndex 5  /*printf start memory address register index */
#define _gcdOCL_PrintfEndMemoryAddressRegIndex   6  /*printf end memory address register index */
#define _gcdOCL_PreScaleGlobalIdRegIndex         7  /*pre-scale global ID register index */
#define _gcdOCL_NumMemoryAddressRegs             8  /*number of memory address registers */
#define _gcdOCL_MaxLocalTempRegs                16  /*maximum # of local temp register including the reserved ones for base addresses to memory spaces*/

#define gcShaderIsDesktopGL(S)                  ((S)->clientApiVersion == gcvAPI_OPENGL)
#define gcShaderIsOpenVG(S)                     ((S)->clientApiVersion == gcvAPI_OPENVG || (((S)->compilerVersion[0] & 0xffff) == _SHADER_VG_TYPE))

#define GetShaderObject(s)                      ((s)->object)
#define GetShaderClientApiVersion(s)            ((s)->clientApiVersion)
#define GetShaderID(s)                          ((s)->_id)
#define GetShaderConstVectorId(s)               ((s)->_constVectorId)
#define GetShaderDummyUniformCount(s)           ((s)->_dummyUniformCount)
#define GetShaderTempRegCount(s)                ((s)->_tempRegCount)
#define SetShaderTempRegCount(s, v)             ((s)->_tempRegCount = (v))
#define GetShaderMaxLocalTempRegCount(s)        ((s)->_maxLocalTempRegCount)
#define SetShaderMaxLocalTempRegCount(s, v)     ((s)->_maxLocalTempRegCount = (v))
#define GetShaderEnableDefaultUBO(s)            ((s)->enableDefaultUBO)
#define GetShaderDefaultUniformBlockIndex(s)    ((s)->_defaultUniformBlockIndex)
#define GetShaderConstUniformBlockIndex(s)      ((s)->constUniformBlockIndex)
#define GetShaderConstUBOSize(s)                ((s)->constUBOSize)
#define GetShaderConstUBOData(s)                ((s)->constUBOData)
#define GetShaderCompilerVersion(s)             ((s)->compilerVersion)
#define GetShaderType(s)                        ((s)->type)
#define GetShaderFlags(s)                       ((s)->flags)
#define GetShaderMaxKernelFunctionArgs(s)       ((s)->maxKernelFunctionArgs)
#define GetShaderGlobalUniformCount(s)          ((s)->globalUniformCount)
#define GetShaderConstantMemorySize(s)          ((s)->constantMemorySize)
#define GetShaderConstantMemoryBuffer(s)        ((s)->constantMemoryBuffer)
#define GetShaderTypeNameBufferSize(s)          ((s)->typeNameBufferSize)
#define GetShaderTypeNameBuffer(s)              ((s)->typeNameBuffer)
#define GetShaderPrivateMemorySize(s)           ((s)->privateMemorySize)
#define GetShaderLocalMemorySize(s)             ((s)->localMemorySize)
#define GetShaderAttributeArraySize(s)          ((s)->attributeArraySize)
#define GetShaderAttributeCount(s)              ((s)->attributeCount)
#define GetShaderAttributes(s)                  ((s)->attributes)
#define GetShaderAttribute(s, i)                ((s)->attributes[(i)])
#define GetShaderUniformArraySize(s)            ((s)->uniformArraySize)
#define GetShaderUniformCount(s)                ((s)->uniformCount)
#define GetShaderUniformVectorCount(s)          ((s)->uniformVectorCount)
#define GetShaderUniforms(s)                    ((s)->uniforms)
#define GetShaderUniform(s, i)                  ((s)->uniforms[i])
#define GetShaderSamplerIndex(s)                ((s)->samplerIndex)
#define GetShaderUniformBlockArraySize(s)       ((s)->uniformBlockArraySize)
#define GetShaderUniformBlockCount(s)           ((s)->uniformBlockCount)
#define GetShaderUniformBlocks(s)               ((s)->uniformBlocks)
#define GetShaderUniformBlock(s, i)             ((s)->uniformBlocks[(i)])
#define GetShaderLocationArraySize(s)           ((s)->locationArraySize)
#define GetShaderLocationCount(s)               ((s)->locationCount)
#define GetShaderLocations(s)                   ((s)->locations)
#define GetShaderOutputArraySize(s)             ((s)->outputArraySize)
#define GetShaderOutputCount(s)                 ((s)->outputCount)
#define GetShaderOutputs(s)                     ((s)->outputs)
#define GetShaderVariableArraySize(s)           ((s)->variableArraySize)
#define GetShaderVariableCount(s)               ((s)->variableCount)
#define GetShaderVariables(s)                   ((s)->variables)
#define GetShaderFunctionArraySize(s)           ((s)->functionArraySize)
#define GetShaderFunctionCount(s)               ((s)->functionCount)
#define GetShaderFunctions(s)                   ((s)->functions)
#define GetShaderCurrentFunction(s)             ((s)->currentFunction)
#define GetShaderKernelFunctionArraySize(s)     ((s)->kernelFunctionArraySize)
#define GetShaderKernelFunctionCount(s)         ((s)->kernelFunctionCount)
#define GetShaderKernelFunctions(s)             ((s)->kernelFunctions)
#define GetShaderKernelFunction(s, i)           ((s)->kernelFunctions[(i)])
#define GetShaderCurrentKernelFunction(s)       ((s)->currentKernelFunction)
#define GetShaderWorkGroupSize(s)               ((s)->shaderLayout.compute.workGroupSize)
#define SetShaderWorkGroupSize(s, v)            ((s)->shaderLayout.compute.workGroupSize[0] = (v)[0], \
                                                 (s)->shaderLayout.compute.workGroupSize[1] = (v)[1], \
                                                 (s)->shaderLayout.compute.workGroupSize[2] = (v)[2])
#define GetShaderCodeCount(s)                   ((s)->codeCount)
#define GetShaderLastInstruction(s)             ((s)->lastInstruction)
#define GetShaderInstrIndex(s)                  ((s)->instrIndex)
#define GetShaderLabels(s)                      ((s)->labels)
#define GetShaderInstructions(s)                ((s)->code)
#define GetShaderInstruction(s, i)              (&((s)->code[(i)]))
#define GetShaderLoadUsers(s)                   ((s)->loadUsers)
#define GetShaderLtcUniformCount(s)             ((s)->ltcUniformCount)
#define GetShaderLtcUniformBegin(s)             ((s)->ltcUniformBegin)
#define GetShaderLtcCodeUniformIndex(s, i)      (gcSHADER_GetLtcCodeUniformIndex((s), (i)))
#define GetShaderLtcInstructionCount(s)         ((s)->ltcInstructionCount)
#define GetShaderLtcExpressions(s)              ((s)->ltcExpressions)
#define GetShaderLtcExpression(s, i)            (&((s)->ltcExpressions[(i)]))
#define GetShaderVsPositionZDependsOnW(s)       ((s)->vsPositionZDependsOnW)
#define GetShaderOptimizationOption(s)          ((s)->optimizationOption)
#define GetShaderTransformFeedback(s)           ((s)->transformFeedback)
#define GetShaderSourceLength(s)                ((s)->sourceLength)
#define GetShaderSourceCode(s)                  ((s)->source)
#define GetShaderMappingTableExntries(s)        ((s)->mappingTableExntries)
#define GetShaderMappingTable(s)                ((s)->mappingTable)
#define GetShaderLinkedToShaderId(s)            ((s)->linkedToShaderId)
#define GetShaderLibraryList(s)                 ((s)->libraryList)
#define GetShaderReplaceIndex(s)                ((s)->replaceIndex)
#define GetShaderRARegWaterMark(s)              ((s)->RARegWaterMark)
#define GetShaderRATempReg(s)                   ((s)->RATempReg)
#define GetShaderRAHighestPass(s)               ((s)->RAHighestPriority)

#define GetShaderHasIntrinsicBuiltin(s)         gcShaderHasIntrinsicBuiltin(s)
#define SetShaderHasIntrinsicBuiltin(s, v)      do { \
                                                   if((v) == gcvTRUE) { \
                                                      gcShaderSetHasIntrinsicBuiltin(s); \
                                                   } \
                                                   else { \
                                                      gcShaderClrHasIntrinsicBuiltin(s); \
                                                   } \
                                                } while (gcvFALSE)

#define GetShaderHasExternFunction(s)           gcShaderHasExternFunction(s)
#define SetShaderHasExternFunction(s, v)        do { \
                                                   if((v) == gcvTRUE) { \
                                                      gcShaderSetHasExternFunction(s); \
                                                   } \
                                                   else { \
                                                      gcShaderClrHasExternFunction(s); \
                                                   } \
                                                } while (gcvFALSE)

#define GetShaderHasExternVariable(s)           gcShaderHasExternVariable(s)
#define SetShaderHasExternVariable(s, v)        do { \
                                                   if((v) == gcvTRUE) { \
                                                      gcShaderSetHasExternVariable(s); \
                                                   } \
                                                   else { \
                                                      gcShaderClrHasExternVariable(s); \
                                                   } \
                                                } while (gcvFALSE)

#define GetShaderNeedPatchForCentroid(s)        gcShaderNeedPatchForCentroid(s)
#define SetShaderNeedPatchForCentroid(s, v)     do { \
                                                   if((v) == gcvTRUE) { \
                                                      gcShaderSetNeedPatchForCentroid(s); \
                                                   } \
                                                   else { \
                                                      gcShaderClrNeedPatchForCentroid(s); \
                                                   } \
                                                } while (gcvFALSE)

#define GetShaderOutputBlends(s)                ((s)->outputBlends)
#define SetShaderOutputBlends(s, v)             ((s)->outputBlends |= (v))
#define ResetShaderOutputBlends(s, v)           ((s)->outputBlends &= ~(v))
#define ClearShaderOutputBlends(s)              ((s)->outputBlends = gcvLAYOUT_QUALIFIER_NONE)

#define SetComputeShaderLayout(s, ws_x, ws_y, ws_z)                         \
        do {                                                                \
             gcmASSERT(GetShaderType(s) == gcSHADER_TYPE_COMPUTE);          \
             (s)->shaderLayout.compute.workGroupSize[0] = (ws_x);           \
             (s)->shaderLayout.compute.workGroupSize[1] = (ws_y);           \
             (s)->shaderLayout.compute.workGroupSize[2] = (ws_z);           \
        } while(0)

#define SetTcsShaderLayout(s, outVertices, inVertices)                      \
        do {                                                                \
             gcmASSERT(GetShaderType(s) == gcSHADER_TYPE_TCS);              \
             (s)->shaderLayout.tcs.tcsPatchOutputVertices = (outVertices);  \
             (s)->shaderLayout.tcs.tcsInputVerticesUniform = (inVertices);  \
        } while(0)

#define SetTesShaderLayout(s, pMode, vSpacing, order, pointMode, inVertices)                    \
        do {                                                                                    \
             gcmASSERT(GetShaderType(s) == gcSHADER_TYPE_TES);                                  \
             (s)->shaderLayout.tes.tessPrimitiveMode      = (gcTessPrimitiveMode)(pMode);       \
             (s)->shaderLayout.tes.tessVertexSpacing      = (gcTessVertexSpacing)(vSpacing);    \
             (s)->shaderLayout.tes.tessOrdering           = (gcTessOrdering)(order);            \
             (s)->shaderLayout.tes.tessPointMode          = (pointMode);                        \
             (s)->shaderLayout.tes.tessPatchInputVertices = (inVertices);                       \
        } while(0)

#define SetGeoShaderLayout(s, invoc, maxVertices, inPrimitive, outPrimitive)\
        do {                                                                \
             gcmASSERT(GetShaderType(s) == gcSHADER_TYPE_GEOMETRY);         \
             (s)->shaderLayout.geo.geoInvocations = (invoc);                \
             (s)->shaderLayout.geo.geoMaxVertices = (maxVertices);          \
             (s)->shaderLayout.geo.geoInPrimitive = (inPrimitive);          \
             (s)->shaderLayout.geo.geoOutPrimitive = (outPrimitive);        \
        } while(0)

typedef enum _gcBuiltinConst
{
    gcBIConst_MaxVertexAttribs = 0,
    gcBIConst_MaxVertexUniformVectors,
    gcBIConst_MaxVertexOutputVectors,
    gcBIConst_MaxFragmentInputVectors,
    gcBIConst_MaxVertexTextureImageUnits,
    gcBIConst_MaxCombinedTextureImageUnits,
    gcBIConst_MaxTextureImageUnits,
    gcBIConst_MaxFragmentUniformVectors,
    gcBIConst_MaxDrawBuffers,
    gcBIConst_MaxSamples,
    gcBIConst_MinProgramTexelOffset,
    gcBIConst_MaxProgramTexelOffset,
    gcBIConst_MaxImageUnits,
    gcBIConst_MaxVertexImageUniforms,
    gcBIConst_MaxFragmentImageUniforms,
    gcBIConst_MaxComputeImageUniforms,
    gcBIConst_MaxCombinedImageUniforms,
    gcBIConst_MaxCombinedShaderOutputResources,
    gcBIConst_MaxComputeWorkGroupCount,
    gcBIConst_MaxComputeWorkGroupSize,
    gcBIConst_MaxComputeUniformComponents,
    gcBIConst_MaxComputeTextureImageUnits,
    gcBIConst_MaxComputeAtomicCounters,
    gcBIConst_MaxComputeAtomicCounterBuffers,
    gcBIConst_MaxVertexAtomicCounters,
    gcBIConst_MaxFragmentAtomicCounters,
    gcBIConst_MaxCombinedAtomicCounters,
    gcBIConst_MaxAtomicCounterBindings,
    gcBIConst_MaxVertexAtomicCounterBuffers,
    gcBIConst_MaxFragmentAtomicCounterBuffers,
    gcBIConst_MaxCombinedAtomicCounterBuffers,
    gcBIConst_MaxAtomicCounterBufferSize,
    /* ES 1.0 */
    gcBIConst_MaxVaryingVectors,
    /* TS EXT */
    gcBIConst_MaxTessControlInputComponents,
    gcBIConst_MaxTessControlOutputComponents,
    gcBIConst_MaxTessControlTextureImageUnits,
    gcBIConst_MaxTessControlImageUniforms,
    gcBIConst_MaxTessControlUniformComponents,
    gcBIConst_MaxTessControlTotalOutputComponents,
    gcBIConst_MaxTessControlAtomicCounters,
    gcBIConst_MaxTessControlAtomicCounterBuffers,
    gcBIConst_MaxTessEvaluationInputComponents,
    gcBIConst_MaxTessEvaluationOutputComponents,
    gcBIConst_MaxTessEvaluationTextureImageUnits,
    gcBIConst_MaxTessEvaluationImageUniforms,
    gcBIConst_MaxTessEvaluationUniformComponents,
    gcBIConst_MaxTessEvaluationAtomicCounters,
    gcBIConst_MaxTessEvaluationAtomicCounterBuffers,
    gcBIConst_MaxTessPatchComponents,
    gcBIConst_MaxPatchVertices,
    gcBIConst_MaxTessGenLevel,
    /* GS EXT */
    gcBIConst_MaxGSInputComponents,
    gcBIConst_MaxGSOutputComponents,
    gcBIConst_MaxGSImageUniforms,
    gcBIConst_MaxGSTextureImageUnits,
    gcBIConst_MaxGSOutputVertices,
    gcBIConst_MaxGSTotalOutputComponents,
    gcBIConst_MaxGSUniformComponents,
    gcBIConst_MaxGSAtomicCounters,
    gcBIConst_MaxGSAtomicCounterBuffers,

    /* Desktop GL */
    gcBIConst_MaxLights,
    gcBIConst_MaxClipDistances,
    gcBIConst_MaxClipPlanes,
    gcBIConst_MaxFragmentUniformComponents,
    gcBIConst_MaxTextureCoords,
    gcBIConst_MaxTextureUnits,
    gcBIConst_MaxVaryingComponents,
    gcBIConst_MaxVaryingFloats,
    gcBIConst_MaxVertexUniformComponents,
    gcBIConst_MaxFragmentInputComponents,
    gcBIConst_MaxVertexOutputComponents,
    gcBIConst_MaxGSVaryingComponents,

    /* Constant count */
    gcBIConst_Count
} gcBuiltinConst;

END_EXTERN_C()

#endif /* __gc_vsc_old_gcsl_h_ */



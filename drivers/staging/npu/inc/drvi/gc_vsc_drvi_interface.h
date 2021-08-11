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
** Main interface header that outside world uses
*/

#ifndef __gc_vsc_drvi_interface_h_
#define __gc_vsc_drvi_interface_h_

#include "gc_vsc_precomp.h"

/* It will be fully removed after VIR totally replaces of gcSL */
#include "old_impl/gc_vsc_old_drvi_interface.h"

/******************************* VIR SHADER BINARY FILE VERSION ******************/
/* current version */
/* 0.0.1.4 add chipModel and ChipRevision, Nov. 30, 2017 */
/* 0.0.1.6 add VIR_OP_CLAMPCOORD, Feb. 2, 2018 */
/* 0.0.1.7 remove VG from shader flags, Mar. 1, 2018 */
/* 0.0.1.8 add component for VIR_Layout, Mar. 1, 2018 */
/* 0.0.1.9 save fixedTypeId for output variable, Mar. 5, 2018 */
/* 0.0.1.10 save extension flags for VIR_Shader, Mar. 6, 2018 */
/* 0.0.1.11 implement lib function nmin, nmax and nclamp, Mar. 12, 2018 */
/* 0.0.1.12 save the funcId for separateImage/separateSampler, Mar. 13, 2018 */
/* 0.0.1.13 save the function sym for a local symbol, Mar. 14, 2018 */
/* 0.0.1.14 add one more flag for VIR_Function, Mar. 21, 2018 */
/* 0.0.1.15 add the parameter "lod" for image_fetch_samplerBuffer, Mar. 30, 2018 */
/* 0.0.1.16 add a flag in VIR_Uniform, Apr. 2, 2018 */
/* 0.0.1.17 save more memoryAccessFlag, Apr. 19, 2018 */
/* 0.0.1.18 change image_fetch_gsamplerBuffer prototype, Aug. 28, 2018 */
/* 0.0.1.19 add atomic patch library function */
/* 0.0.1.20 add imageFetch/texelBufferToImage, Nov. 2, 2018 */
/* 0.0.1.21 save the UBO symbol ID for the baseAddress, Nov. 8, 2018 */
/* 0.0.1.22 modify _viv_atan2_float() to comform to CL spec on 11/20/2018 */
/* 0.0.1.23 using HALTI5 trig functions for all cases (not just conformance) on 12/3/2018 */
/* 0.0.1.24 save the kernel function name ID on 12/24/2018 */
/* 0.0.1.25 add image_query_size lib functions for samplerMS on 12/27/2018 */
/* 0.0.1.26 remove some enumerations for VIR_ShaderFlags on 01/02/2019 */
/* 0.0.1.27 Add VIR_ModifierOrder in VIR_Operand on 01/03/2019 */
/* 0.0.1.28 Add magicNumber on shaderIOBuffer 01/08/2019 */
/* 0.0.1.29 Add some new opcodes 04/01/2019 */
/* 0.0.1.30 Add some flags in VIR_Shader and hints 04/17/2019 */
/* 0.0.1.31 Save module processes in VIR_Shader and hints 05/24/2019 */
/* 0.0.1.32 Add extension flag in VIR_Symbol 05/27/2019 */
/* 0.0.1.33 Add a new opcode LOGICAL_RSHIFT 05/28/2019 */
/* 0.0.1.34 Add WorkGroupSizeFactor in VIR_ComputeLayout on 07/18/2019 */
/* 0.0.1.35 Add skhp flag in VIR_Instruction */
/* 0.0.1.36 Add a function to patch clipDistance in GL VIR lib shader */
/* 0.0.1.37 Add the sampled image information on 03/11/2020 */
/* 0.0.1.38 Saving the return variable to a argument 03/27/2020 */
/* 0.0.1.39 Update the image addre for an image buffer 04/01/2020 */
/* 0.0.1.40 Save the SPIR-V information to VIR shader 04/10/2020 */
/* 0.0.1.41 Add a new opcode MOV_DUAL16 04/23/2020 */
/* 0.0.1.42 Move SkpHp into the instruction flag 05/14/2020 */
/* 0.0.1.43 Add a data precision in VIR_Symbol 05/14/2020 */
/* 0.0.1.44 Add VIR_ModifierOrder in VIR_Operand on 05/14/2020 */
/* 0.0.1.45 Add a new enumeration for VIR_SymFlagExt 05/15/2020 */
/* 0.0.1.46 Add a minimum workGroupSize in VIR_ComputeLayout 05/19/2020 */
/* 0.0.1.47 Save the HW specific attributes in VIR_Shader 05/26/2020 */
/* 0.0.1.48 Add a new variable to save the symbol ID of the register spill base address in VIR_Shader 07/13/2020 */
/* 0.0.1.49 Save the RA instruction ID in VIR_Instruction 08/11/2020 */
#define gcdVIR_SHADER_BINARY_FILE_VERSION gcmCC(SHADER_64BITMODE, 0, 1, 49)
#define gcdVIR_PROGRAM_BINARY_FILE_VERSION gcmCC(SHADER_64BITMODE, 0, 1, 49)

#if !defined(gcdTARGETHOST_BIGENDIAN)
#define gcdTARGETHOST_BIGENDIAN 0  /* default host little endian, to change the
                                    * host to big endian, build with -DgcdHOST_BIGENDIAN=1 */
#endif
#define gcdTARGETDEVICE_BIGENDIAN 0  /* device always little endian */

#define gcdSUPPORT_OCL_1_2          1
#define TREAT_ES20_INTEGER_AS_FLOAT 0
#define __USE_IMAGE_LOAD_TO_ACCESS_SAMPLER_BUFFER__ 1

BEGIN_EXTERN_C()

/* Copy from _VIR_PRIMITIVETYPEID. */
typedef enum _VSC_SHADER_DATA_TYPE
{
    VSC_SHADER_DATA_TYPE_UNKNOWN            = 0,
    VSC_SHADER_DATA_TYPE_VOID,
    /* scalar types */
    /* types can be mapped to equivalent machine type directly */
    VSC_SHADER_DATA_TYPE_FLOAT32,
    VSC_SHADER_DATA_TYPE_FLOAT16,
    VSC_SHADER_DATA_TYPE_INT32,
    VSC_SHADER_DATA_TYPE_INT16,
    VSC_SHADER_DATA_TYPE_INT8,
    VSC_SHADER_DATA_TYPE_UINT32,
    VSC_SHADER_DATA_TYPE_UINT16,
    VSC_SHADER_DATA_TYPE_UINT8,
    VSC_SHADER_DATA_TYPE_SNORM16,
    VSC_SHADER_DATA_TYPE_SNORM8,
    VSC_SHADER_DATA_TYPE_UNORM16,
    VSC_SHADER_DATA_TYPE_UNORM8,

    /* scalar types not supported by HW */
    VSC_SHADER_DATA_TYPE_INT64,
    VSC_SHADER_DATA_TYPE_UINT64,
    VSC_SHADER_DATA_TYPE_FLOAT64,
    VSC_SHADER_DATA_TYPE_BOOLEAN,

    /* vector types */
    /* openCL support vector 8, 16 for all scalar types: int16, int8, etc */
    VSC_SHADER_DATA_TYPE_FLOAT_X2,
    VSC_SHADER_DATA_TYPE_FLOAT_X3,
    VSC_SHADER_DATA_TYPE_FLOAT_X4,
    VSC_SHADER_DATA_TYPE_FLOAT_X8,
    VSC_SHADER_DATA_TYPE_FLOAT_X16,
    VSC_SHADER_DATA_TYPE_FLOAT_X32,

    VSC_SHADER_DATA_TYPE_FLOAT16_X2,
    VSC_SHADER_DATA_TYPE_FLOAT16_X3,
    VSC_SHADER_DATA_TYPE_FLOAT16_X4,
    VSC_SHADER_DATA_TYPE_FLOAT16_X8,
    VSC_SHADER_DATA_TYPE_FLOAT16_X16,
    VSC_SHADER_DATA_TYPE_FLOAT16_X32,

    VSC_SHADER_DATA_TYPE_FLOAT64_X2,
    VSC_SHADER_DATA_TYPE_FLOAT64_X3,
    VSC_SHADER_DATA_TYPE_FLOAT64_X4,
    VSC_SHADER_DATA_TYPE_FLOAT64_X8,
    VSC_SHADER_DATA_TYPE_FLOAT64_X16,
    VSC_SHADER_DATA_TYPE_FLOAT64_X32,

    VSC_SHADER_DATA_TYPE_BOOLEAN_X2,
    VSC_SHADER_DATA_TYPE_BOOLEAN_X3,
    VSC_SHADER_DATA_TYPE_BOOLEAN_X4,
    VSC_SHADER_DATA_TYPE_BOOLEAN_X8,
    VSC_SHADER_DATA_TYPE_BOOLEAN_X16,
    VSC_SHADER_DATA_TYPE_BOOLEAN_X32,

    VSC_SHADER_DATA_TYPE_INTEGER_X2,
    VSC_SHADER_DATA_TYPE_INTEGER_X3,
    VSC_SHADER_DATA_TYPE_INTEGER_X4,
    VSC_SHADER_DATA_TYPE_INTEGER_X8,
    VSC_SHADER_DATA_TYPE_INTEGER_X16,
    VSC_SHADER_DATA_TYPE_INTEGER_X32,

    VSC_SHADER_DATA_TYPE_UINT_X2,
    VSC_SHADER_DATA_TYPE_UINT_X3,
    VSC_SHADER_DATA_TYPE_UINT_X4,
    VSC_SHADER_DATA_TYPE_UINT_X8,
    VSC_SHADER_DATA_TYPE_UINT_X16,
    VSC_SHADER_DATA_TYPE_UINT_X32,

    /* uchar vectors */
    VSC_SHADER_DATA_TYPE_UINT8_X2,
    VSC_SHADER_DATA_TYPE_UINT8_X3,
    VSC_SHADER_DATA_TYPE_UINT8_X4,
    VSC_SHADER_DATA_TYPE_UINT8_X8,
    VSC_SHADER_DATA_TYPE_UINT8_X16,
    VSC_SHADER_DATA_TYPE_UINT8_X32,

    /* char vectors */
    VSC_SHADER_DATA_TYPE_INT8_X2,
    VSC_SHADER_DATA_TYPE_INT8_X3,
    VSC_SHADER_DATA_TYPE_INT8_X4,
    VSC_SHADER_DATA_TYPE_INT8_X8,
    VSC_SHADER_DATA_TYPE_INT8_X16,
    VSC_SHADER_DATA_TYPE_INT8_X32,

    /* ushort vectors */
    VSC_SHADER_DATA_TYPE_UINT16_X2,
    VSC_SHADER_DATA_TYPE_UINT16_X3,
    VSC_SHADER_DATA_TYPE_UINT16_X4,
    VSC_SHADER_DATA_TYPE_UINT16_X8,
    VSC_SHADER_DATA_TYPE_UINT16_X16,
    VSC_SHADER_DATA_TYPE_UINT16_X32,

    /* short vectors */
    VSC_SHADER_DATA_TYPE_INT16_X2,
    VSC_SHADER_DATA_TYPE_INT16_X3,
    VSC_SHADER_DATA_TYPE_INT16_X4,
    VSC_SHADER_DATA_TYPE_INT16_X8,
    VSC_SHADER_DATA_TYPE_INT16_X16,
    VSC_SHADER_DATA_TYPE_INT16_X32,

    /* uint64 vectors */
    VSC_SHADER_DATA_TYPE_UINT64_X2,
    VSC_SHADER_DATA_TYPE_UINT64_X3,
    VSC_SHADER_DATA_TYPE_UINT64_X4,
    VSC_SHADER_DATA_TYPE_UINT64_X8,
    VSC_SHADER_DATA_TYPE_UINT64_X16,
    VSC_SHADER_DATA_TYPE_UINT64_X32,

    /* int64 vectors */
    VSC_SHADER_DATA_TYPE_INT64_X2,
    VSC_SHADER_DATA_TYPE_INT64_X3,
    VSC_SHADER_DATA_TYPE_INT64_X4,
    VSC_SHADER_DATA_TYPE_INT64_X8,
    VSC_SHADER_DATA_TYPE_INT64_X16,
    VSC_SHADER_DATA_TYPE_INT64_X32,

    /* packed data type */

    /* packed float16 (2 bytes per element) */
    VSC_SHADER_DATA_TYPE_FLOAT16_P2,
    VSC_SHADER_DATA_TYPE_FLOAT16_P3,
    VSC_SHADER_DATA_TYPE_FLOAT16_P4,
    VSC_SHADER_DATA_TYPE_FLOAT16_P8,
    VSC_SHADER_DATA_TYPE_FLOAT16_P16,
    VSC_SHADER_DATA_TYPE_FLOAT16_P32,

    /* packed boolean (1 byte per element) */
    VSC_SHADER_DATA_TYPE_BOOLEAN_P2,
    VSC_SHADER_DATA_TYPE_BOOLEAN_P3,
    VSC_SHADER_DATA_TYPE_BOOLEAN_P4,
    VSC_SHADER_DATA_TYPE_BOOLEAN_P8,
    VSC_SHADER_DATA_TYPE_BOOLEAN_P16,
    VSC_SHADER_DATA_TYPE_BOOLEAN_P32,

    /* uchar vectors (1 byte per element) */
    VSC_SHADER_DATA_TYPE_UINT8_P2,
    VSC_SHADER_DATA_TYPE_UINT8_P3,
    VSC_SHADER_DATA_TYPE_UINT8_P4,
    VSC_SHADER_DATA_TYPE_UINT8_P8,
    VSC_SHADER_DATA_TYPE_UINT8_P16,
    VSC_SHADER_DATA_TYPE_UINT8_P32,

    /* char vectors (1 byte per element) */
    VSC_SHADER_DATA_TYPE_INT8_P2,
    VSC_SHADER_DATA_TYPE_INT8_P3,
    VSC_SHADER_DATA_TYPE_INT8_P4,
    VSC_SHADER_DATA_TYPE_INT8_P8,
    VSC_SHADER_DATA_TYPE_INT8_P16,
    VSC_SHADER_DATA_TYPE_INT8_P32,

    /* ushort vectors (2 bytes per element) */
    VSC_SHADER_DATA_TYPE_UINT16_P2,
    VSC_SHADER_DATA_TYPE_UINT16_P3,
    VSC_SHADER_DATA_TYPE_UINT16_P4,
    VSC_SHADER_DATA_TYPE_UINT16_P8,
    VSC_SHADER_DATA_TYPE_UINT16_P16,
    VSC_SHADER_DATA_TYPE_UINT16_P32,

    /* short vectors (2 bytes per element) */
    VSC_SHADER_DATA_TYPE_INT16_P2,
    VSC_SHADER_DATA_TYPE_INT16_P3,
    VSC_SHADER_DATA_TYPE_INT16_P4,
    VSC_SHADER_DATA_TYPE_INT16_P8,
    VSC_SHADER_DATA_TYPE_INT16_P16,
    VSC_SHADER_DATA_TYPE_INT16_P32,

    /* matrix type: only support float type */
    VSC_SHADER_DATA_TYPE_FLOAT_2X2,
    VSC_SHADER_DATA_TYPE_FLOAT_3X3,
    VSC_SHADER_DATA_TYPE_FLOAT_4X4,
    VSC_SHADER_DATA_TYPE_FLOAT_2X3,
    VSC_SHADER_DATA_TYPE_FLOAT_2X4,
    VSC_SHADER_DATA_TYPE_FLOAT_3X2,
    VSC_SHADER_DATA_TYPE_FLOAT_3X4,
    VSC_SHADER_DATA_TYPE_FLOAT_4X2,
    VSC_SHADER_DATA_TYPE_FLOAT_4X3,

    VSC_SHADER_DATA_TYPE_FLOAT16_2X2,
    VSC_SHADER_DATA_TYPE_FLOAT16_3X3,
    VSC_SHADER_DATA_TYPE_FLOAT16_4X4,
    VSC_SHADER_DATA_TYPE_FLOAT16_2X3,
    VSC_SHADER_DATA_TYPE_FLOAT16_2X4,
    VSC_SHADER_DATA_TYPE_FLOAT16_3X2,
    VSC_SHADER_DATA_TYPE_FLOAT16_3X4,
    VSC_SHADER_DATA_TYPE_FLOAT16_4X2,
    VSC_SHADER_DATA_TYPE_FLOAT16_4X3,

    VSC_SHADER_DATA_TYPE_FLOAT64_2X2,
    VSC_SHADER_DATA_TYPE_FLOAT64_3X3,
    VSC_SHADER_DATA_TYPE_FLOAT64_4X4,
    VSC_SHADER_DATA_TYPE_FLOAT64_2X3,
    VSC_SHADER_DATA_TYPE_FLOAT64_2X4,
    VSC_SHADER_DATA_TYPE_FLOAT64_3X2,
    VSC_SHADER_DATA_TYPE_FLOAT64_3X4,
    VSC_SHADER_DATA_TYPE_FLOAT64_4X2,
    VSC_SHADER_DATA_TYPE_FLOAT64_4X3,

    /* sampler type */
    VSC_SHADER_DATA_TYPE_MIN_SAMPLER_TYID,
    VSC_SHADER_DATA_TYPE_SAMPLER_1D = VSC_SHADER_DATA_TYPE_MIN_SAMPLER_TYID,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D,
    VSC_SHADER_DATA_TYPE_SAMPLER_3D,
    VSC_SHADER_DATA_TYPE_SAMPLER_CUBIC,
    VSC_SHADER_DATA_TYPE_SAMPLER_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_SAMPLER,
    VSC_SHADER_DATA_TYPE_ISAMPLER_1D,
    VSC_SHADER_DATA_TYPE_ISAMPLER_2D,
    VSC_SHADER_DATA_TYPE_ISAMPLER_3D,
    VSC_SHADER_DATA_TYPE_ISAMPLER_CUBIC,
    VSC_SHADER_DATA_TYPE_ISAMPLER_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_USAMPLER_1D,
    VSC_SHADER_DATA_TYPE_USAMPLER_2D,
    VSC_SHADER_DATA_TYPE_USAMPLER_3D,
    VSC_SHADER_DATA_TYPE_USAMPLER_CUBIC,
    VSC_SHADER_DATA_TYPE_USAMPLER_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_SAMPLER_EXTERNAL_OES,

    VSC_SHADER_DATA_TYPE_SAMPLER_1D_SHADOW,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_SHADOW,
    VSC_SHADER_DATA_TYPE_SAMPLER_CUBE_SHADOW,
    VSC_SHADER_DATA_TYPE_SAMPLER_CUBE_ARRAY_SHADOW,

    VSC_SHADER_DATA_TYPE_SAMPLER_1D_ARRAY,
    VSC_SHADER_DATA_TYPE_SAMPLER_1D_ARRAY_SHADOW,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_ISAMPLER_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_USAMPLER_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_ARRAY_SHADOW,

    VSC_SHADER_DATA_TYPE_SAMPLER_2D_MS,
    VSC_SHADER_DATA_TYPE_ISAMPLER_2D_MS,
    VSC_SHADER_DATA_TYPE_USAMPLER_2D_MS,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_MS_ARRAY,
    VSC_SHADER_DATA_TYPE_ISAMPLER_2D_MS_ARRAY,
    VSC_SHADER_DATA_TYPE_USAMPLER_2D_MS_ARRAY,
    VSC_SHADER_DATA_TYPE_SAMPLER_BUFFER,
    VSC_SHADER_DATA_TYPE_ISAMPLER_BUFFER,
    VSC_SHADER_DATA_TYPE_USAMPLER_BUFFER,
    VSC_SHADER_DATA_TYPE_VIV_GENERIC_GL_SAMPLER,
    VSC_SHADER_DATA_TYPE_MAX_SAMPLER_TYID = VSC_SHADER_DATA_TYPE_VIV_GENERIC_GL_SAMPLER,

    /* image type */
    VSC_SHADER_DATA_TYPE_MIN_IMAGE_TYID,
    /* subPass input */
    VSC_SHADER_DATA_TYPE_SUBPASSINPUT = VSC_SHADER_DATA_TYPE_MIN_IMAGE_TYID,
    VSC_SHADER_DATA_TYPE_SUBPASSINPUTMS,
    VSC_SHADER_DATA_TYPE_ISUBPASSINPUT,
    VSC_SHADER_DATA_TYPE_ISUBPASSINPUTMS,
    VSC_SHADER_DATA_TYPE_USUBPASSINPUT,
    VSC_SHADER_DATA_TYPE_USUBPASSINPUTMS,

    VSC_SHADER_DATA_TYPE_IMAGE_1D,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_ARRAY_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_BUFFER,
    VSC_SHADER_DATA_TYPE_IIMAGE_1D,
    VSC_SHADER_DATA_TYPE_IIMAGE_1D_ARRAY,
    VSC_SHADER_DATA_TYPE_UIMAGE_1D,
    VSC_SHADER_DATA_TYPE_UIMAGE_1D_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_2D,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_3D,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_MSSA,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_ARRAY_MSSA,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_MSSA_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_ARRAY_MSSA_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_ARRAY_DEPTH,
    VSC_SHADER_DATA_TYPE_IIMAGE_2D,
    VSC_SHADER_DATA_TYPE_IIMAGE_2D_MSSA,
    VSC_SHADER_DATA_TYPE_IIMAGE_2D_ARRAY_MSSA,
    VSC_SHADER_DATA_TYPE_UIMAGE_2D,
    VSC_SHADER_DATA_TYPE_UIMAGE_2D_MSSA,
    VSC_SHADER_DATA_TYPE_UIMAGE_2D_ARRAY_MSSA,
    VSC_SHADER_DATA_TYPE_IIMAGE_3D,
    VSC_SHADER_DATA_TYPE_UIMAGE_3D,
    VSC_SHADER_DATA_TYPE_IIMAGE_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_UIMAGE_2D_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_CUBE,
    VSC_SHADER_DATA_TYPE_IMAGE_CUBE_DEPTH,
    VSC_SHADER_DATA_TYPE_IMAGE_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_CUBE_DEPTH_ARRAY,
    VSC_SHADER_DATA_TYPE_IIMAGE_CUBE,
    VSC_SHADER_DATA_TYPE_IIMAGE_CUBE_DEPTH,
    VSC_SHADER_DATA_TYPE_IIMAGE_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_UIMAGE_CUBE,
    VSC_SHADER_DATA_TYPE_UIMAGE_CUBE_DEPTH,
    VSC_SHADER_DATA_TYPE_UIMAGE_CUBE_ARRAY,
    VSC_SHADER_DATA_TYPE_IMAGE_BUFFER,
    VSC_SHADER_DATA_TYPE_IIMAGE_BUFFER,
    VSC_SHADER_DATA_TYPE_UIMAGE_BUFFER,
    VSC_SHADER_DATA_TYPE_VIV_GENERIC_GL_IMAGE,
    VSC_SHADER_DATA_TYPE_MAX_IMAGE_TYID = VSC_SHADER_DATA_TYPE_VIV_GENERIC_GL_IMAGE,

    /* For OCL */
    VSC_SHADER_DATA_TYPE_MIN_IMAGE_T_TYID,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_T = VSC_SHADER_DATA_TYPE_MIN_IMAGE_T_TYID,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_BUFFER_T,
    VSC_SHADER_DATA_TYPE_IMAGE_1D_ARRAY_T,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_T,
    VSC_SHADER_DATA_TYPE_IMAGE_2D_ARRAY_T,
    VSC_SHADER_DATA_TYPE_IMAGE_3D_T,
    VSC_SHADER_DATA_TYPE_VIV_GENERIC_IMAGE_T,
    VSC_SHADER_DATA_TYPE_MAX_IMAGE_T_TYID = VSC_SHADER_DATA_TYPE_VIV_GENERIC_IMAGE_T,
    VSC_SHADER_DATA_TYPE_SAMPLER_T,
    VSC_SHADER_DATA_TYPE_EVENT_T,

    /* atomic counter type */
    VSC_SHADER_DATA_TYPE_MIN_ATOMIC_COUNTER_TYPID,
    VSC_SHADER_DATA_TYPE_ATOMIC_UINT = VSC_SHADER_DATA_TYPE_MIN_ATOMIC_COUNTER_TYPID,
    VSC_SHADER_DATA_TYPE_ATOMIC_UINT4,
    VSC_SHADER_DATA_TYPE_MAX_ATOMIC_COUNTER_TYPID = VSC_SHADER_DATA_TYPE_ATOMIC_UINT4,

    /* OpenGL 4.0 types */
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_RECT,
    VSC_SHADER_DATA_TYPE_ISAMPLER_2D_RECT,
    VSC_SHADER_DATA_TYPE_USAMPLER_2D_RECT,
    VSC_SHADER_DATA_TYPE_SAMPLER_2D_RECT_SHADOW,
    VSC_SHADER_DATA_TYPE_ISAMPLER_1D_ARRAY,
    VSC_SHADER_DATA_TYPE_USAMPLER_1D_ARRAY,

    VSC_SHADER_DATA_TYPE_PRIMITIVETYPE_COUNT,
    VSC_SHADER_DATA_TYPE_LAST_PRIMITIVETYPE = VSC_SHADER_DATA_TYPE_PRIMITIVETYPE_COUNT-1,
}
VSC_SHADER_DATA_TYPE;

/* Copy from VIR_ImageFormat. */
/* Any modification here, please do the corresponding change for VIR_ImageFormat. */
typedef enum _VSC_IMAGE_FORMAT
{
    VSC_IMAGE_FORMAT_NONE = 0x00000000,
    /*F32.*/
    VSC_IMAGE_FORMAT_RGBA32F,
    VSC_IMAGE_FORMAT_RG32F,
    VSC_IMAGE_FORMAT_R32F,
    /*I32.*/
    VSC_IMAGE_FORMAT_RGBA32I,
    VSC_IMAGE_FORMAT_RG32I,
    VSC_IMAGE_FORMAT_R32I,
    /*UI32.*/
    VSC_IMAGE_FORMAT_RGBA32UI,
    VSC_IMAGE_FORMAT_RG32UI,
    VSC_IMAGE_FORMAT_R32UI,
    /*F16.*/
    VSC_IMAGE_FORMAT_RGBA16F,
    VSC_IMAGE_FORMAT_RG16F,
    VSC_IMAGE_FORMAT_R16F,
    /*I16.*/
    VSC_IMAGE_FORMAT_RGBA16I,
    VSC_IMAGE_FORMAT_RG16I,
    VSC_IMAGE_FORMAT_R16I,
    /*UI16.*/
    VSC_IMAGE_FORMAT_RGBA16UI,
    VSC_IMAGE_FORMAT_RG16UI,
    VSC_IMAGE_FORMAT_R16UI,
    /*F16 SNORM/UNORM.*/
    VSC_IMAGE_FORMAT_RGBA16,
    VSC_IMAGE_FORMAT_RGBA16_SNORM,
    VSC_IMAGE_FORMAT_RG16,
    VSC_IMAGE_FORMAT_RG16_SNORM,
    VSC_IMAGE_FORMAT_R16,
    VSC_IMAGE_FORMAT_R16_SNORM,
    /*F8 SNORM/UNORM.*/
    VSC_IMAGE_FORMAT_BGRA8_UNORM,
    VSC_IMAGE_FORMAT_RGBA8,
    VSC_IMAGE_FORMAT_RGBA8_SNORM,
    VSC_IMAGE_FORMAT_RG8,
    VSC_IMAGE_FORMAT_RG8_SNORM,
    VSC_IMAGE_FORMAT_R8,
    VSC_IMAGE_FORMAT_R8_SNORM,
    /*I8.*/
    VSC_IMAGE_FORMAT_RGBA8I,
    VSC_IMAGE_FORMAT_RG8I,
    VSC_IMAGE_FORMAT_R8I,
    /*UI8.*/
    VSC_IMAGE_FORMAT_RGBA8UI,
    VSC_IMAGE_FORMAT_RG8UI,
    VSC_IMAGE_FORMAT_R8UI,
    /*F-PACK.*/
    VSC_IMAGE_FORMAT_R5G6B5_UNORM_PACK16,
    VSC_IMAGE_FORMAT_ABGR8_UNORM_PACK32,
    VSC_IMAGE_FORMAT_ABGR8I_PACK32,
    VSC_IMAGE_FORMAT_ABGR8UI_PACK32,
    VSC_IMAGE_FORMAT_A2R10G10B10_UNORM_PACK32,
    VSC_IMAGE_FORMAT_A2B10G10R10_UNORM_PACK32,
    VSC_IMAGE_FORMAT_A2B10G10R10UI_PACK32,
} VSC_IMAGE_FORMAT;

typedef enum _VSC_ADDRSPACE
{
    VIR_AS_PRIVATE, /* private address space */
    VIR_AS_GLOBAL, /* global address space */
    VIR_AS_CONSTANT, /* constant address space, uniform mapped to this space */
    VIR_AS_LOCAL            /* local address space, function scope locals mappped
                               into this space */
} VSC_AddrSpace;

typedef enum _VSC_TYQUALIFIER
{
    VIR_TYQUAL_NONE         = 0x00, /* unqualified */
    VIR_TYQUAL_CONST        = 0x01, /* const */
    VIR_TYQUAL_VOLATILE     = 0x02, /* volatile */
    VIR_TYQUAL_RESTRICT     = 0x04, /* restrict */
    VIR_TYQUAL_READ_ONLY    = 0x08, /* readonly */
    VIR_TYQUAL_WRITE_ONLY   = 0x10, /* writeonly */
    VIR_TYQUAL_CONSTANT     = 0x20, /* constant address space */
    VIR_TYQUAL_GLOBAL       = 0x40, /* global address space */
    VIR_TYQUAL_LOCAL        = 0x80, /* local address space */
    VIR_TYQUAL_PRIVATE      = 0x100, /* private address space */
} VSC_TyQualifier;

typedef VSC_AddrSpace        VIR_AddrSpace;
typedef VSC_TyQualifier      VIR_TyQualifier;

/* for different HW, we use different instruction to implement OCL image read/write
 * to achieve best performance */
typedef enum {
    VSC_OCLImgLibKind_UseLoadStore       = 0, /* for v54x GC chips, use LOAD/STORE/TEXLD */
    VSC_OCLImgLibKind_UseImgLoadTexldU   = 1, /* for v55 GC chips, use IMG_LOAD/IMG_STORE/TEXLD_U  -- unused now */
    VSC_OCLImgLibKind_UseImgLoadTexldUXY = 2, /* for v60 GC and v620 GC chips -- unused now*/
    VSC_OCLImgLibKind_UseImgLoadVIP      = 3, /* v60 VIP chip, use IMG_LOAD/IMG_STORE */
    VSC_OCLImgLibKind_Counts, /* count of img libs */
    VSC_OCLImgLibKind_BasedOnHWFeature         /* select library based on HW feature */
} VSC_OCLImgLibKind;

typedef enum
{
    VSC_ImageValueFloat      = 0, /* float type: read_imagef */
    VSC_ImageValueInt        = 1, /* int type: read_imagei */
    VSC_ImageValueUint       = 2        /* unsigned int type: read_imageui */
} vscImageValueType;

typedef union _VSC_Image_desc {
    struct {
        /* the first 4 32-bits are the same as HW imge_desc as of V630 */
        gctUINT   baseAddress;          /* base address of image data */
        gctUINT   row_stride;           /* the row stride (byte) of the image */

#if !gcdENDIAN_BIG
        gctUINT   width          : 16;  /* the width of image (pixels) */
        gctUINT   height         : 16;  /* the height of image (rows) */
#else
        gctUINT   height         : 16;  /* the height of image (rows) */
        gctUINT   width          : 16;  /* the width of image (pixels) */
#endif

#if !gcdENDIAN_BIG
        gctUINT   shift          : 3;   /* Shift value for index. */
        gctUINT   multiply       : 1;   /* Value to multiply index with. */
        gctUINT   addressing     : 2;   /* Addressing mode for LOAD_IMG and STORE_IMG. */
        gctUINT   conversion     : 4;   /* Conversion format. */
        gctUINT   titling        : 2;   /* titling */
        gctUINT   image1Dor2D    : 1;   /* 1D or 2D image */
        gctUINT   imageId0       : 1;   /* ImageID bit0. */
        gctUINT   componentCount : 2;   /* Component count. */
        gctUINT   swizzleR       : 3;   /* swizzle for red */
        gctUINT   imageId1       : 1;   /* ImageID bit1. */
        gctUINT   swizzleG       : 3;   /* swizzle for green */
        gctUINT   imageId2       : 1;   /* ImageID bit2. */
        gctUINT   swizzleB       : 3;   /* swizzle for blue */
        gctUINT   reserved0      : 1;
        gctUINT   swizzleA       : 3;   /* swizzle for alpha */
        gctUINT   reserved1      : 1;
#else
        gctUINT   reserved1      : 1;
        gctUINT   swizzleA       : 3;   /* swizzle for alpha */
        gctUINT   reserved0      : 1;
        gctUINT   swizzleB       : 3;   /* swizzle for blue */
        gctUINT   imageId2       : 1;   /* ImageID bit2. */
        gctUINT   swizzleG       : 3;   /* swizzle for green */
        gctUINT   imageId1       : 1;   /* ImageID bit1. */
        gctUINT   swizzleR       : 3;   /* swizzle for red */
        gctUINT   componentCount : 2;   /* Component count. */
        gctUINT   imageId0       : 1;   /* ImageID bit0. */
        gctUINT   image1Dor2D    : 1;   /* 1D or 2D image */
        gctUINT   titling        : 2;   /* titling */
        gctUINT   conversion     : 4;   /* Conversion format. */
        gctUINT   addressing     : 2;   /* Addressing mode for LOAD_IMG and STORE_IMG. */
        gctUINT   multiply       : 1;   /* Value to multiply index with. */
        gctUINT   shift          : 3;   /* Shift value for index. */
#endif

        /* following data are used by SW to calculate 3D image slice image address
         * and image query data */
        gctUINT   sliceSize;            /* slice size for image 3D */

#if !gcdENDIAN_BIG
        gctUINT   depth_arraySize : 16; /* depth for image 3D, or array_size for image1D/2D array */
        gctUINT   imageType       : 16; /* vscImageValueType: 1D: 0, 1D_buffer: 1, 1D_array: 2, 2D: 3, 2D_array: 4, 3D: 5 */
#else
        gctUINT   imageType       : 16; /* vscImageValueType: 1D: 0, 1D_buffer: 1, 1D_array: 2, 2D: 3, 2D_array: 4, 3D: 5 */
        gctUINT   depth_arraySize : 16; /* depth for image 3D, or array_size for image1D/2D array */
#endif

#if !gcdENDIAN_BIG
        gctUINT   channelOrder    : 16; /* image channel order */
        gctUINT   channelDataType : 16; /* image channel data type */
#else
        gctUINT   channelDataType : 16; /* image channel data type */
        gctUINT   channelOrder    : 16; /* image channel order */
#endif

#if !gcdENDIAN_BIG
        gctUINT   imageValueType  : 2;  /* vscImageValueType (float/int/uint), filled by compiler */
        gctUINT   reserved2       : 30;
#else
        gctUINT   reserved2       : 30;
        gctUINT   imageValueType  : 2;  /* vscImageValueType (float/int/uint), filled by compiler */
#endif
    } sd;  /* structured data */
    gctUINT rawbits[8];
} VSC_ImageDesc;

typedef enum _VSC_SAMPLER_VALUE
{

    /* First byte: addressing mode. */
    VSC_IMG_SAMPLER_ADDRESS_NONE                = 0x00, /* (CL_ADDRESS_NONE            & 0xFF) */
    VSC_IMG_SAMPLER_ADDRESS_CLAMP_TO_EDGE       = 0x01, /* (CL_ADDRESS_CLAMP_TO_EDGE   & 0xFF),*/
    VSC_IMG_SAMPLER_ADDRESS_CLAMP               = 0x02, /* CL_ADDRESS_CLAMP            & 0xFF), */
    VSC_IMG_SAMPLER_ADDRESS_REPEAT              = 0x03, /* CL_ADDRESS_REPEAT           & 0xFF), */
    VSC_IMG_SAMPLER_ADDRESS_MIRRORED_REPEAT     = 0x04, /* CL_ADDRESS_MIRRORED_REPEAT  & 0xFF), */
    VSC_IMG_SAMPLER_ADDRESS_COUNT               = 0x05, /* the count of address mode */
    /* Second byte: filter mode. */
    VSC_IMG_SAMPLER_FILTER_NEAREST              = 0x0000, /* (CL_FILTER_NEAREST  & 0xFF00) << 8), */
    VSC_IMG_SAMPLER_FILTER_LINEAR               = 0x0100, /* (CL_FILTER_LINEAR   & 0xFF00) << 8), */

    /* Third byte: normalized coords. */
    VSC_IMG_SAMPLER_NORMALIZED_COORDS_FALSE     = 0x000000, /*0x0 << 16), */
    VSC_IMG_SAMPLER_NORMALIZED_COORDS_TRUE      = 0x010000, /*0x1 << 16)    */

    /* we treat int or float coordinate type as sampler value,
     * so the <image, sampler> pair will carry the int coordinate info
     * which is useful when construct image read lib function name
     */
    VSC_IMG_SAMPLER_INT_COORDS_FALSE            = 0x0000000, /*0x0 << 24), */
    VSC_IMG_SAMPLER_INT_COORDS_TRUE             = 0x1000000, /*0x1 << 24)    */

    VSC_IMG_SAMPLER_DEFAULT_VALUE               = VSC_IMG_SAMPLER_ADDRESS_NONE |
                                                  VSC_IMG_SAMPLER_FILTER_NEAREST |
                                                  VSC_IMG_SAMPLER_NORMALIZED_COORDS_FALSE |
                                                  VSC_IMG_SAMPLER_INT_COORDS_FALSE,
    VSC_IMG_SAMPLER_UNKNOWN_VALUE               = 0x7FFFFFFF, /* unkown sampler value marker */
    VSC_IMG_SAMPLER_INVALID_VALUE               = 0x7FFFFFFF, /* invalid value marker */
} VSC_SamplerValue;

#define VIR_IMG_isSamplerLinearFilter(sampler)      (((sampler)&((gctUINT)VSC_IMG_SAMPLER_FILTER_LINEAR)) != 0)
#define VIR_IMG_isSamplerNearestFilter(sampler)     (((sampler)&((gctUINT)VSC_IMG_SAMPLER_FILTER_LINEAR)) == 0)
#define VIR_IMG_isSamplerNormalizedCoords(sampler)  (((sampler)&((gctUINT)VSC_IMG_SAMPLER_NORMALIZED_COORDS_TRUE)) != 0)
#define VIR_IMG_isSamplerIntCoords(sampler)         (((sampler)&((gctUINT)VSC_IMG_SAMPLER_INT_COORDS_TRUE)) != 0)
#define VIR_IMG_GetSamplerAddressMode(sampler)      ((VSC_SamplerValue)((sampler)&0xFF))

typedef void* DRIVER_HANDLE;

/* Callback defintions. Note that first param of all drv-callbacks must be hDrv which
   designates the specific client driver who needs insure param hDrv of callbacks and
   VSC_SYS_CONTEXT::hDrv point to the same underlying true driver object (driver-contex/
   driver device/...) */
typedef void* (*PFN_ALLOC_VIDMEM_CB)(DRIVER_HANDLE hDrv,
                                     gceSURF_TYPE type,
                                     gctSTRING tag,
                                     gctSIZE_T size,
                                     gctUINT32 align,
                                     gctPOINTER* ppOpaqueNode,
                                     gctPOINTER* ppVirtualAddr,
                                     gctUINT32* pPhysicalAddr,
                                     gctPOINTER pInitialData,
                                     gctBOOL bZeroMemory);
typedef void* (*PFN_FREE_VIDMEM_CB)(DRIVER_HANDLE hDrv,
                                    gceSURF_TYPE type,
                                    gctSTRING tag,
                                    gctPOINTER pOpaqueNode);

typedef struct _VSC_DRV_CALLBACKS
{
    PFN_ALLOC_VIDMEM_CB pfnAllocVidMemCb;
    PFN_FREE_VIDMEM_CB  pfnFreeVidMemCb;
}VSC_DRV_CALLBACKS, *PVSC_DRV_CALLBACKS;

/* VSC hardware (chip) configuration that poses on (re)-compiling */
typedef struct _VSC_HW_CONFIG
{
    struct
    {
        /* word 0 */
        gctUINT          hasHalti0              : 1;
        gctUINT          hasHalti1              : 1;
        gctUINT          hasHalti2              : 1;
        gctUINT          hasHalti3              : 1;
        gctUINT          hasHalti4              : 1;
        gctUINT          hasHalti5              : 1;
        gctUINT          supportGS              : 1;
        gctUINT          supportTS              : 1;

        gctUINT          supportInteger         : 1;
        gctUINT          hasSignFloorCeil       : 1;
        gctUINT          hasSqrtTrig            : 1;
        gctUINT          hasNewSinCosLogDiv     : 1;
        gctUINT          canBranchOnImm         : 1;
        gctUINT          supportDual16          : 1;
        gctUINT          hasBugFix8             : 1;
        gctUINT          hasBugFix10            : 1;

        gctUINT          hasBugFix11            : 1;
        gctUINT          hasSelectMapSwizzleFix : 1;
        gctUINT          hasSamplePosSwizzleFix : 1;
        gctUINT          hasLoadAttrOOBFix      : 1;
        gctUINT          hasSampleMaskInR0ZWFix : 1;
        gctUINT          hasICacheAllocCountFix : 1;
        gctUINT          hasSHEnhance2          : 1;
        gctUINT          hasMediumPrecision     : 1;

        gctUINT          hasInstCache           : 1;
        gctUINT          hasInstCachePrefetch   : 1;
        gctUINT          instBufferUnified      : 1;
        /* Every single shader stage can use all constant registers. */
        gctUINT          constRegFileUnified    : 1;
        /* Every single shader stage can use all sampler registers. */
        gctUINT          samplerRegFileUnified  : 1;
        gctUINT          bigEndianMI            : 1;
        gctUINT          raPushPosW             : 1;
        gctUINT          vtxInstanceIdAsAttr    : 1;

        /* word 1 */
        gctUINT          vtxInstanceIdAsInteger : 1;
        gctUINT          gsSupportEmit          : 1;
        gctUINT          highpVaryingShift      : 1;
        gctUINT          needCLXFixes           : 1;
        gctUINT          needCLXEFixes          : 1;
        gctUINT          flatDual16Fix          : 1;
        gctUINT          supportEVIS            : 1;
        gctUINT          supportImgAtomic       : 1;

        gctUINT          supportAdvancedInsts   : 1;
        gctUINT          noOneConstLimit        : 1;
        gctUINT          hasUniformB0           : 1;
        gctUINT          supportOOBCheck        : 1;
        gctUINT          hasUniversalTexld      : 1;
        gctUINT          hasUniversalTexldV2    : 1;
        gctUINT          hasTexldUFix           : 1;
        gctUINT          canSrc0OfImgLdStBeTemp : 1;

        gctUINT          hasPSIOInterlock       : 1;
        gctUINT          support128BppImage     : 1;
        gctUINT          supportMSAATexture     : 1;
        gctUINT          supportPerCompDepForLS : 1;
        gctUINT          supportImgAddr         : 1;
        gctUINT          hasUscGosAddrFix       : 1;
        gctUINT          multiCluster           : 1;
        gctUINT          smallBatch             : 1;

        gctUINT          hasImageOutBoundaryFix : 1;
        gctUINT          supportTexldCoordOffset: 1;
        gctUINT          supportLSAtom          : 1;
        gctUINT          supportUnOrdBranch     : 1;
        gctUINT          supportPatchVerticesIn : 1;
        gctUINT          hasHalfDepFix          : 1;
        gctUINT          supportUSC             : 1;
        gctUINT          supportPartIntBranch   : 1;

        /* word 2 */
        gctUINT          supportIntAttrib       : 1;
        gctUINT          hasTxBiasLodFix        : 1;
        gctUINT          supportmovai           : 1;
        gctUINT          useGLZ                 : 1;
        gctUINT          supportHelperInv       : 1;
        gctUINT          supportAdvBlendPart0   : 1;
        gctUINT          supportStartVertexFE   : 1;
        gctUINT          supportTxGather        : 1;

        gctUINT          singlePipeHalti1       : 1;
        gctUINT          supportEVISVX2         : 1;
        gctUINT          computeOnly            : 1;
        gctUINT          hasBugFix7             : 1;
        gctUINT          hasExtraInst2          : 1;
        gctUINT          hasAtomic              : 1;
        gctUINT          supportFullIntBranch   : 1;
        /* All shader stages can use the same constant register at the same time. */
        gctUINT          supportUnifiedConstant : 1;

        /* All shader stages can use the same sampler register at the same time. */
        gctUINT          supportUnifiedSampler  : 1;
        gctUINT          support32BitIntDiv     : 1;
        gctUINT          supportFullCompIntDiv  : 1;
        gctUINT          supportComplex         : 1;
        gctUINT          supportBigEndianLdSt   : 1;
        gctUINT          supportUSCUnalloc      : 1;
        gctUINT          supportEndOfBBReissue  : 1;
        gctUINT          hasDynamicIdxDepFix    : 1;

        gctUINT          supportPSCSThrottle    : 1;
        gctUINT          hasLODQFix             : 1;
        gctUINT          supportHWManagedLS     : 1;
        gctUINT          hasScatteredMemAccess  : 1;
        gctUINT          supportImgLDSTClamp    : 1;
        gctUINT          useSrc0SwizzleAsSrcBin : 1;
        gctUINT          supportSeparatedTex    : 1;
        gctUINT          supportMultiGPU        : 1;

        /* word 3 */
        gctUINT          hasPointSizeFix        : 1;
        gctUINT          supportVectorB0        : 1;
        gctUINT          hasAtomTimingFix       : 1;
        gctUINT          hasUSCAtomicFix2       : 1;
        gctUINT          hasFloatingMadFix      : 1;
        gctUINT          hasA0WriteEnableFix    : 1;
        gctUINT          reserved1              : 26;

        /* Last word */
        /* Followings will be removed after shader programming is removed out of VSC */
        gctUINT          hasSHEnhance3          : 1;
        gctUINT          rtneRoundingEnabled    : 1;
        gctUINT          hasThreadWalkerInPS    : 1;
        gctUINT          newSteeringICacheFlush : 1;
        gctUINT          has32Attributes        : 1;
        gctUINT          hasSamplerBaseOffset   : 1;
        gctUINT          supportStreamOut       : 1;
        gctUINT          supportZeroAttrsInFE   : 1;

        gctUINT          outputCountFix         : 1;
        gctUINT          varyingPackingLimited  : 1;
        gctUINT          robustAtomic           : 1;
        gctUINT          newGPIPE               : 1;
        gctUINT          FEDrawDirect           : 1;
        gctUINT          reserved2              : 19;
    } hwFeatureFlags;

    gctUINT              chipModel;
    gctUINT              chipRevision;
    gctUINT              productID;
    gctUINT              customerID;
    gctUINT              maxCoreCount;
    gctUINT              maxClusterCount;
    gctUINT              maxThreadCountPerCore;
    gctUINT              maxVaryingCount;
    gctUINT              maxAttributeCount;
    gctUINT              maxRenderTargetCount;
    gctUINT              maxShaderCountPerCore;
    gctUINT              maxGPRCountPerCore;
    gctUINT              maxGPRCountPerShader;
    gctUINT              maxGPRCountPerThread;
    gctUINT              maxHwNativeTotalInstCount;
    gctUINT              maxTotalInstCount;
    gctUINT              maxVSInstCount;
    gctUINT              maxPSInstCount;
    gctUINT              maxHwNativeTotalConstRegCount;
    gctUINT              maxTotalConstRegCount;
    gctUINT              unifiedConst;
    gctUINT              maxVSConstRegCount;
    gctUINT              maxTCSConstRegCount;  /* HS */
    gctUINT              maxTESConstRegCount;  /* DS */
    gctUINT              maxGSConstRegCount;
    gctUINT              maxPSConstRegCount;
    gctUINT              vsSamplerRegNoBase;
    gctUINT              tcsSamplerRegNoBase;  /* HS */
    gctUINT              tesSamplerRegNoBase;  /* DS */
    gctUINT              gsSamplerRegNoBase;
    gctUINT              psSamplerRegNoBase;
    gctUINT              csSamplerRegNoBase;
    gctUINT              maxVSSamplerCount;
    gctUINT              maxTCSSamplerCount;   /* HS */
    gctUINT              maxTESSamplerCount;   /* DS */
    gctUINT              maxGSSamplerCount;
    gctUINT              maxPSSamplerCount;
    gctUINT              maxCSSamplerCount;
    gctUINT              maxHwNativeTotalSamplerCount;
    gctUINT              maxSamplerCountPerShader;
    gctUINT              maxUSCAttribBufInKbyte;     /* usc size for non-cache part */
    gctUINT              maxLocalMemSizeInByte; /* local memory size */
    gctUINT              maxResultCacheWinSize;
    gctUINT              vsSamplerNoBaseInInstruction;
    gctUINT              psSamplerNoBaseInInstruction;
    gctFLOAT             minPointSize;
    gctFLOAT             maxPointSize;
    gctUINT              maxTcsOutPatchVectors;

    /* Caps for workGroupSize. */
    gctUINT              initWorkGroupSizeToCalcRegCount;
    gctUINT              maxWorkGroupSize;
    gctUINT              minWorkGroupSize;

    /* Followings will be removed after shader programming is removed out of VSC */
    gctUINT              vsInstBufferAddrBase;
    gctUINT              psInstBufferAddrBase;
    gctUINT              vsConstRegAddrBase;
    gctUINT              tcsConstRegAddrBase;
    gctUINT              tesConstRegAddrBase;
    gctUINT              gsConstRegAddrBase;
    gctUINT              psConstRegAddrBase;
    gctUINT              vertexOutputBufferSize;
    gctUINT              vertexCacheSize;
    gctUINT              ctxStateCount;
}VSC_HW_CONFIG, *PVSC_HW_CONFIG;

typedef gcsGLSLCaps VSC_GL_API_CONFIG, *PVSC_GL_API_CONFIG;

/* VSC supported optional opts */
#define VSC_COMPILER_OPT_NONE                           0x0000000000000000ULL

#define VSC_COMPILER_OPT_ALGE_SIMP                      0x0000000000000001ULL
#define VSC_COMPILER_OPT_GCP                            0x0000000000000002ULL
#define VSC_COMPILER_OPT_LCP                            0x0000000000000004ULL
#define VSC_COMPILER_OPT_LCSE                           0x0000000000000008ULL
#define VSC_COMPILER_OPT_DCE                            0x0000000000000010ULL
#define VSC_COMPILER_OPT_PRE                            0x0000000000000020ULL
#define VSC_COMPILER_OPT_PEEPHOLE                       0x0000000000000040ULL
#define VSC_COMPILER_OPT_CONSTANT_PROPOGATION           0x0000000000000080ULL
#define VSC_COMPILER_OPT_CONSTANT_FOLDING               0x0000000000000100ULL
#define VSC_COMPILER_OPT_FUNC_INLINE                    0x0000000000000200ULL
#define VSC_COMPILER_OPT_INST_SKED                      0x0000000000000400ULL
#define VSC_COMPILER_OPT_GPR_SPILLABLE                  0x0000000000000800ULL
#define VSC_COMPILER_OPT_CONSTANT_REG_SPILLABLE         0x0000000000001000ULL
#define VSC_COMPILER_OPT_VEC                            0x0000000000002000ULL /* Including logic io packing */
#define VSC_COMPILER_OPT_IO_PACKING                     0x0000000000004000ULL /* Physical io packing */
#define VSC_COMPILER_OPT_FULL_ACTIVE_IO                 0x0000000000008000ULL
#define VSC_COMPILER_OPT_DUAL16                         0x0000000000010000ULL
#define VSC_COMPILER_OPT_ILF_LINK                       0x0000000000020000ULL
#define VSC_COMPILER_OPT_LOOP                           0x0000000000040000ULL
#define VSC_COMPILER_OPT_SCPP                           0x0000000000080000ULL
#define VSC_COMPILER_OPT_CPF                            0x0000000000100000ULL

#define VSC_COMPILER_OPT_FULL                           0x00000000001FFFFFULL

#define VSC_COMPILER_OPT_NO_ALGE_SIMP                   0x0000000100000000ULL
#define VSC_COMPILER_OPT_NO_GCP                         0x0000000200000000ULL
#define VSC_COMPILER_OPT_NO_LCP                         0x0000000400000000ULL
#define VSC_COMPILER_OPT_NO_LCSE                        0x0000000800000000ULL
#define VSC_COMPILER_OPT_NO_DCE                         0x0000001000000000ULL
#define VSC_COMPILER_OPT_NO_PRE                         0x0000002000000000ULL
#define VSC_COMPILER_OPT_NO_PEEPHOLE                    0x0000004000000000ULL
#define VSC_COMPILER_OPT_NO_CONSTANT_PROPOGATION        0x0000008000000000ULL
#define VSC_COMPILER_OPT_NO_CONSTANT_FOLDING            0x0000010000000000ULL
#define VSC_COMPILER_OPT_NO_FUNC_INLINE                 0x0000020000000000ULL
#define VSC_COMPILER_OPT_NO_INST_SKED                   0x0000040000000000ULL
#define VSC_COMPILER_OPT_NO_GPR_SPILLABLE               0x0000080000000000ULL
#define VSC_COMPILER_OPT_NO_CONSTANT_REG_SPILLABLE      0x0000100000000000ULL
#define VSC_COMPILER_OPT_NO_VEC                         0x0000200000000000ULL /* Including logic io packing */
#define VSC_COMPILER_OPT_NO_IO_PACKING                  0x0000400000000000ULL /* Physical io packing */
#define VSC_COMPILER_OPT_NO_FULL_ACTIVE_IO              0x0000800000000000ULL
#define VSC_COMPILER_OPT_NO_DUAL16                      0x0001000000000000ULL
#define VSC_COMPILER_OPT_NO_ILF_LINK                    0x0002000000000000ULL
#define VSC_COMPILER_OPT_NO_LOOP                        0x0004000000000000ULL
#define VSC_COMPILER_OPT_NO_SCPP                        0x0008000000000000ULL
#define VSC_COMPILER_OPT_NO_CPF                         0x0010000000000000ULL

#define VSC_COMPILER_OPT_NO_OPT                         0x001FFFFF00000000ULL

/* Compiler flag for special purpose */
#define VSC_COMPILER_FLAG_COMPILE_TO_HL                0x00000001   /* Compile IR to HL, including doing all opts in HL */
#define VSC_COMPILER_FLAG_COMPILE_TO_ML                0x00000002   /* Compile IR to ML, including doing all opts in HL&ML */
#define VSC_COMPILER_FLAG_COMPILE_TO_LL                0x00000004   /* Compile IR to LL, including doing all opts in HL&ML&LL */
#define VSC_COMPILER_FLAG_COMPILE_TO_MC                0x00000008   /* Compile IR to MC, including doing all opts in all levels */
#define VSC_COMPILER_FLAG_COMPILE_CODE_GEN             0x00000010
#define VSC_COMPILER_FLAG_SEPERATED_SHADERS            0x00000020
#define VSC_COMPILER_FLAG_UNI_UNIFORM_UNIFIED_ALLOC    0x00000040
#define VSC_COMPILER_FLAG_UNI_SAMPLER_UNIFIED_ALLOC    0x00000080
#define VSC_COMPILER_FLAG_UNI_UNIFORM_FIXED_BASE_ALLOC 0x00000100
#define VSC_COMPILER_FLAG_UNI_SAMPLER_FIXED_BASE_ALLOC 0x00000200
#define VSC_COMPILER_FLAG_NEED_OOB_CHECK               0x00000400
#define VSC_COMPILER_FLAG_FLUSH_DENORM_TO_ZERO         0x00000800
#define VSC_COMPILER_FLAG_WAVIER_RESLAYOUT_COMPATIBLE  0x00001000   /* Vulkan only for resource layout is provided */
#define VSC_COMPILER_FLAG_NEED_RTNE_ROUNDING           0x00002000
#define VSC_COMPILER_FLAG_API_UNIFORM_PRECISION_CHECK  0x00004000
#define VSC_COMPILER_FLAG_LINK_PROGRAM_PIPELINE_OBJ    0x00008000
#define VSC_COMPILER_FLAG_RECOMPILER                   0x00010000
#define VSC_COMPILER_FLAG_USE_VSC_IMAGE_DESC           0x00020000
#define VSC_COMPILER_FLAG_ENABLE_MULTI_GPU             0x00040000
#define VSC_COMPILER_FLAG_DISABLE_IR_DUMP              0x00080000  /* used by driver to disable patch lib IR dump */
#define VSC_COMPILER_FLAG_ADD_GLOBAL_OFFSET            0x00100000  /* gl_GlobalInvocationID = gl_GlobalInvocationID + #global_offset. */
#define VSC_COMPILER_FLAG_ENABLE_DUAL16_FOR_VK         0x00200000  /* It is a temp option to enable dual16 for vulkan. we need to remove after verify all vulkan cases. */
#define VSC_COMPILER_FLAG_USE_CONST_REG_FOR_UBO        0x00400000
#define VSC_COMPILER_FLAG_FORCE_GEN_FLOAT_MAD          0x00800000  /* Force generate a floating MAD, no matter if HW can support it. */

#define VSC_COMPILER_FLAG_COMPILE_FULL_LEVELS          0x0000000F

#define VSC_MAX_GFX_SHADER_STAGE_COUNT                 5
#define VSC_MAX_CPT_SHADER_STAGE_COUNT                 1
#define VSC_MAX_HW_PIPELINE_SHADER_STAGE_COUNT         VSC_MAX_GFX_SHADER_STAGE_COUNT
#define VSC_MAX_LINKABLE_SHADER_STAGE_COUNT            VSC_MAX_HW_PIPELINE_SHADER_STAGE_COUNT
#define VSC_MAX_SHADER_STAGE_COUNT                     (VSC_MAX_GFX_SHADER_STAGE_COUNT + VSC_MAX_CPT_SHADER_STAGE_COUNT)

/* For indexing VSC_MAX_SHADER_STAGE_COUNT */
#define VSC_SHADER_STAGE_VS                            0
#define VSC_SHADER_STAGE_HS                            1
#define VSC_SHADER_STAGE_DS                            2
#define VSC_SHADER_STAGE_GS                            3
#define VSC_SHADER_STAGE_PS                            4
#define VSC_SHADER_STAGE_CS                            5

/* This means stage (type) of shader is not known, and can not be directly flushed down
   to HW. Normally, this shader might be a combination of several shader stage, or a
   portion of a shader stage, or functional lib that might be linked to another shader.
   So VSC_SHADER_STAGE_UNKNOWN is not counted by VSC_MAX_SHADER_STAGE_COUNT */
#define VSC_SHADER_STAGE_UNKNOWN                       0xFF

#define VSC_SHADER_STAGE_2_STAGE_BIT(shStage)          (1 << (shStage))

typedef enum _VSC_SHADER_STAGE_BIT
{
    VSC_SHADER_STAGE_BIT_VS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_VS),
    VSC_SHADER_STAGE_BIT_HS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_HS),
    VSC_SHADER_STAGE_BIT_DS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_DS),
    VSC_SHADER_STAGE_BIT_GS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_GS),
    VSC_SHADER_STAGE_BIT_PS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_PS),
    VSC_SHADER_STAGE_BIT_CS = VSC_SHADER_STAGE_2_STAGE_BIT(VSC_SHADER_STAGE_CS),
}
VSC_SHADER_STAGE_BIT;

/* For indexing VSC_MAX_GFX_SHADER_STAGE_COUNT, VSC_MAX_HW_PIPELINE_SHADER_STAGE_COUNT
   and VSC_MAX_LINKABLE_SHADER_STAGE_COUNT */
#define VSC_GFX_SHADER_STAGE_VS                        VSC_SHADER_STAGE_VS
#define VSC_GFX_SHADER_STAGE_HS                        VSC_SHADER_STAGE_HS
#define VSC_GFX_SHADER_STAGE_DS                        VSC_SHADER_STAGE_DS
#define VSC_GFX_SHADER_STAGE_GS                        VSC_SHADER_STAGE_GS
#define VSC_GFX_SHADER_STAGE_PS                        VSC_SHADER_STAGE_PS
#define VSC_CPT_SHADER_STAGE_CS                        0

typedef void* SHADER_HANDLE;
typedef void* VSC_PRIV_DATA_HANDLE;

typedef enum _VSC_SHADER_RESOURCE_TYPE
{
    VSC_SHADER_RESOURCE_TYPE_SAMPLER                = 0, /* s#, sampler         */
    VSC_SHADER_RESOURCE_TYPE_COMBINED_IMAGE_SAMPLER = 1, /* s#/t#, sampler2D       */
    VSC_SHADER_RESOURCE_TYPE_SAMPLED_IMAGE          = 2, /* t#, texture2D       */
    VSC_SHADER_RESOURCE_TYPE_STORAGE_IMAGE          = 3, /* u#, image2D         */
    VSC_SHADER_RESOURCE_TYPE_UNIFORM_TEXEL_BUFFER   = 4, /* t#, samplerBuffer   */
    VSC_SHADER_RESOURCE_TYPE_STORAGE_TEXEL_BUFFER   = 5, /* u#, imageBuffer     */
    VSC_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER         = 6, /* cb#, uniform 'block' */
    VSC_SHADER_RESOURCE_TYPE_STORAGE_BUFFER         = 7, /* u#, buffer 'block'  */
    VSC_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER_DYNAMIC = 8, /* cb#, uniform 'block' */
    VSC_SHADER_RESOURCE_TYPE_STORAGE_BUFFER_DYNAMIC = 9, /* u#, buffer 'block'  */
    VSC_SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT       = 10,/* t#, subpass         */
} VSC_SHADER_RESOURCE_TYPE;

typedef struct _VSC_SHADER_RESOURCE_BINDING
{
    VSC_SHADER_RESOURCE_TYPE          type;
    gctUINT32                         set;        /* Set-No in pResourceSets array */
    gctUINT32                         binding;
    gctUINT32                         arraySize;  /* 1 means not an array */
}VSC_SHADER_RESOURCE_BINDING;

typedef struct _VSC_PROGRAM_RESOURCE_BINDING
{
    VSC_SHADER_RESOURCE_BINDING       shResBinding;
    VSC_SHADER_STAGE_BIT              stageBits;  /* Which shader stages could access this resource */
}VSC_PROGRAM_RESOURCE_BINDING;

typedef struct _VSC_PROGRAM_RESOURCE_SET
{
    gctUINT32                         resourceBindingCount;
    VSC_PROGRAM_RESOURCE_BINDING*     pResouceBindings;
}VSC_PROGRAM_RESOURCE_SET;

typedef struct _VSC_SHADER_PUSH_CONSTANT_RANGE
{
    gctUINT32                         offset;
    gctUINT32                         size;
} VSC_SHADER_PUSH_CONSTANT_RANGE;

typedef struct _VSC_PROGRAM_PUSH_CONSTANT_RANGE
{
    VSC_SHADER_PUSH_CONSTANT_RANGE    shPushConstRange;
    VSC_SHADER_STAGE_BIT              stageBits; /* Which shader stages could access this push-constant */
} VSC_PROGRAM_PUSH_CONSTANT_RANGE;

/* Resource layout */
typedef struct _VSC_PROGRAM_RESOURCE_LAYOUT
{
    gctUINT32                         resourceSetCount; /* How many descritor set? */
    VSC_PROGRAM_RESOURCE_SET*         pResourceSets;

    gctUINT32                         pushConstantRangeCount;
    VSC_PROGRAM_PUSH_CONSTANT_RANGE*  pPushConstantRanges;
} VSC_PROGRAM_RESOURCE_LAYOUT;

typedef struct _VSC_SHADER_RESOURCE_LAYOUT
{
    gctUINT32                         resourceBindingCount;
    VSC_SHADER_RESOURCE_BINDING*      pResBindings;

    gctUINT32                         pushConstantRangeCount;
    VSC_SHADER_PUSH_CONSTANT_RANGE*   pPushConstantRanges;
}VSC_SHADER_RESOURCE_LAYOUT;

/* Some special HW settings which are maintained by driver's adapter/device. */
typedef struct _VSC_SPECIFIC_HW_SETTING
{
    /* How many clusters are enabled. */
    gctUINT32                        activeClusterCount;
}VSC_SPECIFIC_HW_SETTING;

/* In general, a core system contex is maintained by driver's adapter/device who can
   designate a GPU chip, which means core system contex is GPU wide global context. */
typedef struct _VSC_CORE_SYS_CONTEXT
{
    /* Designates a target HW */
    VSC_HW_CONFIG                     hwCfg;

    /* Specific HW setting. */
    VSC_SPECIFIC_HW_SETTING           specificHwSetting;

    /* VSC private data, maintained by vscCreatePrivateData and vscDestroyPrivateData */
    VSC_PRIV_DATA_HANDLE              hPrivData;
}VSC_CORE_SYS_CONTEXT, *PVSC_CORE_SYS_CONTEXT;

/* A system context is combination of core system contex and driver callbacks which can
   be driver context/device/... wide based on each driver's architecture */
typedef struct _VSC_SYS_CONTEXT
{
    PVSC_CORE_SYS_CONTEXT             pCoreSysCtx;

    /* Driver system callbacks. When VSC is fully decoupled with driver later,
       no need this anymore. Note hDrv designates driver where callbacks are
       called to and all callbacks' first param must be this hDrv */
    DRIVER_HANDLE                     hDrv;
    VSC_DRV_CALLBACKS                 drvCBs;
}VSC_SYS_CONTEXT, *PVSC_SYS_CONTEXT;

/* Lib-link inerface definitions */
#include "gc_vsc_drvi_lib_link.h"

/* Output profiles definitions */
#include "gc_vsc_drvi_shader_priv_mapping.h"
#include "gc_vsc_drvi_shader_profile.h"
#include "gc_vsc_drvi_program_profile.h"
#include "gc_vsc_drvi_kernel_profile.h"

/* A context designate an enviroment where a shader is going to be compiled */
typedef struct _VSC_CONTEXT
{
    /* Designate invoking client driver */
    gceAPI                            clientAPI;
    gcePATCH_ID                       appNameId;
    gctBOOL                           isPatchLib;

    /* System wide context */
    PVSC_SYS_CONTEXT                  pSysCtx;
}VSC_CONTEXT, *PVSC_CONTEXT;

/* VSC compiler configuration */
typedef struct _VSC_COMPILER_CONFIG
{
    VSC_CONTEXT                       ctx;

    /* Compiler controls */
    gctUINT                           cFlags;
    gctUINT64                         optFlags;
}VSC_COMPILER_CONFIG, *PVSC_COMPILER_CONFIG;

/* Shader compiler parameter for VSC compiler entrance. */
typedef struct _VSC_SHADER_COMPILER_PARAM
{
    VSC_COMPILER_CONFIG               cfg;

    SHADER_HANDLE                     hShader;
    VSC_SHADER_RESOURCE_LAYOUT*       pShResourceLayout; /* Vulkan ONLY */

    /* For static compilation, when it is set (as static lib), pInMasterSEP must
       be set to NULL and hShader should be the original shader.

       For recompilaton, when it is set (as dynamic lib), pInMasterSEP must NOT be
       set to NULL and hShader should be the combination of original shader and static
       lib passed in when static compilation */
    VSC_SHADER_LIB_LINK_TABLE*        pShLibLinkTable;

    SHADER_EXECUTABLE_PROFILE*        pInMasterSEP; /* For recompilation ONLY */
}VSC_SHADER_COMPILER_PARAM, *PVSC_SHADER_COMPILER_PARAM;

/* Program linker parameter for VSC linker entrance. */
typedef struct _VSC_PROGRAM_LINKER_PARAM
{
    VSC_COMPILER_CONFIG               cfg;
    PVSC_GL_API_CONFIG                pGlApiCfg;

    SHADER_HANDLE                     hShaderArray[VSC_MAX_SHADER_STAGE_COUNT];
    VSC_PROGRAM_RESOURCE_LAYOUT*      pPgResourceLayout; /* Vulkan ONLY */

    /* See comment of pShLibLinkTable in VSC_SHADER_COMPILER_PARAM */
    VSC_PROG_LIB_LINK_TABLE*          pProgLibLinkTable;

    PROGRAM_EXECUTABLE_PROFILE*       pInMasterPEP; /* For recompilation ONLY */
}VSC_PROGRAM_LINKER_PARAM, *PVSC_PROGRAM_LINKER_PARAM;

/* Kernel program linker parameter for VSC linker entrance. */
typedef struct VSC_KERNEL_PROGRAM_LINKER_PARAM
{
    VSC_COMPILER_CONFIG               cfg;

    SHADER_HANDLE*                    pKrnlModuleHandlesArray;
    gctUINT                           moduleCount;

    /* See comment of pShLibLinkTable in VSC_SHADER_COMPILER_PARAM */
    VSC_SHADER_LIB_LINK_TABLE*        pShLibLinkTable;

    KERNEL_EXECUTABLE_PROFILE**       ppInMasterKEPArray; /* For recompilation ONLY */
}VSC_KERNEL_PROGRAM_LINKER_PARAM, *PVSC_KERNEL_PROGRAM_LINKER_PARAM;

/* HW pipeline shaders linker parameter */
typedef struct _VSC_HW_PIPELINE_SHADERS_PARAM
{
    PVSC_SYS_CONTEXT                  pSysCtx;

    SHADER_EXECUTABLE_PROFILE*        pSEPArray[VSC_MAX_HW_PIPELINE_SHADER_STAGE_COUNT];
}VSC_HW_PIPELINE_SHADERS_PARAM, *PVSC_HW_PIPELINE_SHADERS_PARAM;

/* Returned HW linker info which will be used for states programming */
typedef struct _VSC_HW_SHADERS_LINK_INFO
{
    SHADER_HW_INFO                    shHwInfoArray[VSC_MAX_HW_PIPELINE_SHADER_STAGE_COUNT];
}VSC_HW_SHADERS_LINK_INFO, *PVSC_HW_SHADERS_LINK_INFO;

/* A state buffer that hold all shaders programming for GPU kickoff */
typedef struct _VSC_HW_PIPELINE_SHADERS_STATES
{
    gctUINT32                         stateBufferSize;
    void*                             pStateBuffer;

    /* It is DEPRECATED if driver directly uses EPs (PEP/KEP/SEP) as all
       the info stored in hints can be retrieved by SEP */
    struct _gcsHINT                   hints;

    gcsPROGRAM_VidMemPatchOffset      patchOffsetsInDW;
    gctUINT32                         stateDeltaSize;
    gctUINT32*                        pStateDelta;

}VSC_HW_PIPELINE_SHADERS_STATES, *PVSC_HW_PIPELINE_SHADERS_STATES;

gceSTATUS vscInitializeHwPipelineShadersStates(VSC_SYS_CONTEXT* pSysCtx, VSC_HW_PIPELINE_SHADERS_STATES* pHwShdsStates);
gceSTATUS vscFinalizeHwPipelineShadersStates(VSC_SYS_CONTEXT* pSysCtx, VSC_HW_PIPELINE_SHADERS_STATES* pHwShdsStates);

/* VSC private data creater/destroyer, every client should call it at least at
   driver device granularity  */
gceSTATUS vscCreatePrivateData(VSC_CORE_SYS_CONTEXT* pCoreSysCtx, VSC_PRIV_DATA_HANDLE* phOutPrivData, gctBOOL bForOCL);
gceSTATUS vscDestroyPrivateData(VSC_CORE_SYS_CONTEXT* pCoreSysCtx, VSC_PRIV_DATA_HANDLE hPrivData);

/* Create a shader with content unfilled. In general, driver calls this
   function to create a shader, and then pass this shader handle to shader
   generator. Or alternatively, shader generator can directly call this
   function to create a shader in mem inside and return it to driver, then
   driver dont need call it. To all these two cases, driver must call
   vscDestroyShader when driver does not need it. */
gceSTATUS vscCreateShader(SHADER_HANDLE* pShaderHandle,
                          gctUINT shStage);
gceSTATUS vscDestroyShader(SHADER_HANDLE hShader);

/* Print (dump) shader */
gceSTATUS vscPrintShader(SHADER_HANDLE hShader,
                         gctFILE hFile,
                         gctCONST_STRING strHeaderMsg,
                         gctBOOL bPrintHeaderFooter);

/* Shader binary saver and loader */

/* vscSaveShaderToBinary()
 * Two ways to save shader binary:
 * 1) compiler allocates memory and return the memory in *pBinary and size in pSizeInByte
 *    if (* pBinary) is NULL when calling the function
 * 2) user query the shader binary size with vscQueryShaderBinarySize() and allocate
 *    memory in (*pBinary), size in pSizeInByte
 */
gceSTATUS vscSaveShaderToBinary(SHADER_HANDLE hShader,
                                void**        pBinary,
                                gctUINT*      pSizeInByte);
/* vscLoadShaderFromBinary()
 * restore shader from pBinary which should be serialized by vscSaveShaderToBinary()
 * if bFreeBinary is true, pBinary is deallocated by this function
 */
gceSTATUS vscLoadShaderFromBinary(void*          pBinary,
                                  gctUINT        sizeInByte,
                                  SHADER_HANDLE* pShaderHandle,
                                  gctBOOL        bFreeBinary);

/* Free the vir intrinsic library. */
gceSTATUS vscFreeVirIntrinsicLib(void);

gceSTATUS vscQueryShaderBinarySize(SHADER_HANDLE hShader, gctUINT* pSizeInByte);

gctPOINTER vscGetDebugInfo(IN SHADER_HANDLE    Shader);
gctPOINTER vscDupDebugInfo(IN SHADER_HANDLE    Shader);
gceSTATUS  vscDestroyDebugInfo(IN gctPOINTER DebugInfo);

/* Shader copy */
gceSTATUS vscCopyShader(SHADER_HANDLE * hToShader, SHADER_HANDLE hFromShader);

/* It will extract a specific sub-shader from main shader. It will be mainly used by CL to pick specific kernel
   from kernel program. */
gceSTATUS vscExtractSubShader(SHADER_HANDLE   hMainShader,
                              gctCONST_STRING pSubShaderEntryName,
                              SHADER_HANDLE   hSubShader);

gcSHADER_KIND vscGetShaderKindFromShaderHandle(SHADER_HANDLE hShader);

/* Link a lib shader to main shader. */
gceSTATUS vscLinkLibShaderToShader(SHADER_HANDLE              hMainShader,
                                   VSC_SHADER_LIB_LINK_TABLE* pShLibLinkTable);

/* DX/CL driver will call this directly, GL will call this directly after FE
   or inside of vscLinkProgram. After successfully shader compilation, a SEP
   may be generated based on cFlags */
gceSTATUS vscCompileShader(VSC_SHADER_COMPILER_PARAM* pCompilerParam,
                           SHADER_EXECUTABLE_PROFILE* pOutSEP /* Caller should destory the mem,
                                                                 so dont use VSC MM to allocate
                                                                 inside of VSC. */
                          );

/* GL/Vulkan driver ONLY interface, this is HL interface to match glLinkProgram
   API. It may call vscCompileShader for each shader inside. After successfully
   program linking, a PEP will be generated. Also program states will auto be
   generated at this time if driver request. If not, driver may want program
   states by itself.  */
gceSTATUS vscLinkProgram(VSC_PROGRAM_LINKER_PARAM*       pPgLinkParam,
                         PROGRAM_EXECUTABLE_PROFILE*     pOutPEP, /* Caller should destory the mem, */
                         VSC_HW_PIPELINE_SHADERS_STATES* pOutPgStates /* so for these two, dont use VSC
                                                                         MM to allocate inside of VSC. */
                        );

/* CL driver ONLY interface, this is HL interface to match clCreateKernel API
   if clBuildProgram does not directly generate MC, or to be called inside of
   vscLinkKernelProgram matched with clLinkProgram/clBuildProgram if it want
   to directly generate MC. It will call vscCompileShader inside. After succ-
   essfully creating, a KEP will be generated. Also kernel states will auto be
   generated at this time if driver request. If not, driver may want program
   states by itself. */
gceSTATUS vscCreateKernel(VSC_SHADER_COMPILER_PARAM*      pCompilerParam,
                          KERNEL_EXECUTABLE_PROFILE*      pOutKEP, /* Caller should destory the mem, */
                          VSC_HW_PIPELINE_SHADERS_STATES* pOutKrnlStates /* so for these two, dont use VSC
                                                                            MM to allocate inside of VSC. */
                         );

/* CL driver ONLY interface, this is HL interface to match clLinkProgram or
   clBuildProgram API. It will call vscCreateKernel inside. After successfully
   creating, a KEP list will be generated to match each kernel. Also kernel
   states array matched each kernel will auto be generated at this time
   if driver request. If not, driver may want program states by itself. */
gceSTATUS vscLinkKernelProgram(VSC_KERNEL_PROGRAM_LINKER_PARAM*  pKrnlPgLinkParam,
                               gctUINT*                          pKernelCount,
                               SHADER_HANDLE                     hOutLinkedProgram,
                               KERNEL_EXECUTABLE_PROFILE**       ppOutKEPArray, /* Caller should destory the mem, */
                               VSC_HW_PIPELINE_SHADERS_STATES**  ppOutKrnlStates      /* so for these two, dont use VSC
                                                                                         MM to allocate inside of VSC. */
                              );

/* This is hw level link that every client should call before programming */
gceSTATUS vscLinkHwShaders(VSC_HW_PIPELINE_SHADERS_PARAM*  pHwPipelineShsParam,
                           VSC_HW_SHADERS_LINK_INFO*       pOutHwShdsLinkInfo,
                           gctBOOL                         bSeperatedShaders);

/* All clients can call this function when drawing/dispatching/kickoffing to program
   shaders stages that current pipeline uses to launch GPU task in cores. For example,
   driver may send program-pipeline which holds all shaders that pipeline needs to ask
   for programming. After success, hw pipeline shaders states will be generated. If
   driver want do program by itself, then it wont be called. */
gceSTATUS vscProgramHwShaderStages(VSC_HW_PIPELINE_SHADERS_PARAM*   pHwPipelineShsParam,
                                   VSC_HW_PIPELINE_SHADERS_STATES*  pOutHwShdsStates, /* Caller should destory the mem,
                                                                                           so dont use VSC MM to allocate
                                                                                           inside of VSC. */
                                   gctBOOL                          bSeperatedShaders   /* It is a temporary fix,
                                                                                           we need to remove it later. */
                                  );

/****************************************************************************
   Following are for future EP, including SEP, KEP and PEP.
*****************************************************************************/
typedef enum _VSC_EP_KIND
{
    VSC_EP_KIND_NONE            = 0,
    /* Shader executable profile. */
    VSC_EP_KIND_SHADER          = 1,
    /* Kernel executable profile. */
    VSC_EP_KIND_KERNEL          = 2,
    /* Program executable profile. */
    VSC_EP_KIND_PROGRAM         = 3,
}VSC_EP_KIND;

/* For saver, it always returns how many bytes it saves to binary buffer, and
              1). if ppOutBinary == NULL (szBinaryInByte will be igored), then not doing real saving, just return byte size
              2). if ppOutBinary != NULL and *ppOutBinary == NULL (szBinaryInByte will be igored), then saver will allocate a
                  binary for user.
              3). *ppOutBinary != NULL, then szBinaryInByte must be the size of the binary (generally, this size got by first usage
                  of this function).
   For loader, szBinaryInByte must be size of binary, and return value is the real sparsed size of binary. */
gctUINT vscSaveEPToBinary(VSC_EP_KIND epKind, void* pEP, void** ppOutBinary, gctUINT szBinaryInByte);
gctUINT vscLoadEPFromBinary(VSC_EP_KIND epKind, void* pInBinary, gctUINT szBinaryInByte, void* pEP);

/****************************************************************************
   Following are for future MC level recompiling. Right now, we are using HL
   recompiling, so these are not used now.
*****************************************************************************/

/* VSC recompiler configuration */
typedef struct _VSC_RECOMPILER_CONFIG
{
    VSC_CONTEXT                       ctx;

    /* Recompiler controls */
    gctUINT                           rcFlags;
}VSC_RECOMPILER_CONFIG, *PVSC_RECOMPILER_CONFIG;

/* Shader recompiler parameter for VSC recompiler entrance. */
typedef struct _VSC_SHADER_RECOMPILER_PARAM
{
    VSC_RECOMPILER_CONFIG             cfg;
}VSC_SHADER_RECOMPILER_PARAM, *PVSC_SHADER_RECOMPILER_PARAM;

gceSTATUS
vscConvertGcShader2VirShader(
    IN  SHADER_HANDLE           GcShader,
    OUT SHADER_HANDLE*          phVirShader
    );

/* For given image descriptor and sampler value for HW cfg, do we
 * need to do recompilation for the image read ? */
gctBOOL
vscImageSamplerNeedLibFuncForHWCfg(
    void *                  pImageDesc,
    gctUINT                 ImageSamplerValue,
    VSC_HW_CONFIG *         pHwCfg,
    VSC_OCLImgLibKind *     pImgLibKind, /* the image lib kind to be used */
    gctBOOL *               UseTexld, /* set true if need to use texld for image_read */
    gctUINT *               KeyofImgSampler         /* the key state of the image-sampler pair */
    );

/* For given image descriptorfor HW cfg, do we
 * need to do recompilation for the image write ?  */
gctBOOL
vscImageWriteNeedLibFuncForHWCfg(
    void *                  pImageDesc,
    VSC_HW_CONFIG *         pHwCfg,
    VSC_OCLImgLibKind *     pImgLibKind, /* the image lib kind to be used */
    gctUINT *               KeyofImgSampler
    );

gceSTATUS
vscConstructImageReadLibFuncName(
    VSC_ImageDesc *         ImageDescHandle, /* VSC_ImageDesc */
    gctUINT                 SamplerValue, /* VSC_SamplerValue */
    VSC_HW_CONFIG *         pHwCfg,
    gctSTRING *             pLibFuncName, /* returned lib function name if needed */
    VSC_OCLImgLibKind *     pImgLibKind, /* the image lib kind to be used */
    gctBOOL *               UseTexld                /* set true if need to use texld for image_read */
    );

gceSTATUS
vscConstructImageWriteLibFuncName(
    VSC_ImageDesc *         ImageDescHandle, /* VSC_ImageDesc */
    VSC_HW_CONFIG *         pHwCfg,
    gctSTRING *             pLibFuncName, /* returned lib function name if needed */
    VSC_OCLImgLibKind *     pImgLibKind             /* the image lib kind to be used */
);

VSC_OCLImgLibKind vscGetOCLImgLibKindForHWCfg(
    IN VSC_HW_CONFIG            *pHwCfg
    );

/* Return the max free reg count for this HW config. */
gctUINT
vscGetHWMaxFreeRegCountPerShader(
    IN VSC_HW_CONFIG   *pHwCfg
    );

gceSTATUS
gcSHADER_WriteBufferToFile(
    IN gctSTRING buffer,
    IN gctUINT32 bufferSize,
    IN gctSTRING ShaderName
    );

gceSTATUS
gcSHADER_ReadBufferFromFile(
    IN gctSTRING    ShaderName,
    OUT gctSTRING    *buf,
    OUT gctUINT *bufSize
    );

void vscSetDriverVIRPath(gctBOOL bUseVIRPath);

gceSTATUS vscGetTemporaryDir(OUT gctSTRING gcTmpDir);

void vscSetIsLibraryShader(SHADER_HANDLE hShader, gctBOOL bIsLibraryShader);

END_EXTERN_C();

#endif /*__gc_vsc_drvi_interface_h_ */



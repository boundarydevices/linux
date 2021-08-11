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
**    Include file the defines the front- and back-end compilers, as well as the
**    objects they use.
*/

#ifndef __gc_vsc_old_drvi_interface_h_
#define __gc_vsc_old_drvi_interface_h_

#define _SUPPORT_LONG_ULONG_DATA_TYPE  1
#define _OCL_USE_INTRINSIC_FOR_IMAGE   1
#define _SUPPORT_NATIVE_IMAGE_READ     1

#include "gc_hal_engine.h"
#include "old_impl/gc_vsc_old_gcsl.h"

BEGIN_EXTERN_C()

#define GC_INIT_BUILTIN_CONSTANTS_BY_DRIVER 1

#ifndef GC_ENABLE_LOADTIME_OPT
#if !DX_SHADER
#define GC_ENABLE_LOADTIME_OPT      1
#endif
#endif

#define TEMP_SHADER_PATCH            1

#ifndef GC_ENABLE_DUAL_FP16
#define GC_ENABLE_DUAL_FP16          1
#endif


#define GC_ICACHE_PREFETCH_TABLE_SIZE   8
#define GC_DEFAULT_INLINE_LEVEL         2
#define GC_DEFAULT_TESS_LEVEL           0xFFFF

/* For OES. */
#define _sldSharedVariableStorageBlockName  "#sh_sharedVar"
#define _sldWorkGroupIdName                 "#sh_workgroupId"
#define __INIT_VALUE_FOR_WORK_GROUP_INDEX__ 0x1234

/* For OCL. */
#define _sldLocalSizeName                   "#local_size"
#define _sldGlobalSizeName                  "#global_size"
#define _sldGlobalOffsetName                "#global_offset"
#define _sldEnqueuedLocalSizeName           "#enqueued_local_size"
#define _sldLocalStorageAddressName         "#sh_local_address"
#define _sldWorkGroupCountName              "#workGroupCount"
#define _sldWorkGroupIdOffsetName           "#workGroupIdOffset"
#define _sldGlobalIdOffsetName              "#globalIdOffset"
#define _sldConstBorderValueName            "#constBorderValue"

/* Shared use. */
#define _sldLocalInvocationIndexName        "#local_invocation_index"
#define _sldLocalMemoryAddressName          "#sh_localMemoryAddress"
#define _sldThreadIdMemoryAddressName       "#sh_threadIdMemAddr"
#define _sldWorkThreadCountName             "#sh_workThreadCount"

#define FULL_PROGRAM_BINARY_SIG_1           gcmCC('F', 'U', 'L', 'L')
#define FULL_PROGRAM_BINARY_SIG_2           gcmCC('P', 'R', 'G', 'M')
/** Full program binary file header format: -
       Word 1:  gcmCC('F', 'U', 'L', 'L') Program binary file signature.
       Word 2:  gcmCC('P', 'R', 'G', 'M') Program binary file signature.
       Word 3:  kernel count.
       Word 4:  size of program binary file in bytes excluding this header.
*/
#define __FULL_PROGRAM_BINARY_HEADER_SIZE__     (sizeof(gctUINT32) * 4) /* Full program binary file header size in bytes*/

/** Program binary file header format: -
       Word 1:  gcmCC('P', 'R', 'G', 'M') Program binary file signature
       Word 2:  ('\od' '\od' '\od' '\od') od = octal digits; program binary file version
       Word 3:  ('\E' '\S' '\0' '\0') or Language type
                ('C' 'L' '\0' '\0')
       Word 4: ('\0' '\0' '\10' '\0') chip model  e.g. gc800
       Wor5: ('\1' '\3' '\6' '\4') chip version e.g. 4.6.3_rc1
       Word 6: size of program binary file in bytes excluding this header */
#define _gcdProgramBinaryHeaderSize   (6 * 4)  /*Program binary file header size in bytes*/

#define __OCL_PRINTF_WRITE_SIG1__               gcmCC('C', 'L', '\0', '\0')
#define __OCL_PRINTF_WRITE_SIG2__               gcmCC('P', 'R', 'I', 'N')

typedef enum _gceKernelBinaryKind
{
    gcvKERNEL_BINARY_NONE               = 0x00,
    gcvKERNEL_BINARY_CONST_BORDER       = 0x01,
}gceKernelinaryKind;

/*
 *   Re-compilation & Dynamic Linker data sturctures
 */

enum gceRecompileKind
{
    gceRK_PATCH_NONE = 0,
    gceRK_PATCH_TEXLD_FORMAT_CONVERSION,
    gceRK_PATCH_OUTPUT_FORMAT_CONVERSION,
    gceRK_PATCH_DEPTH_COMPARISON,
    gceRK_PATCH_CONSTANT_CONDITION,
    gceRK_PATCH_CONSTANT_TEXLD,
    gceRK_PATCH_COLOR_FACTORING,
    gceRK_PATCH_ALPHA_BLENDING,
    gceRK_PATCH_DEPTH_BIAS,
    gceRK_PATCH_NP2TEXTURE,
    gceRK_PATCH_GLOBAL_WORK_SIZE, /* OCL */
    gceRK_PATCH_READ_IMAGE, /* OCL */
    gceRK_PATCH_WRITE_IMAGE, /* OCL */
    gceRK_PATCH_Y_FLIPPED_TEXTURE,
    gceRK_PATCH_REMOVE_ASSIGNMENT_FOR_ALPHA,
    gceRK_PATCH_Y_FLIPPED_SHADER,
    gceRK_PATCH_INVERT_FRONT_FACING,
    gceRK_PATCH_ALPHA_TEST,
    gceRK_PATCH_SAMPLE_MASK,
    gceRK_PATCH_SIGNEXTENT,
    gceRK_PATCH_TCS_INPUT_COUNT_MISMATCH,
    gceRK_PATCH_CL_LONGULONG, /* OCL */
    gceRK_PATCH_COLOR_KILL,
    gceRK_PATCH_ALPHA_BLEND,
};

typedef enum _gceConvertFunctionKind
{
    gceCF_UNKNOWN,
    gceCF_RGBA32,
    gceCF_RGBA32I,
    gceCF_RGBA32UI,
    gceCF_RGBA16I,
    gceCF_RGBA16UI,
    gceCF_RGBA8I,
    gceCF_RGBA8UI,
    gceCF_SRGB8_ALPHA8,
    gceCF_RGB10_A2,
    gceCF_RGB10_A2UI,
/* ... */
}
gceConvertFunctionKind;

typedef enum _gceTexldFlavor
{
    gceTF_NONE,
    gceTF_TEXLD = gceTF_NONE,
    gceTF_PROJ,
    gceTF_PCF,
    gceTF_PCFPROJ,
    gceTF_BIAS_TEXLD,
    gceTF_BIAS_PROJ,
    gceTF_BIAS_PCF,
    gceTF_BIAS_PCFPROJ,
    gceTF_LOD_TEXLD,
    gceTF_LOD_PROJ,
    gceTF_LOD_PCF,
    gceTF_LOD_PCFPROJ,
    gceTF_GRAD_TEXLD,
    gceTF_GRAD_PROJ,
    gceTF_GRAD_PCF,
    gceTF_GRAD_PCFPROJ,
    gceTF_GATHER_TEXLD,
    gceTF_GATHER_PROJ,
    gceTF_GATHER_PCF,
    gceTF_GATHER_PCFPROJ,
    gceTF_FETCH_MS_TEXLD,
    gceTF_FETCH_MS_PROJ,
    gceTF_FETCH_MS_PCF,
    gceTF_FETCH_MS_PCFPROJ,
    gceTF_COUNT
}
gceTexldFlavor;

extern const gctCONST_STRING gcTexldFlavor[gceTF_COUNT];

typedef enum NP2_ADDRESS_MODE
{
    NP2_ADDRESS_MODE_CLAMP  = 0,
    NP2_ADDRESS_MODE_REPEAT = 1,
    NP2_ADDRESS_MODE_MIRROR = 2
}
NP2_ADDRESS_MODE;

typedef enum gcTEXTURE_MODE
{
    gcTEXTURE_MODE_NONE,
    gcTEXTURE_MODE_POINT,
    gcTEXTURE_MODE_LINEAR,
    gcTEXTURE_MODE_COUNT
}
gcTEXTURE_MODE;

typedef struct _gcNPOT_PATCH_PARAM
{
    gctINT               samplerSlot;
    NP2_ADDRESS_MODE     addressMode[3];
    gctINT               texDimension;    /* 2 or 3 */
}
gcNPOT_PATCH_PARAM, *gcNPOT_PATCH_PARAM_PTR;

typedef struct _gcsInputConversion
{
    gctINT                  layers;       /* numberof layers the input format
                                             represented internally (up to 4) */

    gcTEXTURE_MODE          mipFilter;
    gcTEXTURE_MODE          magFilter;
    gcTEXTURE_MODE          minFilter;

    gctFLOAT                LODBias;

    gctINT                  projected;
    gctINT                  mipLevelMax;
    gctINT                  mipLevelMin;

    gctINT                  width;
    gctINT                  height;
    gctINT                  depth;
    gctINT                  dimension;
    gctBOOL                 srgb;
    gcUNIFORM               orgShaderSampler;
    gcUNIFORM               samplers[4];
    gctINT                  arrayIndex;
    gcsSURF_FORMAT_INFO     samplerInfo;
    /* Whether recompile do the swizzle? */
    gctBOOL                 needSwizzle;
    /* Whether recompile do the format convert? */
    gctBOOL                 needFormatConvert;
    /* 1: use depth, 0: use stencil */
    gctBOOL                 depthStencilMode;
    gceTEXTURE_SWIZZLE      swizzle[gcvTEXTURE_COMPONENT_NUM];

    gcSHADER_KIND           shaderKind;
}
gcsInputConversion;

typedef struct _gcsOutputConversion
{
    gctINT                  layers;       /* numberof layers the input format
                                             represented internally (up to 4) */
    gcsSURF_FORMAT_INFO     formatInfo;  /* */
    gctINT                  outputLocation;

    /* private data.
       It is not used in the VIR recompilation */
    gcOUTPUT                outputs[4];
}
gcsOutputConversion;

typedef struct _gcsDepthComparison
{
    gcsSURF_FORMAT_INFO     formatInfo;
    gcUNIFORM               sampler;
    gctINT                  arrayIndex;
    gctUINT                 compMode;
    gctUINT                 compFunction;
    gctBOOL                 convertD32F;   /* the texture format is D32F and
                                              needs to be converted (halti 0) */
} gcsDepthComparison;

typedef struct _gcsConstantCondition
{
    /* */
    gctUINT   uniformCount;
    gctUINT * uniformIdx;       /* a list of index of the uniform which is
                                   used in constant expression */
    gctUINT   conditionCount;   /* the number of conditions depended on
                                   the uniforms */
}
gcsConstantCondition;

typedef struct _gcsConstantTexld
{
    gctUINT   samplerIndex;
    gctUINT   instId;            /* the instruction id which has the constant
                                    coordinate */
    gctFLOAT  value[4];          /* vec4 value of the texld */
}
gcsConstantTexld;

typedef struct _gcsPatchOutputValue
{
    gctSTRING   outputName;
    gctUINT     outputFormatConversion;
}
gcsPatchOutputValue;

typedef struct _gcsPatchColorFactoring
{
    gctINT    outputLocation;    /* location of render target need to patch */
    gctFLOAT  value[4];          /* vec4 value of the color factor */
}
gcsPatchColorFactoring;

typedef struct _gcsPatchAlphaBlending
{
    gctINT    outputLocation;    /* location of render target need to patch */
}
gcsPatchAlphaBlending;

typedef struct _gcsPatchDepthBias
{
    gcUNIFORM depthBiasUniform;  /* uniform holding the value of depth bias */
}
gcsPatchDepthBias;

typedef struct _gcsPatchNP2Texture
{
    gctINT textureCount;
    gcNPOT_PATCH_PARAM_PTR np2Texture;
}
gcsPatchNP2Texture;

typedef struct _gcsPatchGlobalWorkSize
{
    gcUNIFORM  globalWidth;
    gcUNIFORM  groupWidth;
    gctBOOL    patchRealGlobalWorkSize;
}
gcsPatchGlobalWorkSize;

typedef struct _gcsPatchReadImage
{
    gctUINT                 samplerNum;
    gctUINT                 imageNum;
    gctUINT                 imageType;
    gctUINT                 imageDataIndex;
    gctUINT                 imageSizeIndex;
    gctUINT                 samplerValue;
    gctUINT                 channelDataType;
    gctUINT                 channelOrder;
}
gcsPatchReadImage;

typedef struct _gcsPatchWriteImage
{
    gctUINT                 samplerNum;
    gctUINT                 imageDataIndex;
    gctUINT                 imageSizeIndex;
    gctUINT                 channelDataType;
    gctUINT                 channelOrder;
    gctUINT                 imageType;
}
gcsPatchWriteImage;

typedef struct _gcsPatchLongULong
{
    gctUINT                 instructionIndex;
    gctUINT                 channelCount;       /* channel (target enabled) count */
}
gcsPatchLongULong;

typedef struct _gcsPatchYFlippedTexture
{
    gcUNIFORM yFlippedTexture;  /* uniform need to filp y component. */
}
gcsPatchYFlippedTexture;

typedef struct _gcsPatchYFlippedShader
{
    gcUNIFORM rtHeight;  /* uniform contains render target height to filp y component. */
}
gcsPatchYFlippedShader;

typedef struct _gcsPatchAlphaTestShader
{
    gcUNIFORM alphaTestData; /* uniform contains refValue and func. */
}
gcsPatchAlphaTestShader;

typedef struct _gcsPatchFlippedSamplePosition
{
    gctFLOAT  value;  /* change gl_SamplePosition to (value - gl_SamplePosition); */
}
gcsPatchFlippedSamplePosition;

typedef struct _gcsPatchRemoveAssignmentForAlphaChannel
{
    gctBOOL removeOutputAlpha[gcdMAX_DRAW_BUFFERS]; /* Flag whether this output need this patch.*/
}
gcsPatchRemoveAssignmentForAlphaChannel;

typedef struct _gcsPatchSampleMask
{
    /* alpha to converage */
    gctBOOL                 alphaToConverageEnabled;
    /* sample coverage */
    gctBOOL                 sampleConverageEnabled;
    gcUNIFORM               sampleCoverageValue_Invert; /* float32 x: value,
                                                         *         y: invert */
    /* sample mask */
    gctBOOL                 sampleMaskEnabled;
    gcUNIFORM               sampleMaskValue;     /* UINT type, the fragment coverage is ANDed
                                                    with coverage value SAMPLE_MASK_VALUE */
    /* internal data */
    gctUINT                 _finalSampleMask;
    gctINT                  _implicitMaskRegIndex;
}
gcsPatchSampleMask;

typedef struct _gcsPatchSignExtent
{
    gcUNIFORM               uniform;
    gctUINT16               arrayIndex;
}
gcsPatchSignExtent;

typedef struct _gcsPatchTCSInputCountMismatch
{
    gctINT                  inputVertexCount;
}
gcsPatchTCSInputCountMismatch;

typedef struct _gcsPatchColorKill
{
    gctFLOAT                value;
}
gcsPatchColorKill;

typedef struct _gcsPatchAlphaBlend
{
    gctINT      outputLocation;
    gcOUTPUT    outputs[4];
    gcUNIFORM   alphaBlendEquation;
    gcUNIFORM   alphaBlendFunction;
    gcUNIFORM   rtWidthHeight;
    gcUNIFORM   blendConstColor;
    gcUNIFORM   rtSampler;     /* sampler */
    gcUNIFORM   yInvert;

}
gcsPatchAlphaBlend;

typedef struct _gcRecompileDirective * gcPatchDirective_PTR;
typedef struct _gcRecompileDirective
{
    enum gceRecompileKind   kind;
    union {
        gcsInputConversion  *     formatConversion;
        gcsOutputConversion *     outputConversion;
        gcsConstantCondition *    constCondition;
        gcsDepthComparison  *     depthComparison;
        gcsConstantTexld    *     constTexld;
        gcsPatchColorFactoring *  colorFactoring;
        gcsPatchAlphaBlending *   alphaBlending;
        gcsPatchDepthBias *       depthBias;
        gcsPatchNP2Texture *      np2Texture;
        gcsPatchGlobalWorkSize *  globalWorkSize;
        gcsPatchReadImage *       readImage;
        gcsPatchWriteImage *      writeImage;
        gcsPatchYFlippedTexture * yFlippedTexture;
        gcsPatchRemoveAssignmentForAlphaChannel * removeOutputAlpha;
        gcsPatchYFlippedShader *  yFlippedShader;
        gcsPatchAlphaTestShader * alphaTestShader;
        gcsPatchSampleMask *      sampleMask;
        gcsPatchSignExtent *      signExtent;
        gcsPatchTCSInputCountMismatch *  inputMismatch;
        gcsPatchLongULong   *     longULong;
        gcsPatchColorKill *       colorKill;
        gcsPatchAlphaBlend *      alphaBlend;
    } patchValue;
    gcPatchDirective_PTR    next;  /* pointer to next patch directive */
}
gcPatchDirective;

typedef struct _gcDynamicPatchInfo
{
    gctINT                patchDirectiveCount;
    gcPatchDirective      patchDirective[1];
}
gcDynamicPatchInfo;

typedef enum _gcGL_DRIVER_VERSION {
    gcGL_DRIVER_ES11, /* OpenGL ES 1.1 */
    gcGL_DRIVER_ES20, /* OpenGL ES 2.0 */
    gcGL_DRIVER_ES30     /* OpenGL ES 3.0 */
}
gcGL_DRIVER_VERSION;

/* gcSHADER objects. */

typedef struct _gcsHINT *               gcsHINT_PTR;
typedef struct _gcSHADER_PROFILER *     gcSHADER_PROFILER;

typedef struct _gcsPROGRAM_VidMemPatchOffset
{
    gctUINT32 instVidMemInStateBuffer[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 gprSpillVidMemInStateBuffer[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 crSpillVidMemInStateBuffer[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 sharedMemVidMemInStateBuffer;
    gctUINT32 threadIdVidMemInStateBuffer;

    gctUINT32 instVidMemInStateDelta[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 gprSpillVidMemInStateDelta[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 crSpillVidMemInStateDelta[gcMAX_SHADERS_IN_LINK_GOURP];
    gctUINT32 sharedMemVidMemInStateDelta;
    gctUINT32 threadIdVidMemInStateDelta;
}
gcsPROGRAM_VidMemPatchOffset;


#define     VSC_STATE_DELTA_END                     0xfeeffeef
#define     VSC_STATE_DELTA_DESC_SIZE_IN_UINT32     3
/*
** stateDelta format is as below in uint32 unit.
**    offset 0: start state address
**    offset 1: count of states (n)
**    offset 2 ~ n + 2: n value of states.
**    offset n + 3: end tag(VSC_STATE_DELTA_END) for sanity check.
**    offset n + 4: start state address for next batch
**    ******
**    ******
**    offset of last: end tag(VSC_STATE_DELTA_END)
*/

typedef struct _gcsPROGRAM_STATE
{
    /* Shader program state buffer. */
    gctUINT32                stateBufferSize;
    gctPOINTER               stateBuffer;
    gcsHINT_PTR              hints;
    gcsPROGRAM_VidMemPatchOffset patchOffsetsInDW;
    gctUINT32                    stateDeltaSize;
    gctUINT32                    *stateDelta;
}
gcsPROGRAM_STATE, *gcsPROGRAM_STATE_PTR;


typedef enum _gceUNIFOEM_ALLOC_MODE
{
    /* Non unified allocation, all fixed. */
    gcvUNIFORM_ALLOC_NONE_UNIFIED                               = 0,

    /* Allocated unified but with float base address offset, pack all stages one by one. */
    gcvUNIFORM_ALLOC_PACK_FLOAT_BASE_OFFSET                     = 1,

    /* Allocated unified but with float base address offset, pack Gpipe one by one and put them in the top, and put PS in the bottom. */
    gcvUNIFORM_ALLOC_GPIPE_TOP_PS_BOTTOM_FLOAT_BASE_OFFSET      = 2,

    /* Allocated unified but with float base address offset, pack Gpipe one by one and put them in the bottom, and put PS in the top. */
    gcvUNIFORM_ALLOC_PS_TOP_GPIPE_BOTTOM_FLOAT_BASE_OFFSET      = 3,

    /* Allocated in full scope of unified register file, all stages use the same register for one uniform. */
    gcvUNIFORM_ALLOC_FULL_UNIFIED                               = 4,
}gceUNIFOEM_ALLOC_MODE;

typedef struct _gcsPROGRAM_UNIFIED_STATUS
{
    gctBOOL                 useIcache;          /* Icache enabled or not */

    gctBOOL                 instruction; /* unified instruction enabled or not */
    gceUNIFOEM_ALLOC_MODE   constantUnifiedMode;
    gceUNIFOEM_ALLOC_MODE   samplerUnifiedMode;

    gctINT                  instVSEnd;       /* VS instr end for unified instruction */
    gctINT                  instPSStart;     /* PS instr start for unified instruction */
    /* Valid if UnifiedMode is gcvUNIFORM_ALLOC_FLOAT_BASE_OFFSET and unifiedUniform is disabled. */
    gctINT                  constGPipeEnd;   /* GPipe const end for unified constant */
    gctINT                  constPSStart;    /* PS const start for unified constant */
    gctINT                  samplerGPipeStart; /* GPipe sampler start for unified sampler */
    gctINT                  samplerPSEnd;  /* PS sampler end for unified sampler */
    /* Valid if chip can support unified uniform. */
    gctINT                  constCount;      /* The constant reg count for all shader stages. */
    gctINT                  samplerCount;    /* The sampler reg count for all shader stages. */
}
gcsPROGRAM_UNIFIED_STATUS;

#define PROGRAM_UNIFIED_STATUS_Initialize(UnifiedStatus, TotalSamplerCount)     \
    do {                                                                        \
        (UnifiedStatus)->useIcache            = gcvFALSE;                       \
        (UnifiedStatus)->instruction          = gcvFALSE;                       \
        (UnifiedStatus)->constantUnifiedMode  = gcvUNIFORM_ALLOC_NONE_UNIFIED;  \
        (UnifiedStatus)->samplerUnifiedMode   = gcvUNIFORM_ALLOC_NONE_UNIFIED;  \
        (UnifiedStatus)->instVSEnd            = -1;                             \
        (UnifiedStatus)->instPSStart          = -1;                             \
        (UnifiedStatus)->constGPipeEnd        = -1;                             \
        (UnifiedStatus)->constPSStart         = -1;                             \
        (UnifiedStatus)->samplerGPipeStart    = (TotalSamplerCount);            \
        (UnifiedStatus)->samplerPSEnd         = 0;                              \
        (UnifiedStatus)->constCount           = -1;                             \
        (UnifiedStatus)->samplerCount         = -1;                             \
    } while(0)

typedef struct _gcSHADER_VID_NODES
{
    gctPOINTER  instVidmemNode[gcMAX_SHADERS_IN_LINK_GOURP]; /* SURF Node for instruction buffer for I-Cache. */
    gctPOINTER  gprSpillVidmemNode[gcMAX_SHADERS_IN_LINK_GOURP]; /* SURF Node for gpr spill memory. */
    gctPOINTER  crSpillVidmemNode[gcMAX_SHADERS_IN_LINK_GOURP]; /* SURF Node for cr spill memory. */
    gctPOINTER  sharedMemVidMemNode;
    gctPOINTER  threadIdVidMemNode;
}gcSHADER_VID_NODES;

typedef enum _gceMEMORY_ACCESS_FLAG
{
    gceMA_FLAG_NONE                 = 0x0000,
    gceMA_FLAG_LOAD                 = 0x0001,
    gceMA_FLAG_STORE                = 0x0002,
    gceMA_FLAG_IMG_READ             = 0x0004,
    gceMA_FLAG_IMG_WRITE            = 0x0008,
    gceMA_FLAG_ATOMIC               = 0x0010,

    gceMA_FLAG_READ                 = gceMA_FLAG_LOAD       |
                                      gceMA_FLAG_IMG_READ   |
                                      gceMA_FLAG_ATOMIC,
    gceMA_FLAG_WRITE                = gceMA_FLAG_STORE      |
                                      gceMA_FLAG_IMG_WRITE  |
                                      gceMA_FLAG_ATOMIC,
    gceMA_FLAG_BARRIER              = 0x0020,
    gceMA_FLAG_EVIS_ATOMADD         = 0x0040, /* evis atomadd can operate on 16B data in parallel,
                                                * we need to tell driver to turn off workgroup packing
                                                * if it is used so the HW will not merge different
                                                * workgroup into one which can cause the different
                                                * address be used for the evis_atom_add */
/* must sync with SHADER_EDH_MEM_ACCESS_HINT and VIR_MemoryAccessFlag!!! */
}
gceMEMORY_ACCESS_FLAG;

typedef enum _gceFLOW_CONTROL_FLAG
{
    gceFC_FLAG_NONE                 = 0x0000,
    gceFC_FLAG_JMP                  = 0x0001,
    gceFC_FLAG_CALL                 = 0x0002,
    gceFC_FLAG_KILL                 = 0x0004,
/* must sync with SHADER_EDH_FLOW_CONTROL_HINT and VIR_FlowControlFlag!!! */
}
gceFLOW_CONTROL_FLAG;

typedef enum _gceTEXLD_FLAG
{
    gceTEXLD_FLAG_NONE                 = 0x0000,
    gceTEXLD_FLAG_TEXLD                = 0x0001,
/* must sync with SHADER_EDH_TEXLD_HINT and VIR_TexldFlag!!! */
}
gceTEXLD_FLAG;

typedef enum _gceSHADER_LEVEL
{
    gcvSHADER_HIGH_LEVEL            = 0,
    gcvSHADER_MACHINE_LEVEL         = 1,
    gcvSHADER_LEVEL_COUNT           = 2,
}
gceSHADER_LEVEL;

typedef struct _gcWORK_GROUP_SIZE
{
    gctUINT       x;
    gctUINT       y;
    gctUINT       z;
}gcWORK_GROUP_SIZE;

typedef struct _gcsHINT
{
    /* fields for the program */
    gctUINT32   elementCount;       /* Element count. */
    gctUINT32   componentCount;     /* Component count. */
    gctUINT     maxInstCount;
    gctUINT     maxConstCount;      /* Shader uniform registers. */

    gceSHADING  shaderMode;         /* Flag whether program is smooth or flat. */
    gctUINT32   shaderConfigData;   /* Data for register: 0x0218.
                                       For vertex shader, only save the bit that
                                       would be covered on fragment shader.*/

    gctUINT32   balanceMin;         /* Balance minimum. */
    gctUINT32   balanceMax;         /* Balance maximum. */

    gceMEMORY_ACCESS_FLAG memoryAccessFlags[gcvSHADER_LEVEL_COUNT][gcvPROGRAM_STAGE_LAST]; /* Memory access flag. */

    gceFLOW_CONTROL_FLAG flowControlFlags[gcvSHADER_LEVEL_COUNT][gcvPROGRAM_STAGE_LAST]; /* Flow control flag. */

    gceTEXLD_FLAG texldFlags[gcvSHADER_LEVEL_COUNT][gcvPROGRAM_STAGE_LAST]; /* Texld flag. */

    gcsPROGRAM_UNIFIED_STATUS unifiedStatus;

#if TEMP_SHADER_PATCH
    gctUINT32   pachedShaderIdentifier;
#endif

    /* fields for Vertex shader */
    gctUINT     vertexShaderId;     /* vertex shader id, to help identifying
                                     * shaders used by draw commands */
    gctUINT     vsInstCount;
    gctUINT32   vsOutputCount;      /* Numbr of data transfers for VS output */
    gctUINT     vsConstCount;
    gctUINT32   vsMaxTemp;          /* Maximum number of temporary registers used in VS. */
    gctUINT32   vsLTCUsedUniformStartAddress; /* the start physical address for
                                                 uniforms only used in LTC expression */

    /* fields for Fragment shader */
    gctUINT     fragmentShaderId;   /*fragment shader id, to help identifying
                                     * shaders used by draw commands */
    gctUINT     fsInstCount;
    gctUINT32   fsInputCount;       /* Number of data transfers for FS input. */
    gctUINT     fsConstCount;
    gctUINT32   fsMaxTemp;          /* Maximum number of temporary registers used in FS. */
    gctUINT32   fsLTCUsedUniformStartAddress;   /* the start physical address for
                                                   uniforms only used in LTC expression */
    gctUINT     psInputControlHighpPosition;
    gctUINT     psHighPVaryingCount;
    gctINT32    psOutput2RtIndex[gcdMAX_DRAW_BUFFERS];

#if gcdALPHA_KILL_IN_SHADER
    /* States to set when alpha kill is enabled. */
    gctUINT32   killStateAddress;
    gctUINT32   alphaKillStateValue;
    gctUINT32   colorKillStateValue;

    /* Shader instructiuon. */
    gctUINT32   killInstructionAddress;
    gctUINT32   alphaKillInstruction[3];
    gctUINT32   colorKillInstruction[3];
#endif

    /* First word. */
    /* gctBOOL isdefined as signed int, 1 bit will have problem if the value
     * is not used to test zero or not, use 2 bits to avoid the potential error
     */
    gctBOOL     removeAlphaAssignment : 2; /* Flag whether this program can remove
                                              alpha assignment*/
    gctBOOL     autoShift             : 2; /* Auto-shift balancing. */
    gctBOOL     vsHasPointSize        : 2; /* Flag whether VS has point size or not */
    gctBOOL     vsPtSizeAtLastLinkLoc : 2; /* Flag point size will be put at the last link loc for VS */
    gctBOOL     vsUseStoreAttr        : 2;
    gctBOOL     psHasFragDepthOut     : 2; /* Flag whether the PS outputs the depth value or not. */
    gctBOOL     hasKill               : 2; /* Flag whether or not the shader has a KILL instruction. */
    gctBOOL     psHasDiscard          : 2; /* Flag whether the PS code has discard. */

    gctBOOL     threadWalkerInPS      : 2; /* Flag whether the ThreadWalker is in PS. */
    gctBOOL     fsIsDual16            : 2;
    gctBOOL     useSamplePosition     : 2;
    gctBOOL     useFrontFacing        : 2;
    gctBOOL     useDSX                : 2;
    gctBOOL     useDSY                : 2;
    gctBOOL     yInvertAware          : 2;
    gctBOOL     hasCentroidInput      : 2; /* flag if PS uses any inputs defined as centroid. */

    /* Second word. */
    gctBOOL     disableEarlyZ         : 2; /* Disable EarlyZ for this program. */
    gctBOOL     threadGroupSync       : 2;
    gctBOOL     usedSampleIdOrSamplePosition : 2; /* For sample shading. */
    gctBOOL     sampleMaskOutWritten  : 2;
    gctBOOL     psUsedSampleInput     : 2;
    gctBOOL     prePaShaderHasPointSize : 2;  /* Flag whether pre-PA has point size or not */
    gctBOOL     isPtSizeStreamedOut   : 2; /* Flag point size will be streamed out */
    gctBOOL     hasAttrStreamOuted    : 2; /* Flag any attribute that will be streamed out */
    gctBOOL     prePaShaderHasPrimitiveId : 2;
    gctBOOL     sharedMemAllocByCompiler : 2; /* Flag whether share memory is allocated by compiler */
    /* If any reged CTCs are used in the shader. */
#if gcdUSE_WCLIP_PATCH
    gctBOOL     vsPositionZDependsOnW  : 2; /* Flag whether the VS gl_position.z
                                               depends on gl_position.w it's a hint
                                               for wclipping */
    gctBOOL     strictWClipMatch      : 2; /* Strict WClip match. */
    gctBOOL     WChannelEqualToZ      : 2;
#endif
    gctBOOL     useGroupId            : 2;
    gctBOOL     useLocalId            : 2;
    gctBOOL     useEvisInst           : 2;

    /* Third word. */
    gctUINT     fragColorUsage        : 2;
    gctBOOL     dependOnWorkGroupSize : 2;
    gctBOOL     clIsDual16            : 2;
    gctUINT     reserved              : 26;

    /* flag if the shader uses gl_FragCoord, gl_FrontFacing, gl_PointCoord */
    gctCHAR     useFragCoord[4];
    gctCHAR     usePointCoord[4];
    gctCHAR     useRtImage[4];
    gctCHAR     useRegedCTC[gcMAX_SHADERS_IN_LINK_GOURP];
    gctCHAR     interpolationType[128];

    gctBOOL     useEarlyFragmentTest;    /* flag if PS uses early fragment tests. */
    gctINT      sampleMaskLoc; /* -1 means loc can be determined by driver */

    /* They're component index after packing */
    gctINT      pointCoordComponent;
    gctINT      rtArrayComponent;
    gctINT      primIdComponent;

#if gcdUSE_WCLIP_PATCH
    gctINT      MVPCount;
#endif
    /* fields for Tessellation control shader */
    gctUINT     tcsShaderId;        /* Tessellation control shader id, to help
                                     * identifying shaders used by draw commands */

    /* fields for Tessellation evaluation shader */
    gctUINT     tesShaderId;        /* Tessellation evaluation shader id, to help
                                     * identifying shaders used by draw commands */

    /* fields for Geometry shader */
    gctUINT     gsShaderId;        /* Geometry shader id, to help identifying
                                     *  shaders used by draw commands */

    /* Instruction prefetch table. */
    gctINT32    vsICachePrefetch[GC_ICACHE_PREFETCH_TABLE_SIZE];
    gctINT32    tcsICachePrefetch[GC_ICACHE_PREFETCH_TABLE_SIZE];
    gctINT32    tesICachePrefetch[GC_ICACHE_PREFETCH_TABLE_SIZE];
    gctINT32    gsICachePrefetch[GC_ICACHE_PREFETCH_TABLE_SIZE];
    gctINT32    fsICachePrefetch[GC_ICACHE_PREFETCH_TABLE_SIZE];

    gcePROGRAM_STAGE_BIT  stageBits;

    gctUINT     usedSamplerMask;
    gctUINT     usedRTMask;

    /* For CL and CS, global/group/local id order. */
    gctUINT32   valueOrder;

    /* Deferred-program when flushing as they are in VS output ctrl reg */
    gctINT      vsOutput16RegNo;
    gctINT      vsOutput17RegNo;
    gctINT      vsOutput18RegNo;

    gctINT32    shader2PaOutputCount; /* Output count from pre-pa shader (excluding pure TFX count) */
    gctUINT     ptSzAttrIndex;
    /* Sampler Base offset. */
    gctUINT32   samplerBaseOffset[gcvPROGRAM_STAGE_LAST];

    /* const regNo base */
    gctUINT32   hwConstRegBases[gcvPROGRAM_STAGE_LAST];
    gctUINT32   constRegNoBase[gcvPROGRAM_STAGE_LAST];

    gctINT      psOutCntl0to3;
    gctINT      psOutCntl4to7;
    gctINT      psOutCntl8to11;
    gctINT      psOutCntl12to15;

    gcWORK_GROUP_SIZE workGrpSize;
    gctUINT16   workGroupSizeFactor[3];

    /* per-vertex attributeCount. */
    gctUINT     tcsPerVertexAttributeCount;
    gctBOOL     tcsHasNoPerVertexAttribute;

    gctUINT     extraUscPages;

    /* Active cluster count for this shader. */
    gctUINT16   activeClusterCount;

    /* Concurrent workThreadCount, per-GPU when multi-cluster is disabled, per-cluster when multi-cluster is enabled. */
    gctUINT16   workThreadCount;

    /* Local/share memory size. */
    gctUINT     localMemSizeInByte;

    /* Concurrent workGroupCount. */
    gctUINT16   workGroupCount;

    /* Sampler Base offset. */
    gctBOOL     useGPRSpill[gcvPROGRAM_STAGE_LAST];

    /* Misc hints */
    gctINT32    GSmaxThreadsPerHwTG;
    gctUINT     tpgTopology;
    /* padding bytes to make the offset of shaderVidNodes field be consistent in 32bit and 64bit platforms */
    gctCHAR     reservedByteForShaderVidNodeOffset[8];

    /* shaderVidNodes should always be the LAST filed in hits. */
    /* SURF Node for memory that is used in shader. */
    gcSHADER_VID_NODES shaderVidNodes;

    /* padding bytes to make the struct size be consistent in 32bit and 64bit platforms */
    gctCHAR     reservedByteForHitSize[8];
}gcsHINT;

#if defined(_WINDOWS)
char _check_shader_vid_nodes_offset[(gcmOFFSETOF(_gcsHINT, shaderVidNodes) % 8) == 0];
char _check_hint_size[(sizeof(struct _gcsHINT) % 8) == 0];
#endif

#define gcsHINT_isCLShader(Hint)            ((Hint)->clShader)
#define gcsHINT_GetShaderMode(Hint)         ((Hint)->shaderMode)
#define gcsHINT_GetSurfNode(Hint)           ((Hint)->surfNode)

#define gcsHINT_SetProgramStageBit(Hint, Stage) do { (Hint)->stageBits |= 1 << Stage;} while (0)

typedef enum _gcSHADER_TYPE_KIND
{
    gceTK_UNKOWN,
    gceTK_FLOAT,
    gceTK_INT,
    gceTK_UINT,
    gceTK_BOOL,
    gceTK_FIXED,
    gceTK_IMAGE,
    gceTK_IMAGE_T,
    gceTK_SAMPLER,
    gceTK_SAMPLER_T,
    gceTK_ATOMIC, /* Atomic Counter */
    gceTK_INT64,
    gceTK_UINT64,
    gceTK_CHAR,
    gceTK_UCHAR,
    gceTK_SHORT,
    gceTK_USHORT,
    gceTK_FLOAT16,
    gceTK_OTHER
} gcSHADER_TYPE_KIND;

typedef struct _gcSHADER_TYPEINFO
{
    gcSHADER_TYPE      type;              /* e.g. gcSHADER_FLOAT_2X4 */
    gctUINT            components;        /* e.g. 4 components each row
                                           * for packed type it is real component
                                           * number in vec4 register: CHAR_P3 takes 1
                                           * component */
    gctUINT            packedComponents;  /* number of components in packed type,
                                           * it is 3 for CHAR_P3.
                                           * same as components for non-packed type */
    gctUINT            rows;              /* e.g. 2 rows             */
    gcSHADER_TYPE      rowType;           /* e.g. gcSHADER_FLOAT_X4  */
    gcSHADER_TYPE      componentType;     /* e.g. gcSHADER_FLOAT_X1  */
    gcSHADER_TYPE_KIND kind;              /* e.g. gceTK_FLOAT */
    gctCONST_STRING    name;              /* e.g. "FLOAT_2X4" */
    gctBOOL            isPacked;          /* e.g. gcvTRUE if of packed type such as gcSHADER_UINT8_P2 ... */
} gcSHADER_TYPEINFO;

extern const gcSHADER_TYPEINFO gcvShaderTypeInfo[];

#define gcmType_Comonents(Type)         (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].components : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].components)
#define gcmType_PackedComonents(Type)   (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].packedComponents : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].packedComponents)
#define gcmType_Rows(Type)              (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].rows : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].rows)
#define gcmType_RowType(Type)           (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].rowType : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].rowType)
#define gcmType_ComonentType(Type)      (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].componentType : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].componentType)
#define gcmType_Kind(Type)              (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].kind : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].kind)
#define gcmType_Name(Type)              (Type < gcSHADER_TYPE_COUNT? gcvShaderTypeInfo[Type].name : gcvShaderTypeInfo[gcSHADER_UNKONWN_TYPE].name)

#define gcmType_isSampler(Type)         (gcmType_Kind(Type) == gceTK_SAMPLER || gcmType_Kind(Type) == gceTK_SAMPLER_T)

#define gcmType_ComponentByteSize       4

#define gcmType_isMatrix(type) (gcmType_Rows(type) > 1)

enum gceLTCDumpOption {
    gceLTC_DUMP_UNIFORM      = 0x0001,
    gceLTC_DUMP_EVALUATION   = 0x0002,
    gceLTC_DUMP_EXPESSION    = 0x0004,
    gceLTC_DUMP_COLLECTING   = 0x0008,
};

/* single precision floating point NaN, Infinity, etc. constant */
#define SINGLEFLOATPOSITIVENAN      0x7fc00000
#define SINGLEFLOATNEGTIVENAN       0xffc00000
#define SINGLEFLOATPOSITIVEINF      0x7f800000
#define SINGLEFLOATNEGTIVEINF       0xff800000
#define SINGLEFLOATPOSITIVEZERO     0x00000000
#define SINGLEFLOATNEGTIVEZERO      0x80000000

#define isF32PositiveNaN(f)  (*(gctUINT *)&(f) == SINGLEFLOATPOSITIVENAN)
#define isF32NegativeNaN(f)  (*(gctUINT *)&(f) == SINGLEFLOATNEGTIVENAN)
#define isF32NaN(f)  (isF32PositiveNaN(f) || isF32NegativeNaN(f))

#define FLOAT_NaN   (SINGLEFLOATPOSITIVENAN)
#ifndef INT32_MAX
#define INT32_MAX   (0x7fffffff)
#endif
#ifndef INT32_MIN
#define INT32_MIN   (-0x7fffffff-1)
#endif

gceSTATUS gcOPT_GetUniformSrcLTC(
    IN gcSHADER              Shader,
    IN gctUINT               ltcInstIdx,
    IN gctINT                SourceId,
    IN PLTCValue             Results,
    OUT gcUNIFORM*           RetUniform,
    OUT gctINT*              RetCombinedOffset,
    OUT gctINT*              RetConstOffset,
    OUT gctINT*              RetIndexedOffset,
    OUT PLTCValue            SourceValue
    );

gceSTATUS gcOPT_DoConstantFoldingLTC(
    IN gcSHADER              Shader,
    IN gctUINT               ltcInstIdx,
    IN PLTCValue             source0Value, /* set by driver if src0 is app's uniform */
    IN PLTCValue             source1Value, /* set by driver if src1 is app's uniform */
    IN PLTCValue             source2Value, /* set by driver if src2 is app's uniform */
    IN gctBOOL               hasSource2,
    OUT PLTCValue            resultValue, /* regarded as register file */
    IN OUT PLTCValue         Results
    );

gctBOOL gcDumpOption(gctINT Opt);

/* It must be non-zero */
#define POINTSPRITE_TEX_ATTRIBUTE     0x8000

/* Shader flags. */
typedef enum _gceSHADER_FLAGS
{
    gcvSHADER_NO_OPTIMIZATION           = 0x00,
    gcvSHADER_DEAD_CODE                 = 0x01,
    gcvSHADER_RESOURCE_USAGE            = 0x02,
    gcvSHADER_OPTIMIZER                 = 0x04,
    gcvSHADER_USE_GL_Z                  = 0x08,
    /*
        The GC family of GPU cores model GC860 and under require the Z
        to be from 0 <= z <= w.
        However, OpenGL specifies the Z to be from -w <= z <= w.  So we
        have to a conversion here:

            z = (z + w) / 2.

        So here we append two instructions to the vertex shader.
    */
    gcvSHADER_USE_GL_POSITION           = 0x10,
    gcvSHADER_USE_GL_FACE               = 0x20,
    gcvSHADER_USE_GL_POINT_COORD        = 0x40,
    gcvSHADER_LOADTIME_OPTIMIZER        = 0x80,
#if gcdALPHA_KILL_IN_SHADER
    gcvSHADER_USE_ALPHA_KILL            = 0x100,
#endif

    gcvSHADER_ENABLE_MULTI_GPU          = 0x200,

    gcvSHADER_TEXLD_W                   = 0x400,

    gcvSHADER_INT_ATTRIBUTE_W           = 0x800,

    /* The Shader is patched by recompilation. */
    gcvSHADER_IMAGE_PATCHING            = 0x1000,

    /* Remove unused uniforms on shader, only enable for es20 shader. */
    gcvSHADER_REMOVE_UNUSED_UNIFORMS    = 0x2000,

    /* Force generate a floating MAD, no matter if HW can support it. */
    gcSHADER_FLAG_FORCE_GEN_FLOAT_MAD   = 0x4000,

    /* Disable default UBO for vertex and fragment shader. */
    gcvSHADER_DISABLE_DEFAULT_UBO       = 0x8000,

    /* This shader is from recompier. */
    gcvSHADER_RECOMPILER                = 0x10000,

    /* This is a seperated program link */
    gcvSHADER_SEPERATED_PROGRAM         = 0x20000,

    /* disable dual16 for this ps shader */
    gcvSHADER_DISABLE_DUAL16            = 0x40000,

    /* set inline level 0 */
    gcvSHADER_SET_INLINE_LEVEL_0        = 0x80000,

    /* set inline level 1 */
    gcvSHADER_SET_INLINE_LEVEL_1        = 0x100000,

    /* set inline level 2 */
    gcvSHADER_SET_INLINE_LEVEL_2        = 0x200000,

    /* set inline level 3 */
    gcvSHADER_SET_INLINE_LEVEL_3        = 0x400000,

    /* set inline level 4 */
    gcvSHADER_SET_INLINE_LEVEL_4        = 0x800000,

    /* resets inline level to default */
    gcvSHADER_RESET_INLINE_LEVEL        = 0x1000000,

    /* Need add robustness check code */
    gcvSHADER_NEED_ROBUSTNESS_CHECK     = 0x2000000,

    /* Denormalize flag */
    gcvSHADER_FLUSH_DENORM_TO_ZERO      = 0x4000000,

    /* Has image in kernel source code of OCL kernel program */
    gcSHADER_HAS_IMAGE_IN_KERNEL        = 0x8000000,

    gcvSHADER_VIRCG_NONE                = 0x10000000,
    gcvSHADER_VIRCG_ONE                 = 0x20000000,

    gcvSHADER_MIN_COMP_TIME             = 0x40000000,

    /* Link program pipeline object. */
    gcvSHADER_LINK_PROGRAM_PIPELINE_OBJ = 0x80000000,
}
gceSHADER_FLAGS;

typedef struct _gceSHADER_SUB_FLAGS
{
    gctUINT     dual16PrecisionRule;
}
gceSHADER_SUB_FLAGS;

#if gcdUSE_WCLIP_PATCH
gceSTATUS
gcSHADER_CheckClipW(
    IN gctCONST_STRING VertexSource,
    IN gctCONST_STRING FragmentSource,
    OUT gctBOOL * clipW);
#endif

/*******************************************************************************
**                            gcOptimizer Data Structures
*******************************************************************************/
typedef enum _gceSHADER_OPTIMIZATION
{
    /*  No optimization. */
    gcvOPTIMIZATION_NONE,

    /*  Dead code elimination. */
    gcvOPTIMIZATION_DEAD_CODE                   = 1 << 0,

    /*  Redundant move instruction elimination. */
    gcvOPTIMIZATION_REDUNDANT_MOVE              = 1 << 1,

    /*  Inline expansion. */
    gcvOPTIMIZATION_INLINE_EXPANSION            = 1 << 2,

    /*  Constant propagation. */
    gcvOPTIMIZATION_CONSTANT_PROPAGATION        = 1 << 3,

    /*  Redundant bounds/checking elimination. */
    gcvOPTIMIZATION_REDUNDANT_CHECKING          = 1 << 4,

    /*  Vector component operation merge. */
    gcvOPTIMIZATION_VECTOR_INSTRUCTION_MERGE    = 1 << 5,

    /*  Loadtime constant. */
    gcvOPTIMIZATION_LOADTIME_CONSTANT           = 1 << 6,

    /*  MAD instruction optimization. */
    gcvOPTIMIZATION_MAD_INSTRUCTION             = 1 << 7,

    gcvOPTIMIZATION_LOAD_SW_W                   = 1 << 8,

    /*  Move code into conditional block if possile */
    gcvOPTIMIZATION_CONDITIONALIZE              = 1 << 9,

    /*  Expriemental: power optimization mode
        1. add extra dummy texld to tune performance
        2. insert NOP after high power instrucitons
        3. split high power vec3/vec4 instruciton to vec2/vec1 operation
        4. ...
     */
    gcvOPTIMIZATION_POWER_OPTIMIZATION          = 1 << 10,

    /*  Update precision */
    gcvOPTIMIZATION_UPDATE_PRECISION            = 1 << 11,

    /*  Loop rerolling */
    gcvOPTIMIZATION_LOOP_REROLLING              = 1 << 12,

    /*  Image patching: */
    /*     Inline functions using IMAGE_READ or IMAGE_WRITE. */
    gcvOPTIMIZATION_IMAGE_PATCHING              = 1 << 13,

    /*  Optimize a recompiler shader. */
    gcvOPTIMIZATION_RECOMPILER                  = 1 << 14,

    /*  Constant argument propagation. */
    gcvOPTIMIZATION_CONSTANT_ARGUMENT_PROPAGATION = 1 << 15,

    /* Inline level setting. */
    gcvOPTIMIZATION_INLINE_LEVEL_0              = 1 << 16,
    gcvOPTIMIZATION_INLINE_LEVEL_1              = 1 << 17,
    gcvOPTIMIZATION_INLINE_LEVEL_2              = 1 << 18,
    gcvOPTIMIZATION_INLINE_LEVEL_3              = 1 << 19,
    gcvOPTIMIZATION_INLINE_LEVEL_4              = 1 << 20,

    gcvOPTIMIZATION_MIN_COMP_TIME               = 1 << 21,

    /*  Full optimization. */
    /*  Note that gcvOPTIMIZATION_LOAD_SW_W is off. */
    gcvOPTIMIZATION_FULL                        = 0x7FFFFFFF &
                                                  ~gcvOPTIMIZATION_LOAD_SW_W &
                                                  ~gcvOPTIMIZATION_POWER_OPTIMIZATION &
                                                  ~gcvOPTIMIZATION_IMAGE_PATCHING &
                                                  ~gcvOPTIMIZATION_RECOMPILER &
                                                  ~gcvOPTIMIZATION_INLINE_LEVEL_0 &
                                                  ~gcvOPTIMIZATION_INLINE_LEVEL_1 &
                                                  ~gcvOPTIMIZATION_INLINE_LEVEL_2 &
                                                  ~gcvOPTIMIZATION_INLINE_LEVEL_3 &
                                                  ~gcvOPTIMIZATION_INLINE_LEVEL_4 &
                                                  ~gcvOPTIMIZATION_MIN_COMP_TIME
,

    /* Optimization Unit Test flag. */
    gcvOPTIMIZATION_UNIT_TEST                   = 1 << 31

}
gceSHADER_OPTIMIZATION;

typedef enum _gceOPTIMIZATION_VaryingPaking
{
    gcvOPTIMIZATION_VARYINGPACKING_NONE = 0,
    gcvOPTIMIZATION_VARYINGPACKING_NOSPLIT,
    gcvOPTIMIZATION_VARYINGPACKING_SPLIT
} gceOPTIMIZATION_VaryingPaking;


/* The highp in the shader appear because
   1) APP defined as highp
   2) promote from mediump per HW requirements
   3) promote from mediump based on the following rules.
      These rules can be disabled individually.
      Driver will detect benchmark and shader and set
      these rules accordingly.
      VC_OPTION can override these rules.
   */
typedef enum _Dual16_PrecisionRule
{
    /*  No dual16 highp rules applied. */
    Dual16_PrecisionRule_NONE                   = 0,

    /*  promote the texld coordiante (from varying) to highp */
    Dual16_PrecisionRule_TEXLD_COORD_HP         = 1 << 0,

    /*  promote rcp src/dest to highp */
    Dual16_PrecisionRule_RCP_HP                 = 1 << 1,

    /*  promote frac src/dest to highp */
    Dual16_PrecisionRule_FRAC_HP                = 1 << 2,

    /*  immediate is always highp */
    Dual16_PrecisionRule_IMMED_HP               = 1 << 3,

    /*  immediate is always mediump */
    Dual16_PrecisionRule_IMMED_MP               = 1 << 4,

    /* HW Cvt2OutColFmt has issue with 0x2,
    thus require output to be highp */
    Dual16_PrecisionRule_OUTPUT_HP              = 1 << 5,

    /*  default rules */
    Dual16_PrecisionRule_DEFAULT                = Dual16_PrecisionRule_TEXLD_COORD_HP |
                                                  Dual16_PrecisionRule_RCP_HP         |
                                                  Dual16_PrecisionRule_FRAC_HP,

    /*  applied all rules */
    Dual16_PrecisionRule_FULL                   = Dual16_PrecisionRule_TEXLD_COORD_HP |
                                                  Dual16_PrecisionRule_RCP_HP         |
                                                  Dual16_PrecisionRule_FRAC_HP        |
                                                  Dual16_PrecisionRule_IMMED_HP,

}
Dual16_PrecisionRule;

typedef struct _ShaderSourceList ShaderSourceList;
struct _ShaderSourceList
{
    gctINT             shaderId;
    gctINT             sourceSize;
    gctCHAR *          src;
    gctSTRING          fileName;
    ShaderSourceList * next;
};
enum MacroDefineKind
{
    MDK_Define,
    MDK_Undef
};

typedef struct _MacroDefineList MacroDefineList;
struct _MacroDefineList
{
    enum MacroDefineKind kind;
    gctCHAR *            str;    /* name[=value] */
    MacroDefineList *    next;
};

enum ForceInlineKind
{
    FIK_None,
    FIK_Inline,
    FIK_NotInline
};

typedef struct _InlineStringList InlineStringList;
struct _InlineStringList
{
    enum ForceInlineKind kind;
    gctCHAR *            func;   /* function name to force inline/notInline */
    InlineStringList *   next;
};

typedef enum _VIRCGKind
{
    VIRCG_None          = 0,
    VIRCG_DEFAULT       = 1,
    VIRCG_WITH_TREECG   = 1, /* go through VIR pass, but use gcSL LinkerTree to generate MC */
    VIRCG_FULL          = 2, /* go through VIR pass and use VIR Full linker to generate MC */
}VIRCGKind;

typedef struct _gcOPTIMIZER_OPTION
{
    gceSHADER_OPTIMIZATION     optFlags;

    /* debug & dump options:

         VC_OPTION=-DUMP:SRC|:IR|:OPT|:OPTV|:CG|:CGV|:HTP|:ALL|:ALLV|:UNIFORM|:T[-]m,n|RENUM:[0|1]

         SRC:  dump shader source code
         IR:   dump final IR
         SRCLOC: dump IR's corresponding source location
         OPT:  dump incoming and final IR
         OPTV: dump result IR in each optimization phase
         CG:   dump generated machine code
         CGV:  dump BE tree and optimization detail
         HTP:  dump hash table performance
         LOG:  dump FE log file in case of compiler error
         Tm:   turn on dump for shader id m
         Tm,n: turn on dump for shader id is in range of [m, n]
         T-m:  turn off dump for shader id m
         T-m,n: turn off dump for shader id is in range of [m, n]
         ALL = SRC|OPT|CG|LOG
         ALLV = SRC|OPT|OPTV|CG|CGV|LOG

         UNIFORM: dump uniform value when setting uniform
         RENUM:[0|1]: re-number instruction id when dumping IR
     */
    gctBOOL     dumpShaderSource;      /* dump shader source code */
    gctBOOL     dumpOptimizer;         /* dump incoming and final IR */
    gctBOOL     dumpOptimizerVerbose;  /* dump result IR in each optimization phase */
    gctBOOL     dumpBEGenertedCode;    /* dump generated machine code */
    gctBOOL     dumpBEVerbose;         /* dump BE tree and optimization detail */
    gctBOOL     dumpBEFinalIR;         /* dump BE final IR */
    gctBOOL     dumpSrcLoc;            /* dump IR instruction's corresponding source location*/
    gctBOOL     dumpFELog;             /* dump FE log file in case of compiler error */
    gctBOOL     dumpPPedStr2File;      /* dump FE preprocessed string to file */
    gctBOOL     dumpUniform;           /* dump uniform value when setting uniform */
    gctBOOL     dumpSpirvIR;           /* dump VIR shader convert from SPIRV */
    gctBOOL     dumpSpirvToFile;       /* dump SPRIV to file */
    gctBOOL     dumpBinToFile;         /* dump program binary to file when calling gcLoadProgram */
    gctBOOL     dumpHashPerf;          /* dump hash table performance */
    gctINT      _dumpStart;            /* shader id start to dump */
    gctINT      _dumpEnd;              /* shader id end to dump */
    gctINT      renumberInst;          /* re-number instruction when dumping IR */
    gctINT      includeLib;            /* dump library shader too (library shader won't be dumped by default) */

    /* Code generation */

    /* Varying Packing:

          VC_OPTION=-PACKVARYING:[0-2]|:T[-]m[,n]|:LshaderIdx,min,max

          0: turn off varying packing
          1: pack varyings, donot split any varying
          2: pack varyings, may split to make fully packed output

          Tm:    only packing shader pair which vertex shader id is m
          Tm,n:  only packing shader pair which vertex shader id
                   is in range of [m, n]
          T-m:   do not packing shader pair which vertex shader id is m
          T-m,n: do not packing shader pair which vertex shader id
                   is in range of [m, n]

          LshaderIdx,min,max : set  load balance (min, max) for shaderIdx
                               if shaderIdx is -1, all shaders are impacted
                               newMin = origMin * (min/100.);
                               newMax = origMax * (max/100.);
     */
    gceOPTIMIZATION_VaryingPaking    packVarying;
    gctINT                           _triageStart;
    gctINT                           _triageEnd;
    gctINT                           _loadBalanceShaderIdx;
    gctINT                           _loadBalanceMin;
    gctINT                           _loadBalanceMax;

    /* Do not generate immdeiate

          VC_OPTION=-NOIMM

       Force generate immediate even the machine model don't support it,
       for testing purpose only

          VC_OPTION=-FORCEIMM
     */
    gctBOOL     noImmediate;
    gctBOOL     forceImmediate;

    /* Power reduction mode options */
    gctBOOL   needPowerOptimization;

     /* If need to dump hash table performance, set hash table max search times,
        and if the fact search times is more than the max times, also add the number of max search times up,
        if not to set this option, the max search times is 0, that means can not to statistic search times

              VC_OPTION=-HMST:value
    */
    gctINT      hashMaxSearchTimes;

    /* Patch TEXLD instruction by adding dummy texld
       (can be used to tune GPU power usage):
         for every TEXLD we seen, add n dummy TEXLD

        it can be enabled by environment variable:

          VC_OPTION=-PATCH_TEXLD:M:N

        (for each M texld, add N dummy texld)
     */
    gctINT      patchEveryTEXLDs;
    gctINT      patchDummyTEXLDs;

    /* Insert NOP after high power consumption instructions

         VC_OPTION="-INSERTNOP:MUL:MULLO:DP3:DP4:SEENTEXLD"
     */
    gctBOOL     insertNOP;
    gctBOOL     insertNOPAfterMUL;
    gctBOOL     insertNOPAfterMULLO;
    gctBOOL     insertNOPAfterDP3;
    gctBOOL     insertNOPAfterDP4;
    gctBOOL     insertNOPOnlyWhenTexldSeen;

    /* split MAD to MUL and ADD:

         VC_OPTION=-SPLITMAD
     */
    gctBOOL     splitMAD;

    /* Convert vect3/vec4 operations to multiple vec2/vec1 operations

         VC_OPTION=-SPLITVEC:MUL:MULLO:DP3:DP4
     */
    gctBOOL     splitVec;
    gctBOOL     splitVec4MUL;
    gctBOOL     splitVec4MULLO;
    gctBOOL     splitVec4DP3;
    gctBOOL     splitVec4DP4;

    /* turn/off features:

          VC_OPTION=-F:n,[0|1]
          Note: n must be decimal number
     */
    gctUINT     featureBits;

    /* Replace specified shader's source code with the contents in
       specified file:

         VC_OPTION=-SHADER:id1,file1[:id2,file ...]

    */
    ShaderSourceList * shaderSrcList;

    /* Load-time Constant optimization:

        VC_OPTION=-LTC:0|1

    */
    gctBOOL     enableLTC;

    /* debug option:

        VC_OPTION=-DEBUG:0|1|2|3

    */
    gctUINT     enableDebug;

    /* VC_OPTION=-Ddef1[=value1] -Ddef2[=value2] -Uundef1 */
    MacroDefineList * macroDefines;

    /* inliner kind (default 1 VIR inliner):

          VC_OPTION=-INLINER:[0-1]
             0:  gcsl inliner
             1:  VIR inliner

        When VIRCG is not enabled, gcsl inliner is always used.
     */
    gctUINT     inlinerKind;

    /* inline level (default 2 at O1):

          VC_OPTION=-INLINELEVEL:[0-4]
             0:  no inline
             1:  only inline the function only called once or small function
             2:  inline functions be called less than 5 times or medium size function
             3:  inline everything possible within inline budget
             4:  inline everything possible disregard inline budget
     */
    gctUINT     inlineLevel;

    /* inline recompilation functions for depth comparison if inline level is not 0.
       (default 1)

          VC_OPTION=-INLINEDEPTHCOMP:[0-3]
             0:  follows inline level
             1:  inline depth comparison functions for halti2
             2:  inline depth comparison functions for halti1
             3:  inline depth comparison functions for halti0
     */
    gctBOOL     inlineDepthComparison;

    /* inline recompilation functions for format conversion if inline level is not 0.
       (default 1)

          VC_OPTION=-INLINEFORMATCONV:[0-3]
             0:  follows inline level
             1:  inline format conversion functions for halti2
             2:  inline format conversion functions for halti1
             3:  inline format conversion functions for halti0
     */
    gctUINT     inlineFormatConversion;

    /* this is a test only option

       VC_OPTION=-TESSLEVEL:x
       default: GC_DEFAULT_TESS_LEVEL

       set the gl_TessLevelOuter and gl_TessLevelInner to be this value
       when it is not GC_DEFAULT_TESS_LEVEL
    */
    gctUINT     testTessLevel;

    /* dual 16 mode
     *
     *    VC_OPTION=-DUAL16:[0-3]
     *       0:  force dual16 off.
     *       1:  auto-on mode for specific benchmarks.
     *       2:  auto-on mode for all applications.
     *       3:  force dual16 on for all applications ignoring the heuristic.
     */
    gctBOOL     dual16Specified;
    gctUINT     dual16Mode;
    gctINT      _dual16Start;           /* shader id start to enable dual16 */
    gctINT      _dual16End;             /* shader id end to enalbe dual16 */

    /* dual 16 highp rule
    *
    *    VC_OPTION=-HPDUAL16:[0-x]
    *       0: no dual16 highp rules applied
    *       1: Dual16_PrecisionRule_TEXLD_COORD_HP
    *       2: Dual16_PrecisionRule_RCP_HP
    *       4: Dual16_PrecisionRule_FRAC_HP
    *       8: Dual16_PrecisionRule_IMMED_HP
    *       default is Dual16_PrecisionRule_FULL
    *           (which is Dual16_PrecisionRule_TEXLD_COORD_HP |
    *                     Dual16_PrecisionRule_RCP_HP         |
    *                     Dual16_PrecisionRule_FRAC_HP        |
    *                     Dual16_PrecisionRule_IMMED_HP)
    */
    gctUINT     dual16PrecisionRule;

    /* whether the user set dual16PrecisionRule */
    gctBOOL     dual16PrecisionRuleFromEnv;

    /* force inline or not inline a function
     *
     *   VC_OPTION=-FORCEINLINE:func[,func]*
     *
     *   VC_OPTION=-NOTINLINE:func[,func]*
     *
     */
    InlineStringList *   forceInline;

    /* Upload Uniform Block to state buffer if there are space available
     * Doing this may potentially improve the performance as the load
     * instruction for uniform block member can be removed.
     *
     *   VC_OPTION=-UPLOADUBO:0|1
     *
     */
    gctBOOL     uploadUBO;

    /* OpenCL floating point capabilities setting
     * FASTRELAXEDMATH => -cl-fast-relaxed-math option
     * FINITEMATHONLY => -cl-finite-math-only option
     * RTNE => Round To Even
     * RTZ => Round to Zero
     *
     * VC_OPTION=-OCLFPCAPS:FASTRELAXEDMATH:FINITEMATHONLY:RTNE:RTZ
     */
    gctUINT     oclFpCaps;

    /* use VIR code generator:
     *
     *   VC_OPTION=-VIRCG:[0|1]|T[-]m[,n]
     *    Tm:    turn on VIRCG for shader id m
     *    Tm,n:  turn on VIRCG for shader id is in range of [m, n]
     *    T-m:   turn off VIRCG for shader id m
     *    T-m,n: turn off VIRCG for shader id is in range of [m, n]
     *
     */
    VIRCGKind   useVIRCodeGen;
    /* useVIRCodeGen maybe changed for specific test, we need to save the orignal option */
    VIRCGKind   origUseVIRCodeGen;

    gctBOOL     virCodeGenSpecified;
    gctINT      _vircgStart;
    gctINT      _vircgEnd;

    /* create default UBO:
     *
     *   VC_OPTION=-CREATEDEAULTUBO:0|1
     *
     */
    gctBOOL     createDefaultUBO;

    /*
     * Handle OCL basic type as packed
     *
     *   VC_OPTION=-OCLPACKEDBASICTYPE:0|1
     *
     */
    gctBOOL     oclPackedBasicType;

    /*
     * Handle OCL  relaxing local address space in OCV
     *
     *   VC_OPTION=-OCLOCVLOCALADDRESSSPACE:0|1
     *
     */
    gctBOOL    oclOcvLocalAddressSpace;

    /*
     * Handle OCL  in OPENCV
     *
     *   VC_OPTION=-OCLOPENCV:0|1
     *
     */
    gctBOOL    oclOpenCV;

    /*  OCL has long:
     *
     *   VC_OPTION=-OCLHASLONG:0|1
     *
     */
    gctBOOL     oclHasLong;

    /*  OCL long and ulong support in VIR:
     *
     *   VC_OPTION=-OCLINT64INVIR:0|1
     *
     */
    gctBOOL     oclInt64InVir;

    /*  OCL uniforms for constant address space variables
     *
     *   VC_OPTION=-OCLUNIFORMFORCONSTANT:0|1
     *
     */
    gctBOOL     oclUniformForConstant;

    /*  USE gcSL_NEG for -a instead of SUB(0, a)
     *
     *   VC_OPTION=-OCLUSENEG
     *
     */
    gctBOOL     oclUseNeg;

    /*  USE img intrinsic query function for OCL
     *
     *   VC_OPTION=-OCLUSEIMG_INTRINSIC_QUERY:0|1
     *
     */
    gctBOOL     oclUseImgIntrinsicQuery;

    /*  Pass kernel struct arguments by value in OCL
     *
     *   VC_OPTION=-OCLPASS_KERNEL_STRUCT_ARG_BY_VALUE:0|1
     *
     */
    gctBOOL     oclPassKernelStructArgByValue;

    /*  Treat half types as floats in OCL
     *
     *   VC_OPTION=-OCLTREAT_HALF_AS_FLOAT:0|1
     *
    */
    gctBOOL     oclTreatHalfAsFloat;

    /* Specify the log file name
     *
     *   VC_OPTION=-LOG:filename
     */
    gctSTRING   logFileName;
    gctFILE     debugFile;

    /* turn on/off shader patch:
     *   VC_OPTION=-PATCH:[0|1]|T[-]m[,n]
     *    Tm:    turn on shader patch for shader id m
     *    Tm,n:  turn on shader patch for shader id is in range of [m, n]
     *    T-m:   turn off shader patch for shader id m
     *    T-m,n: turn off shader patch for shader id is in range of [m, n]
     */
    gctBOOL     patchShader;
    gctINT      _patchShaderStart;
    gctINT      _patchShaderEnd;

    /* set default fragment shader floating point precision if not specified in shader
     *   VC_OPTION=-FRAGMENT_FP_PRECISION:[highp|mediump|lowp]
     *    highp: high precision
     *    mediump:  medium precision
     *    lowp:   low precision
     */
    gcSHADER_PRECISION   fragmentFPPrecision;

    /* OCL use VIR code generator:
     *
     *   VC_OPTION=-CLVIRCG:[0|1]|T[-]m[,n]
     *    Tm:    turn on VIRCG for OCL shader id m
     *    Tm,n:  turn on VIRCG for OCL shader id is in range of [m, n]
     *    T-m:   turn off VIRCG for OCL shader id m
     *    T-m,n: turn off VIRCG for OCL shader id is in range of [m, n]
     */
    gctBOOL     CLUseVIRCodeGen;

    /* Enable register pack in old compiler:

        VC_OPTION=-PACKREG:0|1

    */
    gctBOOL     enablePackRegister;

    /* Operate shader files :
     *
     * VC_OPTION=-LIBSHADERFILE:0|1|2
     *  0:  Unable to operate shader files
     *  1:  Enable write/read Shader info to/from file
     *  2:  Force rewrite shader info to file
     */
    gctINT     libShaderFile;

    /* whether driver use new VIR path for driver programming
     * passed in by driver
     */
    gctBOOL     DriverVIRPath;

    /* Close all optimization fro OCL debugger */
    gctBOOL     disableOptForDebugger;

    /* NOTE: when you add a new option, you MUST initialize it with default
       value in theOptimizerOption too */
} gcOPTIMIZER_OPTION;

/* DUAL16_FORCE_OFF:  turn off dual16
   DUAL16_AUTO_BENCH: turn on dual16 for selected benchmarks
   DUAL16_AUTO_ALL:   turn on dual16 for all
   DUAL16_FORCE_ON:   we have heuristic to turn off dual16 if the single-t instructions are too many,
                      this option will ignore the heuristic
*/
#define DUAL16_FORCE_OFF            0
#define DUAL16_AUTO_BENCH           1
#define DUAL16_AUTO_ALL             2
#define DUAL16_FORCE_ON             3

#define VC_OPTION_OCLFPCAPS_FASTRELAXEDMATH     (1 << 0 )
#define VC_OPTION_OCLFPCAPS_FINITEMATHONLY      (1 << 1 )
#define VC_OPTION_OCLFPCAPS_ROUNDTOEVEN         (1 << 2 )
#define VC_OPTION_OCLFPCAPS_ROUNDTOZERO         (1 << 3 )
#define VC_OPTION_OCLFPCAPS_NOFASTRELAXEDMATH   (1 << 4 )

#define GCSL_INLINER_KIND        0
#define VIR_INLINER_KIND         1

extern gcOPTIMIZER_OPTION theOptimizerOption;

#define gcmGetOptimizerOption() gcGetOptimizerOption()
#define gcmGetOptimizerOptionVariable() gcGetOptimizerOptionVariable()

#define gcmOPT_DUMP_Start()     (gcmGetOptimizerOption()->_dumpStart)
#define gcmOPT_DUMP_End()     (gcmGetOptimizerOption()->_dumpEnd)

#define gcmOPT_DUMP_SHADER_SRC()        (gcmGetOptimizerOption()->dumpShaderSource != 0)
#define gcmOPT_DUMP_OPTIMIZER_VERBOSE() (gcmGetOptimizerOption()->dumpOptimizerVerbose != 0)
#define gcmOPT_DUMP_OPTIMIZER()         (gcmGetOptimizerOption()->dumpOptimizer != 0 || gcmOPT_DUMP_OPTIMIZER_VERBOSE())
#define gcmOPT_DUMP_CODEGEN_VERBOSE()   (gcmGetOptimizerOption()->dumpBEVerbose != 0)
#define gcmOPT_DUMP_CODEGEN()           (gcmGetOptimizerOption()->dumpBEGenertedCode != 0 || gcmOPT_DUMP_CODEGEN_VERBOSE())
#define gcmOPT_DUMP_FINAL_IR()          (gcmGetOptimizerOption()->dumpBEFinalIR != 0)
#define gcmOPT_DUMP_UNIFORM()           (gcmGetOptimizerOption()->dumpUniform != 0)
#define gcmOPT_DUMP_FELOG()             (gcmGetOptimizerOption()->dumpFELog != 0)
#define gcmOPT_DUMP_PPEDSTR2FILE()      (gcmGetOptimizerOption()->dumpPPedStr2File != 0)
#define gcmOPT_DUMP_SRCLOC()            (gcmGetOptimizerOption()->dumpSrcLoc != 0)

#define gcmOPT_SET_DUMP_SHADER_SRC(v)   (gcmGetOptimizerOption()->dumpShaderSource = (v)

#define gcmOPT_PATCH_TEXLD()  (gcmGetOptimizerOption()->patchDummyTEXLDs != 0)
#define gcmOPT_INSERT_NOP()   (gcmGetOptimizerOption()->insertNOP == gcvTRUE)
#define gcmOPT_SPLITMAD()     (gcmGetOptimizerOption()->splitMAD == gcvTRUE)
#define gcmOPT_SPLITVEC()     (gcmGetOptimizerOption()->splitVec == gcvTRUE)

#define gcmOPT_NOIMMEDIATE()  (gcmGetOptimizerOption()->noImmediate == gcvTRUE)
#define gcmOPT_FORCEIMMEDIATE()  (gcmGetOptimizerOption()->forceImmediate == gcvTRUE)

#define gcmOPT_PACKVARYING()     (gcmGetOptimizerOption()->packVarying)
#define gcmOPT_PACKVARYING_triageStart()   (gcmGetOptimizerOption()->_triageStart)
#define gcmOPT_PACKVARYING_triageEnd()     (gcmGetOptimizerOption()->_triageEnd)
#define gcmOPT_SetVaryingPacking(VP)       do { gcmOPT_PACKVARYING() = VP; } while (0)

#define gcmOPT_LB_ShaderIdx()  (gcmGetOptimizerOption()->_loadBalanceShaderIdx)
#define gcmOPT_LB_Min()        (gcmGetOptimizerOption()->_loadBalanceMin)
#define gcmOPT_LB_Max()        (gcmGetOptimizerOption()->_loadBalanceMax)

#define gcmOPT_ShaderSourceList() (gcmGetOptimizerOption()->shaderSrcList)
#define gcmOPT_MacroDefines()     (gcmGetOptimizerOption()->macroDefines)

#define gcmOPT_EnableLTC()          (gcmGetOptimizerOption()->enableLTC)
#define gcmOPT_EnableDebug()        (gcmGetOptimizerOption()->enableDebug > 0)
#define gcmOPT_EnableDebugDump()    (gcmGetOptimizerOption()->enableDebug > 1)
#define gcmOPT_EnableDebugDumpALL() (gcmGetOptimizerOption()->enableDebug > 2)
#define gcmOPT_GetDebugLevel()      (gcmGetOptimizerOption()->enableDebug)
#define gcmOPT_SetDebugLevel(level) (gcmGetOptimizerOption()->enableDebug = level)
#define gcmOPT_INLINERKIND()        (gcmGetOptimizerOption()->inlinerKind)
#define gcmOPT_INLINELEVEL()        (gcmGetOptimizerOption()->inlineLevel)
#define gcmOPT_SetINLINELEVEL(v)    (gcmGetOptimizerOptionVariable()->inlineLevel = (v))
#define gcmOPT_INLINEDEPTHCOMP()    (gcmGetOptimizerOption()->inlineDepthComparison)
#define gcmOPT_INLINEFORMATCONV()   (gcmGetOptimizerOption()->inlineFormatConversion)
#define gcmOPT_DualFP16Specified()  (gcmGetOptimizerOption()->dual16Specified)
#define gcmOPT_DualFP16Mode()       (gcmGetOptimizerOption()->dual16Mode)
#define gcmOPT_DualFP16Start()      (gcmGetOptimizerOption()->_dual16Start)
#define gcmOPT_DualFP16End()        (gcmGetOptimizerOption()->_dual16End)

#define gcmOPT_DisableOPTforDebugger()     (gcmGetOptimizerOption()->disableOptForDebugger)
#define gcmOPT_SetDisableOPTforDebugger(b) (gcmGetOptimizerOption()->disableOptForDebugger = b)

#define gcmOPT_TESSLEVEL()          (gcmGetOptimizerOption()->testTessLevel)

#define gcmOPT_DualFP16PrecisionRule()          (gcmGetOptimizerOption()->dual16PrecisionRule)
#define gcmOPT_DualFP16PrecisionRuleFromEnv()   (gcmGetOptimizerOption()->dual16PrecisionRuleFromEnv)

#define gcmOPT_ForceInline()        (gcmGetOptimizerOption()->forceInline)
#define gcmOPT_UploadUBO()          (gcmGetOptimizerOption()->uploadUBO)
#define gcmOPT_oclFpCaps()          (gcmGetOptimizerOption()->oclFpCaps)
#define gcmOPT_oclPackedBasicType() (gcmGetOptimizerOption()->oclPackedBasicType)
#define gcmOPT_oclOcvLocalAddressSpace() (gcmGetOptimizerOption()->oclOcvLocalAddressSpace)
#define gcmOPT_oclOpenCV()          (gcmGetOptimizerOption()->oclOpenCV)
#define gcmOPT_oclHasLong()         (gcmGetOptimizerOption()->oclHasLong)
#define gcmOPT_oclInt64InVIR()      (gcmGetOptimizerOption()->oclInt64InVir)
#define gcmOPT_oclUniformForConstant()   (gcmGetOptimizerOption()->oclUniformForConstant)
#define gcmOPT_oclUseNeg()          (gcmGetOptimizerOption()->oclUseNeg)
#define gcmOPT_oclUseImgIntrinsicQuery()   (gcmGetOptimizerOption()->oclUseImgIntrinsicQuery)
#define gcmOPT_oclPassKernelStructArgByValue()   (gcmGetOptimizerOption()->oclPassKernelStructArgByValue)
#define gcmOPT_oclTreatHalfAsFloat()   (gcmGetOptimizerOption()->oclTreatHalfAsFloat)
#define gcmOPT_UseVIRCodeGen()      (gcmGetOptimizerOption()->useVIRCodeGen)
#define gcmOPT_VirCodeGenSpecified()(gcmGetOptimizerOption()->virCodeGenSpecified)
#define gcmOPT_VIRCGStart()         (gcmGetOptimizerOption()->_vircgStart)
#define gcmOPT_VIRCGEnd()           (gcmGetOptimizerOption()->_vircgEnd)
#define gcmOPT_CLUseVIRCodeGen()    (gcmGetOptimizerOption()->CLUseVIRCodeGen)
#define gcmOPT_DriverVIRPath()      (gcmGetOptimizerOption()->DriverVIRPath)
#define gcmOPT_CreateDefaultUBO()   (gcmGetOptimizerOption()->createDefaultUBO)
#define gcmOPT_PatchShader()        (gcmGetOptimizerOption()->patchShader)
#define gcmOPT_PatchShaderStart()   (gcmGetOptimizerOption()->_patchShaderStart)
#define gcmOPT_PatchShaderEnd()     (gcmGetOptimizerOption()->_patchShaderEnd)
#define gcmOPT_PackRegister()       (gcmGetOptimizerOption()->enablePackRegister)
#define gcmOPT_LibShaderFile()       (gcmGetOptimizerOption()->libShaderFile)

#define gcmOPT_SetOclPackedBasicType(val) do { (gcmGetOptimizerOption()->oclPackedBasicType = (val)); } while(0)

#define gcmOPT_FragmentFPPrecision() (gcmGetOptimizerOption()->fragmentFPPrecision)

extern gctBOOL gcSHADER_GoVIRPass(gcSHADER Shader);
extern gctBOOL gcSHADER_DoPatch(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpSource(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpOptimizer(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpOptimizerVerbose(gcSHADER Shader);
extern gctBOOL gcSHADER_DumpCodeGen(void * Shader);
extern gctBOOL gcSHADER_DumpCodeGenVerbose(void * Shader);
extern gctBOOL VirSHADER_DumpCodeGenVerbose(void * Shader);
extern gctBOOL gcSHADER_DumpFinalIR(gcSHADER Shader);
extern gctBOOL VirSHADER_DoDual16(gctINT ShaderId);
extern gctBOOL gcDoTriageForShaderId(gctINT shaderId, gctINT startId, gctINT endId);

/* Setters */
/* feature bits */
#define FB_LIVERANGE_FIX1                   0x0001
#define FB_INLINE_RENAMETEMP                0x0002
#define FB_UNLIMITED_INSTRUCTION            0x0004
#define FB_DISABLE_PATCH_CODE               0x0008
#define FB_DISABLE_MERGE_CONST              0x0010
#define FB_DISABLE_OLD_DCE                  0x0020
#define FB_INSERT_MOV_INPUT                 0x0040  /* insert MOV Rn, Rn for input to help HW team to debug */
#define FB_ENABLE_FS_OUT_INIT               0x0080  /* enable Fragment shader output
                                                       initialization if it is un-initialized */
#define FB_ENABLE_CONST_BORDER              0x0100  /* enable const border value, driver need to set #constBorderValue uniform */
#define FB_FORCE_LS_ACCESS                  0x8000  /* triage use: enforce all load/store as local storage access,
                                                       remove this feature bit once local storage access is supported */
#define FB_FORCE_USC_UNALLOC                0x10000 /* triage use: enforce all load/store as USC Unalloc  */

#define FB_TREAT_CONST_ARRAY_AS_UNIFORM     0x20000 /* Treat a const array as a uniform,
                                                       it can decrease the temp registers but increases the constant registers. */
#define FB_DISABLE_GL_LOOP_UNROLLING        0x40000 /* Disable loop unrolling for GL FE. */

#define FB_GENERATED_OFFLINE_COMPILER       0x80000 /* Enable Offline Compile . */

#define FB_VSIMULATOR_RUNNING_MODE          0x100000 /* VSimulator running mode. */

#define FB_VIV_VX_KERNEL                    0x200000        /* A OVX kernel, we need to replace this by using gceAPI. */

#define gcmOPT_SetPatchTexld(m,n) (gcmGetOptimizerOption()->patchEveryTEXLDs = (m),\
                                   gcmGetOptimizerOption()->patchDummyTEXLDs = (n))
#define gcmOPT_SetSplitVecMUL() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4MUL = gcvTRUE)
#define gcmOPT_SetSplitVecMULLO() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                  gcmGetOptimizerOption()->splitVec4MULLO = gcvTRUE)
#define gcmOPT_SetSplitVecDP3() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4DP3 = gcvTRUE)
#define gcmOPT_SetSplitVecDP4() (gcmGetOptimizerOption()->splitVec = gcvTRUE, \
                                 gcmGetOptimizerOption()->splitVec4DP4 = gcvTRUE)
#define gcmOPT_getFeatureBits(FBit)   (gcmGetOptimizerOption()->featureBits)
#define gcmOPT_hasFeature(FBit)   ((gcmGetOptimizerOption()->featureBits & (FBit)) != 0)
#define gcmOPT_SetFeature(FBit)   do { gcmGetOptimizerOption()->featureBits |= (FBit); } while (0)
#define gcmOPT_ResetFeature(FBit) do { gcmGetOptimizerOption()->featureBits &= ~(FBit); } while (0)

extern void gcOPT_SetFeature(gctUINT FBit);
extern void gcOPT_ResetFeature(gctUINT FBit);

#define gcmOPT_SetPackVarying(v)      (gcmGetOptimizerOption()->packVarying = v)

#define gcmOPT_SetDual16PrecisionRule(v)     (gcmGetOptimizerOption()->dual16PrecisionRule = v)

/* temporarily change PredefinedDummySamplerId from 7 to 8 */
#define PredefinedDummySamplerId       8

/* Function argument qualifier */
typedef enum _gceINPUT_OUTPUT
{
    gcvFUNCTION_INPUT,
    gcvFUNCTION_OUTPUT,
    gcvFUNCTION_INOUT
}
gceINPUT_OUTPUT;

typedef enum _gceVARIABLE_UPDATE_FLAGS
{
    gcvVARIABLE_UPDATE_NOUPDATE = 0,
    gcvVARIABLE_UPDATE_TEMPREG,
    gcvVARIABLE_UPDATE_TYPE_QUALIFIER,
}gceVARIABLE_UPDATE_FLAGS;

typedef enum _gcePROVOKING_VERTEX_CONVENSION
{
    gcvPROVOKING_VERTEX_FIRST = 0,
    gcvPROVOKING_VERTEX_LAST,
    gcvPROVOKING_VERTEX_UNDEFINE
}
gcePROVOKING_VERTEX_CONVENSION;

#define __DEFAULT_GLSL_EXTENSION_STRING__       "GL_OES_texture_storage_multisample_2d_array "\
                                                "GL_KHR_blend_equation_advanced "\
                                                "GL_EXT_texture_buffer "\
                                                "GL_EXT_texture_cube_map_array "\
                                                "GL_EXT_shader_io_blocks "\
                                                "GL_EXT_gpu_shader5 "\
                                                "GL_EXT_geometry_shader "\
                                                "GL_EXT_geometry_point_size "\
                                                "GL_EXT_tessellation_shader "\
                                                "GL_EXT_tessellation_point_size "\
                                                "GL_OES_sample_variables "\
                                                "GL_OES_shader_multisample_interpolation"

typedef struct _gcsGLSLCaps
{
    gctUINT maxDrawBuffers;
    gctUINT maxSamples;
    gctUINT maxVertTextureImageUnits;
    gctUINT maxCmptTextureImageUnits;
    gctUINT maxFragTextureImageUnits;
    gctUINT maxTcsTextureImageUnits;
    gctUINT maxTesTextureImageUnits;
    gctUINT maxGsTextureImageUnits;
    gctUINT maxCombinedTextureImageUnits;
    gctUINT maxTextureSamplers;
    gctINT  minProgramTexelOffset;
    gctINT  maxProgramTexelOffset;
    gctINT  minProgramTexGatherOffset;
    gctINT  maxProgramTexGatherOffset;

    gctUINT maxVertAttributes;
    gctUINT maxUserVertAttributes;
    gctUINT maxVertStreams;
    gctUINT maxBuildInVertAttributes;
    gctUINT maxVaryingVectors;
    gctUINT maxVertOutVectors;
    gctUINT maxFragInVectors;
    gctUINT maxTcsOutVectors;
    gctUINT maxTcsOutPatchVectors;
    gctUINT maxTcsOutTotalVectors;
    gctUINT maxTesOutVectors;
    gctUINT maxGsOutVectors;
    gctUINT maxTcsInVectors;
    gctUINT maxTesInVectors;
    gctUINT maxGsInVectors;
    gctUINT maxGsOutTotalVectors;

    gctUINT maxVertUniformVectors;
    gctUINT maxFragUniformVectors;
    gctUINT maxCmptUniformVectors;
    gctUINT maxTcsUniformVectors;
    gctUINT maxTesUniformVectors;
    gctUINT maxGsUniformVectors;
    gctINT  maxUniformLocations;

    /* buffer bindings */
    gctUINT uniformBufferOffsetAlignment;
    gctUINT maxUniformBufferBindings;
    gctUINT maxVertUniformBlocks;
    gctUINT maxFragUniformBlocks;
    gctUINT maxCmptUniformBlocks;
    gctUINT maxTcsUniformBlocks;
    gctUINT maxTesUniformBlocks;
    gctUINT maxGsUniformBlocks;
    gctUINT maxCombinedUniformBlocks;
    gctUINT64 maxUniformBlockSize;
    gctUINT64 maxCombinedVertUniformComponents;
    gctUINT64 maxCombinedFragUniformComponents;
    gctUINT64 maxCombinedCmptUniformComponents;
    gctUINT64 maxCombinedTcsUniformComponents;
    gctUINT64 maxCombinedTesUniformComponents;
    gctUINT64 maxCombinedGsUniformComponents;

    gctUINT maxVertAtomicCounters;
    gctUINT maxFragAtomicCounters;
    gctUINT maxCmptAtomicCounters;
    gctUINT maxTcsAtomicCounters;
    gctUINT maxTesAtomicCounters;
    gctUINT maxGsAtomicCounters;
    gctUINT maxCombinedAtomicCounters;
    gctUINT maxVertAtomicCounterBuffers;
    gctUINT maxFragAtomicCounterBuffers;
    gctUINT maxCmptAtomicCounterBuffers;
    gctUINT maxTcsAtomicCounterBuffers;
    gctUINT maxTesAtomicCounterBuffers;
    gctUINT maxGsAtomicCounterBuffers;
    gctUINT maxCombinedAtomicCounterBuffers;
    gctUINT maxAtomicCounterBufferBindings;
    gctUINT64 maxAtomicCounterBufferSize;

    gctUINT shaderStorageBufferOffsetAlignment;
    gctUINT maxVertShaderStorageBlocks;
    gctUINT maxFragShaderStorageBlocks;
    gctUINT maxCmptShaderStorageBlocks;
    gctUINT maxTcsShaderStorageBlocks;
    gctUINT maxTesShaderStorageBlocks;
    gctUINT maxGsShaderStorageBlocks;
    gctUINT maxCombinedShaderStorageBlocks;
    gctUINT maxShaderStorageBufferBindings;
    gctUINT64 maxShaderBlockSize;

    gctUINT maxXfbInterleavedComponents;
    gctUINT maxXfbSeparateComponents;
    gctUINT maxXfbSeparateAttribs;
    gctUINT maxXfbBuffers;

    gctUINT maxProgErrStrLen;

    /* Image limits  */
    gctUINT maxVertexImageUniform;
    gctUINT maxFragImageUniform;
    gctUINT maxCmptImageUniform;
    gctUINT maxTcsImageUniform;
    gctUINT maxTesImageUniform;
    gctUINT maxGsImageUniform;
    gctUINT maxImageUnit;
    gctUINT maxCombinedImageUniform;
    gctUINT maxCombinedShaderOutputResource;

    /* Compute limits */
    gctUINT maxWorkGroupCount[3];
    gctUINT maxWorkGroupSize[3];
    gctUINT maxWorkGroupInvocation;
    gctUINT maxShareMemorySize;

    /* TS-only limits */
    gctUINT maxTessPatchVertices;
    gctUINT maxTessGenLevel;
    gctBOOL tessPatchPR;

    /* GS-only limits */
    gctUINT maxGsOutVertices;
    gcePROVOKING_VERTEX_CONVENSION provokingVertex;
    gctUINT maxGsInvocationCount;

    /* Primitive Clipling limits */
    gctUINT maxClipDistances;
    gctUINT maxCullDistances;
    gctUINT maxCombinedClipAndCullDistances;

    /* Desktop GL limits */
    gctUINT maxLights;
    gctUINT maxClipPlanes;
    gctUINT maxFragmentUniformComponents;
    gctUINT maxTextureCoords;
    gctUINT maxTextureUnits;
    gctUINT maxVaryingComponents;
    gctUINT maxVaryingFloats;
    gctUINT maxVertexUniformComponents;
    gctUINT maxFragmentInputComponents;
    gctUINT maxVertexOutputComponents;
    gctUINT maxGSVaryingComponents;

    /* GLSL extension string. */
    gctSTRING extensions;
} gcsGLSLCaps;

/* PatchID*/
extern gcePATCH_ID gcPatchId;
extern gcePATCH_ID *
    gcGetPatchId(
    void
    );

#define GetPatchID()                          (gcGetPatchId())

/* HW caps.*/
typedef struct _VSC_HW_CONFIG gcsHWCaps;
extern gcsHWCaps gcHWCaps;
extern gcsHWCaps *
    gcGetHWCaps(
    void
    );

/* Get HW features. */
#define GetHWHasHalti0()                      (gcGetHWCaps()->hwFeatureFlags.hasHalti0)
#define GetHWHasHalti1()                      (gcGetHWCaps()->hwFeatureFlags.hasHalti1)
#define GetHWHasHalti2()                      (gcGetHWCaps()->hwFeatureFlags.hasHalti2)
#define GetHWHasHalti5()                      (gcGetHWCaps()->hwFeatureFlags.hasHalti5)
#define GetHWHasAdvancedInst()                (gcGetHWCaps()->hwFeatureFlags.supportAdvancedInsts)
#define GetHWHasFmaSupport()                  (GetHWHasHalti5() && GetHWHasAdvancedInst())
#define GetHWHasLoadStoreConv4RoundingMode()  gcvFALSE
#define GetHWHasFullPackedModeSupport()       gcvFALSE
#define GetHWHasTS()                          (gcGetHWCaps()->hwFeatureFlags.supportTS)
#define GetHWHasGS()                          (gcGetHWCaps()->hwFeatureFlags.supportGS)
#define GetHWHasSamplerBaseOffset()           (gcGetHWCaps()->hwFeatureFlags.hasSamplerBaseOffset)
#define GetHWHasUniversalTexldV2()            (gcGetHWCaps()->hwFeatureFlags.hasUniversalTexldV2)
#define GetHWHasTexldUFix()                   (gcGetHWCaps()->hwFeatureFlags.hasTexldUFix)
#define GetHWHasImageOutBoundaryFix()         (gcGetHWCaps()->hwFeatureFlags.hasImageOutBoundaryFix)

/* Get HW caps. */
#define GetHWVertexSamplerBase()              (gcGetHWCaps()->vsSamplerRegNoBase)
#define GetHWFragmentSamplerBase()            (gcGetHWCaps()->psSamplerRegNoBase)

#define GetHWInitWorkGroupSizeToCalcRegCount()(gcGetHWCaps()->initWorkGroupSizeToCalcRegCount)
#define GetHWMaxWorkGroupSize()               (gcGetHWCaps()->maxWorkGroupSize)
#define GetHWMinWorkGroupSize()               (gcGetHWCaps()->minWorkGroupSize)

/* GLSL caps. */
extern gcsGLSLCaps gcGLSLCaps;
extern gcsGLSLCaps *
    gcGetGLSLCaps(
    void
    );
extern gceSTATUS gcInitGLSLCaps(
    OUT gcsGLSLCaps *Caps
    );

#define GetGLMaxVertexAttribs()               (gcGetGLSLCaps()->maxUserVertAttributes)
#define GetGLMaxVertexUniformVectors()        (gcGetGLSLCaps()->maxVertUniformVectors)
#define GetGLMaxVertexOutputVectors()         (gcGetGLSLCaps()->maxVertOutVectors)
#define GetGLMaxFragmentInputVectors()        (gcGetGLSLCaps()->maxFragInVectors)
/* Texture Image. */
#define GetGLMaxVertexTextureImageUnits()     (gcGetGLSLCaps()->maxVertTextureImageUnits)
#define GetGLMaxCombinedTextureImageUnits()   (gcGetGLSLCaps()->maxCombinedTextureImageUnits)
#define GetGLMaxFragTextureImageUnits()       (gcGetGLSLCaps()->maxFragTextureImageUnits)
#define GetGLMaxFragmentUniformVectors()      (gcGetGLSLCaps()->maxFragUniformVectors)
#define GetGLMaxDrawBuffers()                 (gcGetGLSLCaps()->maxDrawBuffers)
#define GetGLMaxSamples()                     (gcGetGLSLCaps()->maxSamples)

#define GetGLMaxUniformLocations()            (gcGetGLSLCaps()->maxUniformLocations)
#define GetGLMinProgramTexelOffset()          (gcGetGLSLCaps()->minProgramTexelOffset)
#define GetGLMaxProgramTexelOffset()          (gcGetGLSLCaps()->maxProgramTexelOffset)
/* Image. */
#define GetGLMaxImageUnits()                  (gcGetGLSLCaps()->maxImageUnit)
#define GetGLMaxVertexImageUniforms()         (gcGetGLSLCaps()->maxVertexImageUniform)
#define GetGLMaxFragmentImageUniforms()       (gcGetGLSLCaps()->maxFragImageUniform)
#define GetGLMaxComputeImageUniforms()        (gcGetGLSLCaps()->maxCmptImageUniform)
#define GetGLMaxCombinedImageUniforms()       (gcGetGLSLCaps()->maxCombinedImageUniform)
#define GetGLMaxCombinedShaderOutputResources() (gcGetGLSLCaps()->maxCombinedShaderOutputResource)
#define GetGLMaxComputeWorkGroupCount(Index)  (gcGetGLSLCaps()->maxWorkGroupCount[(Index)])
#define GetGLMaxComputeWorkGroupSize(Index)   (gcGetGLSLCaps()->maxWorkGroupSize[(Index)])
#define GetGLMaxWorkGroupInvocation()         (gcGetGLSLCaps()->maxWorkGroupInvocation)
#define GetGLMaxSharedMemorySize()            (gcGetGLSLCaps()->maxShareMemorySize)
#define GetGLMaxComputeUniformComponents()    (gcGetGLSLCaps()->maxCmptUniformVectors * 4)
#define GetGLMaxComputeTextureImageUnits()    (gcGetGLSLCaps()->maxCmptTextureImageUnits)
/* Atomic Counters. */
#define GetGLMaxComputeAtomicCounters()       (gcGetGLSLCaps()->maxCmptAtomicCounters)
#define GetGLMaxComputeAtomicCounterBuffers() (gcGetGLSLCaps()->maxCmptAtomicCounterBuffers)
#define GetGLMaxVertexAtomicCounters()        (gcGetGLSLCaps()->maxVertAtomicCounters)
#define GetGLMaxFragmentAtomicCounters()      (gcGetGLSLCaps()->maxFragAtomicCounters)
#define GetGLMaxCombinedAtomicCounters()      (gcGetGLSLCaps()->maxCombinedAtomicCounters)
#define GetGLMaxAtomicCounterBindings()       (gcGetGLSLCaps()->maxAtomicCounterBufferBindings)
#define GetGLMaxVertexAtomicCounterBuffers()  (gcGetGLSLCaps()->maxVertAtomicCounterBuffers)
#define GetGLMaxFragmentAtomicCounterBuffers()(gcGetGLSLCaps()->maxFragAtomicCounterBuffers)
#define GetGLMaxCombinedAtomicCounterBuffers()(gcGetGLSLCaps()->maxCombinedAtomicCounterBuffers)
#define GetGLMaxAtomicCounterBufferSize()     (gcGetGLSLCaps()->maxAtomicCounterBufferSize)
#define GetGLMaxVaryingVectors()              (gcGetGLSLCaps()->maxVaryingVectors)
/* Storage Buffer. */
#define GetGLMaxVertexShaderStorageBufferBindings()     (gcGetGLSLCaps()->maxVertShaderStorageBlocks)
#define GetGLMaxFragmentShaderStorageBufferBindings()   (gcGetGLSLCaps()->maxFragShaderStorageBlocks)
#define GetGLMaxComputeShaderStorageBufferBindings()    (gcGetGLSLCaps()->maxCmptShaderStorageBlocks)
#define GetGLMaxShaderStorageBufferBindings()           (gcGetGLSLCaps()->maxShaderStorageBufferBindings)
/* Uniform Buffer. */
#define GetGLMaxVertexUniformBufferBindings()           (gcGetGLSLCaps()->maxVertUniformBlocks)
#define GetGLMaxFragmentUniformBufferBindings()         (gcGetGLSLCaps()->maxFragUniformBlocks)
#define GetGLMaxComputeUniformBufferBindings()          (gcGetGLSLCaps()->maxCmptUniformBlocks)
#define GetGLMaxCombinedUniformBufferBindings()         (gcGetGLSLCaps()->maxUniformBufferBindings)
#define GetGLMaxUniformBLockSize()            (gcGetGLSLCaps()->maxUniformBlockSize)

/* TS constants */
#define GetGLMaxTCSTextureImageUnits()        (gcGetGLSLCaps()->maxTcsTextureImageUnits)
#define GetGLMaxTCSUniformVectors()           (gcGetGLSLCaps()->maxTcsUniformVectors)
#define GetGLMaxTCSOutTotalVectors()          (gcGetGLSLCaps()->maxTcsOutTotalVectors)
#define GetGLMaxTCSInputVectors()             (gcGetGLSLCaps()->maxTcsInVectors)
#define GetGLMaxTCSOutputVectors()            (gcGetGLSLCaps()->maxTcsOutVectors)
#define GetGLMaxTCSAtomicCounters()           (gcGetGLSLCaps()->maxTcsAtomicCounters)
#define GetGLMaxTCSAtomicCounterBuffers()     (gcGetGLSLCaps()->maxTcsAtomicCounterBuffers)
#define GetGLMaxTCSImageUniforms()            (gcGetGLSLCaps()->maxTcsImageUniform)
#define GetGLMaxTCSUniformBufferBindings()    (gcGetGLSLCaps()->maxTcsUniformBlocks)
#define GetGLMaxTCSShaderStorageBufferBindings()     (gcGetGLSLCaps()->maxTcsShaderStorageBlocks)

#define GetGLMaxTESTextureImageUnits()        (gcGetGLSLCaps()->maxTesTextureImageUnits)
#define GetGLMaxTESUniformVectors()           (gcGetGLSLCaps()->maxTesUniformVectors)
#define GetGLMaxTESOutTotalVectors()          (gcGetGLSLCaps()->maxTesOutTotalVectors)
#define GetGLMaxTESInputVectors()             (gcGetGLSLCaps()->maxTesInVectors)
#define GetGLMaxTESOutputVectors()            (gcGetGLSLCaps()->maxTesOutVectors)
#define GetGLMaxTESAtomicCounters()           (gcGetGLSLCaps()->maxTesAtomicCounters)
#define GetGLMaxTESAtomicCounterBuffers()     (gcGetGLSLCaps()->maxTesAtomicCounterBuffers)
#define GetGLMaxTESImageUniforms()            (gcGetGLSLCaps()->maxTesImageUniform)
#define GetGLMaxTESUniformBufferBindings()    (gcGetGLSLCaps()->maxTesUniformBlocks)
#define GetGLMaxTESShaderStorageBufferBindings()     (gcGetGLSLCaps()->maxTesShaderStorageBlocks)

#define GetGLMaxTessPatchVertices()           (gcGetGLSLCaps()->maxTessPatchVertices)
#define GetGLMaxTessGenLevel()                (gcGetGLSLCaps()->maxTessGenLevel)
#define GetGLMaxTessPatchVectors()            (gcGetGLSLCaps()->maxTcsOutPatchVectors)

/* GS constants. */
#define GetGLMaxGSTextureImageUnits()         (gcGetGLSLCaps()->maxGsTextureImageUnits)
#define GetGLMaxGSOutVectors()                (gcGetGLSLCaps()->maxGsOutVectors)
#define GetGLMaxGSInVectors()                 (gcGetGLSLCaps()->maxGsInVectors)
#define GetGLMaxGSOutTotalVectors()           (gcGetGLSLCaps()->maxGsOutTotalVectors)
#define GetGLMaxGSUniformVectors()            (gcGetGLSLCaps()->maxGsUniformVectors)
#define GetGLMaxGSUniformBlocks()             (gcGetGLSLCaps()->maxGsUniformBlocks)
#define GetGLMaxGSAtomicCounters()            (gcGetGLSLCaps()->maxGsAtomicCounters)
#define GetGLMaxGSAtomicCounterBuffers()      (gcGetGLSLCaps()->maxGsAtomicCounterBuffers)
#define GetGLMaxGSImageUniforms()             (gcGetGLSLCaps()->maxGsImageUniform)
#define GetGLMaxGSOutVertices()               (gcGetGLSLCaps()->maxGsOutVertices)
#define GetGLMaxGSInvocationCount()           (gcGetGLSLCaps()->maxGsInvocationCount)
#define GetGLMaxGSUniformBufferBindings()     (gcGetGLSLCaps()->maxGsUniformBlocks)
#define GetGLMaxGSShaderStorageBufferBindings()     (gcGetGLSLCaps()->maxGsShaderStorageBlocks)

#define GetGLMaxClipDistances()               (gcGetGLSLCaps()->maxClipDistances)
#define GetGLMaxCullDistances()               (gcGetGLSLCaps()->maxCullDistances)
#define GetGLMaxCombinedClipCullDistances()   (gcGetGLSLCaps()->maxCombinedClipAndCullDistances)

/* Desktop GL constants */
#define GetGLMaxLights()                      (gcGetGLSLCaps()->maxLights)
#define GetGLMaxClipPlanes()                  (gcGetGLSLCaps()->maxClipPlanes)
#define GetGLMaxFragmentUniformComponents()   (gcGetGLSLCaps()->maxFragmentUniformComponents)
#define GetGLMaxTextureCoords()               (gcGetGLSLCaps()->maxTextureCoords)
#define GetGLMaxTextureUnits()                (gcGetGLSLCaps()->maxTextureUnits)
#define GetGLMaxVaryingComponents()           (gcGetGLSLCaps()->maxVaryingComponents)
#define GetGLMaxVaryingFloats()               (gcGetGLSLCaps()->maxVaryingFloats)
#define GetGLMaxVertexUniformComponents()     (gcGetGLSLCaps()->maxVertexUniformComponents)
#define GetGLMaxFragmentInputComponents()     (gcGetGLSLCaps()->maxFragmentInputComponents)
#define GetGLMaxVertexOutputComponents()      (gcGetGLSLCaps()->maxVertexOutputComponents)
#define GetGLMaxGSVaryingComponents()         (gcGetGLSLCaps()->maxGSVaryingComponents)

/* GLSL extension string. */
#define GetGLExtensionString()                (gcGetGLSLCaps()->extensions)

void
gcGetOptionFromEnv(
    IN OUT gcOPTIMIZER_OPTION * Option
    );

void
gcSetOptimizerOption(
    IN gceSHADER_FLAGS Flags
    );

gcOPTIMIZER_OPTION *
gcGetOptimizerOption(void);

gcOPTIMIZER_OPTION *
gcGetOptimizerOptionVariable(void);

gctBOOL
gcUseFullNewLinker(gctBOOL HasHalti2);

typedef gceSTATUS (*gctGLSLCompiler)(IN  gctINT ShaderType,
                                     IN  gctUINT SourceSize,
                                     IN  gctCONST_STRING Source,
                                     OUT gcSHADER *Binary,
                                     OUT gctSTRING *Log);

typedef gceSTATUS (*gctGLSLInitCompiler)(IN gcePATCH_ID PatchId,
                                         IN gcsHWCaps *HWCaps,
                                         IN gcsGLSLCaps *Caps);

typedef gceSTATUS (*gctGLSLInitCompilerCaps)(IN gcsGLSLCaps *Caps);

typedef gceSTATUS (*gctGLSLFinalizeCompiler)(void);

void
gcSetGLSLCompiler(
    IN gctGLSLCompiler Compiler
    );

typedef gceSTATUS (*gctCLCompiler)(IN  gcoHAL Hal,
                                       IN  gctUINT SourceSize,
                                       IN  gctCONST_STRING Source,
                                       IN  gctCONST_STRING Option,
                                       OUT gcSHADER *Binary,
                                       OUT gctSTRING *Log);

void
gcSetCLCompiler(
    IN gctCLCompiler Compiler
    );

extern gcsGLSLCaps *gcGetGLSLcap(void);

/*******************************************************************************
**  gcSHADER_SetDefaultUBO
**
**  Set the compiler enable/disable default UBO.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to gcSHADER object
**
**      gctBOOL Enabled
**          Pointer to enable/disable default UBO
*/
gceSTATUS
gcSHADER_SetDefaultUBO(
    IN gcSHADER Shader,
    IN gctBOOL Enabled
    );

/*******************************************************************************
**  gcSHADER_SetCompilerVersion
**
**  Set the compiler version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to gcSHADER object
**
**      gctINT *Version
**          Pointer to a two word version
*/
gceSTATUS
gcSHADER_SetCompilerVersion(
    IN gcSHADER Shader,
    IN gctUINT32 *Version
    );

/*******************************************************************************
**  gcSHADER_SetClientApiVersion
**
**  Set the client API version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gceAPI API
**          Client API version.
*/
gceSTATUS
gcSHADER_SetClientApiVersion(
    IN gcSHADER Shader,
    IN gceAPI Api
    );

/*******************************************************************************
**  gcSHADER_GetCompilerVersion
**
**  Get the compiler version of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32_PTR *CompilerVersion.
**          Pointer to holder of returned compilerVersion pointer
*/
gceSTATUS
gcSHADER_GetCompilerVersion(
    IN gcSHADER Shader,
    OUT gctUINT32_PTR *CompilerVersion
    );

gctBOOL
gcSHADER_IsESCompiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsES11Compiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsES30Compiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsES31Compiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsES32Compiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsHaltiCompiler(
    IN gcSHADER Shader
    );

/*******************************************************************************
**  gcSHADER_IsOGLCompiler
**
**  Check if the shader is OGL shader.
**  Note: should use this API instead of VIR_Shader_IsDesktopGL() to do this check,
**        because detecting by clientApiVersion does not work in some cases.
**
*/
gctBOOL
gcSHADER_IsOGLCompiler(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsGL43(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsGL44(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_IsGL45(
    IN gcSHADER Shader
    );

gctBOOL
gcSHADER_SupportAliasedAttribute(
    IN gcSHADER      pShader
    );

/*******************************************************************************
**  gcSHADER_SetShaderID
**
**  Set a unique id for this shader base on shader source string.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctUINT32 ID
**          The value of shader id.
*/
gceSTATUS
gcSHADER_SetShaderID(
    IN gcSHADER  Shader,
    IN gctUINT32 ID
    );

/*******************************************************************************
**  gcSHADER_GetShaderID
**
**  Get the unique id of this shader.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctUINT32 * ID
**          The value of shader id.
*/
gceSTATUS
gcSHADER_GetShaderID(
    IN gcSHADER  Shader,
    IN gctUINT32 * ID
    );

/*******************************************************************************
**  gcSHADER_SetDisableEZ
**
**  Set disable EZ for this shader.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctBOOL Disabled
**          Disable or not.
*/
gceSTATUS
gcSHADER_SetDisableEZ(
    IN gcSHADER  Shader,
    IN gctBOOL Disabled
    );

/*******************************************************************************
**  gcSHADER_GetDisableEZ
**
**  Get disable EZ of this shader.
**
**  INPUT:
**
**      gcSHADER  Shader
**          Pointer to an shader object.
**
**      gctBOOL * Disabled
**          The value of Disable EZ.
*/
gceSTATUS
gcSHADER_GetDisableEZ(
    IN gcSHADER  Shader,
    IN gctBOOL * Disabled
    );

/*******************************************************************************
**  gcSHADER_GetType
**
**  Get the gcSHADER object's type.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctINT *Type.
**          Pointer to return shader type.
*/
gceSTATUS
gcSHADER_GetType(
    IN gcSHADER Shader,
    OUT gcSHADER_KIND *Type
    );

gctUINT
gcSHADER_NextId(void);

void
gcSHADER_AlignId(void);
/*******************************************************************************
**                             gcSHADER_Construct
********************************************************************************
**
**    Construct a new gcSHADER object.
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctINT ShaderType
**            Type of gcSHADER object to cerate.  'ShaderType' can be one of the
**            following:
**
**                gcSHADER_TYPE_VERTEX    Vertex shader.
**                gcSHADER_TYPE_FRAGMENT    Fragment shader.
**
**    OUTPUT:
**
**        gcSHADER * Shader
**            Pointer to a variable receiving the gcSHADER object pointer.
*/
gceSTATUS
gcSHADER_Construct(
    IN gctINT ShaderType,
    OUT gcSHADER * Shader
    );

/*******************************************************************************
**                              gcSHADER_Destroy
********************************************************************************
**
**    Destroy a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Destroy(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                              gcSHADER_Copy
********************************************************************************
**
**    Copy a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**      gcSHADER Source
**          Pointer to a gcSHADER object that will be copied.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Copy(
    IN gcSHADER Shader,
    IN gcSHADER Source
    );

/*******************************************************************************
**  gcSHADER_LoadHeader
**
**  Load a gcSHADER object from a binary buffer.  The binary buffer is layed out
**  as follows:
**      // Six word header
**      // Signature, must be 'S','H','D','R'.
**      gctINT8             signature[4];
**      gctUINT32           binFileVersion;
**      gctUINT32           compilerVersion[2];
**      gctUINT32           gcSLVersion;
**      gctUINT32           binarySize;
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**          Shader type will be returned if type in shader object is not gcSHADER_TYPE_PRECOMPILED
**
**      gctPOINTER Buffer
**          Pointer to a binary buffer containing the shader data to load.
**
**      gctUINT32 BufferSize
**          Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**  OUTPUT:
**      nothing
**
*/
gceSTATUS
gcSHADER_LoadHeader(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize,
    OUT gctUINT32 * ShaderVersion
    );

/*******************************************************************************
**  gcSHADER_LoadKernel
**
**  Load a kernel function given by name into gcSHADER object
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctSTRING KernelName
**          Pointer to a kernel function name
**
**  OUTPUT:
**      nothing
**
*/
gceSTATUS
gcSHADER_LoadKernel(
    IN gcSHADER Shader,
    IN gctSTRING KernelName
    );

/*     Remove unused register.
 */
gceSTATUS
gcSHADER_PackRegister(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                gcMergeKernel
********************************************************************************
**
**    Merge a list of OpenCL kernel binaries and form a single consistent kernel
**    binary as if the original kernel source corresponding to each kernel binary
**    were concatenated together into one source and then compiled to create the
**    kernel binary.
**
**    INPUT:
**        gctINT KernelCount
**            number of gcSHADER object in the shader array
**
**        gcSHADER *KernelArray
**            Array of gcSHADER object holding information about the compiled
**            openCL kernel.
**
**    OUTPUT:
**
**        gcSHADER * MergedKernel
**            Pointer to a variable receiving the handle to the merged kernel
**
*/
gceSTATUS
gcSHADER_MergeKernel(
    IN gctINT         KernelCount,
    IN gcSHADER *     KernelArray,
    OUT gcSHADER *    MergedKernel
    );

/*******************************************************************************
**                                gcMergeShader
********************************************************************************
**
**    Merge a list of OpenGL shader binaries and form a single consistent shader
**    binary
**
**    INPUT:
**        gctINT ShaderCount
**            number of gcSHADER object in the shader array
**
**        gcSHADER *ShaderArray
**            Array of gcSHADER object holding information about the compiled
**            openGL shader.
**
**    OUTPUT:
**
**        gcSHADER * MergedShader
**            Pointer to a variable receiving the handle to the merged shader
**
*/
gceSTATUS
gcSHADER_MergeShader(
    IN gctINT         ShaderCount,
    IN gcSHADER *     ShaderArray,
    OUT gcSHADER *    MergedShader
    );

/*******************************************************************************
**                                gcSHADER_SetGeoLayout
********************************************************************************
*/
gceSTATUS
gcSHADER_SetGeoLayout(
    IN gcSHADER          Shader,
    IN gctINT            geoMaxVertices,
    IN gcGeoPrimitive    geoInPrimitive,
    IN gcGeoPrimitive    geoOutPrimitive
    );

/*******************************************************************************
**                                gcSHADER_Load
********************************************************************************
**
**    Load a gcSHADER object from a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer containg the shader data to load.
**
**        gctUINT32 BufferSize
**            Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Load(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize
    );

/*******************************************************************************
**                                gcSHADER_Save
********************************************************************************
**
**    Save a gcSHADER object to a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer to be used as storage for the gcSHADER
**            object.  If 'Buffer' is gcvNULL, the gcSHADER object will not be saved,
**            but the number of bytes required to hold the binary output for the
**            gcSHADER object will be returned.
**
**        gctUINT32 * BufferSize
**            Pointer to a variable holding the number of bytes allocated in
**            'Buffer'.  Only valid if 'Buffer' is not gcvNULL.
**
**    OUTPUT:
**
**        gctUINT32 * BufferSize
**            Pointer to a variable receiving the number of bytes required to hold
**            the binary form of the gcSHADER object.
*/
gceSTATUS
gcSHADER_Save(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN OUT gctUINT32 * BufferSize
    );

/*******************************************************************************
**                                gcSHADER_LoadEx
********************************************************************************
**
**    Load a gcSHADER object from a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer containg the shader data to load.
**
**        gctUINT32 BufferSize
**            Number of bytes inside the binary buffer pointed to by 'Buffer'.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_LoadEx(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN gctUINT32 BufferSize
    );

/*******************************************************************************
**                                gcSHADER_SaveEx
********************************************************************************
**
**    Save a gcSHADER object to a binary buffer.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctPOINTER Buffer
**            Pointer to a binary buffer to be used as storage for the gcSHADER
**            object.  If 'Buffer' is gcvNULL, the gcSHADER object will not be saved,
**            but the number of bytes required to hold the binary output for the
**            gcSHADER object will be returned.
**
**        gctUINT32 * BufferSize
**            Pointer to a variable holding the number of bytes allocated in
**            'Buffer'.  Only valid if 'Buffer' is not gcvNULL.
**
**    OUTPUT:
**
**        gctUINT32 * BufferSize
**            Pointer to a variable receiving the number of bytes required to hold
**            the binary form of the gcSHADER object.
*/
gceSTATUS
gcSHADER_SaveEx(
    IN gcSHADER Shader,
    IN gctPOINTER Buffer,
    IN OUT gctUINT32 * BufferSize
    );

gceSTATUS
gcSHADER_LinkBuiltinLibs(
    IN gcSHADER* Shaders
    );

/*******************************************************************************
**  gcSHADER_GetLocationCount
**
**  Get the number of input/output locations for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of locations.
*/
gceSTATUS
gcSHADER_GetLocationCount(
    IN gcSHADER Shader,
    IN gctBOOL Input,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetLocation
**
**  Get the location assocated with an indexed input/output for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of input/output to retreive the location setting for.
**
**  OUTPUT:
**
**      gctINT * Location
**          Pointer to a variable receiving the location value.
*/
gceSTATUS
gcSHADER_GetLocation(
    IN gcSHADER Shader,
    IN gctUINT Index,
    IN gctBOOL Input,
    OUT gctINT * Location
    );

/*******************************************************************************
**  gcSHADER_GetBuiltinNameKind
**
**  Get the builtin name kind for the Name.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          String of the name.
**
**  OUTPUT:
**
**      gceBuiltinNameKind * Kind
**          Pointer to a variable receiving the builtin name kind value.
*/
gceSTATUS
gcSHADER_GetBuiltinNameKind(
    IN gcSHADER              Shader,
    IN gctCONST_STRING       Name,
    OUT gctUINT32 *          Kind
    );

/*******************************************************************************
**  gcSHADER_GetBuiltinNameString
**
**  Get the builtin name corresponding to its kind.
**
**  INPUT:
**
**      gctUINT32 Kind
**          Builtin name kind.
**
**  RETURN:
**
**      gctCONST_STRING
**          Pointer to the builtin name string.
*/
gctCONST_STRING
gcSHADER_GetBuiltinNameString(
    IN gctINT Kind
    );

/*******************************************************************************
**  gcSHADER_ReallocateAttributes
**
**  Reallocate an array of pointers to gcATTRIBUTE objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateAttributes(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                              gcSHADER_AddAttribute
********************************************************************************
**
**    Add an attribute to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the attribute to add.
**
**        gcSHADER_TYPE Type
**            Type of the attribute to add.
**
**        gctUINT32 Length
**            Array length of the attribute to add.  'Length' must be at least 1.
**
**        gctBOOL IsTexture
**            gcvTRUE if the attribute is used as a texture coordinate, gcvFALSE if not.
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_AddAttribute(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctBOOL IsTexture,
    IN gcSHADER_SHADERMODE ShaderMode,
    IN gcSHADER_PRECISION Precision,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_AddAttributeWithLocation
**
**  Add an attribute together with a location to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the attribute to add.
**
**      gcSHADER_TYPE Type
**          Type of the attribute to add.
**
**      gcSHADER_PRECISION Precision,
**          Precision of the attribute to add.
**
**      gctUINT32 Length
**          Array length of the attribute to add.  'Length' must be at least 1.
**
**      gctBOOL IsTexture
**          gcvTRUE if the attribute is used as a texture coordinate, gcvFALSE if not.
**
**      gctINT Location
**          Location associated with the attribute.
**
**  OUTPUT:
**
**      gcATTRIBUTE * Attribute
**          Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_AddAttributeWithLocation(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 Length,
    IN gctUINT32 ArrayLengthCount,
    IN gctBOOL IsTexture,
    IN gcSHADER_SHADERMODE ShaderMode,
    IN gctINT Location,
    IN gctINT FieldIndex,
    IN gctBOOL IsInvariant,
    IN gctBOOL IsPrecise,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_GetVertexInstIDInputIndex
**
**  Get the input index of vertex/instance ID for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
*/
gctINT
gcSHADER_GetVertexInstIdInputIndex(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                         gcSHADER_GetAttributeCount
********************************************************************************
**
**    Get the number of attributes for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of attributes.
*/
gceSTATUS
gcSHADER_GetAttributeCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetAttributeAndBuiltinInputCount
**
**  Get the number of attributes including builtin inputs for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of attributes including builtin inputs.
*/
gceSTATUS
gcSHADER_GetAttributeAndBuiltinInputCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                            gcSHADER_GetAttribute
********************************************************************************
**
**    Get the gcATTRIBUTE object poniter for an indexed attribute for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of the attribute to retrieve.
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_GetAttribute(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**                            gcSHADER_GetAttributeByName
********************************************************************************
**
**    Get the gcATTRIBUTE object poniter for an name attribute for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctSTRING name
**            Name of output to retrieve.
**
**        gctUINT32 nameLength
**            Length of name to retrieve
**
**    OUTPUT:
**
**        gcATTRIBUTE * Attribute
**            Pointer to a variable receiving the gcATTRIBUTE object pointer.
*/
gceSTATUS
gcSHADER_GetAttributeByName(
    IN gcSHADER Shader,
    IN gctSTRING Name,
    IN gctUINT32 NameLength,
    OUT gcATTRIBUTE * Attribute
    );

/*******************************************************************************
**  gcSHADER_ReallocateUniforms
**
**  Reallocate an array of pointers to gcUNIFORM objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateUniforms(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/* find the uniform with Name in the Shader,
 * if found, return it in *Uniform
 * otherwise add the uniform to shader
 */
gceSTATUS
gcSHADER_FindAddUniform(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gcSHADER_PRECISION Precision,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                               gcSHADER_AddUniform
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_AddUniform(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gcSHADER_PRECISION Precision,
    OUT gcUNIFORM * Uniform
    );


/*******************************************************************************
**                               gcSHADER_AddUniformEx
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**      gcSHADER_PRECISION precision
**          Precision of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_AddUniformEx(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION precision,
    IN gctUINT32 Length,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                               gcSHADER_AddUniformEx1
********************************************************************************
**
**    Add an uniform to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform to add.
**
**        gcSHADER_TYPE Type
**            Type of the uniform to add.
**
**      gcSHADER_PRECISION precision
**          Precision of the uniform to add.
**
**        gctUINT32 Length
**            Array length of the uniform to add.  'Length' must be at least 1.
**
**      gcSHADER_VAR_CATEGORY varCategory
**          Variable category, normal or struct.
**
**      gctUINT16 numStructureElement
**          If struct, its element number.
**
**      gctINT16 parent
**          If struct, parent index in gcSHADER.variables.
**
**      gctINT16 prevSibling
**          If struct, previous sibling index in gcSHADER.variables.
**
**      gctINT16 imageFormat
**          image format for the uniform to add
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
**
**      gctINT16* ThisUniformIndex
**          Returned value about uniform index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddUniformEx1(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION precision,
    IN gctINT32 location,
    IN gctINT32 binding,
    IN gctINT32 bindingOffset,
    IN gctINT ArrayLengthCount,
    IN gctINT * ArrayLengthList,
    IN gcSHADER_VAR_CATEGORY varCategory,
    IN gctUINT16 numStructureElement,
    IN gctINT16 parent,
    IN gctINT16 prevSibling,
    IN gctINT16 imageFormat,
    OUT gctINT16* ThisUniformIndex,
    OUT gcUNIFORM * Uniform
    );

/* create uniform for the constant vector and initialize it with Value */
gceSTATUS
gcSHADER_CreateConstantUniform(
    IN gcSHADER                  Shader,
    IN gcSHADER_TYPE             Type,
    IN gcsValue *                Value,
    OUT gcUNIFORM *              Uniform
    );

/* add uniform with compile-time initializer */
gceSTATUS
gcSHADER_AddUniformWithInitializer(
    IN gcSHADER                  Shader,
    IN gctCONST_STRING           Name,
    IN gcSHADER_TYPE             Type,
    IN gctUINT32                 Length,
    IN gcSHADER_PRECISION        Precision,
    IN gcsValue *                Value,
    OUT gcUNIFORM *              Uniform
    );

gcSL_FORMAT
gcGetFormatFromType(
    IN gcSHADER_TYPE Type
    );

/*******************************************************************************
**                          gcSHADER_GetUniformCount
********************************************************************************
**
**    Get the number of uniforms for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                          gcSHADER_GetSamplerCount
********************************************************************************
**
**    Get the number of samplers for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of samplers.
*/
gceSTATUS
gcSHADER_GetSamplerCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                          gcSHADER_GetKernelUniformCount
********************************************************************************
**
**    Get the number of kernel uniforms for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetKernelUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                          gcSHADER_GetKernelOriginalUniformCount
********************************************************************************
**
**    Get the number of kernel original uniforms for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of original uniforms.
*/
gceSTATUS
gcSHADER_GetKernelOriginalUniformCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformVectorCountByCategory
**
**  Get the number of vectors used by uniforms for this shader according to variable
**  category.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSHADER_VAR_CATEGORY Category
**          Category of uniform.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUniformVectorCountByCategory(
    IN gcSHADER Shader,
    IN gcSHADER_VAR_CATEGORY Category,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformVectorCount
**
**  Get the number of vectors used by uniforms for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUniformVectorCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformVectorCountUsedInShader
**
**  Get the number of vectors used by uniforms for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of vectors.
*/
gceSTATUS
gcSHADER_GetUniformVectorCountUsedInShader(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockCount
**
**  Get the number of uniform blocks for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of uniform blocks.
*/
gceSTATUS
gcSHADER_GetUniformBlockCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockCountUsedInShader
**
**  Get the number of uniform blocks for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of uniform blocks.
*/
gceSTATUS
gcSHADER_GetUniformBlockCountUsedInShader(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockUniformCount
**
**  Get the number of uniforms in a uniform block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to uniform block to retreive the uniform count.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of uniforms.
*/
gceSTATUS
gcSHADER_GetUniformBlockUniformCount(
    IN gcSHADER Shader,
    gcsUNIFORM_BLOCK UniformBlock,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetStorageBlockVariableCount
**
**  Get the number of variables in a storage block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsSTORAGE_BLOCK StorageBlock
**          Pointer to storage block to retreive the variable count.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of variables in block.
*/
gceSTATUS
gcSHADER_GetStorageBlockVariableCount(
    IN gcSHADER Shader,
    gcsSTORAGE_BLOCK StorageBlock,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_SetStorageBlockTopLevelMemberArrayInfo
**
**  Set the top level member array info (size, stride) of a storage block
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctINT VariableIndex
**          Index to variable in the storage block
**
**      gctINT TopMember
**          Flag indicating that variable is top level
**
**      gctINT ArraySize
**          Top level array size to be used on non top level variable
**
**      gctINT ArrayStride
**          Top level array stride to be used on non top level variable
**
*/
gceSTATUS
gcSHADER_SetStorageBlockTopLevelMemberArrayInfo(
    IN gcSHADER Shader,
    IN gctINT  VariableIndex,
    IN gctBOOL TopMember,
    IN gctINT ArraySize,
    IN gctINT ArrayStride
    );

/*******************************************************************************
**                             gcSHADER_GetUniform
********************************************************************************
**
**    Get the gcUNIFORM object pointer for an indexed uniform for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of the uniform to retrieve.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniform(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                             gcSHADER_GetUniformByUniformIndex
********************************************************************************
**
**    Get the gcUNIFORM object pointer for an indexed uniform itseft.
**    For a OCL shader, if it has loaded a specified a uniform,
**    the index in gcUNIFORM is not the same as the index in gcSHADER->uniforms,
**    so we can't get the uniform by using gcSHADER->uniforms[index].
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT16 Index
**            Index of the uniform to retrieve.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniformByUniformIndex(
    IN gcSHADER Shader,
    IN gctUINT16 Index,
    OUT gcUNIFORM * Uniform
    );

gceSTATUS
gcSHADER_GetUniformByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING UniformName,
    IN gctUINT32 NameLength,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlock
**
**  Get the gcsUNIFORM_BLOCK object pointer for an indexed uniform block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of uniform to retreive the name for.
**
**  OUTPUT:
**
**      gcsUNIFORM_BLOCK * UniformBlock
**          Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
*/
gceSTATUS
gcSHADER_GetUniformBlock(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcsUNIFORM_BLOCK * UniformBlock
    );

/*******************************************************************************
**  gcSHADER_GetUniformBlockUniform
**
**  Get the gcUNIFORM object pointer for an indexed uniform of an indexed uniform
**  block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to uniform block to retreive the uniform.
**
**      gctUINT Index
**          Index of uniform to retreive the name for.
**
**  OUTPUT:
**
**      gcUNIFORM * Uniform
**          Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniformBlockUniform(
    IN gcSHADER Shader,
    IN gcsUNIFORM_BLOCK UniformBlock,
    IN gctUINT Index,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**  gcSHADER_GetStorageBlockVariable
**
**  Get the gcVARIABLE object pointer for an indexed variable of a storage
**  block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsSTORAGE_BLOCK StorageBlock
**          Pointer to storage block to retreive the variable.
**
**      gctUINT Index
**          Index of variable to retreive the name for.
**
**  OUTPUT:
**
**      gcVARIABLE * Variable
**          Pointer to a variable receiving the gcVARIABLE object pointer.
*/
gceSTATUS
gcSHADER_GetStorageBlockVariable(
    IN gcSHADER Shader,
    IN gcsSTORAGE_BLOCK StorageBlock,
    IN gctUINT Index,
    OUT gcVARIABLE * Variable
    );

/*******************************************************************************
**                    gcSHADER_AddStorageBlock
********************************************************************************
**
**    Add a uniform block to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform block to add.
**
**        gcsSHADER_VAR_INFO *BlockInfo
**            block info associated with uniform block to be added.
**
**        gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout;
**             Memory layout qualifier for members in block
**
**    OUTPUT:
**
**        gcsUNIFORM_BLOCK * StorageBlock
**            Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
**
*/
gceSTATUS
gcSHADER_AddStorageBlock(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcsSHADER_VAR_INFO *BlockInfo,
    IN gceINTERFACE_BLOCK_LAYOUT_ID MemoryLayout,
    OUT gcsSTORAGE_BLOCK * StorageBlock
    );

/*******************************************************************************
**  gcSHADER_GetStorageBlockCount
**
**  Get the number of storage blocks for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of storage blocks.
*/
gceSTATUS
gcSHADER_GetStorageBlockCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetStorageBlock
**
**  Get the gcsSTORAGE_BLOCK object pointer for an indexed storage block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of storage block to retreive the name for.
**
**  OUTPUT:
**
**      gcsSTORAGE_BLOCK * StorageBlock
**          Pointer to a variable receiving the gcsSTORAGE_BLOCK object pointer.
*/
gceSTATUS
gcSHADER_GetStorageBlock(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcsSTORAGE_BLOCK * StorageBlock
    );

/*******************************************************************************
**  gcSHADER_GetIoBlockVariable
**
**  Get the gcVARIABLE object pointer for an indexed variable of a storage
**  block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsIO_BLOCK IoBlock
**          Pointer to io block to retreive the variable.
**
**      gctUINT Index
**          Index of variable to retreive the name for.
**
**  OUTPUT:
**
**      gcVARIABLE * Variable
**          Pointer to a variable receiving the gcVARIABLE object pointer.
*/
gceSTATUS
gcSHADER_GetIoBlockVariable(
    IN gcSHADER Shader,
    IN gcsIO_BLOCK IoBlock,
    IN gctUINT Index,
    OUT gcVARIABLE * Variable
    );

/*******************************************************************************
**  gcSHADER_GetIoBlockVariableCount
**
**  Get the number of variables in a io block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcsIO_BLOCK IoBlock
**          Pointer to io block to retreive the variable count.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of variables in block.
*/
gceSTATUS
gcSHADER_GetIoBlockVariableCount(
    IN gcSHADER Shader,
    gcsIO_BLOCK IoBlock,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                    gcSHADER_AddIoBlock
********************************************************************************
**
**    Add a IO block to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the IO block to add.
**
**        gctCONST_STRING InstanceName
**            Instance name of the IO block to add.
**
**        gcsSHADER_VAR_INFO *BlockInfo
**            block info associated with uniform block to be added.
**
**        gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout;
**             Memory layout qualifier for members in block
**
**    OUTPUT:
**
**        gcsUNIFORM_BLOCK * IoBlock
**            Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
**
*/
gceSTATUS
gcSHADER_AddIoBlock(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gctCONST_STRING InstanceName,
    IN gcsSHADER_VAR_INFO *BlockInfo,
    IN gceINTERFACE_BLOCK_LAYOUT_ID MemoryLayout,
    OUT gcsIO_BLOCK * IoBlock
    );

/*******************************************************************************
**  gcSHADER_GetIoBlockCount
**
**  Get the number of io blocks for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of io blocks.
*/
gceSTATUS
gcSHADER_GetIoBlockCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetIoBlock
**
**  Get the gcsIO_BLOCK object pointer for an indexed io block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of io block to retreive the name for.
**
**  OUTPUT:
**
**      gcsIO_BLOCK * IoBlock
**          Pointer to a variable receiving the gcsIO_BLOCK object pointer.
*/
gceSTATUS
gcSHADER_GetIoBlock(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcsIO_BLOCK * IoBlock
    );

/*******************************************************************************
**  gcSHADER_GetIoBlockByName
**
**  Get the gcsIO_BLOCK object pointer for an indexed io block for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING BlockName
**          Block name of io block to retreive the name for.
**
**      gctCONST_STRING InstanceName
**          Instance name of io block to retreive the name for.
**
**  OUTPUT:
**
**      gcsIO_BLOCK * IoBlock
**          Pointer to a variable receiving the gcsIO_BLOCK object pointer.
*/
gceSTATUS
gcSHADER_GetIoBlockByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING BlockName,
    IN gctCONST_STRING InstanceName,
    OUT gcsIO_BLOCK * IoBlock
    );

/*******************************************************************************
**                             gcSHADER_GetUniformIndexingRange
********************************************************************************
**
**    Get the gcUNIFORM object pointer for an indexed uniform for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctINT uniformIndex
**            Index of the start uniform.
**
**        gctINT offset
**            Offset to indexing.
**
**    OUTPUT:
**
**        gctINT * LastUniformIndex
**            Pointer to index of last uniform in indexing range.
**
**        gctINT * OffsetUniformIndex
**            Pointer to index of uniform that indexing at offset.
**
**        gctINT * DeviationInOffsetUniform
**            Pointer to offset in uniform picked up.
*/
gceSTATUS
gcSHADER_GetUniformIndexingRange(
    IN gcSHADER Shader,
    IN gctINT uniformIndex,
    IN gctINT offset,
    OUT gctINT * LastUniformIndex,
    OUT gctINT * OffsetUniformIndex,
    OUT gctINT * DeviationInOffsetUniform
    );

/*******************************************************************************
**                             gcSHADER_GetUniformByPhysicalAddress
********************************************************************************
**
**    Get the gcUNIFORM object pointer by physical address for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctINT Index
**            physical address of the uniform.
**
**    OUTPUT:
**
**        gcUNIFORM * Uniform
**            Pointer to a variable receiving the gcUNIFORM object pointer.
*/
gceSTATUS
gcSHADER_GetUniformByPhysicalAddress(
    IN gcSHADER Shader,
    IN gctINT PhysicalAddress,
    OUT gcUNIFORM * Uniform
    );

/*******************************************************************************
**                             gcSHADER_ComputeUniformPhysicalAddress
********************************************************************************
**
**    Compuate the gcUNIFORM object pointer for this shader.
**
**    INPUT:
**
**        gctUINT32 HwConstRegBases
**            Base physical addresses for the uniform.
**
**        gctUINT32 PsBaseAddress
**            Fragment Base physical address for the uniform.
**
**        gcUNIFORM Uniform
**            The uniform pointer.
**
**    OUTPUT:
**
**        gctUINT32 * PhysicalAddress
**            Pointer to a variable receiving the physical address.
*/
gceSTATUS
gcSHADER_ComputeUniformPhysicalAddress(
    IN gctUINT32 HwConstRegBases[],
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * PhysicalAddress
    );

/*******************************************************************************
**  gcSHADER_ReallocateUniformBlocks
**
**  Reallocate an array of pointers to gcUNIFORM_BLOCK objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateUniformBlocks(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                    gcSHADER_AddUniformBlock
********************************************************************************
**
**    Add a uniform block to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the uniform block to add.
**
**        gcsSHADER_VAR_INFO *BlockInfo
**            block info associated with uniform block to be added.
**
**        gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout;
**             Memory layout qualifier for members in block
**
**    OUTPUT:
**
**        gcsUNIFORM_BLOCK * Uniform
**            Pointer to a variable receiving the gcsUNIFORM_BLOCK object pointer.
**
*/
gceSTATUS
gcSHADER_AddUniformBlock(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcsSHADER_VAR_INFO *BlockInfo,
    IN gceINTERFACE_BLOCK_LAYOUT_ID  MemoryLayout,
    IN gctINT16 ArrayIndex,
    IN gctUINT16 ArrayLength,
    OUT gcsUNIFORM_BLOCK * UniformBlock
    );

/*******************************************************************************
**  gcSHADER_GetFunctionByName
**
**  Get the gcFUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionName
**          Name of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcFUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetFunctionByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING FunctionName,
    OUT gcFUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetFunctionByHeadIndex
**
**  Get the gcFUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionHeadIndex
**          Head index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcFUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetFunctionByHeadIndex(
    IN     gcSHADER         Shader,
    IN     gctUINT HeadIndex,
    OUT    gcFUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetKernelFunctionByHeadIndex
**
**  Get the gcKERNEL_FUNCTION object pointer for an named kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT FunctionHeadIndex
**          Head index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcKERNEL_FUNCTION * Function
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetKernelFunctionByHeadIndex(
    IN  gcSHADER            Shader,
    IN  gctUINT             HeadIndex,
    OUT gcKERNEL_FUNCTION * Function
    );

/*******************************************************************************
**  gcSHADER_GetKernelFucntion
**
**  Get the gcKERNEL_FUNCTION object pointer for an indexed kernel function for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT Index
**          Index of kernel function to retreive the name for.
**
**  OUTPUT:
**
**      gcKERNEL_FUNCTION * KernelFunction
**          Pointer to a variable receiving the gcKERNEL_FUNCTION object pointer.
*/
gceSTATUS
gcSHADER_GetKernelFunction(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

gceSTATUS
gcSHADER_GetKernelFunctionByName(
    IN gcSHADER Shader,
    IN gctSTRING KernelName,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

/*******************************************************************************
**  gcSHADER_GetKernelFunctionCount
**
**  Get the number of kernel functions for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * Count
**          Pointer to a variable receiving the number of kernel functions.
*/
gceSTATUS
gcSHADER_GetKernelFunctionCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_ReallocateOutputs
**
**  Reallocate an array of pointers to gcOUTPUT objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateOutputs(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                               gcSHADER_AddOutput
********************************************************************************
**
**    Add an output to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the output to add.
**
**        gcSHADER_TYPE Type
**            Type of the output to add.
**
**        gctUINT32 Length
**            Array length of the output to add.  'Length' must be at least 1.
**
**        gctUINT32 TempRegister
**            Temporary register index that holds the output value.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOutput(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctUINT32 TempRegister,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**  gcSHADER_AddOutputWithLocation
**
**  Add an output with an associated location to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the output to add.
**
**      gcSHADER_TYPE Type
**          Type of the output to add.
**
**    gcSHADER_PRECISION Precision
**          Precision of the output.
**
**      gctUINT32 Length
**          Array length of the output to add.  'Length' must be at least 1.
**
**      gctUINT32 TempRegister
**          Temporary register index that holds the output value.
**
**      gctINT Location
**          Location associated with the output.
**
**  OUTPUT:
**
**      gcOUTPUT * Output
**          Pointer to an output receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_AddOutputWithLocation(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gcSHADER_PRECISION Precision,
    IN gctBOOL IsArray,
    IN gctUINT32 Length,
    IN gctUINT32 TempRegister,
    IN gcSHADER_SHADERMODE shaderMode,
    IN gctINT Location,
    IN gctINT FieldIndex,
    IN gctBOOL IsInvariant,
    IN gctBOOL IsPrecise,
    OUT gcOUTPUT * Output
    );

gctINT
gcSHADER_GetOutputDefaultLocation(
    IN gcSHADER Shader
    );

gceSTATUS
gcSHADER_AddOutputIndexed(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gctUINT32 Index,
    IN gctUINT32 TempIndex
    );

/*******************************************************************************
**  gcOUTPUT_SetType
**
**  Set the type of an output.
**
**  INPUT:
**
**      gcOUTPUT Output
**          Pointer to a gcOUTPUT object.
**
**      gcSHADER_TYPE Type
**          Type of the output.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcOUTPUT_SetType(
    IN gcOUTPUT Output,
    IN gcSHADER_TYPE Type
    );

/*******************************************************************************
**                             gcSHADER_GetOutputCount
********************************************************************************
**
**    Get the number of outputs for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of outputs.
*/
gceSTATUS
gcSHADER_GetOutputCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**                               gcSHADER_GetOutput
********************************************************************************
**
**    Get the gcOUTPUT object pointer for an indexed output for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of output to retrieve.
**
**    OUTPUT:
**
**        gcOUTPUT * Output
**            Pointer to a variable receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_GetOutput(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcOUTPUT * Output
    );


/*******************************************************************************
**                               gcSHADER_GetOutputByName
********************************************************************************
**
**    Get the gcOUTPUT object pointer for this shader by output name.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctSTRING name
**            Name of output to retrieve.
**
**      gctUINT32 nameLength
**          Length of name to retrieve
**
**    OUTPUT:
**
**        gcOUTPUT * Output
**            Pointer to a variable receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_GetOutputByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING name,
    IN gctUINT32 nameLength,
    OUT gcOUTPUT * Output
    );

/*******************************************************************************
**                               gcSHADER_GetOutputByTempIndex
********************************************************************************
**
**    Get the gcOUTPUT object pointer for this shader by output temp index.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT32 TempIndex
**            Temp index of output to retrieve.
**
**
**    OUTPUT:
**
**        gcOUTPUT * Output
**            Pointer to a variable receiving the gcOUTPUT object pointer.
*/
gceSTATUS
gcSHADER_GetOutputByTempIndex(
    IN gcSHADER Shader,
    IN gctUINT32 TempIndex,
    OUT gcOUTPUT * Output
    );

gceSTATUS
gcSHADER_GetOutputIndexByOutput(
    IN gcSHADER Shader,
    IN gcOUTPUT Output,
    IN OUT gctINT16 * Index
    );

/*******************************************************************************
**  gcSHADER_ReallocateVariables
**
**  Reallocate an array of pointers to gcVARIABLE objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateVariables(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

/*******************************************************************************
**                               gcSHADER_AddVariable
********************************************************************************
**
**    Add a variable to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctCONST_STRING Name
**            Name of the variable to add.
**
**        gcSHADER_TYPE Type
**            Type of the variable to add.
**
**        gctUINT32 Length
**            Array length of the variable to add.  'Length' must be at least 1.
**
**        gctUINT32 TempRegister
**            Temporary register index that holds the variable value.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddVariable(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    IN gctUINT32 TempRegister
    );


/*******************************************************************************
**  gcSHADER_AddVariableEx
********************************************************************************
**
**  Add a variable to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the variable to add.
**
**      gcSHADER_TYPE Type
**          Type of the variable to add.
**
**      gctUINT32 Length
**          Array length of the variable to add.  'Length' must be at least 1.
**
**      gctUINT32 TempRegister
**          Temporary register index that holds the variable value.
**
**      gcSHADER_VAR_CATEGORY varCategory
**          Variable category, normal or struct.
**
**      gctUINT16 numStructureElement
**          If struct, its element number.
**
**      gctINT16 parent
**          If struct, parent index in gcSHADER.variables.
**
**      gctINT16 prevSibling
**          If struct, previous sibling index in gcSHADER.variables.
**
**  OUTPUT:
**
**      gctINT16* ThisVarIndex
**          Returned value about variable index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddVariableEx(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctINT ArrayLengthCount,
    IN gctINT * ArrayLengthList,
    IN gctUINT32 TempRegister,
    IN gcSHADER_VAR_CATEGORY varCategory,
    IN gctUINT8 Precision,
    IN gctUINT16 numStructureElement,
    IN gctINT16 parent,
    IN gctINT16 prevSibling,
    OUT gctINT16* ThisVarIndex
    );

/*******************************************************************************
**  gcSHADER_AddVariableEx1
**
**  Add a variable to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctCONST_STRING Name
**          Name of the variable to add.
**
**      gctUINT32 TempRegister
**          Temporary register index that holds the variable value.
**
**      gcsSHADER_VAR_INFO *VarInfo
**          Variable information struct pointer.
**
**  OUTPUT:
**
**      gctINT16* ThisVarIndex
**          Returned value about variable index in gcSHADER.
*/
gceSTATUS
gcSHADER_AddVariableEx1(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    IN gctUINT32 TempRegister,
    IN gcsSHADER_VAR_INFO *VarInfo,
    OUT gctINT16* ThisVarIndex
    );

/*******************************************************************************
**  gcSHADER_UpdateVariable
********************************************************************************
**
**  Update a variable to a gcSHADER object.
**
**  INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of variable to retrieve.
**
**        gceVARIABLE_UPDATE_FLAGS flag
**            Flag which property of variable will be updated.
**
**      gctUINT newValue
**          New value to update.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_UpdateVariable(
    IN gcSHADER Shader,
    IN gctUINT Index,
    IN gceVARIABLE_UPDATE_FLAGS flag,
    IN gctUINT newValue
    );

gceSTATUS
gcSHADER_CopyVariable(
    IN gcSHADER Shader,
    IN gcVARIABLE Variable,
    IN gctUINT16 * Index
    );

/*******************************************************************************
**                             gcSHADER_GetVariableCount
********************************************************************************
**
**    Get the number of variables for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        gctUINT32 * Count
**            Pointer to a variable receiving the number of variables.
*/
gceSTATUS
gcSHADER_GetVariableCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

/*******************************************************************************
**  gcSHADER_GetTempCount
**
**  Get the number of temp register used for this shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      return the Shader's temp register count, included in
**      variable, output, arguments, temporay in instruciton
 */
gctUINT
gcSHADER_GetTempCount(
    IN gcSHADER        Shader
    );

/*******************************************************************************
**                               gcSHADER_GetVariable
********************************************************************************
**
**    Get the gcVARIABLE object pointer for an indexed variable for this shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Index
**            Index of variable to retrieve.
**
**    OUTPUT:
**
**        gcVARIABLE * Variable
**            Pointer to a variable receiving the gcVARIABLE object pointer.
*/
gceSTATUS
gcSHADER_GetVariable(
    IN gcSHADER Shader,
    IN gctUINT Index,
    OUT gcVARIABLE * Variable
    );

gceSTATUS
gcSHADER_GetVariableByName(
    IN gcSHADER Shader,
    IN gctCONST_STRING VariableName,
    IN gctUINT16 NameLength,
    OUT gcVARIABLE * Variable
    );

/*******************************************************************************
**                               gcSHADER_GetVariableIndexingRange
********************************************************************************
**
**    Get the gcVARIABLE indexing range.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcVARIABLE variable
**            Start variable.
**
**        gctBOOL whole
**            Indicate whether maximum indexing range is queried
**
**    OUTPUT:
**
**        gctUINT *Start
**            Pointer to range start (temp register index).
**
**        gctUINT *End
**            Pointer to range end (temp register index).
*/
gceSTATUS
gcSHADER_GetVariableIndexingRange(
    IN gcSHADER Shader,
    IN gcVARIABLE variable,
    IN gctBOOL whole,
    OUT gctUINT *Start,
    OUT gctUINT *End
    );

/*******************************************************************************
**                               gcSHADER_AddOpcode
********************************************************************************
**
**    Add an opcode to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gctUINT32 TempRegister
**            Temporary register index that acts as the target of the opcode.
**
**        gctUINT8 Enable
**            Write enable bits for the temporary register that acts as the target
**            of the opcode.
**
**        gcSL_FORMAT Format
**            Format of the temporary register.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcode(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

gceSTATUS
gcSHADER_AddOpcode2(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**                            gcSHADER_AddOpcodeIndexed
********************************************************************************
**
**    Add an opcode to a gcSHADER object that writes to an dynamically indexed
**    target.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gctUINT32 TempRegister
**            Temporary register index that acts as the target of the opcode.
**
**        gctUINT8 Enable
**            Write enable bits  for the temporary register that acts as the
**            target of the opcode.
**
**        gcSL_INDEXED Mode
**            Location of the dynamic index inside the temporary register.  Valid
**            values can be:
**
**                gcSL_INDEXED_X - Use x component of the temporary register.
**                gcSL_INDEXED_Y - Use y component of the temporary register.
**                gcSL_INDEXED_Z - Use z component of the temporary register.
**                gcSL_INDEXED_W - Use w component of the temporary register.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**        gcSL_FORMAT Format
**            Format of the temporary register.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeIndexed(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeIndexedWithPrecision
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target with precision setting.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gctUINT32 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Mode
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**      gcSHADER_PRECISION Precision
**          Precision of register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionIndexed
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition to check.
**
**      gctUINT32 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Indexed
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionIndexed(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Indexed,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionIndexedWithPrecision
**
**  Add an opcode to a gcSHADER object that writes to an dynamically indexed
**  target with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition to check.
**
**      gctUINT32 TempRegister
**          Temporary register index that acts as the target of the opcode.
**
**      gctUINT8 Enable
**          Write enable bits  for the temporary register that acts as the
**          target of the opcode.
**
**      gcSL_INDEXED Indexed
**          Location of the dynamic index inside the temporary register.  Valid
**          values can be:
**
**              gcSL_INDEXED_X - Use x component of the temporary register.
**              gcSL_INDEXED_Y - Use y component of the temporary register.
**              gcSL_INDEXED_Z - Use z component of the temporary register.
**              gcSL_INDEXED_W - Use w component of the temporary register.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**      gcSHADER_PRECISION Precision
**          Precision of register.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT32 TempRegister,
    IN gctUINT8 Enable,
    IN gcSL_INDEXED Indexed,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**                          gcSHADER_AddOpcodeConditional
********************************************************************************
**
**    Add an conditional opcode to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_OPCODE Opcode
**            Opcode to add.
**
**        gcSL_CONDITION Condition
**            Condition that needs to evaluate to gcvTRUE in order for the opcode to
**            execute.
**
**        gctUINT Label
**            Target label if 'Condition' evaluates to gcvTRUE.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditional(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gctUINT Label,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionalFormatted
**
**  Add an conditional jump or call opcode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition that needs to evaluate to gcvTRUE in order for the opcode to
**          execute.
**
**      gcSL_FORMAT Format
**          Format of conditional operands
**
**      gctUINT Label
**          Target label if 'Condition' evaluates to gcvTRUE.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionalFormatted(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gcSL_FORMAT Format,
    IN gctUINT Label,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_AddOpcodeConditionalFormattedEnable
**
**  Add an conditional jump or call opcode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE Opcode
**          Opcode to add.
**
**      gcSL_CONDITION Condition
**          Condition that needs to evaluate to gcvTRUE in order for the opcode to
**          execute.
**
**      gcSL_FORMAT Format
**          Format of conditional operands
**
**      gctUINT8 Enable
**          Write enable value for the target of the opcode.
**
**      gctUINT Label
**          Target label if 'Condition' evaluates to gcvTRUE.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddOpcodeConditionalFormattedEnable(
    IN gcSHADER Shader,
    IN gcSL_OPCODE Opcode,
    IN gcSL_CONDITION Condition,
    IN gcSL_FORMAT Format,
    IN gctUINT8 Enable,
    IN gctUINT Label,
    IN gctUINT32 srcLoc
    );

/*******************************************************************************
**  gcSHADER_FindNextUsedLabelId
**
**  Find a label id which is not used inside the gcSHADER object
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  RETURN:
**
**      the next unused label id
*/
gctUINT
gcSHADER_FindNextUsedLabelId(
    IN gcSHADER Shader
    );

/*******************************************************************************
**  gcSHADER_FindLabel
**
**  Find a label inside the gcSHADER object
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a  gcSHADER object.
**
**      gctUINT Label
**          Label identifier.
**
**  OUTPUT:
**
**      gcSHADER_LABEL * ShaderLabel
**          Pointer to a variable receiving the pointer to the gcSHADER_LABEL
**          structure representing the requested label.
*/
gctBOOL
gcSHADER_FindLabel(
    IN gcSHADER Shader,
    IN gctUINT Label,
    OUT gcSHADER_LABEL * ShaderLabel
    );

/*******************************************************************************
**                                gcSHADER_AddLabel
********************************************************************************
**
**    Define a label at the current instruction of a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT Label
**            Label to define.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddLabel(
    IN gcSHADER Shader,
    IN gctUINT Label
    );

/*******************************************************************************
**  gcSHADER_FindFunctionByLabel
**
**  Find a function inside the gcSHADER object by call target (label)
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a  gcSHADER object.
**
**      gctUINT Label
**          Label identifier.
**
**  OUTPUT:
**
**      gcFUNCTION * Function
**          Pointer to a variable receiving the pointer to the gcFUNCTION
*/
gctBOOL
gcSHADER_FindFunctionByLabel(
    IN gcSHADER Shader,
    IN gctUINT Label,
    OUT gcFUNCTION * Function
    );

/*****************************************************************************************************
**  gcSHADER_UpdateTargetPacked
**
**  Update instruction target's PackedComponents field to indicate the number of packed components
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctINT Components
**          Number of packed components.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_UpdateTargetPacked(
    IN gcSHADER Shader,
    IN gctINT Components
    );

/*******************************************************************************
**  gcSHADER_UpdateSourcePacked
**
**  Update source's PackedComponents field to indicate the number of packed components
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSHADER_INSTRUCTION_INDEX InstIndex
**          Instruction argument index: gcSHADER_SOURCE0/gcSHADER_SOURCE1.
**
**      gctINT Components
**          Number of packed components.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_UpdateSourcePacked(
    IN gcSHADER Shader,
    IN gcSHADER_INSTRUCTION_INDEX InstrIndex,
    IN gctINT Components
    );

/*******************************************************************************
**  gcSHADER_UpdateResOpType
**
**  Update the resOpType
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_OPCODE_RES_TYPE ResOpType
**
**      gctINT Components
**          Number of packed components.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_UpdateResOpType(
    IN gcSHADER Shader,
    IN gcSL_OPCODE_RES_TYPE OpCodeResType
    );

/*******************************************************************************
**  gcSHADER_AddRoundingMode
**
**  Add rounding mode to a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_ROUND Round
**          Type of the source operand.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddRoundingMode(
    IN gcSHADER Shader,
    IN gcSL_ROUND Round
    );


/*******************************************************************************
**  gcSHADER_AddSaturation
**
**  Add saturation modifier to a gcSHADER instruction.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_MODIFIER_SAT Sat
**          Saturation modifier.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSaturation(
    IN gcSHADER Shader,
    IN gcSL_MODIFIER_SAT  Sat
    );

/*******************************************************************************
**  gcSHADER_NewTempRegs
**
**  Allocate RegCount of temp registers from the shader.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT RegCount
**          Count of temp register be allocated.
**
**      gcSHADER_TYPE  Type
**          Type of the temp register.
**
**
**  Return:
**
**      The start temp register index, the next available temp register
**      index is the return value plus RegCount.
*/
gctUINT32
gcSHADER_NewTempRegs(
    IN gcSHADER       Shader,
    IN gctUINT        RegCount,
    IN gcSHADER_TYPE  Type
    );

gctUINT32
gcSHADER_UpdateTempRegCount(
    IN gcSHADER       Shader,
    IN gctUINT        RegCount
    );

gctUINT32
gcSHADER_EndInst(
    IN gcSHADER Shader
    );
/*******************************************************************************
**                               gcSHADER_AddSource
********************************************************************************
**
**    Add a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_TYPE Type
**            Type of the source operand.
**
**        gctUINT32 SourceIndex
**            Index of the source operand.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gcSL_FORMAT Format
**            Format of the source operand.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSource(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT32 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                            gcSHADER_AddSourceIndexed
********************************************************************************
**
**    Add a dynamically indexed source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcSL_TYPE Type
**            Type of the source operand.
**
**        gctUINT32 SourceIndex
**            Index of the source operand.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gcSL_INDEXED Mode
**            Addressing mode for the index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**        gcSL_FORMAT Format
**            Format of the source operand.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceIndexed(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT32 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**  gcSHADER_AddSourceIndexedWithPrecision
**
**  Add a dynamically indexed source operand to a gcSHADER object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcSL_TYPE Type
**          Type of the source operand.
**
**      gctUINT32 SourceIndex
**          Index of the source operand.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gcSL_INDEXED Mode
**          Addressing mode for the index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSHADER_PRECISION Precision
**        Precision of source value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceIndexedWithPrecision(
    IN gcSHADER Shader,
    IN gcSL_TYPE Type,
    IN gctUINT32 SourceIndex,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                           gcSHADER_AddSourceAttribute
********************************************************************************
**
**    Add an attribute as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the attribute in case the attribute is a matrix
**            or array.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttribute(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index
    );

/*******************************************************************************
**                           gcSHADER_AddSourceAttributeIndexed
********************************************************************************
**
**    Add an indexed attribute as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the attribute in case the attribute is a matrix
**            or array.
**
**        gcSL_INDEXED Mode
**            Addressing mode of the dynamic index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttributeIndexed(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

/*******************************************************************************
**                            gcSHADER_AddSourceUniform
********************************************************************************
**
**    Add a uniform as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the uniform in case the uniform is a matrix or
**            array.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniform(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index
    );

/*******************************************************************************
**                        gcSHADER_AddSourceUniformIndexed
********************************************************************************
**
**    Add an indexed uniform as a source operand to a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT8 Swizzle
**            x, y, z, and w swizzle values packed into one 8-bit value.
**
**        gctINT Index
**            Static index into the uniform in case the uniform is a matrix or
**            array.
**
**        gcSL_INDEXED Mode
**            Addressing mode of the dynamic index.
**
**        gctUINT16 IndexRegister
**            Temporary register index that holds the dynamic index.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniformIndexed(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

gceSTATUS
gcSHADER_AddSourceSamplerIndexed(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister
    );

gceSTATUS
gcSHADER_AddSourceAttributeFormatted(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_FORMAT Format
    );

gceSTATUS
gcSHADER_AddSourceAttributeIndexedFormatted(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**  gcSHADER_AddSourceSamplerIndexedFormattedWithPrecision
**
**  Add a "0-based" formatted indexed sampler as a source operand to a gcSHADER
**  object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of sampler value
**
**    gcSHADER_PRECISION Precision
**        Precision of attribute value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceSamplerIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/********************************************************************************************
**  gcSHADER_AddSourceAttributeIndexedFormattedWithPrecision
**
**  Add a formatted indexed attribute as a source operand to a gcSHADER object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcATTRIBUTE Attribute
**          Pointer to a gcATTRIBUTE object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gctINT Index
**          Static index into the attribute in case the attribute is a matrix
**          or array.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of indexed attribute value
**
**    gcSHADER_PRECISION Precision
**        Precision of attribute value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceAttributeIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/********************************************************************************************
**  gcSHADER_AddSourceOutputIndexedFormattedWithPrecision
**
**  Add a formatted indexed output as a source operand to a gcSHADER object with precision.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcOUTPUT Output
**          Pointer to a gcOUTPUT object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gctINT Index
**          Static index into the attribute in case the attribute is a matrix
**          or array.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of indexed attribute value
**
**    gcSHADER_PRECISION Precision
**        Precision of attribute value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceOutputIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gcOUTPUT Output,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

gceSTATUS
gcSHADER_AddSourceUniformFormatted(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_FORMAT Format
    );

gceSTATUS
gcSHADER_AddSourceUniformIndexedFormatted(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/******************************************************************************************
**  gcSHADER_AddSourceUniformIndexedFormattedWithPrecision
**
**  Add a formatted indexed uniform as a source operand to a gcSHADER object with Precision
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**      gctUINT8 Swizzle
**          x, y, z, and w swizzle values packed into one 8-bit value.
**
**      gctINT Index
**          Static index into the uniform in case the uniform is a matrix or
**          array.
**
**      gcSL_INDEXED Mode
**          Addressing mode of the dynamic index.
**
**      gcSL_INDEXED_LEVEL IndexedLevel
**          Indexed level of dynamic index.
**
**      gctUINT16 IndexRegister
**          Temporary register index that holds the dynamic index.
**
**    gcSL_FORMAT Format
**        Format of uniform value
**
**    gcSHADER_PRECISION Precision
**        Precision of uniform value
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcSHADER_AddSourceUniformIndexedFormattedWithPrecision(
    IN gcSHADER Shader,
    IN gcUNIFORM Uniform,
    IN gctUINT8 Swizzle,
    IN gctINT Index,
    IN gcSL_INDEXED Mode,
    IN gcSL_INDEXED_LEVEL IndexedLevel,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

gceSTATUS
gcSHADER_AddSourceSamplerIndexedFormatted(
    IN gcSHADER Shader,
    IN gctUINT8 Swizzle,
    IN gcSL_INDEXED Mode,
    IN gctUINT16 IndexRegister,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**                           gcSHADER_AddSourceConstant
********************************************************************************
**
**    Add a constant floating point value as a source operand to a gcSHADER
**    object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctFLOAT Constant
**            Floating point constant.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstant(
    IN gcSHADER Shader,
    IN gctFLOAT Constant
    );

/*******************************************************************************
**                               gcSHADER_AddSourceConstantFormatted
********************************************************************************
**
**    Add a constant value as a source operand to a gcSHADER
**    object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        void * Constant
**            Pointer to constant.
**
**        gcSL_FORMAT Format
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstantFormatted(
    IN gcSHADER Shader,
    IN void *Constant,
    IN gcSL_FORMAT Format
    );

/*******************************************************************************
**    gcSHADER_AddSourceConstantFormattedWithPrecision
**
**    Add a formatted constant value as a source operand to a gcSHADER object
**    with precision.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        void *Constant
**            Pointer to constant value (32 bits).
**
**        gcSL_FORMAT Format
**            Format of constant value
**
**    gcSHADER_PRECISION Precision
**        Precision of constant value
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AddSourceConstantFormattedWithPrecision(
    IN gcSHADER Shader,
    IN void *Constant,
    IN gcSL_FORMAT Format,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                                  gcSHADER_Pack
********************************************************************************
**
**    Pack a dynamically created gcSHADER object by trimming the allocated arrays
**    and resolving all the labeling.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_Pack(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                  gcSHADER_ExpandArraysOfArrays
********************************************************************************
**
**    Expand array size for a object if this object is an arrays of arrays.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_ExpandArraysOfArrays(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                  gcSHADER_AnalyzeFunctions
********************************************************************************
**
** 1) Check function has sampler indexing
** 2) Detect if there is a recursive function in the shader code
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_AnalyzeFunctions(
    IN gcSHADER Shader,
    IN gctBOOL NeedToCheckRecursive
    );

/*******************************************************************************
**                                gcSHADER_SetOptimizationOption
********************************************************************************
**
**    Set optimization option of a gcSHADER object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gctUINT OptimizationOption
**            Optimization option.  Can be one of the following:
**
**                0                        - No optimization.
**                1                        - Full optimization.
**                Other value                - For optimizer testing.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcSHADER_SetOptimizationOption(
    IN gcSHADER Shader,
    IN gctUINT OptimizationOption
    );

/*******************************************************************************
**  gcSHADER_ReallocateFunctions
**
**  Reallocate an array of pointers to gcFUNCTION objects.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcSHADER_ReallocateFunctions(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

gceSTATUS
gcSHADER_AddFunction(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    OUT gcFUNCTION * Function
    );

gceSTATUS
gcSHADER_DeleteFunction(
    IN gcSHADER Shader,
    IN gcFUNCTION  Function
    );

gceSTATUS
gcSHADER_ReallocateKernelFunctions(
    IN gcSHADER Shader,
    IN gctUINT32 Count
    );

gceSTATUS
gcSHADER_AddKernelFunction(
    IN gcSHADER Shader,
    IN gctCONST_STRING Name,
    OUT gcKERNEL_FUNCTION * KernelFunction
    );

gceSTATUS
gcSHADER_BeginFunction(
    IN gcSHADER Shader,
    IN gcFUNCTION Function
    );

gceSTATUS
gcSHADER_EndFunction(
    IN gcSHADER Shader,
    IN gcFUNCTION Function
    );

gceSTATUS
gcSHADER_BeginKernelFunction(
    IN gcSHADER Shader,
    IN gcKERNEL_FUNCTION KernelFunction
    );

gceSTATUS
gcSHADER_EndKernelFunction(
    IN gcSHADER Shader,
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 LocalMemorySize
    );

gceSTATUS
gcSHADER_SetMaxKernelFunctionArgs(
    IN gcSHADER Shader,
    IN gctUINT32 MaxKernelFunctionArgs
    );

/*******************************************************************************
**  gcSHADER_AddTypeNameBuffer
**
**  add buffer containing all non basic type names of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctSIZE_T TypeNameBufferSize
**          Type name buffer size in bytes
**
**      gctCHAR *TypeNameBuffer
**          Non basic type names
*/
gceSTATUS
gcSHADER_AddTypeNameBuffer(
    IN gcSHADER Shader,
    IN gctUINT32 TypeNameBufferSize,
    IN gctCHAR * TypeNameBuffer
    );
/*******************************************************************************
**  gcSHADER_SetConstantMemorySize
**
**  Set the constant memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 ConstantMemorySize
**          Constant memory size in bytes
**
**      gctCHAR *ConstantMemoryBuffer
**          Constant memory buffer
*/
gceSTATUS
gcSHADER_SetConstantMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 ConstantMemorySize,
    IN gctCHAR * ConstantMemoryBuffer
    );

gceSTATUS
gcSHADER_AddConstantMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 ConstantMemorySize,
    IN gctCHAR * ConstantMemoryBuffer
    );

/*******************************************************************************
**  gcSHADER_GetConstantMemorySize
**
**  Set the constant memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * ConstantMemorySize
**          Pointer to a variable receiving constant memory size in bytes
**
**      gctCHAR **ConstantMemoryBuffer.
**          Pointer to a variable for returned shader constant memory buffer.
*/
gceSTATUS
gcSHADER_GetConstantMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * ConstantMemorySize,
    OUT gctCHAR ** ConstantMemoryBuffer
    );

/*******************************************************************************
**  gcSHADER_SetPrivateMemorySize
**
**  Set the private memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 PrivateMemorySize
**          Private memory size in bytes
*/
gceSTATUS
gcSHADER_SetPrivateMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 PrivateMemorySize
    );

/*******************************************************************************
**  gcSHADER_GetPrivateMemorySize
**
**  Set the private memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * PrivateMemorySize
**          Pointer to a variable receiving private memory size in bytes
*/
gceSTATUS
gcSHADER_GetPrivateMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * PrivateMemorySize
    );

/*******************************************************************************
**  gcSHADER_SetLocalMemorySize
**
**  Set the local memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**      gctUINT32 LocalMemorySize
**          Local memory size in bytes
*/
gceSTATUS
gcSHADER_SetLocalMemorySize(
    IN gcSHADER Shader,
    IN gctUINT32 LocalMemorySize
    );

/*******************************************************************************
**  gcSHADER_GetLocalMemorySize
**
**  Set the local memory address space size of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * LocalMemorySize
**          Pointer to a variable receiving lcoal memory size in bytes
*/
gceSTATUS
gcSHADER_GetLocalMemorySize(
    IN gcSHADER Shader,
    OUT gctUINT32 * LocalMemorySize
    );

/*******************************************************************************
**  gcSHADER_GetWorkGroupSize
**
**  Get the workGroupSize of a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
**  OUTPUT:
**
**      gctUINT32 * WorkGroupSize
**          Pointer to a variable receiving workGroupSize
*/
gceSTATUS
gcSHADER_GetWorkGroupSize(
    IN gcSHADER Shader,
    OUT gctUINT * WorkGroupSize
    );

gctINT
gcSHADER_GetLtcCodeUniformIndex(
    IN gcSHADER Shader,
    IN gctUINT CodeIndex
    );

/*******************************************************************************
**  gcSHADER_CheckValidity
**
**  Check validity for a gcSHADER object.
**
**  INPUT:
**
**      gcSHADER Shader
**          Pointer to a gcSHADER object.
**
*/
gceSTATUS
gcSHADER_CheckValidity(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                               gcSHADER_GetVariableTempTypes
********************************************************************************
**
**    Get the gcVARIABLE temp types and save the type to TempTypeArray.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcVARIABLE variable
**            Start variable.
**
**        gctUINT TempTypeArraySize
**            The size of temp type array.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * TempTypeArray
**            Pointer to temp type array
**
*/
gceSTATUS
gcSHADER_GetVariableTempTypes(
    IN gcSHADER            Shader,
    IN gcVARIABLE          Variable,
    IN gctUINT             TempTypeArraySize,
    IN gctINT              FisrtTempIndex,
    OUT gcSHADER_TYPE *    TempTypeArray
    );

gceSTATUS
gcATTRIBUTE_IsPosition(
        IN gcATTRIBUTE Attribute,
        OUT gctBOOL * IsPosition,
        OUT gctBOOL * IsDirectPosition
        );

/*******************************************************************************
**  gcATTRIBUTE_SetPrecision
**
**  Set the precision of an attribute.
**
**  INPUT:
**
**      gcATTRIBUTE Attribute
**          Pointer to a gcATTRIBUTE object.
**
**    gcSHADER_PRECISION Precision
**          Precision of the attribute.
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcATTRIBUTE_SetPrecision(
    IN gcATTRIBUTE Attribute,
    IN gcSHADER_PRECISION Precision
    );

/*******************************************************************************
**                             gcATTRIBUTE_GetType
********************************************************************************
**
**    Get the type and array length of a gcATTRIBUTE object.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the attribute.  'Type'
**            can be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            attribute was declared as an array.  If the attribute was not
**            declared as an array, the array length will be 1.  'ArrayLength' can
**            be gcvNULL, in which case no array length will be returned.
*/
gceSTATUS
gcATTRIBUTE_GetType(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

gceSTATUS
gcATTRIBUTE_GetLocation(
    IN gcATTRIBUTE Attribute,
    OUT gctINT * Location
    );

gceSTATUS
gcATTRIBUTE_GetPrecision(
    IN gcATTRIBUTE Attribute,
    OUT gcSHADER_PRECISION * Precision
    );

/*******************************************************************************
**                            gcATTRIBUTE_GetName
********************************************************************************
**
**    Get the name of a gcATTRIBUTE object.
**
**    INPUT:
**
**      gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**        gctBOOL UseInstanceName
**            Use instance name for a block member.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the attribute name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the attribute name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcATTRIBUTE_GetName(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    IN gctBOOL UseInstanceName,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**                            gcATTRIBUTE_GetNameEx
********************************************************************************
**
**    Get the name of a gcATTRIBUTE object.
**
**    INPUT:
**
**      gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the attribute name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctSTRING * Name
**            Pointer to a variable receiving the pointer to the attribute name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcATTRIBUTE_GetNameEx(
    IN gcSHADER Shader,
    IN gcATTRIBUTE Attribute,
    OUT gctUINT32 * Length,
    OUT gctSTRING * Name
    );

/*******************************************************************************
**                            gcATTRIBUTE_IsEnabled
********************************************************************************
**
**    Query the enabled state of a gcATTRIBUTE object.
**
**    INPUT:
**
**        gcATTRIBUTE Attribute
**            Pointer to a gcATTRIBUTE object.
**
**    OUTPUT:
**
**        gctBOOL * Enabled
**            Pointer to a variable receiving the enabled state of the attribute.
*/
gceSTATUS
gcATTRIBUTE_IsEnabled(
    IN gcATTRIBUTE Attribute,
    OUT gctBOOL * Enabled
    );

gceSTATUS
gcATTRIBUTE_GetIndex(
    IN gcATTRIBUTE Attribute,
    OUT gctUINT16 * Index
    );

gceSTATUS
gcATTRIBUTE_IsPerPatch(
    IN gcATTRIBUTE Attribute,
    OUT gctBOOL * IsPerPatch
    );


gceSTATUS
gcATTRIBUTE_IsSample(
    IN gcATTRIBUTE Attribute,
    OUT gctBOOL * IsSample
    );

/*******************************************************************************
**                              gcUNIFORM_GetType
********************************************************************************
**
**    Get the type and array length of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the uniform.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            uniform was declared as an array.  If the uniform was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetType(
    IN gcUNIFORM Uniform,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                              gcUNIFORM_GetTypeEx
********************************************************************************
**
**    Get the type and array length of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the uniform.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gcSHADER_PRECISION * Precision
**            Pointer to a variable receiving the precision of the uniform.  'Precision' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            uniform was declared as an array.  If the uniform was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetTypeEx(
    IN gcUNIFORM Uniform,
    OUT gcSHADER_TYPE * Type,
    OUT gcSHADER_TYPE_KIND * Category,
    OUT gcSHADER_PRECISION * Precision,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                              gcUNIFORM_GetFlags
********************************************************************************
**
**    Get the flags of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gceUNIFORM_FLAGS * Flags
**            Pointer to a variable receiving the flags of the uniform.
**
*/
gceSTATUS
gcUNIFORM_GetFlags(
    IN gcUNIFORM Uniform,
    OUT gceUNIFORM_FLAGS * Flags
    );

/*******************************************************************************
**                              gcUNIFORM_SetFlags
********************************************************************************
**
**    Set the flags of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gceUNIFORM_FLAGS Flags
**            Flags of the uniform to be set.
**
**    OUTPUT:
**            Nothing.
**
*/
gceSTATUS
gcUNIFORM_SetFlags(
    IN gcUNIFORM Uniform,
    IN gceUNIFORM_FLAGS Flags
    );

/*******************************************************************************
**                              gcOUTPUT_SetLayoutQualifier
********************************************************************************
**
**    Set the layout qualifiers of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**        gceLAYOUT_QUALIFIER LayoutQualifier
**            Layout qualifier of the output to be set.
**
**    OUTPUT:
**            Nothing.
**
*/
gceSTATUS
gcOUTPUT_SetLayoutQualifier(
    IN gcOUTPUT Output,
    IN gceLAYOUT_QUALIFIER LayoutQualifier
    );

/*******************************************************************************
**                              gcOUTPUT_SetLayoutQualifier
********************************************************************************
**
**    Get the layout qualifiers of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**        gceLAYOUT_QUALIFIER *LayoutQualifier
**            Pointer to a variable receiving the layout qualifier of the output.
**
*/
gceSTATUS
gcOUTPUT_GetLayoutQualifier(
    IN gcOUTPUT Output,
    OUT gceLAYOUT_QUALIFIER * LayoutQualifier
    );

/*******************************************************************************
**                              gcUNIFORM_GetName
********************************************************************************
**
**    Get the name of a gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the uniform name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the uniform name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcUNIFORM_GetName(
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**  gcUNIFORM_BLOCK_GetName
**
**  Get the name of a gcsUNIFORM_BLOCK object.
**
**  INPUT:
**
**      gcsUNIFORM_BLOCK UniformBlock
**          Pointer to a gcsUNIFORM_BLOCK object.
**
**  OUTPUT:
**
**      gctUINT32 * Length
**          Pointer to a variable receiving the length of the uniform block name.
**          'Length' can be gcvNULL, in which case no length will be returned.
**
**      gctCONST_STRING * Name
**          Pointer to a variable receiving the pointer to the uniform block name.
**          'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcUNIFORM_BLOCK_GetName(
    IN gcsUNIFORM_BLOCK UniformBlock,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**                              gcUNIFORM_GetSampler
********************************************************************************
**
**    Get the physical sampler number for a sampler gcUNIFORM object.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        gctUINT32 * Sampler
**            Pointer to a variable receiving the physical sampler.
*/
gceSTATUS
gcUNIFORM_GetSampler(
    IN gcUNIFORM Uniform,
    OUT gctUINT32 * Sampler
    );

gceSTATUS
gcUNIFORM_GetIndex(
    IN gcUNIFORM Uniform,
    OUT gctUINT16 * Index
    );

/*******************************************************************************
**  gcUNIFORM_GetFormat
**
**  Get the type and array length of a gcUNIFORM object.
**
**  INPUT:
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**  OUTPUT:
**
**      gcSL_FORMAT * Format
**          Pointer to a variable receiving the format of element of the uniform.
**          'Type' can be gcvNULL, in which case no type will be returned.
**
**      gctBOOL * IsPointer
**          Pointer to a variable receiving the state wheter the uniform is a pointer.
**          'IsPointer' can be gcvNULL, in which case no array length will be returned.
*/
gceSTATUS
gcUNIFORM_GetFormat(
    IN gcUNIFORM Uniform,
    OUT gcSL_FORMAT * Format,
    OUT gctBOOL * IsPointer
    );

/*******************************************************************************
**  gcUNIFORM_SetFormat
**
**  Set the format and isPointer of a uniform.
**
**  INPUT:
**
**      gcUNIFORM Uniform
**          Pointer to a gcUNIFORM object.
**
**      gcSL_FORMAT Format
**          Format of element of the uniform shaderType.
**
**      gctBOOL IsPointer
**          Wheter the uniform is a pointer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcUNIFORM_SetFormat(
    IN gcUNIFORM Uniform,
    IN gcSL_FORMAT Format,
    IN gctBOOL IsPointer
    );

/*******************************************************************************
**                               gcUNIFORM_SetValue
********************************************************************************
**
**    Set the value of a uniform in integer.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT32 Count
**            Number of entries to program if the uniform has been declared as an
**            array.
**
**        const gctINT * Value
**            Pointer to a buffer holding the integer values for the uniform.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcUNIFORM_SetValue(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN const gctINT * Value
    );

gceSTATUS
gcUNIFORM_SetValue_Ex(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN gcsHINT_PTR Hints,
    IN const gctINT * Value
    );


/*******************************************************************************
**                               gcUNIFORM_SetValueF
********************************************************************************
**
**    Set the value of a uniform in floating point.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**        gctUINT32 Count
**            Number of entries to program if the uniform has been declared as an
**            array.
**
**        const gctFLOAT * Value
**            Pointer to a buffer holding the floating point values for the
**            uniform.
**
**    OUTPUT:
**
**        Nothing.
*/
gceSTATUS
gcUNIFORM_SetValueF(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN const gctFLOAT * Value
    );

gceSTATUS
gcUNIFORM_SetValueF_Ex(
    IN gcUNIFORM Uniform,
    IN gctUINT32 Count,
    IN gcsHINT_PTR Hints,
    IN const gctFLOAT * Value
    );

/*******************************************************************************
**                         gcUNIFORM_GetModelViewProjMatrix
********************************************************************************
**
**    Get the value of uniform modelViewProjMatrix ID if present.
**
**    INPUT:
**
**        gcUNIFORM Uniform
**            Pointer to a gcUNIFORM object.
**
**    OUTPUT:
**
**        Nothing.
*/
gctUINT
gcUNIFORM_GetModelViewProjMatrix(
    IN gcUNIFORM Uniform
    );

/*******************************************************************************
**                                gcOUTPUT_GetType
********************************************************************************
**
**    Get the type and array length of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**
**        gcSHADER_TYPE * Type
**            Pointer to a variable receiving the type of the output.  'Type' can
**            be gcvNULL, in which case no type will be returned.
**
**        gctUINT32 * ArrayLength
**            Pointer to a variable receiving the length of the array if the
**            output was declared as an array.  If the output was not declared
**            as an array, the array length will be 1.  'ArrayLength' can be gcvNULL,
**            in which case no array length will be returned.
*/
gceSTATUS
gcOUTPUT_GetType(
    IN gcOUTPUT Output,
    OUT gcSHADER_TYPE * Type,
    OUT gctUINT32 * ArrayLength
    );

/*******************************************************************************
**                               gcOUTPUT_GetIndex
********************************************************************************
**
**    Get the index of a gcOUTPUT object.
**
**    INPUT:
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**    OUTPUT:
**
**        gctUINT * Index
**            Pointer to a variable receiving the temporary register index of the
**            output.  'Index' can be gcvNULL,. in which case no index will be
**            returned.
*/
gceSTATUS
gcOUTPUT_GetIndex(
    IN gcOUTPUT Output,
    OUT gctUINT * Index
    );

/*******************************************************************************
**                                gcOUTPUT_GetName
********************************************************************************
**
**    Get the name of a gcOUTPUT object.
**
**    INPUT:
**
**      gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**        gctBOOL UseInstanceName
**            Use instance name for a block member.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the output name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the output name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcOUTPUT_GetName(
    IN gcSHADER Shader,
    IN gcOUTPUT Output,
    IN gctBOOL UseInstanceName,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

/*******************************************************************************
**                                gcOUTPUT_GetNameEx
********************************************************************************
**
**    Get the name of a gcOUTPUT object.
**
**    INPUT:
**
**      gcSHADER Shader
**            Pointer to a gcSHADER object.
**
**        gcOUTPUT Output
**            Pointer to a gcOUTPUT object.
**
**        gctBOOL UseInstanceName
**            Use instance name for a block member.
**
**    OUTPUT:
**
**        gctUINT32 * Length
**            Pointer to a variable receiving the length of the output name.
**            'Length' can be gcvNULL, in which case no length will be returned.
**
**        gctCONST_STRING * Name
**            Pointer to a variable receiving the pointer to the output name.
**            'Name' can be gcvNULL, in which case no name will be returned.
*/
gceSTATUS
gcOUTPUT_GetNameEx(
    IN gcSHADER Shader,
    IN gcOUTPUT Output,
    OUT gctUINT32 * Length,
    OUT gctSTRING * Name
    );

/*******************************************************************************
**  gcOUTPUT_GetLocation
**
**  Get the Location of a gcOUTPUT object.
**
**  INPUT:
**
**      gcOUTPUT Output
**          Pointer to a gcOUTPUT object.
**
**  OUTPUT:
**
**      gctUINT * Location
**          Pointer to a variable receiving the Location of the
**          output.
*/
gceSTATUS
gcOUTPUT_GetLocation(
    IN gcOUTPUT Output,
    OUT gctUINT * Location
    );


/*******************************************************************************
*********************************************************** F U N C T I O N S **
*******************************************************************************/

/*******************************************************************************
**  gcFUNCTION_ReallocateArguments
**
**  Reallocate an array of gcsFUNCTION_ARGUMENT objects.
**
**  INPUT:
**
**      gcFUNCTION Function
**          Pointer to a gcFUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcFUNCTION_ReallocateArguments(
    IN gcFUNCTION Function,
    IN gctUINT32 Count
    );

gceSTATUS
gcFUNCTION_AddArgument(
    IN gcFUNCTION Function,
    IN gctUINT16 VariableIndex,
    IN gctUINT32 TempIndex,
    IN gctUINT8 Enable,
    IN gctUINT8 Qualifier,
    IN gctUINT8 Precision,
    IN gctBOOL IsPrecise
    );

gceSTATUS
gcFUNCTION_GetArgument(
    IN gcFUNCTION Function,
    IN gctUINT32 Index,
    OUT gctUINT32_PTR Temp,
    OUT gctUINT8_PTR Enable,
    OUT gctUINT8_PTR Swizzle
    );

gceSTATUS
gcFUNCTION_GetLabel(
    IN gcFUNCTION Function,
    OUT gctUINT_PTR Label
    );

/*******************************************************************************
************************* K E R N E L    P R O P E R T Y    F U N C T I O N S **
*******************************************************************************/
/*******************************************************************************/
gceSTATUS
gcKERNEL_FUNCTION_AddKernelFunctionProperties(
        IN gcKERNEL_FUNCTION KernelFunction,
        IN gctINT propertyType,
        IN gctUINT32 propertySize,
        IN gctINT * values
        );

gceSTATUS
gcKERNEL_FUNCTION_GetPropertyCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetProperty(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gctUINT32 * propertySize,
    OUT gctINT * propertyType,
    OUT gctINT * propertyValues
    );


/*******************************************************************************
*******************************I M A G E   S A M P L E R    F U N C T I O N S **
*******************************************************************************/
/*******************************************************************************
**  gcKERNEL_FUNCTION_ReallocateImageSamplers
**
**  Reallocate an array of pointers to image sampler pair.
**
**  INPUT:
**
**      gcKERNEL_FUNCTION KernelFunction
**          Pointer to a gcKERNEL_FUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcKERNEL_FUNCTION_ReallocateImageSamplers(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddImageSampler(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT8 ImageNum,
    IN gctBOOL IsConstantSamplerType,
    IN gctUINT32 SamplerType
    );

gceSTATUS
gcKERNEL_FUNCTION_GetImageSamplerCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetImageSampler(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gctUINT8 *ImageNum,
    OUT gctBOOL *IsConstantSamplerType,
    OUT gctUINT32 *SamplerType
    );

/*******************************************************************************
*********************************************K E R N E L    F U N C T I O N S **
*******************************************************************************/

/*******************************************************************************
**  gcKERNEL_FUNCTION_ReallocateArguments
**
**  Reallocate an array of gcsFUNCTION_ARGUMENT objects.
**
**  INPUT:
**
**      gcKERNEL_FUNCTION Function
**          Pointer to a gcKERNEL_FUNCTION object.
**
**      gctUINT32 Count
**          Array count to reallocate.  'Count' must be at least 1.
*/
gceSTATUS
gcKERNEL_FUNCTION_ReallocateArguments(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddArgument(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT16 VariableIndex,
    IN gctUINT32 TempIndex,
    IN gctUINT8 Enable,
    IN gctUINT8 Qualifier
    );

gceSTATUS
gcKERNEL_FUNCTION_GetArgument(
    IN gcKERNEL_FUNCTION Function,
    IN gctUINT32 Index,
    OUT gctUINT32_PTR Temp,
    OUT gctUINT8_PTR Enable,
    OUT gctUINT8_PTR Swizzle
    );

gceSTATUS
gcKERNEL_FUNCTION_GetLabel(
    IN gcKERNEL_FUNCTION Function,
    OUT gctUINT_PTR Label
    );

gceSTATUS
gcKERNEL_FUNCTION_GetName(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Length,
    OUT gctCONST_STRING * Name
    );

gceSTATUS
gcKERNEL_FUNCTION_ReallocateUniformArguments(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT32 Count
    );

gceSTATUS
gcKERNEL_FUNCTION_AddUniformArgument(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctCONST_STRING Name,
    IN gcSHADER_TYPE Type,
    IN gctUINT32 Length,
    OUT gcUNIFORM * UniformArgument
    );

gceSTATUS
gcKERNEL_FUNCTION_GetUniformArgumentCount(
    IN gcKERNEL_FUNCTION KernelFunction,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcKERNEL_FUNCTION_GetUniformArgument(
    IN gcKERNEL_FUNCTION KernelFunction,
    IN gctUINT Index,
    OUT gcUNIFORM * UniformArgument
    );

gceSTATUS
gcKERNEL_FUNCTION_SetCodeEnd(
    IN gcKERNEL_FUNCTION KernelFunction
    );

/*******************************************************************************
**                              gcInitializeCompiler
********************************************************************************
**
**  Initialize compiler global variables.
**
**  Input:
**      gcePATCH_ID PatchId,
**      patch ID
**
**      gcsHWCaps HWCaps,
**      HW capabilities filled in by driver and passed to compiler
**
**      gcsGLSLCaps *Caps
**      Min/Max capabilities filled in by driver and passed to compiler
**
**  Output:
**      Nothing
**
*/
gceSTATUS
gcInitializeCompiler(
    IN gcePATCH_ID PatchId,
    IN gcsHWCaps *HWCaps,
    IN gcsGLSLCaps *Caps
    );

gceSTATUS
gcInitializeCompilerCaps(
    IN gcsGLSLCaps *Caps
    );

/*******************************************************************************
**                              gcFinalizeCompiler
********************************************************************************
**  Finalize compiler global variables.
**
**
**  Output:
**      Nothing
**
*/
gceSTATUS
gcFinalizeCompiler(void);

/*******************************************************************************
**                              gcCompileShader
********************************************************************************
**
**    Compile a shader.
**
**    INPUT:
**
**        gctINT ShaderType
**            Shader type to compile.  Can be one of the following values:
**
**                gcSHADER_TYPE_VERTEX
**                    Compile a vertex shader.
**
**                gcSHADER_TYPE_FRAGMENT
**                    Compile a fragment shader.
**
**        gctUINT SourceSize
**            Size of the source buffer in bytes.
**
**        gctCONST_STRING Source
**            Pointer to the buffer containing the shader source code.
**
**    OUTPUT:
**
**        gcSHADER * Binary
**            Pointer to a variable receiving the pointer to a gcSHADER object
**            containg the compiled shader code.
**
**        gctSTRING * Log
**            Pointer to a variable receiving a string pointer containging the
**            compile log.
*/
gceSTATUS
gcCompileShader(
    IN gctINT ShaderType,
    IN gctUINT SourceSize,
    IN gctCONST_STRING Source,
    OUT gcSHADER * Binary,
    OUT gctSTRING * Log
    );


/*******************************************************************************
**                              gcSetClientApiVersion
********************************************************************************
**
**    Set Client API version
**
*/
gceSTATUS
gcSetClientApiVersion(
    IN gceAPI ApiVersion
    );

/*******************************************************************************
**                              gcLoadKernelCompiler
********************************************************************************
**
**    OpenCL kernel shader compiler load.
**
*/
gceSTATUS
gcLoadKernelCompiler(
    IN gcsHWCaps *HWCaps,
    IN gcePATCH_ID PatchId
    );

/*******************************************************************************
**                              gcUnloadKernelCompiler
********************************************************************************
**
**    OpenCL kernel shader compiler unload.
**
*/
gceSTATUS
gcUnloadKernelCompiler(
void
);

/*******************************************************************************
**                              gcOptimizeShader
********************************************************************************
**
**    Optimize a shader.
**
**    INPUT:
**
**        gcSHADER Shader
**            Pointer to a gcSHADER object holding information about the compiled
**            shader.
**
**        gctFILE LogFile
**            Pointer to an open FILE object.
*/
gceSTATUS
gcOptimizeShader(
    IN gcSHADER Shader,
    IN gctFILE LogFile
    );

gceSTATUS
gcSetUniformShaderKind(
    IN gcSHADER Shader
    );

/*******************************************************************************
**                                gcLinkShaders
********************************************************************************
**
**    Link two shaders and generate a harwdare specific state buffer by compiling
**    the compiler generated code through the resource allocator and code
**    generator.
**
**    INPUT:
**
**        gcSHADER VertexShader
**            Pointer to a gcSHADER object holding information about the compiled
**            vertex shader.
**
**        gcSHADER FragmentShader
**            Pointer to a gcSHADER object holding information about the compiled
**            fragment shader.
**
**        gceSHADER_FLAGS Flags
**            Compiler flags.  Can be any of the following:
**
**                gcvSHADER_DEAD_CODE       - Dead code elimination.
**                gcvSHADER_RESOURCE_USAGE  - Resource usage optimizaion.
**                gcvSHADER_OPTIMIZER       - Full optimization.
**                gcvSHADER_USE_GL_Z        - Use OpenGL ES Z coordinate.
**                gcvSHADER_USE_GL_POSITION - Use OpenGL ES gl_Position.
**                gcvSHADER_USE_GL_FACE     - Use OpenGL ES gl_FaceForward.
**
**    OUTPUT:
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receicing the program state.
*/
gceSTATUS
gcLinkShaders(
    IN gcSHADER VertexShader,
    IN gcSHADER FragmentShader,
    IN gceSHADER_FLAGS Flags,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );



/*******************************************************************************************
**  Initialize libfile
*/
gceSTATUS
gcInitializeLibFile(void);

/*******************************************************************************************
**  Finalize libfile.
**
*/
gceSTATUS
gcFinalizeLibFile(void);



/*******************************************************************************
**                                gcSHADER_WriteShaderToFile
********************************************************************************
**
**    write user shader info into file
**
**    INPUT:
**
**        gcSHADER    Binary,
**            Pointer to a gcSHADER object holding information about the shader
**
**        gctSTRING    ShaderName
**            Pointer to a gcSHADER name
**
**    OUTPUT:none
**
*/
gceSTATUS
gcSHADER_WriteShaderToFile(
    IN gcSHADER    Binary,
    IN gctSTRING    ShaderName
    );

/*******************************************************************************
**                                gcSHADER_SetAllOutputShadingModeToFlat
********************************************************************************
**
**    set user shader all output shading mode to flat.
**
**    OUTPUT:
**
**        gcSHADER    Shader
**            Pointer to a gcSHADER object holding information about the shader
**            and all output shading mode are set to flat
**
**
*/

gceSTATUS
gcSHADER_SetAllOutputShadingModeToFlat(
    OUT gcSHADER    Shader
    );

/*******************************************************************************
**                                gcSHADER_ReadShaderFromFile
********************************************************************************
**
**    read user shader info from file
**
**    INPUT:
**        gctSTRING    ShaderName
**            Pointer to a gcSHADER name
**
**
**    OUTPUT:
**        gcSHADER    Binary,
**            Pointer to a gcSHADER object holding information about the shader
**

**
*/
gceSTATUS
gcSHADER_ReadShaderFromFile(
    IN gctSTRING    ShaderName,
    OUT gcSHADER    *Binary
    );

/*******************************************************************************
**                                gcLinkProgram
********************************************************************************
**
**    Link a list shader and generate a hardware specific state buffer by compiling
**    the compiler generated code through the resource allocator and code
**    generator.
**
**    INPUT:
**        gctINT ShaderCount
**            number of gcSHADER object in the shader array
**
**        gcSHADER *ShaderArray
**            Array of gcSHADER object holding information about the compiled
**            shader.
**
**        gceSHADER_FLAGS Flags
**            Compiler flags.  Can be any of the following:
**
**                gcvSHADER_DEAD_CODE       - Dead code elimination.
**                gcvSHADER_RESOURCE_USAGE  - Resource usage optimizaion.
**                gcvSHADER_OPTIMIZER       - Full optimization.
**
**          gcvSHADER_LOADTIME_OPTIMZATION is set if load-time optimizaiton
**          is needed
**
**    OUTPUT:
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receiving the program state.
*/
gceSTATUS
gcLinkProgram(
    IN gctINT               ShaderCount,
    IN gcSHADER *           ShaderArray,
    IN gceSHADER_FLAGS      Flags,
    IN gceSHADER_SUB_FLAGS  *SubFlags,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );


/*******************************************************************************
**                                gcLinkProgramPipeline
********************************************************************************
**
**    Link a list shader and generate a hardware specific state buffer without any
**    optimization
**
**    INPUT:
**        gctINT ShaderCount
**            number of gcSHADER object in the shader array
**
**        gcSHADER *ShaderArray
**            Array of gcSHADER object holding information about the compiled
**            shader.
**
**    OUTPUT:
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receiving the program state.
*/
gceSTATUS
gcLinkProgramPipeline(
    IN gctINT               ShaderCount,
    IN gcSHADER *           ShaderArray,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );

/*******************************************************************************
**                                gcValidateProgramPipeline
********************************************************************************
**
**    Validate program pipeline.
**
**    INPUT:
**        gctINT ShaderCount
**            number of gcSHADER object in the shader array
**
**        gcSHADER *ShaderArray
**            Array of gcSHADER object holding information about the compiled
**            shader.
**
*/
gceSTATUS
gcValidateProgramPipeline(
    IN gctINT               ShaderCount,
    IN gcSHADER *           ShaderArray
    );

/* dynamic patch functions */
/* utility function to constuct patch info */

/* create a format covertion directive with specified sampler name
 * and sampler info, *PatchDirectivePtr must point to NULL at the
 * first to this routine, each subsequent call to this routine create
 *  a new directive and link with the previous one
 *
 *  Multiple-Layer support:
 *    for some format, it is splited to multiple layers, the  SplitLayers
 *    is the extra layers it splits to, 0 means no extra layer to split,
 *    the multi-layer sampler uniform will be created later when doing
 *    dynmaic shader patch, and the driver need to  bind the split
 *    multi-layer texture objects to the multi-layer sampler uniforms
 */
gceSTATUS
gcCreateInputConversionDirective(
    IN gcUNIFORM               Sampler,
    IN gctINT                  ArrayIndex,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    IN gceTEXTURE_SWIZZLE *    Swizzle,
    IN gctUINT                 Layers,
    IN gcTEXTURE_MODE          MipFilter,
    IN gcTEXTURE_MODE          MagFilter,
    IN gcTEXTURE_MODE          MinFilter,
    IN gctFLOAT                LODBias,
    IN gctINT                  Projected,
    IN gctINT                  Width,
    IN gctINT                  Height,
    IN gctINT                  Depth,
    IN gctINT                  Dimension,
    IN gctINT                  MipLevelMax,
    IN gctINT                  MipLevelMin,
    IN gctBOOL                 SRGB,
    IN gctBOOL                 AppendToLast,
    IN gctBOOL                 DepthStencilMode,
    IN gctBOOL                 NeedFormatConvert,
    IN gcSHADER_KIND           ShaderKind,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateOutputConversionDirective(
    IN gctINT                  OutputLocation,
    IN gcsSURF_FORMAT_INFO_PTR FormatInfo,
    IN gctUINT                 Layers,
    IN gctBOOL                 AppendToLast,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateDepthComparisonDirective(
    IN gcsSURF_FORMAT_INFO_PTR SamplerInfo,
    IN gcUNIFORM               Sampler,
    IN gctINT                  ArrayIndex,
    IN gctUINT                 CompMode,
    IN gctUINT                 CompFunction,
    OUT gcPatchDirective **    PatchDirectivePtr
    );

gceSTATUS
gcIsSameDepthComparisonDirectiveExist(
    IN gcsSURF_FORMAT_INFO_PTR SamplerInfo,
    IN gcUNIFORM               Sampler,
    IN gctINT                  ArrayIndex,
    IN gctUINT                 CompMode,
    IN gctUINT                 CompFunction,
    IN gcPatchDirective *      PatchDirectivePtr
    );

gceSTATUS
gcIsSameInputDirectiveExist(
    IN gcUNIFORM               Sampler,
    IN gctINT                  ArrayIndex,
    IN gcPatchDirective *      PatchDirectivePtr
    );

gceSTATUS
gcCreateColorFactoringDirective(
    IN gctINT                  RenderTagets,
    IN gctINT                  FactorCount,
    IN gctFLOAT *              FactorValue,
    IN gctBOOL                 AppendToLast,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateAlphaBlendingDirective(
    IN gctINT                  OutputLocation,
    IN gctBOOL                 AppendToLast,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateDepthBiasDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr
);

gceSTATUS
gcCreateNP2TextureDirective(
    IN gctINT TextureCount,
    IN gcNPOT_PATCH_PARAM_PTR NP2Texture,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateGlobalWorkSizeDirective(
    IN gcUNIFORM                GlobalWidth,
    IN gcUNIFORM                GroupWidth,
    IN gctBOOL                  PatchRealGlobalWorkSize,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateReadImageDirective(
    IN gctUINT                  SamplerNum,
    IN gctUINT                  ImageDataIndex,
    IN gctUINT                  ImageSizeIndex,
    IN gctUINT                  SamplerValue,
    IN gctUINT                  ChannelDataType,
    IN gctUINT                  ChannelOrder,
    IN gctUINT                  ImageType,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateWriteImageDirective(
    IN gctUINT                  SamplerNum,
    IN gctUINT                  ImageDataIndex,
    IN gctUINT                  ImageSizeIndex,
    IN gctUINT                  ChannelDataType,
    IN gctUINT                  ChannelOrder,
    IN gctUINT                  ImageType,
    OUT gcPatchDirective  **    PatchDirectivePtr
    );

gceSTATUS
gcCreateCLLongULongDirective(
    IN  gctUINT                 InstructionIndex,
    IN  gctUINT                 ChannelCount,
    OUT gcPatchDirective  **    PatchDirectivePtr
);

gceSTATUS
gcCreateRemoveAssignmentForAlphaChannel(
    IN gctBOOL *               RemoveOutputAlpha,
    IN gctUINT                 OutputCount,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateYFlippedShaderDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateFrontFacingDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateAlphaTestDirective(
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateSampleMaskDirective(
    IN gctBOOL                 AlphaToConverageEnabled,
    IN gctBOOL                 SampleConverageEnabled,
    IN gctBOOL                 SampleMaskEnabled,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateSignExtent(
    IN gcUNIFORM               Uniform,
    IN gctUINT16               ArrayIndex,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateTcsInputMismatch(
    IN  gctINT                  InputCount,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateColorKillDirective(
    IN gctFLOAT                Value,
    OUT gcPatchDirective  **   PatchDirectivePtr
    );

gceSTATUS
gcCreateAlphaBlendDirective(
IN  gctINT                 OutputLocation,
OUT gcPatchDirective  **   PatchDirectivePtr
);

gceSTATUS
gcDestroyPatchDirective(
    IN OUT gcPatchDirective **  PatchDirectivePtr
    );

gceSTATUS
gcLoadCLPatchLibrary(
    IN gcSHADER   Shader,
    IN gctUINT    LibIndex
    );

gceSTATUS
gcFreeCLPatchLibrary(
    void
    );

/* Query samplers ids for PrimarySamplerID in PatchDirectivePtr
 * if found PrimarySamplerID in the directive, return layers
 * in *Layers, all sampler id (physical) in SamplersID
 */
gceSTATUS
gcQueryFormatConvertionDirectiveSampler(
    IN   gcPatchDirective *   PatchDirectivePtr,
    IN   gcUNIFORM            Sampler,
    IN   gctINT               ArrayIndex,
    IN   gctUINT              SamplerBaseOffset,
    OUT  gctUINT *            SamplersID,
    OUT  gctUINT *            Layers,
    OUT  gctBOOL *            Swizzled
    );

/* Query compiler generated output locations for PrimaryOutputLocation
 * in PatchDirectivePtr, if PrimaryOutputLocation is found in the directive,
 * return layers in *Layers, all outputs location in OutputsLocation
 */
gceSTATUS
gcQueryOutputConversionDirective(
    IN   gcPatchDirective *   PatchDirectivePtr,
    IN   gctUINT              PrimaryOutputLocation,
    OUT  gctUINT *            OutputsLocation,
    OUT  gctUINT *            Layers
    );

gceSTATUS gcLockLoadLibrary(void);
gceSTATUS gcUnLockLoadLibrary(void);
/*******************************************************************************************
**  Initialize recompilation
*/
gceSTATUS
gcInitializeRecompilation(void);

/*******************************************************************************************
**  Finalize recompilation.
**
*/
gceSTATUS
gcFinalizeRecompilation(void);

/* dynamic patch shader */
gceSTATUS gcSHADER_DynamicPatch(
    IN OUT gcSHADER         Shader,
    IN gcPatchDirective  *  PatchDirective,
    IN gctUINT              isScalar
    );

/*******************************************************************************
**                                gcSaveGraphicsProgram
********************************************************************************
**
**  Save pre-compiled shaders and pre-linked programs to a binary file.
**
**  INPUT:
**
**      gcSHADER* GraphicsShaders
**          Pointer to graphics shader object.
**
**      gcsPROGRAM_STATE ProgramState
**          Pointer to program state buffer.
**
**  OUTPUT:
**
**      gctPOINTER * Binary
**          Pointer to a variable receiving the binary data to be saved.
**
**      gctUINT32 * BinarySize
**          Pointer to a variable receiving the number of bytes inside 'Binary'.
*/
gceSTATUS
gcSaveGraphicsProgram(
    IN gcSHADER* GraphicsShaders,
    IN gcsPROGRAM_STATE ProgramState,
    OUT gctPOINTER * Binary,
    OUT gctUINT32 * BinarySize
    );

/*******************************************************************************
**                                gcSaveComputeProgram
********************************************************************************
**
**    Save pre-compiled shaders and pre-linked programs to a binary file.
**
**    INPUT:
**
**        gcSHADER ComputeShader
**            Pointer to compute shader object.
**
**        gcsPROGRAM_STATE ProgramState
**            Program state.
**
**    OUTPUT:
**
**        gctPOINTER * Binary
**            Pointer to a variable receiving the binary data to be saved.
**
**        gctUINT32 * BinarySize
**            Pointer to a variable receiving the number of bytes inside 'Binary'.
*/
gceSTATUS
gcSaveComputeProgram(
    IN gcSHADER ComputeShader,
    IN gcsPROGRAM_STATE ProgramState,
    OUT gctPOINTER * Binary,
    OUT gctUINT32 * BinarySize
    );

/*******************************************************************************
**                                gcSaveCLSingleKerne
********************************************************************************
**
**    Save pre-compiled shaders and pre-linked programs to a binary file.
**
**    INPUT:
**
**        gcSHADER KernelShader
**            Pointer to vertex shader object.
**
**        gcsPROGRAM_STATE ProgramState
**            Program state.
**
**    OUTPUT:
**
**        gctPOINTER * Binary
**            Pointer to a variable receiving the binary data to be saved.
**
**        gctUINT32 * BinarySize
**            Pointer to a variable receiving the number of bytes inside 'Binary'.
*/

gceSTATUS
gcSaveCLSingleKernel(
    IN gcSHADER KernelShader,
    IN gcsPROGRAM_STATE ProgramState,
    OUT gctPOINTER * Binary,
    OUT gctUINT32 * BinarySize
    );


/*******************************************************************************
**                                gcLoadGraphicsProgram
********************************************************************************
**
**    Load pre-compiled shaders and pre-linked programs from a binary file.
**
**    INPUT:
**
**        gctPOINTER Binary
**            Pointer to the binary data loaded.
**
**        gctUINT32 BinarySize
**            Number of bytes in 'Binary'.
**
**    OUTPUT:
**
**      gcSHADER* GraphicsShaders
**          Pointer to graphics shader object.
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receicing the program state.
*/
gceSTATUS
gcLoadGraphicsProgram(
    IN gctPOINTER Binary,
    IN gctUINT32 BinarySize,
    IN OUT gcSHADER* GraphicsShaders,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );

/*******************************************************************************
**                                gcLoadComputeProgram
********************************************************************************
**
**    Load pre-compiled shaders and pre-linked programs from a binary file.
**
**    INPUT:
**
**        gctPOINTER Binary
**            Pointer to the binary data loaded.
**
**        gctUINT32 BinarySize
**            Number of bytes in 'Binary'.
**
**    OUTPUT:
**
**        gcSHADER ComputeShader
**            Pointer to a compute shader object.
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receicing the program state.
*/
gceSTATUS
gcLoadComputeProgram(
    IN gctPOINTER Binary,
    IN gctUINT32 BinarySize,
    OUT gcSHADER ComputeShader,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );

/*******************************************************************************
**                                gcLoadCLSingleKernel
********************************************************************************
**
**    Load pre-compiled shaders and pre-linked programs from a binary file.
**
**    INPUT:
**
**        gctPOINTER Binary
**            Pointer to the binary data loaded.
**
**        gctUINT32 BinarySize
**            Number of bytes in 'Binary'.
**
**    OUTPUT:
**
**        gcSHADER KernelShader
**            Pointer to a vertex shader object.
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receicing the program state.
*/
gceSTATUS
gcLoadCLSingleKernel(
    IN gctPOINTER Binary,
    IN gctUINT32 BinarySize,
    OUT gcSHADER KernelShader,
    IN OUT gcsPROGRAM_STATE *ProgramState
    );

/*******************************************************************************
**                              gcCompileKernel
********************************************************************************
**
**    Compile a OpenCL kernel shader.
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctUINT SourceSize
**            Size of the source buffer in bytes.
**
**        gctCONST_STRING Source
**            Pointer to the buffer containing the shader source code.
**
**    OUTPUT:
**
**        gcSHADER * Binary
**            Pointer to a variable receiving the pointer to a gcSHADER object
**            containg the compiled shader code.
**
**        gctSTRING * Log
**            Pointer to a variable receiving a string pointer containging the
**            compile log.
*/
gceSTATUS
gcCompileKernel(
    IN gcoHAL Hal,
    IN gctUINT SourceSize,
    IN gctCONST_STRING Source,
    IN gctCONST_STRING Options,
    OUT gcSHADER * Binary,
    OUT gctSTRING * Log
    );

/*******************************************************************************
**                              gcCLCompileProgram
********************************************************************************
**
**    Compile a OpenCL kernel program as in clCompileProgram().
**
**    INPUT:
**
**        gcoOS Hal
**            Pointer to an gcoHAL object.
**
**        gctUINT SourceSize
**            Size of the source buffer in bytes.
**
**        gctCONST_STRING Source
**            Pointer to the buffer containing the shader source code.
**
**        gctCONST_STRING Options
**            Pointer to the buffer containing the compiler options.
**
**        gctUINT NumInputHeaders
**            Number of embedded input headers.
**
**        gctCONST_STRING *InputHeaders
**            Array of pointers to the embedded header sources.
**
**        gctCONST_STRING *HeaderIncludeNames
**            Array of pointers to the header include names.
**
**    OUTPUT:
**
**        gcSHADER * Binary
**            Pointer to a variable receiving the pointer to a gcSHADER object
**            containg the compiled shader code.
**
**        gctSTRING * Log
**            Pointer to a variable receiving a string pointer containging the
**            compile log.
*/
gceSTATUS
gcCLCompileProgram(
    IN gcoHAL Hal,
    IN gctUINT SourceSize,
    IN gctCONST_STRING Source,
    IN gctCONST_STRING Options,
    IN gctUINT NumInputHeaders,
    IN gctCONST_STRING *InputHeaders,
    IN gctCONST_STRING *HeaderIncludeNames,
    OUT gcSHADER * Binary,
    OUT gctSTRING * Log
    );

/*******************************************************************************
**                                gcLinkKernel
********************************************************************************
**
**    Link OpenCL kernel and generate a harwdare specific state buffer by compiling
**    the compiler generated code through the resource allocator and code
**    generator.
**
**    INPUT:
**
**        gcSHADER Kernel
**            Pointer to a gcSHADER object holding information about the compiled
**            OpenCL kernel.
**
**        gceSHADER_FLAGS Flags
**            Compiler flags.  Can be any of the following:
**
**                gcvSHADER_DEAD_CODE       - Dead code elimination.
**                gcvSHADER_RESOURCE_USAGE  - Resource usage optimizaion.
**                gcvSHADER_OPTIMIZER       - Full optimization.
**                gcvSHADER_USE_GL_Z        - Use OpenGL ES Z coordinate.
**                gcvSHADER_USE_GL_POSITION - Use OpenGL ES gl_Position.
**                gcvSHADER_USE_GL_FACE     - Use OpenGL ES gl_FaceForward.
**
**    OUTPUT:
**
**        gcsPROGRAM_STATE *ProgramState
**            Pointer to a variable receiving the program states.
*/
gceSTATUS
gcLinkKernel(
    IN gcSHADER Kernel,
    IN gceSHADER_FLAGS Flags,
    OUT gcsPROGRAM_STATE *ProgramState
    );

void
gcTYPE_GetTypeInfo(
    IN gcSHADER_TYPE      Type,
    OUT gctUINT32 *       Components,
    OUT gctUINT32 *       Rows,
    OUT gctCONST_STRING * Name
    );

gctBOOL
gcTYPE_IsTypePacked(
    IN gcSHADER_TYPE      Type
    );

void
gcTYPE_GetFormatInfo(
    IN  gcSL_FORMAT       ElemFormat,
    IN  gctUINT32         Components,
    OUT gctUINT32 *       Rows,
    OUT gcSHADER_TYPE *   Type
    );

gcSHADER_TYPE
gcGetShaderTypeFromFormatAndComponentCount(
    IN gcSL_FORMAT  ElemFormat,
    IN gctINT       ComponentCount,
    IN gctINT       RowCount
    );

gceSTATUS
gcSHADER_SetTransformFeedbackVarying(
    IN gcSHADER Shader,
    IN gctUINT32 Count,
    IN gctCONST_STRING *Varyings,
    IN gceFEEDBACK_BUFFER_MODE BufferMode
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingCount(
    IN gcSHADER Shader,
    OUT gctUINT32 * Count
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVarying(
    IN gcSHADER Shader,
    IN gctUINT32 Index,
    OUT gctCONST_STRING * Name,
    OUT gctUINT *  Length,
    OUT gcSHADER_TYPE * Type,
    OUT gctBOOL * IsArray,
    OUT gctUINT * Size
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingStride(
    IN gcSHADER Shader,
    OUT gctUINT32 * Stride
    );

gceSTATUS
gcSHADER_GetTransformFeedbackVaryingStrideSeparate(
    IN gcSHADER Shader,
    IN gctUINT VaryingIndex,
    OUT gctUINT32 * Stride
    );

gceSTATUS
gcSHADER_FreeRecompilerLibrary(
    void
    );

gceSTATUS
gcSHADER_FreeBlendLibrary(
void
);

gceSTATUS
gcSHADER_InsertList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST *             Root,
    IN gctINT                      Index,
    IN gctINT                      Data0,
    IN gctINT                      Data1
    );

gceSTATUS
gcSHADER_UpdateList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Index,
    IN gctINT                      NewIndex
    );

gceSTATUS
gcSHADER_DeleteList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST *             Root,
    IN gctINT                      Index
    );

gceSTATUS
gcSHADER_FindList(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Index,
    IN gcSHADER_LIST *             List
    );

gceSTATUS
gcSHADER_FindListByData(
    IN gcSHADER                    Shader,
    IN gcSHADER_LIST               Root,
    IN gctINT                      Data0,
    IN gctINT                      Data1,
    IN gcSHADER_LIST *             List
    );

gceSTATUS
gcSHADER_SetEarlyFragTest(
    IN gcSHADER                    Shader,
    IN gctBOOL                     UseEarlyFragTest
    );

gceSTATUS
gcSHADER_GetEarlyFragTest(
    IN gcSHADER                    Shader,
    IN gctBOOL *                   UseEarlyFragTest
    );

gceSTATUS
gcSHADER_GetInstructionCount(
    IN gcSHADER                    Shader,
    IN gctUINT *                   InstructionCount
    );

gceSTATUS
gcSHADER_QueryValueOrder(
    IN gcSHADER                    Shader,
    OUT gctUINT_PTR                ValueOrder
    );

gceSTATUS
gcSHADER_GetTCSPatchOutputVertices(
    IN  gcSHADER                    Shader,
    OUT gctINT *                    TCSPatchOutputVertices
    );

gceSTATUS
gcSHADER_GetTCSPatchInputVertices(
    IN  gcSHADER                    Shader,
    OUT gctINT *                    TCSPatchInputVertices
    );

gceSTATUS
gcSHADER_GetTCSInputVerticesUniform(
    IN  gcSHADER                    Shader,
    OUT gcUNIFORM *                 TCSInputVerticesUniform
    );

gceSTATUS
gcSHADER_GetTESPrimitiveMode(
    IN  gcSHADER                    Shader,
    OUT gcTessPrimitiveMode *       TESPrimitiveMode
    );

gceSTATUS
gcSHADER_GetTESVertexSpacing(
    IN  gcSHADER                    Shader,
    OUT gcTessVertexSpacing *       TESVertexSpacing
    );

gceSTATUS
gcSHADER_GetTESOrdering(
    IN  gcSHADER                    Shader,
    OUT gcTessOrdering *            TESOrdering
    );

gceSTATUS
gcSHADER_GetTESPointMode(
    IN  gcSHADER                    Shader,
    OUT gctBOOL *                   TESPointMode
    );

gceSTATUS
gcSHADER_GetTESPatchInputVertices(
    IN  gcSHADER                    Shader,
    OUT gctINT *                    TESPatchInputVertices
    );

gceSTATUS
gcSHADER_GetGSLayout(
    IN  gcSHADER                    Shader,
    OUT gcGEOLayout *               Layout
    );

gceSTATUS
gcSHADER_InsertNOP2BeforeCode(
    IN OUT gcSHADER                 Shader,
    OUT gctUINT                     CodeIndex,
    OUT gctUINT                     AddCodeCount,
    IN  gctBOOL                     ReplaceJmp,
    IN  gctBOOL                     MergeWithCodeIndexFunc
    );

gceSTATUS
gcSHADER_MoveCodeListBeforeCode(
    IN OUT gcSHADER                 Shader,
    IN  gctUINT                     CodeIndex,
    IN  gctUINT                     CodeHead,
    IN  gctUINT                     CodeTail
    );

gceSTATUS
gcSHADER_ComputeTotalFeedbackVaryingsSize(
    IN gcSHADER                    VertexShader
    );

gceSTATUS
gcSHADER_GetNotStagesRelatedLinkError(
    IN gcSHADER                    Shader,
    OUT gceSTATUS *                NotStagesRelatedLinkError
    );

gceSTATUS
gcSHADER_SetNotStagesRelatedLinkError(
    IN gcSHADER                    Shader,
    IN gceSTATUS                   NotStagesRelatedLinkError
    );

gceSTATUS
gcSHADER_Has64BitOperation(
    IN gcSHADER                    Shader
    );

gceSTATUS
gcSHADER_SetBuildOptions(
    IN gcSHADER             Shader,
    IN gctSTRING            Options
    );

void
gcSHADER_SetDebugInfo(
    IN gcSHADER             Shader,
    IN void *                DebugInfoContext
    );

gctPOINTER
gcSHADER_GetDebugInfo(
    IN gcSHADER             Shader
    );

void
gcSHADER_SetFragOutUsage(
    IN gcSHADER             Shader,
    IN gctUINT              Usage
    );

gceSTATUS
gcSHADER_SetAttrLocationByDriver(
    IN gcSHADER             Shader,
    IN gctCHAR*             Name,
    IN gctINT               Location
    );

gceSTATUS
gcSHADER_SetCLProgramBinaryType(
    IN gcSHADER             Shader,
    IN gctUINT              clProgramBinaryType
    );

gctBOOL
gceLAYOUT_QUALIFIER_HasHWNotSupportingBlendMode(
    IN gceLAYOUT_QUALIFIER         Qualifier
    );

/* return the unsized array variable for StorageBlock
 * if it's last block member the unsized array variable is returned
 * otherwise NULL
 */
gcVARIABLE
gcGetSBLastVariable(
    IN gcSHADER          Shader,
    IN gcsSTORAGE_BLOCK  StorageBlock
    );

/* return unsized array leng for StorageBlock in *UnsizedArrayLength
 * if it's last block member is unsized array variable
 * otherwise return status = gcvSTATUS_INVALID_REQUEST, and length set to 0
 */
gceSTATUS
gcGetSBUnsizedArrayLength(
    IN gcSHADER          Shader,
    IN gcsSTORAGE_BLOCK  StorageBlock,
    IN gctINT            TotalSizeInBytes,
    OUT gctINT *         UnsizedArrayLength
    );

gctBOOL
gcIsSBUnsized(
    IN gcsSTORAGE_BLOCK  StorageBlock
    );

gctUINT
gcHINTS_GetSamplerBaseOffset(
    IN gcsHINT_PTR Hints,
    IN gcSHADER Shader
    );

gceSTATUS
gcHINTS_Destroy(
    IN gcsHINT_PTR Hints
    );

gctINT
gcSL_GetName(
    IN gctUINT32 Length,
    IN gctCONST_STRING Name,
    OUT char * Buffer,
    gctUINT32 BufferSize
    );

gctUINT8
gcSL_ConvertSwizzle2Enable(
    IN gcSL_SWIZZLE X,
    IN gcSL_SWIZZLE Y,
    IN gcSL_SWIZZLE Z,
    IN gcSL_SWIZZLE W
    );

gceSTATUS
gcFreeProgramState(
    IN gcsPROGRAM_STATE ProgramState
    );

/* dump instruction to stdout */
void dbg_dumpIR(gcSL_INSTRUCTION Inst,gctINT n);

END_EXTERN_C()

#endif /* __gc_vsc_old_drvi_interface_h_ */


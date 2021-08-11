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


/***********************************************************************************
   All following definitions are designed for DX/openGL(ES)/Vulkan/openCL drivers to
   manage to do shaders' HW-level linkage programming, shader flush programming and
   shader level recompiler. NOTE that only info that shader uses (these info must be
   assigned specific HW resource for them) are recorded in SEP, any other redundant
   info will not be recorded in SEP.
************************************************************************************/
#ifndef __gc_vsc_drvi_shader_profile_h_
#define __gc_vsc_drvi_shader_profile_h_

BEGIN_EXTERN_C()

/* In-order needs compiler allocate inputs based on io-index ordering, will enable after
   fully switching to new code-gen */
#define IO_HW_LOC_NUMBER_IN_ORDER                0

/* Whether hwRegChannel of SHADER_IO_CHANNEL_MAPPING is identical to channel designating to
   this SHADER_IO_CHANNEL_MAPPING. When it is disabled, any hwRegChannel can be allocated to
   this SHADER_IO_CHANNEL_MAPPING */
#define IO_HW_CHANNEL_IDENTICAL_TO_IO_LL_CHANNEL 1

/* Some macroes for SEP profile version */
#define ENCODE_SEP_VER_TYPE(major, minor)      ((((major)  & 0xFF) << 0x08) | \
                                                (((minor)  & 0xFF) << 0x00))

#define DECODE_SEP_MAJOR_VER(vt)      (((vt) >> 0x08) & 0xFF)
#define DECODE_SEP_MINOR_VER(vt)      (((vt) >> 0x00) & 0xFF)

/* Some macroes for version type */
#define ENCODE_SHADER_VER_TYPE(client, type, major, minor)      ((((client) & 0xFF) << 0x18) | \
                                                                 (((type)   & 0xFF) << 0x10) | \
                                                                 (((major)  & 0xFF) << 0x08) | \
                                                                 (((minor)  & 0xFF) << 0x00))
#define DECODE_SHADER_CLIENT(vt)         (((vt) >> 0x18) & 0xFF)
#define DECODE_SHADER_TYPE(vt)           (((vt) >> 0x10) & 0xFF)
#define DECODE_SHADER_MAJOR_VER(vt)      (((vt) >> 0x08) & 0xFF)
#define DECODE_SHADER_MINOR_VER(vt)      (((vt) >> 0x00) & 0xFF)

/* Capability definition of executable profile */

#define MAX_SHADER_IO_NUM                      36

/* 0~15 for client normal constant buffers, such as DX cb/icb or GLES uniform block
   16 for normal uniforms of OGL(ES), AKA, default uniform block
   17 for LTC constants
   18 for immediate values
   19~31 for other internal usages of constant buffers for patches and other recompiler requests */
#define MAX_SHADER_CONSTANT_ARRAY_NUM          32
#define GLCL_NORMAL_UNIFORM_CONSTANT_ARRAY     16
#define LTC_CONSTANT_ARRAY                     GLCL_NORMAL_UNIFORM_CONSTANT_ARRAY + 1
#define IMM_CONSTANT_ARRAY                     LTC_CONSTANT_ARRAY + 1
#define START_CONSTANT_ARRAY_FOR_PATCH         IMM_CONSTANT_ARRAY + 1

#define MAX_SHADER_SAMPLER_NUM                 16
#define MAX_SHADER_RESOURCE_NUM                128
#define MAX_SHADER_STREAM_OUT_BUFFER_NUM       4
#define MAX_SHADER_UAV_NUM                     8

/* Channel definition */

#define CHANNEL_NUM                            4
#define CHANNEL_X                              0  /* R */
#define CHANNEL_Y                              1  /* G */
#define CHANNEL_Z                              2  /* B */
#define CHANNEL_W                              3  /* A */

#define WRITEMASK_X                            0x1
#define WRITEMASK_Y                            0x2
#define WRITEMASK_Z                            0x4
#define WRITEMASK_W                            0x8
#define WRITEMASK_XY                           (WRITEMASK_X|WRITEMASK_Y)
#define WRITEMASK_XYZ                          (WRITEMASK_X|WRITEMASK_Y|WRITEMASK_Z)
#define WRITEMASK_XYZW                         (WRITEMASK_X|WRITEMASK_Y|WRITEMASK_Z|WRITEMASK_W)
#define WRITEMASK_ALL                          WRITEMASK_XYZW
#define WRITEMASK_NONE                         0x0

#define NOT_ASSIGNED                           0xFFFFFFFF

typedef enum SHADER_TYPE
{
    SHADER_TYPE_UNKNOWN             = 0,
    SHADER_TYPE_VERTEX              = 1,
    SHADER_TYPE_PIXEL               = 2,
    SHADER_TYPE_GEOMETRY            = 3,
    SHADER_TYPE_HULL                = 4, /* TCS/TI */
    SHADER_TYPE_DOMAIN              = 5, /* TES/TS */

    /* GPGPU shader, such as openCL and DX's compute */
    SHADER_TYPE_GENERAL             = 6,

    /* It is not a shader, but indicates a fixed-function unit. */
    SHADER_TYPE_FFU                 = SHADER_TYPE_UNKNOWN,
}
SHADER_TYPE;

typedef enum SHADER_CLIENT
{
    SHADER_CLIENT_UNKNOWN           = 0,
    SHADER_CLIENT_DX                = 1,
    SHADER_CLIENT_GL                = 2,
    SHADER_CLIENT_GLES              = 3,
    SHADER_CLIENT_CL                = 4,
    SHADER_CLIENT_VK                = 5,
}
SHADER_CLIENT;

typedef enum SHADER_IO_USAGE
{
    /* 0 - 13 must be equal to D3DDECLUSAGE!!!!!!! SO DONT CHANGE THEIR VALUES!!!! */

    /* Common usages, for gfx clients only */
    SHADER_IO_USAGE_POSITION                 = 0,
    SHADER_IO_USAGE_TESSFACTOR               = 8,
    SHADER_IO_USAGE_ISFRONTFACE              = 18,

    /* For gfx clients only, but excluding DX1x */
    SHADER_IO_USAGE_BLENDWEIGHT              = 1,
    SHADER_IO_USAGE_BLENDINDICES             = 2,
    SHADER_IO_USAGE_NORMAL                   = 3,
    SHADER_IO_USAGE_POINTSIZE                = 4,
    SHADER_IO_USAGE_TEXCOORD                 = 5,
    SHADER_IO_USAGE_TANGENT                  = 6,
    SHADER_IO_USAGE_BINORMAL                 = 7,
    SHADER_IO_USAGE_TRANSFORMEDPOS           = 9,
    SHADER_IO_USAGE_COLOR                    = 10,
    SHADER_IO_USAGE_FOG                      = 11,
    SHADER_IO_USAGE_DEPTH                    = 12,
    SHADER_IO_USAGE_SAMPLE                   = 13,

    /* For gfx clients only, but excluding DX9 - SGV */
    SHADER_IO_USAGE_VERTEXID                 = 14,
    SHADER_IO_USAGE_PRIMITIVEID              = 15,
    SHADER_IO_USAGE_INSTANCEID               = 16,
    SHADER_IO_USAGE_INPUTCOVERAGE            = 17,
    SHADER_IO_USAGE_SAMPLE_INDEX             = 19,
    SHADER_IO_USAGE_OUTPUTCONTROLPOINTID     = 20,
    SHADER_IO_USAGE_FORKINSTANCEID           = 21,
    SHADER_IO_USAGE_JOININSTANCEID           = 22,
    SHADER_IO_USAGE_DOMAIN_LOCATION          = 23,

    /* For GPGPU client only */
    SHADER_IO_USAGE_THREADID                 = 24,
    SHADER_IO_USAGE_THREADGROUPID            = 25,
    SHADER_IO_USAGE_THREADIDINGROUP          = 26,
    SHADER_IO_USAGE_THREADIDINGROUPFLATTENED = 27,

    /* For gfx clients only, but excluding DX9 - SIV */
    SHADER_IO_USAGE_CLIPDISTANCE             = 28,
    SHADER_IO_USAGE_CULLDISTANCE             = 29,
    SHADER_IO_USAGE_RENDERTARGETARRAYINDEX   = 30,
    SHADER_IO_USAGE_VIEWPORTARRAYINDEX       = 31,
    SHADER_IO_USAGE_DEPTHGREATEREQUAL        = 32,
    SHADER_IO_USAGE_DEPTHLESSEQUAL           = 33,
    SHADER_IO_USAGE_INSIDETESSFACTOR         = 34,
    SHADER_IO_USAGE_SAMPLE_MASK              = 35,

    /* For gfx clients only, and only for GL */
    SHADER_IO_USAGE_POINT_COORD              = 36,
    SHADER_IO_USAGE_FOG_FRAG_COORD           = 37,
    SHADER_IO_USAGE_HELPER_PIXEL             = 38,

    /* For gfx pixel-frequency only (sample-frequency will directly use SHADER_IO_USAGE_DEPTH),
       HW internal SGV/SIV, no client spec refer to it. */
    SHADER_IO_USAGE_SAMPLE_DEPTH             = 39,

    /* For gfx sample-frequency only */
    SHADER_IO_USAGE_SAMPLE_POSITION          = 40,

    /* For gfx's HS/DS/GS only, and only for GL */
    SHADER_IO_USAGE_INPUT_VTX_CP_COUNT       = 41,

    /* For gfx's GS only */
    SHADER_IO_USAGE_INSTANCING_ID            = 42,

    /* A special usage which means IO is used by general purpose */
    SHADER_IO_USAGE_GENERAL                  = 43,

    /* For GPGPU client only */
    SHADER_IO_USAGE_CLUSTER_ID               = 44,

    /* Add NEW usages before here, and make sure update strUsageName too. */

    /* Must be at last!!!!!!! */
    SHADER_IO_USAGE_TOTAL_COUNT,
}
SHADER_IO_USAGE;

#define IS_SHADER_IO_USAGE_SGV(usage)                                                                \
    (((usage) >= SHADER_IO_USAGE_VERTEXID && (usage) <= SHADER_IO_USAGE_THREADIDINGROUPFLATTENED) || \
     ((usage) == SHADER_IO_USAGE_ISFRONTFACE)                                                     || \
     ((usage) == SHADER_IO_USAGE_SAMPLE_MASK)                                                     || \
     ((usage) == SHADER_IO_USAGE_SAMPLE_POSITION)                                                 || \
     ((usage) == SHADER_IO_USAGE_CLUSTER_ID)                                                      || \
     ((usage) >= SHADER_IO_USAGE_POINT_COORD && (usage) <= SHADER_IO_USAGE_INSTANCING_ID))

#define IS_SHADER_IO_USAGE_SIV(usage)                                                                \
    (((usage) >= SHADER_IO_USAGE_CLIPDISTANCE && (usage) <= SHADER_IO_USAGE_SAMPLE_MASK)          || \
     ((usage) == SHADER_IO_USAGE_POSITION)                                                        || \
     ((usage) == SHADER_IO_USAGE_TESSFACTOR)                                                      || \
     ((usage) == SHADER_IO_USAGE_SAMPLE_DEPTH)                                                    || \
     ((usage) == SHADER_IO_USAGE_POINTSIZE))

#define IS_SHADER_IO_USAGE_SV(usage) (IS_SHADER_IO_USAGE_SGV((usage)) && IS_SHADER_IO_USAGE_SIV((usage)))

typedef enum SHADER_CONSTANT_USAGE
{
    /* DX9 only */
    SHADER_CONSTANT_USAGE_FLOAT              = 0,
    SHADER_CONSTANT_USAGE_INTEGER            = 1,
    SHADER_CONSTANT_USAGE_BOOLEAN            = 2,

    /* For other clients */
    SHADER_CONSTANT_USAGE_MIXED              = 3,

    /* Must be at last!!!!!!! */
    SHADER_CONSTANT_USAGE_TOTAL_COUNT        = 4,
}
SHADER_CONSTANT_USAGE;

typedef enum SHADER_RESOURCE_DIMENSION
{
    SHADER_RESOURCE_DIMENSION_UNKNOW         = 0,
    SHADER_RESOURCE_DIMENSION_BUFFER         = 1,
    SHADER_RESOURCE_DIMENSION_1D             = 2,
    SHADER_RESOURCE_DIMENSION_1DARRAY        = 3,
    SHADER_RESOURCE_DIMENSION_2D             = 4,
    SHADER_RESOURCE_DIMENSION_2DARRAY        = 5,
    SHADER_RESOURCE_DIMENSION_2DMS           = 6,
    SHADER_RESOURCE_DIMENSION_2DMSARRAY      = 7,
    SHADER_RESOURCE_DIMENSION_3D             = 8,
    SHADER_RESOURCE_DIMENSION_CUBE           = 9,
    SHADER_RESOURCE_DIMENSION_CUBEARRAY      = 10,

    /* Must be at last!!!!!!! */
    SHADER_RESOURCE_DIMENSION_TOTAL_COUNT,
}
SHADER_RESOURCE_DIMENSION;

typedef enum SHADER_SAMPLER_MODE
{
    SHADER_SAMPLER_MODE_DEFAULT              = 0,
    SHADER_SAMPLER_MODE_COMPARISON           = 1,
    SHADER_SAMPLER_MODE_MONO                 = 2,
}
SHADER_SAMPLER_MODE;

typedef enum SHADER_RESOURCE_RETURN_TYPE
{
    SHADER_RESOURCE_RETURN_TYPE_UNORM        = 0,
    SHADER_RESOURCE_RETURN_TYPE_SNORM        = 1,
    SHADER_RESOURCE_RETURN_TYPE_SINT         = 2,
    SHADER_RESOURCE_RETURN_TYPE_UINT         = 3,
    SHADER_RESOURCE_RETURN_TYPE_FLOAT        = 4,
}
SHADER_RESOURCE_RETURN_TYPE;

typedef enum SHADER_RESOURCE_ACCESS_MODE
{
    SHADER_RESOURCE_ACCESS_MODE_TYPE         = 0,
    SHADER_RESOURCE_ACCESS_MODE_RAW          = 1,
    SHADER_RESOURCE_ACCESS_MODE_STRUCTURED   = 2,
}
SHADER_RESOURCE_ACCESS_MODE;

typedef enum SHADER_UAV_DIMENSION
{
    SHADER_UAV_DIMENSION_UNKNOWN             = 0,
    SHADER_UAV_DIMENSION_BUFFER              = 1,
    SHADER_UAV_DIMENSION_1D                  = 2,
    SHADER_UAV_DIMENSION_1DARRAY             = 3,
    SHADER_UAV_DIMENSION_2D                  = 4,
    SHADER_UAV_DIMENSION_2DARRAY             = 5,
    SHADER_UAV_DIMENSION_3D                  = 6,

    /* Must be at last!!!!!!! */
    SHADER_UAV_DIMENSION_TOTAL_COUNT,
}
SHADER_UAV_DIMENSION;

typedef enum SHADER_UAV_TYPE
{
    SHADER_UAV_TYPE_UNORM                    = 0,
    SHADER_UAV_TYPE_SNORM                    = 1,
    SHADER_UAV_TYPE_SINT                     = 2,
    SHADER_UAV_TYPE_UINT                     = 3,
    SHADER_UAV_TYPE_FLOAT                    = 4,
}
SHADER_UAV_TYPE;

typedef enum SHADER_UAV_ACCESS_MODE
{
    SHADER_UAV_ACCESS_MODE_TYPE              = 0,
    SHADER_UAV_ACCESS_MODE_RAW               = 1,
    SHADER_UAV_ACCESS_MODE_STRUCTURED        = 2,
    SHADER_UAV_ACCESS_MODE_RESIZABLE         = 3,
}
SHADER_UAV_ACCESS_MODE;

typedef enum SHADER_HW_ACCESS_MODE
{
    SHADER_HW_ACCESS_MODE_REGISTER           = 0,
    SHADER_HW_ACCESS_MODE_MEMORY             = 1,
    SHADER_HW_ACCESS_MODE_BOTH_REG_AND_MEM   = 2,
}
SHADER_HW_ACCESS_MODE;

typedef enum SHADER_HW_MEM_ACCESS_MODE
{
    SHADER_HW_MEM_ACCESS_MODE_PLACE_HOLDER   = 0,
    SHADER_HW_MEM_ACCESS_MODE_DIRECT_MEM_ADDR= 1,
    SHADER_HW_MEM_ACCESS_MODE_DIRECT_SAMPLER = 2,
    SHADER_HW_MEM_ACCESS_MODE_SRV            = 3,
    SHADER_HW_MEM_ACCESS_MODE_UAV            = 4,
}
SHADER_HW_MEM_ACCESS_MODE;

typedef enum SHADER_TESSELLATOR_DOMAIN_TYPE
{
    SHADER_TESSELLATOR_DOMAIN_ISOLINE        = 0,
    SHADER_TESSELLATOR_DOMAIN_TRIANGLE       = 1,
    SHADER_TESSELLATOR_DOMAIN_QUAD           = 2
} SHADER_TESSELLATOR_DOMAIN_TYPE;

typedef enum SHADER_TESSELLATOR_PARTITION_TYPE
{
    SHADER_TESSELLATOR_PARTITION_INTEGER         = 0,
    SHADER_TESSELLATOR_PARTITION_POW2            = 1,
    SHADER_TESSELLATOR_PARTITION_FRACTIONAL_ODD  = 2,
    SHADER_TESSELLATOR_PARTITION_FRACTIONAL_EVEN = 3
}
SHADER_TESSELLATOR_PARTITION_TYPE;

typedef enum SHADER_TESSELLATOR_OUTPUT_PRIMITIVE_TOPOLOGY
{
    SHADER_TESSELLATOR_OUTPUT_PRIM_POINT         = 0,
    SHADER_TESSELLATOR_OUTPUT_PRIM_LINE          = 1,
    SHADER_TESSELLATOR_OUTPUT_PRIM_TRIANGLE_CW   = 2,
    SHADER_TESSELLATOR_OUTPUT_PRIM_TRIANGLE_CCW  = 3
} SHADER_TESSELLATOR_OUTPUT_PRIMITIVE_TOPOLOGY;

typedef enum SHADER_GS_INPUT_PRIMITIVE_TOPOLOGY
{
    SHADER_GS_INPUT_PRIMITIVE_POINT              = 0,
    SHADER_GS_INPUT_PRIMITIVE_LINE               = 1,
    SHADER_GS_INPUT_PRIMITIVE_TRIANGLE           = 2,
    SHADER_GS_INPUT_PRIMITIVE_LINE_ADJ           = 3,
    SHADER_GS_INPUT_PRIMITIVE_TRIANGLE_ADJ       = 4,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_1            = 5,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_2            = 6,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_3            = 7,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_4            = 8,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_5            = 9,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_6            = 10,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_7            = 11,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_8            = 12,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_9            = 13,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_10           = 14,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_11           = 15,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_12           = 16,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_13           = 17,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_14           = 18,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_15           = 19,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_16           = 20,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_17           = 21,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_18           = 22,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_19           = 23,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_20           = 24,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_21           = 25,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_22           = 26,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_23           = 27,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_24           = 28,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_25           = 29,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_26           = 30,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_27           = 31,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_28           = 32,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_29           = 33,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_30           = 34,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_31           = 35,
    SHADER_GS_INPUT_PRIMITIVE_PATCH_32           = 36,
} SHADER_GS_INPUT_PRIMITIVE_TOPOLOGY;

typedef enum SHADER_GS_OUTPUT_PRIMITIVE_TOPOLOGY
{
    SHADER_GS_OUTPUT_PRIMITIVE_POINTLIST         = 0,
    SHADER_GS_OUTPUT_PRIMITIVE_LINESTRIP         = 1,
    SHADER_GS_OUTPUT_PRIMITIVE_TRIANGLESTRIP     = 2,
}SHADER_GS_OUTPUT_PRIMITIVE_TOPOLOGY;

typedef enum SHADER_IO_MODE
{
    /* For inputs of HS/DS/GS, per-prim of HS, and per-prim of DS if it includes
       per-prim input other than domain-location, they must be set to be active
       mode, for outputs of PS and per-prim of GS, they must be set to passive mode,
       for others, they can be any of two */
    SHADER_IO_MODE_PASSIVE                       = 0,
    SHADER_IO_MODE_ACTIVE                        = 1,
}SHADER_IO_MODE;

/* These are only used for active mode */
typedef enum SHADER_IO_MEM_ALIGN
{
    SHADER_IO_MEM_ALIGN_4_CHANNELS               = 0,

    /* This pack is not io-pack designated by packedIoIndexMask. This is a mem
       alignment pack both for unpacked io or packed io. For example, o0 and o1
       is packed together into o0, but not packed with o2, but in memory, this
       packed o0 can be still be packed with o2. */
    SHADER_IO_MEM_ALIGN_PACKED                   = 1,
}SHADER_IO_MEM_ALIGN;

typedef enum SHADER_IO_CATEGORY
{
    SHADER_IO_CATEGORY_PER_VTX_PXL               = 0,
    SHADER_IO_CATEGORY_PER_PRIM                  = 1,
}SHADER_IO_CATEGORY;

/* IO mapping table definitions, for v# and o#

   They are used to HW-level linkage programming. For DX9, such linking will alway
   use usage and usage index, but for others clients, it will use ioIndex (ioIndex
   of peers must be same) for most IOs, and usage and usage index for special IOs,
   such as frontface and pointsize, etc.

   Note that, each shader thread works on specific object, such as vertex/pixel/patch.
   Some of objects are composed by multiple other sub-objects, for example, a patch is
   composed by multiple CPs, so such invocation for patch will need per-patch IO and
   per-CP IO, SHADER_IO_MAPPING_PER_EXE_OBJ reflects this situation.
*/

#define SPECIAL_HW_IO_REG_NO           0xFFFFFFFE
typedef struct SHADER_IO_CHANNEL_MAPPING
{
    struct
    {
        /* This channel is active within shader */
        gctUINT                                 bActiveWithinShader     : 1;

        /* This IO channel is declared by 'DCL' instruction, DX only */
        gctUINT                                 bDeclared               : 1;

        /* PS input only, get attributes directly from heading vertex */
        gctUINT                                 bConstInterpolate       : 1;

        /* PS input only, interpolate is from centroid, not pixel center */
        gctUINT                                 bCentroidInterpolate    : 1;

        /* PS input only, need (v#/RHW) */
        gctUINT                                 bPerspectiveCorrection  : 1;

        /* PS input only, for MSAA's subpixel rendering */
        gctUINT                                 bSampleFrequency        : 1;

        /* PS input only, indicate the input has integer data type. It is
           only valid when bHighPrecisionOnDual16 is FALSE on dual16 mode */
        gctUINT                                 bInteger                : 1;

        /* PS only, for high-precision io when executing on dual16 mode */
        gctUINT                                 bHighPrecisionOnDual16  : 1;

        /* Pre-RA shder stages output only, for stream output */
        gctUINT                                 bStreamOutToBuffer      : 1;

        /* Reserved bits */
        gctUINT                                 reserved                : 23;
    } flag;

    /* Usage of this channel, it will be initialized to SHADER_IO_USAGE_GENERAL at first */
    SHADER_IO_USAGE                             ioUsage;

    /* For DX9, it may be greater than 0, but for otherwise, almost all are always 0 */
    gctUINT                                     usageIndex;

    struct
    {
        struct
        {
            union
            {
                /* HW loc number this IO channel is colored to. A channel of same 4-tuple IO register
                   may be colored to different HW loc when this channel is SVs. For channels other than
                   SVs, they must be colored into same hwLoc. Also, when passive mode, for some of SVs,
                   some of HW arch may use special HW reg to reference them, such as frontface/instanceid/
                   vertexid/..., if so, this member will be set to SPECIAL_HW_IO_REG_NO */

                /* Used for passive mode */
                gctUINT                         hwRegNo;

                /* Used for active mode. Note this channel loc has different value for different io mem
                   alignment */
                gctUINT                         hwChannelLoc;
            } u;

            /* HW reg channel this IO channel is colored to. The basic rule of HW channel is total HW
               channel mask of hwRegNo must be 0001(x), 0011(xy), 0111(xyz), or 1111(xyzw) even though
               some sub-channels of them are inactive, which is the requirement of HW design since our
               HW can not support per-channel or channel shift register programming. Besides above basic
               rule, there are different requirements for different shader stages.
               1. VS input and PS output:

                  Since such IO is connected to memory of fixed function unit, we can not arbitarily
                  allocate HW channel, it must conform to data element layout/format (ioChannelMask
                  indicates full/partial of such layout) in memory of FFU unless driver changes data
                  element of FFU with new layout/format, but such changing is not necessary. For example,
                  if ioChannelMask is 0110(yz) or 0101(xz), HW reg channel mask must be 0111(xyz), i.e,
                  must start from x and hole retains, meanwhile, x is mapped to x, y is mapped to y, and
                  so on. But for masked out channels in ioChannelMask, hwRegChannel may be garbage.

                  If the IO is packed with others, as driver must merge multiple FFU data elements with
                  different layout/format to new data element of FFU with new layout/format, IO channel
                  can be mapped to HW channel arbitarily depending on where this channel will be merged.
                  For example, yz and XZ may be merged to XyZz or XyzZ. Driver needs use ioChannelMask
                  and hwRegChannel to analyze and generate such merged memory data element of FFU.

               2. I/O among shaders:
                  Whatever packed or not, as the element layout is fully handled by shader internally, IO
                  channel can be theoretically mapped to HW channel arbitarily as long as HW register
                  footprint are diminished. But current compiler implementation still uses same solution
                  as 1 for unpacked case.

                  When doing HW linkage, for packed case, in addition to do general check, such as usage
                  and usage index, or ioIndex, we also need check whether same channel of same IO is packed
                  into same HW channel.
            */
            gctUINT8                            hwRegChannel;
        } cmnHwLoc;

        /* ONLY valid when bHighPrecisionOnDual16 is TRUE. It designates hw-reg-no and hw-reg-channel for
           T1 hw-thread. */
        struct
        {
            /* For input,
                  1. a next (successive) hw-reg of cmnHwLoc.u.hwRegNo will be used if corresponding SHADER_IO_REG
                     has more than 2 channels used, or
                  2. hw-reg same as cmnHwLoc.u.hwRegNo will be used if corresponding SHADER_IO_REG has less-equal
                     2 channels used
               For output, any hw reg except cmnHwLoc.u.hwRegNo can be used */
            gctUINT                             hwRegNo;

            /* If uses an extra hw-reg against cmnHwLoc.u.hwRegNo, then same cmnHwLoc.hwRegChannel will be used,
               otherwise a different channel against cmnHwLoc.hwRegChannel will be used */
            gctUINT8                            hwRegChannel;
        } t1HwLoc;

    } hwLoc;
}
SHADER_IO_CHANNEL_MAPPING;

struct _SHADER_IO_REG_MAPPING
{
    SHADER_IO_CHANNEL_MAPPING                   ioChannelMapping[CHANNEL_NUM];

    /* It is the same as index of ioRegMapping which owns it */
    gctUINT                                     ioIndex;

    /* Which channels are active, corresponding to bActiveWithinShader */
    gctUINT                                     ioChannelMask;

    /* First valid channel based on io channel mask */
    gctUINT                                     firstValidIoChannel;

    /* Indicate which IOs are packed with this IO. Channels of all IOs other than
       SVs that are packed together share same hwRegNo, otherwise, each IO must be
       colored to different hwRegNo for non-SV channels */
    gctUINT64                                   packedIoIndexMask;

    /* Indicate which buffer this output reg will stream to, only for streamout. If
       it is valid, this output must be masked in soIoIndexMask and bStreamOutToBuffer
       of one of channels of this output reg must be TRUE */
    gctUINT                                     soStreamBufferSlot;

    /* When streaming to soStreamBufferSlot, what sequence index for this output reg */
    gctUINT                                     soSeqInStreamBuffer;

    /* Reg io-mode. NOTE that this reg io-mode might be different with io mode defined
       in SHADER_IO_MAPPING_PER_EXE_OBJ, see comments in that structure */
    SHADER_IO_MODE                              regIoMode;
};

typedef struct USAGE_2_IO
{
    /* Which Io index is used by this usage on usageindex 0 */
    gctUINT                                     mainIoIndex;

    /* Which channels are used by this usage on usageindex 0 */
    gctUINT                                     mainIoChannelMask;

    /* First valid channel based on channel mask on usageindex 0 */
    gctUINT                                     mainFirstValidIoChannel;

    /* Masked due to different usageIndex */
    gctUINT64                                   ioIndexMask;

    /* Mask of usageIndex */
    gctUINT                                     usageIndexMask;
}
USAGE_2_IO;

typedef struct SHADER_IO_MAPPING_PER_EXE_OBJ
{
    /* IO regs */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* Number of IO regs allocated for pIoRegMapping. Can not be greater than
       MAX_SHADER_IO_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfIoRegMapping;

    /* Indicate which IO regs are used */
    gctUINT64                                   ioIndexMask;

    /* Which IO regs are used by each usage */
    USAGE_2_IO                                  usage2IO[SHADER_IO_USAGE_TOTAL_COUNT];

    /* Pre-RA shader stages output only, for stream output */
    gctUINT64                                   soIoIndexMask;

    /* Io mode. Note that it is global io-mode for whole exe-obj, sometimes, some of
       special io-reg have different io-mode against this global io-mode, we need take
       that reg-io mode (ioRegMode) as precedence. This global mode is only use to do
       coarse check in hw linker. Note that if the case that global io mode is different
       with reg-io mode, this global io mode MUST be active mode!!! */
    SHADER_IO_MODE                              ioMode;

    /* Io memory alignment, only valid when ioMode is SHADER_IO_MODE_ACTIVE */
    SHADER_IO_MEM_ALIGN                         ioMemAlign;

    /* Io catetory for exe obj */
    SHADER_IO_CATEGORY                          ioCategory;
}
SHADER_IO_MAPPING_PER_EXE_OBJ;

typedef struct SHADER_IO_MAPPING
{
    /* Per vertex or pixel object IOs. For multiple vertex or pixel case, each vertex has same info, so we don't
       need record all elements of such object array. The size of this array is got from SHADER_EXECUTABLE_NATIVE_HINTS.
       Note that this object array can be indexed by per-object index. */
    SHADER_IO_MAPPING_PER_EXE_OBJ               ioVtxPxl;

    /* Per primitive object IOs which may includes multiple vertex or pixel object. Currently, it is only be used
       for HS/DS/GS shaders */
    SHADER_IO_MAPPING_PER_EXE_OBJ               ioPrim;
}
SHADER_IO_MAPPING;

/* Constant mapping table definitions, for c#/i#/b#/cb#/icb#

   They are used to update constant values
*/

typedef enum SHADER_CONSTANT_OFFSET_KIND
{
    SHADER_CONSTANT_OFFSET_IN_BYTE      = 0,
    SHADER_CONSTANT_OFFSET_IN_ARRAY     = 1,
} SHADER_CONSTANT_OFFSET_KIND;

typedef struct _SHADER_CONSTANT_HW_LOCATION_MAPPING SHADER_CONSTANT_HW_LOCATION_MAPPING;
struct _SHADER_CONSTANT_HW_LOCATION_MAPPING
{
    SHADER_HW_ACCESS_MODE                            hwAccessMode;

    /* Use a structure here because it might save the constant into the reigster and memomry at the same time. */
    struct
    {
        /* Case to map to constant register */
        struct
        {
            gctUINT                                  hwRegNo;
            gctUINT                                  hwRegRange;
        } constReg;

        /* Case to map to constant memory

           CAUTION for memory layout:
           1. For non-CTC spilled mem, currently, memory layout is designed to 4-tuples based as
              constant register,that means every element of scalar/vec2~4 array will be put into
              separated room of 4-tuples. With such layout design, we can support both DX1x cb#/icb#
              and GL named uniform block. So offset must be at 4-tuples alignment.

              But GL named uniform block has more loose layout requirement based on spec (for example,
              there could be no padding between any pair of GL type of data, but it is unfriendly
              for DX), so HW may design a different layout requirement for constant buffer, or directly
              use similar ld as GPGPU uses. If so, we should re-design followings and their users.

           2. For CTC spilled mem, first channel might be started at any location, even might be started
              at part of one channle(FP16/UINT16/INT16), so we can't use offset in array, we need
              to use offset in byte directly. And in this case, firstValidHwChannel must
              be started at X-channel which is located at constantOffset.
        */
        struct
        {
            SHADER_HW_MEM_ACCESS_MODE                hwMemAccessMode;

            union
            {
                /* For place-holder # case */
                gctUINT                              hwConstantArraySlot;

                /* For direct mem address case, if it is used for vulkan resources table maintained
                   by PEP, PEP generator will allocate/free it; if it is used for SEP, just pointing
                   to field of SHADER_CONSTANT_HW_LOCATION_MAPPING of one of entry of constant array
                   table. The pHwDirectAddrBase::hwAccessMode must be SHADER_HW_ACCESS_MODE_REGISTER */
                SHADER_CONSTANT_HW_LOCATION_MAPPING* pHwDirectAddrBase;

                /* For the case that constant array is mapped to SRV, if it is used for vulkan resources
                   table maintained by PEP, PEP generator will allocate/free it; if it is used for SEP,
                   just pointing to one of entry of SRV table */
                SHADER_RESOURCE_SLOT_MAPPING*        pSrv;

                /* For the case that constant array is mapped to UAV, if it is used for vulkan resources
                   table maintained by PEP, PEP generator will allocate/free it; if it is used for SEP,
                   just pointing to one of entry of UAV table */
                SHADER_UAV_SLOT_MAPPING*             pUav;
            } memBase;

            /* At channel boundary. See CAUTION!! */
            SHADER_CONSTANT_OFFSET_KIND              constantOffsetKind;
            gctUINT                                  constantOffset;
            gctUINT                                  componentSizeInByte;
        } memAddr;
    } hwLoc;

    /* Which channels of this HW location are valid for this 4-tuples constant */
    gctUINT                                          validHWChannelMask;

    /* First valid channel based on validHWChannelMask */
    gctUINT                                          firstValidHwChannel;
};

struct _SHADER_CONSTANT_SUB_ARRAY_MAPPING
{
    /* Start and size of sub array. 'startIdx' is the index within 'arrayRange' of parent */
    gctUINT                                     startIdx;
    gctUINT                                     subArrayRange;

    /* Only used by DX, for DX10+, it is the 2nd dimension of cb/icb, otherwise, it is 1st dimension */
    gctUINT                                     firstMSCSharpRegNo;

    /* Which channels are valid for this 4-tuples constant sub array. Note that mapping from validChannelMask
       to validHWChannelMask is not channel-based, so hole is supported. For example, 0011(xy) may be mapped
       to 0011(xy), 0110(yz) or 1100(zw), but 0101(xz) can only be mapped to 0111(xyz) or 1110(yzw) */
    gctUINT                                     validChannelMask;

    /* HW constant location for first element of this array.
       Note that all elements in each sub-array must have same channel mask designated by validChannelMask
       and validHWChannelMask respectively */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwFirstConstantLocation;
};

typedef struct SHADER_CONSTANT_ARRAY_MAPPING
{
    /* It is the same as index of pConstantArrayMapping which owns it */
    gctUINT                                     constantArrayIndex;

    SHADER_CONSTANT_USAGE                       constantUsage;

    /* Array size, including all sub-arrays it holds */
    gctUINT                                     arrayRange;

    /* For 2 purposes, we need split constant buffer into several subs
       1. Not all constant registers are used in constant buffer or a big uniform for OGL, so each
          used part can be put into a sub
       2. OGL may have many uniforms, each may be put into different sub if they can be put together

       So there is a possibility that several sub constant arrays share same startIdx and subArrayRange
       with different validChannelMask, also there is a possibility that several sub constant arrays
       share same HW reg/mem with different validHWChannelMask */
    SHADER_CONSTANT_SUB_ARRAY_MAPPING*          pSubConstantArrays;
    gctUINT                                     countOfSubConstantArray;
}
SHADER_CONSTANT_ARRAY_MAPPING;

struct _SHADER_COMPILE_TIME_CONSTANT
{
    gctUINT                                     constantValue[CHANNEL_NUM];

    /* HW constant location that this CTC maps to. Just use validHWChannelMask to indicate which channels
       have an immedidate CTC value */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwConstantLocation;
};

typedef struct SHADER_CONSTANT_MAPPING
{
    /* Constant buffer arrays */
    SHADER_CONSTANT_ARRAY_MAPPING*              pConstantArrayMapping;

    /* Number of constant buffer arrays allocated for pConstantArrayMapping. Can not be greater than
       MAX_SHADER_CONSTANT_ARRAY_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfConstantArrayMapping;

    /* Indicate which arrays are used */
    gctUINT                                     arrayIndexMask;

    /* Only used for DX9, no meaning for other clients */
    gctUINT                                     usage2ArrayIndex[SHADER_CONSTANT_USAGE_TOTAL_COUNT];

    /* Compiling-time immediate values */
    SHADER_COMPILE_TIME_CONSTANT*               pCompileTimeConstant;
    gctUINT                                     countOfCompileTimeConstant;

    /* HW constant register count that machine shader uses, i.e, only consider constants that have
       SHADER_HW_ACCESS_MODE_REGISTER access mode. It is equal to (max-used-const-regNo + 1). This
       is similiar as gprCount. */
    gctUINT                                     hwConstRegCount;

    /* Max HW constant register index that machine shader users, the default value is -1. */
    gctINT                                      maxHwConstRegIndex;
}
SHADER_CONSTANT_MAPPING;

/* Sampler mapping table definitions for s#

   They are used to update sampler state
*/

struct _SHADER_SAMPLER_SLOT_MAPPING
{
    /* It is the same as index of sampler which owns it */
    gctUINT                                     samplerSlotIndex;

    /* It does not apply to DX1x, and it will be removed after HW supports separated t# */
    SHADER_RESOURCE_DIMENSION                   samplerDimension;

    /* Only for OGL(ES). Return type by sample/ld inst, and it will be removed after HW
       supports separated t# */
    SHADER_RESOURCE_RETURN_TYPE                 samplerReturnType;

    /* Sampler mode, DX1x only */
    SHADER_SAMPLER_MODE                         samplerMode;

    /* HW slot number */
    gctUINT                                     hwSamplerSlot;
};

typedef struct SHADER_SAMPLER_MAPPING
{
    SHADER_SAMPLER_SLOT_MAPPING*                pSampler;

    /* Number of samplers allocated for pSampler. Can not be greater than
       MAX_SHADER_SAMPLER_NUM. It must be (maxUsed# + 1). Hole is permitted.
    */
    gctUINT                                     countOfSamplers;

    /* Indicate which samplers are used */
    gctUINT                                     samplerSlotMask;

    /* It does not apply to DX1x, and it will be removed after HW supports separated t# */
    gctUINT                                     dim2SamplerSlotMask[SHADER_RESOURCE_DIMENSION_TOTAL_COUNT];

    /* HW sampler register count that machine shader uses. */
    gctUINT                                     hwSamplerRegCount;

    /* Max HW sampler register index that machine shader users, the default value is -1. */
    gctINT                                      maxHwSamplerRegIndex;
}
SHADER_SAMPLER_MAPPING;

/* Shader resource mapping table definitions for t#

   They are used to update shader resource state
*/

struct _SHADER_RESOURCE_SLOT_MAPPING
{
    /* It is the same as index of resource which owns it */
    gctUINT                                     resourceSlotIndex;

    SHADER_RESOURCE_ACCESS_MODE                 accessMode;

    union
    {
        /* For type of access mode */
        struct
        {
            /* It only applies to DX1x */
            SHADER_RESOURCE_DIMENSION           resourceDimension;

            /* Resource return type by sample/ld inst */
            SHADER_RESOURCE_RETURN_TYPE         resourceReturnType;
        } s;

        /* For structured of access mode */
        gctUINT                                 structureSize;
    } u;

    /* HW slot number */
    gctUINT                                     hwResourceSlot;
};

typedef struct SHADER_RESOURCE_MAPPING
{
    SHADER_RESOURCE_SLOT_MAPPING*               pResource;

    /* Number of resources allocated for pResource. Can not be greater than
       MAX_SHADER_RESOURCE_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfResources;

    /* Indicate which resources are used */
    gctUINT                                     resourceSlotMask[4];

    /* It only applies to DX1x for typed access mode */
    gctUINT                                     dim2ResourceSlotMask[SHADER_RESOURCE_DIMENSION_TOTAL_COUNT][4];
}
SHADER_RESOURCE_MAPPING;

/* Global memory mapping table definitions for u#

   They are used to update global memory state, such as UAV
*/

struct _SHADER_UAV_SLOT_MAPPING
{
    /* It is the same as index of UAV which owns it */
    gctUINT                                     uavSlotIndex;

    SHADER_UAV_ACCESS_MODE                      accessMode;
    SHADER_HW_MEM_ACCESS_MODE                   hwMemAccessMode;

    /* There are two following reasons for this. The default is 0.
       1. For compiler internally generated UAVs (for patch for the most of cases), we need tell driver
          the total flattened UAV size, so driver can allocate it on vid-mem.
       2 .For sizable access mode, we need know the size of fixed size part.
    */
    gctUINT                                     sizeInByte;
    /* HW slot number, now only some inputAttachment uses this variable.  */
    gctUINT                                     hwSamplerSlot;

    union
    {
        /* For type of access mode */
        struct
        {
            /* It only applies to DX1x */
            SHADER_UAV_DIMENSION                uavDimension;

            /* UAV return type by ld/store inst */
            SHADER_UAV_TYPE                     uavType;
        } s;

        /* For resizable access mode */
        gctUINT                                 sizableEleSize;

        /* For structured of access mode */
        gctUINT                                 structureSize;
    } u;

    union
    {
        /* For place-holder # case */
        gctUINT                                 hwUavSlot;

        /* For direct mem address case, if it is used for vulkan resources table maintained
           by PEP, PEP generator will allocate/free it; if it is used for SEP, just pointing
           to field of SHADER_CONSTANT_HW_LOCATION_MAPPING of one of entry of constant array
           table. The pHwDirectAddrBase::hwAccessMode must be SHADER_HW_ACCESS_MODE_REGISTER */
        SHADER_CONSTANT_HW_LOCATION_MAPPING*    pHwDirectAddrBase;
    } hwLoc;
};

typedef struct SHADER_UAV_MAPPING
{
    SHADER_UAV_SLOT_MAPPING*                    pUAV;

    /* Number of UAVs allocated for pUAV. Can not be greater than
       MAX_SHADER_UAV_NUM. It must be (maxUsed# + 1). Hole is permitted. */
    gctUINT                                     countOfUAVs;

    /* Indicate which UAVs are used */
    gctUINT                                     uavSlotMask;

    /* It only applies to DX1x for typed access mode */
    gctUINT                                     dim2UavSlotMask[SHADER_UAV_DIMENSION_TOTAL_COUNT];
}
SHADER_UAV_MAPPING;

/* For these hints, they must be natively provided by original shader or shader owner, such as HS/DS/GS,
   there are special hints for them, such as vertex/pixel/CP (sub executable object) count and tessellation
   mode for patch primitive, etc */
typedef struct SHADER_EXECUTABLE_NATIVE_HINTS
{
    struct
    {
        /* DX ony. Precision consideration, it is a global flag. 'precise' is a local flag */
        gctUINT                                          bRefactorable    : 1;

        /* For GL(ES), it can be set TRUE or FALSE, for others, it must be set to TRUE */
        gctUINT                                          bSeparatedShader : 1;

        /* For GL(ES), it can be set TRUE or FALSE, for others, it must be set to FALSE */
        gctUINT                                          bLinkProgramPipeline : 1;

        /* What kind of memory access operations shader native holds, see SHADER_EDH_MEM_ACCESS_HINT */
        gctUINT                                          memoryAccessHint  : 6;

        gctUINT                                          flowControlHint   : 3;

        gctUINT                                          texldHint         : 1;

        /* Active cluster count, 4 bits should be enough. */
        gctUINT                                          activeClusterCount: 4;

        gctUINT                                          reserved          : 15;
    } globalStates;

    union
    {
        /* States acted on HS/DS. The primitive processing must be a patch.
           HS/DS share same states because part of them is put different shader for DX and GL.
        */
        struct
        {
            gctUINT                                      inputCtrlPointCount;

            /* HS only */
            gctBOOL                                      hasNoPerVertexInput;
            gctUINT                                      outputCtrlPointCount;

            /* For DX, they are provided in HS, but for OGL they are provided in DS */
            SHADER_TESSELLATOR_DOMAIN_TYPE               tessDomainType;
            SHADER_TESSELLATOR_PARTITION_TYPE            tessPartitionType;
            SHADER_TESSELLATOR_OUTPUT_PRIMITIVE_TOPOLOGY tessOutputPrim;
            gctUINT                                      maxTessFactor;
        } ts;

        /* States acted on GS */
        struct
        {
            gctUINT                                      maxOutputVtxCount;
            gctUINT                                      instanceCount;
            SHADER_GS_INPUT_PRIMITIVE_TOPOLOGY           inputPrim;
            SHADER_GS_OUTPUT_PRIMITIVE_TOPOLOGY          outputPrim;

            /* It is retrieved from inputPrim. Standalone providing this is just for convenience only */
            gctUINT                                      inputVtxCount;

            /* Whether the shader has any stream out other than stream 0. */
            gctBOOL                                      bHasStreamOut;
        } gs;

        /* States acted on PS */
        struct
        {
            /* OGL only */
            gctUINT                                      bEarlyPixelTestInRa : 1;

            gctUINT                                      reserved            : 31;
        } ps;

        /* States acted on gps */
        struct
        {
            gctUINT                                      shareMemSizePerThreadGrpInByte;
            gctUINT                                      currWorkGrpNum;

            gctUINT                                      privMemSizePerThreadInByte;
            gctUINT                                      currWorkThreadNum;

            gctUINT                                      workGroupNumPerShaderGroup;

            gctUINT                                      threadGrpDimX;
            gctUINT                                      threadGrpDimY;
            gctUINT                                      threadGrpDimZ;

            gctUINT                                      calculatedWorkGroupSize;
        } gps;
    } prvStates;
}
SHADER_EXECUTABLE_NATIVE_HINTS;

typedef enum UNIFIED_RF_ALLOC_STRATEGY
{
    /* For current shader type, the start offset and size is fix reserved in unified register file,
       so address offset will be set to that fixed offset. It is full same as allocating unnified
       RF because each shader type has its own space */
    UNIFIED_RF_ALLOC_STRATEGY_FIXED_ADDR_OFFSET                     = 0,

    /* For current shader type, the start offset and size is float (which means not reserved in unified
       register file). For all implementation, each used RF space will be packed together (that
       means this address offset is start from end of previous stage's RF resource */
    UNIFIED_RF_ALLOC_STRATEGY_PACK_FLOAT_ADDR_OFFSET                = 1,

    /* Pack all GPIPE stages together and put them in the top of resource, and put PS in the bottom. */
    UNIFIED_RF_ALLOC_STRATEGY_GPIPE_TOP_PS_BOTTOM_FLOAT_ADDR_OFFSET = 2,

    /* Pack all GPIPE stages together and put them in the bottom of resource, and put PS in the top. */
    UNIFIED_RF_ALLOC_STRATEGY_PS_TOP_GPIPE_BOTTOM_FLOAT_ADDR_OFFSET = 3,

    /* When HW provide unified register file (such as const/sampler), for non-seperated compiling
       for GL(ES), compiler may use different allocation strategy to allocate register in full scope
       of unified register file for a GL(ES) program. In this case, all shaders in this program
       should use same strategy. However, For other cases, such as DX/CL/seperated cases of GL(ES),
       they can not use UNIFIED_RF_ALLOC_STRATEGY_UNIFIED because there are no program concept.

       !!!NOTE that driver need assure it won not mix shaders belonging to different non-seperated
          programs to do programming. Otherwise, result is undefined!!!! */

    /* Allocated in full scope of unified register file, so address offset will be set to zero */
    UNIFIED_RF_ALLOC_STRATEGY_UNIFIED                               = 4,
}
UNIFIED_RF_ALLOC_STRATEGY;

/* Shader mem access hints for executable-derived-hints */
typedef enum SHADER_EDH_MEM_ACCESS_HINT
{
    SHADER_EDH_MEM_ACCESS_HINT_NONE               = 0x0000,
    SHADER_EDH_MEM_ACCESS_HINT_LOAD               = 0x0001,
    SHADER_EDH_MEM_ACCESS_HINT_STORE              = 0x0002,
    SHADER_EDH_MEM_ACCESS_HINT_IMG_READ           = 0x0004,
    SHADER_EDH_MEM_ACCESS_HINT_IMG_WRITE          = 0x0008,
    SHADER_EDH_MEM_ACCESS_HINT_ATOMIC             = 0x0010,

    SHADER_EDH_MEM_ACCESS_HINT_READ               = SHADER_EDH_MEM_ACCESS_HINT_LOAD       |
                                                    SHADER_EDH_MEM_ACCESS_HINT_IMG_READ   |
                                                    SHADER_EDH_MEM_ACCESS_HINT_ATOMIC,
    SHADER_EDH_MEM_ACCESS_HINT_WRITE              = SHADER_EDH_MEM_ACCESS_HINT_STORE      |
                                                    SHADER_EDH_MEM_ACCESS_HINT_IMG_WRITE  |
                                                    SHADER_EDH_MEM_ACCESS_HINT_ATOMIC,

    SHADER_EDH_MEM_ACCESS_HINT_BARRIER            = 0x0020,
    SHADER_EDH_MEM_ACCESS_HINT_EVIS_ATOMADD       = 0x0040, /* evis atomadd can operate on 16B data in parallel,
                                                             * we need to tell driver to turn off workgroup packing
                                                             * if it is used so the HW will not merge different
                                                             * workgroup into one which can cause the different
                                                             * address be used for the evis_atom_add */
/* Note: 1. must sync with VIR_MemoryAccessFlag !!!
 *       2. make sure it fits in bits in SHADER_EXECUTABLE_DERIVED_HINTS::memoryAccessHint */
}SHADER_EDH_MEM_ACCESS_HINT;

/* Shader flow control hints for executable-derived-hints */
typedef enum SHADER_EDH_FLOW_CONTROL_HINT
{
    SHADER_EDH_FLOW_CONTROL_HINT_NONE             = 0x0000,
    SHADER_EDH_FLOW_CONTROL_HINT_JMP              = 0x0001,
    SHADER_EDH_FLOW_CONTROL_HINT_CALL             = 0x0002,
    SHADER_EDH_FLOW_CONTROL_HINT_KILL             = 0x0004,
/* Note: 1. must sync with VIR_FlowControlFlag !!!
 *       2. make sure it fits in bits in SHADER_EXECUTABLE_DERIVED_HINTS::flowControlHint */
}SHADER_EDH_FLOW_CONTROL_HINT;

/* Shader texture hints for executable-derived-hints */
typedef enum SHADER_EDH_TEXLD_HINT
{
    SHADER_EDH_TEXLD_HINT_NONE             = 0x0000,
    SHADER_EDH_TEXLD_HINT_TEXLD            = 0x0001,
/* Note: 1. must sync with VIR_TexldFlag !!!
 *       2. make sure it fits in bits in SHADER_EXECUTABLE_DERIVED_HINTS::texldHint */
}SHADER_EDH_TEXLD_HINT;

/* For these hints, we can retrieve them by analyzing machine code on the fly, but it will
   hurt perf, so collect them by analyzing (derived) directly from compiler. */
typedef struct SHADER_EXECUTABLE_DERIVED_HINTS
{
    struct
    {
        /************************************/
        /* Followings are MUST global hints */
        /************************************/

        /* Shader will run on dual16 mode */
        gctUINT                   bExecuteOnDual16                : 1;

        /* Unified constant register file alloc strategy. For ununified RF, it has no mean */
        gctUINT                   unifiedConstRegAllocStrategy    : 3;

        /* Unified sampler register file alloc strategy. For ununified RF, it has no mean */
        gctUINT                   unifiedSamplerRegAllocStrategy  : 3;

        /* Whether the shader has GPR register spills */
        gctUINT                   bGprSpilled                     : 1;

        /* Whether the shader has constant register spills */
        gctUINT                   bCrSpilled                      : 1;

        /****************************************/
        /* Followings are OPTIONAL global hints */
        /****************************************/

        /* What kind of memory access operations shader holds, see SHADER_EDH_MEM_ACCESS_HINT */
        gctUINT                   memoryAccessHint                : 8;

        /* What kind of flow control operations shader holds, see SHADER_EDH_FLOW_CONTROL_HINT */
        gctUINT                   flowControlHint                 : 3;

        /* What kind of texld operations shader holds, see SHADER_EDH_TEXLD_HINT */
        gctUINT                   texldHint                       : 1;

        /* First HW reg and its channel that will be used to store addresses
           of USC for each vertex when executing hs/ds/gs. */
        gctUINT                   hwStartRegNoForUSCAddrs         : 4;
        gctUINT                   hwStartRegChannelForUSCAddrs    : 2;

        /* Address in USC for input and output vertex/CP are packed into one
           HW reg, NOTE that it is only used when vertex/CP count of input and
           output are all LE 4 */
        gctUINT                   bIoUSCAddrsPackedToOneReg       : 1;

        /* Whether enable multi-GPU. */
        gctUINT                   bEnableMultiGPU                 : 1;

        /* Whether enable robust out-of-bounds check for memory access . */
        gctUINT                   bEnableRobustCheck              : 1;

        gctUINT                   reserved                        : 2;

        gctUINT                   gprSpillSize;  /* the byte count of register spill mem to be
                                                  * allocated by driver in MultiGPU mode*/
    } globalStates;

    struct
    {
        /* Z-channel of output position of pre-RA is dependent on W-channel of that position. This is
           a special hint to do RA wclip SW patch */
        gctUINT                   bOutPosZDepW                    : 1;

        gctUINT                   reserved                        : 31;
    } prePaStates;

    union
    {
        /* States acted on HS */
        struct
        {
            /* Whether data of one of output CP are accessed by other output CP threads */
            gctUINT               bPerCpOutputsUsedByOtherThreads : 1;

            gctUINT               reserved                        : 31;
        } hs;

        /* States acted on GS */
        struct
        {
            /* Shader has explicit restart operation */
            gctUINT               bHasPrimRestartOp               : 1;

            gctUINT               reserved                        : 31;
        } gs;

        /* States acted on PS */
        struct
        {
            /* To determine which io-index of output mapping have alpha write */
            gctUINT               alphaWriteOutputIndexMask       : 8;

            /* Shader has operation to calc gradient on x/y of RT */
            gctUINT               bDerivRTx                       : 1;
            gctUINT               bDerivRTy                       : 1;
            /* shader has dsy IR before lowering to machine code, so it
             * wouldn't count fwidth() as using DSY for yInvert purpose */
            gctUINT               bDsyBeforeLowering              : 1;

            /* Shader has operation to discard pixel (such as texkill/discard) */
            gctUINT               bPxlDiscard                     : 1;

            /* Under per-sample freq shading, sample-mask-in and sample_index are
               put in a channel of special reg number by RA, and sample-mask-out
               will also be written into this channel (bit[0~3]) */
            gctUINT               hwRegNoForSampleMaskId          : 9;
            gctUINT               hwRegChannelForSampleMaskId     : 2;

            /* PS reg start index, exclude #position. */
            gctUINT               psStartRegIndex                 : 2;

            /* Shader will run on per-sample frequency */
            gctUINT               bExecuteOnSampleFreq            : 1;

            /* Position and point-coord per-channel valid info. Note we can not use
               SHADER_IO_REG_MAPPING to check this because it is high-level info, not
               low-level (HW) info. Directly using SHADER_IO_REG_MAPPIN may get wrong
               result if high-level channel maps a HW channel that is differnt with
               high-level channel */
            gctUINT               inputPosChannelValid            : 4;
            gctUINT               inputPntCoordChannelValid       : 2;

            /* To determine whether shader needs read RT data (for example, shader
               implements alpha-blend, or for OGL, lastFragData is presented) */
            gctUINT               bNeedRtRead                     : 1;

            gctUINT               alphaClrKillInstsGened          : 1;
            gctUINT               fragColorUsage                  : 2;

            gctUINT               reserved                        : 28;
        } ps;

        /* States acted on gps */
        struct
        {
            /* Whether whole thread group needs sync */
            gctUINT               bThreadGroupSync                : 1;

            /* Whether use Local memory. */
            gctUINT               bUseLocalMemory                 : 1;

            /* Whether use Private memory. */
            gctUINT               bUsePrivateMemory               : 1;

            /* Whether use Evis instruction. */
            gctUINT               bUseEvisInst                    : 1;

            /* Whether the shader depends on the workGroupSize. */
            gctUINT               bDependOnWorkGroupSize          : 1;

            gctUINT               reserved                        : 27;

            gctUINT16             workGroupSizeFactor[3];
        } gps;
    } prvStates;
}SHADER_EXECUTABLE_DERIVED_HINTS;

typedef struct SHADER_EXECUTABLE_HINTS
{
    SHADER_EXECUTABLE_NATIVE_HINTS          nativeHints;
    SHADER_EXECUTABLE_DERIVED_HINTS         derivedHints;
}SHADER_EXECUTABLE_HINTS;

struct SHADER_EXECUTABLE_INSTANCE;

/* Executable shader profile definition. Each BE compiling or glProgramBinary will generate one
   profile like this. */
typedef struct SHADER_EXECUTABLE_PROFILE
{
    /* Profile version */
    gctUINT                                     profileVersion;

    /* Target HW this executable can run on */
    gctUINT                                     chipModel;
    gctUINT                                     chipRevision;
    gctUINT                                     productID;
    gctUINT                                     customerID;

    /* From MSB to LSB, 8-bits client + 8-bits type + 8-bits majorVersion + 8-bits minorVersion */
    gctUINT                                     shVersionType;

    /* Compiled machine code. Note that current countOfMCInst must be 4-times of DWORD since HW
       inst is 128-bits wide. It may be changed later for future chip */
    gctUINT*                                    pMachineCode;
    gctUINT                                     countOfMCInst;

    /* EndPC of main routine since HW will use it to terminate shader execution. The whole machine
       code is organized as main routine followed by all sub-routines */
    gctUINT                                     endPCOfMainRoutine;

    /* Temp register count the machine code uses */
    gctUINT                                     gprCount;

    /* Special executable hints that this SEP holds. When shader is executed inside of HW, it will
       rely on this special execute hints */
    SHADER_EXECUTABLE_HINTS                     exeHints;

    /* Low level mapping tables (mapping pool) from # to HW resource. Add new mapping tables here,
       for example, function table for DX11+ */
    SHADER_IO_MAPPING                           inputMapping;
    SHADER_IO_MAPPING                           outputMapping;
    SHADER_CONSTANT_MAPPING                     constantMapping;
    SHADER_SAMPLER_MAPPING                      samplerMapping;
    SHADER_RESOURCE_MAPPING                     resourceMapping;
    SHADER_UAV_MAPPING                          uavMapping;

    /* All private mapping tables for static patches. Every entry has a member pointing to a slot in
       above mapping pool */
    SHADER_STATIC_PRIV_MAPPING                  staticPrivMapping;

    /* All private mapping tables for dynamic (lib-link) patches. Every entry has a member pointing
       to a slot in above mapping pool */
    SHADER_DYNAMIC_PRIV_MAPPING                 dynamicPrivMapping;

    SHADER_DEFAULT_UBO_MAPPING                  defaultUboMapping;

    /* Current SEI that this profile uses. This is the one used to program HW registers. Currently disable it
       due to we're using program-level recompiling */
    /*SHADER_EXECUTABLE_INSTANCE*                 pCurInstance;*/
}
SHADER_EXECUTABLE_PROFILE;

gceSTATUS vscInitializeSEP(SHADER_EXECUTABLE_PROFILE* pSEP);
gceSTATUS vscFinalizeSEP(SHADER_EXECUTABLE_PROFILE* pSEP);
gctBOOL vscIsValidSEP(SHADER_EXECUTABLE_PROFILE* pSEP);

gceSTATUS vscInitializeIoRegMapping(SHADER_IO_REG_MAPPING* pIoRegMapping);
gceSTATUS vscFinalizeIoRegMapping(SHADER_IO_REG_MAPPING* pIoRegMapping);

gceSTATUS vscInitializeCnstHwLocMapping(SHADER_CONSTANT_HW_LOCATION_MAPPING* pCnstHwLocMapping);
gceSTATUS vscFinalizeCnstHwLocMapping(SHADER_CONSTANT_HW_LOCATION_MAPPING* pCnstHwLocMapping);

gceSTATUS vscInitializeCTC(SHADER_COMPILE_TIME_CONSTANT* pCompileTimeConstant);
gceSTATUS vscFinalizeCTC(SHADER_COMPILE_TIME_CONSTANT* pCompileTimeConstant);

gceSTATUS vscInitializeCnstArrayMapping(SHADER_CONSTANT_ARRAY_MAPPING* pCnstArrayMapping);
gceSTATUS vscFinalizeCnstArrayMapping(SHADER_CONSTANT_ARRAY_MAPPING* pCnstArrayMapping);

gceSTATUS vscInitializeCnstSubArrayMapping(SHADER_CONSTANT_SUB_ARRAY_MAPPING* pCnstSubArrayMapping);
gceSTATUS vscFinalizeCnstSubArrayMapping(SHADER_CONSTANT_SUB_ARRAY_MAPPING* pCnstSubArrayMapping);

gceSTATUS vscInitializeSamplerSlotMapping(SHADER_SAMPLER_SLOT_MAPPING* pSamplerSlotMapping);
gceSTATUS vscFinalizeSamplerSlotMapping(SHADER_SAMPLER_SLOT_MAPPING* pSamplerSlotMapping);

gceSTATUS vscInitializeUavSlotMapping(SHADER_UAV_SLOT_MAPPING* pUavSlotMapping);
gceSTATUS vscFinalizeUavSlotMapping(SHADER_UAV_SLOT_MAPPING* pUavSlotMapping);

void vscSortIOsByHwLoc(SHADER_IO_MAPPING_PER_EXE_OBJ* pIoMappingPerExeObj, gctUINT* pSortedIoIdxArray);

/* If hShader != NULL, mapping 'symbol->#->hw resource' is dumped, otherwise
   only '#->hw' is dumped. For the 2nd case, it is easy for driver to dump any
   SEP when flushing to hw to triage bugs */
gctBOOL vscPrintSEP(VSC_SYS_CONTEXT* pSysCtx, SHADER_EXECUTABLE_PROFILE* pSEP, SHADER_HANDLE hShader);

/* Linkage info */
typedef struct SHADER_IO_REG_LINKAGE
{
    /* This channel have link by other stage, for linkage */
    gctBOOL                                     bLinkedByOtherStageX : 2;
    gctBOOL                                     bLinkedByOtherStageY : 2;
    gctBOOL                                     bLinkedByOtherStageZ : 2;
    gctBOOL                                     bLinkedByOtherStageW : 2;

    /* This output is only linked to FFU SO */
    gctBOOL                                     bOnlyLinkToSO : 2;

    /* This is dummy link, which means it will not be consumed by anyone. It is only used for active-mode IO */
    gctBOOL                                     bIsDummyLink  : 2;

    /* Used to link to other stage */
    gctUINT                                     linkNo;
}SHADER_IO_REG_LINKAGE;

typedef struct SHADER_IO_LINKAGE_INFO_PER_EXE_OBJ
{
    SHADER_IO_REG_LINKAGE                       ioRegLinkage[MAX_SHADER_IO_NUM];
    gctUINT                                     totalLinkNoCount;
}SHADER_IO_LINKAGE_INFO_PER_EXE_OBJ;

typedef struct SHADER_IO_LINKAGE_INFO
{
    SHADER_IO_LINKAGE_INFO_PER_EXE_OBJ          vtxPxlLinkage;
    SHADER_IO_LINKAGE_INFO_PER_EXE_OBJ          primLinkage;

    /* When linking with other shader, which type of shader is linked to */
    SHADER_TYPE                                 linkedShaderStage;
}SHADER_IO_LINKAGE_INFO;

typedef enum HW_INST_FETCH_MODE
{
    /* Fetched from non-unified inst buffer, using 0x1000 and 0x1800 */
    HW_INST_FETCH_MODE_UNUNIFIED_BUFFER        = 0,

    /* Fetched from unified inst buffer 0, using 0x3000 and 0x2000 */
    HW_INST_FETCH_MODE_UNIFIED_BUFFER_0        = 1,

    /* Fetched from unified inst buffer 1, using 0x8000 */
    HW_INST_FETCH_MODE_UNIFIED_BUFFER_1        = 2,

    /* Fetched from I$ */
    HW_INST_FETCH_MODE_CACHE                   = 3,
}HW_INST_FETCH_MODE;

typedef enum HW_CONSTANT_FETCH_MODE
{
    /* Fetched from non-unified constant RF, using 0x1400 and 0x1C00 */
    HW_CONSTANT_FETCH_MODE_UNUNIFIED_REG_FILE  = 0,

    /* Fetched from unified constant RF, using 0xC000 */
    HW_CONSTANT_FETCH_MODE_UNIFIED_REG_FILE    = 1,
}HW_CONSTANT_FETCH_MODE;

typedef enum HW_SAMPLER_FETCH_MODE
{
    /* Fetched from non-unified sampler RF */
    HW_SAMPLER_FETCH_MODE_UNUNIFIED_REG_FILE   = 0,

    /* Fetched from unified constant RF */
    HW_SAMPLER_FETCH_MODE_UNIFIED_REG_FILE     = 1,
}HW_SAMPLER_FETCH_MODE;

typedef struct SHADER_HW_PROGRAMMING_HINTS
{
    /* Word 1*/
    /* Inst fetch mode */
    gctUINT                                     hwInstFetchMode               : 2;

    /* For HW_INST_FETCH_MODE_UNIFIED_BUFFER_0 and HW_INST_FETCH_MODE_UNIFIED_BUFFER_1
       it can be non-zero; for HW_INST_FETCH_MODE_UNUNIFIED_BUFFER, must be set to 0 */
    gctUINT                                     hwInstBufferAddrOffset        : 12;

    /* Constant fetch mode */
    gctUINT                                     hwConstantFetchMode           : 1;

    /* When constant registers are not allocated as unified, it can be set to no-zero for
       HW_CONSTANT_FETCH_MODE_UNIFIED_REG_FILE, for other cases, it must be set to 0 */
    gctUINT                                     hwConstantRegAddrOffset       : 9;

    /* Sampler fetch mode */
    gctUINT                                     hwSamplerFetchMode            : 1;

    /* When sampler registers are not allocated as unified, it can be set to no-zero for
       HW_SAMPLER_FETCH_MODE_UNIFIED_REG_FILE, for other cases, it must be set to 0 */
    gctUINT                                     hwSamplerRegAddrOffset        : 7;

    /* Word 2*/
    /* Result-cache is used to queue missed data streamming from up-stage and release them
       after they are used. This window-size is the size of queue. Note that this result-$
       occupies some space of USC storage (uscSizeInKbyte). The ocuppied space is calc'ed
       by (resultCacheWindowSize * outputSizePerThread) */
    gctUINT                                     resultCacheWindowSize         : 9;

    /* GS only. Each hw thread-group owns meta data for current TG to save counter/restart/
       stream-index info. Note that it also occupies space of USC storage (uscSizeInKbyte). */
    gctUINT                                     gsMetaDataSizePerHwTGInBtye   : 16;

    /* Max really runnable thread count per HW thread-group, it can not be greater than
       maxHwTGThreadCount, i.e, (maxCoreCount * 4) */
    gctUINT                                     maxThreadsPerHwTG             : 7;

    /* Word 3*/
    /* USC is shared by all shader stages, so we need allocate proper size for each stage
       to get best perf of pipeline. The relation between these two members are
       1. minUscSizeInKbyte can not be greater than maxUscSizeInKbyte.
       2. If the minUscSizeInKbyte is equal to maxUscSizeInKbyte, HW will do static allocation
          within shader type (client) specified maxUscSizeInKbyte USC for this shader stage.
       3. If the minUscSizeInKbyte is less than maxUscSizeInKbyte HW will do dynamic allocation
          on total extra-sizes (each extra size is maxUscSizeInKbyte-minUscSizeInKbyte) for each
          shader stage by insuring the min USC size requirement, */
    gctUINT                                     maxUscSizeInKbyte             : 8;
    gctUINT                                     minUscSizeInKbyte             : 8;

    /* Iteration factor to time 'min parallel shader stage combination' when analyzing USC */
    gctUINT                                     maxParallelFactor             : 16;

    /* Word 4*/
    /* Max patches number. */
    gctUINT                                     tsMaxPatches                  : 7;

    /* Reserved bits */
    gctUINT                                     reserved                      : 25;
}SHADER_HW_PROGRAMMING_HINTS;

typedef struct SHADER_HW_INFO
{
    SHADER_EXECUTABLE_PROFILE*                  pSEP;

    /* Shader linkage info, can be changed by linker */
    SHADER_IO_LINKAGE_INFO                      inputLinkageInfo;
    SHADER_IO_LINKAGE_INFO                      outputLinkageInfo;

    /* Machine code flush hint */
    SHADER_HW_PROGRAMMING_HINTS                 hwProgrammingHints;
}SHADER_HW_INFO;

gceSTATUS vscInitializeShaderHWInfo(SHADER_HW_INFO* pShaderHwInfo, SHADER_EXECUTABLE_PROFILE* pSEP);
gceSTATUS vscFinalizeShaderHWInfo(SHADER_HW_INFO* pShaderHwInfo);

END_EXTERN_C();

#endif /* __gc_vsc_drvi_shader_profile_h_ */



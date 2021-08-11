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


#ifndef __gc_vsc_drvi_shader_priv_mapping_h_
#define __gc_vsc_drvi_shader_priv_mapping_h_

BEGIN_EXTERN_C()

/* Forward declarations */
typedef struct _SHADER_CONSTANT_SUB_ARRAY_MAPPING SHADER_CONSTANT_SUB_ARRAY_MAPPING;
typedef struct _SHADER_COMPILE_TIME_CONSTANT      SHADER_COMPILE_TIME_CONSTANT;
typedef struct _SHADER_UAV_SLOT_MAPPING           SHADER_UAV_SLOT_MAPPING;
typedef struct _SHADER_RESOURCE_SLOT_MAPPING      SHADER_RESOURCE_SLOT_MAPPING;
typedef struct _SHADER_SAMPLER_SLOT_MAPPING       SHADER_SAMPLER_SLOT_MAPPING;
typedef struct _SHADER_IO_REG_MAPPING             SHADER_IO_REG_MAPPING;

/* Shader constant static priv-mapping kind */
typedef enum SHS_PRIV_CONSTANT_KIND
{
    SHS_PRIV_CONSTANT_KIND_COMPUTE_GROUP_NUM                = 0,
    SHS_PRIV_CONSTANT_KIND_IMAGE_SIZE                       = 1,
    SHS_PRIV_CONSTANT_KIND_TEXTURE_SIZE                     = 2,
    SHS_PRIV_CONSTANT_KIND_LOD_MIN_MAX                      = 3,
    SHS_PRIV_CONSTANT_KIND_LEVELS_SAMPLES                   = 4,
    SHS_PRIV_CONSTANT_KIND_BASE_INSTANCE                    = 5,
    SHS_PRIV_CONSTANT_KIND_SAMPLE_LOCATION                  = 6,
    SHS_PRIV_CONSTANT_KIND_ENABLE_MULTISAMPLE_BUFFERS       = 7,
    SHS_PRIV_CONSTANT_KIND_LOCAL_ADDRESS_SPACE              = 8,
    SHS_PRIV_CONSTANT_KIND_PRIVATE_ADDRESS_SPACE            = 9,
    SHS_PRIV_CONSTANT_KIND_CONSTANT_ADDRESS_SPACE           = 10,
    SHS_PRIV_CONSTANT_KIND_GLOBAL_SIZE                      = 11,
    SHS_PRIV_CONSTANT_KIND_LOCAL_SIZE                       = 12,
    SHS_PRIV_CONSTANT_KIND_GLOBAL_OFFSET                    = 13,
    SHS_PRIV_CONSTANT_KIND_WORK_DIM                         = 14,
    SHS_PRIV_CONSTANT_KIND_PRINTF_ADDRESS                   = 15,
    SHS_PRIV_CONSTANT_KIND_WORKITEM_PRINTF_BUFFER_SIZE      = 16,
    SHS_PRIV_CONSTANT_KIND_WORK_THREAD_COUNT                = 17,
    SHS_PRIV_CONSTANT_KIND_WORK_GROUP_COUNT                 = 18,
    SHS_PRIV_CONSTANT_KIND_LOCAL_MEM_SIZE                   = 19,
    SHS_PRIV_CONSTANT_KIND_WORK_GROUP_ID_OFFSET             = 20,
    SHS_PRIV_CONSTANT_KIND_GLOBAL_INVOCATION_ID_OFFSET      = 21,
    SHS_PRIV_CONSTANT_KIND_TEMP_REG_SPILL_MEM_ADDRESS       = 22,
    SHS_PRIV_CONSTANT_KIND_GLOBAL_WORK_SCALE                = 23,
    SHS_PRIV_CONSTANT_KIND_DATA_BIT_SIZE                    = 24,
    SHS_PRIV_CONSTANT_KIND_COMPUTE_GROUP_NUM_FOR_SINGLE_GPU = 25,
    SHS_PRIV_CONSTANT_KIND_VIEW_INDEX                       = 26,
    SHS_PRIV_CONSTANT_KIND_DEFAULT_UBO_ADDRESS              = 27,
    SHS_PRIV_CONSTANT_KIND_THREAD_ID_MEM_ADDR               = 28,
    SHS_PRIV_CONSTANT_KIND_ENQUEUED_LOCAL_SIZE              = 29,
    SHS_PRIV_CONSTANT_KIND_COUNT, /* last member, add new kind beofre this */
}SHS_PRIV_CONSTANT_KIND;

/* Shader mem static priv-mapping kind */
typedef enum SHS_PRIV_MEM_KIND
{
    SHS_PRIV_MEM_KIND_NONE                      = 0,
    SHS_PRIV_MEM_KIND_GPR_SPILLED_MEMORY        = 1, /* For gpr register spillage */
    SHS_PRIV_MEM_KIND_CONSTANT_SPILLED_MEMORY   = 2, /* For constant register spillage */
    SHS_PRIV_MEM_KIND_STREAMOUT_BY_STORE        = 3, /* Stream buffer for SO */
    SHS_PRIV_MEM_KIND_CL_PRIVATE_MEMORY         = 4, /* For CL private mem */
    SHS_PRIV_MEM_KIND_SHARED_MEMORY             = 5, /* For CL local memory or DirectCompute shared mem */
    SHS_PRIV_MEM_KIND_EXTRA_UAV_LAYER           = 6,
    SHS_PRIV_MEM_KIND_THREAD_ID_MEM_ADDR        = 7, /* The global memory to save the consecutive thread ID. */
    SHS_PRIV_MEM_KIND_YCBCR_PLANE               = 8, /* The YCBCR plane. */
    SHS_PRIV_MEM_KIND_COUNT                     = 9,
}SHS_PRIV_MEM_KIND;

/* !!!!!NOTE: For dynamic (lib-link) patch, the priv-mapping flag will directly use VSC_LIB_LINK_TYPE!!!!! */

/* Definition for common entry of shader priv mapping . */
typedef struct SHADER_PRIV_MAPPING_COMMON_ENTRY
{
    /* Each priv-mapping kind */
    gctUINT                                     privmKind;

    /* For some kinds, there might be multiple mapping, so flag-index to distinguish them.
       It is numbered from zero. */
    gctUINT                                     privmKindIndex;

    /* For some flags, they will have their private data to tell driver how to do. */
    gctBOOL                                     notAllocated;
    void*                                       pPrivateData;
}
SHADER_PRIV_MAPPING_COMMON_ENTRY;

/*
  Definition of shader constants priv-mapping
*/

typedef enum SHADER_PRIV_CONSTANT_MODE
{
    SHADER_PRIV_CONSTANT_MODE_CTC           = 0,
    SHADER_PRIV_CONSTANT_MODE_VAL_2_INST    = 1,
    SHADER_PRIV_CONSTANT_MODE_VAL_2_MEMORREG= 2,
    SHADER_PRIV_CONSTANT_MODE_VAL_2_DUBO    = 3,
}
SHADER_PRIV_CONSTANT_MODE;

typedef struct SHADER_PRIV_CONSTANT_INST_IMM
{
    gctUINT                                     patchedPC;
    gctUINT                                     srcNo;
}
SHADER_PRIV_CONSTANT_INST_IMM;

typedef struct SHADER_PRIV_CONSTANT_CTC
{
    SHADER_COMPILE_TIME_CONSTANT*               pCTC;
    gctUINT                                     hwChannelMask;
}
SHADER_PRIV_CONSTANT_CTC;

typedef struct SHADER_PRIV_CONSTANT_ENTRY
{
    SHADER_PRIV_MAPPING_COMMON_ENTRY            commonPrivm;

    /* Which mode that driver use to flush constant */
    SHADER_PRIV_CONSTANT_MODE                   mode;

    union
    {
        SHADER_CONSTANT_SUB_ARRAY_MAPPING*      pSubCBMapping; /* SHADER_PRIV_CONSTANT_MODE_VAL_2_MEMORREG */
        SHADER_PRIV_CONSTANT_CTC                ctcConstant;   /* SHADER_PRIV_CONSTANT_MODE_CTC */
        SHADER_PRIV_CONSTANT_INST_IMM           instImm;       /* SHADER_PRIV_CONSTANT_MODE_VAL_2_INST */
        gctUINT                                 duboEntryIndex;/* SHADER_PRIV_CONSTANT_MODE_VAL_2_DUBO */
    } u;
}
SHADER_PRIV_CONSTANT_ENTRY;

typedef struct SHADER_PRIV_CONSTANT_MAPPING
{
    SHADER_PRIV_CONSTANT_ENTRY*                 pPrivmConstantEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PRIV_CONSTANT_MAPPING;

/*
  Definition of shader uav priv-mapping
*/

typedef struct SHADER_PRIV_MEM_DATA_MAPPING
{
    SHADER_COMPILE_TIME_CONSTANT**              ppCTC;
    gctUINT                                     ctcCount;

    SHADER_CONSTANT_SUB_ARRAY_MAPPING**         ppCnstSubArray;
    gctUINT                                     cnstSubArrayCount;
}SHADER_PRIV_MEM_DATA_MAPPING;

typedef struct SHADER_PRIV_UAV_ENTRY
{
    gctUINT                                     uavEntryIndex;

    SHADER_PRIV_MAPPING_COMMON_ENTRY            commonPrivm;

    /* The data which will be set to this memory */
    SHADER_PRIV_MEM_DATA_MAPPING                memData;

    SHADER_UAV_SLOT_MAPPING*                    pBuffer;
}
SHADER_PRIV_UAV_ENTRY;

typedef struct SHADER_PRIV_UAV_MAPPING
{
    SHADER_PRIV_UAV_ENTRY*                      pPrivUavEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PRIV_UAV_MAPPING;

/*
  Definition of shader resource priv-mapping
*/

typedef struct SHADER_PRIV_RESOURCE_ENTRY
{
    SHADER_PRIV_MAPPING_COMMON_ENTRY            commonPrivm;
    SHADER_RESOURCE_SLOT_MAPPING*               pSrv;
}
SHADER_PRIV_RESOURCE_ENTRY;

typedef struct SHADER_PRIV_RESOURCE_MAPPING
{
    SHADER_PRIV_RESOURCE_ENTRY*                 pPrivResourceEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PRIV_RESOURCE_MAPPING;

/*
  Definition of shader sampler priv-mapping
*/

typedef struct SHADER_PRIV_SAMPLER_ENTRY
{
    SHADER_PRIV_MAPPING_COMMON_ENTRY            commonPrivm;
    SHADER_SAMPLER_SLOT_MAPPING*                pSampler;
}
SHADER_PRIV_SAMPLER_ENTRY;

typedef struct SHADER_PRIV_SAMPLER_MAPPING
{
    SHADER_PRIV_SAMPLER_ENTRY*                  pPrivSamplerEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PRIV_SAMPLER_MAPPING;

/*
  Definition of shader output priv-mapping
*/

typedef struct SHADER_PRIV_OUTPUT_ENTRY
{
    SHADER_PRIV_MAPPING_COMMON_ENTRY            commonPrivm;
    SHADER_IO_REG_MAPPING*                      pOutput;
}
SHADER_PRIV_OUTPUT_ENTRY;

typedef struct SHADER_PRIV_OUTPUT_MAPPING
{
    SHADER_PRIV_OUTPUT_ENTRY*                   pPrivOutputEntries;
    gctUINT                                     countOfEntries;
}
SHADER_PRIV_OUTPUT_MAPPING;

/* Static private mapping table */
typedef struct SHADER_STATIC_PRIV_MAPPING
{
     SHADER_PRIV_CONSTANT_MAPPING               privConstantMapping;
     SHADER_PRIV_UAV_MAPPING                    privUavMapping;
}SHADER_STATIC_PRIV_MAPPING;

/* Dynamic private mapping table */
typedef struct SHADER_DYNAMIC_PRIV_MAPPING
{
     SHADER_PRIV_SAMPLER_MAPPING                privSamplerMapping;
     SHADER_PRIV_OUTPUT_MAPPING                 privOutputMapping;
}SHADER_DYNAMIC_PRIV_MAPPING;

/* Default UBO mapping. */
typedef enum SHS_DEFAULT_UBO_MEMBER_KIND
{
    SHS_DEFAULT_UBO_MEMBER_PRIV_CONST           = 0,
}SHS_DEFAULT_UBO_MEMBER_KIND;

typedef struct SHADER_DEFAULT_UBO_MEMBER_ENTRY
{
    gctUINT                                     memberIndexInOtherEntryTable;
    SHS_DEFAULT_UBO_MEMBER_KIND                 memberKind;
    gctUINT                                     offsetInByte;
}SHADER_DEFAULT_UBO_MEMBER_ENTRY;

typedef struct SHADER_DEFAULT_UBO_MAPPING
{
    gctUINT                                     baseAddressIndexInPrivConstTable;
    SHADER_DEFAULT_UBO_MEMBER_ENTRY*            pDefaultUboMemberEntries;
    gctUINT                                     countOfEntries;
    gctUINT                                     sizeInByte;
}SHADER_DEFAULT_UBO_MAPPING;

END_EXTERN_C();

#endif /* __gc_vsc_drvi_shader_priv_mapping_h_ */


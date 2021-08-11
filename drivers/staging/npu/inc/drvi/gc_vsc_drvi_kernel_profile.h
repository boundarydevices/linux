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


/*********************************************************************************
   Following definitions are ONLY used for openCL(ES) to communicate with client
   query/binding request from kernel. NOTE that ONLY active parts are collected
   into each high level table.
**********************************************************************************/

#ifndef __gc_vsc_drvi_kernel_profile_h_
#define __gc_vsc_drvi_kernel_profile_h_

BEGIN_EXTERN_C()

#define VIR_CL_INVALID_ARG_INDEX               0xffffffff

typedef struct PROG_CL_UNIFORM_COMMON_ENTRY
{
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;
    gctBOOL                                     pointer;

    SHADER_CONSTANT_SUB_ARRAY_MAPPING *         hwMapping;
    /*SHADER_CONSTANT_ARRAY_MAPPING *             hwMapping;*/
}
PROG_CL_UNIFORM_COMMON_ENTRY;

typedef struct PROG_CL_UNIFORM_TABLE_ENTRY
{
    gctUINT                                     argIndex;

    PROG_CL_UNIFORM_COMMON_ENTRY                common;
}
PROG_CL_UNIFORM_TABLE_ENTRY;

typedef struct PROG_CL_UNIFORM_TABLE
{
    PROG_CL_UNIFORM_TABLE_ENTRY*                pUniformEntries;
    gctUINT                                     countOfEntries;
}
PROG_CL_UNIFORM_TABLE;

typedef struct PROG_CL_PRV_UNIFORM_COMMON_ENTRY
{
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;

    SHADER_PRIV_CONSTANT_ENTRY *                hwMapping;
}
PROG_CL_PRV_UNIFORM_COMMON_ENTRY;

typedef struct PROG_CL_PRV_UNIFORM_TABLE
{
    PROG_CL_PRV_UNIFORM_COMMON_ENTRY*           pUniformEntries;
    gctUINT                                     countOfEntries;
}
PROG_CL_PRV_UNIFORM_TABLE;

typedef struct PROG_CL_COMBINED_TEXTURE_SAMPLER_HW_MAPPING
{
    /* For the case
       1. HW natively supports separated sampler, so sampler part of API combined texture
          sampler will be directly mapped to HW separated sampler
       2. HW does not natively support separated sampler, so API combined texture sampler
          is directly mapped to HW combined sampler */
    SHADER_SAMPLER_SLOT_MAPPING                 samplerMapping;

    /* For the case that HW natively supports separated texture, so texture part of API
       combined texture sampler will be directly mapped to HW separated texture */
    SHADER_RESOURCE_SLOT_MAPPING                texMapping;

}PROG_CL_COMBINED_TEXTURE_SAMPLER_HW_MAPPING;

/* TO_DO, image array and sampler array */
typedef struct PROG_CL_COMBINED_TEX_SAMPLER_TABLE_ENTRY
{
    gctUINT                                    imageArgIndex;

    gctBOOL                                    kernelHardcodeSampler;
    gctUINT                                    samplerArgIndex;
    gctUINT                                    constSamplerValue;

    PROG_CL_COMBINED_TEXTURE_SAMPLER_HW_MAPPING * hwMappings;
}
PROG_CL_COMBINED_TEX_SAMPLER_TABLE_ENTRY;


typedef struct PROG_CL_COMBINED_TEXTURE_SAMPLER_TABLE
{
    PROG_CL_COMBINED_TEX_SAMPLER_TABLE_ENTRY*   pCombTsEntries;
    gctUINT                                     countOfEntries;
}
PROG_CL_COMBINED_TEXTURE_SAMPLER_TABLE;

typedef struct PROG_CL_IMAGE_TABLE_ENTRY
{
    gctUINT                                    imageArgIndex;

    gctBOOL                                    kernelHardcodeSampler;
    gctUINT                                    samplerArgIndex;
    gctUINT                                    constSamplerValue;

    SHADER_CONSTANT_SUB_ARRAY_MAPPING *        hwMapping;
    VSC_ImageDesc                              imageDesc; /*the assumed value of image descriptor by compiler*/
    gctUINT                                    assumedSamplerValue; /*the assumed value of sampler by compiler*/
    /*SHADER_CONSTANT_ARRAY_MAPPING *            hwMapping;*/
}
PROG_CL_IMAGE_TABLE_ENTRY;

typedef struct PROG_CL_IMAGE_TABLE
{
    PROG_CL_IMAGE_TABLE_ENTRY*                  pImageEntries;
    gctUINT                                     countOfEntries;
}
PROG_CL_IMAGE_TABLE;

/* Resource set */
typedef struct PROG_CL_RESOURCE_TABLE
{
    /* For texLd */
    PROG_CL_COMBINED_TEXTURE_SAMPLER_TABLE      combinedSampTexTable;
    /* For image */
    PROG_CL_IMAGE_TABLE                         imageTable;
    /* For other uniform include private???*/
    PROG_CL_UNIFORM_TABLE                       uniformTable;
    /* private uniform table */
    PROG_CL_PRV_UNIFORM_TABLE                   prvUniformTable;
}
PROG_CL_RESOURCE_TABLE;

typedef struct PROG_CL_ARG_ENTRY
{
    gctUINT                                     argIndex;
    gctBOOL                                     isImage;
    gctBOOL                                     isSampler;
    gctBOOL                                     isPointer;
    VSC_SHADER_DATA_TYPE                        type;
    /* for none basic type, VIR need have a name pool like GetUniformTypeNameOffset() */
    gctINT                                      typeNameOffset;
    gctUINT                                     addressQualifier;
    gctUINT                                     typeQualifier;
    gctUINT                                     accessQualifier;
    gctSTRING                                   argName;
    gctSTRING                                   argTypeName;
}PROG_CL_ARG_ENTRY;

typedef struct PROG_CL_ARG_TABLE
{
    gctUINT                                     countOfEntries;
    PROG_CL_ARG_ENTRY *                         pArgsEntries;
}PROG_CL_ARG_TABLE;

typedef struct KERNEL_FUNCTIN_PROPERTY
{
    gctUINT                                     type;
    gctUINT                                     size;
    gctUINT                                     value[3];
}KERNEL_FUNCTIN_PROPERTY;

typedef struct KERNEL_FUNCTION_HINTS
{
    gctBOOL                                     hasPrintf;
    gctUINT                                     imageCount;
    gctUINT                                     samplerCount;
    /* req work group size/ work group size hint/scalar hint */
    KERNEL_FUNCTIN_PROPERTY                     property[3];

    gctUINT32                                   privateMemorySize;
    gctUINT32                                   localMemorySize;
    gctUINT32                                   constantMemorySize;
    gctCHAR *                                   constantMemBuffer;
}KERNEL_FUNCTION_HINTS;

/* Executable kernel profile definition. It is generated only when clCreateKernel calling.
   Each client kernel only has one profile like this. */
typedef struct KERNEL_EXECUTABLE_PROFILE
{
    /* The shaders that this kernel contains. */
    SHADER_EXECUTABLE_PROFILE                   sep;

    /* hint and other attribte of this kernel */
    KERNEL_FUNCTION_HINTS                       kernelHints;

    /* origin kernel arg table, include info that driver need */
    PROG_CL_ARG_TABLE                           argTable;

    PROG_CL_RESOURCE_TABLE                      resourceTable;
}
KERNEL_EXECUTABLE_PROFILE;

gceSTATUS vscInitializeKEP(KERNEL_EXECUTABLE_PROFILE* pKEP);
gceSTATUS vscFinalizeKEP(KERNEL_EXECUTABLE_PROFILE* pKEP);

END_EXTERN_C();

#endif /* __gc_vsc_drvi_kernel_profile_h_ */



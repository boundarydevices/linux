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
   Following definitions are ONLY used for openGL(ES)/Vulkan to communicate with
   client query/binding request from program. NOTE that
   1. For openGL(ES), ONLY active parts are collected into each high level table.
      In this case, every table entry has a member pointing to HW resource in SEP.
   2. For Vulkan, all (whatever it's active or not, whatever the original shaders
      use or not) must be collected into each high level table as Vulkan's pipeline
      layout needs all resources in it be allocated into HW resources to avoid driver
      re-flushing when pipeline is switched. In this case, entry of each table wont
      point to HW resource in SEP as there might be no such HW resource in SEP which
      only record info that shader uses.
**********************************************************************************/

#ifndef __gc_vsc_drvi_program_profile_h_
#define __gc_vsc_drvi_program_profile_h_

BEGIN_EXTERN_C()

typedef enum _PEP_CLIENT
{
    PEP_CLIENT_UNKNOWN                = 0,
    PEP_CLIENT_GL                     = 1,
    PEP_CLIENT_VK                     = 2,
}
PEP_CLIENT;

typedef enum _GL_DATA_PRECISION
{
    GL_DATA_PRECISION_DEFAULT         = 0,
    GL_DATA_PRECISION_HIGH            = 1,
    GL_DATA_PRECISION_MEDIUM          = 2,
    GL_DATA_PRECISION_LOW             = 3,
}
GL_DATA_PRECISION;

typedef enum GL_MATRIX_MAJOR
{
    GL_MATRIX_MAJOR_ROW               = 0,
    GL_MATRIX_MAJOR_COLUMN            = 1,
}
GL_MATRIX_MAJOR;

typedef enum _GL_FX_BUFFERING_MODE
{
    GL_FX_BUFFERING_MODE_INTERLEAVED  = 0,
    GL_FX_BUFFERING_MODE_SEPARATED    = 1,
}
GL_FX_BUFFERING_MODE;

typedef enum GL_FX_STREAMING_MODE
{
    GL_FX_STREAMING_MODE_FFU          = 0,
    GL_FX_STREAMING_MODE_SHADER       = 1,
}
GL_FX_STREAMING_MODE;

typedef enum GL_FRAGOUT_USAGE
{
    GL_FRAGOUT_USAGE_USER_DEFINED     = 0,
    GL_FRAGOUT_USAGE_FRAGCOLOR        = 1,
    GL_FRAGOUT_USAGE_FRAGDATA         = 2,
}
GL_FRAGOUT_USAGE;

typedef enum GL_UNIFORM_HW_SUB_MAPPING_MODE
{
    GL_UNIFORM_HW_SUB_MAPPING_MODE_CONSTANT     = 0,
    GL_UNIFORM_HW_SUB_MAPPING_MODE_SAMPLER      = 1,
}
GL_UNIFORM_HW_SUB_MAPPING_MODE;

typedef enum GL_UNIFORM_USAGE
{
    /* All user defined uniforms will get this type */
    GL_UNIFORM_USAGE_USER_DEFINED     = 0,

    /* Built-in uniforms */
    GL_UNIFORM_USAGE_DEPTHRANGE_NEAR  = 1,
    GL_UNIFORM_USAGE_DEPTHRANGE_FAR   = 2,
    GL_UNIFORM_USAGE_DEPTHRANGE_DIFF  = 3,
    GL_UNIFORM_USAGE_HEIGHT           = 4,
}
GL_UNIFORM_USAGE;

typedef enum VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING_MODE
{
    VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING_MODE_NATIVELY_SUPPORT        = 0,
    VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING_MODE_NOT_NATIVELY_SUPPORT    = 1,
}
VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING_MODE;


/**********************************************************************************************************************
 **********************************     Common program mapping table definitions     **********************************
 **********************************************************************************************************************/

typedef enum _VSC_RES_ENTRY_BIT
{
    VSC_RES_ENTRY_BIT_NONE      = 0x0000,
    VSC_RES_ENTRY_BIT_8BIT      = 0x0001,
    VSC_RES_ENTRY_BIT_16BIT     = 0x0002,
    VSC_RES_ENTRY_BIT_32BIT     = 0x0004,
}VSC_RES_ENTRY_BIT;

/* Attribute table definition

   For GL(ES),
       1. glGetActiveAttrib works on boundary of PROG_ATTRIBUTE_TABLE_ENTRY which currently does not support
          array. It even must work when glLinkProgram failed.
       2. glGetAttribLocation and other location related functions work on boundary of SHADER_IO_REG_MAPPING,
          i.e, it must be on vec4 boundary.
   For Vulkan, no API query, just driver will use to do binding
*/

typedef struct PROG_ATTRIBUTE_TABLE_ENTRY
{
    VSC_SHADER_DATA_TYPE                        type;
    VSC_RES_ENTRY_BIT                           resEntryBit;
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;

    /* Decl'ed array size by GLSL, at this time, it must be 1 */
    gctSIZE_T                                   arraySize;

    /* Index based on countOfEntries in PROG_ATTRIBUTE_TABLE. It is corresponding to active attribute
       index */
    gctUINT                                     attribEntryIndex;

    /* Points to HW IO mapping that VS SEP maintains. Compiler MUST assure ioRegMappings are squeezed
       together for all active vec4 that activeVec4Mask are masked. So driver just needs go through
       activeVec4Mask to program partial of type to HW when programing vertexelement/vs input related
       registers */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* There are 2 types of pre-assigned location, from high priority to low priority
       1. Shader 'layout(location = ?)' qualifier
       2. By glBindAttribLocation API call

       If user didn't assign it, we should allocate it then. Each ioRegMapping has a
       corresponding location. */
    gctUINT*                                    pLocation;
    gctUINT                                     locationCount;

    /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4 */
    gctUINT                                     vec4BasedCount;

    /* To vec4BasedCount vec4 that expanded from type, which are really used by VS */
    gctUINT                                     activeVec4Mask;
}
PROG_ATTRIBUTE_TABLE_ENTRY;

typedef struct PROG_ATTRIBUTE_TABLE
{
    /* Entries for active attributes of VS */
    PROG_ATTRIBUTE_TABLE_ENTRY*                 pAttribEntries;
    gctUINT                                     countOfEntries;

    /* For query with ACTIVE_ATTRIBUTE_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
PROG_ATTRIBUTE_TABLE;

/* Fragment-output table definition

   For GL(ES),
       1. No API works on boundary of PROG_FRAGOUT_TABLE_ENTRY.
       2. glGetFragDataLocation works on boundary of SHADER_IO_REG_MAPPING, i.e, it must be on vec4 boundary.
   For Vulkan, no API query, just driver will use to do binding
*/

typedef struct PROG_FRAGOUT_TABLE_ENTRY
{
    VSC_SHADER_DATA_TYPE                        type;
    VSC_RES_ENTRY_BIT                           resEntryBit;
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;

    /* Specially for built-in ones */
    GL_FRAGOUT_USAGE                            usage;

    /* Index based on countOfEntries in PROG_FRAGOUT_TABLE. */
    gctUINT                                     fragOutEntryIndex;

    /* Points to HW IO mapping that PS SEP maintains. Compiler MUST assure ioRegMappings are squeezed
       together for all active vec4 that activeVec4Mask are masked. So driver just needs go through
       activeVec4Mask to program partial of type to HW when programing PE/ps output related registers */
    SHADER_IO_REG_MAPPING*                      pIoRegMapping;

    /* Shader can have 'layout(location = ?)' qualifier, if so, just use it. Otherwise, we should allocate
       it then. Each ioRegMapping has a corresponding location. */
    gctUINT*                                    pLocation;
    gctUINT                                     locationCount;

    /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4. Since
       frag output variable can only be scalar or vector, so it actually is the array size GLSL decl'ed */
    gctUINT                                     vec4BasedCount;

    /* To vec4BasedCount vec4 that expanded from type, which are really used by PS */
    gctUINT                                     activeVec4Mask;
}
PROG_FRAGOUT_TABLE_ENTRY;

typedef struct PROG_FRAGOUT_TABLE
{
    /* Entries for active fragment color of fragment shader */
    PROG_FRAGOUT_TABLE_ENTRY*                   pFragOutEntries;
    gctUINT                                     countOfEntries;
}
PROG_FRAGOUT_TABLE;


/**********************************************************************************************************************
 **********************************       GL program mapping table definitions       **********************************
 **********************************************************************************************************************/

/* Uniform (default uniform-block) table definition

   1. glGetActiveUniform works on boundary of PROG_GL_UNIFORM_TABLE_ENTRY which does not support
      struct, and the least supported data layout is array. It even must work when glLinkProgram
      failed.
   2. glGetUniformLocation and other location related functions work on boundary of element of
      scalar/vec/matrix/sampler array, i.e, single scalar/vec/matrix/sampler boundary.
*/

typedef struct PROG_GL_UNIFORM_HW_SUB_MAPPING
{
    /* We have 2 choices here

       1. Dynamic-indexed sub array or imm-indexed sub array?
          For Dynamic-indexed sub array, HW register/memory must be successive, while for imm-indexed
          sub array, each element can be allocated into anywhere HW can hold it, by which we can reduce
          HW uniform storage footprint. So do we need split imm-indexed sub array into more pieces??

       2. What's more, do we need much smaller granularity other than element of array, such as vec or
          channel??

       Currently, imm-indexed sub array is regard as dynamic-indexed sub array and granularity of element
       of array are used, by which, overhead of driver is small, but HW uniform storage footprint is big.
    */

    /* Active range of sub array of whole array uniform sized by 'activeArraySize'. */
    gctUINT                                     startIdx;
    gctUINT                                     rangeOfSubArray;

    /* Which channels are really used by compiler. For example, vec3 can be 0x07, 0x06 and 0x05,etc and when
       it is 0x05, although CHANNEL_Y is disabled, compiler will still allocate continuous 3 HW channels for it.
       That means compiler will assure minimum usage of HW channels while hole is permitted, unless above 2nd
       choice is channel-based granularity (see commments of validChannelMask of SHADER_CONSTANT_SUB_ARRAY_MAPPING).
       To driver uniform update, it then can use validChannelMask to match validChannelMask of SHADER_CONSTANT_SUB_ARRAY_MAPPING
       (further validHWChannelMask) to determine where data goes. For sampler, it must be 0x01 */
    gctUINT                                     validChannelMask;

    GL_UNIFORM_HW_SUB_MAPPING_MODE              hwSubMappingMode;

    /* Points to HW constant/sampler mapping that SEPs maintains. Compiler assure this active sub range is
       mapped to successive HW register/memory. */
    union
    {
        /* Directly increase HW regNo or memory address of pSubCBMapping with (4-tuples * (1~4) based on type of
           element) to access next element of array if 'rangeOfSubArray' is GT 1 */
        SHADER_CONSTANT_SUB_ARRAY_MAPPING*      pSubCBMapping;

        /* Use (pSamplerMapping ++) to next sampler mapping access if 'rangeOfSubArray' is GT 1 */
        SHADER_SAMPLER_SLOT_MAPPING*            pSamplerMapping;
    } hwMapping;
}
PROG_GL_UNIFORM_HW_SUB_MAPPING;

typedef struct PROG_GL_UNIFORM_HW_MAPPING
{
    /* For following two cases, pHWSubMappings maps whole of uniform to HW resource, and countOfHWSubMappings
       must be 1.

       a. For GL, when mapping to mem, it looks individual uniform of named uniform block can not be separated
          into several active sections due to there is no API let user set part of uniform data, so as long as
          any part of such uniform is used, compiler must deem all data of uniform is used.
       b. For Vulkan, to support pipeline layout compability, we also must deem all data of uniform is used. */
    PROG_GL_UNIFORM_HW_SUB_MAPPING*             pHWSubMappings;
    gctUINT                                     countOfHWSubMappings;
}
PROG_GL_UNIFORM_HW_MAPPING;

typedef struct PROG_GL_UNIFORM_COMMON_ENTRY
{
    VSC_SHADER_DATA_TYPE                        type;
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;
    GL_DATA_PRECISION                           precision;

    /* Decl'ed array size by GLSL */
    gctSIZE_T                                   arraySize;

    /* Maximun used element index + 1, considering in whole program scope */
    gctSIZE_T                                   activeArraySize;

    /* It is corresponding to active uniform index. NOTE that this index is numbered in global
       scope, including default uniform block and all named uniform blocks */
    gctUINT                                     uniformEntryIndex;

    /* Different shader stage may have different HW mappings. */
    PROG_GL_UNIFORM_HW_MAPPING                  hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_GL_UNIFORM_COMMON_ENTRY;

typedef struct PROG_GL_UNIFORM_TABLE_ENTRY
{
    PROG_GL_UNIFORM_COMMON_ENTRY                common;

    /* Specially for built-in ones */
    GL_UNIFORM_USAGE                            usage;

    /* First location index of this uniform, the last location for this uniform is
       locationBase + (activeArraySize of GL_UNIFORM_COMMON_ENTRY) - 1 */
    gctUINT                                     locationBase;
}
PROG_GL_UNIFORM_TABLE_ENTRY;

typedef struct PROG_GL_UNIFORM_TABLE
{
    /* Entries for active uniforms in default uniform block of whole program. They are
       arranged into 3 chunks in order (sampler + builtin + user-defined) */
    PROG_GL_UNIFORM_TABLE_ENTRY*                pUniformEntries;
    gctUINT                                     countOfEntries;
    gctUINT                                     firstBuiltinUniformEntryIndex;
    gctUINT                                     firstUserUniformEntryIndex;

    /* For query with ACTIVE_UNIFORM_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
PROG_GL_UNIFORM_TABLE;

/* Uniform-block (named uniform-block, pure constants, can't be samplers) table definition

   1. glGetActiveUniformBlockiv and other uniform block index related functions work on boundary of
      PROG_GL_UNIFORMBLOCK_TABLE_ENTRY (uniform group).
   2. glGetActiveUniform works on boundary of PROG_GL_UNIFORMBLOCK_UNIFORM_ENTRY which does not support
      struct, and the least supported data layout is array. It even must work when glLinkProgram failed.
   3. Uniforms of uniform-block have no location concept.
*/

typedef struct PROG_GL_UNIFORMBLOCK_UNIFORM_ENTRY
{
    PROG_GL_UNIFORM_COMMON_ENTRY                common;

    /* Layout info for individual uniform of uniform block */
    gctSIZE_T                                   offset;
    gctSIZE_T                                   arrayStride;
    gctSIZE_T                                   matrixStride;
    GL_MATRIX_MAJOR                             matrixMajor;
}
PROG_GL_UNIFORMBLOCK_UNIFORM_ENTRY;

typedef struct PROG_GL_UNIFORMBLOCK_TABLE_ENTRY
{
    PROG_GL_UNIFORMBLOCK_UNIFORM_ENTRY*         pUniformEntries;
    gctUINT                                     countOfEntries;

    /* Each includes copy of uniformEntryIndex of 'common' of pUniformEntries. Whole list is for
       query with UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES */
    gctUINT*                                    pUniformIndices;

    /* Index based on countOfEntries in PROG_GL_UNIFORMBLOCK_TABLE. It is corresponding to active uniform
       block index */
    gctUINT                                     ubEntryIndex;

    /* How many byte size does storage store this uniform block in minimum? */
    gctUINT                                     dataSizeInByte;

    /* If true, there must be something in hwMappings in PROG_GL_UNIFORM_COMMON_ENTRY */
    gctBOOL                                     bRefedByShader[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_GL_UNIFORMBLOCK_TABLE_ENTRY;

typedef struct PROG_GL_UNIFORMBLOCK_TABLE
{
    /* Entries for active uniform blocks of whole program */
    PROG_GL_UNIFORMBLOCK_TABLE_ENTRY*           pUniformBlockEntries;
    gctUINT                                     countOfEntries;

    /* For query with ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH */
    gctUINT                                     maxLengthOfName;
}
PROG_GL_UNIFORMBLOCK_TABLE;

/* Transform feedback table definition

   glTransformFeedbackVaryings and glGetTransformFeedbackVarying work on boundary of PROG_GL_XFB_OUT_TABLE_ENTRY.
*/

typedef struct PROG_GL_XFB_OUT_TABLE_ENTRY
{
    VSC_SHADER_DATA_TYPE                        type;
    gctCONST_STRING                             name;
    gctUINT                                     nameLength;

    /* Decl'ed array size by GLSL */
    gctSIZE_T                                   arraySize;

    /* Index based on countOfEntries in PROG_GL_XFB_OUT_TABLE. */
    gctUINT                                     fxOutEntryIndex;

    /* Data streamming-2-memory mode, by fixed function unit or shader? */
    GL_FX_STREAMING_MODE                        fxStreamingMode;

    union
    {
        /* For writing to XFBO with fixed-function */
        struct
        {
            /* Points to HW IO mapping that VS SEP maintains.

               From spec, if VS has no write to output variable, the value streamed out to buffer is undefined.
               So, we have 2 choices depending on compiler design.

               1. Compiler MUST assure ioRegMappings are squeezed together for all active vec4 that activeVec4Mask
                  are masked. So driver needs go through activeVec4Mask to select correct HW reg for active ones and
                  any of HW reg for unactive ones when programing SO related registers.

               2. Compiler allocate HW regs for vec4 by regarding them all active. In this case, activeVec4Mask still
                  points out which are really written by VS

               Choice 2 is simple, but choice 1 can get better perf */
            SHADER_IO_REG_MAPPING*              pIoRegMapping;

            /* How many vec4 'type' can group? For component count of type LT vec4, regarding it as vec4 */
            gctUINT                             vec4BasedCount;

            /* To vec4BasedCount vec4 that expanded from type, which are really written by VS */
            gctUINT                             activeVec4Mask;
        } s;

        /* For the case that shader stores data into XFBO */
        SHADER_UAV_SLOT_MAPPING*                pSoBufferMapping;
    } u;
}
PROG_GL_XFB_OUT_TABLE_ENTRY;

typedef struct PROG_GL_XFB_OUT_TABLE
{
    /* Entries for active transform feedback varyings. pFxOutEntries must be in-orderly corresponding to
       param 'varyings' of glTransformFeedbackVaryings, and countOfEntries is also corresponding to param
       'count' of that API */
    PROG_GL_XFB_OUT_TABLE_ENTRY*                pFxOutEntries;
    gctUINT                                     countOfEntries;

    /* To control how to serialize to buffer */
    GL_FX_BUFFERING_MODE                        mode;

    /* For query with TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH */
    gctUINT                                     maxLengthOfName;
}
PROG_GL_XFB_OUT_TABLE;


/**********************************************************************************************************************
 **********************************        VK program mapping table definitions       *********************************
 **********************************************************************************************************************/

typedef struct PROG_VK_IMAGE_FORMAT_INFO
{
    VSC_IMAGE_FORMAT                            imageFormat;
    gctBOOL                                     bSetInSpriv;
}
PROG_VK_IMAGE_FORMAT_INFO;

typedef struct PROG_VK_IMAGE_DERIVED_INFO
{
    /* For an image, it might need an image-size attached. As each image in
       Binding::arraySize array has image-size, so this is the first entry
       of image-size array. */
    SHADER_PRIV_CONSTANT_ENTRY*                 pImageSize;

    /* Extra layer HW mapping. As currently, for images in in tBinding::arraySize
       array, if one image has extra image, all other images must have extra image, so
       this is the first entry of extra-image */
    SHADER_PRIV_UAV_ENTRY*                      pExtraLayer;

    /* For an image, it might need a mip level attached. As each texel buffer in
       Binding::arraySize array has levelsSamples, so this is the first entry
       of pMipLevel array. */
    SHADER_PRIV_CONSTANT_ENTRY*                 pMipLevel;

    /* ImageFormat, can be NONE. */
    PROG_VK_IMAGE_FORMAT_INFO                   imageFormatInfo;
}
PROG_VK_IMAGE_DERIVED_INFO;

typedef struct PROG_VK_SAMPLER_DERIVED_INFO
{
    /* For a sampler, it might need a texture-size attached. As each sampler in
       arraySize array has texture-size, so this is the first entry
       of texture-size array. */
    SHADER_PRIV_CONSTANT_ENTRY*                 pTextureSize[2];

    /* For a sampler, it might need a lodMinMax attached. As each sampler in
       arraySize array has lodMinMax, so this is the first entry
       of lodMinMax array. */
    SHADER_PRIV_CONSTANT_ENTRY*                 pLodMinMax[2];

    /* For a sampler, it might need a levelsSamples attached. As each sampler in
       rraySize array has levelsSamples, so this is the first entry
       of levelsSamples array. */
    SHADER_PRIV_CONSTANT_ENTRY*                 pLevelsSamples[2];
}
PROG_VK_SAMPLER_DERIVED_INFO;

typedef struct PROG_VK_SUB_RESOURCE_BINDING
{
    /* Pointer to original API resource binding */
    VSC_SHADER_RESOURCE_BINDING*                pResBinding;

    /* Start array index in original pResBinding::arraySize */
    gctUINT                                     startIdxOfSubArray;

    /* Size of sub array size*/
    gctUINT                                     subArraySize;
}
PROG_VK_SUB_RESOURCE_BINDING;

typedef struct PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING
{
    /* Index based on countOfArray in PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_POOL. */
    gctUINT                                     pctsHmEntryIndex;

    /* For sub-array-size m of sampler, and sub-array-size n of tex, if
       bSamplerMajor is true, the hw sampler will be organized as
              ((t0, t1, t2...tn), (t0, t1, t2...tn), ...),
       otherwise it will be organized as
              ((s0, s1, s2...sm), (s0, s1, s2...sm), ...) */
    gctBOOL                                     bSamplerMajor;

    /* Texture and sampler part for this private combined tex-sampler */
    PROG_VK_SUB_RESOURCE_BINDING                texSubBinding;
    PROG_VK_SUB_RESOURCE_BINDING                samplerSubBinding;

    /* HW sampler slot mapping */
    SHADER_SAMPLER_SLOT_MAPPING*                pSamplerSlotMapping;

    /* The array size is ((sub-array-size m of sampler) * (sub-array-size n of tex)) */
    SHADER_PRIV_SAMPLER_ENTRY**                 ppExtraSamplerArray;

    /*----------------------------------Sampler-related----------------------------------*/
    PROG_VK_SAMPLER_DERIVED_INFO                samplerDerivedInfo;
}
PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING;

typedef struct PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_POOL
{
    PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING*      pPrivCombTsHwMappingArray;
    gctUINT                                     countOfArray;
}
PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_POOL, PROG_VK_PRIV_CTS_HW_MAPPING_POOL;

typedef struct PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST
{
    /* The entry index (which is indexed to array pPrivCombTsHwMappingArray of
       PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_POOL) array */
    gctUINT*                                    pPctsHmEntryIdxArray;

    gctUINT                                     arraySize;
}PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST;

/* Combined texture sampler table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_COMBINED_IMAGE_SAMPLER
*/

#define __YCBCR_PLANE_COUNT__   3
typedef struct PROG_VK_COMBINED_TEXTURE_SAMPLER_HW_MAPPING
{
    /* For the case
       1. HW natively supports separated sampler, so sampler part of API combined texture
          sampler will be directly mapped to HW separated sampler
       2. HW does not natively support separated sampler, so API combined texture sampler
          is directly mapped to HW combined sampler */
    SHADER_SAMPLER_SLOT_MAPPING                 samplerMapping;

    /* For the case that HW does not natively support separated sampler. The array size is
       combTsBinding::arraySize */
    SHADER_PRIV_SAMPLER_ENTRY**                 ppExtraSamplerArray;

    /* For the ycbcr texture recompilation. */
    SHADER_PRIV_UAV_ENTRY**                     ppYcbcrPlanes;

    /* For the case that HW natively supports separated texture, so texture part of API
       combined texture sampler will be directly mapped to HW separated texture */
    SHADER_RESOURCE_SLOT_MAPPING                texMapping;

    /* For the case that API combined texture sampler is combined to with other
       API sampler/texture (HW does not natively support separated sampler) */
    PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST  samplerHwMappingList;
    PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST  texHwMappingList;
}PROG_VK_COMBINED_TEXTURE_SAMPLER_HW_MAPPING;

typedef struct PROG_VK_COMBINED_TEX_SAMPLER_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 combTsBinding;

    /* Index based on countOfEntries in PROG_VK_COMBINED_TEXTURE_SAMPLER_TABLE. */
    gctUINT                                     combTsEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /*----------------------------------Sampler-related----------------------------------*/
    PROG_VK_SAMPLER_DERIVED_INFO                samplerDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /* Which kinds of inst operation acting on sampler (texture part). The count of
       this resOpBit is same as combTsBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;

    /* Different shader stage may have different HW mappings. */
    PROG_VK_COMBINED_TEXTURE_SAMPLER_HW_MAPPING hwMappings[VSC_MAX_SHADER_STAGE_COUNT];

    /* The sampled image is saved in the storage table, save the index. */
    gctUINT                                     sampledImageIndexInStorageTable[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_COMBINED_TEX_SAMPLER_TABLE_ENTRY;

typedef struct PROG_VK_COMBINED_TEXTURE_SAMPLER_TABLE
{
    PROG_VK_COMBINED_TEX_SAMPLER_TABLE_ENTRY*   pCombTsEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_COMBINED_TEXTURE_SAMPLER_TABLE;

/* Separated-sampler table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_SAMPLER
*/

typedef union PROG_VK_SEPARATED_SAMPLER_HW_MAPPING
{
    /* For HW natively supports separated sampler */
    SHADER_SAMPLER_SLOT_MAPPING                 samplerMapping;

    /* For HW does not natively supports separated sampler */
    PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST  samplerHwMappingList;
}
PROG_VK_SEPARATED_SAMPLER_HW_MAPPING;

typedef struct PROG_VK_SEPARATED_SAMPLER_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 samplerBinding;

    /* Index based on countOfEntries in PROG_VK_SEPARATED_SAMPLER_TABLE. */
    gctUINT                                     samplerEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /* Which kinds of inst operation acting on sampler. The count of this
       resOpBit is same as samplerBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;

    /* Whether to use hw mapping list in hwMappings. TRUE only under HW
       does not natively supports separated sampler */
    gctBOOL                                     bUsingHwMppingList;

    /* Different shader stage may have different HW mappings. */
    PROG_VK_SEPARATED_SAMPLER_HW_MAPPING        hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_SEPARATED_SAMPLER_TABLE_ENTRY;

typedef struct PROG_VK_SEPARATED_SAMPLER_TABLE
{
    PROG_VK_SEPARATED_SAMPLER_TABLE_ENTRY*      pSamplerEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_SEPARATED_SAMPLER_TABLE;

/* Separated-texture table definition

   Must support following resource types if HW supports separated texture
   VSC_SHADER_RESOURCE_TYPE_SAMPLED_IMAGE
*/

typedef union PROG_VK_SEPARATED_TEXTURE_HW_MAPPING
{
    /* For HW natively supports separated texture */
    SHADER_RESOURCE_SLOT_MAPPING                    texMapping;

    /* For HW does not natively supports separated texture */
    struct
    {
        PROG_VK_IMAGE_DERIVED_INFO                  imageDerivedInfo;

        /* We still need to allocate a constant image for this separated texture for the imageFetch operation.*/
        SHADER_UAV_SLOT_MAPPING                     hwMapping;

        PROG_VK_PRIV_COMB_TEX_SAMP_HW_MAPPING_LIST  texHwMappingList;
    } s;
}
PROG_VK_SEPARATED_TEXTURE_HW_MAPPING;

typedef struct PROG_VK_SEPARATED_TEXTURE_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 texBinding;

    /* Index based on countOfEntries in PROG_VK_SEPARATED_TEXTURE_TABLE. */
    gctUINT                                     textureEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /* Which kinds of inst operation acting on texture. The count of this
       resOpBit is same as texBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;

    /* Whether to use hw mapping list in hwMappings. TRUE only under HW
       does not natively supports separated texture */
    gctBOOL                                     bUsingHwMppingList;

    /* Different shader stage may have different HW mappings. */
    PROG_VK_SEPARATED_TEXTURE_HW_MAPPING        hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_SEPARATED_TEXTURE_TABLE_ENTRY;

typedef struct PROG_VK_SEPARATED_TEXTURE_TABLE
{
    PROG_VK_SEPARATED_TEXTURE_TABLE_ENTRY*      pTextureEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_SEPARATED_TEXTURE_TABLE;

/* Uniform texel buffer table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_UNIFORM_TEXEL_BUFFER
*/

typedef struct PROG_VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING
{
    VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING_MODE     hwMappingMode;

    union
    {
        /* For HW natively supports separated texture */
        SHADER_RESOURCE_SLOT_MAPPING            texMapping;

        /* For HW does not natively supports separated texture */
        struct
        {
            SHADER_HW_MEM_ACCESS_MODE               hwMemAccessMode;

            union
            {
                SHADER_SAMPLER_SLOT_MAPPING         samplerMapping;

                SHADER_CONSTANT_HW_LOCATION_MAPPING*pHwDirectAddrBase;
            } hwLoc;

            /* The array size is utbBinding::arraySize */
            SHADER_PRIV_SAMPLER_ENTRY**         ppExtraSamplerArray;
        } s;
    } u;
}
PROG_VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING;

typedef enum PROG_VK_UNIFORM_TEXEL_BUFFER_ENTRY_FLAG
{
    PROG_VK_UTB_ENTRY_FLAG_NONE                         = 0x0000,
    /* Treat a texelBuffer as an image, now from a recompilation only. */
    PROG_VK_UTB_ENTRY_FLAG_TREAT_TEXELBUFFER_AS_IMAGE   = 0x0002,
} PROG_VK_UNIFORM_TEXEL_BUFFER_ENTRY_FLAG;

typedef struct PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 utbBinding;

    /* Index based on countOfEntries in PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE. */
    gctUINT                                     utbEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    PROG_VK_UNIFORM_TEXEL_BUFFER_ENTRY_FLAG     utbEntryFlag;

    /*----------------------------------Sampler-related----------------------------------*/
    PROG_VK_SAMPLER_DERIVED_INFO                samplerDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /*----------------------------------Image-related----------------------------------*/
    PROG_VK_IMAGE_DERIVED_INFO                  imageDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /* Which kinds of inst operation acting on texture. The count of this
       resOpBit is same as utbBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;

    /* Different shader stage may have different HW mappings. */
    PROG_VK_UNIFORM_TEXEL_BUFFER_HW_MAPPING     hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE_ENTRY;

typedef struct PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE
{
    PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE_ENTRY*    pUtbEntries;
    gctUINT                                      countOfEntries;
}
PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE;

/* Input attachment table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT
*/

typedef union PROG_VK_INPUT_ATTACHMENT_HW_MAPPING
{
    SHADER_UAV_SLOT_MAPPING                     uavMapping;

    /* For the case that HW does not natively support separated sampler. The array size is
       iaBinding::arraySize */
    SHADER_PRIV_SAMPLER_ENTRY**                 ppExtraSamplerArray;
}
PROG_VK_INPUT_ATTACHMENT_HW_MAPPING;

typedef struct PROG_VK_INPUT_ATTACHMENT_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 iaBinding;

    /* Index based on countOfEntries in PROG_VK_INPUT_ATTACHMENT_TABLE. */
    gctUINT                                     iaEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /* Different shader stage may have different HW mappings. */
    PROG_VK_INPUT_ATTACHMENT_HW_MAPPING         hwMappings[VSC_MAX_SHADER_STAGE_COUNT];

    /*
    ** If hwMappings[stageIdx].uavMapping.hwMemAccessMode = SHADER_HW_MEM_ACCESS_MODE_DIRECT_SAMPLER,
    ** it is treated as a sampler, otherwise it is treated as an image.
    */
    /*----------------------------------Image-related----------------------------------*/
    PROG_VK_IMAGE_DERIVED_INFO                  imageDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /*----------------------------------Sampler-related----------------------------------*/
    PROG_VK_SAMPLER_DERIVED_INFO                samplerDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /* Which kinds of inst operation acting on sampler (has flag VIR_SRE_FLAG_TREAT_IA_AS_SAMPLER). The count of
       this resOpBit is same as iaBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;
}
PROG_VK_INPUT_ATTACHMENT_TABLE_ENTRY;

typedef struct PROG_VK_INPUT_ATTACHMENT_TABLE
{
    PROG_VK_INPUT_ATTACHMENT_TABLE_ENTRY*        pIaEntries;
    gctUINT                                      countOfEntries;
}
PROG_VK_INPUT_ATTACHMENT_TABLE;

/* Storage table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_STORAGE_BUFFER
   VSC_SHADER_RESOURCE_TYPE_STORAGE_IMAGE
   VSC_SHADER_RESOURCE_TYPE_STORAGE_TEXEL_BUFFER
   VSC_SHADER_RESOURCE_TYPE_STORAGE_BUFFER_DYNAMIC
*/

typedef struct PROG_VK_STORAGE_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 storageBinding;

    /* Index based on countOfEntries in PROG_VK_STORAGE_TABLE. */
    gctUINT                                     storageEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /*----------------------------------Image-related----------------------------------*/
    PROG_VK_IMAGE_DERIVED_INFO                  imageDerivedInfo[VSC_MAX_SHADER_STAGE_COUNT];

    /* Which kinds of inst operation acting on texture. The count of this
       resOpBit is same as storageBinding::arraySize */
    VSC_RES_OP_BIT*                             pResOpBits;

    /* Different shader stage may have different HW mappings. */
    SHADER_UAV_SLOT_MAPPING                     hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_STORAGE_TABLE_ENTRY;

typedef struct PROG_VK_STORAGE_TABLE
{
    PROG_VK_STORAGE_TABLE_ENTRY*                pStorageEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_STORAGE_TABLE;

/* Uniform-buffer table definition

   Must support following resource types
   VSC_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER
   VSC_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER_DYNAMIC
*/

typedef struct PROG_VK_UNIFORM_BUFFER_TABLE_ENTRY
{
    /* API resource binding */
    VSC_SHADER_RESOURCE_BINDING                 ubBinding;

    /* Index based on countOfEntries in PROG_VK_UNIFORM_BUFFER_TABLE. */
    gctUINT                                     ubEntryIndex;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /* Different shader stage may have different HW mappings. */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_UNIFORM_BUFFER_TABLE_ENTRY;

typedef struct PROG_VK_UNIFORM_BUFFER_TABLE
{
    PROG_VK_UNIFORM_BUFFER_TABLE_ENTRY*         pUniformBufferEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_UNIFORM_BUFFER_TABLE;

/* Resource set */
typedef struct PROG_VK_RESOURCE_SET
{
    /* This set only holds the entries with current set-NO in each resource table */

    PROG_VK_COMBINED_TEXTURE_SAMPLER_TABLE      combinedSampTexTable;
    PROG_VK_SEPARATED_SAMPLER_TABLE             separatedSamplerTable;
    PROG_VK_SEPARATED_TEXTURE_TABLE             separatedTexTable;
    PROG_VK_UNIFORM_TEXEL_BUFFER_TABLE          uniformTexBufTable;
    PROG_VK_INPUT_ATTACHMENT_TABLE              inputAttachmentTable;
    PROG_VK_STORAGE_TABLE                       storageTable;
    PROG_VK_UNIFORM_BUFFER_TABLE                uniformBufferTable;
}
PROG_VK_RESOURCE_SET;

/* Push-constant table definition */

typedef struct PROG_VK_PUSH_CONSTANT_TABLE_ENTRY
{
    /* API push-constant range */
    VSC_SHADER_PUSH_CONSTANT_RANGE              pushConstRange;

    /* Which shader stages access this entry */
    VSC_SHADER_STAGE_BIT                        stageBits;

    /* Is this entry really used by shader */
    gctUINT                                     activeStageMask;

    /* Different shader stage may have different HW mappings. */
    SHADER_CONSTANT_HW_LOCATION_MAPPING         hwMappings[VSC_MAX_SHADER_STAGE_COUNT];
}
PROG_VK_PUSH_CONSTANT_TABLE_ENTRY;

typedef struct PROG_VK_PUSH_CONSTANT_TABLE
{
    PROG_VK_PUSH_CONSTANT_TABLE_ENTRY*          pPushConstantEntries;
    gctUINT                                     countOfEntries;
}
PROG_VK_PUSH_CONSTANT_TABLE;

/* Executable program profile definition. It is generated only when glLinkProgram or glProgramBinary
   calling. Each client program only has one profile like this. */
typedef struct PROGRAM_EXECUTABLE_PROFILE
{
    PEP_CLIENT                                  pepClient;

    /* The executable shaders that this program contains. */
    SHADER_EXECUTABLE_PROFILE                   seps[VSC_MAX_SHADER_STAGE_COUNT];

    /* I/O high level mapping tables (high level variable to #, pointing to entry of SEP) */
    PROG_ATTRIBUTE_TABLE                        attribTable;
    PROG_FRAGOUT_TABLE                          fragOutTable;

    /* Other high level mapping tables */
    union
    {
        /* GL client only. High level variable to #, pointing to entry of SEP */
        struct
        {
            PROG_GL_UNIFORM_TABLE               uniformTable;
            PROG_GL_UNIFORMBLOCK_TABLE          uniformBlockTable;
            PROG_GL_XFB_OUT_TABLE               fxOutTable;
        } gl;

        /* Vulkan client only. High level binding to hw resource */
        struct
        {
            PROG_VK_RESOURCE_SET*               pResourceSets;
            gctUINT32                           resourceSetCount;

            PROG_VK_PUSH_CONSTANT_TABLE         pushConstantTable;

            /* It is not part of API resources, but for some HWs which does not support separated
               texture/sampler, api texture/sampler need to be combined by ourselves, so we need
               a separated private combined texture sampler table to maintain all pairs of api
               texture/sampler which have a specific hw mapping */
            PROG_VK_PRIV_CTS_HW_MAPPING_POOL    privateCombTsHwMappingPool;
        } vk;
    } u;
}
PROGRAM_EXECUTABLE_PROFILE;

gceSTATUS vscInitializePEP(PROGRAM_EXECUTABLE_PROFILE* pPEP);
gceSTATUS vscFinalizePEP(PROGRAM_EXECUTABLE_PROFILE* pPEP);

END_EXTERN_C();

#endif /* __gc_vsc_drvi_program_profile_h_ */



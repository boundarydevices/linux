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


#ifndef __gc_hal_priv_h_
#define __gc_hal_priv_h_

#include "gc_hal_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _gcsSYMBOLSLIST *        gcsSYMBOLSLIST_PTR;
/******************************************************************************
**
** Patch defines which should be moved to dedicate file later
**
** !!! ALWAYS ADD new ID in the TAIL, otherwise will break exising TRACE FILE
*******************************************************************************/
typedef enum _gcePATCH_ID
{
    gcvPATCH_NOTINIT = -1,
    gcvPATCH_INVALID = 0,

    gcvPATCH_DEBUG,
    gcvPATCH_GTFES30,
    gcvPATCH_CTGL11,
    gcvPATCH_CTGL20,
    gcvPATCH_GLBM11,
    gcvPATCH_GLBM21,
    gcvPATCH_GLBM25,
    gcvPATCH_GLBM27,
    gcvPATCH_GLBMGUI,
    gcvPATCH_GFXBENCH,
    gcvPATCH_ANTUTU, /* Antutu 3.x */
    gcvPATCH_ANTUTU4X, /* Antutu 4.x */
    gcvPATCH_ANTUTU5X, /* Antutu 5.x */
    gcvPATCH_ANTUTUGL3, /* Antutu 3D Rating */
    gcvPATCH_QUADRANT,
    gcvPATCH_GPUBENCH,
    gcvPATCH_GLOFTSXHM,
    gcvPATCH_XRUNNER,
    gcvPATCH_BUSPARKING3D,
    gcvPATCH_SIEGECRAFT,
    gcvPATCH_PREMIUM,
    gcvPATCH_RACEILLEGAL,
    gcvPATCH_MEGARUN,
    gcvPATCH_BMGUI,
    gcvPATCH_NENAMARK,
    gcvPATCH_NENAMARK2,
    gcvPATCH_FISHNOODLE,
    gcvPATCH_MM06,
    gcvPATCH_MM07,
    gcvPATCH_BM21,
    gcvPATCH_SMARTBENCH,
    gcvPATCH_JPCT,
    gcvPATCH_NEOCORE,
    gcvPATCH_RTESTVA,
    gcvPATCH_NBA2013,
    gcvPATCH_BARDTALE,
    gcvPATCH_F18,
    gcvPATCH_CARPARK,
    gcvPATCH_CARCHALLENGE,
    gcvPATCH_HEROESCALL,
    gcvPATCH_GLOFTF3HM,
    gcvPATCH_CRAZYRACING,
    gcvPATCH_FIREFOX,
    gcvPATCH_CHROME,
    gcvPATCH_MONOPOLY,
    gcvPATCH_SNOWCOLD,
    gcvPATCH_BM3,
    gcvPATCH_BASEMARKX,
    gcvPATCH_DEQP,
    gcvPATCH_SF4,
    gcvPATCH_MGOHEAVEN2,
    gcvPATCH_SILIBILI,
    gcvPATCH_ELEMENTSDEF,
    gcvPATCH_GLOFTKRHM,
    gcvPATCH_OCLCTS,
    gcvPATCH_A8HP,
    gcvPATCH_A8CN,
    gcvPATCH_WISTONESG,
    gcvPATCH_SPEEDRACE,
    gcvPATCH_FSBHAWAIIF,
    gcvPATCH_AIRNAVY,
    gcvPATCH_F18NEW,
    gcvPATCH_CKZOMBIES2,
    gcvPATCH_EADGKEEPER,
    gcvPATCH_BASEMARK2V2,
    gcvPATCH_RIPTIDEGP2,
    gcvPATCH_OESCTS,
    gcvPATCH_GANGSTAR,
    gcvPATCH_TRIAL,
    gcvPATCH_WHRKYZIXOVAN,
    gcvPATCH_GMMY16MAPFB,
    gcvPATCH_UIMARK,
    gcvPATCH_NAMESGAS,
    gcvPATCH_AFTERBURNER,
    gcvPATCH_ANDROID_CTS_MEDIA,
    gcvPATCH_FM_OES_PLAYER,
    gcvPATCH_SUMSUNG_BENCH,
    gcvPATCH_ROCKSTAR_MAXPAYNE,
    gcvPATCH_TITANPACKING,
    gcvPATCH_OES20SFT,
    gcvPATCH_OES30SFT,
    gcvPATCH_BASEMARKOSIICN,
    gcvPATCH_ANDROID_WEBGL,
    gcvPATCH_ANDROID_COMPOSITOR,
    gcvPATCH_CTS_TEXTUREVIEW,
    gcePATCH_ANDROID_CTS_GRAPHICS_GLVERSION,
    gcvPATCH_WATER2_CHUKONG,
    gcvPATCH_GOOGLEEARTH,
    gcvPATCH_LEANBACK,
    gcvPATCH_YOUTUBE_TV,
    gcvPATCH_NETFLIX,
    gcvPATCH_ANGRYBIRDS,
    gcvPATCH_REALRACING,
    gcvPATCH_TEMPLERUN,
    gcvPATCH_SBROWSER,
    gcvPATCH_CLASHOFCLAN,
    gcvPATCH_YOUILABS_SHADERTEST,
    gcvPATCH_AXX_SAMPLE,
    gcvPATCH_3DMARKSS,
    gcvPATCH_GFXBENCH4,
    gcvPATCH_BATCHCOUNT,
    gcvPATCH_SILICONSTUDIOGPUMARK,
    gcvPATCH_LEANBACKSCROLLING,
    gcvPATCH_ANTUTU6X, /* Antutu 6.x */
    gcvPATCH_ANTUTU3DBench,
    gcvPATCH_CAR_CHASE,
    gcvPATCH_ANDROID_BROWSER,
    gcvPATCH_COMPUTBENCH_CL, /* ComputBench 1.5 */
    gcvPATCH_ANDROID_CTS_UIRENDERING,
    gcvPATCH_GLU3, /* gfx3.0 glu.*/
    gcvPATCH_GLU4, /* gfx4.0 glu.*/
    gcvPATCH_MRVELBM20,
    gcvPATCH_OPENCV_ATOMIC,
    gcvPATCH_WESTON,
    gcvPATCH_SKIA_SKQP,
    gcvPATCH_SASCHAWILLEMS,
    gcvPATCH_NATIVEHARDWARE_CTS,
    gcvPATCH_ANDROID_PHOTOS,
    gcvPATCH_OVX_CTS,
    gcvPATCH_DEQP_VK,
    gcvPATCH_KANZI,
    gcvPATCH_OPENCV_TEST_PHOTO,
    gcvPATCH_VKMARK,
    gcvPATCH_VK_T3DSTRESSTEST,
    gcvPATCH_VK_HDR02_FBBASICTONEMAPPING,
    gcvPATCH_VK_BLOOM,
    gcvPATCH_CLBLAST,
    gcvPATCH_GRAPHICS_BENCHMARKS,
    gcvPATCH_INTEL_CL_BITONICSORT,
    gcvPATCH_PIGLIT_CL,

    gcvPATCH_COUNT
} gcePATCH_ID;


#if gcdENABLE_3D
#define gcdPROC_IS_WEBGL(patchId)       ((patchId) == gcvPATCH_CHROME           || \
                                         (patchId) == gcvPATCH_FIREFOX          || \
                                         (patchId) == gcvPATCH_ANDROID_WEBGL    || \
                                         (patchId) == gcvPATCH_ANDROID_BROWSER)


enum
{
    gcvHINT_BIT_0 = 1 << 0,
    gcvHINT_BIT_1 = 1 << 1,
    gcvHINT_BIT_2 = 1 << 2,
};

#endif /* gcdENABLE_3D */


/******************************************************************************\
******************************* Process local storage *************************
\******************************************************************************/

typedef struct _gcsPLS * gcsPLS_PTR;

typedef void (* gctPLS_DESTRUCTOR) (
    gcsPLS_PTR
    );

typedef struct _gcsPLS
{
    /* Global objects. */
    gcoOS                       os;
    gcoHAL                      hal;

    /* Internal memory pool. */
    gctSIZE_T                   internalSize;
    gctUINT32                   internalPhysName;
    gctPOINTER                  internalLogical;

    /* External memory pool. */
    gctSIZE_T                   externalSize;
    gctUINT32                   externalPhysName;
    gctPOINTER                  externalLogical;

    /* Contiguous memory pool. */
    gctSIZE_T                   contiguousSize;
    gctUINT32                   contiguousPhysName;
    gctPOINTER                  contiguousLogical;

    /* EGL-specific process-wide objects. */
    gctPOINTER                  eglDisplayInfo;
    gctPOINTER                  eglSurfaceInfo;
    gceSURF_FORMAT              eglConfigFormat;

    /* PLS reference count */
    gcsATOM_PTR                 reference;

    /* PorcessID of the constrcutor process */
    gctUINT32                   processID;

    /* ThreadID of the constrcutor process. */
    gctSIZE_T                   threadID;
    /* Flag for calling module destructor. */
    gctBOOL                     exiting;

    gctBOOL                     bNeedSupportNP2Texture;
    gctBOOL                     bDeviceOpen;

    gctPLS_DESTRUCTOR           destructor;
    /* Mutex to guard PLS access. currently it's for EGL.
    ** We can use this mutex for every PLS access.
    */
    gctPOINTER                  accessLock;

    /* Mutex to gurad gl FE compiler access. */
    gctPOINTER                  glFECompilerAccessLock;

    /* Mutex to gurad CL FE compiler access. */
    gctPOINTER                  clFECompilerAccessLock;

    /* Mutex to gurad VX context access. */
    gctPOINTER                  vxContextGlobalLock;

    /* Global patchID to overwrite the detection */
    gcePATCH_ID                 patchID;

    /* Global fenceID to record each fence object */
    gcsATOM_PTR                 globalFenceID;

    /* flag for memory profile */
    gctHANDLE               mainThread;
    gctBOOL                 bMemoryProfile;
    gctPOINTER              profileLock;
    gctUINT32               allocCount;
    gctUINT64               allocSize;
    gctUINT64               maxAllocSize;
    gctUINT32               freeCount;
    gctUINT64               freeSize;
    gctUINT64               currentSize;

    gctUINT32               video_allocCount;
    gctUINT64               video_allocSize;
    gctUINT64               video_maxAllocSize;
    gctUINT32               video_freeCount;
    gctUINT64               video_freeSize;
    gctUINT64               video_currentSize;
}
gcsPLS;

extern gcsPLS gcPLS;

gceSTATUS
gcoHAL_GetBaseAddr(
    IN  gcoHAL Hal,
    OUT gctUINT32 *BaseAddr
    );

gceSTATUS
gcoHAL_SetPatchID(
    IN  gcoHAL Hal,
    IN  gcePATCH_ID PatchID
    );

/* Get Patch ID based on process name */
gceSTATUS
gcoHAL_GetPatchID(
    IN  gcoHAL Hal,
    OUT gcePATCH_ID * PatchID
    );

gceSTATUS
gcoHAL_SetGlobalPatchID(
    IN  gcoHAL Hal,
    IN  gcePATCH_ID PatchID
    );

/* Detect if the current process is the executable specified. */
gceSTATUS
gcoOS_DetectProcessByName(
    IN gctCONST_STRING Name
    );

gceSTATUS
gcoOS_DetectProcessByEncryptedName(
    IN gctCONST_STRING Name
    );

gceSTATUS
gcoOS_DetectProgrameByEncryptedSymbols(
    IN gcsSYMBOLSLIST_PTR Symbols
    );

/* Get access to the process local storage. */
gceSTATUS
gcoHAL_GetPLS(
    OUT gcsPLS_PTR * PLS
    );

gceSTATUS
gcoHAL_SetPriority(
    IN gcoHAL Hal,
    IN gctUINT32 PriorityID
    );

gceSTATUS
gcoHAL_GetPriority(
    IN gcoHAL Hal,
    OUT gctUINT32 * PriorityID
    );

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_priv_h_ */



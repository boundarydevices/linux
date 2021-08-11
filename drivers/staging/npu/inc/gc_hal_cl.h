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


#ifndef __gc_hal_user_cl_h_
#define __gc_hal_user_cl_h_


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
****************************** Object Declarations *****************************
\******************************************************************************/

/* gcoCL_DEVICE_INFO object. */
typedef struct _gcoCL_DEVICE_INFO
{
    gctUINT             maxComputeUnits;
    gctUINT             ShaderCoreCount;
    gctUINT             maxWorkItemDimensions;
    gctUINT             maxWorkItemSizes[3];
    gctUINT             maxWorkGroupSize;
    gctUINT64           maxGlobalWorkSize ;
    gctUINT             clockFrequency;

    gctUINT             addrBits;
    gctUINT64           maxMemAllocSize;
    gctUINT64           globalMemSize;
    gctUINT64           localMemSize;
    gctUINT             localMemType;               /* cl_device_local_mem_type */
    gctUINT             globalMemCacheType;         /* cl_device_mem_cache_type */
    gctUINT             globalMemCachelineSize;
    gctUINT64           globalMemCacheSize;
    gctUINT             maxConstantArgs;
    gctUINT64           maxConstantBufferSize;
    gctUINT             maxParameterSize;
    gctUINT             memBaseAddrAlign;
    gctUINT             minDataTypeAlignSize;
    gctSIZE_T           maxPrintfBufferSize;

    gctBOOL             imageSupport;
    gctUINT             maxReadImageArgs;
    gctUINT             maxWriteImageArgs;
    gctUINT             vectorWidthChar;
    gctUINT             vectorWidthShort;
    gctUINT             vectorWidthInt;
    gctUINT             vectorWidthLong;
    gctUINT             vectorWidthFloat;
    gctUINT             vectorWidthDouble;
    gctUINT             vectorWidthHalf;
    gctUINT             image2DMaxWidth;
    gctUINT             image2DMaxHeight;
    gctUINT             image3DMaxWidth;
    gctUINT             image3DMaxHeight;
    gctUINT             image3DMaxDepth;
    gctUINT             maxSamplers;

    gctUINT64           queueProperties;        /* cl_command_queue_properties */
    gctBOOL             hostUnifiedMemory;
    gctBOOL             errorCorrectionSupport;
    gctUINT64           singleFpConfig;         /* cl_device_fp_config */
    gctUINT64           doubleFpConfig;         /* cl_device_fp_config */
    gctUINT             profilingTimingRes;
    gctBOOL             endianLittle;
    gctBOOL             deviceAvail;
    gctBOOL             compilerAvail;
    gctBOOL             linkerAvail;
    gctUINT64           execCapability;         /* cl_device_exec_capabilities */
    gctBOOL             atomicSupport;
    gctSIZE_T           imageMaxBufferSize;

    gctBOOL             computeOnlyGpu;           /* without graphih, only compute core GPU flag */
    gctBOOL             supportIMGInstr;          /* support image instruction */
    gctBOOL             psThreadWalker;           /* TW in PS */
    gctBOOL             supportSamplerBaseOffset; /* HW has samplerBaseOffset */
    gctUINT             maxRegisterCount;         /* max temp register count */
    gctBOOL             TxIntegerSupport;
    gctBOOL             halti2;                   /* Halit2 support */
    gctBOOL             multiWGPack;
    gctBOOL             asyncBLT;
    gctBOOL             multiCluster;

    /* cluster info */
    gctBOOL             clusterSupport;
    gctUINT32           clusterCount;
    gctUINT32           clusterAliveMask;
    gctUINT32           clusterAliveCount;
    gctUINT32           clusterMinID;
    gctUINT32           clusterMaxID;

    gceCHIPMODEL        chipModel;
    gctUINT32           chipRevision;

} gcoCL_DEVICE_INFO;

typedef gcoCL_DEVICE_INFO *  gcoCL_DEVICE_INFO_PTR;

/*******************************************************************************
**
**  gcoCL_SetHardwareType
**
**  Set hardware type in CL. If the specific type is not available,
**  it will query the first available type and set it.
**
**  INPUT:
**
**      The hardware type to be set.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_SetHardwareType(
    IN gceHARDWARE_TYPE Type
    );

gceSTATUS
gcoCL_ForceSetHardwareType(
    IN gceHARDWARE_TYPE Type,
    OUT gceHARDWARE_TYPE *savedType
    );

gceSTATUS
gcoCL_ForceRestoreHardwareType(
    IN gceHARDWARE_TYPE savedType
    );
/*******************************************************************************
**
**  gcoCL_InitializeHardware
**
**  Initialize hardware.  This is required for each thread.
**
**  INPUT:
**
**      Nothing
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_InitializeHardware(
    );

/**********************************************************************
**
**  gcoCL_SetHardware
**
**  Set the gcoHARDWARE object for current thread.
**
**  INPUT:
**
**      Nothing
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_SetHardware(
    IN gcoHARDWARE hw,
    OUT gcoHARDWARE *savedHW,
    OUT gceHARDWARE_TYPE *savedType,
    OUT gctUINT32 *savedCoreIndex
    );



    gceSTATUS
gcoCL_RestoreContext(
    IN gcoHARDWARE preHW,
    IN gceHARDWARE_TYPE preType,
    IN gctUINT32  preCoreIndex
    );



#define gcmDECLARE_SWITCHVARS \
    gcoHARDWARE _savedHW = gcvNULL; \
    gctBOOL  _switched = gcvFALSE;  \
    gctUINT32  _savedCoreIndex = 0;  \
    gceHARDWARE_TYPE  _savedType = gcvHARDWARE_INVALID

#define gcmSWITCH_TO_HW(hw)\
    { gcoCL_SetHardware((hw), &_savedHW, &_savedType, &_savedCoreIndex); \
        _switched = gcvTRUE;}

#define gcmSWITCH_TO_DEFAULT() \
     {gcoCL_SetHardware(gcvNULL, &_savedHW, &_savedType, &_savedCoreIndex); \
         _switched = gcvTRUE;}

#define gcmRESTORE_HW() \
    {if(_switched) { gcoCL_RestoreContext(_savedHW, _savedType, _savedCoreIndex);  _switched = gcvFALSE; }}


/*******************************************************************************
**
**  gcoCL_AllocateMemory
**
**  Allocate memory from the kernel.
**
**  INPUT:
**
**      gctUINT * Bytes
**          Pointer to the number of bytes to allocate.
**
**  OUTPUT:
**
**      gctUINT * Bytes
**          Pointer to a variable that will receive the aligned number of bytes
**          allocated.
**
**      gctUINT32_PTR Physical
**          Pointer to a variable that will receive the gpu virtual address of
**          the allocated memory, might be same as gpu physical address for flat
**          mapping case etc.
**
**      gctPOINTER * Logical
**          Pointer to a variable that will receive the logical address of the
**          allocation.
**
**      gcsSURF_NODE_PTR  * Node
**          Pointer to a variable that will receive the gcsSURF_NODE structure
**          pointer that describes the video memory to lock.
*/
gceSTATUS
gcoCL_AllocateMemory(
    IN OUT gctUINT *        Bytes,
    OUT gctUINT32_PTR       Physical,
    OUT gctPOINTER *        Logical,
    OUT gcsSURF_NODE_PTR *  Node,
    IN  gceSURF_TYPE        Type,
    IN  gctUINT32           Flag
    );

/*******************************************************************************
**
**  gcoCL_FreeMemory
**
**  Free contiguous memeory to the kernel.
**
**  INPUT:
**
**      gctUINT32 Physical
**          The gpu virutal addresses of the allocated pages.
**
**      gctPOINTER Logical
**          The logical address of the allocation.
**
**      gctUINT Bytes
**          Number of bytes allocated.
**
**      gcsSURF_NODE_PTR  Node
**          Pointer to a gcsSURF_NODE structure
**          that describes the video memory to unlock.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_FreeMemory(
    IN gctUINT32            Physical,
    IN gctPOINTER           Logical,
    IN gctUINT              Bytes,
    IN gcsSURF_NODE_PTR     Node,
    IN gceSURF_TYPE         Type
    );

/*******************************************************************************
**
**  gcoCL_WrapUserMemory
**
*/
gceSTATUS
gcoCL_WrapUserMemory(
    IN gctPOINTER           Ptr,
    IN gctUINT              Bytes,
    IN gctBOOL              VIVUnCached,
    OUT gctUINT32_PTR       Physical,
    OUT gcsSURF_NODE_PTR *  Node
    );

/*******************************************************************************
**
**  gcoCL_WrapUserPhysicalMemory
**
*/
gceSTATUS
gcoCL_WrapUserPhysicalMemory(
    IN gctUINT32_PTR        Physical,
    IN gctUINT              Bytes,
    IN gctBOOL              VIVUnCached,
    OUT gctPOINTER *        Logical,
    OUT gctUINT32   *       Address,
    OUT gcsSURF_NODE_PTR *  Node
    );
/*******************************************************************************
**
**  gcoCL_FlushMemory
**
**  Flush memory to the kernel.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR  Node
**          Pointer to a gcsSURF_NODE structure
**          that describes the video memory to flush.
**
**      gctPOINTER Logical
**          The logical address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes allocated.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_FlushMemory(
    IN gcsSURF_NODE_PTR     Node,
    IN gctPOINTER           Logical,
    IN gctSIZE_T            Bytes
    );

/*******************************************************************************
**
**  gcoCL_InvalidateMemoryCache
**
**  Invalidate memory cache in CPU.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR  Node
**          Pointer to a gcsSURF_NODE structure
**          that describes the video memory to flush.
**
**      gctPOINTER Logical
**          The logical address of the allocation.
**
**      gctSIZE_T Bytes
**          Number of bytes allocated.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_InvalidateMemoryCache(
    IN gcsSURF_NODE_PTR     Node,
    IN gctPOINTER           Logical,
    IN gctSIZE_T            Bytes
    );

/*******************************************************************************
**
**  gcoCL_ShareMemoryWithStream
**
**  Share memory with a stream.
**
**  INPUT:
**
**      gcoSTREAM Stream
**          Pointer to the stream object.
**
**  OUTPUT:
**
**      gctSIZE_T * Bytes
**          Pointer to a variable that will receive the aligned number of bytes
**          allocated.
**
**      gctUINT32_PTR  Physical
**          Pointer to a variable that will receive the gpu virtual addresses of
**          the allocated memory.
**
**      gctPOINTER * Logical
**          Pointer to a variable that will receive the logical address of the
**          allocation.
**
**      gcsSURF_NODE_PTR  * Node
**          Pointer to a variable that will receive the gcsSURF_NODE structure
**          pointer that describes the video memory to lock.
*/
gceSTATUS
gcoCL_ShareMemoryWithStream(
    IN gcoSTREAM            Stream,
    OUT gctSIZE_T *         Bytes,
    OUT gctUINT32_PTR       Physical,
    OUT gctPOINTER *        Logical,
    OUT gcsSURF_NODE_PTR *  Node
    );

/*******************************************************************************
**
**  gcoCL_ShareMemoryWithBufObj
**
**  Share memory with a BufObj.
**
**  INPUT:
**
**      gcoBUFOBJ Stream
**          Pointer to the stream object.
**
**  OUTPUT:
**
**      gctSIZE_T * Bytes
**          Pointer to a variable that will receive the aligned number of bytes
**          allocated.
**
**      gctUINT32_PTR  Physical
**          Pointer to a variable that will receive the gpu virtual addresses of
**          the allocated memory.
**
**      gctPOINTER * Logical
**          Pointer to a variable that will receive the logical address of the
**          allocation.
**
**      gcsSURF_NODE_PTR  * Node
**          Pointer to a variable that will receive the gcsSURF_NODE structure
**          pointer that describes the video memory to lock.
*/
gceSTATUS
gcoCL_ShareMemoryWithBufObj(
    IN gcoBUFOBJ            BufObj,
    OUT gctSIZE_T *         Bytes,
    OUT gctUINT32_PTR       Physical,
    OUT gctPOINTER *        Logical,
    OUT gcsSURF_NODE_PTR *  Node
    );

/*******************************************************************************
**
**  gcoCL_UnshareMemory
**
**  Unshare memory with a stream.
**
**  INPUT:
**
**      gcsSURF_NODE_PTR  Node
**          Pointer to a  gcsSURF_NODE structure
*/
gceSTATUS
gcoCL_UnshareMemory(
    IN gcsSURF_NODE_PTR     Node
    );

/*******************************************************************************
**
**  gcoCL_FlushSurface
**
**  Flush surface to the kernel.
**
**  INPUT:
**
**      gcoSURF           Surface
**          gcoSURF structure
**          that describes the surface to flush.
**
**  OUTPUT:
**
**      Nothing
*/
gceSTATUS
gcoCL_FlushSurface(
    IN gcoSURF              Surface
    );

gceSTATUS
gcoCL_LockSurface(
    IN gcoSURF Surface,
    OUT gctUINT32 * Address,
    OUT gctPOINTER * Memory
    );

gceSTATUS
gcoCL_UnlockSurface(
    IN gcoSURF Surface,
    IN gctPOINTER Memory
    );

/*******************************************************************************
**
**  gcoCL_CreateTexture
**
**  Create texture for image.
**
**  INPUT:
**
**      gctUINT Width
**          Width of the image.
**
**      gctUINT Heighth
**          Heighth of the image.
**
**      gctUINT Depth
**          Depth of the image.
**
**      gctCONST_POINTER Memory
**          Pointer to the data of the input image.
**
**      gctUINT Stride
**          Size of one row.
**
**      gctUINT Slice
**          Size of one plane.
**
**      gceSURF_FORMAT FORMAT
**          Format of the image.
**
**      gceENDIAN_HINT EndianHint
**          Endian needed to handle the image data.
**
**  OUTPUT:
**
**      gcoTEXTURE * Texture
**          Pointer to a variable that will receive the gcoTEXTURE structure.
**
**      gcoSURF * Surface
**          Pointer to a variable that will receive the gcoSURF structure.
**
**      gctUINT32 * Physical
**          Pointer to a variable that will receive the gpu virtual addresses of
**          the allocated memory.
**
**      gctPOINTER * Logical
**          Pointer to a variable that will receive the logical address of the
**          allocation.
**
**      gctUINT * SurfStride
**          Pointer to a variable that will receive the stride of the texture.
*/
gceSTATUS
gcoCL_CreateTexture(
    IN OUT gceIMAGE_MEM_TYPE* MapHostMemory,
    IN gctUINT                Width,
    IN gctUINT                Height,
    IN gctUINT                Depth,
    IN gctCONST_POINTER       Memory,
    IN gctUINT                Stride,
    IN gctUINT                Slice,
    IN gceSURF_FORMAT         Format,
    IN gceENDIAN_HINT         EndianHint,
    OUT gcoTEXTURE *          Texture,
    OUT gcoSURF *             Surface,
    OUT gctUINT32 *           Physical,
    OUT gctPOINTER *          Logical,
    OUT gctUINT *             SurfStride,
    OUT gctUINT *             SurfSliceSize
    );

/*******************************************************************************
**
**  gcoCL_DestroyTexture
**
**  Destroy an gcoTEXTURE object.
**
**  INPUT:
**
**      gcoTEXTURE Texture
**          Pointer to an gcoTEXTURE object.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoCL_DestroyTexture(
    IN gcoTEXTURE Texture,
    IN gcoSURF    Surface
    );

gceSTATUS
gcoCL_SetupTexture(
    IN gcoTEXTURE           Texture,
    IN gcoSURF              Surface,
    IN gctUINT              SamplerNum,
    gceTEXTURE_ADDRESSING   AddressMode,
    gceTEXTURE_FILTER       FilterMode
    );

/*******************************************************************************
**
**  gcoCL_QueryDeviceInfo
**
**  Query the OpenCL capabilities of the device.
**
**  INPUT:
**
**      Nothing
**
**  OUTPUT:
**
**      gcoCL_DEVICE_INFO_PTR DeviceInfo
**          Pointer to the device information
*/
gceSTATUS
gcoCL_QueryDeviceInfo(
    OUT gcoCL_DEVICE_INFO_PTR   DeviceInfo
    );

gceSTATUS
gcoCL_QueryDeviceCount(
    OUT gctUINT32 * DeviceCount,
    OUT gctUINT32 * GPUCountPerDevice
    );

gceSTATUS
gcoCL_QueryDeviceCountWithGPUType(
    OUT gctUINT32 * DeviceCount,
    OUT gctUINT32 * GPUCountPerDevice
    );

gceSTATUS
gcoCL_QueryDeviceCountWithVIPType(
    OUT gctUINT32 * DeviceCount,
    OUT gctUINT32 * GPUCountPerDevice
    );

gceSTATUS
gcoCL_QueryDeviceCountWith3D2DType(
    OUT gctUINT32 * DeviceCount,
    OUT gctUINT32 * GPUCountPerDevice
    );

gceSTATUS
gcoCL_CreateHW(
    IN gctUINT32    DeviceId,
    OUT gcoHARDWARE * Hardware
    );

gceSTATUS
gcoCL_CreateHWWithType(
    IN gceHARDWARE_TYPE hwType,
    IN gctUINT32    DeviceId,
    OUT gcoHARDWARE * Hardware
    );

gceSTATUS
gcoCL_DestroyHW(
    gcoHARDWARE  Hardware
    );

gceSTATUS
gcoCL_GetHWConfigGpuCount(
     gctUINT32 * GpuCount
    );
/*******************************************************************************
**
**  gcoCL_Commit
**
**  Commit the current command buffer to hardware and optionally wait until the
**  hardware is finished.
**
**  INPUT:
**
**      gctBOOL Stall
**          gcvTRUE if the thread needs to wait until the hardware has finished
**          executing the committed command buffer.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoCL_Commit(
    IN gctBOOL Stall
    );

gceSTATUS
gcoCL_Flush(
    IN gctBOOL      Stall
    );

/*******************************************************************************
**
**  gcoCL_CreateSignal
**
**  Create a new signal.
**
**  INPUT:
**
**      gctBOOL ManualReset
**          If set to gcvTRUE, gcoOS_Signal with gcvFALSE must be called in
**          order to set the signal to nonsignaled state.
**          If set to gcvFALSE, the signal will automatically be set to
**          nonsignaled state by gcoOS_WaitSignal function.
**
**  OUTPUT:
**
**      gctSIGNAL * Signal
**          Pointer to a variable receiving the created gctSIGNAL.
*/
gceSTATUS
gcoCL_CreateSignal(
    IN gctBOOL ManualReset,
    OUT gctSIGNAL * Signal
    );

/*******************************************************************************
**
**  gcoCL_DestroySignal
**
**  Destroy a signal.
**
**  INPUT:
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoCL_DestroySignal(
    IN gctSIGNAL Signal
    );

gceSTATUS
gcoCL_SubmitSignal(
    IN gctSIGNAL    Signal,
    IN gctHANDLE    Processs,
    IN gceENGINE    Engine
    );

/*******************************************************************************
**
**  gcoCL_WaitSignal
**
**  Wait for a signal to become signaled.
**
**  INPUT:
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**      gctUINT32 Wait
**          Number of milliseconds to wait.
**          Pass the value of gcvINFINITE for an infinite wait.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoCL_WaitSignal(
    IN gctSIGNAL Signal,
    IN gctUINT32 Wait
    );

/*******************************************************************************
**
**  gcoCL_SetSignal
**
**  Make a signal to become signaled.
**
**  INPUT:
**
**      gctSIGNAL Signal
**          Pointer to the gctSIGNAL.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gcoCL_SetSignal(
    IN gctSIGNAL Signal
    );
/*******************************************************************************
**                                gcoCL_LoadKernel
********************************************************************************
**
**  Load a pre-compiled and pre-linked kernel program into the hardware.
**
**  INPUT:
**
**      gcsPROGRAM_STATE *ProgramState
**          Program state pointer.
*/
gceSTATUS
gcoCL_LoadKernel(
    IN gcsPROGRAM_STATE *ProgramState
    );

gceSTATUS
gcoCL_InvokeKernel(
    IN gctUINT      WorkDim,
    IN size_t       GlobalWorkOffset[3],
    IN size_t       GlobalScale[3],
    IN size_t       GlobalWorkSize[3],
    IN size_t       LocalWorkSize[3],
    IN gctUINT      ValueOrder,
    IN gctBOOL      BarrierUsed,
    IN gctUINT32    MemoryAccessFlag,
    IN gctBOOL      bDual16
    );

gceSTATUS
gcoCL_InvokeThreadWalker(
    IN gcsTHREAD_WALKER_INFO_PTR Info
    );

gceSTATUS
gcoCL_MemBltCopy(
    IN gctUINT32 SrcAddress,
    IN gctUINT32 DestAddress,
    IN gctUINT32 CopySize,
    IN gceENGINE engine
    );

gceSTATUS
gcoCL_MemWaitAndGetFence(
    IN gcsSURF_NODE_PTR Node,
    IN gceENGINE Engine,
    IN gceFENCE_TYPE GetType,
    IN gceFENCE_TYPE WaitType
    );

gceSTATUS
gcoCL_ChooseBltEngine(
    IN gcsSURF_NODE_PTR node,
    OUT gceENGINE * engine
    );

gceSTATUS
gcoCL_IsFeatureAvailable(
    IN gcoHARDWARE Hardware,
    IN gceFEATURE Feature
    );

gceSTATUS
gcoCL_3dBltLock(
    IN gcoHARDWARE Hardware,
    IN gceENGINE Engine,
    IN gctBOOL forceSingle,
    IN gctUINT32_PTR * Memory
    );

gceSTATUS
gcoCL_3dBltUnlock(
    IN gcoHARDWARE Hardware,
    IN gceENGINE Engine,
    IN gctBOOL forceSingle,
    IN gctUINT32_PTR * Memory
    );

gceSTATUS
gcoCL_SubmitCmdBuffer(
    gcoHARDWARE Hardware,
    uint32_t *states,
    uint32_t count);


gctUINT
gcoCL_coreIdToChip(
    gcoHARDWARE Hardware,
    gctUINT coreId);

#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_user_cl_h_ */



/****************************************************************************
*
*    The MIT License (MIT)
*
*    Copyright (c) 2014 - 2020 Vivante Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************
*
*    The GPL License (GPL)
*
*    Copyright (C) 2014 - 2020 Vivante Corporation
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    as published by the Free Software Foundation; either version 2
*    of the License, or (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*****************************************************************************
*
*    Note: This software is released under dual MIT and GPL licenses. A
*    recipient may use this file under the terms of either the MIT license or
*    GPL License. If you wish to use only one license not the other, you can
*    indicate your decision by deleting one of the above license notices in your
*    version of this file.
*
*****************************************************************************/


#include "gc_hal_kernel_precomp.h"
#include "gc_hal_kernel_context.h"
#include "gc_hal_kernel_preemption.h"

#if gcdENABLE_SW_PREEMPTION
#define _GC_OBJ_ZONE    gcvZONE_KERNEL

/*******************************************************************************
**
**  gckKERNEL_DestroyPreemptCommit
**
**  Destroy the preempt commit.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gckPREEMPT_COMMIT PreemptCommit
**          Pointer to an gckPREEMPT_COMMIT object to destroy.
**
**  OUTPUT:
**
**      Nothing.
*/
gceSTATUS
gckKERNEL_DestroyPreemptCommit(
    IN gckKERNEL Kernel,
    IN gckPREEMPT_COMMIT PreemptCommit
    )
{
    gcsHAL_COMMAND_LOCATION *cmdLoc = gcvNULL;
    gcsHAL_COMMAND_LOCATION *nextCmdLoc = gcvNULL;
    gckVIDMEM_NODE nodeObject = gcvNULL;
    gcsQUEUE_PTR eventQueue = gcvNULL;
    gcsQUEUE_PTR nextEventQueue = gcvNULL;
    gceSTATUS status = gcvSTATUS_OK;

    gcmkHEADER_ARG("Kernel=%p PreemptComimt=%p", Kernel, PreemptCommit);

    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(PreemptCommit != gcvNULL);

    cmdLoc = PreemptCommit->cmdLoc;

    while (cmdLoc)
    {
        gcmkVERIFY_OK(gckVIDMEM_HANDLE_Lookup(
            Kernel,
            PreemptCommit->pid,
            cmdLoc->videoMemNode,
            &nodeObject
            ));

        gcmkVERIFY_OK(gckVIDMEM_NODE_UnlockCPU(
            Kernel,
            nodeObject,
            PreemptCommit->pid,
            gcvFALSE,
            gcvFALSE
            ));

        nextCmdLoc = (gcsHAL_COMMAND_LOCATION *)gcmUINT64_TO_PTR(cmdLoc->next);

        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, cmdLoc));

        cmdLoc = nextCmdLoc;
    }

    PreemptCommit->cmdLoc = gcvNULL;

    eventQueue = PreemptCommit->eventQueue;

    while (eventQueue)
    {
        nextEventQueue = (gcsQUEUE_PTR)gcmUINT64_TO_PTR(eventQueue->next);

        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, eventQueue));

        eventQueue = nextEventQueue;
    }

    PreemptCommit->eventQueue = gcvNULL;

    if (PreemptCommit->recordArray)
    {
        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, PreemptCommit->recordArray));
    }

    if (PreemptCommit->mapEntryID)
    {
        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, PreemptCommit->mapEntryID));
    }

    if (PreemptCommit->mapEntryIndex)
    {
        gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, PreemptCommit->mapEntryIndex));
    }

    gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, PreemptCommit));

    gcmkFOOTER();

    return status;
}

/*******************************************************************************
**
**  gckKERNEL_ConstructPreemptCommit
**
**  Construct the preempt commit.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gcsHAL_SUBCOMMIT_PTR SubCommit
**          SubCommit pointer from user driver.
**
**      gceENGINE Engine
**          Engine type of the user commit.
**
**      gctUINT32 ProcessID
**          Proccess ID of this commit.
**
**      gctBOOL Shared
**          If it is multi-core and shared commit.
**
**  OUTPUT:
**      gckPREEMPT_COMMIT * PreemptCommit
**          The gckPREEMPT_COMMIT object to construct.
**
*/
gceSTATUS
gckKERNEL_ConstructPreemptCommit(
    IN gckKERNEL Kernel,
    IN gcsHAL_SUBCOMMIT_PTR SubCommit,
    IN gceENGINE Engine,
    IN gctUINT32 ProcessID,
    IN gctBOOL Shared,
    OUT gckPREEMPT_COMMIT *PreemptCommit
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER pointer = gcvNULL;
    gckCONTEXT context = gcvNULL;
    gctPOINTER userPtr = gcvNULL;
    gctBOOL needCopy = gcvFALSE;
    gctUINT64 next = 0;
    gcsHAL_COMMAND_LOCATION *cmdLoc = gcvNULL;
    gcsHAL_COMMAND_LOCATION *cursor = gcvNULL;
    gcsHAL_COMMAND_LOCATION *cmdLocHead = gcvNULL;
    gcsSTATE_DELTA_PTR uDelta = gcvNULL;
    gcsSTATE_DELTA_PTR kDelta = gcvNULL;
    gcsSTATE_DELTA_RECORD_PTR kRecordArray = gcvNULL;
    gcsQUEUE_PTR uQueue = gcvNULL;
    gcsQUEUE_PTR kQueue = gcvNULL;
    gcsQUEUE_PTR kQueueHead = gcvNULL;
    gcsQUEUE_PTR kQueueTail = gcvNULL;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;
    gckVIDMEM_NODE commandBufferVideoMem = gcvNULL;
    gctPOINTER commandBufferLogical= gcvNULL;
    gctUINT32 dirtyRecordArraySize = 0;

    gcmkHEADER_ARG("Kernel=%p SubCommit=%p Engine=%x ProcessID=%x Shared=%x",
                    Kernel, SubCommit, Engine, ProcessID, Shared);

    /* Verify the arguments. */
    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(SubCommit != gcvNULL);

    gcmkONERROR(gckOS_Allocate(
        Kernel->os,
        gcmSIZEOF(gcsPREEMPT_COMMIT),
        &pointer));

    preemptCommit = (gckPREEMPT_COMMIT)pointer;

    if (SubCommit->context)
    {
        context = gckKERNEL_QueryPointerFromName(
            Kernel,
            (gctUINT32)(SubCommit->context)
            );
    }

    gcmkVERIFY_OK(gckOS_QueryNeedCopy(Kernel->os, ProcessID, &needCopy));

    gcmkONERROR(gckOS_Allocate(
        Kernel->os,
        gcmSIZEOF(gcsHAL_COMMAND_LOCATION),
        &pointer));

    cmdLocHead = (gcsHAL_COMMAND_LOCATION *)pointer;

    cmdLoc = &SubCommit->commandBuffer;

    gcmkONERROR(gckVIDMEM_HANDLE_Lookup(
        Kernel,
        ProcessID,
        cmdLoc->videoMemNode,
        &commandBufferVideoMem
        ));

    gcmkONERROR(gckVIDMEM_NODE_LockCPU(
        Kernel,
        commandBufferVideoMem,
        gcvFALSE,
        gcvFALSE,
        &commandBufferLogical
        ));

    gcmkONERROR(gckOS_MemCopy(cmdLocHead, cmdLoc, gcmSIZEOF(gcsHAL_COMMAND_LOCATION)));

    cmdLocHead->logical = gcmPTR_TO_UINT64(commandBufferLogical);

    cursor = cmdLocHead;

    do
    {
        gcsHAL_COMMAND_LOCATION * user;
        if (userPtr)
        {
            gcmkONERROR(
                gckOS_Allocate(Kernel->os,
                               gcmSIZEOF(gcsHAL_COMMAND_LOCATION),
                               &pointer));

            cmdLoc = (gcsHAL_COMMAND_LOCATION *)pointer;

            if (needCopy)
            {
                status = gckOS_CopyFromUserData(
                    Kernel->os,
                    cmdLoc,
                    userPtr,
                    gcmSIZEOF(gcsHAL_COMMAND_LOCATION)
                    );
            }
            else
            {
                status = gckOS_MapUserPointer(
                    Kernel->os,
                    userPtr,
                    gcmSIZEOF(gcsHAL_COMMAND_LOCATION),
                    (gctPOINTER *)&cmdLoc
                    );
            }

            if (gcmIS_ERROR(status))
            {
                userPtr = gcvNULL;
                gcmkONERROR(status);
            }

            gcmkONERROR(gckVIDMEM_HANDLE_Lookup(
                Kernel,
                ProcessID,
                cmdLoc->videoMemNode,
                &commandBufferVideoMem
                ));

            gcmkONERROR(gckVIDMEM_NODE_LockCPU(
                Kernel,
                commandBufferVideoMem,
                gcvFALSE,
                gcvFALSE,
                &commandBufferLogical
                ));

            cursor->next = gcmPTR_TO_UINT64(cmdLoc);
            cursor = (gcsHAL_COMMAND_LOCATION *)gcmUINT64_TO_PTR(cursor->next);
            cursor->logical = gcmPTR_TO_UINT64(commandBufferLogical);
        }

        next = cmdLoc->next;

        if (!needCopy && userPtr)
        {
            gcmkVERIFY_OK(gckOS_UnmapUserPointer(
                Kernel->os,
                userPtr,
                gcmSIZEOF(gcsHAL_COMMAND_LOCATION),
                cmdLoc
                ));
        }

        userPtr = gcmUINT64_TO_PTR(next);
        user = (gcsHAL_COMMAND_LOCATION *)userPtr;
    }
    while (userPtr);

    if (SubCommit->delta)
    {
        uDelta = gcmUINT64_TO_PTR(SubCommit->delta);

        gcmkONERROR(gckKERNEL_OpenUserData(
            Kernel, needCopy,
            &preemptCommit->sDelta,
            uDelta, gcmSIZEOF(gcsSTATE_DELTA),
            (gctPOINTER *)&kDelta
            ));

        dirtyRecordArraySize
            = gcmSIZEOF(gcsSTATE_DELTA_RECORD) * kDelta->recordCount;

        if (dirtyRecordArraySize)
        {
            gcmkONERROR(gckOS_Allocate(
                Kernel->os,
                gcmSIZEOF(gcsSTATE_DELTA_RECORD) * dirtyRecordArraySize,
                &pointer
                ));

            preemptCommit->recordArray = (gcsSTATE_DELTA_RECORD_PTR)pointer;

            gcmkONERROR(gckKERNEL_OpenUserData(
                Kernel, needCopy,
                preemptCommit->recordArray,
                gcmUINT64_TO_PTR(kDelta->recordArray),
                dirtyRecordArraySize,
                (gctPOINTER *) &kRecordArray
                ));

            if (kRecordArray == gcvNULL)
            {
                gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            gcmkONERROR(gckKERNEL_CloseUserData(
                Kernel, needCopy,
                gcvFALSE,
                gcmUINT64_TO_PTR(kDelta->recordArray),
                dirtyRecordArraySize,
                (gctPOINTER *) &kRecordArray
                ));

        }
        else
        {
            preemptCommit->recordArray = gcvNULL;
        }

        kDelta->recordArray = gcmPTR_TO_UINT64(preemptCommit->recordArray);

        if (context && context->maxState > 0)
        {
            gctSIZE_T bytes = gcmSIZEOF(gctUINT) * context->maxState;
            gctUINT32 *kMapEntryID = gcvNULL;
            gctUINT32 *kMapEntryIndex = gcvNULL;

            gcmkONERROR(gckOS_Allocate(
                Kernel->os, bytes, &pointer
                ));

            preemptCommit->mapEntryID = (gctUINT32 *)pointer;

            kDelta->mapEntryIDSize = (gctUINT32)bytes;

            gcmkONERROR(gckKERNEL_OpenUserData(
                Kernel, needCopy,
                preemptCommit->mapEntryID,
                gcmUINT64_TO_PTR(kDelta->mapEntryID),
                bytes,
                (gctPOINTER *) &kMapEntryID
                ));

            if (kMapEntryID == gcvNULL)
            {
                gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            gcmkONERROR(gckKERNEL_CloseUserData(
                Kernel, needCopy,
                gcvFALSE,
                gcmUINT64_TO_PTR(kDelta->mapEntryID),
                bytes,
                (gctPOINTER *) &kMapEntryID
                ));

            kDelta->mapEntryID = gcmPTR_TO_UINT64(preemptCommit->mapEntryID);

            gcmkONERROR(gckOS_Allocate(
                Kernel->os, bytes, &pointer
                ));

            preemptCommit->mapEntryIndex = (gctUINT32 *)pointer;

            gcmkONERROR(gckKERNEL_OpenUserData(
                Kernel, needCopy,
                preemptCommit->mapEntryIndex,
                gcmUINT64_TO_PTR(kDelta->mapEntryIndex),
                bytes,
                (gctPOINTER *) &kMapEntryIndex
                ));

            if (kMapEntryIndex == gcvNULL)
            {
                gcmkONERROR(gcvSTATUS_INVALID_ARGUMENT);
            }

            gcmkONERROR(gckKERNEL_CloseUserData(
                Kernel, needCopy,
                gcvFALSE,
                gcmUINT64_TO_PTR(kDelta->mapEntryIndex),
                bytes,
                (gctPOINTER *) &kMapEntryIndex
                ));

            kDelta->mapEntryIndex = gcmPTR_TO_UINT64(preemptCommit->mapEntryIndex);
        }

        preemptCommit->delta = kDelta;

        gcmkONERROR(gckKERNEL_CloseUserData(
            Kernel, needCopy,
            gcvFALSE,
            uDelta, gcmSIZEOF(gcsSTATE_DELTA),
            (gctPOINTER *) &kDelta
            ));
    }

    uQueue = gcmUINT64_TO_PTR(SubCommit->queue);
    if (uQueue != gcvNULL)
    {
        gcsQUEUE_PTR sEventQueue = gcvNULL;
        gcsQUEUE_PTR next = gcvNULL;

        gcmkONERROR(
            gckOS_Allocate(Kernel->os,
                           gcmSIZEOF(gcsQUEUE),
                           &pointer));

        sEventQueue = (gcsQUEUE_PTR)pointer;

        gcmkONERROR(gckKERNEL_OpenUserData(
            Kernel, needCopy,
            sEventQueue,
            uQueue, gcmSIZEOF(gcsQUEUE),
            (gctPOINTER *)&kQueue
            ));

        next = (gcsQUEUE_PTR)gcmUINT64_TO_PTR(kQueue->next);

        if (!needCopy && uQueue)
        {
            gcmkVERIFY_OK(gckOS_UnmapUserPointer(
                Kernel->os,
                uQueue,
                gcmSIZEOF(gcsQUEUE),
                (gctPOINTER *)&kQueue
                ));
        }

        uQueue = next;
        kQueueHead = kQueueTail = kQueue;

        while (uQueue != gcvNULL)
        {
            gcmkONERROR(
                gckOS_Allocate(Kernel->os,
                               gcmSIZEOF(gcsQUEUE),
                               &pointer));

            sEventQueue = (gcsQUEUE_PTR)pointer;

            gcmkONERROR(gckKERNEL_OpenUserData(
                Kernel, needCopy,
                sEventQueue,
                uQueue, gcmSIZEOF(gcsQUEUE),
                (gctPOINTER *)&kQueue
                ));

            next = (gcsQUEUE_PTR)gcmUINT64_TO_PTR(kQueue->next);

            if (!needCopy && uQueue)
            {
                gcmkVERIFY_OK(gckOS_UnmapUserPointer(
                    Kernel->os,
                    uQueue,
                    gcmSIZEOF(gcsQUEUE),
                    (gctPOINTER *)&kQueue
                    ));
            }

            uQueue = next;
            kQueueTail->next = gcmPTR_TO_UINT64(kQueue);
            kQueueTail = kQueue;
        }
    }

    preemptCommit->engine = Engine;
    preemptCommit->pid    = ProcessID;
    preemptCommit->shared = Shared;
    preemptCommit->cmdLoc = cmdLocHead;

    preemptCommit->dirtyRecordArraySize = dirtyRecordArraySize;

    preemptCommit->context     = context;
    preemptCommit->eventQueue  = kQueueHead;
    preemptCommit->eventOnly   = gcvFALSE;
    preemptCommit->priorityID  = SubCommit->priorityID;
    preemptCommit->next        = gcvNULL;
    preemptCommit->isEnd       = gcvFALSE;
    preemptCommit->isNop       = gcvFALSE;

    *PreemptCommit = preemptCommit;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (preemptCommit != gcvNULL)
    {
        gcmkVERIFY_OK(gckKERNEL_DestroyPreemptCommit(Kernel, preemptCommit));
    }

    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_PreparePreemptEvent
**
**  Prepare the preempt event.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gcsQUEUE_PTR Queue
**          Pointer to the user commit event queue.
**
**      gctUINT32 PriorityID
**          Priority ID of this event.
**
**      gctUINT32 ProcessID
**          Proccess ID of this event.
**
**  OUTPUT:
**      gckPREEMPT_COMMIT * PreemptCommit
**          The gckPREEMPT_COMMIT object to construct.
**
*/
gceSTATUS
gckKERNEL_PreparePreemptEvent(
    IN gckKERNEL Kernel,
    IN gcsQUEUE_PTR Queue,
    IN gctUINT32 PriorityID,
    IN gctUINT32 ProcessID,
    OUT gckPREEMPT_COMMIT *PreemptCommit
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER pointer = gcvNULL;
    gctBOOL needCopy = gcvFALSE;
    gcsQUEUE_PTR uQueue = Queue;
    gcsQUEUE_PTR kQueue = gcvNULL;
    gcsQUEUE_PTR kQueueHead = gcvNULL;
    gcsQUEUE_PTR record = gcvNULL;
    gctSIGNAL signal = gcvNULL;
    gcsQUEUE_PTR kQueueTail = gcvNULL;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p Queue=%p PriorityID=%x ProcessID=%x",
                    Kernel, Queue, ProcessID);

    gcmkONERROR(gckOS_Allocate(
        Kernel->os,
        gcmSIZEOF(gcsPREEMPT_COMMIT),
        &pointer));

    preemptCommit = (gckPREEMPT_COMMIT)pointer;

    gcmkVERIFY_OK(gckOS_QueryNeedCopy(Kernel->os, ProcessID, &needCopy));

    if (uQueue)
    {
        gcsQUEUE_PTR sEventQueue = gcvNULL;
        gcsQUEUE_PTR next = gcvNULL;

        gcmkONERROR(
            gckOS_Allocate(Kernel->os,
                           gcmSIZEOF(gcsQUEUE),
                           &pointer));

        sEventQueue = (gcsQUEUE_PTR)pointer;

        gcmkONERROR(gckKERNEL_OpenUserData(
            Kernel, needCopy,
            sEventQueue,
            uQueue, gcmSIZEOF(gcsQUEUE),
            (gctPOINTER *)&kQueue
            ));

        next = (gcsQUEUE_PTR)gcmUINT64_TO_PTR(kQueue->next);

        if (!needCopy && uQueue)
        {
            gcmkVERIFY_OK(gckOS_UnmapUserPointer(
                Kernel->os,
                uQueue,
                gcmSIZEOF(gcsQUEUE),
                (gctPOINTER *)&kQueue
                ));
        }

        uQueue = next;
        kQueueHead = kQueueTail = kQueue;

        while (uQueue != gcvNULL)
        {
            gcmkONERROR(
                gckOS_Allocate(Kernel->os,
                               gcmSIZEOF(gcsQUEUE),
                               &pointer));

            sEventQueue = (gcsQUEUE_PTR)pointer;

            gcmkONERROR(gckKERNEL_OpenUserData(
                Kernel, needCopy,
                sEventQueue,
                uQueue, gcmSIZEOF(gcsQUEUE),
                (gctPOINTER *)&kQueue
                ));

            next = (gcsQUEUE_PTR)gcmUINT64_TO_PTR(kQueue->next);

            if (!needCopy && uQueue)
            {
                gcmkVERIFY_OK(gckOS_UnmapUserPointer(
                    Kernel->os,
                    uQueue,
                    gcmSIZEOF(gcsQUEUE),
                    (gctPOINTER *)&kQueue
                    ));
            }

            uQueue = next;
            kQueueTail->next = gcmPTR_TO_UINT64(kQueue);
            kQueueTail = kQueue;
        }
    }

    preemptCommit->eventQueue = kQueueHead;

    record = preemptCommit->eventQueue;

    while (record != gcvNULL)
    {
        signal = gcmUINT64_TO_PTR(record->iface.u.Signal.signal);

        if (record->iface.u.Signal.fenceSignal && gcmUINT64_TO_PTR(record->iface.u.Signal.process))
        {
            /* User signal. */
            gcmkONERROR(gckOS_UserSignal(
                Kernel->os,
                signal,
                gcmUINT64_TO_PTR(record->iface.u.Signal.process)
                ));
        }


        /* Next record in the queue. */
        record = gcmUINT64_TO_PTR(record->next);
    }

    preemptCommit->priorityID  = PriorityID;
    preemptCommit->eventOnly   = gcvTRUE;
    preemptCommit->pid         = ProcessID;
    preemptCommit->next        = gcvNULL;
    preemptCommit->isEnd       = gcvFALSE;
    preemptCommit->isNop       = gcvFALSE;

    preemptCommit->cmdLoc        = gcvNULL;
    preemptCommit->mapEntryID    = gcvNULL;
    preemptCommit->mapEntryIndex = gcvNULL;
    preemptCommit->recordArray   = gcvNULL;
    preemptCommit->delta         = gcvNULL;
    preemptCommit->context       = gcvNULL;

    *PreemptCommit = preemptCommit;

    gcmkFOOTER_NO();
    return gcvSTATUS_OK;

OnError:
    if (preemptCommit != gcvNULL)
    {
        gcmkVERIFY_OK(gckKERNEL_DestroyPreemptCommit(Kernel, preemptCommit));
    }

    gcmkFOOTER();
    return status;
}

/* Construct a NOP preemptCommit which means it is the end commit. */
gceSTATUS
gckKERNEL_PreemptCommitDone(
    IN gckKERNEL Kernel,
    IN gctUINT32 PriorityID,
    OUT gckPREEMPT_COMMIT *PreemptCommit
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p", Kernel);

    gcmkONERROR(gckOS_Allocate(
        Kernel->os,
        gcmSIZEOF(gcsPREEMPT_COMMIT),
        (gctPOINTER *)&preemptCommit));

    gckOS_ZeroMemory(preemptCommit, sizeof(gcsPREEMPT_COMMIT));

    preemptCommit->priorityID = PriorityID;

    preemptCommit->isEnd = gcvTRUE;

    *PreemptCommit = preemptCommit;

OnError:
    gcmkFOOTER();
    return status;
}

/* Construct a NOP preemptCommit to block all the lower priority queues. */
gceSTATUS
gckKERNEL_PreemptCommitNop(
    IN gckKERNEL Kernel,
    IN gctUINT32 PriorityID,
    OUT gckPREEMPT_COMMIT *PreemptCommit
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p", Kernel);

    gcmkONERROR(gckOS_Allocate(
        Kernel->os,
        gcmSIZEOF(gcsPREEMPT_COMMIT),
        (gctPOINTER *)&preemptCommit));

    gckOS_ZeroMemory(preemptCommit, sizeof(gcsPREEMPT_COMMIT));

    preemptCommit->priorityID = PriorityID;

    preemptCommit->isNop = gcvTRUE;

    *PreemptCommit = preemptCommit;

OnError:
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_PriorityQueueConstruct
**
**  Construct the priority queue.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gctUINT32 PriorityID
**          Priority ID of this queue.
**
**  OUTPUT:
**      gcsPRIORITY_QUEUE_PTR
**          The priority queue to construct.
**
*/
gceSTATUS
gckKERNEL_PriorityQueueConstruct(
    gckKERNEL Kernel,
    gctUINT PriorityID,
    gcsPRIORITY_QUEUE_PTR * Queue
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctPOINTER pointer = gcvNULL;
    gcsPRIORITY_QUEUE_PTR queue;

    gcmkHEADER_ARG("Kernel=%p PriorityID=%d", Kernel, PriorityID);

    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(Queue != gcvNULL);

    gcmkONERROR(
        gckOS_Allocate(Kernel->os,
                       gcmSIZEOF(gcsPRIORITY_QUEUE),
                       &pointer));

    queue = pointer;

    queue->id = PriorityID;
    queue->head = gcvNULL;
    queue->tail = gcvNULL;

    *Queue = queue;

OnError:
    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_PriorityQueueDestroy
**
**  Destroy the priority queue.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gcsPRIORITY_QUEUE_PTR
**          The priority queue to destroy.
**
*/
gceSTATUS
gckKERNEL_PriorityQueueDestroy(
    gckKERNEL Kernel,
    gcsPRIORITY_QUEUE_PTR Queue
    )
{
    gceSTATUS status = gcvSTATUS_OK;

    gcmkHEADER_ARG("Kernel=%p", Kernel);

    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(Queue != gcvNULL);

    Kernel->priorityQueues[Queue->id] = gcvNULL;

    gcmkVERIFY_OK(gcmkOS_SAFE_FREE(Kernel->os, Queue));

    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_PriorityQueueAppend
**
**  Append preempt commit to the priority queue.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gctUINT32 PriorityID
**          Priority ID of this queue.
**
**      gckPREEMPT_COMMIT
**          The preempt commit.
**
*/
gceSTATUS
gckKERNEL_PriorityQueueAppend(
    IN gckKERNEL Kernel,
    IN gctUINT PriorityID,
    IN gckPREEMPT_COMMIT PreemptCommit
    )
{
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL acquired = gcvFALSE;
    gctBOOL constructed = gcvFALSE;
    gcsPRIORITY_QUEUE_PTR queue = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p PriorityID=%d PreemptCommit=%p", Kernel, PriorityID, PreemptCommit);

    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(PreemptCommit != gcvNULL);
    gcmkVERIFY_ARGUMENT(PriorityID < gcdMAX_PRIORITY_QUEUE_NUM);

    gcmkONERROR(
        gckOS_AcquireMutex(Kernel->os,
                           Kernel->priorityQueueMutex[PriorityID],
                           gcvINFINITE));
    acquired = gcvTRUE;

    queue = Kernel->priorityQueues[PriorityID];

    if (!queue)
    {
        gcmkONERROR(gckKERNEL_PriorityQueueConstruct(Kernel, PriorityID, &queue));

        Kernel->priorityQueues[PriorityID] = queue;

        constructed = gcvTRUE;
    }

    if (queue->head == gcvNULL)
    {
        queue->head = PreemptCommit;
        queue->tail = PreemptCommit;
    }
    else
    {
        queue->tail->next = PreemptCommit;
        queue->tail       = PreemptCommit;
    }

    gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[PriorityID]));


    gcmkFOOTER_NO();

    return gcvSTATUS_OK;

OnError:
    if (acquired)
    {
        gcmkVERIFY_OK(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[PriorityID]));
    }

    if (constructed && queue)
    {
        gcmkVERIFY_OK(gckKERNEL_PriorityQueueDestroy(Kernel, queue));
    }

    gcmkFOOTER();
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_PriorityQueueRemove
**
**  Remove preempt commit from the priority queue.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gcsPRIORITY_QUEUE_PTR
**          The priority queue to append.
**
**  OUTPUT:
**      gckPREEMPT_COMMIT *
**          The preempt commit.
**
*/
gceSTATUS
gckKERNEL_PriorityQueueRemove(
    IN gckKERNEL Kernel,
    IN gcsPRIORITY_QUEUE_PTR Queue,
    OUT gckPREEMPT_COMMIT * PreemptCommit
    )
{
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p Queue=%p PreemptCommit=%p", Kernel, Queue, PreemptCommit);

    /* Verify the arguments. */
    gcmkVERIFY_OBJECT(Kernel, gcvOBJ_KERNEL);
    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(PreemptCommit != gcvNULL);

    if (Queue->head != gcvNULL)
    {
        preemptCommit = Queue->head;
        Queue->head = preemptCommit->next;
    }

    *PreemptCommit = preemptCommit;

    gcmkFOOTER_NO();

    return gcvSTATUS_OK;
}

gceSTATUS
gckKERNEL_NormalPreemption(
    gckKERNEL Kernel
    )
{
    gcsPRIORITY_QUEUE_PTR queue = gcvNULL;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;
    gceSTATUS status = gcvSTATUS_OK;
    gctBOOL queueAvailable;
    gctINT id;

    for (id = gcdMAX_PRIORITY_QUEUE_NUM - 1; id >= 0; id--)
    {
        gcmkONERROR(gckOS_AcquireMutex(Kernel->os, Kernel->priorityQueueMutex[id], gcvINFINITE));

        queueAvailable = gcvFALSE;

        queue = Kernel->priorityQueues[id];
        if (!queue || !queue->head)
        {
            gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[id]));
            continue;
        }
        else
        {
            gcmkONERROR(gckKERNEL_PriorityQueueRemove(Kernel, queue, &preemptCommit));

            if (preemptCommit)
            {
                gctBOOL forced;

                if (!preemptCommit->eventOnly)
                {
                    if (preemptCommit->context)
                    {
                        gcmkONERROR(gckCONTEXT_UpdateDelta(preemptCommit->context, preemptCommit->delta));
                    }

                    status = gckCOMMAND_PreemptCommit(Kernel->command, preemptCommit);

                    if (status != gcvSTATUS_INTERRUPTED)
                    {
                        gcmkONERROR(status);
                    }

                    forced = Kernel->hardware->options.powerManagement;
                }
                else
                {
                    forced = gcvFALSE;
                }

                status = gckEVENT_PreemptCommit(Kernel->eventObj,
                                                preemptCommit,
                                                forced);

                if (status != gcvSTATUS_INTERRUPTED)
                {
                    gcmkONERROR(status);
                }

                gcmkONERROR(gckKERNEL_DestroyPreemptCommit(Kernel, preemptCommit));
            }

        }

        gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[id]));
    }

OnError:
    return status;
}

gceSTATUS
gckKERNEL_FullPreemption(
    gckKERNEL Kernel
    )
{
    gcsPRIORITY_QUEUE_PTR queue = gcvNULL;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;
    gceSTATUS status = gcvSTATUS_OK;
    gctINT32 curHighestPriorityID = 0;
    gctINT id;

    for (id = gcdMAX_PRIORITY_QUEUE_NUM - 1; id >= 0; id--)
    {
        gcmkONERROR(gckOS_AcquireMutex(Kernel->os, Kernel->priorityQueueMutex[id], gcvINFINITE));

        queue = Kernel->priorityQueues[id];
        if (!queue || !queue->head)
        {
            gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[id]));
            continue;
        }

        do
        {
            gcmkVERIFY_OK(gckOS_AtomGet(Kernel->os, Kernel->device->atomPriorityID, &curHighestPriorityID));
            if (id < curHighestPriorityID)
            {
                gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[id]));
                return gcvSTATUS_OK;
            }

            gcmkONERROR(gckKERNEL_PriorityQueueRemove(Kernel, queue, &preemptCommit));

            if (preemptCommit && !preemptCommit->isNop)
            {
                gctBOOL forced;

                if (!preemptCommit->eventOnly)
                {
                    if (preemptCommit->context)
                    {
                        if (preemptCommit->context->prevDeltaPtr)
                        {
                            gcmkONERROR(gckCONTEXT_UpdateDelta(preemptCommit->context, preemptCommit->context->prevDeltaPtr));
                            gcmkONERROR(gckCONTEXT_DestroyPrevDelta(preemptCommit->context));
                        }

                        gcmkONERROR(gckCONTEXT_UpdateDelta(preemptCommit->context, preemptCommit->delta));
                    }

                    status = gckCOMMAND_PreemptCommit(Kernel->command, preemptCommit);

                    if (status != gcvSTATUS_INTERRUPTED)
                    {
                        gcmkONERROR(status);
                    }

                    forced = Kernel->hardware->options.powerManagement;
                }
                else
                {
                    forced = gcvFALSE;
                }

                status = gckEVENT_PreemptCommit(Kernel->eventObj,
                                                preemptCommit,
                                                forced);

                if (status != gcvSTATUS_INTERRUPTED)
                {
                    gcmkONERROR(status);
                }

                gcmkONERROR(gckKERNEL_DestroyPreemptCommit(Kernel, preemptCommit));
            }

        } while (queue->head != gcvNULL);

        gcmkONERROR(gckOS_ReleaseMutex(Kernel->os, Kernel->priorityQueueMutex[id]));
    }

OnError:
    return status;
}

gceSTATUS
gckKERNEL_PreemptionThread(
    gckKERNEL Kernel
    )
{
    gceSTATUS status;

    if (Kernel->preemptionMode == gcvFULLY_PREEMPTIBLE_MODE)
    {
        gcmkONERROR(gckKERNEL_FullPreemption(Kernel));
    }
    else
    {
        gcmkONERROR(gckKERNEL_NormalPreemption(Kernel));
    }

OnError:
    return status;
}

/*******************************************************************************
**
**  gckKERNEL_CommandCommitPreemption
**
**  Commit the command with preemption.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gceENGINE Engine
**          Engine type of the user commit.
**
**      gctUINT32 ProcessID
**          Proccess ID of this commit.
**
**      gckCOMMAND Command
**          Kernel command queue.
**
**      gckEVENT EventObj
**          Event object.
**
**      gcsHAL_SUBCOMMIT * SubCommit
**          SubCommit pointer from user driver.
**
**      gcsHAL_COMMIT * Commit
**          Commit pointer from user driver.
**
*/
gceSTATUS
gckKERNEL_CommandCommitPreemption(
    IN gckKERNEL Kernel,
    IN gceENGINE Engine,
    IN gctUINT32 ProcessID,
    IN gckCOMMAND Command,
    IN gckEVENT EventObj,
    IN gcsHAL_SUBCOMMIT * SubCommit,
    IN OUT gcsHAL_COMMIT * Commit
    )
{
    gctUINT32 priorityID = 0;
    gctUINT32 curHighestPriorityID = 0;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;
    gceSTATUS status = gcvSTATUS_OK;

    gcmkHEADER_ARG("Kernel=%p Engine=%x ProcessID=%x Command=%p, EventObj=%p SubCommit=%p Commit=%p",
                    Kernel, Engine, ProcessID, Command, EventObj, SubCommit, Commit);

    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(Command != gcvNULL);
    gcmkVERIFY_ARGUMENT(SubCommit != gcvNULL);
    gcmkVERIFY_ARGUMENT(Commit != gcvNULL);

    priorityID = SubCommit->priorityID;

    if (Kernel->preemptionMode == gcvFULLY_PREEMPTIBLE_MODE)
    {
        Commit->needMerge = gcvTRUE;
        Commit->pending   = gcvFALSE;

        gcmkVERIFY_OK(gckOS_AtomGet(Kernel->os, Kernel->device->atomPriorityID, (gctINT32_PTR)&curHighestPriorityID));

        if (SubCommit->topPriority && priorityID != curHighestPriorityID)
        {
            priorityID = curHighestPriorityID;
        }

        if (!Kernel->priorityDBCreated[priorityID])
        {
            gcmkONERROR(
                gckKERNEL_AddProcessDB(Kernel,
                                       ProcessID,
                                       gcvDB_PRIORITY,
                                       gcmINT2PTR(priorityID),
                                       gcvNULL,
                                       0));
        }

        if (priorityID < curHighestPriorityID ||
            (priorityID == curHighestPriorityID && Kernel->priorityQueues[priorityID]
            && Kernel->priorityQueues[priorityID]->head))
        {

            gcmkONERROR(
                gckKERNEL_ConstructPreemptCommit(Kernel,
                                                 SubCommit,
                                                 Engine,
                                                 ProcessID,
                                                 Commit->shared,
                                                 &preemptCommit));

            gcmkONERROR(
                gckKERNEL_PriorityQueueAppend(Kernel,
                                              priorityID,
                                              preemptCommit));

            Commit->commitStamp = Command->commitStamp++;
            Commit->needMerge = gcvFALSE;
            Commit->pending   = gcvTRUE;

            if (priorityID == curHighestPriorityID)
            {
                gcmkONERROR(gckOS_ReleaseSemaphoreEx(Kernel->os, Kernel->preemptSema));
            }
        }
        else
        {
            gcmkVERIFY_OK(gckOS_AtomSet(Kernel->os, Kernel->device->atomPriorityID, priorityID));

            status = gckCOMMAND_Commit(Command,
                                       SubCommit,
                                       ProcessID,
                                       Commit->shared,
                                       &Commit->commitStamp,
                                       &Commit->contextSwitched);

            if (status != gcvSTATUS_INTERRUPTED)
            {
                gcmkONERROR(status);
            }

            status = gckEVENT_Commit(
                EventObj,
                gcmUINT64_TO_PTR(SubCommit->queue),
                Kernel->hardware->options.powerManagement
                );

            if (status != gcvSTATUS_INTERRUPTED)
            {
                gcmkONERROR(status);
            }
        }
    }
    else
    {
        gcmkONERROR(
            gckKERNEL_ConstructPreemptCommit(Kernel,
                                             SubCommit,
                                             Engine,
                                             ProcessID,
                                             Commit->shared,
                                             &preemptCommit));

        gcmkONERROR(
            gckKERNEL_PriorityQueueAppend(Kernel,
                                          priorityID,
                                          preemptCommit));

        Commit->commitStamp = Command->commitStamp++;
        Commit->needMerge = gcvFALSE;

        gcmkONERROR(gckOS_ReleaseSemaphore(Kernel->os, Kernel->preemptSema));
    }

OnError:
    gcmkFOOTER();

    return status;
}

/*******************************************************************************
**
**  gckKERNEL_EventCommitPreemption
**
**  Commit the event with preemption.
**
**  INPUT:
**
**      gckKERNEL Kernel
**          Pointer to an gckKERNEL.
**
**      gceENGINE Engine
**          Engine type of the user commit.
**
**      gctUINT32 ProcessID
**          Proccess ID of this commit.
**
**      gcsQUEUE_PTR Queue
**          Event queue of the user commit.
**
**      gctUINT32 PriorityID
**          Priority ID of this commit.
**
**      gctBOOL TopPriority
**          If this commit requires top priority.
**
*/
gceSTATUS
gckKERNEL_EventCommitPreemption(
    IN gckKERNEL Kernel,
    IN gceENGINE Engine,
    IN gctUINT32 ProcessID,
    IN gcsQUEUE_PTR Queue,
    IN gctUINT32 PriorityID,
    IN gctBOOL TopPriority
    )
{
    gctUINT32 priorityID = PriorityID;
    gctUINT32 curHighestPriorityID = 0;
    gceSTATUS status = gcvSTATUS_OK;
    gckPREEMPT_COMMIT preemptCommit = gcvNULL;

    gcmkHEADER_ARG("Kernel=%p Engine=%x ProcessID=%x Queue=%p, PriorityID=%x",
                    Kernel, Engine, ProcessID, Queue, PriorityID);

    gcmkVERIFY_ARGUMENT(Kernel != gcvNULL);
    gcmkVERIFY_ARGUMENT(Queue != gcvNULL);

    if (Kernel->preemptionMode == gcvFULLY_PREEMPTIBLE_MODE)
    {
        gcmkVERIFY_OK(gckOS_AtomGet(Kernel->os, Kernel->device->atomPriorityID, (gctINT32_PTR)&curHighestPriorityID));

        if (TopPriority && priorityID != curHighestPriorityID)
        {
            priorityID = curHighestPriorityID;
        }

        if (!Kernel->priorityDBCreated[priorityID])
        {
            gcmkONERROR(
                gckKERNEL_AddProcessDB(Kernel,
                                       ProcessID,
                                       gcvDB_PRIORITY,
                                       gcmINT2PTR(priorityID),
                                       gcvNULL,
                                       0));
        }

        if (priorityID < curHighestPriorityID ||
            (priorityID == curHighestPriorityID && Kernel->priorityQueues[priorityID]
            && Kernel->priorityQueues[priorityID]->head))
        {
            gcmkONERROR(
                gckKERNEL_PreparePreemptEvent(Kernel,
                                              Queue,
                                              priorityID,
                                              ProcessID,
                                              &preemptCommit));

            gcmkONERROR(
                gckKERNEL_PriorityQueueAppend(Kernel,
                                              priorityID,
                                              preemptCommit));

            if (priorityID == curHighestPriorityID)
            {
                gcmkONERROR(gckOS_ReleaseSemaphoreEx(Kernel->os, Kernel->preemptSema));
            }
        }
        else
        {
            gckEVENT eventObj = gcvNULL;

            gcmkVERIFY_OK(gckOS_AtomSet(Kernel->os, Kernel->device->atomPriorityID, priorityID));

            if (Engine == gcvENGINE_BLT)
            {
                if (!gckHARDWARE_IsFeatureAvailable(Kernel->hardware, gcvFEATURE_ASYNC_BLIT))
                {
                    gcmkONERROR(gcvSTATUS_NOT_SUPPORTED);
                }

                eventObj = Kernel->asyncEvent;
            }
            else
            {
                eventObj = Kernel->eventObj;
            }

            gcmkONERROR(
                gckEVENT_Commit(eventObj,
                                Queue,
                                gcvFALSE));

        }
    }
    else
    {
        gcmkONERROR(
            gckKERNEL_PreparePreemptEvent(Kernel,
                                          Queue,
                                          priorityID,
                                          ProcessID,
                                          &preemptCommit));

        gcmkONERROR(
            gckKERNEL_PriorityQueueAppend(Kernel,
                                          priorityID,
                                          preemptCommit));

        gcmkONERROR(gckOS_ReleaseSemaphore(Kernel->os, Kernel->preemptSema));
    }

OnError:
    gcmkFOOTER();

    return status;
}
#endif


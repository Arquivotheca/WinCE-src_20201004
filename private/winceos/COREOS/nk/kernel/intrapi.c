//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/**     TITLE("Interrupt APIs")
 *++
 *
 *
 * Module Name:
 *
 *    intrapi.c
 *
 * Abstract:
 *
 *  This file contains the interrupt API functions. These functions allow
 * user mode threads to receive interrupt notifications and to interact with
 * the firmware interrupt management routines.
 *--
 */
#include "kernel.h"

void NKAcquireOalSpinLock (void);
void NKReleaseOalSpinLock (void);

/*
    @doc    EXTERNAL KERNEL HAL

    @topic Kernel Interrupt Support | 
          The interrupt model for the 
          kernel is that all interrupts get vectored into the kernel. To handle 
          an interrupt, the OEM would typically install both an ISR (the actual 
          interrupt service routine), and an IST(Interrupt service thread). All 
          the work is mostly done in the IST. To 
          install an ISR which should be invoked on 
          a particular hardware interrupt, the OEM code should call the 
          function <f HookInterrupt>. This will cause the kernel to jump into 
          the registered function on that interrupt - with as small an overhead 
          as possible - essentially just handfuls of assembly instructions.

          These ISR's however, are a bit different from traditions ISR's in 
          systems like DOS or Windows. They are expected to be *extremely* 
          light weight and fast - not executing more than  a handful of 
          assembly instructions. The reason is that these routines have lots if 
          limitations - they execute with all interrupts turned off, they 
          cannot call any kernel functions at all, they cannot use any stack, 
          and they cant even use all of the registers. Above all they cannot 
          cause any nested exception. What they can do is return back to the 
          kernel with a return value which either tells the kernel that the 
          interrupt has been handled (NOP), or asks it to schedule a specific 
          IST by returning the interrupt code corresponding to it. See 
          <l Interrupt ID's.Interrupt ID's> for details of the return 
          codes.
   
          If the ISR returns a NOP, the kernel gets out of the exception handler
          right away. This means 
          the ISR should have done everything required - for instance, signal 
          an end of interrupt if necessary. If an IST is to be scheduled, it 
          calls <l SetEvent.SetEvent> on the registered event and then gets out of the 
          handler.

          To register an IST with the kernel, the OEM device driver should call
          the <f InterruptInitialize> function and register the event which 
          should be signalled when an interrupt needs to be handled. On waking 
          up, the thread should do everything required  - eg talk to the 
          hardware, read any data, process it etc. It should then call the <f 
          InterruptDone> function inside the kernel - which simply calls the 
          firmware routine <l OEMInterruptDone.OEMInterruptDone>. The OEM function
          should do the appropriate unmasking or end of interrupt signalling
          which is required. 

          Other functions are used to initialize the system, and enable/disable 
          specific interrupts.
          
    @xref <l OEMInit.OEMInit> <l OEMInterruptEnable.OEMInterruptEnable>
          <l OEMInterruptDisable.OEMInterruptDisable> <l OEMIdle.OEMIdle>
          <l OEMInterruptDone.OEMInterruptDone> <l HookInterrupt.HookInterrupt>
          <l InterruptInitialize.InterruptInitialize>
          <l InterruptDone.InterruptDone>
          <l InterruptDisable.InterruptDisable> <l Interrupt ID's.Interrupt ID's>
          
    @module intrapi.c - Interrupt Support APIs |
           Kernel API Functions for device driver interrupt handling.
    
    @xref <f InterruptInitialize> <f InterruptDone> <f InterruptDisable>
          <l Interrupt ID's.Interrupt ID's> <f HookInterrupt>
          <l Overview.Kernel Interrupt Support>
 */

/*
    @func   BOOL | HookInterrupt | Allows the firware to hook a specific
            hardware interrupt.
    @rdesc  return FALSE if hook failed.
    @parm   int | hwIntNumber | This is the hardware interrupt line which 
            went off. Note - This is NOT an interrupt ID as defined by the 
            Windows CE system.
    @parm   FARPROC | pfnHandler | The interrupt handler to be registered
    @comm   This is a kernel exposed function which is typically used by the
            OEM firmware to register it's ISR's during the <f OEMInit> call.
    @xref   <l Overview.Kernel Interrupt Support> <f OEMInit>                                     
*/ 



//------------------------------------------------------------------------------
//
//  @func   BOOL | InterruptMask | Mask Hardware Interrupt
//  @rdesc  Mask a hardware interrupt
//  @parm   DWORD | idInt | Interrupt ID to be associated with this IST.
//          See <l Interrupt ID's.Interrupt ID's> for possible values.
//  @parm   BOOL | fDisable | Either to disable (mask) or enable (unmask) the interrupt
//  @comm   A device driver can call InterrupMask to temporary mask/unmask an interrupt
//          so that it can safely access the hardware without worring about being
//          interrupted by the same interrupt.
//  @xref   <f InterruptDone> <f InterruptDisable> <f OEMInterruptEnable>
//          <l Overview.Kernel Interrupt Support>
//
//------------------------------------------------------------------------------
void INTRMask (DWORD idInt, BOOL fDisable)
{
    
    TRUSTED_API_VOID (L"INTRMask");

    if (IsInRange (idInt, SYSINTR_DEVICES, SYSINTR_MAXIMUM)) {
        PTHREAD pCurTh     = pCurThread;
        DWORD   dwAffinity = pCurTh->dwAffinity;
        BOOL    fEnable;
        SCHL_SetThreadAffinity (pCurTh, 1);
        
        fEnable = INTERRUPTS_ENABLE (FALSE);
        OEMInterruptMask (idInt, fDisable);
        INTERRUPTS_ENABLE (fEnable);
        
        SCHL_SetThreadAffinity (pCurTh, dwAffinity);
    }

}


//------------------------------------------------------------------------------
//
//  @func   BOOL | InterruptInitialize | Initialize Hardware Interrupt
//  @rdesc  returns FALSE if unable to initialize the interrupt
//  @parm   DWORD | idInt | Interrupt ID to be associated with this IST.
//          See <l Interrupt ID's.Interrupt ID's> for possible values.
//  @parm   HANDLE | hEvent | Event to be signalled when the interrupt
//          goes off.
//  @parm   LPVOID | pvData | Data to be passed to the firmware 
//          <f OEMInterruptEnable> call. This should be used for init data, or
//          for allocating scratch space for the firmware ISR to use. The kernel
//          will lock this data down and pass the physical address to the 
//          firmware routine so it can access it without being in danger of
//          getting a TLB miss.
//  @parm   DWORD | cbData | size of data passed in.            
//  @comm   A device driver calls InterruptInitialize to register an event
//          to be signalled when the interrupt occurs and to enable the
//          interrupt.
//  @xref   <f InterruptDone> <f InterruptDisable> <f OEMInterruptEnable>
//          <l Overview.Kernel Interrupt Support>
//
//------------------------------------------------------------------------------
BOOL 
INTRInitialize(
    DWORD idInt,
    HANDLE hEvent,
    LPVOID pvData,
    DWORD cbData
    )
{
    BOOL fRet = FALSE;
    TRUSTED_API (L"INTRInitialize", FALSE);
    
    /* Verify that the interrupt id is valid and that it is not already
     * initialized. */
    if (IsInRange (idInt, SYSINTR_DEVICES, SYSINTR_MAXIMUM)     // valid SYSINTR
        && !IntrEvents[idInt-SYSINTR_DEVICES]) {                // not inuse
        
        PEVENT lpe = LockIntrEvt (hEvent);
        if (lpe) {
            // valid event type
            PTHREAD pCurTh     = pCurThread;
            DWORD   dwAffinity = pCurTh->dwAffinity;
            BOOL    fEnable;
            SCHL_SetThreadAffinity (pCurTh, 1);
            
            fEnable = INTERRUPTS_ENABLE (FALSE);
            IntrEvents[idInt-SYSINTR_DEVICES] = lpe;
            fRet = OEMInterruptEnable (idInt, pvData, cbData);
            INTERRUPTS_ENABLE (fEnable);
            
            SCHL_SetThreadAffinity (pCurTh, dwAffinity);
            
            if (!fRet) {
                // OEMInterruptEnable failed
                IntrEvents[idInt-SYSINTR_DEVICES] = NULL;

                UnlockIntrEvt (lpe);
            }
        }
    }

    if (!fRet) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    }
    return fRet;
}



//------------------------------------------------------------------------------
//
//  @func   VOID | InterruptDone | Signal completion of interrupt processing
//  @parm   DWORD | idInt | Interrupt ID. See <l Interrupt ID's.Interrupt ID's>
//          for a list of possible ID's.
//  @rdesc  none
//  @comm   A device driver calls InterruptDone when it has completed the
//          processing for an interrupt and is ready for another interrupt.
//          InterruptDone must be called to unmask the interrupt before the
//          driver waits for the registered event to be signaled again. The
//          kernel essentially calls through to the <f OEMInterruptDone> 
//          function.
//  @xref <f InterruptInitialize> <f OEMInterruptDone>
//          <l Overview.Kernel Interrupt Support>
//
//------------------------------------------------------------------------------
void 
INTRDone(
    DWORD idInt
    )
{
    TRUSTED_API_VOID (L"INTRDone");
    if (IsInRange (idInt, SYSINTR_DEVICES, SYSINTR_MAXIMUM)) {
        PTHREAD pCurTh     = pCurThread;
        DWORD   dwAffinity = pCurTh->dwAffinity;
        BOOL    fEnable;
        SCHL_SetThreadAffinity (pCurTh, 1);

        fEnable = INTERRUPTS_ENABLE (FALSE);
        OEMInterruptDone (idInt);
        INTERRUPTS_ENABLE (fEnable);

        SCHL_SetThreadAffinity (pCurTh, dwAffinity);
    }
}



//------------------------------------------------------------------------------
//
//  @func   VOID | InterruptDisable | Disable Hardware Interrupt
//  @parm   DWORD | idInt | Interrupt ID. See <l Interrupt ID's.Interrupt ID's>
//          for a list of possible ID's.
//  @rdesc  none
//  @comm   A device driver calls InterruptDisable to disable the hardware
//          interrupt and to de-register the event registered via
//          InterruptInitialize. The driver must call InterruptDisable
//          before closing the event handle. The kernel calls through to the
//          <f OEMInterruptDisable> routine as part of this.
//  @xref   <f InterruptInitialize> <f OEMInterruptDisable>
//          <l Overview.Kernel Interrupt Support>
//
//------------------------------------------------------------------------------
void 
INTRDisable(
    DWORD idInt
    )
{
    TRUSTED_API_VOID (L"INTRDisable");
    if (IsInRange (idInt, SYSINTR_DEVICES, SYSINTR_MAXIMUM)) {
        
        PEVENT pEvent = IntrEvents[idInt-SYSINTR_DEVICES];

        if (pEvent) {

            DWORD loop;
            PTHREAD pCurTh     = pCurThread;
            DWORD   dwAffinity = pCurTh->dwAffinity;
            BOOL    fEnable;

            SCHL_SetThreadAffinity (pCurTh, 1);
            
            fEnable = INTERRUPTS_ENABLE (FALSE);
            OEMInterruptDisable (idInt);
            IntrEvents[idInt-SYSINTR_DEVICES] = 0;
            INTERRUPTS_ENABLE (fEnable);

            SCHL_SetThreadAffinity (pCurTh, dwAffinity);
            
            for (loop = SYSINTR_DEVICES; loop < SYSINTR_MAXIMUM; loop++) {
                if ((loop != idInt) && (pEvent == IntrEvents[loop-SYSINTR_DEVICES]))
                    break;
            }

            if (loop == SYSINTR_MAXIMUM) {
                // last reference to the interrupt event
                PTHREAD pthWake = SCHL_EventModIntr (pEvent,EVENT_SET);

                if (pthWake)
                    SCHL_MakeRunIfNeeded (pthWake);
            }
            UnlockIntrEvt (pEvent);
        }
    }
}


//------------------------------------------------------------------------------
// NKSetInterruptEvent
//------------------------------------------------------------------------------
BOOL NKSetInterruptEvent (DWORD idInt)
{
    BOOL fRet = IsInRange (idInt, SYSINTR_DEVICES, SYSINTR_MAXIMUM);
    if (fRet) {
        long mask;
        long pend;
        long *ptrPend;

        idInt -= SYSINTR_DEVICES;
        ptrPend = (long *) KInfoTable[KINX_PENDEVENTS];
        
        // calculate which DWORD based on idInt
        ptrPend += idInt >> 5;  // idInt / 32
        idInt   &= 0x1f;        // idInt % 32

        mask = 1 << idInt;
        do {
            pend = *ptrPend;
            if (pend & mask) {
                break;          // The bit is already set, so all done.
            }
        } while (InterlockedTestExchange(ptrPend, pend, pend|mask) != pend);
    }
    return fRet;
}


//------------------------------------------------------------------------------
// NKIsSysIntrValid
//------------------------------------------------------------------------------
BOOL NKIsSysIntrValid (DWORD idInt)
{
    idInt -= SYSINTR_DEVICES;
    return (idInt < SYSINTR_MAX_DEVICES)
        && IntrEvents[idInt];
}



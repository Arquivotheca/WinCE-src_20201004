//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __COMMON_H__
#define __COMMON_H__


/*++


Module Name:

   common.h

Abstract:

    This module contains header definitions and include files needed by all modules in the project



Environment:

   Kernel mode only


Revision History:

--*/

extern "C" {
#if defined (UNDER_CE) || defined (WINCE_EMULATION)
//#include <ntstatus.h>
  #if defined (WINCE_EMULATION)
    #include "winsock2.h"
  #endif
  #include <windows.h>
  #include <winnt.h>
//  #if ! defined (WINCE_EMULATION)
//    #include <wdm.h>
//  #else // ! defined (WINCE_EMULATION)
    #include <linklist.h>
//  #endif
  #include <ceport.h>
//#include <ntcompat.h>
#else  
  #include <ntddk.h>
  #pragma warning (disable:4200)
  #include "bthporti.h"
  #pragma warning (default:4200)
#endif  
}

#if defined (UNDER_CE) || defined (WINCE_EMULATION)
#include <svsutil.hxx>
  
inline NTSTATUS
WmiFireEvent(
    IN PDEVICE_OBJECT DeviceObject,
    IN LPGUID Guid,
    IN ULONG InstanceIndex,
    IN ULONG EventDataSize,
    IN PVOID EventData
    ) { return ERROR_SUCCESS; }
#endif

#define SELF_DESTRUCT void SelfDestruct(){delete this;}
#define SELF_IMPLODE(aClass)  void SelfDestruct() { this->~##aClass##(); }
#define ZERO_THIS() RtlZeroMemory(this, sizeof(*this))
#define POOLTAG_BTHBUS                                         'BHTB'
#define DEBUG_VARIABLE

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
//
// forward declartion
//
struct HciPacket;
class HciInterface;

typedef struct _L2CAP_CONNECTION *PL2CAP_CONNECTION;
typedef struct _L2CAP_INTERFACE *PL2CAP_INTERFACE;
typedef struct _HCI_CONNECTION *PHCI_CONNECTION;
typedef struct _ChildPdo *PChildPdo;

class SdpInterface;
class BThDevice;
class CanelableQue;
class SdpDatabase;
class SecurityDatabase;
struct _ChildPdo;
struct _HCI_COMMAND_ENTRY;
typedef union _HCI_EVENT *PHCI_EVENT;
#endif // UNDER_CE


#include <stdio.h>
#include "kmisc.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include <wmilib.h>
#if (DBG)
#define STR_MODULENAME "BTHPORT: "
#endif

#define BTH_NEW_PRINTF 1

#include "debug.h"
#include "debugdef.h"

#include <btdebug.h>
#include "bthlib.h"

#include "kmisc.h"
#include "irpque.h"
#include "bt_100.h"
#include "bthpriv.h"
#include "bthioctl.h"
#include "bthddi.h"
#include "bthguid.h"

typedef 
void
(*PFNBTCALLBACK)(
    PVOID pContext,
    NTSTATUS ntStatus,
	USHORT btStatus
 );

#include "btdevice.h"
#include "bthci.h"
#include "btl2cap.h"
#include "bthsdpdef.h"
#else
#include "winsock2.h"
#include "bt_buffer.h"
#include "bt_ddi.h"
#include "bt_api.h"
#include "bt_sdp.h"
#include "bthsdpdef.h"
#endif

#include "sdplib.h"
#include "sdp_priv.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "bthsdpddi.h"
#endif
#include "SdpDB.h"

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "SecurityDB.h"
#endif

#include "btsdp.h"

#define BD_TO_BT_ADDR(_pbd_addr, _pbt_addr)                                    \
    (*(_pbt_addr)) = SET_NAP_SAP((_pbd_addr)->NAP, (_pbd_addr)->SAP)
    
#define BT_TO_BD_ADDR(_pbt_addr, _pbd_addr) { (_pbd_addr)->SAP = GET_SAP((*(_pbt_addr))); \
                                              (_pbd_addr)->NAP = GET_NAP((*(_pbt_addr))); }


#define BT_IRP_STORAGE_0        0  // store IRP in Irp->Tail.Overlay.DriverContext[0]
#define BT_IRP_STORAGE_1        1 //  store CancelableQue* in Irp->Tail.Overlay.DriverContext[1]
#define BT_IRP_STORAGE_2        2 //  store l2cap state
#define BT_IRP_STORAGE_3        3 // 

// Slap CancelableQueue into this slot of the irp
#define BT_IRP_CANCEL_DATA          (1)
#define BT_IRP_STATE                (2)

#define STATE_FROM_IRP(irp)         \
    PtrToUlong((irp)->Tail.Overlay.DriverContext[BT_IRP_STATE]) 
    
#define STATE_TO_IRP(irp, state)         \
    (irp)->Tail.Overlay.DriverContext[BT_IRP_STATE]  = UlongToPtr((state))


#define SDP_IRP_STORAGE_UUID_MAX    BT_IRP_STORAGE_2 // store SDP max uuids in irp
#define SDP_IRP_STORAGE_RANGE_MAX   BT_IRP_STORAGE_3 // store SDP max range in irp

#define BTH_IRP_STORAGE_BRB			BT_IRP_STORAGE_0 // store brb in irp.
#define BTH_IRP_STORAGE_CALLBACK    BT_IRP_STORAGE_2 // store inquiry callback in irp
#define BTH_IRP_STORAGE_CONTEXT     BT_IRP_STORAGE_3 // store inquiry callback context in





////////////////////////////////////////////////////////////////////////////////////////////////////////
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#define INIT_CODE       code_seg("INIT", "CODE")
#define INIT_DATA       data_seg("INIT", "DATA")
#define LOCKED_CODE     code_seg(".text", "CODE")
#define LOCKED_DATA     data_seg(".data", "DATA")
#define LOCKED_BSS      bss_seg(".data", "DATA")
#if LOOK_AT_LATER
    #define PAGEABLE_CODE   code_seg("PAGE", "CODE")
#else
    #define PAGEABLE_CODE   code_seg(".text", "CODE")
#endif
#define PAGEABLE_DATA   data_seg("PAGEDATA", "DATA")
#define PAGEABLE_BSS    bss_seg("PAGEDATA", "DATA")

#if LOOK_AT_LATER
    #define PAGEDCODE code_seg("page")
#else
    #define PAGEDCODE code_seg(".text")
#endif

#define LOCKEDCODE code_seg(".text")
#define INITCODE code_seg("init")

#define PAGEDDATA data_seg("page")
#define LOCKEDDATA data_seg()
#define INITDATA data_seg("init")
#endif // UNDER_CE

#define arraysize(p) (sizeof(p)/sizeof((p)[0]))

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#ifndef GUID_STR_LENGTH
#define GUID_STR_LENGTH     38  // "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
#endif  // GUID_STR_LENGTH

#ifndef BD_ADDR_STR_LENGTH
#define BD_ADDR_STR_LENGTH  12  // "aabbccddeeff"
#endif  //  BD_ADDR_STR_LENGTH

#define STR_REG_MACHINE     L"\\Registry\\Machine\\"

#define IS_EVEN(v) (((v) % 2) == 0)
#define IS_ODD(v)  (((v) % 2) == 1)

#define IS_IN_RANGE(v, minV, maxV) ((v) >= (minV) && (v)  <= (maxV))

#define IS_REQUEST_SIGNAL(v) IS_EVEN(v)
#define IS_RESPONSE_SIGNAL(v) IS_ODD(v)




#undef PAGED_CODE
#define PAGED_CODE()

typedef
NTSTATUS
(*PFN_DELETE_VALUE_KEY)(
    HANDLE KeyHandle,
    PUNICODE_STRING ValueName
    );

struct BthPortGlobals {
    //
    // SDP database
    //
    SdpDatabase *pSdpDB;

    //
    // Security manager
    //
    SecurityDatabase *pSecDB;

    //
    // Function pointer to ZwDeleteValueKey since it is not exported by default
    // (and that is by mistake)
    //
    PFN_DELETE_VALUE_KEY pZwDeleteValueKey;

    //
    // Reg path in services tree
    //
    UNICODE_STRING ServiceKey;

    //
    // Count of the unnamed devices we have seen
    //
    LONG UnnamedDeviceCount;
};

extern BthPortGlobals Globals;

#endif  // UNDER_CE

//Macro to align to 2, 4, 8 bytes (MUST be power of 2)
#define __SDP_ALIGN(datum, bound) (((unsigned int)(datum) + (bound) - 1) & (~((bound) - 1)))
#define __SDP_IS_ALIGNED(datum, bound) (((unsigned int)(datum) & ((bound) - 1)) == 0)
#endif


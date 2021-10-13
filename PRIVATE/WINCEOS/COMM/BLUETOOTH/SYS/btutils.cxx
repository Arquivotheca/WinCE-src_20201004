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
//------------------------------------------------------------------------------
// 
//      Miscellaneous Bluetooth Utilities
// 
// 
// Module Name:
// 
//      btutils.cxx
// 
// Abstract:
// 
//      This file implements misc Bluetooth utilities
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>

#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_os.h>

#include <bt_debug.h>

int StatusToError (unsigned char status, int iGeneric) {
    switch (status) {
    case BT_ERROR_SUCCESS:
        return ERROR_SUCCESS;

    case BT_ERROR_COMMAND_DISALLOWED:
    case BT_ERROR_UNKNOWN_HCI_COMMAND:
    case BT_ERROR_UNSUPPORTED_FEATURE_OR_PARAMETER:
    case BT_ERROR_INVALID_HCI_PARAMETER:
    case BT_ERROR_UNSUPPORTED_REMOTE_FEATURE:
        return ERROR_CALL_NOT_IMPLEMENTED;

    case BT_ERROR_NO_CONNECTION:
        return ERROR_DEVICE_NOT_CONNECTED;

    case BT_ERROR_HARDWARE_FAILURE:
    case BT_ERROR_UNSPECIFIED_ERROR:
        return ERROR_ADAP_HDW_ERR;

    case BT_ERROR_PAGE_TIMEOUT:
    case BT_ERROR_CONNECTION_TIMEOUT:
    case BT_ERROR_HOST_TIMEOUT:
        return ERROR_TIMEOUT;

    case BT_ERROR_AUTHENTICATION_FAILURE:
        return ERROR_NOT_AUTHENTICATED;

    case BT_ERROR_KEY_MISSING:
        return ERROR_NO_USER_SESSION_KEY;

    case BT_ERROR_MEMORY_FULL:
        return ERROR_OUTOFMEMORY;

    case BT_ERROR_MAX_NUMBER_OF_CONNECTIONS:
    case BT_ERROR_MAX_NUMBER_OF_SCO_CONNECTIONS:
    case BT_ERROR_MAX_NUMBER_OF_ACL_CONNECTIONS:
        return ERROR_NO_SYSTEM_RESOURCES;

    case BT_ERROR_HOST_REJECTED_LIMITED_RESOURCES:
    case BT_ERROR_HOST_REJECTED_SECURITY_REASONS:
    case BT_ERROR_HOST_REJECTED_PERSONAL_DEVICE:
    case BT_ERROR_PAIRING_NOT_ALLOWED:
        return ERROR_CONNECTION_REFUSED;

    case BT_ERROR_OETC_USER_ENDED:
    case BT_ERROR_OETC_LOW_RESOURCES:
    case BT_ERROR_OETC_POWERING_OFF:
        return ERROR_GRACEFUL_DISCONNECT;

    case BT_ERROR_CONNECTION_TERMINATED_BY_LOCAL_HOST:
        return ERROR_CONNECTION_ABORTED;

    case BT_ERROR_REPEATED_ATTEMPTS:
        return ERROR_CONNECTION_COUNT_LIMIT;
    }

    return iGeneric;
}

void BufferFree (BD_BUFFER *pBuf) {
    if (! pBuf->fMustCopy)
        g_funcFree (pBuf, g_pvFreeData);
}

BD_BUFFER *BufferAlloc (int cSize) {
    SVSUTIL_ASSERT (cSize > 0);

    BD_BUFFER *pRes = (BD_BUFFER *)g_funcAlloc (cSize + sizeof (BD_BUFFER), g_pvAllocData);
    if (pRes) {
        pRes->cSize = cSize;

        pRes->cEnd = pRes->cSize;
        pRes->cStart = 0;

        pRes->fMustCopy = FALSE;
        pRes->pFree = BufferFree;
        pRes->pBuffer = (unsigned char *)(pRes + 1);
    }
    
    return pRes;
}

BD_BUFFER *BufferCopy (BD_BUFFER *pBuffer) {
    BD_BUFFER *pRes = BufferAlloc (pBuffer->cSize);
    if (pRes) {
        pRes->cSize = pBuffer->cSize;
        pRes->cStart = pBuffer->cStart;
        pRes->cEnd = pBuffer->cEnd;
        pRes->fMustCopy = FALSE;
        pRes->pFree = BufferFree;
        pRes->pBuffer = (unsigned char *)(pRes + 1);

        memcpy (pRes->pBuffer, pBuffer->pBuffer, pRes->cSize);
    }
    
    return pRes;
}

BD_BUFFER *BufferCompress (BD_BUFFER *pBuffer) {
    BD_BUFFER *pRes = BufferAlloc (BufferTotal (pBuffer));
    if (pRes) {
        pRes->fMustCopy = FALSE;
        pRes->pFree = BufferFree;
        pRes->pBuffer = (unsigned char *)(pRes + 1);

        memcpy (pRes->pBuffer, pBuffer->pBuffer + pBuffer->cStart, pRes->cSize);
    }
    
    return pRes;
}

enum STAGES {
    INITIALIZED,
    STARTED,
    STOPPED,
    GONE
};

class UtilState : public SVSThreadPool {
public:
    STAGES eStage;

    UtilState (void) { eStage = INITIALIZED; }
    ~UtilState (void) { eStage = GONE; }
} *utilState;

class BTHandleSys : public SVSSynch, public SVSHandleSystem {
public:
    STAGES eStage;

    BTHandleSys (void) { eStage = INITIALIZED; }
    ~BTHandleSys (void) { eStage = GONE; }
} *handleSys;


SVSCookie btutil_ScheduleEvent (LPTHREAD_START_ROUTINE pfn, LPVOID lpParameter, unsigned long ulDelayTime) {
    utilState->Lock ();
    if (utilState->eStage != STARTED) {
        utilState->Unlock ();
        return 0;
    }

    SVSCookie res = utilState->ScheduleEvent (pfn, lpParameter, ulDelayTime);
    utilState->Unlock ();

    return res;
}

int btutil_UnScheduleEvent (SVSCookie ulCookie) {
    utilState->Lock ();
    if (utilState->eStage != STARTED) {
        utilState->Unlock ();
        return FALSE;
    }

    int res = utilState->UnScheduleEvent (ulCookie);
    utilState->Unlock ();

    return res;
}

#define MAX_LTD_BYTES       32
#define MAX_LTD_INDICES     1
#define MAX_LTD_DEVICES        32

struct LTD {
    BD_ADDR         ba;
    unsigned int    uiRef;
    unsigned int    uiDataMask;
    unsigned char   ucData[MAX_LTD_BYTES];
};

struct LTD_DESCR {
    int offset;
    int size;
};

static CRITICAL_SECTION g_csLongTerm;
static LTD              g_ltd[MAX_LTD_DEVICES];
static LTD_DESCR        g_ltdDescr[MAX_LTD_INDICES] = {
    {0, (sizeof(InquiryResultBuffer))}
};

void btutil_PersistPeerInfo (BD_ADDR *pPeerBA, unsigned int fWhat, void *pData, int cbData) {
    if ((fWhat <= 0) || (fWhat > MAX_LTD_INDICES)) {
        SVSUTIL_ASSERT (0);
        return;
    }

    --fWhat;    // convert to 0-based

    if (cbData > g_ltdDescr[fWhat].size) {
        SVSUTIL_ASSERT (0);
        return;
    }

    //    
    //    The following code loops through the list of devices looking for a matching
    //  address or an empty slot.  It also keeps track of the index with the least
    //  references and will replace that item in the event that the LTD is full.
    //

    int iRepl = -1;

    EnterCriticalSection (&g_csLongTerm);
    for (int i = 0 ; i < MAX_LTD_DEVICES ; ++i) {
        if (g_ltd[i].uiRef == 0)
            break;

        if (g_ltd[i].ba == *pPeerBA)
            break;

        if ((iRepl < 0) || (g_ltd[iRepl].uiRef > g_ltd[i].uiRef))
            iRepl = i;
    }

    if (i < MAX_LTD_DEVICES)
        iRepl = i;

    g_ltd[iRepl].ba = *pPeerBA;
    g_ltd[iRepl].uiRef++;
    g_ltd[iRepl].uiDataMask |= (1 << fWhat);
    memcpy (g_ltd[iRepl].ucData + g_ltdDescr[fWhat].offset, pData, g_ltdDescr[fWhat].size);

    LeaveCriticalSection (&g_csLongTerm);
}

int  btutil_RetrievePeerInfo (BD_ADDR *pPeerBA, unsigned int fWhat, void *pData, int cbData) {
    if ((fWhat <= 0) || (fWhat > MAX_LTD_INDICES)) {
        SVSUTIL_ASSERT (0);
        return FALSE;
    }

    --fWhat;    // convert to 0-based

    int iRes = FALSE;

    if (cbData > g_ltdDescr[fWhat].size) {
        SVSUTIL_ASSERT (0);
        return FALSE;
    }

    EnterCriticalSection (&g_csLongTerm);
    for (int i = 0 ; i < MAX_LTD_DEVICES ; ++i) {
        if (g_ltd[i].uiRef == 0)
            break;

        if (g_ltd[i].ba == *pPeerBA) {
            if (g_ltd[i].uiDataMask & (1 << fWhat)) {
                memcpy (pData, g_ltd[i].ucData + g_ltdDescr[fWhat].offset, g_ltdDescr[fWhat].size);
                g_ltd[i].uiRef++;
                iRes = TRUE;
            }

            break;
        }
    }
    LeaveCriticalSection (&g_csLongTerm);

    return iRes;
}

int btutil_IteratePeerInfo (DWORD dwIterate, unsigned int fWhat, BD_ADDR *pPeerBA, void *pData, int cbData) {
    if ((fWhat <= 0) || (fWhat > MAX_LTD_INDICES)) {
        SVSUTIL_ASSERT (0);
        return FALSE;
    }

    if (dwIterate >= MAX_LTD_DEVICES)
        return FALSE;

    --fWhat;    // convert to 0-based

    int fRet = FALSE;

    EnterCriticalSection (&g_csLongTerm);
    if (g_ltd[dwIterate].uiRef > 0) {
        if (cbData > g_ltdDescr[fWhat].size) {
            SVSUTIL_ASSERT (0);            
        } else {        
            *pPeerBA = g_ltd[dwIterate].ba;
            memcpy (pData, g_ltd[dwIterate].ucData + g_ltdDescr[fWhat].offset, g_ltdDescr[fWhat].size);
            fRet = TRUE;
        }
    }
    LeaveCriticalSection (&g_csLongTerm);

    return fRet;
}


SVSHandle btutil_AllocHandle(LPVOID pvData) {
    handleSys->Lock ();
    if (handleSys->eStage != STARTED) {
        handleSys->Unlock ();
        return SVSUTIL_HANDLE_INVALID;
    }

    SVSHandle h = handleSys->AllocHandle (pvData);

    IFDBG(DebugOut (DEBUG_VERBOSE, L"[UTILS] btutil_AllocHandle :: h=0x%08X pv=0x%08X\n", h, pvData));

    handleSys->Unlock ();

    return h;
}

LPVOID btutil_CloseHandle(SVSHandle h) {
    handleSys->Lock ();
    if (handleSys->eStage != STARTED) {
        handleSys->Unlock ();
        return NULL;
    }

    LPVOID lpv = handleSys->CloseHandle (h);

    IFDBG(DebugOut (DEBUG_VERBOSE, L"[UTILS] btutil_CloseHandle :: h=0x%08X pv=0x%08X\n", h, lpv));

    handleSys->Unlock ();

    return lpv;
}

LPVOID btutil_TranslateHandle(SVSHandle h) {
    handleSys->Lock ();
    if (handleSys->eStage != STARTED) {
        handleSys->Unlock ();
        return NULL;
    }

    LPVOID lpv = handleSys->TranslateHandle (h);

    handleSys->Unlock ();

    return lpv;
}


int btutil_InitializeOnce (void) {
    SVSUTIL_ASSERT (utilState == NULL);
    
    utilState = new UtilState;
    if (! utilState)
        return ERROR_OUTOFMEMORY;
    
    handleSys = new BTHandleSys;
    if (! handleSys) {
        delete utilState;
        utilState = NULL;

        return ERROR_OUTOFMEMORY;
    }

    InitializeCriticalSection (&g_csLongTerm);

    return ERROR_SUCCESS;
}

int btutil_UninitializeOnce (void) {
    SVSUTIL_ASSERT (utilState);
    SVSUTIL_ASSERT (utilState->eStage == STOPPED);

    delete utilState;
    utilState = NULL;

    delete handleSys;
    handleSys = NULL;

    DeleteCriticalSection (&g_csLongTerm);

    return ERROR_SUCCESS;
}

int btutil_CreateDriverInstance (void) {
    utilState->eStage = STARTED;
    handleSys->eStage = STARTED;
    
    return ERROR_SUCCESS;
}

int btutil_CloseDriverInstance (void) {
    utilState->Shutdown ();
    utilState->eStage = STOPPED;
    
    handleSys->eStage = STOPPED;

    return ERROR_SUCCESS;
}

int btutil_VerifyExtendedL2CAPOptions (int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
    int iRes = TRUE;

    __try {
        for (int i = 0 ; i < cOptNum ; ++i) {
            if ((ppExtendedOptions[i]->type & 0x80) == 0) {
                IFDBG(DebugOut (DEBUG_WARN, L"[UTILS] btutil_VerifyExtendedL2CAPOptions :: unrecognized option 0x%02x is required - rejecting the connection!\n", ppExtendedOptions[i]->type));
                iRes = FALSE;
                break;
            }
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[UTILS] btutil_VerifyExtendedL2CAPOptions :: exception in processing options!\n"));
        iRes = FALSE;
    }

    return iRes;
}

int btutil_GetLastIrb(HANDLE hHCI, BT_LAYER_IO_CONTROL pIoctl, BD_ADDR* pbda, InquiryResultBuffer* pirb)
{
    int cSizeRet = 0;
    
    int iError = pIoctl (hHCI, BTH_HCI_IOCTL_GET_LAST_INQUIRY_DATA, sizeof(*pbda), (char *)pbda,
                sizeof(*pirb), (char *)pirb, &cSizeRet);

    if ((iError != ERROR_SUCCESS) || (cSizeRet != sizeof(*pirb))) {
        memset (pirb, 0, sizeof(*pirb));
        pirb->page_scan_repetition_mode = 2; // use R2 by default
        return ERROR_NOT_FOUND;
    }

    return ERROR_SUCCESS;    
}

#if defined (SDK_BUILD)
static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

extern "C" int CheckTime (void) {
    char *pDate = __DATE__;

    int iRet = FALSE;

    for ( ; ; ) {
        for (int i = 0 ; i < 12 ; ++i) {
            if (strncmp (pDate, months[i], 3) == 0)
                break;
        }

        if (i == 12)
            break;

        int iMonth = i + 1;

        for (i = 0 ; (i < 7) && (*pDate != '\0') ; ++i, ++pDate)
            ;

        int iYear = atoi (pDate);

        if ((iYear < 2000) || (iYear > 2010))
            break;

        //    Add 6 months

        iMonth += 6;
        if (iMonth > 12) {
            iMonth -= 12;
            iYear += 1;
        }


        SYSTEMTIME st;
        GetSystemTime (&st);

        if ((st.wYear < iYear) || ((st.wYear == iYear) && (st.wMonth <= iMonth)))
            iRet = TRUE;

        break;
    }

    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, L"Software\\Microsoft\\Bluetooth", 0, KEY_WRITE, &hk)) {
        WCHAR szDate[128];

        if (! iRet)
            wcscpy (szDate, L"EXPIRED");
        else
            MultiByteToWideChar (CP_ACP, 0, __DATE__, -1, szDate, 128);

        RegSetValueEx (hk, L"BETA", 0, REG_SZ, (BYTE *)szDate, (wcslen (szDate) + 1) * sizeof(WCHAR));
    }

    return iRet;
}
#endif



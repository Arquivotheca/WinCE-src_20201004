//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <guiddef.h>
#include <devload.h>
#include <pnp.h>
#include "devloadp.h"
#include "device.h"

#include <msgqueue.h>

typedef struct AdvertisementListElement_ AdvertisementListElement;
typedef struct AdvertisementListElement_ {
    AdvertisementListElement *pNext;    // next in list of interfaces
    HPROCESS hProc;             // handle of the process advertising this interface
    DEVDETAIL devDetail;        // information about the interface
} AdvertisementListElement;

VOID StartDeviceNotify(LPTSTR DevName, BOOL bNew);

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

#define GUID_FORMAT_W   L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"
#define GUIDPARAMS(g)   (g).Data1, (g).Data2, (g).Data3, (g).Data4[0], (g).Data4[1], (g).Data4[2], (g).Data4[3], (g).Data4[4], (g).Data4[5], (g).Data4[6], (g).Data4[7]

static struct {
    NotifyListElement *phNotify;
    AdvertisementListElement *phAdvertisement;
    CRITICAL_SECTION cs;
} g_Notifications;

void InitializeDeviceNotifications (void)
{
    InitializeCriticalSection(&g_Notifications.cs);
    g_Notifications.phNotify = NULL;
    g_Notifications.phAdvertisement = NULL;
}

HANDLE RegisterDeviceNotifications (LPCGUID pclass, HANDLE hQ)
{
    MSGQUEUEOPTIONS msgopts;
    NotifyListElement *pNote;

    DEBUGMSG(ZONE_PNP, (_T("+RegisterDeviceNotifications({%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}, %08x)\r\n"), 
        GUIDPARAMS(*pclass), hQ));
    
    pNote = LocalAlloc(0, sizeof(*pNote));
    if (pNote == NULL)
        return INVALID_HANDLE_VALUE;  //OOM!

    msgopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgopts.bReadAccess = FALSE;  // this is the only option that matters here
    pNote->hSend = OpenMsgQueue(GetCallerProcess(), hQ, &msgopts);
    if (pNote->hSend == NULL) {
        DEBUGMSG(ZONE_PNP|ZONE_ERROR, (_T("-RegisterDeviceNotifications(0x%08x): ERROR 0x%x\r\n"), hQ, GetLastError()));
        LocalFree(pNote);
        return INVALID_HANDLE_VALUE;
    }

    if (pclass)
        pNote->guidDevClass = *pclass;
    else
        memset(&pNote->guidDevClass, 0, sizeof(pNote->guidDevClass));

    // now add it to the list.
    EnterCriticalSection(&g_Notifications.cs);
    pNote->pNext = g_Notifications.phNotify;
    g_Notifications.phNotify = pNote;
    LeaveCriticalSection(&g_Notifications.cs);

    DEBUGMSG(ZONE_PNP, (_T("-RegisterDeviceNotifications(0x%08x): 0x%08x\r\n"), hQ, pNote));

    return (HANDLE)pNote;
}


void UnregisterDeviceNotifications (HANDLE h)
{
    NotifyListElement **pp, *pNote = (NotifyListElement *)h;
    
    DEBUGMSG(ZONE_PNP, (_T("+UnregisterDeviceNotifications(0x%08x)\r\n"), h));

    EnterCriticalSection(&g_Notifications.cs);
    for (pp = &g_Notifications.phNotify; *pp; pp = &(*pp)->pNext)
        if (*pp == pNote) {
            *pp = pNote->pNext;
            break;
        }
    LeaveCriticalSection(&g_Notifications.cs);

    CloseHandle(pNote->hSend);
    LocalFree(pNote);
    DEBUGMSG(ZONE_PNP, (_T("-UnregisterDeviceNotifications(0x%08x)\r\n"), h));
}

static void SendToMaybe (NotifyListElement *pNote, PDEVDETAIL pDev)
{
    static const GUID DEVCLASS_ALL = {0};

    if (GUID_ISEQUAL(pNote->guidDevClass, DEVCLASS_ALL) || GUID_ISEQUAL(pNote->guidDevClass, pDev->guidDevClass)) {
        BOOL r;
        DEBUGMSG(ZONE_PNP, (_T("SendToMaybe(0x%08x): '%s' %s\r\n"), pNote, pDev->szName, pDev->fAttached ? _T("Attaching") : _T("Detaching")));
        r = WriteMsgQueue(pNote->hSend, pDev, sizeof(*pDev)+pDev->cbName, 0, 0);
        if (!r)
            DEBUGMSG(ZONE_WARNING,
                     (TEXT("Device!SendToMaybe: WriteMsgQ(0x%x) failed: err %d\n"),
                      pNote->hSend, GetLastError()));
    }
}

static void SendNotifications (PDEVDETAIL pDev)
{
    NotifyListElement *pNote;

    // Ensure the list doesn't change while we're traversing it
    DEBUGMSG(ZONE_PNP, (_T("+SendNotifications: '%s' %s\r\n"), pDev->szName, pDev->fAttached ? _T("Attaching") : _T("Detaching")));
    EnterCriticalSection(&g_Notifications.cs);
    for (pNote = g_Notifications.phNotify; pNote; pNote = pNote->pNext)
        SendToMaybe(pNote, pDev);
    LeaveCriticalSection(&g_Notifications.cs);
    DEBUGMSG(ZONE_PNP, (_T("-SendNotifications\r\n")));
}

void AdvertisementSendTo(HANDLE h)
{
    NotifyListElement *pNote = (NotifyListElement *) h;
    AdvertisementListElement *pAd;

    DEBUGMSG(ZONE_PNP, (_T("+AdvertisementSendTo(0x%08x)\r\n"), h));
    EnterCriticalSection(&g_Notifications.cs);

    // look for the interface
    for(pAd = g_Notifications.phAdvertisement; pAd != NULL; pAd = pAd->pNext) {
        SendToMaybe(pNote, &pAd->devDetail);
    }

    LeaveCriticalSection(&g_Notifications.cs);
    DEBUGMSG(ZONE_PNP, (_T("-AdvertisementSendTo()\r\n")));
}

static AdvertisementListElement *FindAdvertisedInterface(LPCGUID pDevClass, LPCWSTR pszName)
{
    AdvertisementListElement *pAd;
    int cbName;

    // should never get a 0-length name, but let's be conservative
    if(pszName == NULL || pszName[0] == 0) {
        cbName = 0;
    } else {
        cbName = wcslen(pszName) + 1;
    }
    cbName *= sizeof(WCHAR);        // convert to byte count, including null
    
    // look for another instance of the interface (matching name and guid)
    for(pAd = g_Notifications.phAdvertisement; pAd != NULL; pAd = pAd->pNext) {
        if(GUID_ISEQUAL(pAd->devDetail.guidDevClass, *pDevClass) 
            && pAd->devDetail.cbName == cbName
            && memcmp(pAd->devDetail.szName, pszName, cbName) == 0) {
            break;
        }        
    }

    return pAd;
}

static BOOL AddAdvertisedInterface(AdvertisementListElement *pAd, LPCGUID pDevClass, LPCWSTR pszName, DWORD dwReserved)
{
    BOOL fOk = TRUE;
    AdvertisementListElement *pNewAd;
    int cbName;
    
    // try to allocate a new entry
    cbName = wcslen(pszName) * sizeof(WCHAR);
    pNewAd = LocalAlloc(LPTR, sizeof(*pNewAd) + cbName);
    if(pNewAd == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        fOk = FALSE;
    } else {
        // fill in the new entry
        memset(pNewAd, 0, sizeof(*pNewAd) + cbName);
        pNewAd->hProc = GetCallerProcess();
        pNewAd->devDetail.guidDevClass = *pDevClass;
        pNewAd->devDetail.dwReserved = dwReserved;
        pNewAd->devDetail.fAttached = TRUE;
        pNewAd->devDetail.cbName = cbName + sizeof(WCHAR);       // include byte count for NULL
        memcpy(pNewAd->devDetail.szName, pszName, cbName);       // null terminated during memset

        // add this interface to the list
        pNewAd->pNext = g_Notifications.phAdvertisement;
        g_Notifications.phAdvertisement = pNewAd;

        // send out the notification
        DEBUGMSG(ZONE_PNP, (_T("AddAdvertisedInterface: proc 0x%08x added {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} ('%s')\r\n"),
            pNewAd->hProc, GUIDPARAMS(pNewAd->devDetail.guidDevClass), pNewAd->devDetail.szName));
        SendNotifications(&pNewAd->devDetail);
    }
    
    return fOk;
}

static BOOL DeleteAdvertisedInterface(AdvertisementListElement *pAd)
{
    BOOL fOk = TRUE;
    
    DEBUGMSG(ZONE_PNP, (_T("DeleteAdvertisedInterface: deleting {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} ('%s') from proc 0x%08x\r\n"),
        GUIDPARAMS(pAd->devDetail.guidDevClass), pAd->devDetail.szName, pAd->hProc));

    // notify listeners that this interface is going away
    pAd->devDetail.fAttached = FALSE;
    SendNotifications(&pAd->devDetail);

    // yes, remove it from the list
    if(g_Notifications.phAdvertisement == pAd) {
        // remove it from the start of the list
        g_Notifications.phAdvertisement = pAd->pNext;
    } else {
        AdvertisementListElement *pTmpAd;

        // remove it from the middle or end of the list
        for(pTmpAd = g_Notifications.phAdvertisement; pTmpAd != NULL; pTmpAd = pTmpAd->pNext) {
            if(pTmpAd->pNext == pAd) {
                pTmpAd->pNext = pAd->pNext;
                break;
            }
        }
    }

    // free the entry
    LocalFree(pAd);

    return fOk;
}

BOOL UpdateAdvertisedInterface(LPCGUID pDevClass, LPCWSTR pszName, BOOL fAdd)
{
    AdvertisementListElement *pAd;
    BOOL fOk = TRUE;
    int iNameChars;

    DEBUGMSG(ZONE_PNP, (_T("+UpdateAdvertisedInterface('%s'): fAdd is %d\r\n"), pszName, fAdd));
    
    // sanity check parameters
    if(pDevClass== NULL || pszName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // make sure we don't have a 0 length name
    iNameChars = wcslen(pszName);
    if(iNameChars == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    EnterCriticalSection(&g_Notifications.cs);

    // look for another instance of the interface (matching name and guid)
    pAd = FindAdvertisedInterface(pDevClass, pszName);

    // add or remove it as appropriate
    if(fAdd) {
        // this must be a new interface
        if(pAd != NULL) {
            SetLastError(ERROR_ALREADY_EXISTS);
            fOk = FALSE;
        } else {
            // not a duplicate, try to add it
            fOk = AddAdvertisedInterface(pAd, pDevClass, pszName, 0xFFFFFFFF);
        }
    } else {
        // did we find the interface?
        if(pAd == NULL) {
            // no, return failure
            SetLastError(ERROR_FILE_NOT_FOUND);
            fOk = FALSE;
        } else {
            // free the entry
            fOk = DeleteAdvertisedInterface(pAd);
        }
    }

    LeaveCriticalSection(&g_Notifications.cs);

    // send out device notifications of our new interface if the notification system can
    // handle the name and we haven't had any errors.  The device notification routines
    // expect device names to be <= DEVNAME_LEN characters.
    if(fOk && iNameChars < DEVNAME_LEN) {
        TCHAR szNotifyName[DEVNAME_LEN];
        memset(szNotifyName, 0, sizeof(szNotifyName));      // fill unused characters
        _tcscpy(szNotifyName, pszName);                            // copy the name string
        StartDeviceNotify(szNotifyName, fAdd);                    // tell appliations about the interface
    }
    
    DEBUGMSG(ZONE_PNP, (_T("-UpdateAdvertisedInterface: returning %d\r\n"), fOk));
    return fOk;
}

void DeleteProcessAdvertisedInterfaces(DWORD dwFlags, HPROCESS hProc, HTHREAD hThread)
{
    AdvertisementListElement *pAd, *pAdNext;
    BOOL fOk;

    DEBUGMSG(ZONE_PNP, (_T("+DeleteProcessAdvertisedInterfaces(0x%08x)\r\n"), hProc));
    EnterCriticalSection(&g_Notifications.cs);

    // look for the interface
    pAd = g_Notifications.phAdvertisement;
    fOk = TRUE;
    while(fOk && pAd != NULL) {
        pAdNext = pAd->pNext;
        if(pAd->hProc == hProc) {
            // found an interface created by the exiting process; delete it
            fOk = UpdateAdvertisedInterface(&pAd->devDetail.guidDevClass, pAd->devDetail.szName, FALSE);
            if(!fOk) {
                DEBUGMSG(ZONE_PNP | ZONE_WARNING, 
                    (_T("DeleteProcessAdvertisedInterfaces: couldn't delete 0x%08x for proc 0x%08x\r\n"),
                    pAd, hProc));
            }
        }
        pAd = pAdNext;
    }

    LeaveCriticalSection(&g_Notifications.cs);
    DEBUGMSG(ZONE_PNP, (_T("-DeleteProcessAdvertisedInterfaces(0x%08x)\r\n"), hProc));
}


static BOOL ConvertStringToGuid (IN LPCTSTR pwszGuid, OUT GUID *pGuid)
{
    UINT Data4[8];
    int  Count;

    if (swscanf(pwszGuid,
                      GUID_FORMAT_W,
                      &pGuid->Data1, 
                      &pGuid->Data2, 
                      &pGuid->Data3, 
                      &Data4[0], 
                      &Data4[1], 
                      &Data4[2], 
                      &Data4[3], 
                      &Data4[4], 
                      &Data4[5], 
                      &Data4[6], 
                      &Data4[7]) != 11) 
    {
        return FALSE;
    }

    for(Count = 0; Count < sizeof(Data4)/sizeof(Data4[0]); Count++) 
    {
        pGuid->Data4[Count] = (UCHAR)Data4[Count];
    }

    return TRUE;
}

// Given the ActiveKey,
// check to see if there is an "IClass" value;
// if not, check for one in the DevKey.
// If still not, then if there's a "Prefix" value, use the DEVCLASS_STREAM guid;
// otherwise issue a warning - this driver is not PnP-CE compatible.
//
// For each guid in the IClass value (assume 1 if we defaulted):
//   it should be of the form "GUID[=name]" where GUID is in the standard
//   registry form - {8-4-4-4-12} - and name is a string representing the
//   the device address as advertised over the interface designated by the GUID.
//   If the "=name" component is not present then the driver must have specified
//   a Prefix and the name will default to the name of the stream interface.

static void ProcessInterface (LPCTSTR class, LPCTSTR name, DWORD id, BOOL fAdd, NotifyListElement *pNote)
{
    DEVDETAIL *pDev = alloca(sizeof(DEVDETAIL) + _tcslen(name)*sizeof(TCHAR));

    DEBUGMSG(1, (L"PNP interface class %s (%s) %sTACH\n", class, name, fAdd?L"AT":L"DE"));
    
    if (pDev == NULL) {
        // OOM!
        return;
    }
    
    if (ConvertStringToGuid(class, &pDev->guidDevClass) == FALSE) {
        // WARN! bad guid syntax
        return;
    }
    pDev->fAttached = fAdd;
    pDev->dwReserved = id;
    pDev->cbName = (_tcslen(name) + 1) * sizeof(TCHAR);
    _tcscpy(&pDev->szName[0], name);

    if (pNote) {
        // if pNote is not null, we are bringing a caller of RequestDeviceNotifications() up to speed
        SendToMaybe(pNote, pDev);
    } else {
        // this must be a new interface coming online or an old one going away
        SendNotifications(pDev);
        StartDeviceNotify((LPTSTR)name, fAdd);
    }
}

#define GETREG(k,v,b,pz) RegQueryValueEx(HKEY_LOCAL_MACHINE, DEVLOAD_##v##_VALNAME, (LPDWORD)(k), NULL, (LPBYTE)(b), pz)

static void NotifyAll (LPCTSTR ActiveKey, BOOL fAdd, NotifyListElement *pNote)
{
    DWORD size, activeid;
    LPTSTR DevKey, buf = NULL, p;
    LPCTSTR key = NULL;
    TCHAR defname[65];  // temp var to hold default device name

    // The last element of the ActiveKey path is an integer; it will be used
    // in a somewhat roundabout way so that a pnp notification client can find
    // the device key for the device causing the notification to be sent.
    // Currently, only TAPI uses this and the implementation is pretty hacky.
    // But the API is valid and someday we ought to fix the implementation;
    // that would require help from filesys.
    activeid = _ttoi(_tcsrchr(ActiveKey, (TCHAR)'\\') + 1);

    // Fill in the default device name so it will be handy if we need it
    size = sizeof(defname);
    if (GETREG(ActiveKey, DEVNAME, defname, &size) != ERROR_SUCCESS) {
        defname[0] = L'\0';  // not a stream device so null out the name
    }
    
    // Look for an IClass value
    if (GETREG(ActiveKey, ICLASS, NULL, &size) == ERROR_SUCCESS) {
        key = ActiveKey;
    } else {
        size = DEVKEY_LEN * sizeof(TCHAR);
        DevKey = alloca(size);
        if (DevKey == NULL) {
            return;  // OOM!
        }
        if (GETREG(ActiveKey, DEVKEY, DevKey, &size) != ERROR_SUCCESS) {
            return;  // no DevKey - this should be an assertion failure.
        }
        if (GETREG(DevKey, ICLASS, NULL, &size) == ERROR_SUCCESS) {
            key = DevKey;
        }
        // else no IClass in the DevKey - leave key==NULL
    }

    // Now allocate and fill the buffer
    if (key != NULL) {
        buf = alloca(size + sizeof(TCHAR));     // add space for extra terminator
        if (buf) {
            if (GETREG(key, ICLASS, buf, &size) != ERROR_SUCCESS) {
                return;  // should never happen, eh?
            }
            buf[size / sizeof(TCHAR)] = L'\0';  // allows handling of both sz and multi_sz
        }
    } else {
        // NULL key means no explicit IClass so we now check to see if we should default it
        if (defname[0]) {
            // we can do this because the lack of an '=' means we won't modify it below
            buf = DEVCLASS_STREAM_STRING;
        } else {
            // WARN! This is not a stream device and it also has no
            // explicit IClass so it cannot generate PnP events.
            return;
        }
    }

    // Make sure we still have something to do
    if (buf == NULL) {
        return;  // OOM!
    }

    // Parse the IClass data
    while (*buf) {
        p = buf + _tcscspn(buf, TEXT("="));
        if (*p) {
            ASSERT(*p == L'=');
            // terminate the IClass GUID and make p point to the name
            *p++ = L'\0';
            ProcessInterface(buf, p, activeid, fAdd, pNote);
        } else {
            // There's no name so use the stream interface's name
            ProcessInterface(buf, defname, activeid, fAdd, pNote);
        }
        buf = p + _tcslen(p) + 1;
    }
}

void PnpProcessInterfaces (LPCTSTR ActiveKey, BOOL fAdd)
{
    NotifyAll(ActiveKey, fAdd, NULL);
}

void PnpSendTo (HANDLE h, DWORD dwActiveId)
{
    NotifyListElement *pNote = (NotifyListElement *) h;
    TCHAR activekey[REG_PATH_LEN];

    wsprintf(activekey, TEXT("%s\\%02d"), s_ActiveKey, dwActiveId);

    EnterCriticalSection(&g_Notifications.cs);
    // We assert that the handle is still valid because this function is only
    // called from RequestDeviceNotifications which just allocated the handle.

    NotifyAll(activekey, TRUE, pNote);

    LeaveCriticalSection(&g_Notifications.cs);
}

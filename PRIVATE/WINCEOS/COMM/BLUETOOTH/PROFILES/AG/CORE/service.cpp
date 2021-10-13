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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//


#include "btagpriv.h"


CRITICAL_SECTION    g_csLock;
DWORD               g_dwState = SERVICE_STATE_UNINITIALIZED;
CAGService*         g_pAG = NULL;
HINSTANCE           g_hInstance;

extern "C" BOOL WINAPI DllMain( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    if (DLL_PROCESS_ATTACH == fdwReason) {
        DisableThreadLibraryCalls((HMODULE)hInstDll);
        DEBUGREGISTER((HINSTANCE)hInstDll);
        g_hInstance = (HINSTANCE)hInstDll;
        InitializeCriticalSection(&g_csLock);
    }
    else if (DLL_PROCESS_DETACH == fdwReason) {
        DeleteCriticalSection(&g_csLock);
    }
    
    return TRUE;
}


// 
// The following BAG_xxxxx functions are called by services.exe to control this particular
// services dll.
//

extern "C" DWORD BAG_Init (DWORD dwData)
{
    BOOL fRetVal = TRUE;
    HKEY hk = NULL;
    DWORD cdwBytes;
    DWORD dwStartService = FALSE;

    EnterCriticalSection(&g_csLock);

    if (! g_pAG) {
        DWORD dwErr;
        
        g_pAG = new CAGService;
        if (! g_pAG) {
            fRetVal = FALSE;
            SetLastError(ERROR_OUTOFMEMORY);
            goto exit;
        }

        dwErr = g_pAG->Init();
        if (ERROR_SUCCESS != dwErr) {
            fRetVal = FALSE;
            SetLastError(dwErr);
            goto exit;
        }

		dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Bluetooth\\HandsfreeProfileOn", 0, 0, &hk);
		if (ERROR_SUCCESS != dwErr) {
		    fRetVal = FALSE;
		    SetLastError(dwErr);
		    goto exit;
		}
		RegCloseKey(hk);
		hk = NULL;

        dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AG_SERVICE, 0, 0, &hk);
        if (ERROR_SUCCESS != dwErr) {
            fRetVal = FALSE;
            SetLastError(dwErr);
            goto exit;
        }

        cdwBytes = sizeof(dwStartService);
        RegQueryValueEx(hk, _T("IsEnabled"), 0, NULL, (PBYTE)&dwStartService, &cdwBytes);

        if (dwStartService) {
            g_dwState = SERVICE_STATE_STARTING_UP;

            dwErr = g_pAG->Start();
            if (ERROR_SUCCESS != dwErr) {
                fRetVal = FALSE;
                SetLastError(dwErr);
                goto exit;
            }

            g_dwState = SERVICE_STATE_ON;
        }
    }
    else {
        fRetVal = FALSE;
        SetLastError(ERROR_ALREADY_INITIALIZED);
    }

exit:
    if ((FALSE == fRetVal) && g_pAG) {
        g_pAG->Deinit();
        delete g_pAG;
        g_pAG = NULL;
        g_dwState = SERVICE_STATE_UNINITIALIZED;
    }

    if (hk) {
        RegCloseKey(hk);
    }
    
    LeaveCriticalSection(&g_csLock);
    
    return fRetVal;
}

extern "C" BOOL BAG_Deinit(DWORD dwData)
{
    g_dwState = SERVICE_STATE_SHUTTING_DOWN;
    
    EnterCriticalSection(&g_csLock);

    if (g_pAG) {
        g_pAG->Stop();
        g_dwState = SERVICE_STATE_UNLOADING;
        g_pAG->Deinit();
        delete g_pAG;
        g_pAG = NULL;
    }

    g_dwState = SERVICE_STATE_UNINITIALIZED;

    LeaveCriticalSection(&g_csLock);
    
    return TRUE;
}

extern "C" DWORD BAG_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
    return TRUE;
}

extern "C" BOOL BAG_Close (DWORD dwData) 
{
    return TRUE;
}

extern "C" DWORD BAG_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen)
{
    return -1;
}

extern "C" DWORD BAG_Read (DWORD dwData, LPVOID pBuf, DWORD dwLen)
{
    return -1;
}

extern "C" DWORD BAG_Seek (DWORD dwData, long pos, DWORD type)
{
    return (DWORD)-1;
}

extern "C" void BAG_PowerUp(void)
{
    return;
}

extern "C" void BAG_PowerDown(void)
{
    return;
}

extern "C" BOOL BAG_IOControl(DWORD dwData, DWORD dwCode, PBYTE pBufIn,
    DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
    PDWORD pdwActualOut)
{
    DWORD dwErr;
    HKEY hk;
    DWORD dwRegData;
    
    switch (dwCode) {
        case IOCTL_SERVICE_START:
            if (g_pAG) {
                g_dwState = SERVICE_STATE_STARTING_UP;

                EnterCriticalSection(&g_csLock);
               
                dwErr = g_pAG->Start();                
                if (ERROR_SUCCESS != dwErr) {
                    LeaveCriticalSection(&g_csLock);
                    g_dwState = SERVICE_STATE_OFF;
                    SetLastError(dwErr);
                    return FALSE;
                }

                LeaveCriticalSection(&g_csLock);

                dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AG_SERVICE, 0, 0, &hk);
                if (ERROR_SUCCESS != dwErr) {
                    LeaveCriticalSection(&g_csLock);
                    g_dwState = SERVICE_STATE_OFF;
                    SetLastError(dwErr);
                    return FALSE;
                }

                dwRegData = 1;
                RegSetValueEx(hk, _T("IsEnabled"), 0, REG_DWORD, (PBYTE)&dwRegData, sizeof(dwRegData));

                RegCloseKey(hk);

                g_dwState = SERVICE_STATE_ON;
                
                return TRUE;
            }

            SetLastError(ERROR_NOT_READY);
            return FALSE;

        case IOCTL_SERVICE_REFRESH:
            if (g_pAG) {
                g_dwState = SERVICE_STATE_SHUTTING_DOWN;

                EnterCriticalSection(&g_csLock);
                
                g_pAG->Stop();                
                g_dwState = SERVICE_STATE_STARTING_UP;
                
                dwErr = g_pAG->Start();                
                if (ERROR_SUCCESS != dwErr) {
                    LeaveCriticalSection(&g_csLock);
                    g_dwState = SERVICE_STATE_OFF;
                    SetLastError(dwErr);
                    return FALSE;
                }

                g_dwState = SERVICE_STATE_ON;

                LeaveCriticalSection(&g_csLock);
                                
                return TRUE;
            }

            SetLastError(ERROR_NOT_READY);
            return FALSE;
            
        case IOCTL_SERVICE_STOP:
            if (g_pAG) {
                g_dwState = SERVICE_STATE_SHUTTING_DOWN;

                EnterCriticalSection(&g_csLock);
                
                g_pAG->Stop();

                g_dwState = SERVICE_STATE_OFF;

                LeaveCriticalSection(&g_csLock);

                dwErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AG_SERVICE, 0, 0, &hk);
                if (ERROR_SUCCESS != dwErr) {
                    LeaveCriticalSection(&g_csLock);
                    g_dwState = SERVICE_STATE_OFF;
                    SetLastError(dwErr);
                    return FALSE;
                }

                dwRegData = 0;
                RegSetValueEx(hk, _T("IsEnabled"), 0, REG_DWORD, (PBYTE)&dwRegData, sizeof(dwRegData));

                RegCloseKey(hk);
                
                return TRUE;
            }

            SetLastError(ERROR_NOT_READY);
            return FALSE;

        case IOCTL_SERVICE_STATUS:
            if (g_pAG && pBufOut && (dwLenOut == sizeof(DWORD)))  {
                BOOL fRetVal = TRUE;
                __try {
                    *(DWORD *)pBufOut = g_dwState;
                    if (pdwActualOut)
                        *pdwActualOut = sizeof(DWORD);
                } __except (1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }
                return fRetVal;
            }
            break;

        case IOCTL_AG_OPEN_AUDIO:
            dwErr = g_pAG->OpenAudio();
            if (ERROR_SUCCESS != dwErr) {
                SetLastError(dwErr);
                return FALSE;
            }

            return TRUE;

        case IOCTL_AG_CLOSE_AUDIO:
            dwErr = g_pAG->CloseAudio();
            if (ERROR_SUCCESS != dwErr) {
                SetLastError(dwErr);
                return FALSE;
            }
            
            return TRUE;

        case IOCTL_AG_OPEN_CONTROL:
        {
            BOOL fFirstOnly = FALSE;
            BOOL fRetVal = TRUE;

            __try {
                if (pBufIn && (dwLenIn >= sizeof(BOOL))) {
                    fFirstOnly = *(BOOL*)pBufIn;
                }
            } __except(1) {
                fRetVal = FALSE;
                SetLastError(ERROR_INVALID_PARAMETER);
            }

            if (fRetVal) {
                dwErr = g_pAG->OpenControlConnection(fFirstOnly);
                if (ERROR_SUCCESS != dwErr) {
                    SetLastError(dwErr);
                    fRetVal = FALSE;
                }
            }
            
            return fRetVal;
        }            
            

        case IOCTL_AG_CLOSE_CONTROL:
            dwErr = g_pAG->CloseControlConnection();
            if (ERROR_SUCCESS != dwErr) {
                SetLastError(dwErr);
                return FALSE;
            }

            return TRUE;

        case IOCTL_AG_SET_SPEAKER_VOL:
            if (pBufIn && (dwLenIn >= sizeof(USHORT))) {
                BOOL fRetVal = TRUE;
                USHORT usVol = 0;
                
                __try {
                    usVol = *(PUSHORT)pBufIn;
                } __except(1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                if (fRetVal) {
                    dwErr = g_pAG->SetSpeakerVolume(usVol);
                    if (ERROR_SUCCESS != dwErr) {
                        SetLastError(dwErr);
                        fRetVal = FALSE;
                    }
                }
                
                return fRetVal;
            }
            
            break;

        case IOCTL_AG_SET_MIC_VOL:
            if (pBufIn && (dwLenIn >= sizeof(USHORT))) {
                BOOL fRetVal = TRUE;
                USHORT usVol;

                __try {
                    usVol = *(PUSHORT)pBufIn;
                } __except(1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                if (fRetVal) {
                    dwErr = g_pAG->SetMicVolume(usVol);
                    if (ERROR_SUCCESS != dwErr) {
                        SetLastError(dwErr);
                        fRetVal = FALSE;
                    }
                }
                
                return fRetVal;
            }

            break;

        case IOCTL_AG_GET_SPEAKER_VOL:
            if (pBufOut && (dwLenOut >= sizeof(USHORT))) {
                USHORT usVol = 0;
                dwErr = g_pAG->GetSpeakerVolume(&usVol);
                if (ERROR_SUCCESS != dwErr) {
                    SetLastError(dwErr);
                    return FALSE;
                }

                BOOL fRetVal = TRUE;
                
                __try {
                    *(USHORT*)pBufOut = usVol;
                    if (pdwActualOut)
                        *pdwActualOut = sizeof(usVol);
                } __except(1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                return fRetVal;
            }

            break;

        case IOCTL_AG_GET_MIC_VOL:
            if (pBufOut && (dwLenOut >= sizeof(USHORT))) {
                USHORT usVol = 0;
                dwErr = g_pAG->GetMicVolume(&usVol);
                if (ERROR_SUCCESS != dwErr) {
                    SetLastError(dwErr);
                    return FALSE;
                }

                BOOL fRetVal = TRUE;

                __try {
                    *(USHORT*)pBufOut = usVol;
                    if (pdwActualOut)
                        *pdwActualOut = sizeof(usVol);
                } __except(1) {
                    fRetVal = FALSE;    
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                return fRetVal;
            }

            break;

        case IOCTL_AG_GET_POWER_MODE:
            if (pBufOut && (dwLenOut >= sizeof(BOOL))) {
                BOOL fMode = 0;
                dwErr = g_pAG->GetPowerMode(&fMode);
                if (ERROR_SUCCESS != dwErr) {
                    SetLastError(dwErr);
                    return FALSE;
                }

                BOOL fRetVal = TRUE;

                __try {
                    *(USHORT*)pBufOut = fMode;
                    if (pdwActualOut)
                        *pdwActualOut = sizeof(fMode);
                } __except(1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }                

                return fRetVal;
            }
            
            break;

        case IOCTL_AG_SET_POWER_MODE:
            if (pBufIn && (dwLenIn >= sizeof(BOOL))) {                
                BOOL fRetVal = TRUE;
                BOOL fMode;

                __try {
                    fMode = (*(PBOOL)pBufIn) ? TRUE : FALSE;
                } __except (1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                if (fRetVal) {
                    dwErr = g_pAG->SetPowerMode(fMode);
                    if (ERROR_SUCCESS != dwErr) {
                        SetLastError(dwErr);
                        fRetVal = FALSE;
                    }
                }
                
                return fRetVal;
            }

            break;      

        case IOCTL_AG_SET_USE_HF_AUDIO:
            if (pBufIn && (dwLenIn >= sizeof(BOOL))) {
                BOOL fRetVal = TRUE;
                BOOL fMode;

                __try {
                    fMode = (*(PBOOL)pBufIn) ? TRUE : FALSE;
                } __except (1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                if (fRetVal) {
                    dwErr = g_pAG->SetUseHFAudio(fMode);
                    if (ERROR_SUCCESS != dwErr) {
                        SetLastError(dwErr);
                        fRetVal = FALSE;
                    }
                }

                return fRetVal;
            }

            break;

        case IOCTL_AG_SET_INBAND_RING:
            if (pBufIn && (dwLenIn >= sizeof(BOOL))) {
                BOOL fRetVal = TRUE;
                BOOL fInbandRing;

                __try {
                    fInbandRing = (*(PBOOL)pBufIn) ? TRUE : FALSE;
                } __except (1) {
                    fRetVal = FALSE;
                    SetLastError(ERROR_INVALID_PARAMETER);
                }

                if (fRetVal) {
                    dwErr = g_pAG->SetUseInbandRing(fInbandRing);
                    if (ERROR_SUCCESS != dwErr) {
                        SetLastError(dwErr);
                        fRetVal = FALSE;
                    }
                }

                return fRetVal;
            }

            break;

        case IOCTL_SERVICE_DEREGISTER_SOCKADDR:
        case IOCTL_SERVICE_DEBUG:
        case IOCTL_SERVICE_STARTED:
        case IOCTL_SERVICE_CONTROL:
            return TRUE;

        case IOCTL_SERVICE_CONNECTION:
        case IOCTL_SERVICE_REGISTER_SOCKADDR:
            return FALSE;
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}




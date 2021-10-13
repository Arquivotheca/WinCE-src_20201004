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
#ifndef __WIFIPOWER_HPP_
#define __WIFIPOWER_HPP_

#pragma once

#include <windows.h>
#include <tchar.h>
#include <Pm.h>
#include <ntddndis.h>
#include <strsafe.h>
#include <ndispwr.h>
#include <Winbase.h>
#include <eapol.h>
#include <wzcsapi.h>
#include <netmain.h>
#include <cmnprint.h>
#include <CUIOAdapter.h>

#define MAX_ADAPTER_NAME_SIZE 256

extern TCHAR g_szAdapter[MAX_ADAPTER_NAME_SIZE ];

namespace ce {
namespace qa {

// TODO: Move to cpp
static const DWORD _MAX_QUERY_BUFF_BYTES_SIZE_  = 512;

enum { PM_UNSPECIFIED =0, PM_AWARE,  PM_UNAWARE };

class WifiDevicePowerManager
{
private :
      TCHAR szAdapter[MAX_PATH];
      
      DWORD dwFlagPMAware;
      DWORD dwNicPowerState;

      NDIS_PNP_CAPABILITIES sPNP;
      
      CUIOAdapter oUIO;

      WifiDevicePowerManager() 
      { 
          pSingletonInstance = NULL; 
          szAdapter[0]=0; 
          dwFlagPMAware =  PM_UNSPECIFIED;
          dwNicPowerState = PwrDeviceUnspecified;
      }
      
      WifiDevicePowerManager(const WifiDevicePowerManager& src);
      WifiDevicePowerManager& operator=(const WifiDevicePowerManager& src);      

      static WifiDevicePowerManager* pSingletonInstance;

    // Will try to get the adapter name specified by the user or query automatically 
    // and try to open the specified adapter in read write mode
     DWORD Init()
    {
        DWORD dwResult = ERROR_SUCCESS;
        
        if (g_szAdapter[0] != 0)
        {
            // User specified an adapter on the command line
            HRESULT hr = StringCbCopy(szAdapter, sizeof(szAdapter), g_szAdapter);
            if (hr != S_OK)
            {
                CmnPrint(PT_FAIL,TEXT("Cannot copy name of adapter, hr = %x "),hr);
                return ERROR_INVALID_PARAMETER;
            }
        }
        else
        {
            dwResult = QueryWiFiAdapter();
            if (dwResult != ERROR_SUCCESS)
            {
                CmnPrint(PT_FAIL, TEXT("User did not specify an adapter name and querying failed"));
                return dwResult;
            }
        }
        
        if (!oUIO.Open(szAdapter) && !oUIO.Open(szAdapter, GENERIC_WRITE))
        {
            CmnPrint(PT_FAIL,TEXT("Failed to Associate %s adapter with NDISUIO"),szAdapter);                
            return ERROR_INVALID_STATE;
        }                
        else
            CmnPrint(PT_LOG, TEXT("Found adaper %s specified by user"), szAdapter);
        return ERROR_SUCCESS;
        
    }

     DWORD QueryWiFiAdapter()
     {
        DWORD dwStatus;
        
        szAdapter[0] = L'\0';
        INTFS_KEY_TABLE IntfsTable;
        IntfsTable.dwNumIntfs = 0;
        IntfsTable.pIntfs = NULL;

        CmnPrint(PT_LOG,TEXT("Trying to determine wifi adapter automatically"));

        dwStatus = WZCEnumInterfaces(NULL, &IntfsTable);

        if(dwStatus != ERROR_SUCCESS)
        {
            CmnPrint(PT_FAIL, TEXT("WZCEnumInterfaces() error 0x%08X\n"), dwStatus);
            return dwStatus;        
        }

        // print the GUIDs
        // note that in CE the GUIDs are simply the device instance name
        // i.e XWIFI11B1, CISCO1, ISLP2, ...
        //

        if(!IntfsTable.dwNumIntfs)
        {
            CmnPrint(PT_FAIL, TEXT("system has no wireless card.\n"));
            return ERROR_FUNCTION_FAILED;
        }

        wcsncpy(szAdapter, IntfsTable.pIntfs[0].wszGuid, MAX_PATH-1);
        CmnPrint(PT_LOG, TEXT("wireless card found: %s\n"), szAdapter);

        // need to free memory allocated by WZC for us.
        LocalFree(IntfsTable.pIntfs);

        return dwStatus;
     }


public:
       
    static WifiDevicePowerManager* GetInstance()
    {
        if (pSingletonInstance)
            return pSingletonInstance;
        else
        {
            WifiDevicePowerManager* pWDPM = new WifiDevicePowerManager();                
            if (ERROR_SUCCESS == pWDPM->Init())
            {
                pSingletonInstance = pWDPM;
            }            
            else
                delete pWDPM;
            return pSingletonInstance;
        }
    }

    VOID QueryPnpCapabilities()
    {
        DWORD dwQBuffer[_MAX_QUERY_BUFF_BYTES_SIZE_/sizeof(DWORD)];
        PBYTE pQuerybuffer = PBYTE (dwQBuffer);
        DWORD dwBuffSize = _MAX_QUERY_BUFF_BYTES_SIZE_;
        DWORD dwBytesReturned = 0;     
        
        if (oUIO.QueryOID(OID_PNP_CAPABILITIES,pQuerybuffer,dwBuffSize,dwBytesReturned,szAdapter))
        {
            dwFlagPMAware = PM_AWARE;
            memcpy(&(sPNP), pQuerybuffer, sizeof(NDIS_PNP_CAPABILITIES));            
            CmnPrint(PT_LOG, TEXT("Querying Oid for Pnp capability succeeded"));
        }    
        else
        {
            dwFlagPMAware = PM_UNAWARE;
            CmnPrint(PT_LOG, TEXT("Querying Oid for Pnp capability failed with error %d"), GetLastError());
        }
    }

    DWORD GetDeviceState(PDWORD pdwWiFiNicState)
    {
        DWORD dwResult = ERROR_SUCCESS;
        
        if(!dwFlagPMAware)
            QueryPnpCapabilities();
        
        if(dwFlagPMAware != PM_AWARE)
        {
            dwNicPowerState = PwrDeviceUnspecified;
            CmnPrint(PT_FAIL, TEXT("Cannot query power state as driver is not Power Manager aware"));
            dwResult = ERROR_FUNCTION_FAILED;
        }
        else
         {
            //Get NIC's power state.
            TCHAR                szName[MAX_PATH];        
            int                  nChars;        
            nChars = _sntprintf(szName, MAX_PATH-1, TEXT("%s\\%s"), PMCLASS_NDIS_MINIPORT,
                szAdapter);

            szName[MAX_PATH-1]=0;

            if(nChars != (-1)) 
            {
                if (ERROR_SUCCESS != (dwResult = GetDevicePower(szName, POWER_NAME, 
                    PCEDEVICE_POWER_STATE(&(dwNicPowerState)))))
                {
                    dwNicPowerState = PwrDeviceUnspecified;
                    CmnPrint(PT_FAIL,TEXT("Error: Failed to get power state for %s"), szAdapter);
                }
                else
                {
                    CmnPrint(PT_LOG,TEXT("Successfully got device power state %d"), dwNicPowerState);
                }                
            }
            else
            {
                CmnPrint(PT_FAIL, TEXT("Unable to create device name into buffer of size %d bytes"), sizeof(szName));
            }
        }

        if (pdwWiFiNicState)        
            *pdwWiFiNicState = dwNicPowerState;
        else
        {
             CmnPrint(PT_FAIL,TEXT("Pointer to variable to store power state is invalid"));
             dwResult = ERROR_INVALID_PARAMETER;
        }

        return dwResult;
    }


     DWORD SetDeviceState(DWORD dwNewState)
     {
        DWORD dwResult = ERROR_SUCCESS;
        
        if (!dwFlagPMAware)
              QueryPnpCapabilities();

        if (dwFlagPMAware == PM_UNAWARE)
        {
            CmnPrint(PT_FAIL, TEXT("Cannot set power state as driver is not Power Manager aware"));
            dwResult = ERROR_FUNCTION_FAILED;
        }

        TCHAR                szName[MAX_PATH];        
        int                  nChars;        
        nChars = _sntprintf(szName, MAX_PATH-1, TEXT("%s\\%s"), PMCLASS_NDIS_MINIPORT,
                szAdapter);
        szName[MAX_PATH-1]=0;

        if (nChars != (-1))
        {
            dwResult = SetDevicePower(szName, POWER_NAME,  (CEDEVICE_POWER_STATE) dwNewState);
            if (dwResult == ERROR_SUCCESS)
            {
                CmnPrint(PT_LOG,TEXT("Successfully set Device state to %d"), dwNewState);
            }
            else
            {
                CmnPrint(PT_FAIL,TEXT("Failed to set Device state to %d, Error %d"), dwNewState, dwResult);
            }
         }
        else
        {
            CmnPrint(PT_FAIL, TEXT("Unable to create device name into buffer of size %d bytes"), sizeof(szName));
        }

        return dwResult;
     }

     DWORD
     SetPowerSaveMode(const DWORD dwNewMode)
     {
        DWORD dwResult = ERROR_SUCCESS; 
        BOOL fResult;
        
        DWORD dwBufferSize =sizeof(NDISUIO_SET_OID) + sizeof(dwNewMode);
        BYTE* pBuffer = new BYTE [dwBufferSize];
        if (!pBuffer)
        {
            CmnPrint(PT_FAIL, TEXT("Failed to allocate memory of size %d in SetPowerSaveMode"), dwBufferSize);
            return ERROR_OUTOFMEMORY;
        }
        memcpy(pBuffer, &dwNewMode, min(dwBufferSize, sizeof(dwNewMode)));
                    
        fResult =  oUIO.SetOID(OID_802_11_POWER_MODE, 
                pBuffer, dwBufferSize, 
                sizeof(dwNewMode), szAdapter);        

        if (!fResult)
        {
            dwResult = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER == dwResult)
            {
                CmnPrint(PT_FAIL, TEXT("Insufficient buffer while setting power save mode oid"));
                CmnPrint(PT_FAIL, TEXT("Trying again with size %d"), dwBufferSize);
                
                pBuffer = new BYTE [dwBufferSize];
                if (!pBuffer)
                {
                    CmnPrint(PT_FAIL, TEXT("Failed to allocate memory of size %d in SetPowerSaveMode"), dwBufferSize);
                    return ERROR_OUTOFMEMORY;
                }
                memcpy(pBuffer, &dwNewMode, min(dwBufferSize, sizeof(dwNewMode)));
                            
                fResult =  oUIO.SetOID(OID_802_11_POWER_MODE, 
                        pBuffer, dwBufferSize, 
                        sizeof(dwNewMode), szAdapter);        
            }

            if (!fResult)
            {
                dwResult = GetLastError();
                CmnPrint(PT_FAIL, TEXT("Error %d in setting oid power save mode %d"), dwResult, dwNewMode);
            }
            else
            {
                // If failure is not due to system error, then dwResult may still be ERROR_SUCCESS as GetLastError returned 0
                if (!dwResult)
                    dwResult = ERROR_INVALID_STATE;
            }

        }       

        if (fResult)
        {
            CmnPrint(PT_LOG,TEXT("Set Power save mode to %d"), dwNewMode);
        }


        return dwResult;
        
     }

     DWORD
     GetPowerSaveMode(PDWORD pdwCurrentState)
     {
        DWORD dwResult = ERROR_SUCCESS; 
        BOOL fResult;
        
        if (!pdwCurrentState)
        {
            CmnPrint(PT_FAIL, TEXT("Error Invalid pointer to get Current Power Save mode"));
            return ERROR_INVALID_PARAMETER;
        }

        DWORD dwBufferSize =sizeof(NDISUIO_SET_OID) + sizeof(DWORD);
        BYTE* pBuffer = new BYTE [dwBufferSize];
        if (!pBuffer)
        {
            CmnPrint(PT_FAIL, TEXT("Failed to allocate memory of size %d in GetPowerSaveMode"), dwBufferSize);
            return ERROR_OUTOFMEMORY;
        }
        
        DWORD dwNumberOfBytesReturned = 0;      
        fResult = oUIO.QueryOID(OID_802_11_POWER_MODE, 
                pBuffer, dwBufferSize, 
                dwNumberOfBytesReturned, szAdapter);

        if (!fResult)
        {
            dwResult = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER == dwResult)
            {
                CmnPrint(PT_FAIL, TEXT("Insufficient buffer while querying power save mode oid"));
                CmnPrint(PT_FAIL, TEXT("Trying again with size %d"), dwBufferSize);
                
                pBuffer = new BYTE [dwBufferSize];
                if (!pBuffer)
                {
                    CmnPrint(PT_FAIL, TEXT("Failed to allocate memory of size %d in GetPowerSaveMode"), dwBufferSize);
                    return ERROR_OUTOFMEMORY;
                }
                            
                fResult =  oUIO.QueryOID(OID_802_11_POWER_MODE, 
                        &pBuffer, dwBufferSize, 
                        dwNumberOfBytesReturned, szAdapter);        
            }

            if (!fResult)
            {
                dwResult = GetLastError();
                CmnPrint(PT_FAIL, TEXT("Error %d in getting oid power save mode"), dwResult);
            }
        } 
        
        if (fResult)
        {
            memcpy(pdwCurrentState, pBuffer, min(dwBufferSize, sizeof(DWORD)));
            CmnPrint(PT_LOG,TEXT("Got Power save mode %d"),*pdwCurrentState);
        }
        else
        {
            // If failure is not due to system error, then dwResult may still be ERROR_SUCCESS as GetLastError returned 0
            if (!dwResult)
                dwResult = ERROR_INVALID_STATE;
        }
        return dwResult;
     }
};

}
}

#endif

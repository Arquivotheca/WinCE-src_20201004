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

#include "BtA2dpPair.h"

#include <bt_api.h>



#ifndef PREFAST_ASSERT
#define PREFAST_ASSERT(x) ASSERT(x)
#endif

#ifndef BOOLIFY
#define BOOLIFY(e)      (!!(e))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))
#endif

static const DWORD MAX_SUBKEY_NUM = 4;
static const DWORD MIN_SUBKEY_NUM = 1;
static const size_t PATH = 256;

//
// This function gets the list of HF devices from the registry
//

static const TCHAR pRegistryAddress[] = TEXT("SOFTWARE\\Microsoft\\Bluetooth\\A2DP\\Devices");
static BD_ADDR ReadRegistryBDAddress(DWORD SubKeyNumber)
{
    //figure out if within the range of possible registry entries
    BD_ADDR Ret;
    memset(&Ret, 0, sizeof (BD_ADDR));
    ASSERT(MAX_SUBKEY_NUM >= SubKeyNumber && MIN_SUBKEY_NUM <= SubKeyNumber);
    if(!(MAX_SUBKEY_NUM >= SubKeyNumber && MIN_SUBKEY_NUM <= SubKeyNumber))
        return Ret;


    //build sub key name
    WCHAR pSubKey[PATH];
    StringCchPrintfW(pSubKey, PATH, TEXT("%s\\%u"), pRegistryAddress, SubKeyNumber);


    HKEY    Hk;


    //open key
    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, pSubKey, 0, KEY_READ, &Hk) == ERROR_SUCCESS) 
        {
        BT_ADDR pBtAddr[1];
        pBtAddr[0] = (BT_ADDR)0;
        DWORD Type = 0;
        DWORD Size;

        Size = sizeof(pBtAddr[0]);
        BD_ADDR * a;
        if ((RegQueryValueEx (Hk, L"Address", NULL, &Type, (LPBYTE)pBtAddr, &Size) == ERROR_SUCCESS) &&
            (Type == REG_BINARY)) 
                {
                a = (BD_ADDR*) pBtAddr;
                Ret = *a;
                }

        RegCloseKey (Hk);
    }

    return Ret;
}

DWORD WriteListToRegistry(BD_ADDR* pBdAdresses, const USHORT usNumDevices)
{
    DWORD       dwRetVal    = ERROR_SUCCESS;
    DWORD       dwDis       = 0;
    HKEY        hk          = NULL;
    BT_ADDR     bt_temp     = 0;
    BD_ADDR zero;
    memset(&zero, 0, sizeof (BD_ADDR));
    // Write the list back to the registry
     dwRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE, pRegistryAddress, 0, NULL, 0, NULL, NULL, &hk, &dwDis);
    if (ERROR_SUCCESS == dwRetVal) {
        int dwIdx = 0;
        DWORD cbName = MAX_PATH;

        while ((dwIdx < usNumDevices))// && (0 != memcmp(&pBdAdresses[dwIdx], &zero, sizeof (BD_ADDR))))
        {
            HKEY hkAddr;
            WCHAR wszIdx[10];            
            _itow(dwIdx+1, wszIdx, 10);

            dwDis = 0;
            DWORD dwErr = RegCreateKeyEx(hk, wszIdx, 0, NULL, 0, NULL, NULL, &hkAddr, &dwDis);
            if (ERROR_SUCCESS != dwErr) {
                //DEBUGMSG(1, (L"BTA2DPPAIR: Could not open registry key for BT Addr: %d.\n", dwRetVal));        
                break;
            }
            bt_temp = SET_NAP_SAP(pBdAdresses[dwIdx].NAP, pBdAdresses[dwIdx].SAP);
            dwErr = RegSetValueEx(hkAddr, L"Address", NULL, REG_BINARY, (PBYTE)&bt_temp, sizeof(BT_ADDR));
            if (dwErr != ERROR_SUCCESS) {
                RegCloseKey(hkAddr);
                //DEBUGMSG(1, (L"BTA2DPPAIR: Could not set registry value for BT Addr: %d.\n", dwErr));        
                break;
            }

            //DEBUGMSG(1, (L"BTA2DPPAIR: Saving to registry btAddr %04x%08x at index %d.\n", GET_NAP(bt_temp), GET_SAP(bt_temp), dwIdx+1));

            RegCloseKey(hkAddr);
            
            dwIdx++;        
        }

        RegCloseKey (hk);
    }
    return dwRetVal;
}

//returns number of non-zero adresses read
DWORD GetBDAddrList(BD_ADDR* pBdAdresses, const USHORT usNumDevices)
{
    DWORD dwRetVal = 0;
    HKEY hk = NULL;
    int dwIdx = 0;
    WCHAR szName[MAX_PATH];
    DWORD cchName = ARRAY_SIZE(szName);
    BD_ADDR temp;
    BD_ADDR zero;

    memset(&temp, 0, sizeof (BD_ADDR));
    memset(&zero, 0, sizeof (BD_ADDR));
    
    ASSERT(pBdAdresses && usNumDevices);
    if(usNumDevices < MAX_NUM_BT_ADRESSES)
        {
        // Clear the current list
        memset(pBdAdresses, 0, usNumDevices*sizeof(pBdAdresses[0]));

        for(UINT i=1; i<= usNumDevices; i++)
            {
            temp = ReadRegistryBDAddress(i);
            if(0 == memcmp(&temp, &zero, sizeof (BD_ADDR)))
                break;

            //DEBUGMSG(1, (L"BTA2DPPAIR: Setting bdAddr at index %d to %x.\n", i, temp)); 
            pBdAdresses[i-1] = temp;
            }
            
        dwRetVal = i-1;
        }
    else
        {
        dwRetVal = 0;
        }
    
    
    return dwRetVal;
    
    
}


BOOL FindBDAddrInList(const BD_ADDR bdaSearch)
{
    BOOL fFound = FALSE;
    BD_ADDR pBdAdresses[MAX_SUBKEY_NUM];


    GetBDAddrList(pBdAdresses, MAX_SUBKEY_NUM); // Update from registry

    // Loop through list until we find the address
    int i = 0;
    while ((i < MAX_SUBKEY_NUM) && (0 != memcmp(&bdaSearch, &pBdAdresses[i], sizeof (BD_ADDR)))) {
        i++;
    }

    if (i < MAX_SUBKEY_NUM) {
        fFound = TRUE;
    }

    return fFound;
}



// This method writes the address list back to the registry
// sets the parameter btAddrDefault at the top of the list
// and as the active device.
DWORD SetBDAddrList(BD_ADDR* pBdAdresses, const USHORT cBdAdresses, BD_ADDR bdAddrDefault)
{
    DWORD       dwRetVal    = ERROR_SUCCESS;
    DWORD       dwDis       = 0;
    HKEY        hk          = NULL;
    BT_ADDR     bt_temp     = 0;
    BD_ADDR zero;
    memset(&zero, 0, sizeof (BD_ADDR));

    GetBDAddrList(pBdAdresses, cBdAdresses); // Update from registry

    // Loop through list until we find the address
    int i = 0;
    while ((i < cBdAdresses) && (0 != memcmp(&bdAddrDefault, &pBdAdresses[i], sizeof (BD_ADDR)))) {
        i++;
    }
    
    if (i > 0) {
        if(i == cBdAdresses)//address not found
        {//only want to move cBdAdresses-1 one elements (avoid buffer overflow)
        i--;
        }
        memmove(&pBdAdresses[1], pBdAdresses, i*sizeof(pBdAdresses[0]));
        
        pBdAdresses[0] = bdAddrDefault;
    }


    dwRetVal = WriteListToRegistry(pBdAdresses, cBdAdresses);
    return dwRetVal;
}

//This method deletes a *BT* address from the registry
//returns success or failure
DWORD DeleteBTAddr(BT_ADDR BtAdressDelete)
{
    DWORD       dwRetVal    = ERROR_SUCCESS;
    DWORD       dwDis       = 0;
    HKEY        hk          = NULL;
    BT_ADDR     bt_temp     = 0;
    BD_ADDR zero;
    memset(&zero, 0, sizeof (BD_ADDR));
    BD_ADDR pBdAdresses[MAX_SUBKEY_NUM];
    if(0 == BtAdressDelete)
        {
        return ERROR_BAD_ARGUMENTS;
        }
    GetBDAddrList(pBdAdresses, ARRAY_SIZE(pBdAdresses)); // Update from registry

    // Loop through list until we find the address
    int i = 0;
    while ((i < ARRAY_SIZE(pBdAdresses)))
        {
        bt_temp = SET_NAP_SAP(pBdAdresses[i].NAP, pBdAdresses[i].SAP);
        if(bt_temp != BtAdressDelete)
            {
            i++;
            }
        else
            {
            break;
            }
        }
    
    if (i < ARRAY_SIZE(pBdAdresses)) 
        {//found address
        if(i != ARRAY_SIZE(pBdAdresses)-1)
            {//i is not last element in the array
            memmove(&pBdAdresses[i], &pBdAdresses[i+1],(ARRAY_SIZE(pBdAdresses)-(i+1))*sizeof(pBdAdresses[0]));
            }
        memset(&pBdAdresses[ARRAY_SIZE(pBdAdresses)-1], 0, sizeof(pBdAdresses[0]));

        }

    // Write the list back to the registry
    dwRetVal = WriteListToRegistry(pBdAdresses, ARRAY_SIZE(pBdAdresses));
    return dwRetVal;
}

//This method adds a *BT* address ti the registry, at higheest priority
//returns success or failure
DWORD AddBTAddr(BT_ADDR BtAdressAdd)
{
    DWORD       dwRetVal    = ERROR_SUCCESS;

    BD_ADDR pBdAdresses[MAX_SUBKEY_NUM];
    BD_ADDR BdAdressAdd = *((BD_ADDR*) &BtAdressAdd);
    dwRetVal = SetBDAddrList(pBdAdresses, ARRAY_SIZE(pBdAdresses), BdAdressAdd);
    return dwRetVal;
}



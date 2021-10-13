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
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "string.h"
#include "Eapcfg.h"
#include "Raseapif.h"
#include "EapMethod.hpp"
#include "WiFUtils.hpp"
#include "WiFiRegHelper.hpp"

using namespace ce::qa;

extern "C"
DWORD APIENTRY
RasEapGetIdentity(
   IN  DWORD           dwEapTypeId,
   IN  HWND            hwndParent,
   IN  DWORD           dwFlags,
   IN  const WCHAR*    pwszPhonebook,
   IN  const WCHAR*    pwszEntry,
   IN  BYTE*           pConnectionDataIn,
   IN  DWORD           dwSizeOfConnectionDataIn,
   IN  BYTE*           pUserDataIn,
   IN  DWORD           dwSizeOfUserDataIn,
   OUT BYTE**          ppUserDataOut,
   OUT DWORD*          pcbSizeOfUserDataOut,
   OUT WCHAR**         ppwszIdentity
)
{


   if( NULL == pcbSizeOfUserDataOut || 
       NULL == ppUserDataOut || NULL == ppwszIdentity )
    {
       LogError(_T("Some input paramets are NULL"));
       return ERROR_INVALID_PARAMETER;
    }   
   
   TCHAR  regPath[128];
   TCHAR  UserName[128];
   TCHAR  PassWD[128];	
   TCHAR  Domain[128];	

   _stprintf_s(regPath, _countof(regPath),_T("%s\\%d"),LEGACY_TESTMETHOD_PATH, dwEapTypeId);	

   WiFiRegHelper  testRegHelper;
   if(!testRegHelper.Init(regPath))
   {
      LogError(_T("Failed to create registry helper key : %s"),regPath);
      return ERROR_ACCESS_DENIED;
   }    	

   if(!testRegHelper.OpenKey())
   {
      LogError(_T("Failed to open key for %s"),regPath);
      return ERROR_ACCESS_DENIED;
   } 	

   DWORD keyLen = 128;
   if(!testRegHelper.ReadStrValue(LEGACY_TESTMETHOD_USER, UserName,(PLONG)&keyLen))   
   {
      LogError(_T("Failed to read value for user name"));
      return ERROR_ACCESS_DENIED;
   }

   keyLen = 128;
   if(!testRegHelper.ReadStrValue(LEGACY_TESTMETHOD_DOMAIN, Domain,(PLONG)&keyLen))   
   {
      LogError(_T("Failed to read value for domain"));
      return ERROR_ACCESS_DENIED;
   }   

   keyLen = 128;
   if(dwEapTypeId == PPP_EAP_PEAP)
   {
      if(!testRegHelper.ReadStrValue(LEGACY_TESTMETHOD_PASSWD, PassWD,(PLONG)&keyLen))   
      {
         LogError(_T("Failed to read value for pass word"));
         return ERROR_ACCESS_DENIED;
      }   

   }
   else
      PassWD[0] = _T('\0');

   // Need to save how many time NetUI get called from the methods
   DWORD netUICount = 0;

   if(!testRegHelper.ReadDwordValue(LEGACY_TESTMETHOD_UICOUNT, &netUICount))
        netUICount = 0;

   if(netUICount < 0)
        netUICount = 0;

   netUICount ++;
   if(!testRegHelper.SetDwordValue(LEGACY_TESTMETHOD_UICOUNT,netUICount))
   {
      LogError(_T("Failed to set value for NetuiCount"));
      return ERROR_ACCESS_DENIED;
   }        
      

   // Now will have to pack username/passwd/domain into one buffer in the same as generic UI data
   DWORD totalSize = 
      (_tcslen(UserName) + 1 + _tcslen(Domain) + 1 + _tcslen(PassWD) + 1) * sizeof(TCHAR) + 3 * sizeof(size_t);


   *ppUserDataOut = (PBYTE)LocalAlloc(LPTR, totalSize);
   BYTE* position = *ppUserDataOut;

   // Copy Username
   size_t length = _tcslen(UserName) + 1;
   length *= sizeof(TCHAR);
   memcpy(position, &length, sizeof(size_t));

   position += sizeof(size_t);
   memcpy(position, UserName, length);
   position += length;

   
   if(dwEapTypeId == PPP_EAP_PEAP)
   {
      // Copy Password
      length = _tcslen(PassWD) + 1;
   }
   else
   {
      length = 1;
   }
   

   length *= sizeof(TCHAR);
   memcpy(position, &length, sizeof(size_t));

   position += sizeof(size_t);
   memcpy(position, PassWD, length);      
   position += length;   


   // Copy Domain
   length = _tcslen(Domain) + 1;
   length *= sizeof(TCHAR);
   memcpy(position, &length, sizeof(size_t));

   position += sizeof(size_t);
   memcpy(position, Domain, length);
   
   *pcbSizeOfUserDataOut = totalSize;

   DWORD identityLen = _tcslen(UserName)  
                + 2 //for \ and Null char 
                + _tcslen(Domain);   

   identityLen *= sizeof(TCHAR);
   WCHAR* szIdentityLen = (WCHAR*)LocalAlloc(LPTR, identityLen);
   *ppwszIdentity =  szIdentityLen;

    const WCHAR slash[] = TEXT("\\");
    
    wcscpy_s(
       szIdentityLen, 
       identityLen, 
       Domain
       );
    wcscat_s(
       szIdentityLen, 
       identityLen, 
       slash
       );
    wcscat_s(
       szIdentityLen, 
       identityLen, 
       UserName
       );

    return NO_ERROR;
}

extern "C"
DWORD APIENTRY
RasEapFreeMemory(
    IN  PBYTE pMemory)
//
//  This function is a Dll export.
//  The EAP engine will call this to free the pUserPwd allocated
//  in RasEapInvokeInteractiveUI.
//
{
	LocalFree(pMemory);
	return NO_ERROR;
}

typedef struct _PEAP_TEST_INTERACTIVE_UI
{
    DWORD           dwEapTypeId;    //Embedded Eap Type Id requesting 
                                    // interactive UI
    DWORD           dwSizeofUIContextData;
    BYTE            bUIContextData[1];
}PEAP_TEST_INTERACTIVE_UI, *PPEAP_TEST_INTERACTIVE_UI;

/*

Returns:
    Error codes only from winerror.h, raserror.h or mprerror.h

Notes:
    RasEapInvokeInteractiveUI entry point called by the EAP-PPP engine by name.

*/
extern "C"
DWORD
RasEapInvokeInteractiveUI(
    IN  DWORD           dwEapTypeId,
    IN  HWND            hwndParent,
    IN  BYTE*           pUIContextData,
    IN  DWORD           dwSizeofUIContextData,
    OUT BYTE**          ppDataFromInteractiveUI,
    OUT DWORD*          pdwSizeOfDataFromInteractiveUI
)
{
    if (PPP_EAP_PEAP == dwEapTypeId) // This has to be called in the PEAP context
    {
        PPEAP_TEST_INTERACTIVE_UI peapUI = (PPEAP_TEST_INTERACTIVE_UI)pUIContextData;
        if(peapUI)
        {
           EAPEXTUI_INPUT *pExtUIInput = (EAPEXTUI_INPUT *)peapUI->bUIContextData;
           *ppDataFromInteractiveUI = NULL;
           *pdwSizeOfDataFromInteractiveUI = 0;

           if (NULL != pExtUIInput
               && pExtUIInput->dwVersion == 1
               && pExtUIInput->dwSize >= sizeof(EAPEXTUI_INPUT)
               )
           {
               if(pExtUIInput->dwCode == EAPEXTUI_CODE_SERVERAUTH_ERROR)
               {
                    if (pExtUIInput->fFlags & EAPEXTUI_FLAG_RESPONSE)
                    {
                        // response expected
                        EAPEXTUI_OUTPUT *pExtUIOutput = (EAPEXTUI_OUTPUT *)LocalAlloc(LPTR, sizeof(EAPEXTUI_OUTPUT));
                        if (pExtUIOutput)
                        {
                            pExtUIOutput->dwVersion =1;
                            pExtUIOutput->dwSize = sizeof(EAPEXTUI_OUTPUT);
                            pExtUIOutput->dwValue = IDYES; // Set to YES always
                            *ppDataFromInteractiveUI = (PBYTE)pExtUIOutput;
                            *pdwSizeOfDataFromInteractiveUI = pExtUIOutput->dwSize;
                        }
                    }
               }

           }
        }
    }


    return  NO_ERROR;
}



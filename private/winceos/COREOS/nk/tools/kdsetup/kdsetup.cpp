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
/*
 */
#include <windows.h>
#include <dbgapi.h>
#include <kdioctl.h>

#include <guts.h>
#include <ehm.h>

#include "FexXmlParser.h"

LPCTSTR g_pszDllKey   = TEXT("SYSTEM\\Debugger");
LPCTSTR g_pszDllValue = TEXT("Library");


HRESULT EnsureDebuggerAttached(LPCTSTR pszDebuggerName)
{
   HRESULT hr = E_FAIL;

   if (AttachDebugger(pszDebuggerName))
   {
      hr = S_OK;
   }

   return hr;
}

HRESULT ClearExistingRuleset(HANDLE hKD)
{
   HRESULT hr = E_FAIL;
   DWORD cCurrentRules = 0;

   if (KernelLibIoControl(hKD, IOCTL_DBG_FEX_GET_MAX_RULES, NULL, 0, &cCurrentRules, sizeof(cCurrentRules), NULL))
   {
      hr = S_OK;
      
      for (DWORD i=0; i < cCurrentRules; i++)
      {
          if (!KernelLibIoControl(hKD, IOCTL_DBG_FEX_REMOVE_RULE, &i, sizeof(i), NULL, 0, NULL))
          {
             hr = E_FAIL;
             break;
          }
      }
   }
   
   return hr;
}

int _tmain(int argc, LPCTSTR* argv)
{
   HRESULT hr = S_OK;
   FexXmlParser *pParser = NULL;
   WCHAR debuggerName[MAX_PATH] = {0};
   KDFEX_RULESET_STATUS status = KDFEX_RULESET_STATUS_OFF;
   KDFEX_RULE_INFO ruleInfo = {0};
   DWORD dwRuleCount = 0;
   DWORD dwRuleIndex = 0;
   DWORD statusIoctlCode = IOCTL_DBG_FEX_OFF;

   if ((argc == 2) && (argv != NULL))
   {
      pParser = new FexXmlParser();
      if (pParser == NULL)
      {
         RETAILMSG(1, (L"KDSetup Aborting: Not enough memory in the system to parse the configuration file\r\n"));
         CHR(E_OUTOFMEMORY);
      }
   
      CHR(pParser->LoadFexRuleset(argv[1]));
      CHR(pParser->GetDebuggerName(debuggerName, _countof(debuggerName), NULL));

      RETAILMSG(1, (L"KDSetup: Config file loaded, attempting to apply configuration...\r\n"));

      hr = EnsureDebuggerAttached(debuggerName);
      if (SUCCEEDED(hr))
      {
         CHR(pParser->GetExceptionFilteringStatus(&status));

         if (status == KDFEX_RULESET_STATUS_ON)
         {
            statusIoctlCode = IOCTL_DBG_FEX_ON;
         }
   
         dwRuleCount = pParser->GetRuleCount();
      
         HANDLE hKD = LoadKernelLibrary(debuggerName);
         if (hKD == NULL)
         {
            RETAILMSG(1, (L"KDSetup Aborting: %s is registered as a debugger but is not loaded in the kernel\r\n", debuggerName));
            CHR(E_UNEXPECTED);
         }
         
         if (!KernelLibIoControl(hKD, statusIoctlCode, NULL, 0, NULL, 0, NULL))
         {
            RETAILMSG(1, (L"KDSetup Aborting: Unable to set Exception filtering status, IOCTL call failed\r\n"));
            CHR(E_FAIL);
         }

         if (pParser->ShouldClearExistingRuleset())
         {
            hr = ClearExistingRuleset(hKD);
            if (FAILED(hr))
            {
               RETAILMSG(1, (L"KDSetup Aborting: Unable to clear the existing Exception filtering ruleset, IOCTL call failed\r\n"));
               CHR(hr);
            }
         }
      
         for (DWORD i=0; i < dwRuleCount; i++)
         {
             CHR(pParser->GetRuleInfo(i, &ruleInfo));
             if (KernelLibIoControl(hKD, IOCTL_DBG_FEX_ADD_RULE, &ruleInfo, sizeof(ruleInfo), &dwRuleIndex, sizeof(dwRuleIndex), NULL))
             {
               RETAILMSG(1, (L"KDSetup: Exception filtering rule #%d Added\r\n", i+1));
             }
             else
             {
                RETAILMSG(1, (L"KDSetup Aborting: Unable to add Exception filtering rule #%d, IOCTL call failed\r\n", i+1));
                CHR(E_FAIL); 
             }
         }

         RETAILMSG(1, (L"KDSetup: Configuration applied successfully! \r\n"));
      }
      else
      {
         RETAILMSG(1, (L"KDSetup Aborting: unable to attach %s as a kernel debugger\r\n", debuggerName));
      }
   }
   else
   {
      RETAILMSG(1, (L"Usage:\r\n"));
      RETAILMSG(1, (L"\t kdsetup.exe config.xml\r\n\r\n"));
      hr = E_FAIL;
   }

Error:

   if (pParser != NULL)
   {
      delete pParser;
   }

   return (SUCCEEDED(hr) ? 0 : (int)hr);
}


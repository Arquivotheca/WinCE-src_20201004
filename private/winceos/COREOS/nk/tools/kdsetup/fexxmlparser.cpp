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
#include <guts.h>
#include <ehm.h>
#include <strsafe.h>
#include "filestream.h"

#include "FexXmlParser.h"

/*
<?xml version="1.0" encoding="utf-8"?>
<debugger name="kd.dll">
  <configuration>
      <exception_filtering status="ON" clear="YES">
          <rules>
             <rule decision="break">
                <code>0xC0000005</code>
                <handling>unhandled</handling>
                <process>cprog.exe</process>
                <module>core.dll</module>
                <first_address>0x00000000</first_address>
                <last_address>0x80000000</last_address>
             </rule>
          </rules>
      </exception_filtering>
  </configuration>
</debugger>
*/

const WCHAR* c_wszDebugger      = L"debugger";
const WCHAR* c_wszConfiguration = L"configuration";
const WCHAR* c_wszExceptionFiltering = L"exception_filtering";
const WCHAR* c_wszStatus       = L"status";
const WCHAR* c_wszClear        = L"clear";
const WCHAR* c_wszName         = L"name";
const WCHAR* c_wszRules        = L"rules";
const WCHAR* c_wszRule         = L"rule";
const WCHAR* c_wszDecision     = L"decision";
const WCHAR* c_wszCode         = L"code";
const WCHAR* c_wszHandling     = L"handling";
const WCHAR* c_wszProcess      = L"process";
const WCHAR* c_wszModule       = L"module";
const WCHAR* c_wszFirstAddress = L"first_address";
const WCHAR* c_wszLastAddress  = L"last_address";

HRESULT GetXmlAttributeValue(IXmlReader *pXmlReader, const WCHAR* pwszAttributeName, LPCWSTR *ppwszValue)
{
   HRESULT hr = S_OK;
   UINT cAttributeCount = 0;
   LPCWSTR pwszLocalName = NULL;

   ASSERT(pXmlReader);
   ASSERT(pwszAttributeName != NULL);
   ASSERT(ppwszValue != NULL);
      
   CHR(pXmlReader->GetAttributeCount(&cAttributeCount));
   CBR(cAttributeCount > 0);
   
   CHR(pXmlReader->MoveToFirstAttribute());
   
   while (hr == S_OK)
   {
       CHR(pXmlReader->GetLocalName(&pwszLocalName, NULL));
   
       if (wcscmp(pwszAttributeName, pwszLocalName) == 0)
       {
          CHR(pXmlReader->GetValue(ppwszValue, NULL));
          hr = S_OK;
          break;
       }
       CHR(pXmlReader->MoveToNextAttribute());
   }

   CBR(*ppwszValue != NULL);

Error:
   return hr;
}

HRESULT GetCurrentXmlElementText(IXmlReader *pXmlReader, LPCWSTR *ppwszValue)
{
   HRESULT hr = S_OK;
   XmlNodeType nodeType;

   ASSERT(pXmlReader);
   ASSERT(ppwszValue != NULL);
   
   CHR(pXmlReader->Read(&nodeType));

   while (hr == S_OK)
   {
      if (nodeType == XmlNodeType_Text)
      {
         CHR(pXmlReader->GetValue(ppwszValue, NULL));
         break;
      }
      else if ((nodeType == XmlNodeType_Element) || (nodeType == XmlNodeType_EndElement))
      {
         CHR(E_UNEXPECTED);
      }
      
      CHR(pXmlReader->Read(&nodeType));
   }

   CBR(*ppwszValue != NULL);
   
Error:
   return hr;
}

BOOL IsRelevantValue(LPCWSTR pwszValue)
{
   ASSERT(pwszValue);
   return (wcscmp(pwszValue, L"ANY") != 0);
}


FexXmlParser::FexXmlParser()
{
   m_vecRules.clear();
   ClearFexRuleset();
}

FexXmlParser::~FexXmlParser()
{
   ClearFexRuleset();
}

VOID FexXmlParser::ClearFexRuleset()
{
   for (int i=0; i < m_vecRules.size(); i++)
   {
      if (m_vecRules[i] != NULL)
      {
         LocalFree(m_vecRules[i]);
         m_vecRules[i] = NULL;
      }
   }
   m_vecRules.clear();
   m_status = KDFEX_RULESET_STATUS_OFF;
   m_isInitialized = FALSE;
   m_fClearExistingRuleset = FALSE;
}

DWORD FexXmlParser::GetRuleCount()
{
   return m_vecRules.size();
}

BOOL FexXmlParser::ShouldClearExistingRuleset()
{
   return m_fClearExistingRuleset;
}

HRESULT FexXmlParser::GetRuleInfo(DWORD ruleIndex, __out KDFEX_RULE_INFO * pRuleInfo)
{
   HRESULT hr = S_OK;

   CBREx(ruleIndex < m_vecRules.size(), E_INVALIDARG);
   CPREx(pRuleInfo, E_INVALIDARG);
   CPREx(m_vecRules[ruleIndex], E_FAIL);

   *pRuleInfo = *(m_vecRules[ruleIndex]);

Error:
   return hr;
}

HRESULT FexXmlParser::GetDebuggerName(WCHAR *pwszDebuggerName, DWORD cchMaxSize, DWORD *pcchRequiredSize)
{
   HRESULT hr = S_OK;

   CPREx(pwszDebuggerName, E_INVALIDARG);
   CBREx(m_isInitialized, E_UNEXPECTED);

   hr = StringCchCopyW(pwszDebuggerName, cchMaxSize, m_wszDebuggerDLLName);
   if ((hr == STRSAFE_E_INVALID_PARAMETER) || (hr == STRSAFE_E_INSUFFICIENT_BUFFER))
   {
      if (pcchRequiredSize)
      {
         *pcchRequiredSize = wcsnlen(m_wszDebuggerDLLName, _countof(m_wszDebuggerDLLName));
      }
      CHR(STRSAFE_E_INSUFFICIENT_BUFFER);
   }
   
Error:

   return hr;
}

HRESULT FexXmlParser::GetExceptionFilteringStatus(KDFEX_RULESET_STATUS *pRulesetStatus)
{
   HRESULT hr = S_OK;

   CPREx(pRulesetStatus, E_INVALIDARG);
   CBREx(m_isInitialized, E_UNEXPECTED);

   *pRulesetStatus = m_status;

Error:
   return hr;
}


HRESULT FexXmlParser::LoadFexRuleset(LPCTSTR pszFilename)
{
   HRESULT hr = S_OK;
   IXmlReader *pXmlReader = NULL;
   IStream *pStream = NULL;

   pStream = CreateFileStream(pszFilename, FALSE);
   CPR(pStream);
   
   CHR(CreateXmlReader(__uuidof(IXmlReader), (void**) &pXmlReader, NULL));
   
   CHR(pXmlReader->SetInput(pStream)); 
   
   CHR(PopulateRuleset(pXmlReader));
    
Error:

   if (pXmlReader != NULL)
   {
      pXmlReader->Release();
   }

   if (pStream != NULL)
   {
      pStream->Release();
   }

   if (FAILED(hr))
   {
     RETAILMSG(1, (L"KDSetup Aborting: Unable to load the KD setup from the provided XML file\r\n"));
   }

   return hr;
}

HRESULT FexXmlParser::ProcessXmlEndElement(IXmlReader *pXmlReader)
{
   HRESULT hr = S_OK;
   LPCWSTR pwszElementName = NULL;

   CHR(pXmlReader->GetLocalName(&pwszElementName, NULL));

   switch(m_currentParsingNode)
   {
      case FEX_XMLCONFIG_RELEVANT_NODE_DEBUGGER:
        if (wcscmp(pwszElementName, c_wszDebugger) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_ROOT;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_CONFIG:
        if (wcscmp(pwszElementName, c_wszConfiguration) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_DEBUGGER;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_FEX:
        if (wcscmp(pwszElementName, c_wszExceptionFiltering) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_CONFIG;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_RULES:
        if (wcscmp(pwszElementName, c_wszRules) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_FEX;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_RULE:
        if (wcscmp(pwszElementName, c_wszRule) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_RULES;
        }
        break;

      default:
        break;
   }

Error:
   return hr;
}

HRESULT FexXmlParser::ProcessRuleXmlElement(IXmlReader *pXmlReader)
{
   HRESULT hr = S_OK;
   XmlNodeType nodeType;
   LPCWSTR pwszElementName = NULL;
   LPCWSTR pwszElementValue = NULL;
   LPCWSTR pwszAttributeValue = NULL;
   WCHAR *pwcStop = NULL;
   KDFEX_RULE_INFO ruleInfo = {0};
   ruleInfo.dwSize = sizeof(KDFEX_RULE_INFO);
   ruleInfo.fgRelevanceFlags = KDFEX_RULE_RELEVANCE_GENERAL;

   ASSERT(pXmlReader);
   ASSERT(FEX_XMLCONFIG_RELEVANT_NODE_RULE == m_currentParsingNode);

   CHR(GetXmlAttributeValue(pXmlReader, c_wszDecision, &pwszAttributeValue));

   if (wcscmp(pwszAttributeValue, L"break") == 0)
   {
      ruleInfo.decision = KDFEX_RULE_DECISION_BREAK;
   }
   else if (wcscmp(pwszAttributeValue, L"ignore") == 0)
   {
      ruleInfo.decision = KDFEX_RULE_DECISION_IGNORE;
   }
   else if (wcscmp(pwszAttributeValue, L"absorb") == 0)
   {
      ruleInfo.decision = KDFEX_RULE_DECISION_ABSORB;
   }
   else
   {
      RETAILMSG(1, (L"KDSetup Aborting: Unexpected value for attribute \"%s\" of element <%s>\r\n", c_wszDecision, c_wszRule));
      CHR(E_FAIL);
   }

   CHR(pXmlReader->Read(&nodeType));

   while (hr == S_OK)
   {
      if (nodeType == XmlNodeType_Element)
      {  
         CHR(pXmlReader->GetLocalName(&pwszElementName, NULL));
         
         if (wcscmp(pwszElementName, c_wszCode) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_CODE;
               ruleInfo.dwExceptionCode = wcstoul(pwszElementValue, &pwcStop, 16);
            }
         }
         else if (wcscmp(pwszElementName, c_wszHandling) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_HANDLING;
               if (wcscmp(pwszElementValue, L"handled") == 0)
               {
                  ruleInfo.handlingLevel = KDFEX_HANDLING_HANDLED;
               }
               else if (wcscmp(pwszElementValue, L"unhandled") == 0)
               {
                  ruleInfo.handlingLevel = KDFEX_HANDLING_UNHANDLED;
               }
               else if (wcscmp(pwszElementValue, L"user_safetynet") == 0)
               {
                  ruleInfo.handlingLevel = KDFEX_HANDLING_USER_SAFETYNET;
               }
               else if (wcscmp(pwszElementValue, L"kernel_safetynet") == 0)
               {
                  ruleInfo.handlingLevel = KDFEX_HANDLING_KERNEL_SAFETYNET;
               }
               else
               {
                  RETAILMSG(1, (L"KDSetup Aborting: Unexpected value for element <%s> of a rule\r\n", c_wszHandling));
                  CHR(E_FAIL);
               }
            }
         }
         else if (wcscmp(pwszElementName, c_wszProcess) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_PROCESS;
               CHR(StringCchPrintfA(ruleInfo.szProcessName, _countof(ruleInfo.szProcessName), "%S", pwszElementValue));
            }
         }
         else if (wcscmp(pwszElementName, c_wszModule) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_MODULE;
               CHR(StringCchPrintfA(ruleInfo.szModuleName, _countof(ruleInfo.szModuleName), "%S", pwszElementValue));
            }
         }
         else if (wcscmp(pwszElementName, c_wszFirstAddress) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_ADDRESS;
               ruleInfo.dwFirstAddress = wcstoul(pwszElementValue, &pwcStop, 16);
            }
         }
         else if (wcscmp(pwszElementName, c_wszLastAddress) == 0)
         {
            CHR(GetCurrentXmlElementText(pXmlReader, &pwszElementValue));
            if (IsRelevantValue(pwszElementValue))
            {
               ruleInfo.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_ADDRESS;
               ruleInfo.dwLastAddress = wcstoul(pwszElementValue, &pwcStop, 16);
            }
         }
      }
      else if (nodeType == XmlNodeType_EndElement)
      {
         CHR(ProcessXmlEndElement(pXmlReader));
         if (m_currentParsingNode != FEX_XMLCONFIG_RELEVANT_NODE_RULE)
         {
            break;
         }
      }

      CHR(pXmlReader->Read(&nodeType));
   }

   hr = S_OK;

   if ((ruleInfo.fgRelevanceFlags & KDFEX_RULE_RELEVANCE_ADDRESS) != 0)
   {
      if (ruleInfo.dwFirstAddress > ruleInfo.dwLastAddress)
      {
         RETAILMSG(1, (L"KDSetup Aborting: Last Address of a rule should always be greather than or equal to the First Address\r\n"));
         CHR(E_FAIL);
      }
   }
   
   KDFEX_RULE_INFO *pRuleToSave = (KDFEX_RULE_INFO *)LocalAlloc(LMEM_FIXED, sizeof(KDFEX_RULE_INFO));
   CPR(pRuleToSave);

   *pRuleToSave = ruleInfo;
   
   if (m_vecRules.push_back(pRuleToSave))
   {
      hr = S_OK;
   }
   else
   {
      LocalFree(pRuleToSave);
      hr = E_OUTOFMEMORY;
   }
   
Error:
   return hr;
}

HRESULT FexXmlParser::ProcessXmlElement(IXmlReader *pXmlReader)
{
   HRESULT hr = S_OK;
   LPCWSTR pwszElementName = NULL;
   LPCWSTR pwszAttributeValue = NULL;
   
   ASSERT(pXmlReader);

   CHR(pXmlReader->GetLocalName(&pwszElementName, NULL));

   switch(m_currentParsingNode)
   {
      case FEX_XMLCONFIG_RELEVANT_NODE_ROOT:
        if (wcscmp(pwszElementName, c_wszDebugger) == 0)
        {
           hr = GetXmlAttributeValue(pXmlReader, c_wszName, &pwszAttributeValue);
           if (FAILED(hr))
           {
              RETAILMSG(1, (L"KDSetup Aborting: Expecting a value for attribute \"%s\" of element <%s>\r\n", c_wszName, c_wszDebugger));
              CHR(hr);
           }
           hr =StringCchCopy(m_wszDebuggerDLLName, _countof(m_wszDebuggerDLLName), pwszAttributeValue);
           if (FAILED(hr))
           {
              RETAILMSG(1, (L"KDSetup Aborting: Debugger name is bigger than expected\r\n", pwszAttributeValue));
              CHR(hr);
           }
      
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_DEBUGGER;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_DEBUGGER:
        if (wcscmp(pwszElementName, c_wszConfiguration) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_CONFIG;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_CONFIG:
        if (wcscmp(pwszElementName, c_wszExceptionFiltering) == 0)
        {          
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_FEX;

           hr  = GetXmlAttributeValue(pXmlReader, c_wszStatus, &pwszAttributeValue);

           if (FAILED(hr))
           {
              RETAILMSG(1, (L"KDSetup Aborting: Expecting a value for attribute \"%s\" of element <%s>\r\n", c_wszStatus, c_wszExceptionFiltering));
              CHR(hr);
           }

           if (wcscmp(pwszAttributeValue, L"ON") == 0)
           {
              m_status = KDFEX_RULESET_STATUS_ON;
           }
           else if (wcscmp(pwszAttributeValue, L"OFF") == 0)
           {
              m_status = KDFEX_RULESET_STATUS_OFF;
           }
           else
           {
              RETAILMSG(1, (L"KDSetup Aborting: Unexpected value for attribute \"%s\" of element <%s>\r\n", c_wszStatus, c_wszExceptionFiltering));
              CHR(E_FAIL);
           }   
          
           if (SUCCEEDED(GetXmlAttributeValue(pXmlReader, c_wszClear, &pwszAttributeValue)))
           {
              if (wcscmp(pwszAttributeValue, L"YES") == 0)
              {
                 m_fClearExistingRuleset = TRUE;
              }
              else if (wcscmp(pwszAttributeValue, L"NO") == 0)
              {
                 m_fClearExistingRuleset = FALSE;
              }
              else
              {
                 RETAILMSG(1, (L"KDSetup Aborting: Unexpected value for attribute \"%s\" of element <%s>\r\n", c_wszClear, c_wszExceptionFiltering));
                 CHR(E_FAIL);
              }                
           }
     
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_FEX:
        if (wcscmp(pwszElementName, c_wszRules) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_RULES;
        }
        break;

      case FEX_XMLCONFIG_RELEVANT_NODE_RULES:
        if (wcscmp(pwszElementName, c_wszRule) == 0)
        {
           m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_RULE;
           CHR(ProcessRuleXmlElement(pXmlReader));
        }
        break;

      default:
        break;
   }
  

Error:
   return hr;
}

HRESULT FexXmlParser::PopulateRuleset(IXmlReader *pXmlReader)
{
   HRESULT hr = S_OK;
   XmlNodeType nodeType;

   ClearFexRuleset();

   m_currentParsingNode = FEX_XMLCONFIG_RELEVANT_NODE_ROOT;

   CPRAEx(pXmlReader, E_INVALIDARG);
   
   CHR(pXmlReader->Read(&nodeType));

   while (hr == S_OK)
   {
      if (nodeType == XmlNodeType_Element)
      {
         CHR(ProcessXmlElement(pXmlReader));
      }
      else if (nodeType == XmlNodeType_EndElement)
      {
         CHR(ProcessXmlEndElement(pXmlReader));
      }
      
      CHR(pXmlReader->Read(&nodeType));
   }

   //if we got here then there were no parsing errors and the rules in the XML file were well formatted.
   m_isInitialized = TRUE;
   hr = S_OK;
   
Error:

   if (FAILED(hr))
   {
      RETAILMSG(1, (L"KDSetup Aborting: the provided XML file contains error\r\n"));
   }

   return hr;
}


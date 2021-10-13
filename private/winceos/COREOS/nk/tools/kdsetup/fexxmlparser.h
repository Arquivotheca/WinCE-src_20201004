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

#pragma once

#include <vector.hxx>
#include <xmllite.h>
#include <kdioctl.h>

enum FEX_XMLCONFIG_RELEVANT_NODE
{
   FEX_XMLCONFIG_RELEVANT_NODE_ROOT = 0,
   FEX_XMLCONFIG_RELEVANT_NODE_DEBUGGER,
   FEX_XMLCONFIG_RELEVANT_NODE_CONFIG,
   FEX_XMLCONFIG_RELEVANT_NODE_FEX,
   FEX_XMLCONFIG_RELEVANT_NODE_RULES,
   FEX_XMLCONFIG_RELEVANT_NODE_RULE
};


class FexXmlParser
{
   private:
      BOOL                          m_fClearExistingRuleset;
      BOOL                          m_isInitialized;
      ce::vector<KDFEX_RULE_INFO *> m_vecRules;
      KDFEX_RULESET_STATUS          m_status;
      WCHAR                         m_wszDebuggerDLLName[MAX_PATH];
      FEX_XMLCONFIG_RELEVANT_NODE   m_currentParsingNode;

   private:
      HRESULT PopulateRuleset(IXmlReader *pXmlReader);
      HRESULT ProcessXmlElement(IXmlReader *pXmlReader);
      HRESULT ProcessXmlEndElement(IXmlReader *pXmlReader);
      HRESULT ProcessRuleXmlElement(IXmlReader *pXmlReader);

   public:
      FexXmlParser();
     ~FexXmlParser();

      HRESULT LoadFexRuleset(LPCTSTR pszFilename);
      VOID    ClearFexRuleset();
      
      DWORD   GetRuleCount();
      HRESULT GetRuleInfo(DWORD ruleIndex, __out KDFEX_RULE_INFO *pRuleInfo);
      HRESULT GetDebuggerName(WCHAR *pwszDebuggerName, DWORD cchMaxSize, DWORD *pcchRequiredSize);
      HRESULT GetExceptionFilteringStatus(KDFEX_RULESET_STATUS *pRulesetStatus);
      BOOL    ShouldClearExistingRuleset();

};



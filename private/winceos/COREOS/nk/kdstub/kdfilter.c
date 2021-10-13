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


#include "kdp.h"
#include "kdfilter.h"


//Rule Field scores
#define RULE_FIELD_SCORE_DIRECT       3  //Applied when a field matches the current exception entirely
#define RULE_FIELD_SCORE_INCLUSION    2  //Applied when a field matches the current exception by inclusion
#define RULE_FIELD_SCORE_GENERIC      1  //Applied when a field is irrelevant
#define RULE_FIELD_SCORE_NEGATIVE (-100) //Applied when a field is relevant and doesn't match the current exception


BOOL g_fFilterExceptions = FALSE;                      //Global flag that indicates if we are filtering exceptions or not
KDFEX_RULE_INFO g_kdFexRules[KDFEX_MAXRULES] = {0};    //Global ruleset of exception filtering rules

//Friendly names for the well known Handling Levels
const WCHAR* g_cwszHandlingTypeNames[] =
      {
        L"Custom",
        L"Handled",
        L"USafety",
        L"KSafety",
        L"Unhandled"
      };

//Friendly names for the well known exception filtering rule Decisions
const WCHAR* g_cwszDecisionNames[] =
      {
        L"Custom",
        L"Ignore",
        L"Break",
        L"Absorb"
      };

//signature documented in kdfilter.h
const WCHAR* GetHandlingLevelTypeName(KDFEX_HANDLING ruleHandlingLevel)
{
   DWORD index = (DWORD)ruleHandlingLevel;
   if (index >= _countof(g_cwszHandlingTypeNames))
   {
      //We dont have a friendly name for this "handling level".. the user must be using a custom one
      index = 0;
   }
   return g_cwszHandlingTypeNames[index];
}

//signature documented in kdfilter.h
const WCHAR* GetFilterRuleDecisionName(KDFEX_RULE_DECISION decision)
{
   BYTE index = (BYTE)decision;
   if (index >= _countof(g_cwszDecisionNames))
   {
      //We dont have a friendly name for this "decision".. the user must be using a custom one
      index = 0;
   }
   return g_cwszDecisionNames[index];
}

//signature documented in kdfilter.h
BOOL IsRelevantField(DWORD fgRelevanceFlags, KDFEX_RULE_RELEVANCE_FLAGS field)
{
   return ((fgRelevanceFlags & field) != 0);
}

///<summary>
/// Get a score based on how well the field in the provided rule matches the provided value
///</summary>
///<param name="ruleIndex">Index of the rule to be evaluated (index of g_kdFexRules</param>
///<param name="field" ref="KDFEX_RULE_RELEVANCE_FLAGS"> flag indicated which field is going be evaluated</param>
///<param name="pFieldValue">pointer to the value to be compared against the field</param>
///<returns>A score based on how well the field in the provided rule matches the provided value</returns>
int GetRuleWeightForField(DWORD ruleIndex, KDFEX_RULE_RELEVANCE_FLAGS field, VOID *pFieldValue)
{
   int iWeight = RULE_FIELD_SCORE_NEGATIVE;

   //1. check the provided field is relevant for the rule
   if (IsRelevantField(g_kdFexRules[ruleIndex].fgRelevanceFlags, field))
   {
      //2. Evaluate the provided value againt the relevant field in the rule
      //3. Give a high score if the values and the field match
      //4. Give a negative score if the value and the field dont match
      switch (field)
      {
         case KDFEX_RULE_RELEVANCE_CODE:
           if (g_kdFexRules[ruleIndex].dwExceptionCode == *((DWORD *)pFieldValue))
           {
              iWeight = RULE_FIELD_SCORE_DIRECT;
           }
           break;

         case KDFEX_RULE_RELEVANCE_PROCESS:
           if (strcmp((CHAR *)pFieldValue, g_kdFexRules[ruleIndex].szProcessName) == 0)
           {
              iWeight = RULE_FIELD_SCORE_DIRECT;
           }
           break;

         case KDFEX_RULE_RELEVANCE_MODULE:
           if (strcmp((CHAR *)pFieldValue, g_kdFexRules[ruleIndex].szModuleName) == 0)
           {
              iWeight = RULE_FIELD_SCORE_DIRECT;
           }
           break;

         case KDFEX_RULE_RELEVANCE_HANDLING:
           if (g_kdFexRules[ruleIndex].handlingLevel == *((KDFEX_HANDLING *)pFieldValue))
           {
              iWeight = RULE_FIELD_SCORE_DIRECT;
           }
           break;

         case KDFEX_RULE_RELEVANCE_ADDRESS:
           if ((g_kdFexRules[ruleIndex].dwFirstAddress <= *((DWORD *)pFieldValue)) && (g_kdFexRules[ruleIndex].dwLastAddress >= *((DWORD *)pFieldValue)))
           {
              iWeight = RULE_FIELD_SCORE_DIRECT;
           }
           break;

         case KDFEX_RULE_RELEVANCE_ALL:
         default:
           DEBUGGERMSG (KDZONE_ALERT, (L"  KdFilter: [ERROR] Trying to get the score of an unknown field\r\n"));
           iWeight = RULE_FIELD_SCORE_NEGATIVE;
           break;
      }
   }
   else
   {
      //The provided field is not relevant for the rule, give it a generic low score
      iWeight = RULE_FIELD_SCORE_GENERIC;
   }

   return iWeight;
}

//signature documented in kdfilter.h
KDFEX_RULE_DECISION GetFilterRuleDecision(DWORD dwExceptionCode,DWORD dwExceptionAddress, KDFEX_HANDLING handlingLevel)
{
   int iter;
   int iWeight;
   int iBestIndex = -1;
   int iBestWeight = -1;
   KDFEX_RULE_DECISION decision = KDFEX_RULE_DECISION_BREAK;
   LPCHAR szCurrentProcName = NULL;
   LPCHAR szCurrentModuleName = NULL;
   DWORD dwOffsetInModule = 0;
   PPROCESS pCurrentProc = NULL;

   // 1. Check if exception filtering is turned ON
   if (g_fFilterExceptions)
   {
     //2. The default decision is to ignore the exception (in case we don't have a rule to apply)
     decision = KDFEX_RULE_DECISION_IGNORE;

     pCurrentProc = GetPCB()->pCurPrc;

     //3. Obtain the rule that best matches the current exception context
     //   To do this we evaluate each of the rules in the list and keep the one with the highest score
     for (iter = 0; iter < KDFEX_MAXRULES; iter++)
     {
        //3.1 check that the rule is active (at least is generally relevant)
        if (g_kdFexRules[iter].fgRelevanceFlags != KDFEX_RULE_RELEVANCE_NONE)
        {
           iWeight = 0;

           //3.2 Obtain the current process name (from the exception context) only if needed
           if ((szCurrentProcName == NULL) && IsRelevantField(g_kdFexRules[iter].fgRelevanceFlags, KDFEX_RULE_RELEVANCE_PROCESS))
           {
              szCurrentProcName = GetWin32ExeName (pCurrentProc);
              //3.2.1 Obtain the offset inside the process from the exception address
              if ((pCurrentProc->e32.e32_vbase <= dwExceptionAddress) && (dwExceptionAddress < (pCurrentProc->e32.e32_vbase + pCurrentProc->e32.e32_vsize)))
              {
                 dwExceptionAddress = (dwExceptionAddress - pCurrentProc->e32.e32_vbase);
              }
           }

           //3.3 Obtain the module name in which the exception happen only if needed
           if ((szCurrentModuleName == NULL) && IsRelevantField(g_kdFexRules[iter].fgRelevanceFlags, KDFEX_RULE_RELEVANCE_MODULE))
           {
              dwOffsetInModule = 0;
              szCurrentModuleName = GetWin32DLLNameFromAddress(pCurrentProc, dwExceptionAddress, &dwOffsetInModule);
              //3.3.1 Obtain the offset inside the module from the exception address
              if (dwOffsetInModule != 0)
              {
                 dwExceptionAddress = dwOffsetInModule;
              }
           }

           //3.4 Obtain the score of each of the fields in the rule and add them
           iWeight += GetRuleWeightForField(iter, KDFEX_RULE_RELEVANCE_CODE, &dwExceptionCode);
           iWeight += GetRuleWeightForField(iter, KDFEX_RULE_RELEVANCE_PROCESS, szCurrentProcName);
           iWeight += GetRuleWeightForField(iter, KDFEX_RULE_RELEVANCE_MODULE, szCurrentModuleName);
           iWeight += GetRuleWeightForField(iter, KDFEX_RULE_RELEVANCE_HANDLING, &handlingLevel);
           iWeight += GetRuleWeightForField(iter, KDFEX_RULE_RELEVANCE_ADDRESS, &dwExceptionAddress);
   
           //3.5 if the obtained aggreaged score of this rule is higher than the best we had before, replace it
           if (iWeight > iBestWeight)
           {
              iBestWeight = iWeight;
              iBestIndex = iter;
           }
        }
     }
     
     if (iBestIndex >= 0)
     {
        decision = g_kdFexRules[iBestIndex].decision;
     }
   }
   else
   {
     //If exception filtering is OFF then we respect the kernel debugger normal behavior. we tell it to break
     //on any exception.
     decision = KDFEX_RULE_DECISION_BREAK;
   }

   return decision;
}


///<summary>
/// Gets the index of the first free slot in the ruleset
///</summary>
///<returns>If succeeded, The positive 0 based index of the first empty slot in the ruleset</returns>
///<returns>-1 if there are no empty slots in the ruleset</returns>
int GetFreeFilterRuleInfoIndex()
{
   int iter;
   int index = -1;

   //1. check every rule in the ruleset
   for (iter = 0; iter < KDFEX_MAXRULES; iter++)
   {
       //2. If a rule has no relevant fields then we can use that slot.
       if (g_kdFexRules[iter].fgRelevanceFlags == KDFEX_RULE_RELEVANCE_NONE)
       {
          index = iter;
          break;
       }
   }

   return index;
}

///<summary>
/// Gets how many relevant rules exist in the ruleset
///</summary>
///<returns>Amount of relevant rules in the ruleset</returns>
DWORD GetRelevantFilterRuleCount()
{
   DWORD iter = 0;
   DWORD cRelevant = 0;

   //1. check every rule in the ruleset
   for (iter = 0; iter < KDFEX_MAXRULES; iter++)
   {
       //2. If a rule has at least one relevant field or is generically relevant then we count it as a relevant rule.
       if (g_kdFexRules[iter].fgRelevanceFlags != KDFEX_RULE_RELEVANCE_NONE)
       {
          cRelevant++;
       }
   }

   return cRelevant;
}

///<summary>
/// Copies a KDFEX_RULE_INFO from source to destination
///</summary>
///<param name="pDestination" ref="KDFEX_RULE_INFO">pointer to the destination KDFEX_RULE_INFO structure</param>
///<param name="pSource" ref="KDFEX_RULE_INFO">pointer to the source KDFEX_RULE_INFO structure</param>
///<returns>TRUE if the copy operation succeeded</returns>
///<returns>FALSE if the copy operation failed</returns>
///<remarks>
/// 1. Both pDestination and pSource cannot be NULL. if any of them is NULL the function will not perform any operation
/// 2. The function only copies information from relevant fields
///</remarks>
BOOL CopyFilterRuleInfo(KDFEX_RULE_INFO *pDestination, KDFEX_RULE_INFO *pSource)
{
   BOOL fCopySuccess = FALSE;
   
   if (pDestination && pSource)
   {
      memset(pDestination, 0, sizeof(KDFEX_RULE_INFO));
      pDestination->dwSize = sizeof(KDFEX_RULE_INFO);
      pDestination->dwSize = pSource->dwSize;
      pDestination->fgRelevanceFlags = pSource->fgRelevanceFlags;
      pDestination->dwExceptionCode  = pSource->dwExceptionCode;
      pDestination->handlingLevel    = pSource->handlingLevel;
      pDestination->decision         = pSource->decision;
      pDestination->dwFirstAddress   = pSource->dwFirstAddress;
      pDestination->dwLastAddress    = pSource->dwLastAddress;
 
      if (IsRelevantField(pSource->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_PROCESS))
      {
         strncpy_s(pDestination->szProcessName, _countof(pDestination->szProcessName), pSource->szProcessName, _TRUNCATE);
      }
      if (IsRelevantField(pSource->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_MODULE))
      {
         strncpy_s(pDestination->szModuleName, _countof(pDestination->szModuleName), pSource->szModuleName, _TRUNCATE);
      }
      fCopySuccess = TRUE;
   }
   return fCopySuccess;
}

///<summary>
/// Verifies that the provided pointer to a KDFEX_RULE_INFO structure is not NULL and it contains a valid KDFEX_RULE_INFO value
///</summary>
///<param name="pFexRuleInfo" ref="KDFEX_RULE_INFO">pointer to the KDFEX_RULE_INFO structure to be evaluated</param>
///<returns>FALSE if the pointer is NULL or any of the fields contains invalid values</returns>
///<returns>TRUE if the pointer is not NULL and the fields contains valid values</returns>
BOOL IsValidFilterRule(IN KDFEX_RULE_INFO *pFexRuleInfo)
{
   BOOL isValid = FALSE;
   DWORD nameLen = 0;

   if ((pFexRuleInfo != NULL) && (pFexRuleInfo->dwSize == sizeof(KDFEX_RULE_INFO)))
   {
      isValid = TRUE;
      
      if (IsRelevantField(pFexRuleInfo->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_PROCESS))
      {
         nameLen = strnlen(pFexRuleInfo->szProcessName, _countof(pFexRuleInfo->szProcessName));
         isValid = ((nameLen > 0) && (nameLen < _countof(pFexRuleInfo->szProcessName)));
      }
      if (isValid && IsRelevantField(pFexRuleInfo->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_MODULE))
      {
         nameLen = strnlen(pFexRuleInfo->szModuleName, _countof(pFexRuleInfo->szModuleName));
         isValid = ((nameLen > 0) && (nameLen < _countof(pFexRuleInfo->szModuleName)));
      }
      if (isValid && IsRelevantField(pFexRuleInfo->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_ADDRESS))
      {
         isValid = (pFexRuleInfo->dwFirstAddress <= pFexRuleInfo->dwLastAddress);
      }
   }

   return isValid;
}

//signature documented in kdfilter.h
int  AddFilterRule(IN KDFEX_RULE_INFO *pFexRuleInfo)
{
    int iIndex = -1;
    KDFEX_RULE_INFO marshalledRule;

    //1. make a local copy of the rule so no one can tamper with it while we validate the fields
    if (CopyFilterRuleInfo(&marshalledRule, pFexRuleInfo))
    {
       //2. Check that the provided rule is valid
       if (IsValidFilterRule(&marshalledRule))
       {
          //3. Get an empty slot from the ruleset
          iIndex = GetFreeFilterRuleInfoIndex();
          if (iIndex >= 0)
          {
             //4. Copy the provided rule information into the ruleset's empty slot
             marshalledRule.fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_GENERAL;
             CopyFilterRuleInfo(&g_kdFexRules[iIndex], &marshalledRule);
          }
          else
          {
             //If the ruleset is full we return -2
             iIndex = -2;
          }
       }
    }
    return iIndex;
}

//signature documented in kdfilter.h
int  AddSimpleFilterRule(DWORD dwExceptionCode, KDFEX_RULE_DECISION decision)
{
    int iIndex;

    //1. Get an empty slot from the rulset
    iIndex = GetFreeFilterRuleInfoIndex();
    if (iIndex >= 0)
    {
       //2. create a rule for the exception code provided with the given decision
       //3. copy the rule into the ruleset empty slot
       memset(&g_kdFexRules[iIndex], 0, sizeof(KDFEX_RULE_INFO));
       g_kdFexRules[iIndex].dwSize = sizeof(KDFEX_RULE_INFO);
       g_kdFexRules[iIndex].fgRelevanceFlags = (KDFEX_RULE_RELEVANCE_GENERAL | KDFEX_RULE_RELEVANCE_CODE);
       g_kdFexRules[iIndex].dwExceptionCode = dwExceptionCode;
       g_kdFexRules[iIndex].decision = decision;
    }
    else
    {
       //If the ruleset is full we return -2
       iIndex = -2;
    }
    return iIndex;
}

//signature documented in kdfilter.h
void RemoveFilterRule(DWORD ruleIndex)
{
   //1. check that the provided index is valid
   if (ruleIndex < KDFEX_MAXRULES)
   {
      //2. clear the ruleset slot in such index
      memset(&(g_kdFexRules[ruleIndex]), 0, sizeof(KDFEX_RULE_INFO));
      g_kdFexRules[ruleIndex].dwSize = sizeof(KDFEX_RULE_INFO);
      g_kdFexRules[ruleIndex].fgRelevanceFlags = KDFEX_RULE_RELEVANCE_NONE;
   }
}

//signature documented in kdfilter.h
KDFEX_RULE_INFO* GetFilterRulePtr(DWORD ruleIndex)
{
   KDFEX_RULE_INFO* pRule = NULL;

   //1. check that the provided index is valid
   if (ruleIndex < KDFEX_MAXRULES)
   {
      //2. get the pointer to the KDFEX_RULE_INFO structure in the rulset and return it
      pRule = &g_kdFexRules[ruleIndex];
   }
   return pRule;
}


//signature documented in kdfilter.h
BOOL 
KDFex_IOControl(
    DWORD   dwInstData,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned)
{
   BOOL fRequestSucceeded = FALSE;
   KDFEX_RULE_INFO *pRuleInfoProvided = NULL;
   KDFEX_RULE_INFO *pLocalRuleInfo = NULL;
   int ruleIndex = 0;
   
   switch (dwIoControlCode)
   {
       case IOCTL_DBG_FEX_OFF:
         g_fFilterExceptions = FALSE;
         fRequestSucceeded = TRUE;
         break;
         
       case IOCTL_DBG_FEX_ON:
         g_fFilterExceptions = TRUE;
         fRequestSucceeded = TRUE;
         break;

       case IOCTL_DBG_FEX_GET_STATUS:
         if ((lpOutBuf != NULL) && (nOutBufSize == sizeof(KDFEX_RULESET_STATUS)))
         {
            *((KDFEX_RULESET_STATUS *)lpOutBuf) = (g_fFilterExceptions ? KDFEX_RULESET_STATUS_ON : KDFEX_RULESET_STATUS_OFF);
            fRequestSucceeded = TRUE;
         }
         break;

       case IOCTL_DBG_FEX_GET_MAX_RULES:
         if ((lpOutBuf != NULL) && (nOutBufSize == sizeof(DWORD)))
         {
            *((DWORD *)lpOutBuf) = (DWORD)_countof(g_kdFexRules);
            fRequestSucceeded = TRUE;
         }
         break;

       case IOCTL_DBG_FEX_GET_RELEVANT_RULE_COUNT:
         if ((lpOutBuf != NULL) && (nOutBufSize == sizeof(DWORD)))
         {
            *((DWORD *)lpOutBuf) = GetRelevantFilterRuleCount();
            fRequestSucceeded = TRUE;
         }
         break;
         
       case IOCTL_DBG_FEX_ADD_RULE:
         if ((lpInBuf != NULL) && (nInBufSize == sizeof(KDFEX_RULE_INFO)))
         {
            pRuleInfoProvided = (KDFEX_RULE_INFO *)lpInBuf;
            ruleIndex = AddFilterRule(pRuleInfoProvided);
            if (ruleIndex >= 0)
            {
               if (lpOutBuf != NULL)
               {
                  if (nOutBufSize == sizeof(DWORD))
                  {
                     *((DWORD *)lpOutBuf) = (DWORD)ruleIndex;
                     fRequestSucceeded = TRUE;
                  }
                  else
                  {
                     RemoveFilterRule(ruleIndex);
                     fRequestSucceeded = FALSE;
                  }
               }
               else
               {
                  fRequestSucceeded = TRUE;
               }
            }
         }
         break;
         
       case IOCTL_DBG_FEX_REMOVE_RULE:
         if (lpInBuf != NULL && nInBufSize == sizeof(DWORD))
         {
            RemoveFilterRule(*(DWORD*)lpInBuf);
            fRequestSucceeded = TRUE;
         }
         break;

       case IOCTL_DBG_FEX_GET_RULE_INFO:
         if (((lpInBuf != NULL) && (nInBufSize == sizeof(DWORD))) && ((lpOutBuf != NULL) && (nOutBufSize == sizeof(KDFEX_RULE_INFO))))
         {
            pLocalRuleInfo = GetFilterRulePtr(*(DWORD*)lpInBuf);
            pRuleInfoProvided = (KDFEX_RULE_INFO *)lpOutBuf;
            if (pLocalRuleInfo != NULL)
            {
               CopyFilterRuleInfo(pRuleInfoProvided, pLocalRuleInfo);
               fRequestSucceeded = TRUE;
            }
         }
         break;

       default:
         fRequestSucceeded = FALSE;
         break;
   }

   return fRequestSucceeded;
}


BOOL
FilterTrap (
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT *pContextRecord,
    BOOL SecondChance,
    BOOL *pfIgnore
    )
{
    CHAR *szProcName;
    KDFEX_RULE_DECISION decision;
    BOOL filtered;

    filtered = FALSE;
    
    if (!g_fFilterExceptions)
        goto exit;

    *pfIgnore = TRUE;

    decision = GetFilterRuleDecision((DWORD)ExceptionRecord->ExceptionCode, (DWORD)ExceptionRecord->ExceptionAddress, (SecondChance ? KDFEX_HANDLING_UNHANDLED : KDFEX_HANDLING_HANDLED));

    szProcName = GetWin32ExeName (GetPCB()->pCurPrc);
                                
    if (decision == KDFEX_RULE_DECISION_IGNORE)
    {
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdTrap: IGNORING EXCEPTION (0x%08x) in process '%s'--\r\n", ExceptionRecord->ExceptionCode, szProcName));
        *pfIgnore = FALSE;
        filtered = TRUE;
    }
    else if (decision == KDFEX_RULE_DECISION_ABSORB)
    {
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdTrap: ABSORBING EXCEPTION (0x%08x) in process '%s'--\r\n", ExceptionRecord->ExceptionCode, szProcName));
        *pfIgnore = TRUE;
        filtered = TRUE;
    }

exit:
    return filtered;
}


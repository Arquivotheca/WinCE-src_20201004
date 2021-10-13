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


/***************
*
* 1. SUMMARY  - EXCEPTION FILTERING RULE SYSTEM
*
* An exception filtering ruleset that gets evaluated by the exception trapping mechanism of the kernel debugger 
* to decide if the debugger should break or continue on the current exception.
*
* The exception filtering ruleset is composed by multiple rules that relate to an 'exception condition' 
* that could be reached in the system. The rule could be very general or very specific depending on the amount of 
* conditions that compose the rule.
*
* The rules receive a score calculated based on how well they match an exception context. 
* When an exception occurs, the context of such exception is evaluated against all the rules in the ruleset, each rule will 
* receive a score and the rule with the highest score will tell us what to do with the current exception (break or not)
*
* The more specific the rule, the highest the score it can get if it matches the exception at stake.

* The exception filtering subsystem can be turned ON or OFF as needed
* If filtering is turned OFF then the kernel debugger will act normally and no rules will be evaluated.
*
*
* 2. RULE COMPOSITION
*
* A rule is defined by a KDFEX_RULE_INFO structure.
* See <kdioctl.h> for more information on the fields
*
* 
* 3. RULE SCORE CALCULATION
*
* The rule declares which fields inside the KDFEX_RULE_INFO structure are relevant and should be evaluated to calculate the score.
*
* If the rule declares that (for example) the process name is relevant then, when an exception is evaluated, the process name for the exception will
* be compared against the one specified in the rule information. If it matches the score is incremented, if it doesn't match then the score becomes negative
* indicating that the rule doesn't apply for the exception evaluated.
*
* Scores:
* Exact Match for a relevant field       : +3
* Match by inclusion for a relevent field: +2
* Irrelevant field of an active rule     : +1
* Miss for a relevent field              : -100
*
* 4. DEALING WITH AMBIGUITY
* 
* It is possible that 2 or more rules will have the same score for a given exception. This happens because the rule fields
* are considered equally relevant.
* In such case the first rule in the ruleset is the one applied.
*
*
* 5. CONTROLLING EXCEPTION FILTERING
*
* 5.1 Advanced commands interface
*
* fex ?                                             - Request help on Filter Exceptions commands
* fex on/off                                        - Enable or disable Filter Exceptions functionality
* fex ar code/all procname/all level allow/ignore   - Add an exception filter rule
* fex ar ?                                          - More help about the fex ar command
* fex dr ruleIndex                                  - Delete an exception filter rule
* fex ad ruleIndex                                  - Enable dump creation for the given rule index
* fex cr [rule index] [field num] [new value]       - Chance a value of a field of the given rule
*
* [FEX ar] Explained
* code/all : 
*          specifies the exception code that this rule should be applied to,
*          you can use a particular hexadecimal value or the all keyword to refer to all possible values.
* procname/all : 
*          specifies the process name that this rule should be applied to,
*          you can use an app name myapp.exe or the all keyword to refer to all possible procs.
* level   : 
*          specifies the handling of the exception that this rule should be applied to,
*          you can use 1 of handled, 4 for unhandled. 
* allow/ignore : 
*          specifies the action to take when this rule is applied. Allow means break into the debugger
*          ignore means dont break into the debugger
*
* 
* 5.2 IOCTL Interface
*
* Refer to <kdioctl.h> for more information
*
*
* 7. MINIMUM RULESET RECOMMENDATION
*
* It is strongly recommended that the next 3 rules are ALWAYS included in the ruleset,
* these 3 exception filtering rules allow the debugger to perform the most basic debugging operations like Break,
* Single Step and StepTo.
*
* + Break if an exception code = 0x80000003 occurs.
* + Break if an exception code = 0x80000004 occurs.
* + Break if an exception code = 0x80000114 occurs.
*
***************/



#pragma once
#include <kernel.h>
#include <kdioctl.h>


//Generic values used to specify a "Any/All" condition in a filter rule
#define KDFEX_ALL "all"

//Max number of filter rules
#define KDFEX_MAXRULES 25


///<summary>
/// Function used to obtain a decision based on the exception filtering ruleset for a particular exception.
/// This function is called by the FilterTrap function.
///</summary>
///<param name="dwExceptionCode">The exception code of the current exception</param>
///<param name="dwExceptionAddress">The address at which the current exception occurred</param>
///<param name="handlingLevel" ref="KDFEX_HANDLING">Handling level of the current exception</param>
///<returns>KDFEX_RULE_DECISION to be taken against the exception evaluated</returns>
///<remarks>
/// 1. If the exception filter is turned OFF the function always returns KDFEX_RULE_DECISION_BREAK
/// 2. If the exception filter is turned ON
///     If there is no rule that can be applied to the exception the function returns KDFEX_RULE_DECISION_IGNORE
///     If there are multiple rules that match the exception, the function returns the KDFEX_RULE_DECISION from the rule that best matches the exception
///     In case of multiple rules that match the exception with the same score the function returns the KDFEX_RULE_DECISION from the first rule found.
///</remarks>
KDFEX_RULE_DECISION GetFilterRuleDecision(DWORD dwExceptionCode,DWORD dwExceptionAddress, KDFEX_HANDLING handlingLevel);

///<summary>
/// Function used to add an exception filtering rule to the filtering ruleset.
///</summary>
///<param name="pFexRuleInfo" ref="KDFEX_RULE_INFO">Pointer to a valid KDFEX_RULE_INFO structure that describes the rule to add</param>
///<returns>On Success the function returns the positive 0 based index of the rule added</returns>
///<returns>On Failure the function returns a negative value (see remarks)</returns>
///<remarks>
/// 1. The function will return -1 (error) if the specified rule is NULL or any of its fields invalid
/// 2. The function will return -2 (error) if the ruleset is full and the specified rule cannot be added.
/// 3. The provided KDFEX_RULE_INFO structure should be initialized and its dwSize member equal to sizeof(KDFEX_RULE_INFO)
/// 4. Rules can be added to the ruleset even if exception filtering is OFF. Once filtering is turned ON the rules will be evaluated as expected.
///<remarks>
int  AddFilterRule(IN KDFEX_RULE_INFO *pFexRuleInfo);

///<summary>
/// Function used to add a simple exception filtering rule to the filtering ruleset.
///</summary>
///<param name="dwExceptionCode">The exception code of the current exception</param>
///<param name="decision" ref="KDFEX_RULE_DECISION">Decision to be applied if this rule is matched</param>
///<returns>On Success the function returns the positive 0 based index of the rule added</returns>
///<returns>On Failure the function returns a negative value (see remarks)</returns>
///<remarks>
/// 1. The function will return -1 (error) if the specified rule is NULL or any of its fields invalid
/// 2. The function will return -2 (error) if the ruleset is full and the specified rule cannot be added.
/// 3. The provided KDFEX_RULE_INFO structure should be initialized and its dwSize member equal to sizeof(KDFEX_RULE_INFO)
///<remarks>
int  AddSimpleFilterRule(DWORD dwExceptionCode, KDFEX_RULE_DECISION decision);

///<summary>
/// Removes an exception filtering rule from the exception filtering ruleset
///</summary>
///<param name="ruleIndex">0 based index of the rule to be removed</param>
///<remarks>
/// 1. If the specified index is invalid or is not occupied by an active rule, the function doesn't perform any operation.
///</remarks>
void RemoveFilterRule(DWORD ruleIndex);

///<summary>
/// Gets the pointer to the KDFEX_RULE_INFO structure that contains the rule information
///</summary>
///<param name="ruleIndex">0 based index of the rule to be obtained</param>
///<returns>NULL if the specified index is invalid</returns>
///<returns>A pointer to the KDFEX_RULE_INFO structure for the provided index</returns>
KDFEX_RULE_INFO* GetFilterRulePtr(DWORD ruleIndex);

///<summary>
/// Gets a friendly name that represents the provided handling level
///</summary>
///<param name="handlingLevel" ref="KDFEX_HANDLING">handling level</param>
///<returns>const WCHAR* pointing to the name for the provided handling level</returns>
const WCHAR* GetHandlingLevelTypeName(KDFEX_HANDLING handlingLevel);

///<summary>
/// Gets a friendly name that represents the provided decision value
///</summary>
///<param name="decision" ref="KDFEX_RULE_DECISION">decision value</param>
///<returns>const WCHAR* pointing to the name for the provided decision value</returns>
const WCHAR* GetFilterRuleDecisionName(KDFEX_RULE_DECISION decision);

///<summary>
/// Evaluates if the given field is relevant according to the given relevance flags 
///</summary>
///<param name="fgRelevanceFlags">relevance flags to be evaluated</param>
///<param name="field" ref="KDFEX_RULE_RELEVANCE_FLAGS">field to check in the relevance flags</param>
///<returns>TRUE if the field is relevant according to the given relevance flags</returns>
///<returns>FALSE if the field is not relevant according to the given relevance flags</returns>
BOOL IsRelevantField(DWORD fgRelevanceFlags, KDFEX_RULE_RELEVANCE_FLAGS field);


///<summary>
/// IOCTL entry point for IOCTL codes related to exception filtering
///</summary>
///<remarks>
///The values for each of the parameters depend on the IOCTL code that needs to be handled
///refer to <kdioctl.h> for more information
///</remarks>
BOOL 
KDFex_IOControl(
    DWORD   dwInstData,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned);

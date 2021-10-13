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
/*
** These test hooks are provided to support the WinPVR Test team's
** needs.
*/
#ifndef __DVREngineTestHooks_h
#define __DVREngineTestHooks_h

#include "DVREngine.h"
#include "CopyProtectionPolicies.h"

#ifdef __cplusplus
extern "C" {
#endif

// Exported from the DVR engine DLL:
HRESULT DVRENGINE_API DVREngine_OverrideCopyProtectionInput(
		CGMS_A_Policy eCGMSA, 
		MacrovisionPolicy eMacrovisionVBI, 
		MacrovisionPolicy eMacrovisionProperty);

HRESULT DVRENGINE_API DVREngine_ResumeNormalCopyProtection();

// Internal to the DVR engine DLL, called by the Tagger:
bool DVREngine_GetOverrideCopyProtectionInput(
		CGMS_A_Policy &eCGMSA, 
		MacrovisionPolicy &eMacrovisionVBI, 
		MacrovisionPolicy &eMacrovisionProperty,
		ULONG & uCopyProtectionOverrideVersion);
void DVREngine_ExpireVBICopyProtectionInput();

#ifdef __cplusplus
}
#endif

#endif // __DVREngineTestHooks_h

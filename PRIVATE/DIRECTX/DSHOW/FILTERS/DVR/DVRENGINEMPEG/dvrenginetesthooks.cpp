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
#include "stdafx.h"
#include "DVREngineTestHooks.h"


// File statics:
#ifndef SHIP_BUILD
static bool g_fOverrideCopyProtection = false;
static CGMS_A_Policy g_eCGMSAOverride = CGMS_A_NotApplicable;
static MacrovisionPolicy g_eMacrovisionVBIOverride = Macrovision_NotApplicable;
static MacrovisionPolicy g_eMacrovisionPropertyOverride = Macrovision_NotApplicable;
static ULONG g_uCopyProtectionOverrideVersion = 0;
static CCritSec s_cCritSecCopyProtectionOverride;
#endif // !SHIP_BUILD

// Exported from the DVR engine DLL:
HRESULT DVRENGINE_API DVREngine_OverrideCopyProtectionInput(
		CGMS_A_Policy eCGMSA,
		MacrovisionPolicy eMacrovisionVBI,
		MacrovisionPolicy eMacrovisionProperty)
{
#ifndef SHIP_BUILD
	CAutoLock cAutoLock(&s_cCritSecCopyProtectionOverride);

	g_eCGMSAOverride = eCGMSA;
	g_eMacrovisionVBIOverride = eMacrovisionVBI;
	g_eMacrovisionPropertyOverride = eMacrovisionProperty;
	++g_uCopyProtectionOverrideVersion;
	g_fOverrideCopyProtection = true;
	return S_OK;
#else // !SHIP_BUILD
	return E_NOTIMPL;
#endif // !SHIP_BUILD
} // DVREngine_OverrideCopyProtectionInput

HRESULT DVRENGINE_API DVREngine_ResumeNormalCopyProtection()
{
#ifndef SHIP_BUILD
	CAutoLock cAutoLock(&s_cCritSecCopyProtectionOverride);

	g_eCGMSAOverride = CGMS_A_NotApplicable;
	g_eMacrovisionVBIOverride = Macrovision_NotApplicable;
	g_eMacrovisionPropertyOverride = Macrovision_NotApplicable;
	++g_uCopyProtectionOverrideVersion;
	g_fOverrideCopyProtection = false;
	return S_OK;
#else // DEBUG
	return E_NOTIMPL;
#endif // !SHIP_BUILD
} // DVREngine_ResumeNormalCopyProtection

// Internal to the DVR engine DLL, called by the Tagger:
bool DVREngine_GetOverrideCopyProtectionInput(
		CGMS_A_Policy &eCGMSA,
		MacrovisionPolicy &eMacrovisionVBI,
		MacrovisionPolicy &eMacrovisionProperty,
		ULONG & uCopyProtectionOverrideVersion)
{
#ifndef SHIP_BUILD
	CAutoLock cAutoLock(&s_cCritSecCopyProtectionOverride);

	eCGMSA = g_eCGMSAOverride;
	eMacrovisionVBI = g_eMacrovisionVBIOverride;
	eMacrovisionProperty = g_eMacrovisionPropertyOverride;
	uCopyProtectionOverrideVersion = g_uCopyProtectionOverrideVersion;
	return g_fOverrideCopyProtection;
#else // !SHIP_BUILD
	eCGMSA = CGMS_A_NotApplicable;
	eMacrovisionVBI = Macrovision_NotApplicable;
	eMacrovisionProperty = Macrovision_NotApplicable;
	uCopyProtectionOverrideVersion = 0;
	return false;
#endif // !SHIP_BUILD
} // DVREngine_GetOverrideCopyProtectionInput

void DVREngine_ExpireVBICopyProtectionInput()
{
#ifndef SHIP_BUILD
	CAutoLock cAutoLock(&s_cCritSecCopyProtectionOverride);

	g_eCGMSAOverride = CGMS_A_NotApplicable;
	g_eMacrovisionVBIOverride = Macrovision_NotApplicable;
	++g_uCopyProtectionOverrideVersion;
#endif // !SHIP_BUILD
} // DVREngine_ExpireVBICopyProtectionInput

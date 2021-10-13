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
#pragma once

#include "CopyProtectionPolicies.h"

namespace MSDvr
{
	// Helper class and enumerations for obtaining the copy protection
	// policy from a tagged media sample:

	// Tag bits and fields:

	enum {
		// Sample tag fields offsets (shifts from bit 0):

		SAMPLE_FIELD_OFFSET_CGMSA_PRESENT = 49,			 // bit 49
		SAMPLE_FIELD_OFFSET_CGMSA = 50,					 // bits 50-51
		SAMPLE_FIELD_OFFSET_VBI_MACROVISION = 52,		 // bits 52-53
		SAMPLE_FIELD_OFFSET_KS_MACROVISION_PRESENT = 54, // bit 54
		SAMPLE_FIELD_OFFSET_KS_MACROVISION = 55,		 // bits 55-56
		SAMPLE_FIELD_OFFSET_KS_ENCRYPTED = 57,

		// Event data fields offsets (shifts from bit 0):

		EVENT_FIELD_OFFSET_CGMSA_PRESENT = 0,			 // bit 0
		EVENT_FIELD_OFFSET_CGMSA = 1,					 // bits 1-2
		EVENT_FIELD_OFFSET_VBI_MACROVISION = 3,			 // bits 3-4
		EVENT_FIELD_OFFSET_KS_MACROVISION_PRESENT = 5,	 // bit 5
		EVENT_FIELD_OFFSET_KS_MACROVISION = 6,			 // bits 6-7

		SAMPLE_FIELD_MASK_CGMSA = 0x3,					 // a 2 bit field
		SAMPLE_FIELD_MASK_MACROVISION = 0x3,			 // a 2 bit field

		// Field values/flags
		SAMPLE_ENUM_MACROVISION_DISABLED = 0,
		SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01 = 1,
		SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2 = 2,
		SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4 = 3,

		SAMPLE_ENUM_CGMSA_COPYFREELY = 0,
		SAMPLE_ENUM_CGMSA_COPYONCE = 2,
		SAMPLE_ENUM_CGMSA_COPYNEVER = 3,
		SAMPLE_ENUM_CGMSA_RESERVED = 1
	};

	struct SContentRestrictionStamp
	{
		DWORD dwSize;
		CGMS_A_Policy eCGMSAPolicy;
		MacrovisionPolicy eMacrovisionPolicyVBI;
		MacrovisionPolicy eMacrovisionPolicyProperty;

		// Not used yet:
		EnTvRat_System eEnTvRatSystem;
		EnTvRat_GenericLevel eEnTvRatGeneric;
		DWORD dwBfEnShowAttributes;
		DWORD dwProgramCounter;
		DWORD dwTuningCounter;
		BOOL fTuneComplete;
		BOOL fEncrypted;
	};

} // namespace MSDvr

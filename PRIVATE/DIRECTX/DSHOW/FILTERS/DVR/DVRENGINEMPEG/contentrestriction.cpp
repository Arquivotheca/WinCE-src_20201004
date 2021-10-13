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

#include "stdafx.h"
#include "ContentRestriction.h"

using namespace MSDvr;

CGMS_A_Policy CCopyProtectionExtractor::GetCGMSA(IMediaSample &riMediaSample)
{
	REFERENCE_TIME rtStart, rtEnd;
	CGMS_A_Policy eCGMSAPolicy = CGMS_A_NotApplicable;

	HRESULT hr = riMediaSample.GetTime(&rtStart, &rtEnd);
	if (SUCCEEDED(hr))
	{
		// Note:  Constant values such as SAMPLE_FIELD_OFFSET_CGMSA_PRESENT are 
		//        defined as enumeration values (per coding policy for the DVR
		//        engine). The compiler limits enumeration values to 32 bits. So 
		//        instead of defining a bit-field directly, the code here and
		//        the rest of the code defines the number of bits to shift and
		//        then checks for the shifted bit.
		if ((rtStart >> SAMPLE_FIELD_OFFSET_CGMSA_PRESENT) & 0x1)
		{
			switch ((rtStart >> SAMPLE_FIELD_OFFSET_CGMSA) & SAMPLE_FIELD_MASK_CGMSA)
			{
			case SAMPLE_ENUM_CGMSA_COPYFREELY:
				eCGMSAPolicy = CGMS_A_CopyFreely;
				break;
			case SAMPLE_ENUM_CGMSA_COPYONCE:
				eCGMSAPolicy = CGMS_A_CopyOnce;
				break;
			case SAMPLE_ENUM_CGMSA_COPYNEVER:
				eCGMSAPolicy = CGMS_A_CopyNever;
				break;
			case SAMPLE_ENUM_CGMSA_RESERVED:
				eCGMSAPolicy = CGMS_A_Reserved;
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	}

	return eCGMSAPolicy;
} // CCopyProtectionExtractor::GetCGMSA

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovisionVBI(IMediaSample &riMediaSample)
{
	REFERENCE_TIME rtStart, rtEnd;
	MacrovisionPolicy eMacrovisionPolicy = Macrovision_NotApplicable;

	HRESULT hr = riMediaSample.GetTime(&rtStart, &rtEnd);
	if (SUCCEEDED(hr))
	{
		if (((rtStart >> SAMPLE_FIELD_OFFSET_CGMSA_PRESENT) & 0x1) &&
			(((rtStart >> SAMPLE_FIELD_OFFSET_CGMSA) & SAMPLE_FIELD_MASK_CGMSA) == SAMPLE_ENUM_CGMSA_COPYNEVER))
		{
			// There are Macrovision qualifiers associated with CGMS-A copy-never:

			switch ((rtStart >> SAMPLE_FIELD_OFFSET_VBI_MACROVISION) & SAMPLE_FIELD_MASK_MACROVISION)
			{
			case SAMPLE_ENUM_MACROVISION_DISABLED:
				eMacrovisionPolicy = Macrovision_Disabled;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_01;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe2;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe4;
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	}

	return eMacrovisionPolicy;
} // CCopyProtectionExtractor::GetMacrovisionVBI

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovisionProperty(IMediaSample &riMediaSample)
{
	REFERENCE_TIME rtStart, rtEnd;
	MacrovisionPolicy eMacrovisionPolicy = Macrovision_NotApplicable;

	HRESULT hr = riMediaSample.GetTime(&rtStart, &rtEnd);
	if (SUCCEEDED(hr))
	{
		if ((rtStart >> SAMPLE_FIELD_OFFSET_KS_MACROVISION_PRESENT) & 0x1)
		{
			// Macrovision policy was explicitly supplied via IKsPropertySet:

			switch ((rtStart >> SAMPLE_FIELD_OFFSET_KS_MACROVISION) & SAMPLE_FIELD_MASK_MACROVISION)
			{
			case SAMPLE_ENUM_MACROVISION_DISABLED:
				eMacrovisionPolicy = Macrovision_Disabled;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_01;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe2;
				break;
			case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4:
				eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe4;
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	}

	return eMacrovisionPolicy;
} // CCopyProtectionExtractor::GetMacrovisionProperty

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovision(IMediaSample &riMediaSample)
{
	return CombineMacrovision(GetMacrovisionVBI(riMediaSample), GetMacrovisionProperty(riMediaSample));
} // CCopyProtectionExtractor::GetMacrovision

MacrovisionPolicy CCopyProtectionExtractor::CombineMacrovision(
	MacrovisionPolicy eMacrovisionPolicyVBI, MacrovisionPolicy eMacrovisionPolicyProperty)
{
	// We have carefully ordered the enumeration so that increasing restrictions go
	// hand-in-hand with increasing enumeration order:

	return (eMacrovisionPolicyVBI < eMacrovisionPolicyProperty) ? eMacrovisionPolicyProperty : eMacrovisionPolicyVBI;
} // CCopyProtectionExtractor::CombineMacrovision

CopyProtectionPolicy CCopyProtectionExtractor::GetCopyProtection(
	IMediaSample &riMediaSample)
{
	return CombineCopyProtection(GetMacrovision(riMediaSample), GetCGMSA(riMediaSample));
} // CCopyProtectionExtractor::GetCopyProtection

bool CCopyProtectionExtractor::IsEncrypted(IMediaSample &riMediaSample)
{
	REFERENCE_TIME rtStart, rtEnd;
	bool fEncrypted = false;

	HRESULT hr = riMediaSample.GetTime(&rtStart, &rtEnd);
	if (SUCCEEDED(hr))
	{
		if (rtStart & ((LONGLONG)1 << SAMPLE_FIELD_OFFSET_KS_ENCRYPTED))
			fEncrypted = true;
	}
	return fEncrypted;
} // CCopyProtectionExtractor::IsEncrypted

CopyProtectionPolicy CCopyProtectionExtractor::CombineCopyProtection(
	MacrovisionPolicy eMacrovisionPolicy, CGMS_A_Policy eCGMSAPolicy)
{
	CopyProtectionPolicy eCopyProtectionPolicy = CopyPolicy_CopyFreely;
	switch (eMacrovisionPolicy)
	{
	case Macrovision_NotApplicable:
	case Macrovision_Disabled:
		switch (eCGMSAPolicy)
		{
		case CGMS_A_NotApplicable:
			eCopyProtectionPolicy = CopyPolicy_NotApplicable;
			break;

		case CGMS_A_CopyFreely:
		case CGMS_A_Reserved:
			eCopyProtectionPolicy = CopyPolicy_CopyFreely;
			break;

		case CGMS_A_CopyOnce:
			eCopyProtectionPolicy = CopyPolicy_CopyOnce;
			break;

		case CGMS_A_CopyNever:
			eCopyProtectionPolicy = CopyPolicy_CopyNever;
			break;

		default:
			ASSERT(0);
			break;
		}
		break;
	case Macrovision_CopyNever_AGC_01:
	case Macrovision_CopyNever_AGC_ColorStripe2:
	case Macrovision_CopyNever_AGC_ColorStripe4:
		eCopyProtectionPolicy = CopyPolicy_CopyNever;
		break;

	default:
		ASSERT(0);
		break;
	}
	return eCopyProtectionPolicy;
} // CCopyProtectionExtractor::GetCopyProtection

CGMS_A_Policy CCopyProtectionExtractor::GetCGMSA(unsigned uCopyProtectionEventData)
{
	CGMS_A_Policy eCGMSAPolicy = CGMS_A_NotApplicable;

	if ((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_CGMSA_PRESENT) & 0x1)
	{
		switch ((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_CGMSA) & SAMPLE_FIELD_MASK_CGMSA)
		{
		case SAMPLE_ENUM_CGMSA_COPYFREELY:
			eCGMSAPolicy = CGMS_A_CopyFreely;
			break;
		case SAMPLE_ENUM_CGMSA_COPYONCE:
			eCGMSAPolicy = CGMS_A_CopyOnce;
			break;
		case SAMPLE_ENUM_CGMSA_COPYNEVER:
			eCGMSAPolicy = CGMS_A_CopyNever;
			break;
		case SAMPLE_ENUM_CGMSA_RESERVED:
			eCGMSAPolicy = CGMS_A_Reserved;
			break;
		default:
			ASSERT(0);
			break;
		}
	}

	return eCGMSAPolicy;
} // CCopyProtectionExtractor::GetCGMSA

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovisionVBI(unsigned uCopyProtectionEventData)
{
	MacrovisionPolicy eMacrovisionPolicy = Macrovision_NotApplicable;

	if (((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_CGMSA_PRESENT) & 0x1) &&
		(((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_CGMSA) & SAMPLE_FIELD_MASK_CGMSA) == SAMPLE_ENUM_CGMSA_COPYNEVER))
	{
		// There are Macrovision qualifiers associated with CGMS-A copy-never:

		switch ((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_VBI_MACROVISION) & SAMPLE_FIELD_MASK_MACROVISION)
		{
		case SAMPLE_ENUM_MACROVISION_DISABLED:
			eMacrovisionPolicy = Macrovision_Disabled;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_01;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe2;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe4;
			break;
		default:
			ASSERT(0);
			break;
		}
	}

	return eMacrovisionPolicy;
} // CCopyProtectionExtractor::GetMacrovisionVBI

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovisionProperty(unsigned uCopyProtectionEventData)
{
	MacrovisionPolicy eMacrovisionPolicy = Macrovision_NotApplicable;

	if ((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_KS_MACROVISION_PRESENT) & 0x1)
	{
		// Macrovision policy was explicitly supplied via IKsPropertySet:

		switch ((uCopyProtectionEventData >> EVENT_FIELD_OFFSET_KS_MACROVISION) & SAMPLE_FIELD_MASK_MACROVISION)
		{
		case SAMPLE_ENUM_MACROVISION_DISABLED:
			eMacrovisionPolicy = Macrovision_Disabled;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_01;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe2;
			break;
		case SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4:
			eMacrovisionPolicy = Macrovision_CopyNever_AGC_ColorStripe4;
			break;
		default:
			ASSERT(0);
			break;
		}
	}

	return eMacrovisionPolicy;
} // CCopyProtectionExtractor::GetMacrovisionProperty

MacrovisionPolicy CCopyProtectionExtractor::GetMacrovision(unsigned uCopyProtectionEventData)
{
	return CombineMacrovision(GetMacrovisionVBI(uCopyProtectionEventData), GetMacrovisionProperty(uCopyProtectionEventData));
} // CCopyProtectionExtractor::GetMacrovision

CopyProtectionPolicy CCopyProtectionExtractor::GetCopyProtection(unsigned uCopyProtectionEventData)
{
	return CombineCopyProtection(GetMacrovision(uCopyProtectionEventData), GetCGMSA(uCopyProtectionEventData));
} // CCopyProtectionExtractor::GetCopyProtection

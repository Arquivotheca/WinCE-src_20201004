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
// Enumerations that are exported to both C and C++:

#ifndef __CopyProtectionPolicies_H
#define __CopyProtectionPolicies_H

#include "DVREngine.h"

typedef enum CGMS_A_Policy {
	CGMS_A_NotApplicable,
	CGMS_A_CopyFreely,
	CGMS_A_CopyOnce,
	CGMS_A_CopyNever,
	CGMS_A_Reserved
} CGMS_A_Policy;

typedef enum MacrovisionPolicy {
	Macrovision_NotApplicable,
	Macrovision_Disabled,
	Macrovision_CopyNever_AGC_01,
	Macrovision_CopyNever_AGC_ColorStripe2,
	Macrovision_CopyNever_AGC_ColorStripe4
} MacrovisionPolicy;

#ifdef __cplusplus

namespace MSDvr
{
	enum CopyProtectionPolicy {
		CopyPolicy_NotApplicable,
		CopyPolicy_CopyFreely,
		CopyPolicy_CopyOnce,
		CopyPolicy_CopyNever
	};

	class DVRENGINE_API CCopyProtectionExtractor
	{
	public:
		static CGMS_A_Policy GetCGMSA(IMediaSample &riMediaSample);
		static MacrovisionPolicy GetMacrovisionVBI(IMediaSample &riMediaSample);
		static MacrovisionPolicy GetMacrovisionProperty(IMediaSample &riMediaSample);
		static MacrovisionPolicy GetMacrovision(IMediaSample &riMediaSample);
		static CopyProtectionPolicy GetCopyProtection(IMediaSample &riMediaSample);

		static bool IsEncrypted(IMediaSample &riMediaSample);

		static CGMS_A_Policy GetCGMSA(unsigned uCopyProtectionEventData);
		static MacrovisionPolicy GetMacrovisionVBI(unsigned uCopyProtectionEventData);
		static MacrovisionPolicy GetMacrovisionProperty(unsigned uCopyProtectionEventData);
		static MacrovisionPolicy GetMacrovision(unsigned uCopyProtectionEventData);
		static CopyProtectionPolicy GetCopyProtection(unsigned uCopyProtectionEventData);

		static CopyProtectionPolicy CombineCopyProtection(
			MacrovisionPolicy eMacrovisionPolicy, CGMS_A_Policy eCGMSAPolicy);
		static MacrovisionPolicy CombineMacrovision(
			MacrovisionPolicy eMacrovisionPolicyVBI, MacrovisionPolicy eMacrovisionPolicyProperty);
	};

}

#endif // __cplusplus

#endif /* __CopyProtectionPolicies_H */

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
////////////////////////////////////////////////////////////////////////////////

#ifndef _PIN_ENUMERATOR
#define _PIN_ENUMERATOR

#include <dshow.h>

class CPinEnumerator
{
private:
	IEnumPins* m_pEnumPins;
	PIN_DIRECTION m_dir;
	GUID m_guidMajorType;
	GUID m_guidSubType;

private:
	// Does the media type match the specified manor and sub types?
	HRESULT IsMatching(AM_MEDIA_TYPE* pMediaType, GUID majortype, GUID subtype);
	
	// This method involves querying for the connected media if connected to match the major and sub type
	// Or to query for atleast one media type that has a matching major and sub type
	HRESULT QueryMatchPinType(IPin* pPin);

public:
	// Construct a pin enumerator 
	CPinEnumerator(IEnumPins* pEnumPins, PIN_DIRECTION dir, GUID majortype, GUID subtype);

	// Destuctor
	~CPinEnumerator();

	// Find the next pin that matches the given specifiers
	HRESULT Next(IPin** ppPin);

	// Reset the enumerator
	HRESULT Reset();
};


#endif
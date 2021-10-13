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

#include "CPinMapping.h"

namespace MSDvr
{
	class CMPEGInputMediaTypeAnalyzer : public IMediaTypeAnalyzer
	{
	public:
		CMPEGInputMediaTypeAnalyzer() {}

		CMediaTypeDescription *AnalyzeMediaType(CMediaType &rcMediaType) const;
	};

	class CMPEGOutputMediaTypeAnalyzer : public IMediaTypeAnalyzer
	{
	public:
		CMPEGOutputMediaTypeAnalyzer() {}

		CMediaTypeDescription *AnalyzeMediaType(CMediaType &rcMediaType) const;
	};

	class CMPEGPSMediaTypeDescription : public CMediaTypeDescription
	{
	public:
		CMPEGPSMediaTypeDescription(CMediaType &rcMediaType);
		virtual ~CMPEGPSMediaTypeDescription();
	};

	class CMPEGVideoMediaTypeDescription : public CMediaTypeDescription
	{
	public:
		CMPEGVideoMediaTypeDescription(CMediaType &rcMediaType);
		virtual ~CMPEGVideoMediaTypeDescription();
	};

	class CDVDAC3MediaTypeDescription : public CMediaTypeDescription
	{
	public:
		CDVDAC3MediaTypeDescription(CMediaType &rcMediaType);
		virtual ~CDVDAC3MediaTypeDescription();
	};

	class CMPEGVBIMediaTypeDescription : public CMediaTypeDescription
	{
	public:
		CMPEGVBIMediaTypeDescription(CMediaType &rcMediaType);
	};

}


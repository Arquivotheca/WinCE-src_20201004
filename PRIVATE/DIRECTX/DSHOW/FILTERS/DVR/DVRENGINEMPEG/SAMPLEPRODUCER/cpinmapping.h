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

namespace MSDvr 
{
	typedef IPin *TPIPin;

	struct SPinMappingCompare : std::binary_function<const TPIPin &, const TPIPin &, bool>
	{
		bool operator () (const TPIPin &arg1, const TPIPin &arg2) const;
	};

	typedef std::map<TPIPin, UCHAR, SPinMappingCompare> t_PinMapping;

	class CMediaTypeDescription
	{
	public:
		CMediaTypeDescription(CMediaType &rcMediaType);
		virtual ~CMediaTypeDescription() {}

		CMediaType m_cMediaType;
		ULONG m_uMinimumMediaSampleBufferSize;
		ULONG m_uMaximumSamplesPerSecond;
		LONGLONG m_hyMaxTimeUnitsBetweenKeyFrames;  // 100 ns time units

		bool m_fIsAudio, m_fIsVideo, m_fIsAVStream;
	};

	struct SPinState
	{
		bool fPinFlushing;
		bool fPinFlushed;
		CMediaTypeDescription *pcMediaTypeDescription;
	};

	typedef std::vector<SPinState> t_PinStates;

	class IMediaTypeAnalyzer
	{
	public:
		IMediaTypeAnalyzer() {}
		virtual ~IMediaTypeAnalyzer() {}

		virtual CMediaTypeDescription *AnalyzeMediaType(CMediaType &rcMediaType) const = 0;
	};

	class CPinMappings 
	{
	public:
		CPinMappings(CBaseFilter &rcBaseFilter, 
			IMediaTypeAnalyzer *piMediaTypeAnalyzer = NULL,
		    IPin *piPinPrimary = NULL,
			bool fMapUnconnectedPins = false);
		~CPinMappings();

		bool IsPrimaryPin(UCHAR bPinIndex) const { return (m_bPrimaryPin == bPinIndex); }
		UCHAR GetPrimaryPin() const { return m_bPrimaryPin; }
		UCHAR FindPinPos(IPin *piPin) const;
		UCHAR FindPinPos(const CMediaType &rcMediaType) const;
		UCHAR GetConnectedPinCount() const { return m_bConnectedPins; }
		IPin &GetPin(UCHAR bPinPos);
		UCHAR GetPinCount() const { return (UCHAR) (m_mapPinToIndex.size()); }
		SPinState &GetPinState(UCHAR bPinPos);

	protected:
		enum PRIMARY_PIN_QUALITY {
			PRIMARY_PIN_QUALITY_IDEAL = 4,
			PRIMARY_PIN_QUALITY_GOOD = 3,
			PRIMARY_PIN_QUALITY_POOR = 2,
			PRIMARY_PIN_QUALITY_LAST_RESORT = 1,
			PRIMARY_PIN_QUALITY_NOT_KNOWN = 0
		};

		static PRIMARY_PIN_QUALITY IsPrimaryPin(IPin &riPin);

		UCHAR m_bPrimaryPin, m_bConnectedPins;
		t_PinMapping m_mapPinToIndex;
		t_PinStates m_vectorPinStates;

	private:
		CPinMappings & operator = (const CPinMappings &);
		CPinMappings(const CPinMappings &);
	};

}

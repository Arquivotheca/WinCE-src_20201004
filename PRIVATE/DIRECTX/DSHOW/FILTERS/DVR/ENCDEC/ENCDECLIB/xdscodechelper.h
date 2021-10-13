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

/*++


    Module Name:

        XDSCodecHelper.h

    Abstract:

        This module contains the declarations for an XDSCodecHelper object
		extracted from Encrypter/Tagger filter declarations

    Revision History:

        07-Mar-2002    created

--*/

#ifndef __EncDec__XDSCodecHelper_h
#define __EncDec__XDSCodecHelper_h


#include "TvRatings.h"
#include "XDSTypes.h"

#ifdef UNDER_CE

// taken from ksmedia.h because the CE version isn't recent enough

// These KS_CC_SUBSTREAM_* bitmasks are used with VBICODECFILTERING_CC_SUBSTREAMS
#define KS_CC_SUBSTREAM_ODD               0x0001L // Unfiltered Field 1 Data
#define KS_CC_SUBSTREAM_EVEN              0x0002L // Unfiltered Field 2 Data

// The following flags describe CC field 1 substreams: CC1,CC2,TT1,TT2
#define KS_CC_SUBSTREAM_FIELD1_MASK    	  0x00F0L
#define KS_CC_SUBSTREAM_SERVICE_CC1       0x0010L
#define KS_CC_SUBSTREAM_SERVICE_CC2       0x0020L
#define KS_CC_SUBSTREAM_SERVICE_T1        0x0040L
#define KS_CC_SUBSTREAM_SERVICE_T2        0x0080L

// The following flags describe CC field 2 substreams: CC3,CC4,TT3,TT4,XDS
#define KS_CC_SUBSTREAM_FIELD2_MASK       0x1F00L
#define KS_CC_SUBSTREAM_SERVICE_CC3       0x0100L
#define KS_CC_SUBSTREAM_SERVICE_CC4       0x0200L
#define KS_CC_SUBSTREAM_SERVICE_T3        0x0400L
#define KS_CC_SUBSTREAM_SERVICE_T4        0x0800L
#define KS_CC_SUBSTREAM_SERVICE_XDS       0x1000L

#else

#include <ks.h>
#include <ksmedia.h>

#endif // UNDER_CE




const DWORD XDS_CMGS_A_B1               = (1<<4);
const DWORD XDS_CMGS_A_B2               = (1<<3);

//  --------------------------------------------------------------------
//  Class used to issue notifications:
//  --------------------------------------------------------------------

struct IXDSCodecCallbacks
{
	virtual void OnNewXDSPacket() = 0;
	virtual void OnNewXDSRating() = 0;
	virtual void OnDuplicateXDSRating() = 0;
};

//  --------------------------------------------------------------------
//  class CXDSCodec
//  --------------------------------------------------------------------

class CXDSCodecHelper
{
    public :

        CXDSCodecHelper(IXDSCodecCallbacks *piXDSCodecCallbacks) ;
        ~CXDSCodecHelper () ;


		// =====================================================================
		// To support filter and VBI input pin actions. Throw an exception if
		// an error

		unsigned GetBufferCountRecommendation() { return 32; }
		unsigned GetBufferSizeRecommendation() { return 10; }

		void OnStop();
		void OnPause();
		void OnRun(REFERENCE_TIME rtStart);
		void OnConnect(IPin *piVBIInputPin);
		void OnDisconnect();
		void CacheClock(IReferenceClock *piReferenceClock);

		// =====================================================================
		// Worker methods

				// called by the parsers
		HRESULT GoNewXDSRatings(IMediaSample * pMediaSample, PackedTvRating TvRat);
		HRESULT GoDuplicateXDSRatings(IMediaSample * pMediaSample, PackedTvRating TvRat);
		HRESULT GoNewXDSPacket(IMediaSample * pMediaSample,  long pktClass, long pktType, BSTR bstrXDSPkt);

				// the  crux of the whole system
		HRESULT	ParseXDSBytePair(IMediaSample * mediaSample, BYTE byte1, BYTE byte2);	// parser our data

				// parse non ratings XDS packets (CGMS_A in particular)
		HRESULT	_ParseXDSBytePair(BYTE byte1,
									BYTE byte2,
									long *pPktClass,
									long *pPktType,
									BSTR *pBsPktData);

				// call to reinit the above parser (called in ResetToDontKnow or TunedChanged)
		HRESULT	InitXDSParser();

				// reinit XDS parser state (for discontinuties), and kickoff event
		HRESULT	ResetToDontKnow(IN IMediaSample *pSample);

		void DoTuneChanged();

		// =====================================================================
		//		IXDSCodec

		STDMETHODIMP
		get_XDSToRatObjOK(
			OUT HRESULT *pHrCoCreateRetVal	
			);

		STDMETHODIMP
		put_CCSubstreamService(				// will return S_FALSE if unable to set
			IN long SubstreamMask
			);

		STDMETHODIMP
		get_CCSubstreamService(
			OUT long *pSubstreamMask
			);

		STDMETHODIMP
		GetContentAdvisoryRating(
			OUT PackedTvRating *pRat,			// long
			OUT long *pPktSeqID,
			OUT long *pCallSeqID,
			OUT REFERENCE_TIME *pTimeStart,	// time this sample started
			OUT REFERENCE_TIME *pTimeEnd
			);


		STDMETHODIMP GetXDSPacket(
			OUT long *pXDSClassPkt,				// ENUM EnXDSClass		
			OUT long *pXDSTypePkt,
			OUT BSTR *pBstrCCPkt,
			OUT long *pPktSeqID,
			OUT long *pCallSeqID,
			OUT REFERENCE_TIME *pTimeStart,	// time this sample started
			OUT REFERENCE_TIME *pTimeEnd
			);


        //  ====================================================================
        //  class methods

		HRESULT
		SetSubstreamChannel(
				DWORD dwChanType	// bitfield of:  KS_CC_SUBSTREAM_SERVICE_CC1, _CC2,_CC3, _CC4,  _T1, _T2, _T3 _T4 And/Or _XDS;
		);


        HRESULT
        Process (
            IN  IMediaSample *
            ) ;

private:
	FILTER_STATE				m_eFilterState;
    BOOL                        m_fJustDiscontinuous;   // latch to detect sequential discontinuity samples
	CComPtr<IPin>				m_pPinCCDecoder;
	DWORD						m_dwSubStreamMask;
	IReferenceClock			   *m_piReferenceClock;
	REFERENCE_TIME              m_rtStart;
    CCritSec					m_cCritSec;
	IXDSCodecCallbacks		   *m_piXDSCodecCallbacks;

	CComPtr<IXDSToRat>			m_spXDSToRat;

	HRESULT						m_hrXDSToRatCoCreateRetValue;

		// Sequence Counters
	long						m_cTvRatPktSeq;
	long						m_cTvRatCallSeq;
	PackedTvRating				m_TvRating;
	REFERENCE_TIME				m_TimeStartRatPkt;
	REFERENCE_TIME				m_TimeEndRatPkt;

	long						m_cXDSPktSeq;
	long						m_cXDSCallSeq;
	long						m_XDSClassPkt;
	long						m_XDSTypePkt;
	CComBSTR					m_spbsXDSPkt;
	REFERENCE_TIME				m_TimeStartXDSPkt;
	REFERENCE_TIME				m_TimeEndXDSPkt;

             // times of last packet recieved for non-packet (tune) events
    REFERENCE_TIME              m_TimeStart_LastPkt;
    REFERENCE_TIME              m_TimeEnd_LastPkt;

				// XDS Parsing
	BOOL						m_fIsXDSParsing;
	BOOL						m_fXDSParsingPaused;
	long						m_cXDSLoc;
	char						m_rgXDSBuff[256];		// actually need multiple of these for true parser.. 1 ok just for CGMS-A
    DWORD                       m_crcXDS;               // need multiple of these too...
                // Stats
    DWORD                       m_cRestarts;
    DWORD                       m_cPackets;             // number of byte-pairs given to parse
    DWORD                       m_cRatingsDetected;     // times parsed a Rating
    DWORD                       m_cRatingsFailures;     // parse errors
    DWORD                       m_cRatingsChanged;      // times ratings changed
    DWORD                       m_cRatingsDuplicate;    // times same ratings duplicated
    DWORD                       m_cUnratedChanged;      // times changed into 'unrated' rating
    DWORD                       m_cRatingsGets;         // times rating values requested
    DWORD                       m_cXDSGets;             // times XDS Packet values requested
    DWORD                       m_cCGMSA_Packets;       // times we got good CGMDA data
    DWORD                       m_cCGMSA_Bad_Packets;   // times CGMDA packets failed parity or CRC's
    DWORD                       m_cGoodParity;          // number of bytes with good parities found
    DWORD                       m_cBadParity;           // number of bytes with bad parities found

    void    InitStats()
    {
            m_cPackets = m_cUnratedChanged = m_cRatingsDetected = m_cRatingsChanged = m_cRatingsDuplicate = m_cRatingsFailures = 0;
            m_cRatingsGets = m_cXDSGets = 0;
            m_cCGMSA_Packets = m_cCGMSA_Bad_Packets = m_cGoodParity = m_cBadParity = 0;
            m_cRestarts++;
    }
} ;

#endif  //  __EncDec__XDSCodecHelper_h

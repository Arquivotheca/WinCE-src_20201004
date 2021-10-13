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
#include "..\\PipelineInterfaces.h"
#include "mpegparse.h"

namespace MSDvr
{

class CMpegEncoderReceiver : public ICapturePipelineComponent
{
public:
	CMpegEncoderReceiver() :	m_uiLastPTS(0),
								m_llLastMT(0),
								m_llPTS64(0),
								m_dwLastTickCount(0) {}

	// The guts of this component.  It basicly finds the first PTS in the media sample,
	// converts that to a media time, sets the media time, and sends the sample on.
	ROUTE ProcessInputSample(	IMediaSample& riSample,
								CDVRInputPin& rcInputPin)
	{
		static DWORD dwAssertAt = -1;

		if (GetTickCount() > dwAssertAt)
		{
			RETAILMSG(1, (L"* BUGBUG: Asserting - retrieve last recording file and look at bum pts.\n"));
			ASSERT(FALSE);
		}

		if (rcInputPin.GetPinNum() != 0)
			return UNHANDLED_CONTINUE;		// let non-MPEG samples pass on through.

		// Get the buffer pointer
		BYTE *pBuf;
		if (FAILED(riSample.GetPointer(&pBuf)))
		{
			ASSERT(FALSE);
			return UNHANDLED_STOP;
		}

		// Reset if we're at a discontinuity
		bool fIsDiscontinuity = riSample.IsDiscontinuity() == S_OK;
		bool fSyncPoint = riSample.IsSyncPoint() == S_OK;

		// Get the buffer length and make sure it's 2k aligned
		DWORD dwLen = riSample.GetActualDataLength();
		if ((dwLen % 2048) != 0)
		{
			ASSERT(FALSE);
			return UNHANDLED_STOP;
		}

		if (!fSyncPoint)
		{
			// Not a sync point so just bump the media time
			m_llLastMT++;
		}
		else
		{
			// Walk through each pack until we find a valid PTS
			UINT32 uiPTS = 0;
			bool fFound = false;
			for (DWORD dwOffset = 0; (dwOffset < dwLen) && !fFound; dwOffset += 2048)
			{
				fFound = GetPts(pBuf + dwOffset, &uiPTS);
			}

			if (!fFound)
			{
				// This is especially bad.  Clear the sync point flag
				ASSERT(FALSE);
				RETAILMSG(1, (L"* BUGBUG:  MpegEncoderReceiver received sync point with no video PTS.\n"));
				riSample.SetSyncPoint(FALSE);
				m_llLastMT++;
			}
			else
			{
				// Update our 64-bit PTS
//				DEBUGMSG(1, (L"* %d : %d\n", uiPTS, (DWORD) fIsDiscontinuity));

				if (uiPTS < m_uiLastPTS)
				{
					DEBUGMSG(1, (L"MpegEncoderReceiver: 418 PTS ROLLED OVER.\n"));
					m_llPTS64 += MAX_PTS + uiPTS - m_uiLastPTS;	// Rollover
				}
				else
				{
					m_llPTS64 += uiPTS - m_uiLastPTS;	// Typical case
				}

				// Update the last PTS and media time
				m_uiLastPTS = uiPTS;
				m_llLastMT = PTS_To_DShow_Time(m_llPTS64);
			}
			m_dwLastTickCount = GetTickCount();
		}

		// Set the media time
		LONGLONG llMTEnd = m_llLastMT + 1;
		riSample.SetMediaTime(&m_llLastMT, &llMTEnd);

		return HANDLED_CONTINUE;
	}

	// Mandatory functions which we just NOP
	void RemoveFromPipeline() {}
	ROUTE ConfigurePipeline(UCHAR, CMediaType[], UINT, BYTE[]) { return HANDLED_CONTINUE; }	
	unsigned char AddToPipeline(ICapturePipelineManager&) { return 0; }

private:
	UINT32 m_uiLastPTS;				// Keeps track of the last PTS we've encountered.
	LONGLONG m_llPTS64;				// If we had a 64-bit PTS that didn't reset, this is what
									// it'd be.
	LONGLONG m_llLastMT;			// Last media time delivered.
	DWORD m_dwLastTickCount;		// Tick count when last sample was passed on

	static const LONGLONG MAX_PTS = 0x100000000;			// 2 ^ 32
	static const UINT32 VOBU_LEN_IN_PTS = 45045;			// 1 vobu in pts

	inline LONGLONG PTS_To_DShow_Time(LONGLONG llPTS) { return llPTS * 1000 / 9; }

	// GetPTS() in mpegparse.h assumes that the system header
	// will never be coded into a video pack (in DVD, system
	// headers always live in the nav pack), so here's a quick
	// method that doesn't make that assumption.
	bool GetPts(BYTE *pPack, UINT32 *pPts)
	{
		if (!pPack || !pPts)
			{
			ASSERT(false);
			return false;
			}

		*pPts = 0;
		long Offset = 14;

		ASSERT( pPack[0] == 0x00 &&
				pPack[1] == 0x00 &&
				pPack[2] == 0x01 &&
				pPack[3] == 0xBA );

		// add pack_stuffing_length to the offset
		Offset += (pPack[13] & 0x07);

		// now look for system or PES header
		ASSERT( pPack[Offset    ] == 0x00 &&
				pPack[Offset + 1] == 0x00 &&
				pPack[Offset + 2] == 0x01 );

		if (pPack[Offset + 3] == 0xBB)
			{
			// found a system header, adjust the offset
			long Length =
				(static_cast<long>(pPack[Offset + 4]) << 8) |
				pPack[Offset + 5];

			// header_length field is the length of the header
			// _excluding_ the first 6 bytes
			Offset += (6 + Length);

			// should be looking at a PES header
			ASSERT( pPack[Offset    ] == 0x00 &&
					pPack[Offset + 1] == 0x00 &&
					pPack[Offset + 2] == 0x01 );
			}

		if ( 0xE0 == (pPack[Offset + 3] & 0xE0) )
			{
			// found a video pack, check the first bit of the
			// PTS_DTS_flags field to see if there's a PTS
			if ( 0 != (pPack[Offset + 7] & 0x80) )
				{
				PDVD_PTS pPtsField =
					reinterpret_cast<PDVD_PTS>(pPack + Offset + 9);

				HRESULT hr = ConvertPTStoUINT(*pPtsField, pPts);
				ASSERT(SUCCEEDED(hr));

				return true;
				}
			}

		return false;
	}
};

} // namespace MSDvr

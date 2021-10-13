//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef AVISTREAM_H
#define AVISTREAM_H

#include "AVIReader.h"
#include "AVIIndex.h"

class CAVIStream
{
public:
  // Stream methods

  CAVIStream(CAVIReader* pAVIReader);

  ~CAVIStream();

  // Get the media type supported by this stream
  HRESULT GetMediaType(CMediaType *pMediaType);

  // Stream format
  BYTE* GetStrf();
  
  // Stream header
  AVISTREAMHEADER *GetStrh();

  // Currently returns 0. Do something with the index to calculate this.
  ULONG GetMaxSampleSize();

  // Construct the media types that we support.
  HRESULT BuildMT();

  // Parse this stream
  HRESULT ParseStream(RIFFLIST * pRiffList, UINT id);

  // Transform the tick into what?
  REFERENCE_TIME GetRefTime(ULONG tick);

  // Get the duration of the stream in what?
  HRESULT GetDuration(LONGLONG *pllDur);

  // Stream start: usually 0? in same units as length.
  LONGLONG GetStreamStart();

  // Length of the stream in what? Has to be munged to get duration.
  LONGLONG GetStreamLength();

  // Set the start and stop positions? Does not seem to be used after setting this.
  HRESULT RecordStartAndStop(LONGLONG *pCurrent, LONGLONG *pStop, REFTIME *pTime, const GUID *const pGuidFormat);

  HRESULT StreamingInit(
  LONGLONG *pllCurrentOut,
  ImsValues *pImsValues);

  HRESULT ReadStream(
    LONGLONG &rllCurrent,         // current time updated
    BOOL &rfQueuedSample,         // [out] queued sample?
    ImsValues *pImsValues,
    IMediaSample* pSampleOut
  );


  HRESULT InitializeIndex();

private:
  // Helper methods for stream processing
  LONGLONG ConvertToTick(const LONGLONG ll,  const TimeFormat Format);
  LONGLONG ConvertToTick(const LONGLONG ll, const GUID *const pFormat);
  LONGLONG ConvertFromTick(const LONGLONG ll,  const TimeFormat Format);
  LONGLONG ConvertFromTick(const LONGLONG ll, const GUID *const pFormat);
  LONGLONG ConvertRTToInternal(const REFERENCE_TIME rtVal);
  REFERENCE_TIME ConvertInternalToRT(const LONGLONG llVal);

  // Dont know why this is used?
  HRESULT SetAudioSubtypeAndFormat(CMediaType *pmt, BYTE *pbwfx, ULONG cbwfx);

private:
  // Back pointer to AVI Reader object
  CAVIReader* m_pAVIReader;

  // The stream number
  UINT m_id;

  // This seems to be an allocator that does not allocate buffer space in the media samples. Why?
  IMemAllocator *m_pRecAllocator;

  // The interface to the index implementation.
  IAviIndex *m_pImplIndex;

  // Used by the data generationg code, not sure what it is used for.
  ULONG m_cbAudioChunkOffset;

  // Set when there is a change in the pallete. Is this only a palette change or a new media type?
  bool m_fDeliverPaletteChange;

  // Used when delivering MPEG audio timestamps. Adjust the timestamps. Why?
  bool m_fFixMPEGAudioTimeStamps;


  // Stream header for this stream
  AVISTREAMHEADER *m_pStrh;

  // Audio format information. Why only audio?
  WAVEFORMATEX m_wfx;

  // The stream format RIFF chunk.
  RIFFCHUNK *m_pStrf;
  
  // The name of the stream - may not be present. Optional chunk.
  char *m_pStrn;

  // What is this for? meta?
  AVIMETAINDEX *m_pIndx;

  // Do we contain a DV stream? One of the following 4cc stream type/ 4cc codec - 'iavs','vids'/'dvsd','dvhd','dvh1'?
  bool m_fIsDV;

  // Media type - why?
  CMediaType m_mtFirstSample;
 
  CMediaType m_mtNextSample;

  // Why?
  // nver deliver more than this many bytes of audio. computed when
  // the file is parsed from nAvgBytesPerSecond. unset for video
  ULONG m_cbMaxAudio;

  // This is constructed from the stream name with stream id prefix.
  WCHAR* m_pName;
  
  // This is used to represent the current time format used by the stream?
  GUID m_guidFormat;
  TimeFormat m_Format;

  // The set stop and start positions. Are these respected?
  LONGLONG m_llCvtImsStart, m_llCvtImsStop;

  // To detect a discontinuity from basemsr
  LONGLONG m_llPushFirst;

  // Index state
  IxReadReq m_Irr;
  enum IrrState { IRR_NONE, IRR_REQUESTED, IRR_QUEUED };
  IrrState m_IrrState;
};


#endif // AVISTREAM_H

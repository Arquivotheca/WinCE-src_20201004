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
#ifndef AVI_READER_H
#define AVI_READER_H

#include <aviriff.h>

class CAVIReader;
class CAVIStream;
class CImplStdAviIndex;
class CImplOldAviIndex;

// This is code adapted from AVI filter (avimsr.cpp ) in the WinCE codebase. 
// The objective was to deliver a simple stand-alone AVI parser without any intelligence.

#define MAX_STREAMS 8

enum TimeFormat
{
  FORMAT_NULL,
  FORMAT_TIME,
  FORMAT_SAMPLE,
  FORMAT_FRAME
};

struct ImsValues
{
  double dRate;

  // tick values
  LONGLONG llTickStart, llTickStop;

  // values IMediaSelection or IMediaPosition sent us. used for
  // partial frames
  LONGLONG llImsStart, llImsStop;

  // Flags for the seek
  DWORD dwSeekFlags;
};

static const UINT CB_STRH_SHORT = 0x24;
static const UINT CB_STRH_NORMAL = 0x30;

class CAVIReader
{
	friend class CAVIStream;
	friend class CImplStdAviIndex;
	friend class CImplOldAviIndex;
public:
  // constructors, etc.
  CAVIReader();
  ~CAVIReader();
  HRESULT Open(const TCHAR* file);
  int GetNumStreams();
  HRESULT ReadStream(int iPosition, IMediaSample *pMediaSample);
  HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

private:
  // This loads the AVI header at the beginning of the file into a member variable.
  HRESULT LoadAVIHeader();

  // This parses the AVI header that was loaded
  HRESULT ParseHeader();

  // Are we tight interleaved? Dang! if I knew what that meant.
  inline bool IsTightInterleaved() { return m_fIsTightInterleaved; }

  // These two methods search the file for specific 4cc chunks
  HRESULT Search(
    DWORDLONG *qwPosOut,
    FOURCC fccSearchKey,
    DWORDLONG qwPosStart,
    ULONG *cb);

  HRESULT SearchList(
    DWORDLONG *qwPosOut,
    FOURCC fccSearchKey,
    DWORDLONG qwPosStart,
    ULONG *cb);

  // Something to do with indexing. I have cut all the indexing code out.
  HRESULT GetIdx1(AVIOLDINDEX **ppIdx1);

  // Where is the 'movi' chunk? This is needed to init the old 1.0 index parsers.
  HRESULT GetMoviOffset(DWORDLONG *pqw);

  // Initial frames: do these represent the skew between audio and video?
  REFERENCE_TIME GetInitialFrames();

  // Wrapper over the stream interface to do a read of a specific number of bytes
  HRESULT SyncRead(LONGLONG llPosition, LONG lLength, BYTE *pBuffer);

  // Allocates and reads data from the stream at a specific position. Used to read RIFFs.
  HRESULT AllocateAndRead (BYTE **ppb, DWORD cb, DWORDLONG qwPos);

  // Internal time format representation and translation to universal GUIDs.
  TimeFormat MapGuidToFormat(const GUID *const pGuidFormat);

private:
  // I introduced this as an encapsulation of the file stream.
  IStream *m_pStream;

  // The AVI file header. Pointer to buffer containing all of AVI 'hdrl' chunk. (allocated)
  BYTE * m_pAviHeader;

  // size of avi header data (does not include size of riff header or 'hdrl' bytes)
  UINT m_cbAviHeader;

  // The AVI file information container. Pointer to main avi header within m_pAviHeader (not allocated)
  AVIMAINHEADER * m_pAviMainHeader;

  // pointer to odml list within m_pAviHeader or 0. (not allocated)
  RIFFLIST * m_pOdmlList;

  // The cached file offset for the movi chunk. Needed by GetMoviInfo needed by the index parsers.
  DWORDLONG m_cbMoviOffset;

  // Older AVI 1.0 index. 4cc - idx1. Passed in to create the older index parser object.
  AVIOLDINDEX *m_pIdx1;

  // Tight interleaving - apparently cheap hardware - CDs do this? 2 streams and the AVIF_ISINTERLEAVED flag is set in dwFlags in the AVI main header
  bool m_fIsTightInterleaved;

  // The number of streams in this AVI.
  int m_cStreams;

  CAVIStream *m_pAVIStream[MAX_STREAMS];
};

#endif // AVI_READER_H

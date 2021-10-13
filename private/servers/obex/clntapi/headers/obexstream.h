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

#ifndef OBEXSTREAM_H
#define OBEXSTREAM_H


#define OBEX_PUT                        0
#define OBEX_GET                        1
#define OBEX_ERR                        2  //USED when error occured during PUT/GET
#define OBEX_DISCONNECTED   3

#define OBEX_AUTH_HEADER_SIZE 21  //3 header + (2+16) NONCE
#define OBEX_DEFAULT_MAX_FILE_CHUNK_SIZE        4096
extern UINT g_uiMaxFileChunk;

class CHeaderEnum; // forward declaration

class CObexStream : public IStream, public IObexResponse, public IHeaderEnum
{
public:

    CObexStream(IObexTransportConnection *pConnection, CObexDevice *pMyOBEXDevice, UINT uiMaxPacket, UINT uiConnectionId, WCHAR *wcPassword, LPHEADERCOLLECTION pHeaders);
    CObexStream(IObexTransportConnection *pConnection, CObexDevice *pMyOBEXDevice, UINT uiMaxPacket, UINT uiConnectionId, WCHAR *wcPassword, LPHEADERCOLLECTION *pHeaders);
    ~CObexStream();
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

        //
        //  IStream Interface
        //
        HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead);
        HRESULT STDMETHODCALLTYPE Write(const void *pv, ULONG cb, ULONG *pcbWritten);
        HRESULT STDMETHODCALLTYPE WriteAll(char cOpCode, const void *pv, ULONG cb, ULONG *pcbWritten);

        HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove,
                                       DWORD dwOrigin,
                                       ULARGE_INTEGER *plibNewPosition);

        HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize);
        HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm,
                                         ULARGE_INTEGER cb,
                                         ULARGE_INTEGER *pcbRead,
                                         ULARGE_INTEGER *pcbWritten);

        HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags);
        HRESULT STDMETHODCALLTYPE Revert(void);

        HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset,
                                             ULARGE_INTEGER cb,
                                             DWORD dwLockType);

        HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset,
                                               ULARGE_INTEGER cb,
                                               DWORD dwLockType);

        HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg,
                                       DWORD grfStatFlag);

        HRESULT STDMETHODCALLTYPE Clone(IStream **ppstm);

        //
        // IObexResponse Interface
        //
        HRESULT STDMETHODCALLTYPE GetLastResponseCode(BYTE *pResponseCode);

        //
        // IHeaderEnum Interface
        //
        HRESULT STDMETHODCALLTYPE Next(ULONG celt, OBEX_HEADER **rgelt, ULONG *pceltFetched);
        HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
        HRESULT STDMETHODCALLTYPE Reset(void);
        HRESULT STDMETHODCALLTYPE Clone(IHeaderEnum **ppenum);

        //
        // Misc helper functions, not exposed to client directly
        //
        HRESULT FlushSendBuffers(char opCode);

        UINT GetStreamID() {return uiStreamID;}

        BOOL IsInited() { return m_Inited; }

private:
       HRESULT ObexSendRecvWithAuthSaveHeaders(IObexTransportConnection *pConnection, UINT uiMaxPacket,
                                WCHAR *wcPassword, BYTE cOpCode, char *additionalDta,
                                UINT cAddnlDta, IHeaderCollection *pHeaderCollection,
                                unsigned char **pNewPacket, ULONG *pSize);
           HRESULT ReadHelper(void *pv, ULONG cb, ULONG *pcbRead);
           HRESULT WriteAllHeaders(BYTE cOpCode, UINT uiMINPackSize);
           HRESULT WriteAllHelper(BYTE cOpCode, const void *pv, ULONG cb, ULONG *pcbWritten);

       HRESULT CopyResponseHeaders(unsigned char *pNewPacket, ULONG Size);
       HRESULT SetupHeaderEnum(void);


       ULONG _refCount;
       IHeaderCollection *myHeaderCollection;
       CObexDevice *pMyOBEXDevice;

       char fHeaderAccounted;
       UINT uiStreamType;

       //storage space for the body information
       unsigned char *pcBodyChunk;
       UINT uiBodyLen;

       //pointer into the cBody data (used for GET's)
       unsigned char *pcBodyPtr;

       UCHAR fFirstPacket;
       UCHAR fReadFinished;

       //this flag is used only for Aboart logic()!
       BOOL fHaveSentData;

       IObexTransportConnection *pConnection;
       UINT uiMaxPacket;
       WCHAR wcPassword[MAX_PASS_SIZE];
       UINT uiConnectionId;

       UINT uiStreamID;

       BOOL StreamDisabled();

       // Stores response code in last packet sent to us by server
       BYTE           bLastResponseCode;
       // Contains obex packet headers.  On GET this will be only headers
       // from the first response packet.  On PUT this will be headers
       // returned on the last response packet.
       unsigned char *pResponseHeaders;
       unsigned int   uiResponseHeaderLen;
       CHeaderEnum   *pResponseHeadEnum;

       //If errors happen in constructor, this flag will be set as FALSE; otherwise, it's TRUE.
       BOOL m_Inited;
};

#endif

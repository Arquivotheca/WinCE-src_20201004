//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// Module Name:    isapistrm.h
// 
// Contents:
//
//    ISAPI Extension that services SOAP packages.
//
//-----------------------------------------------------------------------------

#ifndef ISAPISTRM_H_INCLUDED
#define ISAPISTRM_H_INCLUDED


enum ERROR_CODE
{
    Error_Success,                  // not an error
    Error_BadRequest,               // Baq query
    Error_AccessDenied,             // permission denied
    Error_NotFound,                 // bad server or db name or not specified
    Error_UnsupportedContentType,   // content type's other than text/xml, application/xml,
                                    // or application/x-www-form-urlencoded
    Error_UnprocessableEntity,      // XSL related errors
    Error_InternalError,            // Out of memory etc.
    Error_NotImplemeneted,
    Error_Timeout,
    Error_Last,
    Error_MethodNotAllowed,          // Method not allowed on this vdir
    Error_ServiceUnavailable,       //  SOAP Service is unavailable
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  class: CIsapiStream
//
//  description:
//      Base class that implements IStream interface 
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

class CIsapiStream : public IStream
{

public:
	ULONG	m_cRef;			// Ref count
	
    EXTENSION_CONTROL_BLOCK * m_pECB;

    CIsapiStream() { m_cRef = 1; } ;
    ~CIsapiStream() { } ;
    
    //
	// IUnknown members
	//
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);
	STDMETHODIMP			QueryInterface(REFIID riid, LPVOID *ppv);

    //
    // ISequentialStream
    //

    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
    STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);

    //
    // IStream
    //

    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(IStream **ppstm);

};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  class: CInputStream
//
//  description:
//      Implements IStream interface for reading from iis 
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

class CInputStream : public CIsapiStream
{
    ULONG   m_dwTotalRead;
public:

    CInputStream(EXTENSION_CONTROL_BLOCK *pECB) 
    {
        m_pECB          = pECB;
        m_dwTotalRead   = 0;
    }
    
    ~CInputStream() {  };
    
    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  class: COutputStream
//
//  description:
//      Implements IStream interface for writing to iis 
//          
////////////////////////////////////////////////////////////////////////////////////////////////////

// 1460 is the internal payload size for TCP/IP Internet packets. Use some number slightly smaller for buffer size
#define STRM_BUFFER_SIZE    1280
#define MAX_HEADER_LENGTH       256

// Inner helper class for COutputStream
class CSOAPIsapiResponse : public ISOAPIsapiResponse
{
	IUnknown *  m_pIUnkOuter;
	
public:	
	CAutoRg<WCHAR>  m_pwstrHTTPStatus;
	CAutoRg<WCHAR>  m_pwstrHTTPCharset;

    CSOAPIsapiResponse() : 
    	m_pIUnkOuter(NULL), m_pwstrHTTPStatus(NULL), m_pwstrHTTPCharset(NULL) { };
    
    ~CSOAPIsapiResponse() {  };
    
	void Init(IUnknown *pIUnkOuter);    
    //
	// IUnknown members
	//
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID *ppv);

    // ISOAPIsapiResponse  interface
	STDMETHOD(get_HTTPStatus)(BSTR *pbstrStatus);
	STDMETHOD(put_HTTPStatus)(BSTR bstrStatus);
	STDMETHOD(get_HTTPCharset)(BSTR *pbstrCharset);
	STDMETHOD(put_HTTPCharset)(BSTR bstrCharset);
	

};

// Declaration for buffer deletion function
void deleteOutputBuffer(void *);

class COutputStream : public IStream
{
	ULONG		m_cRef;			// Ref count
    EXTENSION_CONTROL_BLOCK * m_pECB;
    
    // Output is written into a series of linked buffers of size STRM_BUFFER_SIZE,
    //	 and then flushed at the end once the final content-length is known.
    // The first buffer is embedded for performance reasons. The soap payloads with sizes
    //	less than STRM_BUFFER_SIZE will avoid buffer allocation/deletes.
    ULONG      m_cbFirstBuffer;		// Bytes written in the first
    BYTE 		m_bFirstBuffer[STRM_BUFFER_SIZE];
    CLinkedList   m_pBuffersList;
    ULONG      m_cLinkedBuffers;	// No of buffers in linked list
    ULONG      m_cbLastBuffer;		// Bytes written in the last buffer
    BYTE *		m_pLastBuffer;		// Already in the list
    bool        m_fHeaderSent;
    bool        m_fHeadersOnly;
    ULONGLONG   m_cbFileSize;
    ERROR_CODE  m_errorcode;
    CSOAPIsapiResponse  m_SOAPIsapiResponse;
    CAutoBSTR          m_bstrErrorDescription;
    CAutoBSTR          m_bstrErrorSource;
    
public:
    COutputStream(EXTENSION_CONTROL_BLOCK *pECB) :  
    		m_pBuffersList(deleteOutputBuffer)
    {
        m_cRef = 0;
        m_pECB          = pECB;
        m_cbFirstBuffer = 0;
        m_cLinkedBuffers = 0;
        m_cbLastBuffer = 0;
        m_pLastBuffer = NULL;
        m_fHeaderSent   = FALSE;
        m_fHeadersOnly  = FALSE;
        m_cbFileSize    = 0;
        m_errorcode     = Error_Success;
        m_SOAPIsapiResponse.Init((IUnknown *)this);
    }
    
    ~COutputStream() { };
    
    STDMETHOD(FlushBuffer)(void);
    BOOL SendHeader(void);
    inline SetHeadersOnly() { m_fHeadersOnly = TRUE;}
    void  WriteFaultMessage(HRESULT hr, BSTR bstrActor);
    STDMETHOD(Write)(IStream *pIStrmIn);
    void  SetErrorCode(ERROR_CODE errCode);

    //
	// IUnknown members
	//
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG)	Release(void);
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID *ppv);
                                                     //
    // ISequentialStream
    //

    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
    STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);

    //
    // IStream
    //

    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(IStream **ppstm);

};


#endif  // ISAPISTRM_H_INCLUDED


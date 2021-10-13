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
#ifndef __SAX_CONTENT_HANDLER__
#define __SAX_CONTENT_HANDLER__

#include "msxml2.h"
#include "string.hxx"

#ifndef Chk
#define Chk(val) { hr = (val); if (FAILED(hr)) goto Cleanup; }
#endif

#ifndef ChkBool
#define ChkBool(val, x) { if (!(val)) { hr = x; goto Cleanup; } }
#endif

namespace ce
{

// SAXReader
class SAXReader
{
public:
    SAXReader(REFCLSID rclsid = CLSID_SAXXMLReader)
        : m_pReader(NULL)
    {
    }

    ~SAXReader()
    {
        if(m_pReader)
        {
            m_pReader->Release();
        }
    }
    
    HRESULT Init()
    {
        HRESULT hr = S_OK;
        
        hr = CoCreateInstance(CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER, IID_ISAXXMLReader, (void**)&m_pReader);
        
        return hr;
    }

    ISAXXMLReader * GetXMLReader()
    {
        return m_pReader; 
    }

private:
    ISAXXMLReader *m_pReader;
};

inline BOOL InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
    return (
        ((PLONG) &rguid1)[0] == ((PLONG) &rguid2)[0] &&
        ((PLONG) &rguid1)[1] == ((PLONG) &rguid2)[1] &&
        ((PLONG) &rguid1)[2] == ((PLONG) &rguid2)[2] &&
        ((PLONG) &rguid1)[3] == ((PLONG) &rguid2)[3]);
}

// SequentialStream
template <typename T>
class SequentialStream : public ISequentialStream
{
public:
    SequentialStream(T& x, int cbMaxSize = -1)
        : m_x(x),
          m_cbMaxSize(cbMaxSize)
    {
        ;
    };

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return 1; 
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        return 1; 
    }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(ce::InlineIsEqualGUID(iid, IID_IUnknown) || 
           ce::InlineIsEqualGUID(iid, IID_ISequentialStream))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }
    
// ISequentialStream
public:
    // Read
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)
    {
        assert(pcbRead);
        assert(pv);
        
        HRESULT hr = m_x.Read(pv, cb, pcbRead) ? S_OK : m_x.GetHresult();
        
        if(SUCCEEDED(hr) && m_cbMaxSize != -1)
        {
            assert(m_cbMaxSize >= 0);
            
            if(*pcbRead > (ULONG)m_cbMaxSize)
            {
                hr = E_ABORT;
            }
            else
            {
                m_cbMaxSize -= *pcbRead;
            }
            
            assert(m_cbMaxSize >= 0);
        }
        
        return hr;
    }
    
    // Write
    HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten)
    {
        return E_NOTIMPL;
    }

private:
    T&  m_x;
    int m_cbMaxSize;
};


// SequentialStream
template <>
class SequentialStream<HANDLE> : public ISequentialStream
{
public:
    SequentialStream(HANDLE h)
        : m_h(h)
    {
        ;
    };

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return 1; 
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        return 1; 
    }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(ce::InlineIsEqualGUID(iid, IID_IUnknown) || 
           ce::InlineIsEqualGUID(iid, IID_ISequentialStream))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }
    
// ISequentialStream
public:
    // Read
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)
    {
        HRESULT hr = S_OK;
        
        if (!ReadFile(m_h, pv, cb, pcbRead, NULL))
        {
            Chk(HRESULT_FROM_WIN32(GetLastError()));
        }
        
    Cleanup:        
        return hr;
    }
    
    // Write
    HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten)
    {
        return E_NOTIMPL;
    }

private:
    HANDLE m_h;
};


// SAXContentHandler
class SAXContentHandler : public ISAXContentHandler
{
public:
    SAXContentHandler()
    {
    }

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return 1; 
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        return 1; 
    }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(ce::InlineIsEqualGUID(iid, IID_IUnknown) || 
           ce::InlineIsEqualGUID(iid, IID_ISAXContentHandler))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }
    
// ISAXContentHandler
public:
    virtual HRESULT STDMETHODCALLTYPE putDocumentLocator( 
        /* [in] */ ISAXLocator __RPC_FAR *pLocator)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startDocument(void)
    {
        HRESULT hr = S_OK;
        
        ChkBool(m_strFullElementName.reserve(300), E_OUTOFMEMORY);
        ChkBool(m_strFullElementName.assign(L""), E_OUTOFMEMORY);
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endDocument( void)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startPrefixMapping( 
        /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
        /* [in] */ int cchPrefix,
        /* [in] */ const wchar_t __RPC_FAR *pwchUri,
        /* [in] */ int cchUri)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endPrefixMapping( 
        /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
        /* [in] */ int cchPrefix)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
    {
        HRESULT hr = S_OK;
        
        if(cchNamespaceUri)
        {
            ChkBool(m_strFullElementName.append(L"<"), E_OUTOFMEMORY);
            ChkBool(m_strFullElementName.append(pwchNamespaceUri, cchNamespaceUri), E_OUTOFMEMORY);
            ChkBool(m_strFullElementName.append(L">"), E_OUTOFMEMORY);
        }
        
        ChkBool(m_strFullElementName.append(L"<"), E_OUTOFMEMORY);
        ChkBool(m_strFullElementName.append(pwchLocalName, cchLocalName), E_OUTOFMEMORY);
        ChkBool(m_strFullElementName.append(L">"), E_OUTOFMEMORY);
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName)
    {
        HRESULT hr = S_OK;
        
        ChkBool(
            m_strFullElementName.resize(
                m_strFullElementName.length() - cchNamespaceUri - cchLocalName - (cchNamespaceUri ? 4 : 2)
                ),
            E_OUTOFMEMORY);
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE processingInstruction( 
        /* [in] */ const wchar_t __RPC_FAR *pwchTarget,
        /* [in] */ int cchTarget,
        /* [in] */ const wchar_t __RPC_FAR *pwchData,
        /* [in] */ int cchData)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE skippedEntity( 
        /* [in] */ const wchar_t __RPC_FAR *pwchName,
        /* [in] */ int cchName)
    {
        return S_OK;
    }

protected:
    ce::wstring m_strFullElementName;
};


// SAXContentSubset
class SAXContentSubset : public SAXContentHandler
{
public:
    SAXContentSubset(LPCWSTR pwszElement, SAXContentHandler* p)
        : m_strElement(pwszElement),
          m_pSAXContentHandler(p)
    {
    }

// ISAXContentHandler
public:
    virtual HRESULT STDMETHODCALLTYPE putDocumentLocator( 
        /* [in] */ ISAXLocator __RPC_FAR *pLocator)
    {
        HRESULT hr = S_OK;
        
        ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
        
        hr = m_pSAXContentHandler->putDocumentLocator(pLocator);
        
    Cleanup:        
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startDocument(void)
    {
        HRESULT hr = S_OK;
        
        Chk(SAXContentHandler::startDocument());
        
        m_bWithinSubset = false;
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endDocument( void)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startPrefixMapping( 
        /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
        /* [in] */ int cchPrefix,
        /* [in] */ const wchar_t __RPC_FAR *pwchUri,
        /* [in] */ int cchUri)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
            
            hr = m_pSAXContentHandler->startPrefixMapping(pwchPrefix, cchPrefix, pwchUri, cchUri);
        }
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endPrefixMapping( 
        /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
        /* [in] */ int cchPrefix)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
            
            hr = m_pSAXContentHandler->endPrefixMapping(pwchPrefix, cchPrefix);
        }
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
    {
        HRESULT hr = S_OK;
        
        ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
        
        if(m_strElement == m_strFullElementName)
        {
            m_bWithinSubset = true;
            
            Chk(m_pSAXContentHandler->startDocument()); 
        }

        if(m_bWithinSubset)
        {
            Chk(m_pSAXContentHandler->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes));
        }
        
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName)
    {
        HRESULT hr = S_OK;
        
        ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
        
        if(m_strElement == m_strFullElementName)
        {
            m_bWithinSubset = false;
            
            Chk(m_pSAXContentHandler->endDocument()); 
        }
        
        if(m_bWithinSubset)
        {
            Chk(m_pSAXContentHandler->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName));
        }

    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
        
            hr = m_pSAXContentHandler->characters(pwchChars, cchChars);
        }
        
    Cleanup:    
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
            
            hr = m_pSAXContentHandler->ignorableWhitespace(pwchChars, cchChars);
        }
    
    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE processingInstruction( 
        /* [in] */ const wchar_t __RPC_FAR *pwchTarget,
        /* [in] */ int cchTarget,
        /* [in] */ const wchar_t __RPC_FAR *pwchData,
        /* [in] */ int cchData)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
            
            hr = m_pSAXContentHandler->processingInstruction(pwchTarget, cchTarget, pwchData, cchData);
        }

    Cleanup:
        return hr;
    }
    
    virtual HRESULT STDMETHODCALLTYPE skippedEntity( 
        /* [in] */ const wchar_t __RPC_FAR *pwchName,
        /* [in] */ int cchName)
    {
        HRESULT hr = S_OK;
        
        if(m_bWithinSubset)
        {
            ChkBool(m_pSAXContentHandler, E_UNEXPECTED);
            
            hr = m_pSAXContentHandler->skippedEntity(pwchName, cchName);
        }
            
    Cleanup:
        return hr;
    }

protected:
    bool                m_bWithinSubset;
    ce::wstring         m_strElement;
    SAXContentHandler*  m_pSAXContentHandler;
};

}; // namespace ce

#endif // __SAX_CONTENT_HANDLER__

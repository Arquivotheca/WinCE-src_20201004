//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SAX_CONTENT_HANDLER__
#define __SAX_CONTENT_HANDLER__

#include "msxml2.h"
#include "string.hxx"

namespace ce
{

// SAXReader
class SAXReader
{
public:
    SAXReader(REFCLSID rclsid = CLSID_SAXXMLReader)
        : m_pReader(NULL)
        {hr = CoCreateInstance(CLSID_SAXXMLReader, NULL, CLSCTX_INPROC_SERVER, IID_ISAXXMLReader, (void**)&m_pReader); }

    ~SAXReader()
    {
        if(m_pReader)
            m_pReader->Release();
    }

    ISAXXMLReader* operator->()
        {return m_pReader; }

    bool valid()
        {return m_pReader != NULL; }

    HRESULT Error()
        {return hr; }

private:
    ISAXXMLReader*  m_pReader;
    HRESULT         hr;
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
    {};

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
        {return 1; }

    virtual ULONG STDMETHODCALLTYPE Release(void)
        {return 1; }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(InlineIsEqualGUID(iid, IID_IUnknown) || 
           InlineIsEqualGUID(iid, IID_ISequentialStream))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
            return E_NOINTERFACE;
    }
    
// ISequentialStream
public:
    // Read
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)
    {
        assert(pcbRead);
        assert(pv);
        
        HRESULT hr = m_x.Read(pv, cb, pcbRead) ? S_OK : E_FAIL;
        
        if(SUCCEEDED(hr) && m_cbMaxSize != -1)
        {
            assert(m_cbMaxSize >= 0);
            
            if(*pcbRead > (ULONG)m_cbMaxSize)
                hr = E_ABORT;
            else
                m_cbMaxSize -= *pcbRead;
            
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
    SequentialStream(T h)
        : m_h(h)
    {};

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
        {return 1; }

    virtual ULONG STDMETHODCALLTYPE Release(void)
        {return 1; }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(InlineIsEqualGUID(iid, IID_IUnknown) || 
           InlineIsEqualGUID(iid, IID_ISequentialStream))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
            return E_NOINTERFACE;
    }
    
// ISequentialStream
public:
    // Read
    HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead)
    {
        return ReadFile(m_h, pv, cb, pcbRead, NULL) ? S_OK : E_FAIL;
    }
    
    // Write
    HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten)
    {
        return E_NOTIMPL;
    }

private:
    T m_h;
};


// SAXContentHandler
class SAXContentHandler : public ISAXContentHandler
{
public:
    SAXContentHandler()
        {m_strFullElementName.reserve(300); }

// IUnknown
public:
    // fake AddRef/Release
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
        {return 1; }

    virtual ULONG STDMETHODCALLTYPE Release(void)
        {return 1; }

    // need working QI
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
    {
        if(InlineIsEqualGUID(iid, IID_IUnknown) || 
           InlineIsEqualGUID(iid, IID_ISAXContentHandler))
        {
            *ppvObject = this;

            return S_OK;
        }
        else
            return E_NOINTERFACE;
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
        m_strFullElementName = L"";

        return S_OK;
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
        if(cchNamespaceUri)
        {
            m_strFullElementName.append(L"<");
            m_strFullElementName.append(pwchNamespaceUri, cchNamespaceUri);
            m_strFullElementName.append(L">");
        }
        m_strFullElementName.append(L"<");
        m_strFullElementName.append(pwchLocalName, cchLocalName);
        m_strFullElementName.append(L">");

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName)
    {
        m_strFullElementName.resize(m_strFullElementName.length() - cchNamespaceUri - cchLocalName - (cchNamespaceUri ? 4 : 2));

        return S_OK;
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
        {};

// ISAXContentHandler
public:
    virtual HRESULT STDMETHODCALLTYPE putDocumentLocator( 
        /* [in] */ ISAXLocator __RPC_FAR *pLocator)
    {
        m_pSAXContentHandler->putDocumentLocator(pLocator);

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startDocument(void)
    {
        SAXContentHandler::startDocument();

        m_bWithinSubset = false;

        return S_OK;
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
        if(m_bWithinSubset)
            return m_pSAXContentHandler->startPrefixMapping(pwchPrefix, cchPrefix, pwchUri, cchUri);
        
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endPrefixMapping( 
        /* [in] */ const wchar_t __RPC_FAR *pwchPrefix,
        /* [in] */ int cchPrefix)
    {
        if(m_bWithinSubset)
            return m_pSAXContentHandler->endPrefixMapping(pwchPrefix, cchPrefix);
        
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
        if(m_strElement == m_strFullElementName)
        {
            m_bWithinSubset = true;
            m_pSAXContentHandler->startDocument(); 
        }

        if(m_bWithinSubset)
            return m_pSAXContentHandler->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName)
    {
        if(m_strElement == m_strFullElementName)
        {
            m_bWithinSubset = false;
            m_pSAXContentHandler->endDocument(); 
        }
        
        if(m_bWithinSubset)
            return m_pSAXContentHandler->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        if(m_bWithinSubset)
            return m_pSAXContentHandler->characters(pwchChars, cchChars);
        
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars)
    {
        if(m_bWithinSubset)
            return m_pSAXContentHandler->ignorableWhitespace(pwchChars, cchChars);
            
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE processingInstruction( 
        /* [in] */ const wchar_t __RPC_FAR *pwchTarget,
        /* [in] */ int cchTarget,
        /* [in] */ const wchar_t __RPC_FAR *pwchData,
        /* [in] */ int cchData)
    {
        if(m_bWithinSubset)
            return m_pSAXContentHandler->processingInstruction(pwchTarget, cchTarget, pwchData, cchData);

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE skippedEntity( 
        /* [in] */ const wchar_t __RPC_FAR *pwchName,
        /* [in] */ int cchName)
    {
        if(m_bWithinSubset)
            return m_pSAXContentHandler->skippedEntity(pwchName, cchName);
            
        return S_OK;
    }

protected:
    bool                m_bWithinSubset;
    ce::wstring         m_strElement;
    SAXContentHandler*  m_pSAXContentHandler;
};

}; // namespace ce

#endif // __SAX_CONTENT_HANDLER__

//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SAX_WRITER__
#define __SAX_WRITER__

#include "msxml2.h"
#include "string.hxx"

namespace ce
{


// XMLOutputHelper
class XMLOutputHelper
{
public:
	XMLOutputHelper(UINT cp)
		: m_cp(cp),
		  m_pchBuff(NULL),
		  m_nBuff(0),
		  m_hFile(INVALID_HANDLE_VALUE)
	{}

	~XMLOutputHelper()
	{
		delete[] m_pchBuff;

		CloseHandle(m_hFile);
	}

	bool open(LPCWSTR pszFile);
	void close();

	void write(const wchar_t* pwch, int cch = -1);
	void write(const wchar_t wch);
	void writeString(const wchar_t* pwch, int cch = -1);
	void pcdataText(const wchar_t* pwch, int cch = -1);
	
private:
	char*	m_pchBuff;
	int		m_nBuff;
	UINT	m_cp;
	HANDLE	m_hFile;
};


// SAXWriter
class SAXWriter : public ISAXContentHandler
{
public:
    SAXWriter(UINT cp, bool bOmitXMLDeclaration = false)
		: m_bOmitXMLDeclaration(bOmitXMLDeclaration),
		  m_out(cp)
    {
	};

	bool Open(LPCWSTR pwszFile)
	{
		return m_out.open(pwszFile);
	}

// IUnknown
public:
    // fake AddRef/Release/QueryInterface
    virtual ULONG STDMETHODCALLTYPE AddRef(void)
        {return 1; }

    virtual ULONG STDMETHODCALLTYPE Release(void)
        {return 1; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
		{return S_OK; }
    
// ISAXContentHandler
public:
    virtual HRESULT STDMETHODCALLTYPE putDocumentLocator( 
        /* [in] */ ISAXLocator __RPC_FAR *pLocator)
    {
        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);
    
    virtual HRESULT STDMETHODCALLTYPE endDocument(void);
    
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
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);
    
    virtual HRESULT STDMETHODCALLTYPE ignorableWhitespace( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);
    
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
	XMLOutputHelper	m_out;
	bool			m_bOmitXMLDeclaration;
};

}; // namespace ce

#endif // __SAX_WRITER__

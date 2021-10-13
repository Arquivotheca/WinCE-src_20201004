//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "Windows.h"
#include "SAXWriter.h"
#include "assert.h"

#define CHK_BOOL(b, err) if(!(b))return err;

namespace ce
{

// startDocument
HRESULT STDMETHODCALLTYPE 
SAXWriter::startDocument( void)
{
    HRESULT hr = S_OK;
    
	if (!m_bOmitXMLDeclaration) 
        m_out.write(L"<?xml version=\"1.0\"?>\r\n");

    return hr;
}


// endDocument
HRESULT STDMETHODCALLTYPE 
SAXWriter::endDocument(void)
{
    HRESULT hr = S_OK;
    
	m_out.close();

    return hr;
}


// startElement
HRESULT STDMETHODCALLTYPE 
SAXWriter::startElement(
    /* [in] */ const WCHAR __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const WCHAR __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const WCHAR __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    HRESULT         hr = S_OK;
    int             nLength;
    const WCHAR*	pwchAttQName;
    int             cchAttQName;
    const WCHAR*	pwchAttValue;
    int             cchAttValue;
    int             i;

    CHK_BOOL(pwchNamespaceUri, E_INVALIDARG);
    CHK_BOOL(pwchLocalName, E_INVALIDARG);
    CHK_BOOL(pwchQName, E_INVALIDARG);

	m_out.write(L"<");
	m_out.write(pwchQName, cchQName);

    if (pAttributes)
	{
        pAttributes->getLength( &nLength);

        for (i=0; nLength--; i++)
        {      
            m_out.write(L' ');
            
			pAttributes->getQName(i, &pwchAttQName, &cchAttQName);
            m_out.write(pwchAttQName, cchAttQName);
        
            pAttributes->getValue(i, &pwchAttValue, &cchAttValue);
            m_out.write(L'=');
            
			m_out.write(L'"');
			m_out.writeString(pwchAttValue, cchAttValue);
			m_out.write(L'"');
        }
    }

	m_out.write(L">");
	
    return hr;
}


// endElement   
HRESULT STDMETHODCALLTYPE 
SAXWriter::endElement(
    /* [in] */ const WCHAR __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const WCHAR __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const WCHAR __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    HRESULT hr = S_OK;

    CHK_BOOL(pwchNamespaceUri, E_INVALIDARG);
    CHK_BOOL(pwchLocalName, E_INVALIDARG);
    CHK_BOOL(pwchQName, E_INVALIDARG);

	m_out.write(L'<');
	m_out.write(L'/');
	m_out.write(pwchQName, cchQName);
	m_out.write(L'>');

    return hr;
}


// characters
HRESULT STDMETHODCALLTYPE 
SAXWriter::characters(
    /* [in] */ const WCHAR __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    HRESULT hr = S_OK;

    CHK_BOOL(pwchChars, E_INVALIDARG);

    m_out.pcdataText(pwchChars, cchChars);

    return hr;
}


// ignorableWhitespace
HRESULT STDMETHODCALLTYPE 
SAXWriter::ignorableWhitespace(
    /* [in] */ const WCHAR __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    HRESULT hr = S_OK;
    
    CHK_BOOL(pwchChars, E_INVALIDARG);

    m_out.write(pwchChars, cchChars);

    return hr;
}

////////////////////////////////////////////////////////////
// XMLOutputHelper


// open
bool 
XMLOutputHelper::open(LPCWSTR pwszFile)
{
	m_hFile = ::CreateFile(pwszFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	return m_hFile != INVALID_HANDLE_VALUE;
}


// close
void
XMLOutputHelper::close()
{
	CloseHandle(m_hFile);

	m_hFile = INVALID_HANDLE_VALUE;
}


// write
void 
XMLOutputHelper::write(const wchar_t* pwch, int cch/* = -1*/)
{
	// calculate length if needed
	if(cch == -1)
		cch = wcslen(pwch);

	// calculate size of buffer for mbcs string
	int nSize = ::WideCharToMultiByte(m_cp, 0, pwch, cch, NULL, 0, NULL, NULL);

    if(!nSize)
        if(nSize = ::WideCharToMultiByte(CP_ACP, 0, pwch, cch, NULL, 0, NULL, NULL))
            m_cp = CP_ACP;

	// realloc the buffer if needed
	if(m_nBuff < nSize)
	{
		delete[] m_pchBuff;
		m_pchBuff = new char[m_nBuff = nSize];
	}

	// convert to mbcs
	nSize = ::WideCharToMultiByte(m_cp, 0, pwch, cch, m_pchBuff, m_nBuff, NULL, NULL);

	// write to file
	DWORD dw;

	WriteFile(m_hFile, m_pchBuff, nSize, &dw, NULL);
}


// write
void 
XMLOutputHelper::write(const wchar_t wch)
{
	write(&wch, 1);
}


// pcdataText
void
XMLOutputHelper::pcdataText(const wchar_t* wszText, int len)
{
	assert(len >= 0);

	while (len--)
    {
        int ch = *wszText++;
        if (ch <= L'>')
        {
            switch (ch)
            {
            case L'&':
                write(L"&amp;");
                continue;

            case L'<':
                write(L"&lt;");
                continue;

            case L'>':
                write(L"&gt;");
                continue;

            case 0xd:
                write(0xd);
                write(0xa);
                if (len && *wszText == 0xa)
                {
                    // If 0xa follows 0xd, then suppress 0xa
                    len--;
                    wszText++;
                }
                continue;

            case 0xa:
                write(0xd);
                write(0xa);
                continue;
            }
        }

        write((WCHAR)ch);
    }
}


// writeString
void
XMLOutputHelper::writeString(const wchar_t* c, int len)
{
    assert(len >= 0);
    
    while (len--)
    {
        int ch = *c++;
        if (ch <= '>')
        {
            switch (ch)
            {
            case L'"': 
                write(L"&quot;");
                continue;

            case L'&':
                write(L"&amp;");
                continue;

            case L'<':
                write(L"&lt;");
                continue;

            case L'>':
                write(L"&gt;");
                continue;

            case 0xd:
                write(0xd);
                write(0xa);
                if (len && *c == 0xa)
                {
                    // If 0xa follows 0xd, then suppress 0xa
                    len--;
                    c++;
                }
                continue;

            case 0xa:
                write(0xd);
                write(0xa);
                continue;
            }
        }

        write((TCHAR)ch);
    }
}

}; // namespace ce

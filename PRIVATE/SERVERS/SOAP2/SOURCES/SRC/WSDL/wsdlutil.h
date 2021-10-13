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
// File:    wsdlutil.h
// 
// Contents:
//
//  Header File 
//
//		function prototypes for some useful helpers
//	
//	Created 
//
//-----------------------------------------------------------------------------
#ifndef __WSDLRUTIL_H_INCLUDED__
#define __WSDLRUTIL_H_INCLUDED__

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

const int _MAX_ATTRIBUTE_LEN = 255;
const int _MAX_BUFFER_SIZE = 2048;

const int c_saveIDRefs = 0x1; 

HRESULT _WSDLUtilGetNodeText(IXMLDOMNode *pNode, BSTR *pbstrText);
HRESULT _WSDLUtilSplitQName(TCHAR *pchStringToStrip, TCHAR *pchPrefix, BSTR *pbstrName);

HRESULT _WSDLUtilGetStyle(IXMLDOMNode *pNode, BOOL *pIsDocuement);
HRESULT _WSDLUtilGetStyleString(BSTR *pbstrStyle, BOOL bValue);

HRESULT _WSDLUtilFindExtensionInfo(IXMLDOMNode *pNode, BSTR *pbstrEncoding, BOOL *pbCreateHrefs);
HRESULT _WSDLUtilFindDocumentation(IXMLDOMNode *pNode, BSTR *pbstrDocumentation);
HRESULT _WSDLUtilGetRootNodeFromReader(ISoapReader *pReader, IXMLDOMNode **pNode);

HRESULT _WSDLUtilReturnAutomationBSTR(BSTR *pbstrOut, const TCHAR *pchIn);
HRESULT _WSDLUtilFindFirstChild(IXMLDOMNode *pNode, IXMLDOMNode **ppChild);


//
// the following template class is used to store Ixxxx pointers
// and is used for the EnumInterfaces
//
template<class TYPE>
class CList
{
public:
// Construction
	CList();
	~CList();

// Attributes
	HRESULT Size(long *plSize) const;
	HRESULT	Add(TYPE toAdd);
	HRESULT	AddOrdered(TYPE toAdd, long lKey);	
	HRESULT Next(long lToFetch, TYPE *pReturn, long *plFetched);
	HRESULT Reset(void);
	HRESULT Skip(long lToSkip);
	HRESULT Find(WCHAR *pchElementToFind, TYPE *pReturn);
	HRESULT getNext(TYPE *pReturn, DWORD *pdwCookie);
    HRESULT Clone(CList<TYPE> &p);

protected:
	class CNode
	{
		public:
			CNode() 
			{
				pNext = 0; 
				pData = 0; 
			}
			CNode* pNext;
			TYPE  pData;
			long    lKey;
	};

	CNode * m_pHead;
	CNode * m_pCurrent; 
	long	m_lSize;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CList<TYPE>::CList()
//
//  parameters:
//		
//  description:
//		
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
CList<TYPE>::CList()
{
	m_pHead = 0;
	m_pCurrent = 0; 
	m_lSize = 0; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CList * CList<TYPE>::Clone()
//
//  parameters:
//		
//  description:
//		
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Clone(CList<TYPE> &p)
{
    HRESULT hr = S_OK; 
    CNode *pTemp;
    pTemp = m_pHead;

    while (pTemp)
    {
        CHK(p.Add(pTemp->pData));
    	pTemp = pTemp->pNext;
    }
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CList<TYPE>::~CList()
//
//  parameters:
//		
//  description:
//		
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
CList<TYPE>::~CList()
{
	CNode *pTemp;
	CNode *pKill; 

	pTemp = m_pHead;

	while (pTemp)
	{
		pKill = pTemp;
		pTemp = pTemp->pNext;
		pKill->pData->Release();
		delete pKill; 
	}
	m_pHead = 0; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Add(TYPE toAdd)
//
//  parameters:
//		
//  description:
//		adds an item to end of the list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Add(TYPE toAdd)
{
	HRESULT hr = E_FAIL;
	CNode * pNew = new CNode();
	CNode * pTemp=0; 

	if (!pNew)
	{
		hr = E_OUTOFMEMORY;
		goto Cleanup;
	}

	pNew->pData = toAdd;
	toAdd->AddRef();
    
#ifndef UNDER_CE
	if (!m_pHead)
#else
	if(NULL == (pTemp = m_pHead))
#endif 
	{
		m_pHead = pNew;
	}
	else
	{
#ifndef UNDER_CE
        pTemp = m_pHead;
#endif 
	    while (pTemp && pTemp->pNext)
	    {
	    	pTemp = pTemp->pNext;
	    }
       	pNew->pNext = pTemp->pNext;
        pTemp->pNext = pNew;
	}
	// everytime the list has changed, reset the current
	m_pCurrent = m_pHead;
	hr = S_OK;
	m_lSize++;


Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
	
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::AddOrdered(TYPE toAdd)
//
//  parameters:
//		
//  description:
//		adds an item to the list, using the order number
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::AddOrdered(TYPE toAdd, long lKey)
{
	HRESULT hr = E_FAIL;
	CNode * pNew = new CNode();
	CNode * pTemp=0; 
	CNode * pPrev=0;

	if (!pNew)
	{
		hr = E_OUTOFMEMORY;
		goto Cleanup;
	}

	pNew->pData = toAdd;
	toAdd->AddRef();
	pNew->lKey = lKey;
    
	if (!m_pHead)
	{
		m_pHead = pNew;
	}
	else
	{
        pTemp = m_pHead;
        pPrev = m_pHead;
	    while (pTemp && pTemp->pNext && pTemp->lKey < lKey)
	    {
	        pPrev = pTemp;
	    	pTemp = pTemp->pNext;
	    }
	    if (pTemp->lKey < lKey)
	    {
	        // add at end
           	pNew->pNext = pTemp->pNext;
            pTemp->pNext = pNew;
	    }
	    else
	    {
	        pNew->pNext = pTemp;
	        if (pTemp == m_pHead)
	        {
	            m_pHead = pNew; 
	        }
	        else
	        {
	            pPrev->pNext = pNew;
	        }
	    }
	}
	// everytime the list has changed, reset the current
	m_pCurrent = m_pHead;
	hr = S_OK;
	m_lSize++;


Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
	
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Size(long *plSize) const
//
//  parameters:
//		
//  description:
//		returns the size of the list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Size(long *plSize) const
{
	*plSize = m_lSize;
	return(S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::getNext(TYPE *pReturn, DWORD *pdwCookie)
//
//  parameters:
//		
//  description:
//		finds the next item in the list, returns the pointer as a COOKIE
//		if cookie is 0, returns first
//  returns: 
//		S_OK while there are elements left
//		S_FALSE when done
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::getNext(TYPE *pReturn, DWORD *pdwCookie)
{
	HRESULT hr = S_FALSE;
	CNode    *pTemp;

	if (*pdwCookie==0)
	{
		*pdwCookie = (DWORD)m_pHead;
		if (m_pHead)
		{
    		assign(pReturn, m_pHead->pData);
		}
	}
	else
	{
		pTemp = (CNode*) *pdwCookie;
		if (pTemp && pTemp->pNext)
		{
			pTemp = pTemp->pNext;
			assign(pReturn, pTemp->pData);
			*pdwCookie = (DWORD) pTemp;
		}
		else
		{
			*pdwCookie = 0;
		}
	}

	if (*pdwCookie)
	{
		hr = S_OK;
	}
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Reset(void)
//
//  parameters:
//		
//  description:
//		returns the size of the list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Reset(void)
{
	m_pCurrent = m_pHead;
	return(S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Skip(long lToSkip)
//
//  parameters:
//		
//  description:
//		returns the size of the list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Skip(long lToSkip)
{
	HRESULT hr = S_FALSE;

	while (lToSkip > 0 && m_pCurrent && m_pCurrent->pNext)
	{
		m_pCurrent = m_pCurrent->pNext;
		lToSkip--;
	}
	if (lToSkip==0)
		hr = S_OK;
	else
		m_pCurrent =0;

	return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Next(long lToFetch, TYPE *pReturn, long *plFetched)
//
//  parameters:
//		
//  description:
//		returns the size of the list
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Next(long lToFetch, TYPE *pReturn, long *plFetched)
{
	HRESULT hr = S_FALSE;

	ASSERT(pReturn!=0);
	ASSERT(lToFetch>0);

	*plFetched = 0; 
	*pReturn = 0;

	if (lToFetch < 1)
	{
		return(E_INVALIDARG);
	}

	if (m_pCurrent)
	{
		assign(pReturn, m_pCurrent->pData);
		m_pCurrent=m_pCurrent->pNext;
		*plFetched = 1; 
		hr = S_OK; 
	}

	ASSERT(SUCCEEDED(hr));

	return(hr);
	

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CList<TYPE>::Find(WCHAR *pchElementToFind, TYPE *pReturn)
//
//  parameters:
//		
//  description:
//		returns the found element. does not modify pCurrent
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class TYPE>
HRESULT CList<TYPE>::Find(WCHAR *pchElementToFind, TYPE *pReturn)
{
	HRESULT hr = E_INVALIDARG;
	CNode 	*pTemp=m_pHead; 
	BSTR	bstr;


	if (!pchElementToFind || !pReturn)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

	*pReturn = 0; 
	while (pTemp)
	{
		if (FAILED(pTemp->pData->get_name(&bstr))) 
		{
			goto Cleanup;
		}
		if (wcscmp(bstr, pchElementToFind)==0)
		{
			assign(pReturn, pTemp->pData);
			hr = S_OK;
			goto Cleanup;
		}
		free_bstr(bstr);
		pTemp=pTemp->pNext;

	}

Cleanup:
	free_bstr(bstr);
	ASSERT(SUCCEEDED(hr));
	return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






#endif

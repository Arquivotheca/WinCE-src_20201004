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
//+----------------------------------------------------------------------------
//
// 
// File:    ensoapmp.cpp
// 
// Contents:
//
//  implementation file 
//
//		IEnumSoapMapper Interface implemenation
//	
//	Created 
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "ensoapmp.h"
#include "soapmapr.h"


#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


BEGIN_INTERFACE_MAP(CEnumSoapMappers)
	ADD_IUNKNOWN(CEnumSoapMappers, IEnumSoapMappers)
	ADD_INTERFACE(CEnumSoapMappers, IEnumSoapMappers)
END_INTERFACE_MAP(CEnumSoapMappers)




/////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEnumSoapMappers::CEnumSoapMappers()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CEnumSoapMappers::CEnumSoapMappers()
{
	m_bRetVal = false;
	m_bCheckedForRetVal = false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CEnumSoapMappers::~CEnumSoapMappers()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CEnumSoapMappers::~CEnumSoapMappers()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Next(long celt, ISoapMapper **ppSoapMapper, long *pulFetched)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Next(long celt, ISoapMapper **ppSoapMapper, long *pulFetched)
{
	return(m_soapMapperList.Next(celt, ppSoapMapper, pulFetched));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Skip(long celt)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Skip(long celt)
{
	return(m_soapMapperList.Skip(celt));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Reset(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Reset(void)
{
	return(m_soapMapperList.Reset());
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Clone(IEnumSoapMappers **ppenum)
//
//  parameters:
//
//  description:
//
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Clone(IEnumSoapMappers **ppenum)
{
    HRESULT hr = S_OK; 
    CEnumSoapMappers *pMapper; 
    pMapper = new CSoapObject<CEnumSoapMappers>(INITIAL_REFERENCE); 
    CHK_BOOL(pMapper, E_OUTOFMEMORY);
    CHK(pMapper->Copy(this));
    pMapper->Reset();
    *ppenum = pMapper; 
Cleanup:    
    return (S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Copy(CEnumSoapMappers *pOrg)
//
//  parameters:
//
//  description:
//
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Copy(CEnumSoapMappers *pOrg)
{
    HRESULT hr = S_OK;
    CHK(pOrg->m_soapMapperList.Clone(m_soapMapperList));
    m_bRetVal = pOrg->m_bRetVal;
    m_bCheckedForRetVal = pOrg->m_bCheckedForRetVal;
Cleanup:    
    return(hr);
}    
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::FindParameter(long lparaOrder, ISoapMapper **ppSoapMapper)
//
//  parameters:
//		lparaOrder -> dispatch number of parameter
//		ppSoapMapper -> return mapper, might be 0, if we just want to check if the ordernumber is there
//  description:
//		
//
//  returns: 
//		S_OK: mapper found
//		E_FAIL: not found
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::FindParameter(long lparaOrder, ISoapMapper **ppSoapMapper)
{
	DWORD                  dwCookie = 0; 
	CAutoRefc<ISoapMapper>	pMapper;
	HRESULT             	hr=E_FAIL; 
	long					lOrder;

    if (ppSoapMapper)
    {
    	*ppSoapMapper = 0; 
    }

	while (m_soapMapperList.getNext(&pMapper, &dwCookie)==S_OK)
	{
		pMapper->get_parameterOrder(&lOrder);
		if (lOrder == lparaOrder)
		{
			// that's the guy
			hr = S_OK;
			if (ppSoapMapper)
			{
    			assign(ppSoapMapper,(ISoapMapper*)pMapper);
			}
			goto Cleanup;
		}
		pMapper.Clear();
	}
	
Cleanup:
	return (hr);
	
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::parameterCount(long *plArgCount)
//
//  parameters:
//		plArgcount -> out, number of parameters to message call
//  description:
//		
//
//  returns: 
//		S_OK: 
//		E_INVALIDARG
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::parameterCount(long *plArgCount)
{
	DWORD                  dwCookie = 0; 
	CAutoRefc<ISoapMapper>	pMapper;
	long					lOrder;

	if (!plArgCount)
		return (E_INVALIDARG);

	*plArgCount = 0; 

	while (m_soapMapperList.getNext(&pMapper, &dwCookie)==S_OK)
	{
		pMapper->get_parameterOrder(&lOrder);
		if (lOrder >= 0)
		{
			(*plArgCount)++;
		}
		pMapper.Clear();
	}
	return (S_OK);	

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::getNext(ISoapMapper **ppSoapMapper, DWORD *pdwCookie)
//
//  parameters:
//		ppSOapMapper -> out, returned mapper
//      	pdwCookie       -> in/out position in list, 0 is initial value for start of list
//  description:
//		
//
//  returns: 
//		S_OK: more available
//      S_FALSE: end of list
//		
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::getNext(ISoapMapper **ppSoapMapper, DWORD *pdwCookie)
{
    return m_soapMapperList.getNext(ppSoapMapper, pdwCookie);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Add(ISoapMapper *pISoapMapper)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Add(ISoapMapper *pISoapMapper)
{
	return(m_soapMapperList.Add(pISoapMapper));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////			


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::AddOrdered(ISoapMapper *pISoapMapper)
//
//  parameters:
//
//  description:
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::AddOrdered(ISoapMapper *pISoapMapper)
{
    long lOrder;
    pISoapMapper->get_parameterOrder(&lOrder);
	return(m_soapMapperList.AddOrdered(pISoapMapper, lOrder));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////			


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::Size(long *pulSize)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::Size(long *pulSize)
{
	return(m_soapMapperList.Size(pulSize));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CEnumSoapMappers::CountOfArguments(long *pulArgCount)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CEnumSoapMappers::CountOfArguments(long *pulArgCount)
{
	HRESULT hr = S_OK;
	LONG	lRes;
	long 	lCallIndex;

	ISoapMapper	*pSoapMapper=0; 

	if (!pulArgCount)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

	if (!m_bCheckedForRetVal)
	{
		// we need to go over the list and find if there is a retval
		m_bCheckedForRetVal = true;
		Reset();
		while (Next(1, &pSoapMapper, &lRes)==S_OK)
		{
			hr = pSoapMapper->get_callIndex(&lCallIndex);
			if (FAILED(hr)) 
			{
				goto Cleanup;
			}
			if (lCallIndex == -1)
			{
				m_bRetVal = true; 
			}
			release(&pSoapMapper);
		}
		Reset();
	}
	hr = m_soapMapperList.Size(&lRes);
	if (FAILED(hr)) 
	{
		goto Cleanup;
	}
	lRes -= m_bRetVal ? 1 : 0 ; 

	*pulArgCount = lRes; 

Cleanup:
	release(&pSoapMapper);
	return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

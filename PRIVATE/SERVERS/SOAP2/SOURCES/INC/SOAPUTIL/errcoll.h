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
// File:
//      errcoll.h
//
// Contents:
//
//      ErrorCollection object defintion
//
//-----------------------------------------------------------------------------

#ifndef __ERRCOLL_H_INCLUDED__
#define __ERRCOLL_H_INCLUDED__


class CErrorDescription  :  public CDynArrayEntry
{

public:
    CErrorDescription()
    {
        m_pchDescription = 0;
        m_pchActor = 0 ;
        m_pchComponent = 0;
        m_hrErrorCode = 0;
        m_bFaultEntry = 0;
        m_pchFaultString = 0;
    }
    ~CErrorDescription()
    {
        delete [] m_pchDescription;
        delete [] m_pchActor;
        delete [] m_pchComponent;
        delete [] m_pchFaultString;
    }

    TCHAR * GetDescription(void);
    TCHAR * GetActor(void);
    TCHAR * GetComponent(void);
    TCHAR * GetFaultString(void);
    HRESULT GetErrorCode(void);
    BOOL    IsFaultInfo(void);

    HRESULT Init(TCHAR *pchDescription, TCHAR *pchComponent, TCHAR *pchActor, TCHAR *pchFaultString, HRESULT hrErrorCode);

private:
    TCHAR * m_pchDescription;
    TCHAR * m_pchActor;
    TCHAR * m_pchComponent;
    TCHAR * m_pchFaultString;
    bool     m_bFaultEntry;
    HRESULT m_hrErrorCode;
};



class CErrorListEntry :  public CDynArrayEntry
{
public:

    CErrorListEntry(DWORD dwCookie )
	{
        m_parrErrors = NULL;
        m_dwCookie = dwCookie;
        m_fGotErrorInfo = false;
        m_fGotSoapError = false;
    };

	~CErrorListEntry()
	{
        if (m_parrErrors)
        {
            m_parrErrors->Shutdown();
        }
        delete m_parrErrors;
	};

    UINT Size(void)
    {
        return(m_parrErrors->Size());

    }

    HRESULT SetValue(CTypedDynArray<CErrorDescription>* pArr)
    {
        if (m_parrErrors)
        {
            m_parrErrors->Shutdown();
            delete m_parrErrors;
        }
        m_parrErrors = pArr;
        return S_OK;
    }

    DWORD GetKey(void)
    {
        return(m_dwCookie);
    }

    HRESULT AddEntry(CErrorDescription *pNewEntry)
    {
        return m_parrErrors->Add(pNewEntry);
    }

    CErrorDescription * GetAt(UINT uiPos)
    {
        return(m_parrErrors->GetAt(uiPos));
    }

    HRESULT getFaultCode(BSTR *pbstrFaultCode);
    HRESULT getFaultActor(BSTR *pbstrActor);
    HRESULT getFaultDetailBSTR(BSTR *pbstrDetail);
    HRESULT getFaultString(BSTR *pbstrFaultString);
    HRESULT setFaultActor(BSTR bstrActor);
    HRESULT getFaultNamespace(BSTR *pbstrNamespace);

    HRESULT SetErrorInfo(BSTR bstrDescription, BSTR bstrSource, BSTR bstrHelpFile, DWORD dwHelpContext, HRESULT hrFromException);
    HRESULT GetErrorInfo(BSTR *pbstrDescription, BSTR *pbstrSource, BSTR *pbstrHelpFile, DWORD *pdwHelpContext, HRESULT *phrFromException);
    HRESULT SetSoapError(BSTR bstrFaultString, BSTR bstrFaultActor, BSTR bstrDetail, BSTR bstrFaultCode, BSTR bstrNameSpace);
    HRESULT GetSoapError(BSTR *pbstrFaultString, BSTR *pbstrFaultActor, BSTR *pbstrDetail, BSTR *pbstrFaultCode, BSTR *pbstrNameSpace);
    HRESULT GetReturnCode(HRESULT *phrReturnCode);
    void     Reset(void);
    BOOL    isISoapError(void) { return m_fGotSoapError; };
        


private:
    DWORD           m_dwCookie;
    CTypedDynArray<CErrorDescription> *m_parrErrors;
    CAutoBSTR       m_bstrActor;
    CAutoBSTR       m_bstrHelpFile;
    CAutoBSTR       m_bstrDescription;
    CAutoBSTR       m_bstrSource;
    DWORD          m_dwHelpContext;
    HRESULT         m_hrFromException;
    BOOL            m_fGotErrorInfo;
    BOOL            m_fGotSoapError; 

    // those here hold the ISoapError information
    CAutoBSTR       m_bstrFaultString;
    CAutoBSTR       m_bstrFaultActor;
    CAutoBSTR       m_bstrFaultDetail;
    CAutoBSTR       m_bstrFaultCode;
    CAutoBSTR       m_bstrFaultNamespace; 
};



class CErrorCollection
{
public:
    HRESULT AddErrorEntry(TCHAR *pchDescription, TCHAR *pchComponent, TCHAR *pchActor, TCHAR *pchFaultString, HRESULT hrErrorCode);
    HRESULT AddErrorFromErrorInfo(LPEXCEPINFO pExecpInfo, HRESULT hrErrorCode);
    HRESULT AddErrorFromSoapError(IDispatch * pDispatch);
    HRESULT GetErrorEntry(CErrorListEntry **ppErrorList);
    HRESULT setActor(BSTR bstrActor);
    void    Reset(void);
    void    Shutdown(void);

private:
    CErrorListEntry * getCurrentList(void);
    CKeyedDoubleList<CErrorListEntry, CTypedDynArray<CErrorDescription>*>		m_errorList;
};



// void globalAddError(UINT idsResource, UINT idsComponent, HRESULT hr);
void globalAddError(UINT idsResource, UINT idsComponent, HRESULT hr, ...);


void globalDeleteErrorList(void);
void globalGetError(CErrorListEntry **ppErrorList);
void globalResetErrors(void);
void globalErrorSetActor(TCHAR *pchActor);
void globalAddErrorFromErrorInfo(EXCEPINFO * pexepInfo, HRESULT hrErrorCode);
void globalAddErrorFromISoapError(IDispatch *pDispatch, HRESULT hrErrorCode);
HRESULT globalSetActor(WCHAR * bstrCurActor);



#define ERROR_ONHR0( HR, A, B )          { if (FAILED(HR)) { globalAddError(A, B, HR); CHK(HR); } }
#define ERROR_ONHR1( HR, A, B, T1 )      { if (FAILED(HR)) { globalAddError(A, B, HR, T1); CHK(HR); } }
#define ERROR_ONHR2( HR, A, B, T1, T2 )  { if (FAILED(HR)) { globalAddError(A, B, HR, T1, T2); CHK(HR); } }

#endif //__ERRCOLL_H_INCLUDED__

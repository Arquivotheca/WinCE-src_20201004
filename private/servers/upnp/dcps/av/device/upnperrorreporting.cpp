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

#include "av_upnp.h"

using namespace av_upnp;

/////////////////////////////////////////////////////////////////////////////
// UPnPAVErrorReporting

DWORD UPnPErrorReporting::AddErrorDescription(int UPnPErrorNum, const wstring &UPnPErrorDescription)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    if(m_mapErrDescrips.find(UPnPErrorNum) != m_mapErrDescrips.end())
    {
        return ERROR_AV_INVALID_INSTANCE;
    }
    
    if(m_mapErrDescrips.insert(UPnPErrorNum, UPnPErrorDescription) == m_mapErrDescrips.end())
    {
        return ERROR_AV_OOM;
    }

    return SUCCESS_AV;
}


DWORD UPnPErrorReporting::RemoveErrorDescription(int UPnPErrorNum)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    ErrDescripMap::iterator it = m_mapErrDescrips.find(UPnPErrorNum);
    if(it == m_mapErrDescrips.end())
    {
        return ERROR_AV_INVALID_INSTANCE;
    }

    m_mapErrDescrips.erase(it);

    return SUCCESS_AV;
}


DWORD UPnPErrorReporting::AddToolkitError(int UPnPToolkitErrorNum, int UPnPErrorNum)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    if(m_mapToolkitErrs.find(UPnPToolkitErrorNum) != m_mapToolkitErrs.end())
    {
        return ERROR_AV_INVALID_INSTANCE;
    }

    if(m_mapToolkitErrs.insert(UPnPToolkitErrorNum, UPnPErrorNum) == m_mapToolkitErrs.end())
    {
        return ERROR_AV_OOM;
    }

    return SUCCESS_AV;
}


DWORD UPnPErrorReporting::RemoveToolkitError(int UPnPToolkitErrorNum)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    ToolkitErrMap::iterator it = m_mapToolkitErrs.find(UPnPToolkitErrorNum);
    if(it == m_mapToolkitErrs.end())
    {
        return ERROR_AV_INVALID_INSTANCE;
    }

    m_mapToolkitErrs.erase(it);

    return SUCCESS_AV;
}

#ifdef DEBUG
HRESULT UPnPErrorReporting::ReportActionError(int UPnPErrorNum, LPCWSTR pszAction)
#else
HRESULT UPnPErrorReporting::ReportActionError(int UPnPErrorNum)
#endif
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    if(SUCCESS_AV == UPnPErrorNum)
    {
        return S_OK;
    }

    wstring strUPnPErrorDescription;

    if(UPnPErrorNum < 0)
    {
        // If not a UPnP error, attempt to look up as an internal toolkit error and use the resultant UPnP error,
        // if the lookup fails, return a generic UPnP ACTION_FAILED
        const ToolkitErrMap::const_iterator itErrMap = m_mapToolkitErrs.find(UPnPErrorNum);
        
        if(itErrMap != m_mapToolkitErrs.end())
        {
            UPnPErrorNum = itErrMap->second;
        }
        else
        {
            UPnPErrorNum = ERROR_AV_UPNP_ACTION_FAILED;
        }
    }

    const ErrDescripMap::const_iterator itErrMap = m_mapErrDescrips.find(UPnPErrorNum);
    
    if(itErrMap != m_mapErrDescrips.end())
    {
        strUPnPErrorDescription = itErrMap->second;
    }
    else
    {
        // no description provided for this error
        assert(false);
    }

    DEBUGMSG(ZONE_AV_WARN, (AV_TEXT("Action \"%s\" returned error (%d) \"%s\""), pszAction, UPnPErrorNum, static_cast<LPCWSTR>(strUPnPErrorDescription)));
    
    HRESULT                     hr;
    CComPtr<ICreateErrorInfo>   pcerrinfo;

    hr = CreateErrorInfo(&pcerrinfo);
    
    if(SUCCEEDED(hr))
    {
        CComQIPtr<IErrorInfo, &IID_IErrorInfo> perrinfo(pcerrinfo);

        if(perrinfo)
        {
            // Convert arguments

            const int nitotMax = 33;
            TCHAR pszErrNum[nitotMax];
            _itot(UPnPErrorNum, pszErrNum, 10);

            ce::auto_bstr bstrErrNum = SysAllocString(pszErrNum);
            if(!bstrErrNum.valid())
            {
                return E_OUTOFMEMORY;
            }
            ce::auto_bstr bstrErrDesc = SysAllocString(strUPnPErrorDescription);
            if(!bstrErrDesc.valid())
            {
                return E_OUTOFMEMORY;
            }


            // Set the error number and description

            if(FAILED(pcerrinfo->SetSource(bstrErrNum)))
            {
                return E_OUTOFMEMORY;
            }
            if(FAILED(pcerrinfo->SetDescription(bstrErrDesc)))
            {
                return E_OUTOFMEMORY;
            }
            
            SetErrorInfo(0, perrinfo);


            hr = DISP_E_EXCEPTION; // No COM errors occured, set hr to indicate a UPnP error
        }
        else
            return E_NOINTERFACE;
    }

    return hr;
}

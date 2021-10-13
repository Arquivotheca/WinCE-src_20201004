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

#include "av_dcp.h"
#include "av_upnp.h"

using namespace av_upnp;

/////////////////////////////////////////////////////////////////////////////
// ContentDirectoryServiceImpl

namespace av_upnp
{
namespace details
{
    const DWORD g_nCDMinModeratedEventSleepPeriodms = 2000; // as specified by CDS's spec, the min time between moderated events
}
}


ContentDirectoryServiceImpl::ContentDirectoryServiceImpl()
    : m_pContentDirectory(NULL),
      ModeratedEventSupport(details::g_nCDMinModeratedEventSleepPeriodms),
      m_bSystemUpdateIDChanged(false)
{
    InitErrDescrips();
    InitToolkitErrs();
    InitDISPIDs();
}


ContentDirectoryServiceImpl::~ContentDirectoryServiceImpl()
{
    if(m_pContentDirectory)
    {
        const DWORD retUnadvise = m_pContentDirectory->Unadvise(this);
        // if Unadvise() fails, we can do nothing but ignore.
    }
}


DWORD ContentDirectoryServiceImpl::Init(/* [in] */ IContentDirectory* pIContentDirectory)
{
    if(m_pContentDirectory)
    {
        return ERROR_AV_ALREADY_INITED;
    }

    if(!pIContentDirectory)
    {
        return ERROR_AV_POINTER;
    }


    m_pContentDirectory = pIContentDirectory;


    const DWORD retAdvise = m_pContentDirectory->Advise(this);
    if(SUCCESS_AV != retAdvise)
    {
        return retAdvise;
    }

    return SUCCESS_AV;
}



//
// IEventSink
//

DWORD ContentDirectoryServiceImpl::OnStateChanged(
                                    /*[in]*/ LPCWSTR pszStateVariableName,
                                    /*[in*/  LPCWSTR pszValue)
{
    if(!pszStateVariableName || !pszValue)
    {
        return ERROR_AV_POINTER;
    }

    DWORD ret = SUCCESS_AV;

    ce::gate<ce::critical_section> csEventsLock(m_csEvents);

    if(0 == wcscmp(ContentDirectoryState::SystemUpdateID, pszStateVariableName))
    {
        // This is a moderated event
        // Mark that this moderated variable has been updated and store its value for get_ to retrieve later on

        if(!m_bSystemUpdateIDChanged)
        {
            m_bSystemUpdateIDChanged = true;

            // no updates -> an update exists, notify TimedEventCaller
            AddRefModeratedEvent();
        }
    }
    else if(0 == wcscmp(ContentDirectoryState::ContainerUpdateIDs, pszStateVariableName))
    {
        // This is a moderated event
        // Mark that this moderated variable has been updated and store its value for get_ to retrieve later on

        wstring             strContainerID, strUpdateID;
        wstring::size_type  n;
        
        strContainerID = pszValue;
        
        if(wstring::npos == (n = strContainerID.find(L",")))
        {
            return ERROR_AV_INVALID_STATEVAR;
        }
        
        strContainerID.resize(n);
        strUpdateID = pszValue + n + 1;
        
        if(strContainerID.empty())
        {
            return ERROR_AV_INVALID_STATEVAR;
        }
        
        if(strUpdateID.empty())
        {
            return ERROR_AV_INVALID_STATEVAR;
        }
        
        if(wstring::npos != strUpdateID.find(L","))
        {
            return ERROR_AV_INVALID_STATEVAR;
        }

        ContainerUpdateIDsMap::iterator it;
        
        it = m_mapContainerUpdateIDs.find(strContainerID);
        
        if(m_mapContainerUpdateIDs.end() == it)
        {
            if(m_mapContainerUpdateIDs.end() == m_mapContainerUpdateIDs.insert(strContainerID, strUpdateID))
            {
                return ERROR_AV_OOM;
            }
            
            if(1 == m_mapContainerUpdateIDs.size())
            {
                // no updates -> an update exists, notify TimedEventCaller
                AddRefModeratedEvent();
            }
        }
        else
        {
            it->second = strUpdateID;
        }
    }
    else
    {
        if(0 == wcscmp(ContentDirectoryState::TransferIDs, pszStateVariableName))
        {
            // This is a non-moderated event that CDSImpl implements, store its new value for get_ to retrieve
            // (and pass on to IEventSinkSupport like all non-moderated events)

            m_strTransferIDs = pszValue;
        }

        // This is a non-moderated event, pass on to IEventSinkSupport to notify subscriber

        ret = IEventSinkSupport::OnStateChanged(pszStateVariableName, pszValue);
    }


    return ret;
}

        
DWORD ContentDirectoryServiceImpl::OnStateChanged(
                                    /*[in]*/ LPCWSTR pszStateVariableName,
                                    /*[in*/  long nValue)
{
    if(!pszStateVariableName)
    {
        return ERROR_AV_POINTER;
    }


    // Convert nValue from a long to a LPCWSTR and pass on to the string version of OnStateChanged()

    wstring   strValue;
    const int nltowLimit = 33;
    
    strValue.reserve(nltowLimit);

    _ltow(nValue, strValue.get_buffer(), 10);

    return OnStateChanged(pszStateVariableName, strValue);
}


DWORD ContentDirectoryServiceImpl::OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue)
{
    // This overload of OnStateChanged is only for RenderingControl use
    return ERROR_AV_UPNP_ACTION_FAILED;
}



//
// TimedEventCallee
//

void ContentDirectoryServiceImpl::TimedEventCall()
{
    // If a state variable has been updated:
    // Send update to IEventSinkSupport, reset the updated flag, and remove this var's Ref from TimedEventCaller.
    // Ignore return values, we can't do anything in response here.

    ce::gate<ce::critical_section> csEventsLock(m_csEvents);

    if(m_bSystemUpdateIDChanged)
    {
        IEventSinkSupport::OnStateChanged(ContentDirectoryState::SystemUpdateID, (LPCWSTR)NULL);

        m_bSystemUpdateIDChanged = false;
        ReleaseModeratedEvent();
    }

    if(!m_mapContainerUpdateIDs.empty())
    {
        IEventSinkSupport::OnStateChanged(ContentDirectoryState::ContainerUpdateIDs, (LPCWSTR)NULL);

        m_mapContainerUpdateIDs.clear();
        ReleaseModeratedEvent();
    }
}



//
// IUPnPService_ContentDirectory1
//

STDMETHODIMP ContentDirectoryServiceImpl::get_TransferIDs(BSTR* pTransferIDs)
{
    if(!pTransferIDs)
    {
        return E_POINTER;
    }

   
    ce::gate<ce::critical_section> csEventsLock(m_csEvents);

    *pTransferIDs = SysAllocString(m_strTransferIDs);
    
    if(!*pTransferIDs)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    return S_OK;
}


// get_A_ARG_TYPE_foo() methods are implemented only to satisfy upnp's devicehost, they are not used

STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_ObjectID(BSTR* pA_ARG_TYPE_ObjectID)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_Result(BSTR* pA_ARG_TYPE_Result)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_SearchCriteria(BSTR* pA_ARG_TYPE_SearchCriteria)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_BrowseFlag(BSTR* pA_ARG_TYPE_BrowseFlag)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_Filter(BSTR* pA_ARG_TYPE_Filter)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_SortCriteria(BSTR* pA_ARG_TYPE_SortCriteria)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_Index(unsigned long* pA_ARG_TYPE_Index)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_Count(unsigned long* pA_ARG_TYPE_Count)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_UpdateID(unsigned long* pA_ARG_TYPE_UpdateID)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_TransferID(unsigned long* pA_ARG_TYPE_TransferID)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_TransferStatus(BSTR* pA_ARG_TYPE_TransferStatus)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_TransferLength(BSTR* pA_ARG_TYPE_TransferLength)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_TransferTotal(BSTR* pA_ARG_TYPE_TransferTotal)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_TagValueList(BSTR* pA_ARG_TYPE_TagValueList)
{
    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_A_ARG_TYPE_URI(BSTR* pA_ARG_TYPE_URI)
{
    return S_OK;
}



STDMETHODIMP ContentDirectoryServiceImpl::get_SearchCapabilities(BSTR* pSearchCapabilities)
{
    if(!pSearchCapabilities)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments for call to IContentDirectory
    wstring strSearchCapabilities;

    // Make the call
    const DWORD retPFC = m_pContentDirectory->GetSearchCapabilities(&strSearchCapabilities);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }

    // Set out arg
    *pSearchCapabilities = SysAllocString(strSearchCapabilities);
    if(!*pSearchCapabilities)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_SortCapabilities(BSTR* pSortCapabilities)
{
    if(!pSortCapabilities)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments for call to IContentDirectory
    wstring strSortCapabilities;

    // Make the call
    const DWORD retPFC = m_pContentDirectory->GetSortCapabilities(&strSortCapabilities);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }

    // Set out arg
    *pSortCapabilities = SysAllocString(strSortCapabilities);
    if(!*pSortCapabilities)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_SystemUpdateID(unsigned long* pSystemUpdateID)
{
    if(!pSystemUpdateID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }

    const DWORD retPFC = m_pContentDirectory->GetSystemUpdateID(pSystemUpdateID);
    
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::get_ContainerUpdateIDs(BSTR* pContainerUpdateIDs)
{
    // Though "ContentDirectory:1 Service Template Version 1.01", p18, s2.5.21 states ctrl points should use this variable
    // only for eventing, we implement it to allow ce's upnp to get the value when we do an IUPnPEventSink->OnStateChanged(),
    // and do not disallow ctrl points getting if they wish
    
    ce::gate<ce::critical_section> csEventsLock(m_csEvents);

    wstring strValue;
    
    for(ContainerUpdateIDsMap::iterator it = m_mapContainerUpdateIDs.begin(), itEnd = m_mapContainerUpdateIDs.end(); it != itEnd; ++it)
    {
        if(!strValue.empty())
        {
            strValue += AVDCPListDelimiter;
        }
        
        strValue += it->first;
        strValue += AVDCPListDelimiter;
        strValue += it->second;
    }
    
    *pContainerUpdateIDs = SysAllocString(strValue);
    
    if(!*pContainerUpdateIDs)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    return S_OK;
}



STDMETHODIMP ContentDirectoryServiceImpl::GetSearchCapabilities(BSTR* pSearchCaps)
{
    return get_SearchCapabilities(pSearchCaps);
}


STDMETHODIMP ContentDirectoryServiceImpl::GetSortCapabilities(BSTR* pSortCaps)
{
    return get_SortCapabilities(pSortCaps);
}


STDMETHODIMP ContentDirectoryServiceImpl::GetSystemUpdateID(unsigned long* pId)
{
    return get_SystemUpdateID(pId);
}


STDMETHODIMP ContentDirectoryServiceImpl::Browse(BSTR           ObjectID,
                                                 BSTR           BrowseFlag,
                                                 BSTR           Filter,
                                                 unsigned long  StartingIndex,
                                                 unsigned long  RequestedCount,
                                                 BSTR           SortCriteria,
                                                 BSTR*          pResult,
                                                 unsigned long* pNumberReturned,
                                                 unsigned long* pTotalMatches,
                                                 unsigned long* pUpdateID)
{
    if(!pResult || !pNumberReturned || !pTotalMatches || !pUpdateID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments common to both ICD::Browse calls for call to IContentDirectory
    wstring strObjectID, strBrowseFlag, strFilter;
    wstring strResult;
    
    if(   !strObjectID.assign(ObjectID,     SysStringLen(ObjectID))
       || !strBrowseFlag.assign(BrowseFlag, SysStringLen(BrowseFlag))
       || !strFilter.assign(Filter,         SysStringLen(Filter))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Decide whether to call BrowseMetadata or BrowseChildren, setup function-specific args, and call

    LPCWSTR pszBrowseFlag_Metadata       = L"BrowseMetadata",
            pszBrowseFlag_DirectChildren = L"BrowseDirectChildren";

    if(strBrowseFlag == pszBrowseFlag_Metadata)
    {
        // Make the call
        const DWORD retPFC = m_pContentDirectory->BrowseMetadata(strObjectID, strFilter, &strResult, pUpdateID);
        if(SUCCESS_AV != retPFC)
        {
            return m_ErrReport.ReportError(retPFC);
        }

        // Set NumberReturned and TotalMatches to one, "ContentDirectory:1 Service Template Version 1.01", p23, table 2.7.4.2
        *pNumberReturned = 1;
        *pTotalMatches   = 1;
    }
    else if(strBrowseFlag == pszBrowseFlag_DirectChildren)
    {
        // Setup argument
        wstring strSortCriteria;
        if(!strSortCriteria.assign(SortCriteria, SysStringLen(SortCriteria)))
        {
            return m_ErrReport.ReportError(ERROR_AV_OOM);
        }
 

        // Make the call
        const DWORD retPFC = m_pContentDirectory->BrowseChildren(strObjectID,
                                                                 strFilter,
                                                                 StartingIndex,
                                                                 RequestedCount,
                                                                 strSortCriteria,
                                                                 &strResult,
                                                                 pNumberReturned,
                                                                 pTotalMatches,
                                                                 pUpdateID);
        if(SUCCESS_AV != retPFC)
        {
            return m_ErrReport.ReportError(retPFC);
        }
    }
    else
    {
        // Invalid BrowseFlag
        return m_ErrReport.ReportError(ERROR_AV_UPNP_CD_REQUEST_FAILED);
    }

    
    // Set out args
    *pResult = SysAllocString(strResult);
    if(!*pResult)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::Search(BSTR          ContainerID,
                                                 BSTR          SearchCriteria,
                                                 BSTR          Filter,
                                                 unsigned long StartingIndex,
                                                 unsigned long RequestedCount,
                                                 BSTR          SortCriteria,
                                                 BSTR*          pResult,
                                                 unsigned long* pNumberReturned,
                                                 unsigned long* pTotalMatches,
                                                 unsigned long* pUpdateID)
{
    if(!pResult || !pNumberReturned || !pTotalMatches || !pUpdateID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments for call to IContentDirectory
    wstring strContainerID, strSearchCriteria, strFilter, strSortCriteria;
    wstring strResult;

    if(   !strContainerID.assign(ContainerID,       SysStringLen(ContainerID))
       || !strSearchCriteria.assign(SearchCriteria, SysStringLen(SearchCriteria))
       || !strFilter.assign(Filter,                 SysStringLen(Filter))
       || !strSortCriteria.assign(SortCriteria,     SysStringLen(SortCriteria))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->Search(strContainerID,
                                                     strSearchCriteria,
                                                     strFilter,
                                                     StartingIndex,
                                                     RequestedCount,
                                                     strSortCriteria,
                                                     &strResult,
                                                     pNumberReturned,
                                                     pTotalMatches,
                                                     pUpdateID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    // Set out arg
    *pResult = SysAllocString(strResult);
    if(!*pResult)
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::CreateObject(BSTR ContainerID,
                                                       BSTR Elements,
                                                       BSTR* pObjectID,
                                                       BSTR* pResult)
{
    if(!pObjectID || !pResult)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments for call to IContentDirectory
    wstring strContainerID, strElements;
    wstring strObjectID, strResult;

    if(   !strContainerID.assign(ContainerID, SysStringLen(ContainerID))
       || !strElements.assign(Elements, SysStringLen(Elements))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->CreateObject(strContainerID, strElements, &strObjectID, &strResult);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    // Set out args
    if(   !(*pObjectID = SysAllocString(strObjectID))
        ||!(*pResult   = SysAllocString(strResult))   ) 
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::DestroyObject(BSTR ObjectID)
{
    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments to call to IContentDirectory
    wstring strObjectID;
    if(!strObjectID.assign(ObjectID, SysStringLen(ObjectID)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->DestroyObject(strObjectID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }
    

    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::UpdateObject(BSTR ObjectID,
                                                       BSTR CurrentTagValue,
                                                       BSTR NewTagValue)
{
    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments to call to IContentDirectory
    wstring strstrObjectID, strCurrentTagValue, strNewTagValue;
    if(   !strstrObjectID.assign(ObjectID,            SysStringLen(ObjectID))
       || !strCurrentTagValue.assign(CurrentTagValue, SysStringLen(CurrentTagValue))
       || !strNewTagValue.assign(NewTagValue,         SysStringLen(NewTagValue))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->UpdateObject(strstrObjectID, strCurrentTagValue, strNewTagValue);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::ImportResource(BSTR SourceURI,
                                                         BSTR DestinationURI,
                                                         unsigned long* pTransferID)
{
    if(!pTransferID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments to call to IContentDirectory
    wstring strSourceURI, strDestinationURI;
    if(   !strSourceURI.assign(SourceURI,           SysStringLen(SourceURI))
       || !strDestinationURI.assign(DestinationURI, SysStringLen(DestinationURI))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->ImportResource(strSourceURI, strDestinationURI, pTransferID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::ExportResource(BSTR SourceURI,
                                                         BSTR DestinationURI,
                                                         unsigned long* pTransferID)
{
    if(!pTransferID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments to call to IContentDirectory
    wstring strSourceURI, strDestinationURI;
    if(   !strSourceURI.assign(SourceURI,           SysStringLen(SourceURI))
       || !strDestinationURI.assign(DestinationURI, SysStringLen(DestinationURI))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->ExportResource(strSourceURI, strDestinationURI, pTransferID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::StopTransferResource(unsigned long TransferID)
{
    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->StopTransferResource(TransferID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::GetTransferProgress(unsigned long TransferID,
                                                              BSTR* pTransferStatus,
                                                              BSTR* pTransferLength,
                                                              BSTR* pTransferTotal)
{
    if(!pTransferStatus || !pTransferLength || !pTransferTotal)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments for call to IContentDirectory
    wstring strTransferStatus, strTransferLength, strTransferTotal;

    // Make the call
    const DWORD retPFC = m_pContentDirectory->GetTransferProgress(TransferID, &strTransferStatus, &strTransferLength, &strTransferTotal);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }

    // Set out args
    if(   !(*pTransferStatus = SysAllocString(strTransferStatus))
       || !(*pTransferLength = SysAllocString(strTransferLength))
       || !(*pTransferTotal  = SysAllocString(strTransferTotal))   )
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::DeleteResource(BSTR ResourceURI)
{
    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup arguments to call to IContentDirectory
    wstring strResourceURI;
    if(!strResourceURI.assign(ResourceURI, SysStringLen(ResourceURI)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->DeleteResource(strResourceURI);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }


    return S_OK;
}


STDMETHODIMP ContentDirectoryServiceImpl::CreateReference(BSTR ContainerID,
                                                          BSTR ObjectID,
                                                          BSTR* pNewID)
{
    if(!pNewID)
    {
        return E_POINTER;
    }

    if(!m_pContentDirectory)
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    // Setup argument for call to IContentDirectory
    wstring strContainerID, strObjectID;
    wstring strNewID;

    if(   !strContainerID.assign(ContainerID,   SysStringLen(ContainerID))
       || !strObjectID.assign(ObjectID,         SysStringLen(ObjectID))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    // Make the call
    const DWORD retPFC = m_pContentDirectory->CreateReference(strContainerID, strObjectID, &strNewID);
    if(SUCCESS_AV != retPFC)
    {
        return m_ErrReport.ReportError(retPFC);
    }

    // Set out arg
    *pNewID = SysAllocString(strNewID);
    if(!*pNewID)
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}


// InvokeVendorAction
DWORD ContentDirectoryServiceImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return m_pContentDirectory->InvokeVendorAction(pszActionName, pdispparams, pvarResult);
}



//
// Private
//

HRESULT ContentDirectoryServiceImpl::InitToolkitErrs()
{
    const int vErrMap[][2] =
    {
        {ERROR_AV_POINTER,          ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_OOM,              ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_INVALID_INSTANCE, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_INVALID_STATEVAR, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_ALREADY_INITED,   ERROR_AV_UPNP_ACTION_FAILED},
        {0,                         0} // 0 is used to denote the end of this array
    };


    for(unsigned int i=0; 0 != vErrMap[i][0]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddToolkitError(vErrMap[i][0], vErrMap[i][1]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}


HRESULT ContentDirectoryServiceImpl::InitErrDescrips()
{
    const int vErrNums[] =
    {
        // UPnP
        ERROR_AV_UPNP_INVALID_ACTION,
        ERROR_AV_UPNP_ACTION_FAILED,
        // ContentDirectory
        ERROR_AV_UPNP_CD_NO_SUCH_OBJECT,
        ERROR_AV_UPNP_CD_INVALID_CURRENTTAGVALUE,
        ERROR_AV_UPNP_CD_INVALID_NEWTAGVALUE,
        ERROR_AV_UPNP_CD_REQUIRED_TAG_DELETE,
        ERROR_AV_UPNP_CD_READONLY_TAG_UPDATE,
        ERROR_AV_UPNP_CD_PARAMETER_NUM_MISMATCH,
        /* there does not exist an error 707 (ContentDirectory:1 Service Template Version 1.01) */
        ERROR_AV_UPNP_CD_BAD_SEARCH_CRITERIA,
        ERROR_AV_UPNP_CD_BAD_SORT_CRITERIA,
        ERROR_AV_UPNP_CD_NO_SUCH_CONTAINER,
        ERROR_AV_UPNP_CD_RESTRICTED_OBJECT,
        ERROR_AV_UPNP_CD_BAD_METADATA,
        ERROR_AV_UPNP_CD_RESTRICTED_PARENT_OBJECT,
        ERROR_AV_UPNP_CD_NO_SUCH_SOURCE_RESOURCE,
        ERROR_AV_UPNP_CD_SOURCE_RESOURCE_ACCESS_DENIED,
        ERROR_AV_UPNP_CD_TRANSFER_BUSY,
        ERROR_AV_UPNP_CD_NO_SUCH_FILE_TRANSFER,
        ERROR_AV_UPNP_CD_NO_SUCH_DESTINATION_RESOURCE,
        ERROR_AV_UPNP_CD_DESTINATION_RESOURCE_ACCESS_DENIED,
        ERROR_AV_UPNP_CD_REQUEST_FAILED,
        0 // 0 is used to denote the end of this array
    };
    
    const wstring vErrDescrips[] =
    {
        // UPnP
        L"Invalid Action",
        L"Action Failed",
        // ContentDirectory
        L"No such object",
        L"Invalid CurrentTagValue",
        L"Invalid NewTagValue",
        L"Required tag",
        L"Read only tag",
        L"Parameter Mismatch",
        /* there does not exist an error 707 (ContentDirectory:1 Service Template Version 1.01) */
        L"Unsupported or invalid search criteria",
        L"Unsupported or invalid sort cirteria",
        L"No such container",
        L"Restricted object",
        L"Bad metadata",
        L"Restricted parent object",
        L"No such source resource",
        L"Source resource access denied",
        L"Transfer busy",
        L"No such file transfer",
        L"No such destination resource",
        L"Destination resource access denied",
        L"Cannot process the request"
    };


    for(unsigned int i=0; 0 != vErrNums[i]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddErrorDescription(vErrNums[i], vErrDescrips[i]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}


void ContentDirectoryServiceImpl::InitDISPIDs()
{
    // Evented variables for ContentDirectory
    LPCWSTR vpszStateVarNames[] =
    {
        ContentDirectoryState::TransferIDs,
        ContentDirectoryState::SystemUpdateID,
        ContentDirectoryState::ContainerUpdateIDs,
        0 // 0 is used to denote the end of this array
    };

    const DISPID vdispidStateVarDISPIDs[] =
    {
        DISPID_TRANSFERIDS,
        DISPID_SYSTEMUPDATEID,
        DISPID_CONTAINERUPDATEIDS
    };


    for(unsigned int i=0; 0 != vpszStateVarNames[i]; ++i)
    {
        if(m_mapDISPIDs.end() == m_mapDISPIDs.insert(vpszStateVarNames[i], vdispidStateVarDISPIDs[i]))
        {
            return;
        }
    }
}

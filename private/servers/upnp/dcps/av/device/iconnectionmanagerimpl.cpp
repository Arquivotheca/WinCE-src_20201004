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
// Non-member internal functions
//
// These functions could be static const private member functions, but since they don't depend on data members of
// IConnectionManagerImpl they are made non-members to simplify this class's declaration.

namespace av_upnp {
    namespace details {
        typedef ce::hash_set<wstring> ProtocolInfoSet;

        DWORD generic_AddProtocol(   LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet);
        DWORD generic_RemoveProtocol(LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet);
        DWORD generic_get_ProtocolInfo(wstring* pstrProtocolInfo, const ProtocolInfoSet &protocolInfoSet);
    }
}



/////////////////////////////////////////////////////////////////////////////
// IConnectionManagerImpl

IConnectionManagerImpl::IConnectionManagerImpl()
    : m_pSubscriber(NULL)
{
}


IConnectionManagerImpl::~IConnectionManagerImpl()
{
    assert(m_pSubscriber == NULL);
}


//
// IEventSource
//

DWORD IConnectionManagerImpl::Advise(/*[in]*/ av::IEventSink *pSubscriber)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);
    
    m_pSubscriber = pSubscriber;

    wstring strSourceProtocolInfo, strSinkProtocolInfo;

    GetProtocolInfo( &strSourceProtocolInfo, &strSinkProtocolInfo );

    if(m_pSubscriber)
    {
        m_pSubscriber->OnStateChanged(ConnectionManagerState::SinkProtocolInfo, (LPCWSTR)strSinkProtocolInfo);
        m_pSubscriber->OnStateChanged(ConnectionManagerState::SourceProtocolInfo, (LPCWSTR)strSourceProtocolInfo);
        m_pSubscriber->OnStateChanged(ConnectionManagerState::CurrentConnectionIDs, (LPCWSTR)(L"0"));
    }
    return av::SUCCESS_AV;}


DWORD IConnectionManagerImpl::Unadvise(/*[in]*/ av::IEventSink *pSubscriber)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);
    
    m_pSubscriber = NULL;

    return av::SUCCESS_AV;
}


DWORD IConnectionManagerImpl::AddSinkProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retAdd = details::generic_AddProtocol(pszProtocolInfo, &m_setSinkProtocols);
    
    if(SUCCESS_AV != retAdd)
    {
        return retAdd;
    }

    if(m_pSubscriber)
    {
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SinkProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
    }
    
    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::RemoveSinkProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retRem = details::generic_RemoveProtocol(pszProtocolInfo, &m_setSinkProtocols);
    if(SUCCESS_AV != retRem)
        return retRem;

    if(m_pSubscriber)
    {
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SinkProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
    }
            
    return SUCCESS_AV;
}
 

DWORD IConnectionManagerImpl::AddSourceProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retAdd = details::generic_AddProtocol(pszProtocolInfo, &m_setSourceProtocols);
    if(SUCCESS_AV != retAdd)
    {
        return retAdd;
    }

    if(m_pSubscriber)
    {
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SourceProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
    }

    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::RemoveSourceProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retRem = details::generic_RemoveProtocol(pszProtocolInfo, &m_setSourceProtocols);
    if(SUCCESS_AV != retRem)
    {
        return retRem;
    }

    if(m_pSubscriber)
    {
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SourceProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
    }

    return SUCCESS_AV;
}



//
// IConnectionManager
//

DWORD IConnectionManagerImpl::PrepareForConnection(
            /* [in] */ LPCWSTR pszRemoteProtocolInfo,
            /* [in] */ LPCWSTR pszPeerConnectionManager,
            /* [in] */ long PeerConnectionID,
            /* [in] */ DIRECTION Direction,
            /* [in, out] */ long* pConnectionID,
            /* [in, out] */ IAVTransport** ppAVTransport,
            /* [in, out] */ IRenderingControl** ppRenderingControl)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IConnectionManagerImpl::ConnectionComplete(
        /* [in] */ long ConnectionID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IConnectionManagerImpl::GetFirstConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    if(!pConnectionID)
    {
        return ERROR_AV_POINTER;
    }

    *pConnectionID = 0;
    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::GetNextConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    return ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE; // no remaining connections
}


DWORD IConnectionManagerImpl::GetProtocolInfo(
    /* [in, out] */ wstring* pstrSourceProtocolInfo,
    /* [in, out] */ wstring* pstrSinkProtocolInfo)
{
    if(!pstrSourceProtocolInfo || !pstrSinkProtocolInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);
    DWORD ret;

    ret = details::generic_get_ProtocolInfo(pstrSourceProtocolInfo, m_setSourceProtocols);
    if(SUCCESS_AV != ret)
    {
        return ret;
    }

    ret = details::generic_get_ProtocolInfo(pstrSinkProtocolInfo,   m_setSinkProtocols);
    if(SUCCESS_AV != ret)
    {
        return ret;
    }

    return SUCCESS_AV;
}


#define  CONNECTION_ID_UNKNOWN   -1
#define  CONNECTION_NAME_UNKNOWN  L""
#define  CONNECTION_STATUS_OK     L"OK"

DWORD IConnectionManagerImpl::GetCurrentConnectionInfo(
        /* [in] */ long ConnectionID,
        /* [in, out] */ ConnectionInfo* pConnectionInfo)
{
    // Check parameter
    if(!pConnectionInfo)
    {
        return ERROR_AV_POINTER;
    }

    // Fill in the connection info
    pConnectionInfo->pRenderingControl = NULL;
    pConnectionInfo->pAVTransport = NULL;
    pConnectionInfo->strRemoteProtocolInfo = CONNECTION_NAME_UNKNOWN;
    pConnectionInfo->strPeerConnectionManager = CONNECTION_NAME_UNKNOWN;
    pConnectionInfo->nPeerConnectionID  = CONNECTION_ID_UNKNOWN;
    pConnectionInfo->Direction          = INPUT;
    pConnectionInfo->strStatus          = CONNECTION_STATUS_OK;
    return SUCCESS_AV;
}


// InvokeVendorAction
DWORD IConnectionManagerImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}



//
// Private
//


//
// Non-member internal functions
//

namespace av_upnp {
    namespace details {

        DWORD details::generic_AddProtocol(LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet)
        {
            if(!pszProtocolInfo || !pProtocolInfoSet)
            {
                return ERROR_AV_POINTER;
            }

            wstring strProtocolInfo(pszProtocolInfo);
            if(FAILED(EscapeAVDCPListDelimiters(&strProtocolInfo)))
            {
                return ERROR_AV_OOM;
            }

            if(pProtocolInfoSet->end() == pProtocolInfoSet->insert(strProtocolInfo))
            {
                return ERROR_AV_OOM;
            }

            return SUCCESS_AV;
        }


        DWORD details::generic_RemoveProtocol(LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet)
        {
            if(!pszProtocolInfo || !pProtocolInfoSet)
            {
                return ERROR_AV_POINTER;
            }

            wstring strProtocolInfo(pszProtocolInfo);
            if(FAILED(EscapeAVDCPListDelimiters(&strProtocolInfo)))
            {
                return ERROR_AV_OOM;
            }

            pProtocolInfoSet->erase(strProtocolInfo);

            return SUCCESS_AV;
        }


        DWORD details::generic_get_ProtocolInfo(wstring* pstrProtocolInfo, const ProtocolInfoSet &protocolInfoSet)
        {
            if(!pstrProtocolInfo)
            {
                return ERROR_AV_POINTER;
            }

            // Build AV DCP list of protocols
            bool firstItem = true;
            const ProtocolInfoSet::const_iterator itEnd = protocolInfoSet.end();

            for(ProtocolInfoSet::const_iterator it = protocolInfoSet.begin(); it != itEnd; ++it)
            {
                //first run through the list and append all DLNA.ORG ProtocolInfo
                if(wcsstr(*it,L"DLNA.ORG"))
                {
                    if(!firstItem)
                    {                
                        if(!pstrProtocolInfo->append(AVDCPListDelimiter))
                        {
                            return ERROR_AV_OOM;
                        }
                    }
                    else
                    {
                        firstItem = false;
                    }
                       
                    if(!pstrProtocolInfo->append(*it))
                    {
                        return ERROR_AV_OOM;
                    }
                }
            }

            for(ProtocolInfoSet::const_iterator it = protocolInfoSet.begin(); it != itEnd; ++it)
            {
                //next run through the list and append all other ProtocolInfo
                if(!wcsstr(*it,L"DLNA.ORG"))
                {
                    if(!firstItem)
                    {                
                        if(!pstrProtocolInfo->append(AVDCPListDelimiter))
                        {
                            return ERROR_AV_OOM;
                        }
                    }
                    else
                    {
                        firstItem = false;
                    }
                       
                    if(!pstrProtocolInfo->append(*it))
                    {
                        return ERROR_AV_OOM;
                    }
                }
            }

            return SUCCESS_AV;
        }

    }
}

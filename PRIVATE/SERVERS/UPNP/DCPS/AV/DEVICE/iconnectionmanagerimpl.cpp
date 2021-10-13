//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
    : m_nLastCreatedConnectionID(-1),
      m_pSubscriber(NULL)
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

    return av::SUCCESS_AV;
}


DWORD IConnectionManagerImpl::Unadvise(/*[in]*/ av::IEventSink *pSubscriber)
{
    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);
    
    m_pSubscriber = NULL;

    return av::SUCCESS_AV;
}


DWORD IConnectionManagerImpl::AddSinkProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retAdd = details::generic_AddProtocol(pszProtocolInfo, &m_setSinkProtocols);
    
    if(SUCCESS_AV != retAdd)
        return retAdd;

    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SinkProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
    
    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::RemoveSinkProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retRem = details::generic_RemoveProtocol(pszProtocolInfo, &m_setSinkProtocols);
    if(SUCCESS_AV != retRem)
        return retRem;

    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SinkProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }
            
    return SUCCESS_AV;
}
 

DWORD IConnectionManagerImpl::AddSourceProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retAdd = details::generic_AddProtocol(pszProtocolInfo, &m_setSourceProtocols);
    if(SUCCESS_AV != retAdd)
        return retAdd;

    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SourceProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }

    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::RemoveSourceProtocol(LPCWSTR pszProtocolInfo)
{
    if(!pszProtocolInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const DWORD retRem = details::generic_RemoveProtocol(pszProtocolInfo, &m_setSourceProtocols);
    if(SUCCESS_AV != retRem)
        return retRem;

    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::SourceProtocolInfo, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
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
    if(!pszRemoteProtocolInfo || !pszPeerConnectionManager || !pConnectionID || !ppAVTransport || !ppRenderingControl)
        return ERROR_AV_POINTER;


    const DWORD retCUCID = CreateUniqueConnectionID(pConnectionID);
    if(SUCCESS_AV != retCUCID)
        return retCUCID;

    *ppAVTransport = NULL;
    *ppRenderingControl = NULL;
    

    //
    // Call customization point
    //
    const DWORD retCC = CreateConnection(pszRemoteProtocolInfo,
                                         Direction,
                                         *pConnectionID,
                                         ppAVTransport,
                                         ppRenderingControl);
    if(SUCCESS_AV != retCC)
        return retCC;


    ConnectionInfo connection;
    
    connection.pRenderingControl        = *ppRenderingControl;
    connection.pAVTransport             = *ppAVTransport;
    connection.Direction                = Direction;
    connection.nPeerConnectionID        = PeerConnectionID;
    connection.strPeerConnectionManager = pszPeerConnectionManager;
    connection.strRemoteProtocolInfo    = pszRemoteProtocolInfo;
    connection.strStatus                = L"Unknown";

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    if(m_mapConnections.end() == m_mapConnections.insert(*pConnectionID, connection))
        return ERROR_AV_OOM;
    
    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::CurrentConnectionIDs, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }

    m_nLastCreatedConnectionID = *pConnectionID;

    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::ConnectionComplete(
		/* [in] */ long ConnectionID)
{
    // Call customization point
    const DWORD retEC = EndConnection(ConnectionID);
    
    if(SUCCESS_AV != retEC)
        return retEC;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    m_mapConnections.erase(ConnectionID);

    if(m_pSubscriber)
        if(SUCCESS_AV != m_pSubscriber->OnStateChanged(ConnectionManagerState::CurrentConnectionIDs, static_cast<LPCWSTR>(NULL)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OnStateChanged failed!")));
        }

    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::GetFirstConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    if(!pConnectionID)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    m_itLastAskedForConnection = m_mapConnections.begin();
    if(m_itLastAskedForConnection == m_mapConnections.end())
        return ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE; // no connections
    *pConnectionID = m_itLastAskedForConnection->first;
    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::GetNextConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    if(!pConnectionID)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    m_itLastAskedForConnection++;
    if(m_itLastAskedForConnection == m_mapConnections.end())
        return ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE; // no remaining connections
    *pConnectionID = m_itLastAskedForConnection->first;
    return SUCCESS_AV;
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
        return ret;

    ret = details::generic_get_ProtocolInfo(pstrSinkProtocolInfo,   m_setSinkProtocols);
    if(SUCCESS_AV != ret)
        return ret;

    return SUCCESS_AV;
}


DWORD IConnectionManagerImpl::GetCurrentConnectionInfo(
		/* [in] */ long ConnectionID,
		/* [in, out] */ ConnectionInfo* pConnectionInfo)
{
    if(!pConnectionInfo)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const ConnInfoMap::const_iterator it = m_mapConnections.find(ConnectionID);
    if(it == m_mapConnections.end())
        return ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE;

    *pConnectionInfo = it->second;

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

DWORD IConnectionManagerImpl::CreateUniqueConnectionID(long* pNewConnectionID)
{
    if(!pNewConnectionID)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section> csDataMembersLock(m_csDataMembers);

    const long firstID = m_nLastCreatedConnectionID; // used to detect if we've stepped through all connection ids
    *pNewConnectionID  = firstID;
    
    do {
        if(*pNewConnectionID >= 0)
            ++(*pNewConnectionID);
        else // ConnectionIDs can not be negative, skip over all negative numbers
            *pNewConnectionID = 0;    

    } while (m_mapConnections.end() != m_mapConnections.find(*pNewConnectionID)
             && firstID != *pNewConnectionID);

    if(firstID == *pNewConnectionID)
        return ERROR_AV_UPNP_CM_LOCAL_RESTRICTIONS; // all connection ids in use
    else
        return SUCCESS_AV;
}



//
// Non-member internal functions
//

namespace av_upnp {
    namespace details {

        DWORD details::generic_AddProtocol(LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet)
        {
            if(!pszProtocolInfo || !pProtocolInfoSet)
                return ERROR_AV_POINTER;

            wstring strProtocolInfo(pszProtocolInfo);
            if(FAILED(EscapeAVDCPListDelimiters(&strProtocolInfo)))
                return ERROR_AV_OOM;

            if(pProtocolInfoSet->end() == pProtocolInfoSet->insert(strProtocolInfo))
                return ERROR_AV_OOM;

            return SUCCESS_AV;
        }


        DWORD details::generic_RemoveProtocol(LPCWSTR pszProtocolInfo, ProtocolInfoSet* pProtocolInfoSet)
        {
            if(!pszProtocolInfo || !pProtocolInfoSet)
                return ERROR_AV_POINTER;

            wstring strProtocolInfo(pszProtocolInfo);
            if(FAILED(EscapeAVDCPListDelimiters(&strProtocolInfo)))
                return ERROR_AV_OOM;

            pProtocolInfoSet->erase(strProtocolInfo);

            return SUCCESS_AV;
        }


        DWORD details::generic_get_ProtocolInfo(wstring* pstrProtocolInfo, const ProtocolInfoSet &protocolInfoSet)
        {
            if(!pstrProtocolInfo)
                return ERROR_AV_POINTER;

            // Build AV DCP list of protocols
            bool firstItem = true;
            const ProtocolInfoSet::const_iterator itEnd = protocolInfoSet.end();
            for(ProtocolInfoSet::const_iterator it = protocolInfoSet.begin(); it != itEnd; ++it)
            {
                if(!firstItem)
                {
                    if(!pstrProtocolInfo->append(AVDCPListDelimiter))
                        return ERROR_AV_OOM;
                }
                else
                {
                    firstItem = false;
                }
                   
                if(!pstrProtocolInfo->append(*it))
                    return ERROR_AV_OOM;
            }

            return SUCCESS_AV;
        }

    }
}
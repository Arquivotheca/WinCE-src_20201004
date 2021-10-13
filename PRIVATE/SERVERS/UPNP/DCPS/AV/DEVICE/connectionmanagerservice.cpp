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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "av_upnp.h"

using namespace av_upnp;

/////////////////////////////////////////////////////////////////////////////
// ConnectionManagerServiceImpl

ConnectionManagerServiceImpl::ConnectionManagerServiceImpl()
    : m_pIConnectionManager(NULL), m_pAVTS(NULL), m_pRCS(NULL)
{
    InitErrDescrips();
    InitToolkitErrs();
    InitDISPIDs();
}


ConnectionManagerServiceImpl::~ConnectionManagerServiceImpl()
{
    if(m_pIConnectionManager)
        const DWORD retUnadvise = m_pIConnectionManager->Unadvise(this);
        // if Unadvise() fails, we can do nothing but ignore.
}


DWORD ConnectionManagerServiceImpl::Init(/* [in] */ IConnectionManager*             pIConnectionManager,
                                         /* [in] */ AVTransportServiceImpl*         pAVTransportService,
                                         /* [in] */ RenderingControlServiceImpl*    pRenderingControlService)
{
    if(m_pIConnectionManager || m_pAVTS || m_pRCS)
        return ERROR_AV_ALREADY_INITED;

    if(!pIConnectionManager)
        return ERROR_AV_POINTER; // this class requires m_pIConnectionManager in all cases


    m_pIConnectionManager = pIConnectionManager;
    m_pAVTS               = pAVTransportService;
    m_pRCS                = pRenderingControlService;

    const DWORD retAdvise = m_pIConnectionManager->Advise(this);
    if(SUCCESS_AV != retAdvise)
        return retAdvise;

    return SUCCESS_AV;
}



//
// IUPnPEventSource
//

STDMETHODIMP ConnectionManagerServiceImpl::Advise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    if(!punkSubscriber)
        return E_POINTER;

    return punkSubscriber->QueryInterface(IID_IUPnPEventSink, reinterpret_cast<void**>(&m_punkSubscriber));
}


STDMETHODIMP ConnectionManagerServiceImpl::Unadvise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    if(m_punkSubscriber)
        m_punkSubscriber.Release();

    return S_OK;
}



//
// IUPnPService_ConnectionManager1
//

STDMETHODIMP ConnectionManagerServiceImpl::get_SourceProtocolInfo(BSTR* pSourceProtocolInfo)
{
    if(!pSourceProtocolInfo)
        return E_POINTER;

    const HRESULT hr = generic_get_ProtocolInfo(pSourceProtocolInfo, NULL);
    if(FAILED(hr))
        return hr; // generic_get_ProtocolInfo does error work, so we do not.

    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_SinkProtocolInfo(BSTR* pSinkProtocolInfo)
{
    if(!pSinkProtocolInfo)
        return E_POINTER;

    const HRESULT hr = generic_get_ProtocolInfo(NULL, pSinkProtocolInfo);
    if(FAILED(hr))
        return hr; // generic_get_ProtocolInfo does error work, so we do not.
  
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_CurrentConnectionIDs(BSTR* pCurrentConnectionIDs)
{
    if(!pCurrentConnectionIDs)
        return E_POINTER;

    if(!m_pIConnectionManager)
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);


    wstring strConnectionIDs;

    wstring   strCurConnectionID;
    const int nltowLimit = 33;
    const int radixTen = 10;
    long      nCurConnectionID;

    strCurConnectionID.reserve(nltowLimit);


    if(SUCCESS_AV == m_pIConnectionManager->GetFirstConnectionID(&nCurConnectionID))
    {
        _ltow(nCurConnectionID, strCurConnectionID.get_buffer(), radixTen);
        if(!strConnectionIDs.append(strCurConnectionID))
            return m_ErrReport.ReportError(ERROR_AV_OOM);

        while(SUCCESS_AV == m_pIConnectionManager->GetNextConnectionID(&nCurConnectionID))
        {
            _itow(nCurConnectionID, strCurConnectionID.get_buffer(), radixTen);
            if(!strConnectionIDs.append(AVDCPListDelimiter))
                return m_ErrReport.ReportError(ERROR_AV_OOM);
            if(!strConnectionIDs.append(strCurConnectionID))
                return m_ErrReport.ReportError(ERROR_AV_OOM);
        }
    }
    else
    {
        // no connections, empty string
    }


    // Set out argument
    *pCurrentConnectionIDs = SysAllocString(strConnectionIDs);
    if(!*pCurrentConnectionIDs)
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    return S_OK;
}


// get_A_ARG_TYPE_foo() methods are implemented only to satisfy upnp's devicehost, they are not used

STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_ConnectionStatus(BSTR* pA_ARG_TYPE_ConnectionStatus)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_ConnectionManager(BSTR* pA_ARG_TYPE_ConnectionManager)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_Direction(BSTR* pA_ARG_TYPE_Direction)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_ProtocolInfo(BSTR* pA_ARG_TYPE_ProtocolInfo)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_ConnectionID(long* pA_ARG_TYPE_ConnectionID)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_AVTransportID(long* pA_ARG_TYPE_AVTransportID)
{
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::get_A_ARG_TYPE_RcsID(long* pA_ARG_TYPE_RcsID)
{
    return S_OK;
}



STDMETHODIMP ConnectionManagerServiceImpl::GetProtocolInfo(BSTR* pSource,
                                                           BSTR* pSink)
{
    if(!pSource || !pSink)
        return E_POINTER;

    return generic_get_ProtocolInfo(pSource, pSink); // generic_get_ProtocolInfo does error work, so we do not.
}


void ReleaseVirtualService(IVirtualService* pService)
{
    pService->Release();
}


STDMETHODIMP ConnectionManagerServiceImpl::PrepareForConnection(BSTR RemoteProtocolInfo,
                                                                BSTR PeerConnectionManager,
                                                                long PeerConnectionID,
                                                                BSTR Direction,
                                                                long* pConnectionID,
                                                                long* pAVTransportID,
                                                                long* pRcsID)
{
    if(!pConnectionID || !pAVTransportID || !pRcsID)
        return E_POINTER;

    if(!m_pIConnectionManager)
       return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);

    // Setup arguments for call to IConnectionManager
    DIRECTION   direction;
    wstring     strDirection;
    wstring     strRemoteProtocolInfo, strPeerConnectionManager;
    
    if(!strRemoteProtocolInfo.assign(RemoteProtocolInfo, SysStringLen(RemoteProtocolInfo)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);
        
    if(!strPeerConnectionManager.assign(PeerConnectionManager, SysStringLen(PeerConnectionManager)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    if(!strDirection.assign(Direction, SysStringLen(Direction)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);
        
    if(details::Direction::Input == strDirection)
        direction = INPUT;
        
    else if(details::Direction::Output == strDirection)
        direction = OUTPUT;
    else
        return m_ErrReport.ReportError(ERROR_AV_UPNP_CM_INCOMPATIBLE_DIRECTION);

    // using smart pointers for virtual services to make sure they are release in case of any failure
    typedef ce::auto_xxx<IAVTransport*, void (*)(IVirtualService*), ReleaseVirtualService, NULL>        auto_av_transport_ptr;
    typedef ce::auto_xxx<IRenderingControl*, void (*)(IVirtualService*), ReleaseVirtualService, NULL>   auto_rendering_control_ptr;
    
    auto_av_transport_ptr       pIAVTransport;
    auto_rendering_control_ptr  pIRenderingControl;

    // Make the call
    const DWORD retPFC = m_pIConnectionManager->PrepareForConnection(strRemoteProtocolInfo,
                                                                     strPeerConnectionManager,
                                                                     PeerConnectionID,
                                                                     direction,
                                                                     pConnectionID,
                                                                     &pIAVTransport,
                                                                     &pIRenderingControl);
                                                                     
    if(retPFC != SUCCESS_AV)
        return m_ErrReport.ReportError(retPFC);
    
    // add entry to the map of virtual instance ids
    IDMap::iterator itIDs = m_mapIDs.insert(*pConnectionID, InstanceIDs());
    
    if(m_mapIDs.end() != itIDs)
    {
        // set ids to -1 until the services are successfully registered
        itIDs->second.AVTID = *pAVTransportID = -1;
        itIDs->second.RCID = *pRcsID = -1;
    }
    else
    {
        // cleanup after the incomplete connection
        ConnectionComplete(*pConnectionID);
                
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    // register virtual instance of AVTransport
    if(pIAVTransport)
    {
        if(!m_pAVTS)
        {
            DEBUGMSG(ZONE_AV_ERROR, (TEXT("IConnectionManager::PrepareForConnection or CreateConnection returned non-NULL pIAVTransport but")));
            DEBUGMSG(ZONE_AV_ERROR, (TEXT("\tConnectionManagerServiceImpl::Init was called with NULL for pAVTransportService")));
        }
        else
        {
            const DWORD retRI = m_pAVTS->RegisterInstance(pIAVTransport, pAVTransportID);
            
            if(SUCCESS_AV != retRI)
            {
                // cleanup after the incomplete connection
                ConnectionComplete(*pConnectionID);
                
                return m_ErrReport.ReportError(retRI);
            }
            
            // service was registered successfully, we can release the smart pointer and store service ID
            pIAVTransport.release();
            itIDs->second.AVTID = *pAVTransportID;
        }
    }

    // register virtual instance of AVTransport
    if(pIRenderingControl)
    {
        if(!m_pRCS)
        {
            DEBUGMSG(ZONE_AV_ERROR, (TEXT("IConnectionManager::PrepareForConnection or CreateConnection returned non-NULL pIRenderingControl but")));
            DEBUGMSG(ZONE_AV_ERROR, (TEXT("\tConnectionManagerServiceImpl::Init was called with NULL for pIRenderingControlService")));
        }
        else
        {
            const DWORD retRI = m_pRCS->RegisterInstance(pIRenderingControl, pRcsID);
            
            if(SUCCESS_AV != retRI)
            {
                // cleanup after the incomplete connection
                ConnectionComplete(*pConnectionID);
                
                return m_ErrReport.ReportError(retRI);
            }
        
            // service was registered successfully, we can release the smart pointer and store service ID
            pIRenderingControl.release();
            itIDs->second.RCID = *pRcsID;
        }
    }
    
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::ConnectionComplete(long ConnectionID)
{
    if(!m_pIConnectionManager)
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
        
    // Complete connection
    const DWORD retCC = m_pIConnectionManager->ConnectionComplete(ConnectionID);
    
    if(SUCCESS_AV != retCC)
        return m_ErrReport.ReportError(retCC);

    IDMap::iterator itIDs = m_mapIDs.find(ConnectionID);
    
    if(itIDs != m_mapIDs.end())
    {
        // Unregister AVTransport instance
        if(-1 != itIDs->second.AVTID)
        {
            assert(m_pAVTS);

            if(SUCCESS_AV != m_pAVTS->UnregisterInstance(itIDs->second.AVTID))
                assert(false);
        }
        
        // Unregister RenderingControl instance
        if(-1 != itIDs->second.RCID)
        {
            assert(m_pRCS);

            if(SUCCESS_AV != m_pRCS->UnregisterInstance(itIDs->second.RCID))
                assert(false);
        }
        
        m_mapIDs.erase(itIDs);
    }
    
    return S_OK;
}


STDMETHODIMP ConnectionManagerServiceImpl::GetCurrentConnectionIDs(BSTR* pConnectionIDs)
{
    return get_CurrentConnectionIDs(pConnectionIDs);
}


STDMETHODIMP ConnectionManagerServiceImpl::GetCurrentConnectionInfo(long ConnectionID,
                                                                    long* pRcsID,
                                                                    long* pAVTransportID,
                                                                    BSTR* pProtocolInfo,
                                                                    BSTR* pPeerConnectionManager,
                                                                    long* pPeerConnectionID,
                                                                    BSTR* pDirection,
                                                                    BSTR* pStatus)
{
    if(!pRcsID || !pAVTransportID || !pProtocolInfo || !pPeerConnectionManager || !pPeerConnectionID || !pDirection || !pStatus)
        return E_POINTER;

    if(!m_pIConnectionManager)
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);

    ConnectionInfo ConnectionInfo;
    
    const DWORD retGCCI = m_pIConnectionManager->GetCurrentConnectionInfo(ConnectionID, &ConnectionInfo);
    
    if(retGCCI != SUCCESS_AV)
        return m_ErrReport.ReportError(retGCCI);

    // Set out arguments
    if(0 == m_mapIDs.size())
    {
        //
        // Special case for when Connection Manager does not implement PrepareForConnection and ConnectionComplete.
        // In this case while ConnectionManagerService does not know of the connection, there may an AVT and/or RC
        // associated with the default ConnectionID 0
        //
        
        // Implementation of IConnectionManager may succeed only for special connection instance 0
        assert(0 == ConnectionID);
        
        // Return instance ID of 0 if default instance of AVTransport is registered or -1 otherwise
        if(m_pAVTS && SUCCESS_AV == m_pAVTS->FindInstance(0, NULL))
            *pAVTransportID = 0;
        else
            *pAVTransportID = -1;
            
        // Return instance ID of 0 if default instance of RenderingControl is registered or -1 otherwise
        if(m_pRCS && SUCCESS_AV == m_pRCS->FindInstance(0, NULL))
            *pRcsID = 0;
        else
            *pRcsID = -1;
    }
    else
    {
        //
        // General case, Connection Manager does implement PrepareForConnection and ConnectionComplete,
        // thus it knows of all current connections.
        //
        const IDMap::const_iterator it = m_mapIDs.find(ConnectionID);
        
        if(it == m_mapIDs.end())
            return m_ErrReport.ReportError(ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE);
            
        *pAVTransportID  = it->second.AVTID;
        *pRcsID          = it->second.RCID;
    }

    // RemoteProtocolInfo
    if(NULL == (*pProtocolInfo = SysAllocString(ConnectionInfo.strRemoteProtocolInfo)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    // PeerConnectionManager
    if(NULL == (*pPeerConnectionManager = SysAllocString(ConnectionInfo.strPeerConnectionManager)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    // PeerConnectionID
    *pPeerConnectionID = ConnectionInfo.nPeerConnectionID;

    // Direction
    if(INPUT == ConnectionInfo.Direction)
        *pDirection = SysAllocString(details::Direction::Input);
    else
    {
        assert(OUTPUT == ConnectionInfo.Direction);
        *pDirection = SysAllocString(details::Direction::Output);
    }
    
    // Status
    if(NULL == *pDirection)
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    if(NULL == (*pStatus = SysAllocString(ConnectionInfo.strStatus)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    return S_OK;
}


// InvokeVendorAction
DWORD ConnectionManagerServiceImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return m_pIConnectionManager->InvokeVendorAction(pszActionName, pdispparams, pvarResult);
}


//
// Private
//

HRESULT ConnectionManagerServiceImpl::InitToolkitErrs()
{
    const int vErrMap[][2] =
    {
        {ERROR_AV_POINTER,          ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_OOM,              ERROR_AV_UPNP_CM_LOCAL_RESTRICTIONS},
        {ERROR_AV_INVALID_INSTANCE, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_INVALID_STATEVAR, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_ALREADY_INITED,   ERROR_AV_UPNP_ACTION_FAILED},
        {0,                         0} // 0 is used to denote the end of this array
    };


    for(unsigned int i=0; 0 != vErrMap[i][0]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddToolkitError(vErrMap[i][0], vErrMap[i][1]))
            return E_OUTOFMEMORY;
    }

    return S_OK;
}


HRESULT ConnectionManagerServiceImpl::InitErrDescrips()
{
    const int vErrNums[] =
    {
        // UPnP
        ERROR_AV_UPNP_INVALID_ACTION,
        ERROR_AV_UPNP_ACTION_FAILED,
        // ConnectionManager
        ERROR_AV_UPNP_CM_NOT_IN_NETWORK,
        ERROR_AV_UPNP_CM_INCOMPATIBLE_PROTOCOL,
        ERROR_AV_UPNP_CM_INCOMPATIBLE_DIRECTION,
        ERROR_AV_UPNP_CM_INSUFFICIENT_NET_RESOURCES,
        ERROR_AV_UPNP_CM_LOCAL_RESTRICTIONS,
        ERROR_AV_UPNP_CM_ACCESS_DENIED,
        ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE,
        0 // 0 is used to denote the end of this array
    };
    
    const wstring vErrDescrips[] =
    {
        // UPnP
        L"Invalid Action",
        L"Action Failed",
        // ConnectionManager
        L"Not in network",
        L"Incompatible protocol info",
        L"Incompatible directions",
        L"Insufficient network resources",
        L"Local restrictions",
        L"Access denied",
        L"Invalid connection reference"
    };


    for(unsigned int i=0; 0 != vErrNums[i]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddErrorDescription(vErrNums[i], vErrDescrips[i]))
            return E_OUTOFMEMORY;
    }

    return S_OK;
}


void ConnectionManagerServiceImpl::InitDISPIDs()
{
    // Evented variables for ConnectionManager
    LPCWSTR vpszStateVarNames[] =
    {
        ConnectionManagerState::SourceProtocolInfo,
        ConnectionManagerState::SinkProtocolInfo,
        ConnectionManagerState::CurrentConnectionIDs,
        0 // 0 is used to denote the end of this array
    };

    const DISPID vdispidStateVarDISPIDs[] =
    {
        DISPID_SOURCEPROTOCOLINFO,
        DISPID_SINKPROTOCOLINFO,
        DISPID_CURRENTCONNECTIONIDS
    };


    for(unsigned int i=0; 0 != vpszStateVarNames[i]; ++i)
    {
        if(m_mapDISPIDs.end() == m_mapDISPIDs.insert(vpszStateVarNames[i], vdispidStateVarDISPIDs[i]))
            return;
    }
}


HRESULT ConnectionManagerServiceImpl::generic_get_ProtocolInfo(BSTR* pSource, BSTR* pSink)
{
    if(!m_pIConnectionManager)
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);


    wstring strSourceProtocolInfo, strSinkProtocolInfo;

    const DWORD retGPI = m_pIConnectionManager->GetProtocolInfo(&strSourceProtocolInfo, &strSinkProtocolInfo);
    if(SUCCESS_AV != retGPI)
        return m_ErrReport.ReportError(retGPI);


    if(pSource)
    {
        *pSource = SysAllocString(strSourceProtocolInfo);
        if(!*pSource)
            return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    if(pSink)
    {
        *pSink   = SysAllocString(strSinkProtocolInfo);
        if(!*pSink)
            return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}

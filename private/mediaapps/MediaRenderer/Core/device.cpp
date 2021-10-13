//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// Device.cpp : Implementation of CDevice

#include "stdafx.h"


CDevice::CDevice() :
    m_pConnectionManagerService(NULL),
    m_pAVTransportService(NULL),
    m_pRenderingControlService(NULL)
{
}


HRESULT CDevice::Init()
{
    HRESULT hr;
    ce::smart_ptr<IConnection>      pConnection;
    ce::auto_ptr<CAVTransport>      pAVTransport;
    ce::auto_ptr<CRenderingControl> pRenderingControl;
    const long                      nDefaultInstanceID = 0;
    
    // Instantiate ConnectionManager service
    Chk( CComObject<av::ConnectionManagerService>::CreateInstance(&m_pConnectionManagerService));
    m_pConnectionManagerService->AddRef();

    // Instantiate AVTransport service
    Chk( CComObject<av::AVTransportService>::CreateInstance(&m_pAVTransportService));
    m_pAVTransportService->AddRef();
    
    // Instantiate RenderingControl service
    Chk( CComObject<av::RenderingControlService>::CreateInstance(&m_pRenderingControlService));
    m_pRenderingControlService->AddRef();
    
    //
    // Create Connection instance based on if the media player is in the image.
    //
    ChkBool(SUCCEEDED(GetConnection(&pConnection)), E_FAIL);
    ChkBool( pConnection != NULL, E_OUTOFMEMORY );
    
    // Default instance of AVTransport
    pAVTransport = new CAVTransport( pConnection );
    ChkBool( pAVTransport != NULL, E_OUTOFMEMORY );
    Chk( pAVTransport->Init() );
        
    ChkBool( m_pAVTransportService->RegisterInstance( pAVTransport, nDefaultInstanceID ) == av::SUCCESS_AV, E_FAIL );
    pAVTransport.release();

    // Default instance of RenderingControl
    pRenderingControl = new CRenderingControl( pConnection );
    ChkBool( pRenderingControl != NULL, E_OUTOFMEMORY );
        
    ChkBool( m_pRenderingControlService->RegisterInstance( pRenderingControl, nDefaultInstanceID ) == av::SUCCESS_AV, E_FAIL );       
    pRenderingControl.release();

    ChkBool( m_pConnectionManagerService->Init(&m_ConnectionManager, m_pAVTransportService, m_pRenderingControlService) == av::SUCCESS_AV, E_FAIL );              
    ChkBool( m_ConnectionManager.SetConnection( pConnection ) == av::SUCCESS_AV, E_FAIL );
    Chk( LoadProtocols());

Cleanup:
    return hr;
    
}


CDevice::~CDevice()
{
    if(m_pConnectionManagerService)
        m_pConnectionManagerService->Release();

    if(m_pAVTransportService)
    {
        m_pAVTransportService->UnregisterInstance( 0 );
        m_pAVTransportService->Release();
    }

    if(m_pRenderingControlService)
    {
        const long nDefaultInstanceID = 0;
        m_pRenderingControlService->UnregisterInstance( 0 );
        m_pRenderingControlService->Release();
    }
}


STDMETHODIMP 
CDevice::Initialize( BSTR bstrXMLDesc, BSTR bstrDeviceIdentifier, BSTR bstrInitString )
{
    HRESULT  hr = S_OK;;
    CComBSTR bstr;
    CComPtr<IUPnPRegistrar> pRegistrar;
    
    Chk( CoCreateInstance( CLSID_UPnPRegistrar, NULL, CLSCTX_SERVER, IID_IUPnPRegistrar, (LPVOID*)&pRegistrar ));
       
    bstr = SysAllocString(L"uuid:A1DF31F3-405F-14ee-940D-7CA2DAC16C64");
    ChkBool( bstr != NULL, E_OUTOFMEMORY )
    Chk( pRegistrar->GetUniqueDeviceName( bstrDeviceIdentifier, bstr, &m_bstrUDN ));
    Chk( Init());
    
Cleanup:
    return hr;
}


STDMETHODIMP CDevice::GetServiceObject( BSTR bstrUDN, BSTR bstrServiceId, IDispatch ** ppdispService )
{
    HRESULT hr = S_OK;

    ChkBool( m_pConnectionManagerService != NULL, E_UNEXPECTED );
    ChkBool( m_pAVTransportService != NULL, E_UNEXPECTED );
    ChkBool( m_pRenderingControlService != NULL, E_UNEXPECTED );        
    ChkBool( m_bstrUDN == bstrUDN, E_FAIL );

    if( wcscmp(L"urn:upnp-org:serviceId:ConnectionManager", bstrServiceId) == 0)
    {
        Chk( m_pConnectionManagerService->QueryInterface(IID_IDispatch, (void**)ppdispService));
    }
    else if( wcscmp(L"urn:upnp-org:serviceId:AVTransport", bstrServiceId) == 0)
    {
        Chk( m_pAVTransportService->QueryInterface(IID_IDispatch, (void**)ppdispService));
    }
    else if( wcscmp(L"urn:upnp-org:serviceId:RenderingControl", bstrServiceId) == 0 )
    {
        Chk( m_pRenderingControlService->QueryInterface(IID_IDispatch, (void**)ppdispService));
    }
    else
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("MediaRenderer::GetServiceObject(): Unable to match given ServiceID!")));
        Chk( E_FAIL );
    }

Cleanup:
    return hr;
}

//
// LoadProtocols from registry
//
HRESULT CDevice::LoadProtocols()
{
    HKEY hKey;
    DWORD dwIdx = 0;
    DWORD dwName, dwNameLength = 0;

    if(ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"COMM\\UPnPDevices\\MediaRenderer\\SinkProtocolInfo", 0,0, &hKey)) {
        DEBUGMSG(1, (L"LoadProtocols: error, cant open HKLM\\COMM\\UPnPDevices\\MediaRenderer\\SinkProtocolInfo\n"));
        return E_FAIL;
    }

    LONG lResult = ERROR_SUCCESS;
    while(ERROR_SUCCESS == lResult) 
    {
        WCHAR wcName[2000];
        dwName = sizeof(wcName) / sizeof(wcName[0]);
        dwNameLength = dwName;

        lResult = RegEnumKeyEx(hKey, dwIdx++, wcName, &dwName, NULL, NULL, NULL, NULL);
        if(av::SUCCESS_AV != m_ConnectionManager.AddSinkProtocol(wcName))
        {
            RegCloseKey(hKey);
            return E_FAIL;
        }

    }

    RegCloseKey(hKey);
    return S_OK;
}

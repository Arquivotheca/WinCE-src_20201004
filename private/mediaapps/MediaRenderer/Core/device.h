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
// Device.h : Declaration of the CDevice

#pragma once

    
class ATL_NO_VTABLE CDevice : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CDevice, &CLSID_Device>,
    public IUPnPDeviceControl
{
public:
    CDevice();
    ~CDevice();

    DECLARE_PROTECT_FINAL_CONSTRUCT()
    DECLARE_NO_REGISTRY()

    BEGIN_COM_MAP(CDevice)
        COM_INTERFACE_ENTRY(IUPnPDeviceControl)
    END_COM_MAP()


    // IUPnPDeviceControl methods
    STDMETHOD( Initialize )( BSTR bstrXMLDesc, BSTR bstrDeviceIdentifier, BSTR bstrInitString );
    STDMETHOD( GetServiceObject)( BSTR bstrUDN, BSTR bstrServiceId, IDispatch ** ppdispService );

private:
    HRESULT Init();
    HRESULT LoadProtocols();

    CComObject<av::ConnectionManagerService>*   m_pConnectionManagerService;
    CComObject<av::AVTransportService>*         m_pAVTransportService;
    CComObject<av::RenderingControlService>*    m_pRenderingControlService;

    CConnectionManager                          m_ConnectionManager;
    CComBSTR                                    m_bstrUDN;
};



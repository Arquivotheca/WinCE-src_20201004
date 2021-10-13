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
#include "av_upnp_ctrl_internal.h"

using namespace av_upnp;

/////////////////////////////////////////////////////////////////////////////
// MediaServerDevice

MediaServerDevice::MediaServerDevice(IUPnPDevice* pDevice)
    : m_pDevice(pDevice),
      m_pConnectionManager(NULL),
      m_pContentDirectory(NULL)
{
}


MediaServerDevice::~MediaServerDevice()
{
    delete m_pConnectionManager;
    delete m_pContentDirectory;
}


//
// GetConectionManager
// 
DWORD MediaServerDevice::GetConnectionManager(
                        IConnectionManager** ppConnectionManager,
                        wstring* pstrConnectionManager)
{
    if(!ppConnectionManager)
        return ERROR_AV_POINTER;

    ce::gate<ce::critical_section>  gate(m_cs);

    if(!m_pConnectionManager)
    {
        ce::auto_ptr<details::ConnectionManagerCtrl> pConnectionManager;
        
        pConnectionManager = new details::ConnectionManagerCtrl;
        
        if(!pConnectionManager->Init(m_pDevice))
        {
            return ERROR_AV_UPNP_ACTION_FAILED;
        }
            
        m_pConnectionManager = pConnectionManager.release();
        
        ce::auto_bstr bstrUDN;
        
        assert(m_pDevice);
        
        m_pDevice->get_UniqueDeviceName(&bstrUDN);
        
        m_strUDN = bstrUDN;
    }
        
    *ppConnectionManager = m_pConnectionManager;

    if(pstrConnectionManager)
    {
        assert(!m_strUDN.empty());
        
        *pstrConnectionManager = m_strUDN;
        *pstrConnectionManager += L"/";
        *pstrConnectionManager += m_pConnectionManager->ServiceID();
    }

    return SUCCESS_AV;
}


//
// GetContentDirectory
//
DWORD MediaServerDevice::GetContentDirectory(
            IContentDirectory** ppContentDirectory)
{
    if(!ppContentDirectory)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section>  gate(m_cs);

    if(!m_pContentDirectory)
    {
        ce::auto_ptr<details::ContentDirectoryCtrl> pContentDirectory;
        
        pContentDirectory = new details::ContentDirectoryCtrl;
        
        if(!pContentDirectory->Init(m_pDevice))
        {
            return ERROR_AV_UPNP_ACTION_FAILED;
        }
            
        m_pContentDirectory = pContentDirectory.release();
    }
        
    *ppContentDirectory = m_pContentDirectory;

    return SUCCESS_AV;
}


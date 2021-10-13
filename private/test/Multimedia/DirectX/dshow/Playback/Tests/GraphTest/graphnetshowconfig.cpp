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
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"

HRESULT TestFixedUDPPort(IAMNetShowConfig *pInterface,PlaybackInterfaceTestDesc *pConfigDesc)
{
    HRESULT hr = S_OK;
    VARIANT_BOOL status_t = VARIANT_TRUE;
    VARIANT_BOOL status_f = VARIANT_FALSE;
    long port = pConfigDesc->testPort;
    long getPort;    
    //disable HTTP and TCP protocol
    //then enable UDP and UseFixedDDPPort
    //then test put_FixedUDPPort and get_FixedUDPPort

    if(pInterface == NULL)
    {
        LOG(TEXT("%s():Null printer for IAMNetShowConfig."),__FUNCTION__);    
        hr =  E_FAIL;
        goto cleanup;
    }

    LOG(TEXT("__Begin test for FixedUDPProt"));

    hr = pInterface->put_EnableHTTP(status_f);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call put_EnableHTTP(FALSE) with hr 0x%0x."), __FUNCTION__, hr);
        hr =    E_FAIL;
        goto cleanup;
    }
    
    hr = pInterface->put_EnableUDP(status_t);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call put_EnableUDP(TRUE) with hr 0x%0x.\n"), __FUNCTION__, hr);
        hr =    E_FAIL;
        goto cleanup;
    }

    hr = pInterface->put_UseFixedUDPPort(status_t);
    if(FAILED(hr)) 
    {
          LOG(TEXT("%s(): Fail to call put_UseFixedUDPPort(TRUE) with hr 0x%0x.\n"), __FUNCTION__, hr);
        hr =    E_FAIL;
        goto cleanup;
    }

    hr = pInterface->put_FixedUDPPort(port);
    if(FAILED(hr)) 
    {
          LOG(TEXT("%s(): Fail to call put_FixedUDPPort() with hr 0x%0x.\n"), __FUNCTION__, hr);
        hr =    E_FAIL;
        goto cleanup;
    } 

    hr = pInterface->get_FixedUDPPort(&getPort);
    if(FAILED(hr)) 
    {
          LOG(TEXT("%s(): Fail to call get_FixedUDPPort() with hr 0x%0x.\n"), __FUNCTION__, hr);
        hr =    E_FAIL;
        goto cleanup;
    } 

    if(getPort != port)
    {
        LOG(TEXT("result of get_FixedUDPPort(%d) dismatch wiht put_FixedUDPPort(%d)"),getPort,port);
        hr =  E_FAIL;
        goto cleanup;
    }
    else{
        LOG(TEXT("put(%d) and get(%d) FixedUDPPort succeed."),port,getPort);
    }
cleanup:
    return hr;
}

HRESULT TestHTTPProxy(IAMNetShowConfig *pInterface,PlaybackInterfaceTestDesc *pConfigDesc)
{    
    HRESULT hr = S_OK;
    VARIANT_BOOL status_t = VARIANT_TRUE;
    VARIANT_BOOL status_f = VARIANT_FALSE;
    BSTR pszHost = pConfigDesc->pszHost; 
    BSTR pszValue = NULL;
    long port = pConfigDesc->testPort;
    long getPort;
    //disable     AutoProxy, then enable HTTPProxy.
    //then test HTTPProxyHost, HTTPProxyPort 
    
    if(pInterface == NULL)
    {
        LOG(TEXT("%s():Null printer for IAMNetShowConfig."),__FUNCTION__);    
        hr =  E_FAIL;
        goto cleanup;
    }

    LOG(TEXT("__Begin test for HTTP Proxy host and port"));

    hr = pInterface->put_EnableAutoProxy(status_f);
    if(FAILED(hr))
    {
        LOG(TEXT("%s(): Fail to call put_EnableAutoProxy(VARIANT_FALSE)"), __FUNCTION__);
        hr =  E_FAIL;
        goto cleanup;
    }

    hr = pInterface->put_UseHTTPProxy(status_t);
    if(FAILED(hr))
    {
        LOG(TEXT("%s(): Fail to call put_UseHTTPProxy(VARIANT_TRUE)"), __FUNCTION__);
        hr =  E_FAIL;
        goto cleanup;
    }

    //test HTTPProxyHost
    hr = pInterface->put_HTTPProxyHost(pszHost);
    if(FAILED(hr)) 
    {
          LOG(TEXT("%s(): Fail to call put_HTTPProxyHost(%s)\n"), __FUNCTION__,pszHost);
        hr =    E_FAIL;
        goto cleanup;
       }

    hr = pInterface->get_HTTPProxyHost(&pszValue);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call get_HTTPProxyHost()\n"), __FUNCTION__);
        hr =    E_FAIL;
        goto cleanup;
    }

    if(!wcscmp(pszHost, pszValue))
    {
        LOG(TEXT("put(%s) and get(%s) HTTPProxyHost succeed."),pszHost,pszValue);
    }
    else
    {
        LOG(TEXT("put(%s) and get(%s) HTTPProxyHost mismatch.Test failed.."),pszHost,pszValue);
        hr =  E_FAIL;
        goto cleanup;
    }

    //test HTTPProxyPort
    hr = pInterface->put_HTTPProxyPort(port);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call put_HTTPProxyPort(%d)\n"), __FUNCTION__,port);
        goto cleanup;
    }

    hr = pInterface->get_HTTPProxyPort(&getPort);
    if(FAILED(hr)) 
    {
          LOG(TEXT("%s(): Fail to call get_HTTPProxyPort()\n"), __FUNCTION__);
          goto cleanup;
    }

    if(getPort != port)
    {
        LOG(TEXT("result of get_HTTPProxyPort(%d) dismatch the put__HTTPProxyPort(%d)"),getPort,port);
        hr = E_FAIL;
       goto cleanup;    
    }
    else{
        LOG(TEXT("put(%d) and get(%d) HTTPProxyPort succeed."),port,getPort);
    }

cleanup:
    if(pszHost)
    SysFreeString(pszHost);
    if(pszValue)
    SysFreeString(pszValue);

    return hr;
}

HRESULT TestBufferingTime(IAMNetShowConfig *pInterface,PlaybackInterfaceTestDesc *pConfigDesc)
{
    HRESULT hr = S_OK;
    double getTime,referTime = pConfigDesc->bufferTime;

    if(pInterface == NULL)
    {
        LOG(TEXT("%s():Null printer for IAMNetShowConfig."),__FUNCTION__);    
        hr =  E_FAIL;
        goto cleanup;
    }

    LOG(TEXT("__Begin test for Buffering Time"));

    hr = pInterface->put_BufferingTime(referTime);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call put_BufferingTime."), __FUNCTION__);
        hr =    E_FAIL;
        goto cleanup;
    }else{
        LOG(TEXT("Call put_BufferingTime succeed, set buffer = %f"),referTime);
    }

    hr = pInterface->get_BufferingTime(&getTime);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%s(): Fail to call get_BufferingTime()."), __FUNCTION__);
        hr =    E_FAIL;
        goto cleanup;
    }
    else{
        LOG(TEXT("%s(): get_BufferingTime succed, value=(%f)"), __FUNCTION__, getTime);
    }
    
    if(getTime != referTime)
    {
        LOG(TEXT("Failure: result of get_BufferingTime():(%f) doesn't match the put_BufferingTime():(%f)"),getTime,referTime);
        hr =  E_FAIL;
        goto cleanup;
    } 

cleanup:
    return hr;
}

enum ProtocolType {
    Type_HTTP,
    Type_UDP,
    Type_TCP,
    Type_AutoProxy,
    Type_HTTPProxy,
    Type_MultiCast,
    Type_FixedUDPPort,
};

HRESULT SwitchProtocol(IAMNetShowConfig *pInterface,VARIANT_BOOL status,ProtocolType type)
{    

    HRESULT hr = S_OK;    
    TCHAR protocalName[MAX_PATH] = TEXT("");
    VARIANT_BOOL getvalue = VARIANT_FALSE; 
    
    //call put enalbe protocol function.
    switch(type)
    {
        case Type_HTTP:
            LOG(TEXT("Test for put Enable HTTP protocol, switch to %d status: "),status);
            
            StringCchCopy(protocalName, MAX_PATH, TEXT("HTTP"));
            hr = pInterface->put_EnableHTTP(status);
            break;
        case Type_UDP:
            LOG(TEXT("Test for put Enable UDP protocol, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("UDP"));
            hr = pInterface->put_EnableUDP(status);
            break;
        case Type_TCP:
            LOG(TEXT("Test for put Enable TCP protocol, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("TCP"));
            hr = pInterface->put_EnableTCP(status);
            break;
        case Type_AutoProxy:
            LOG(TEXT("Test for put Enable AutoProxy, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("AutoProxy"));
            hr = pInterface->put_EnableAutoProxy(status);
            break;
        case Type_HTTPProxy:
            LOG(TEXT("Test for put Enable HTTPProxy, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("UseHTTPProxy"));
            hr = pInterface->put_UseHTTPProxy(status);
            break;
        case Type_MultiCast:
            LOG(TEXT("Test for put Enable Multicast, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("MultiCast"));
            hr = pInterface->put_EnableMulticast(status);
            break;
        case Type_FixedUDPPort:
            LOG(TEXT("Test for  put_UseFixedUDPPort, switch to %d status: "),status);
            StringCchCopy(protocalName, MAX_PATH, TEXT("FixedUDPPort"));
            hr = pInterface->put_UseFixedUDPPort(status);
            break;
        default:
            LOG(TEXT("Unknow protocol type, test failed."));
            hr = E_FAIL;
            break;
    };

    if(FAILED(hr)) 
    {
       LOG(TEXT("Failed to call put_Enable%s() with hr 0x%0x.\n"),protocalName , hr);
        goto cleanup;
    }

    //call get enable protocol function
    switch(type)
    {
        case Type_HTTP:
            LOG(TEXT("Test for get_EnableHTTP"));
            hr = pInterface->get_EnableHTTP(&getvalue);
            break;
        case Type_UDP:
            LOG(TEXT("Test for get_EnableUDP"));
            hr = pInterface->get_EnableUDP(&getvalue);
            break;
        case Type_TCP:
            LOG(TEXT("Test for get_EnableTCP"));
            hr = pInterface->get_EnableTCP(&getvalue);
            break;
        case Type_AutoProxy:
            LOG(TEXT("Test for get_EnableAutoProxy"));
            hr = pInterface->get_EnableAutoProxy(&getvalue);
            break;
        case Type_HTTPProxy:
            LOG(TEXT("Test for get_UseHTTPProxy"));
            hr = pInterface->get_UseHTTPProxy(&getvalue);
            break;
        case Type_MultiCast:
            LOG(TEXT("Test for get_EnableMulticast"));
            hr = pInterface->get_EnableMulticast(&getvalue);
            break;
        case Type_FixedUDPPort:
            LOG(TEXT("Test for get_UseFixedUDPPort"));
            hr = pInterface->get_UseFixedUDPPort(&getvalue);
            break;
        default:
            LOG(TEXT("Unknow protocol type, test failed."));
            hr =  E_FAIL;
            break;
    };

    if(FAILED(hr)) 
    {
          LOG(TEXT("Failed to call get_xxx%s() with hr 0x%0x."),protocalName, hr);
        goto cleanup;
    }

    if(getvalue != status )
    {
        LOG(TEXT("%s protocol,put value(%d) != get value(%d). test failed."),protocalName,status,getvalue);
        hr =    E_FAIL;
        goto cleanup;
    }else{
    LOG(TEXT("  %s protocol,put value(%d) == get value(%d). test succeed."),protocalName,status,getvalue);
    }

cleanup:
    return hr;
}

HRESULT TestEnableProtocol(IAMNetShowConfig *pInterface)
{    
    HRESULT hr = S_OK;
    VARIANT_BOOL status_t = VARIANT_TRUE;
    VARIANT_BOOL status_f = VARIANT_FALSE;
    ProtocolType type;

    if(pInterface == NULL)
    {
        LOG(TEXT("%s():Null printer for IAMNetShowConfig."),__FUNCTION__);    
        hr =  E_FAIL;
        goto cleanup;
    }

    LOG(TEXT("__Begin test for Enable HTTP,TCP,UDP,AutoProxy,UseHTTPProxy and MultiCast"));
    
    for(int i = Type_HTTP;i <=  Type_FixedUDPPort; i ++)
    {
        type = (ProtocolType)i;
        
        //test for enable protocol
        hr = SwitchProtocol(pInterface,status_t,type);
        if(FAILED(hr)) 
        {
            goto cleanup;    
        }
        //test for disable protocol
        hr = SwitchProtocol(pInterface,status_f,type);
        if(FAILED(hr)) 
        {
            goto cleanup;
        }
    }
cleanup:
    return    hr;
}

TESTPROCAPI IAMNetShowConfigTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IAMNetShowConfig *pNetShowConfig = NULL;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
    {
        return SPR_HANDLED;
    }
    else if (TPM_EXECUTE != uMsg)
    {
        return TPR_NOT_HANDLED;
    }

    // Get the test config object from the test parameter
    PlaybackInterfaceTestDesc* pTestDesc = (PlaybackInterfaceTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object\n"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
    {
        testGraph.UseRemoteGB( TRUE );
    }

    //set Meida
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media for testGraph %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }else {
        LOG(TEXT("%S: successfully set media for %s"), __FUNCTION__, pMedia->GetUrl());
    }
     
    // Build the graph
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }else{
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = testGraph.FindInterfaceOnGraph( IID_IAMNetShowConfig, (void **)&pNetShowConfig );
    if ( FAILED(hr) || !pNetShowConfig )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMNetShowConfig from the graph" ), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    //set the graph in stopped state and do interface test
    hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Stopped (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    //test for all functions of IAMNetShowConfig
    hr = TestBufferingTime(pNetShowConfig,pTestDesc);
    if(FAILED(hr))
    {
        retval = TPR_FAIL;
        goto cleanup;    
    }
    
    hr = TestEnableProtocol(pNetShowConfig);
    if(FAILED(hr))
    {
        retval = TPR_FAIL;
        goto cleanup;    
    }

    hr = TestHTTPProxy(pNetShowConfig,pTestDesc);
    if(FAILED(hr))
    {
        retval = TPR_FAIL;
        goto cleanup; 
    }

    hr = TestFixedUDPPort(pNetShowConfig,pTestDesc);
    if(FAILED(hr))
    {
        retval = TPR_FAIL;
        goto cleanup; 
    }

cleanup:
    SAFE_RELEASE(pNetShowConfig);    
    // Reset the testGraph
    testGraph.Reset();
    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}

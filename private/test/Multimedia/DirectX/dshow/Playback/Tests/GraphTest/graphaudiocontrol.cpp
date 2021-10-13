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
//
//  AudioControl Test functions
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"


typedef  HRESULT ( __stdcall *pFuncpfv)();

//
// Graph/Filter/Pin display system for debugging
//

void DisplayPinsOfFilter(IBaseFilter *pFilter)
{
    IEnumPins *pEnum = NULL;
    IPin *pPin = NULL;
    IPin *pConnectedToPin = NULL;
    PIN_INFO   pinInfo;

    HRESULT hr = pFilter->EnumPins(&pEnum);

    if (SUCCEEDED(hr))
    {
        while(pEnum->Next(1, &pPin, 0) == S_OK)
        {
            if(SUCCEEDED(pPin->QueryPinInfo(&pinInfo)))
            {
                pPin->ConnectedTo(&pConnectedToPin);

                LOG(TEXT("    Pin(%s) %s - %s"), pinInfo.dir == PINDIR_INPUT? TEXT("INPUT") : TEXT("OUTPUT"), pinInfo.achName, pConnectedToPin==NULL ? TEXT("Unconnected") : TEXT("Connected"));

                if(pConnectedToPin != NULL)
                {
                    pConnectedToPin = NULL;
                }

                if(pinInfo.pFilter != NULL)
                {
                    pinInfo.pFilter->Release();
                    pinInfo.pFilter = NULL;
                }
            }

            pPin = NULL;
        }

        pEnum = NULL;
    }
}


HRESULT DisplayFilters(IGraphBuilder *m_pGraphBuilder)
{
    IEnumFilters *pIEnumFilters;
    IBaseFilter *pIBaseFilter;
    ULONG cFetched;
    FILTER_INFO filterInfo;

    HRESULT hr = m_pGraphBuilder->EnumFilters(&pIEnumFilters);

    if(SUCCEEDED(hr))
    {
        hr = E_FAIL;

        while(pIEnumFilters->Next(1, &pIBaseFilter, &cFetched) == S_OK)
        {
            hr = pIBaseFilter->QueryFilterInfo(&filterInfo);

            LOG(TEXT("Filter %s"), filterInfo.achName);

            DisplayPinsOfFilter(pIBaseFilter);

            if(filterInfo.pGraph != NULL)
            {
                filterInfo.pGraph->Release();
                filterInfo.pGraph = NULL;
            }

            pIBaseFilter = NULL;
        }

        pIEnumFilters = NULL;
    }

    return hr;
}

//
// End display
//

HRESULT FindPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, const GUID *pType, int index, IPin **ppPin)
{
    HRESULT hr = S_OK;

    if(NULL == ppPin || NULL == pFilter)
        return E_INVALIDARG;

    IEnumPins* pEnumPins = NULL;
    *ppPin = NULL;

    if(SUCCEEDED(hr = pFilter->EnumPins(&pEnumPins)))
    {
        IPin* pPin = NULL;
        int matches = 0;

        while(!*ppPin && SUCCEEDED(hr) && S_OK == (hr = pEnumPins->Next(1, &pPin, 0)))
        {
            PIN_DIRECTION CurrentDirection = PINDIR_INPUT;
            BOOL isMatch = FALSE;
            if(SUCCEEDED(hr = pPin->QueryDirection(&CurrentDirection)))
            {
                if(CurrentDirection == PinDir)
                {
                    if(!pType)
                        isMatch=TRUE;
                    else
                    {
                        IEnumMediaTypes *pEnumMediaType = NULL;
                        AM_MEDIA_TYPE *pMediaType = NULL;

                        if(SUCCEEDED(hr = pPin->EnumMediaTypes(&pEnumMediaType)))
                        {
                            while(!isMatch && S_OK == (hr = pEnumMediaType->Next(1, &pMediaType, NULL)))
                            {
                                if(pMediaType->majortype == *pType)
                                    isMatch = TRUE;
                                DeleteMediaType(pMediaType);
                            }
                            pEnumMediaType->Release();
                            pEnumMediaType = NULL;
                        }
                    }

                    if(isMatch && matches++ == index)
                    {
                        *ppPin = pPin;
                        (*ppPin)->AddRef();
                    }
                }

                pPin->Release();
                pPin=NULL;
            }
        }

        pEnumPins->Release();
        pEnumPins=NULL;
    }

    if(*ppPin == NULL)
        hr = E_FAIL;

    return hr;
}

// Retrieve the device ID of a bluetooth device
BOOL GetBluetoothDeviceId(DWORD &DeviceID)
{
    UINT TotalDevs = waveOutGetNumDevs();
    WAVEOUTCAPS wc;
    UINT Size = sizeof(wc);
    BOOL Ret = FALSE;
    for(UINT i =0; i<TotalDevs; i++)
        {
        MMRESULT Res = waveOutGetDevCaps(i, &wc, Size);
        if(MMSYSERR_NOERROR == Res && 
           (0 ==_tcsncmp(TEXT("Bluetooth Advanced Audio Output"),  wc.szPname, min(MAXPNAMELEN, sizeof(TEXT("Bluetooth Advanced Audio Output"))))))
            {
            DeviceID = i;
            Ret = TRUE;
            break;
            }
        }

    return Ret;
}

// Retrieve the device ID of an onboard audio device
BOOL GetdwAudioDeviceId(DWORD &DeviceID)
{
    UINT TotalDevs = waveOutGetNumDevs();
    WAVEOUTCAPS wc;
    UINT Size = sizeof(wc);
    BOOL Ret = FALSE;
    for(UINT i =0; i<TotalDevs; i++)
        {
        MMRESULT Res = waveOutGetDevCaps(i, &wc, Size);
        if(MMSYSERR_NOERROR == Res && 
           (0 ==_tcsncmp(TEXT("Audio Output"),  wc.szPname, min(MAXPNAMELEN, sizeof(TEXT("Audio Output"))))))
            {
            DeviceID = i;
            Ret = TRUE;
            break;
            }
        }

    return Ret;
}

HRESULT LoadDLL(LPCTSTR tzDllName)
{
    HINSTANCE hInst = LoadLibrary(tzDllName);
    pFuncpfv pfv = NULL;

    if( hInst == NULL)
        return E_FAIL;

    pfv = (pFuncpfv)GetProcAddress(hInst, TEXT("DllRegisterServer"));
    if(pfv == NULL )
    {
        FreeLibrary(hInst);
        return E_FAIL;
    }
    (*pfv)();
    FreeLibrary(hInst);
    return S_OK;
}

HRESULT AddRenderer(TestGraph *pTestGraph, IBaseFilter **pAudioFilter, IBaseFilter **pAudioFilter2)
{
    HRESULT hr = S_OK;
    IPin *m_pOutput = NULL;
    IPin *m_pInput = NULL;
    IPin *m_pAudioIn = NULL;
    IBaseFilter *pTeeFilter = NULL;
    IGraphBuilder* pGraph = pTestGraph->GetGraph();
    IBaseFilter *pTmpAudio = NULL;
    IBaseFilter *pTmpAudio2 = NULL;


    //Find Audio Renderer
    if(SUCCEEDED(pGraph->FindFilterByName(L"Audio Renderer", &pTmpAudio)))
    {
        if(SUCCEEDED(FindPin(pTmpAudio, PINDIR_INPUT, NULL, 0, &m_pAudioIn)))
        {
            if(FAILED(m_pAudioIn->ConnectedTo(&m_pOutput)))
            {
                FAIL(TEXT("Failed to retrieve the filter the renderer is connected to."));
                hr = E_FAIL;
            }
        }
        else
        {
            FAIL(TEXT("Could not get audio renderer input pin."));
            hr = E_FAIL;
        }
    }
    else
    {
        FAIL(TEXT("Failed to find the audio renderer."));
        hr = E_FAIL;
    }
    *pAudioFilter = pTmpAudio;
    //Disconnect renderer
    if(SUCCEEDED(hr))
    {
        if(SUCCEEDED(m_pOutput->Disconnect()))
        {
            if(FAILED(m_pAudioIn->Disconnect()))
            {
                FAIL(TEXT("Failed to disconnect the renderer input."));
                hr = E_FAIL;
            }
        }
        else 
        {
            FAIL(TEXT("Failed to disconnect the wave output."));
            hr = E_FAIL;
        }
    }

    //Create infinite tee and add to graph
    if(SUCCEEDED(hr) && FAILED(pTestGraph->AddFilter(InfiniteTee)))
    {
        FAIL(TEXT("Could not add infinite audio tee."));
        hr = E_FAIL;
    }

    //Add a second audio renderer
    if(SUCCEEDED(hr) && FAILED(pTestGraph->AddFilter(DefaultAudioRenderer)))
    {
        FAIL(TEXT("Could not add second audio renderer."));
        hr = E_FAIL;
    }

    //Connect infinite tee to graph
    if(SUCCEEDED(hr))
    {
        if(SUCCEEDED(pGraph->FindFilterByName(L"Infinite Tee", &pTeeFilter)))
        {
            if(SUCCEEDED(FindPin(pTeeFilter, PINDIR_INPUT, NULL, 0, &m_pInput)))
            {
                if(FAILED(pGraph->ConnectDirect(m_pOutput, m_pInput, NULL)))
                {
                    FAIL(TEXT("Could not connect graph to infinite tee."));
                    hr = E_FAIL;
                }
            }
            else 
            {
                FAIL(TEXT("Could not get tee input pin."));
                hr = E_FAIL;
            }
        }
    }

    if(m_pOutput)
    {
        m_pOutput->Release();
        m_pOutput = NULL;
    }
    if(m_pInput)
    {
        m_pInput->Release();
        m_pInput = NULL;
    }

    //Connect second renderer to the tee
    if(SUCCEEDED(hr))
    {
        if(SUCCEEDED(pGraph->FindFilterByName(L"Audio Renderer 00001", &pTmpAudio2)))
        {
            if(SUCCEEDED(FindPin(pTmpAudio2, PINDIR_INPUT, NULL, 0, &m_pInput)))
            {            
                if(SUCCEEDED(FindPin(pTeeFilter, PINDIR_OUTPUT, NULL, 0, &m_pOutput)))
                {
                    if(FAILED(pGraph->ConnectDirect(m_pOutput, m_pInput, NULL)))
                    {
                        FAIL(TEXT("Could not connect the tee to the first renderer."));
                        hr = E_FAIL;
                    }
                }
                else 
                {
                    FAIL(TEXT("Could not get infinite audio tee output pin."));
                    hr = E_FAIL;
                }
            }
            else 
            {
                FAIL(TEXT("Could not get first audio renderer input pin."));
                hr = E_FAIL;
            }
        }
        else
        {
            FAIL(TEXT("Could not find second audio renderer filter."));
            hr = E_FAIL;
        }
    }
    *pAudioFilter2 = pTmpAudio2;

    if(m_pOutput)
    {
        m_pOutput->Release();
        m_pOutput = NULL;
    }
    if(m_pInput)
    {
        m_pInput->Release();
        m_pInput = NULL;
    }

    //Connect the original audio renderer to the tee
    if(SUCCEEDED(hr))
    {
        if(SUCCEEDED(FindPin(pTeeFilter, PINDIR_OUTPUT, NULL, 1, &m_pOutput)))
        {
            if(FAILED(pGraph->ConnectDirect(m_pOutput, m_pAudioIn, NULL)))
            {
                FAIL(TEXT("Could not connect the tee to the second renderer."));
                hr = E_FAIL;
            }
        }
        else
        {
            FAIL(TEXT("Could not get tee's second output pin."));
            hr = E_FAIL;
        }
    }

    if(m_pOutput)
    {
        m_pOutput->Release();
        m_pOutput = NULL;
    }
    if(pTeeFilter)
    {
        pTeeFilter->Release();
        pTeeFilter = NULL;
    }
    if(m_pAudioIn)
    {
        m_pAudioIn->Release();
        m_pAudioIn = NULL;
    }
    if(pGraph)
    {
        pGraph->Release();
        pGraph = NULL;
    }

    return hr;
}

HRESULT CheckDeviceId(DWORD SelectRend, bool UseBT, IAudioControl* pAudioControl, DWORD dwBTDevice, DWORD dwAudioDevice)
{
    DWORD dwIACoutput = NULL;
    HRESULT hr = S_OK;

    //Check the audiocontrol setting
    switch(SelectRend)
    {
        case 0:
            if(UseBT)
            {
                if(FAILED(pAudioControl->GetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the default device to bluetooth."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwBTDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            else
            {
                if(FAILED(pAudioControl->GetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the default device to onboard audio."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwAudioDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            break;

        case 1:
            if(UseBT)
            {
                if(FAILED(pAudioControl->GetDeviceId(L"Audio Renderer", &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the first renderer to bluetooth."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwBTDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            else
            {
                if(FAILED(pAudioControl->GetDeviceId(L"Audio Renderer", &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the first renderer to onboard audio."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwAudioDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            break;

        case 2:
            if(UseBT)
            {
                if(FAILED(pAudioControl->GetDeviceId(L"Audio Renderer 00001", &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the second renderer to bluetooth."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwBTDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            else
            {
                if(FAILED(pAudioControl->GetDeviceId(L"Audio Renderer 00001", &dwIACoutput)))
                {
                    FAIL(TEXT("Could not set the second renderer to onboard audio."));
                    hr = E_FAIL;
                }
                else
                {
                    if(dwIACoutput == dwAudioDevice)
                    {
                        LOG(TEXT("The audio setting is correct."));
                    }
                    else
                    {
                        FAIL(TEXT("The audio setting is not correct."));
                        hr = E_FAIL;
                    }
                }
            }
            break;
    }

    return hr;
}

TESTPROCAPI IAudioControlTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    IAudioControl* pAudioControl = NULL;
    DWORD dwAudioDevice = 0;
    DWORD dwBTDevice = 0;
    TestGraph testGraph;
    AudioControlTestDesc *pAudioTestDesc;
    IGraphBuilder* pGraph = NULL;
    IBaseFilter *pAudioFilter = NULL;
    IBaseFilter *pAudioFilter2 = NULL;

    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    pAudioTestDesc = (AudioControlTestDesc*)lpFTE->dwUserData;
    
    PlaybackMedia* pMedia = pAudioTestDesc->GetMedia(0);
    if (!pMedia)
    {
        FAIL(TEXT("Failed to retrieve media object"));
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        if (FAILED(testGraph.SetMedia(pMedia)))
        {
            FAIL(TEXT("Failed to set media"));
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    //Create graph for sample audio
    if (FAILED(testGraph.BuildGraph()))
    {
        FAIL(TEXT("Failed to render graph"));
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
        LOG(TEXT("Successfully rendered graph."));

    //Get the graph builder
    pGraph = testGraph.GetGraph();

    //Get IAudioControl interface
    if(FAILED(pGraph->QueryInterface(IID_IAudioControl, (void **) &pAudioControl)))
    {
        FAIL(TEXT("Failed to find the IAudioControl interface."));
        retval = TPR_FAIL;
        goto cleanup;
    }

    //We only want an input API test
    if(pAudioTestDesc->Input)
    {
        UINT TotalDevs = waveInGetNumDevs();
        HRESULT hr = S_OK;
        WAVEINCAPS wc;
        UINT Size = sizeof(wc);
        DWORD result = 0;
        
        LOG(TEXT("Running audio input test."));
        // Run a full test for each audio input device
        for(UINT i =0; i<TotalDevs; i++)
        {
            MMRESULT Res = waveInGetDevCaps(i, &wc, Size);
            if(MMSYSERR_NOERROR == Res)
            {
                LOG(TEXT("Testing input %s"),wc.szPname);
                
                //Set as default
                if(FAILED(hr = pAudioControl->SetDeviceId(DEFAULT_WAVE_IN_DEVICE_ID, i)))
                {
                    FAIL(TEXT("Could not set default device."));
                    retval = TPR_FAIL;
                    break;
                }

                //Get default
                if(SUCCEEDED(hr) && FAILED(hr = pAudioControl->GetDeviceId(DEFAULT_WAVE_IN_DEVICE_ID, &result)))
                {
                    FAIL(TEXT("Could not get default device."));
                    retval = TPR_FAIL;
                    break;
                }

                //Check default
                if(SUCCEEDED(hr) && result != i)
                {
                    FAIL(TEXT("Device does not match."));
                    retval = TPR_FAIL;
                    break;
                }

                //Set using name
                if(FAILED(hr = pAudioControl->SetDeviceId(wc.szPname, i)))
                {
                    FAIL(TEXT("Could not set device."));
                    retval = TPR_FAIL;
                    break;
                }

                //Get using name
                if(SUCCEEDED(hr) && FAILED(hr = pAudioControl->GetDeviceId(wc.szPname, &result)))
                {
                    FAIL(TEXT("Could not get device."));
                    retval = TPR_FAIL;
                    break;
                }

                //Check named
                if(SUCCEEDED(hr) && result != i)
                {
                    FAIL(TEXT("Device does not match."));
                    retval = TPR_FAIL;
                    break;
                }
            }
        }
        goto cleanup;
    }

    //Get onboard audio device ID
    if(!GetdwAudioDeviceId((DWORD &)dwAudioDevice))
    {
        FAIL(TEXT("Failed to find an onboard audio device."));
        retval = TPR_FAIL;
        goto cleanup;
    }

    //Get onboard bluetooth device ID
    if(pAudioTestDesc->UseBT && !GetBluetoothDeviceId((DWORD &)dwBTDevice))
    {
        FAIL(TEXT("Failed to find a bluetooth audio device."));
        retval = TPR_FAIL;
        goto cleanup;
    }

    //Add a second renderer if needed
    if(pAudioTestDesc->ExtraRend && FAILED(AddRenderer(&testGraph, &pAudioFilter, &pAudioFilter2)))
    {
        retval = TPR_FAIL;
        goto cleanup;
    }

    ////DEBUG: Output the graph
    //DisplayFilters(pGraph);

    //Select the renderer and device
    switch(pAudioTestDesc->SelectRend)
    {
        case 0:
            if(pAudioTestDesc->UseBT)
            {
                if(FAILED(pAudioControl->SetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, dwBTDevice)))
                {
                    FAIL(TEXT("Could not set the default device to bluetooth."));
                    retval = TPR_FAIL;
                    goto cleanup;
                }
            }
            else
            {
                if(FAILED(pAudioControl->SetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, dwAudioDevice)))
                {
                    FAIL(TEXT("Could not set the default device to onboard audio."));
                    retval = TPR_FAIL;
                    goto cleanup;
                }
            }
            break;

        case 1:
            if (pAudioTestDesc->SelectRend == 1)
            {
                if(pAudioTestDesc->UseBT)
                {
                    if(FAILED(pAudioControl->SetDeviceId(L"Audio Renderer", dwBTDevice)))
                    {
                        FAIL(TEXT("Could not set the first renderer to bluetooth."));
                        retval = TPR_FAIL;
                        goto cleanup;
                    }
                }
                else
                {
                    if(FAILED(pAudioControl->SetDeviceId(L"Audio Renderer", dwAudioDevice)))
                    {
                        FAIL(TEXT("Could not set the first renderer to onboard audio."));
                        retval = TPR_FAIL;
                        goto cleanup;
                    }
                }
            }
            break;

        case 2:
            if(pAudioTestDesc->UseBT)
            {
                if(FAILED(pAudioControl->SetDeviceId(L"Audio Renderer 00001", dwBTDevice)))
                {
                    FAIL(TEXT("Could not set the second renderer to bluetooth."));
                    retval = TPR_FAIL;
                    goto cleanup;
                }
            }
            else
            {
                if(FAILED(pAudioControl->SetDeviceId(L"Audio Renderer 00001", dwAudioDevice)))
                {
                    FAIL(TEXT("Could not set the second renderer to onboard audio."));
                    retval = TPR_FAIL;
                    goto cleanup;
                }
            }
            break;
    }

    //Check the audiocontrol setting prior to playback
    LOG(TEXT("Checking audio setting prior to playback:"));
    if(FAILED(CheckDeviceId(pAudioTestDesc->SelectRend, pAudioTestDesc->UseBT, pAudioControl, dwBTDevice, dwAudioDevice)))
    {
        retval = TPR_FAIL;
        goto cleanup;
    }


    if (pAudioTestDesc->Prompt)
    {
        // Make tester aware of where output will be
        int query;

        if(pAudioTestDesc->UseBT)
            query = MessageBox(NULL, TEXT("Prepare to hear audio out of the bluetooth output."), TEXT("Manual IAudioControl Test"), MB_OK | MB_SETFOREGROUND);
        else
            query = MessageBox(NULL, TEXT("Prepare to hear audio out of the onboard audio output."), TEXT("Manual IAudioControl Test"), MB_OK | MB_SETFOREGROUND);
    }

    // Run the graph
    if(FAILED(testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS)))
    {
        FAIL(TEXT("Failed to change state to Running"));
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    // Wait for completion of playback with a duration of twice the expected time
    if(FAILED(testGraph.WaitForCompletion(INFINITE)))
    {
        FAIL(TEXT("WaitForCompletion failed"));
        retval = TPR_FAIL;
        goto cleanup;
    }

    if (pAudioTestDesc->Prompt)
    {
        // Ask the tester if playback was ok
        int query = MessageBox(NULL, TEXT("Did the playback complete succesfully out of the proper device?"), TEXT("Manual Playback Test"), MB_YESNO | MB_SETFOREGROUND);
        if (query == IDYES)
            LOG(TEXT("Playback completed successfully."));
        else if (query == IDNO)
        {
            FAIL(TEXT("Playback was not correct."));
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("Unknown response or MessageBox error."));
            retval = TPR_ABORT;
        }
    }

    //Check the audiocontrol setting after playback
    LOG(TEXT("Checking audio setting after playback:"));
    if(FAILED(CheckDeviceId(pAudioTestDesc->SelectRend, pAudioTestDesc->UseBT, pAudioControl, dwBTDevice, dwAudioDevice)))
    {
        retval = TPR_FAIL;
        goto cleanup;
    }

cleanup:
    if(pAudioFilter)
    {
        pAudioFilter->Release();
        pAudioFilter = NULL;
    }
    if(pAudioFilter2)
    {
        pAudioFilter2->Release();
        pAudioFilter2 = NULL;
    }
    if(pAudioControl)
    {
        pAudioControl->Release();
        pAudioControl = NULL;
    }

    testGraph.Reset();

    if(pGraph)
    {
        pGraph->Release();
        pGraph = NULL;
    }

    return retval;
}

// IAudioControl API test using IGraphBuilder2
TESTPROCAPI IAudioControlTest2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder *pGraph = NULL;
    IAudioControl *pAudioCtrl = NULL;
    DWORD dwAudioDevices;
    WAVEINCAPS wc;
    DWORD dwSize = sizeof(wc);
    DWORD dwDeviceId;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    AudioControlTestDesc *pTestDesc = (AudioControlTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
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
    }
    else {
        LOG(TEXT("%S: successfully set media for %s"), __FUNCTION__, pMedia->GetUrl());
    }
     
    // Build the graph
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    
    //Get the graph builder
    pGraph = testGraph.GetGraph();
    if ( !pGraph )
    {
        LOG(TEXT("%S: ERROR %d@%S - Null graph builder!"), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    //Get IAudioControl interface
    if(FAILED(pGraph->QueryInterface(IID_IAudioControl, (void **) &pAudioCtrl)))
    {
        FAIL(TEXT("Failed to find the IAudioControl interface."));
        retval = TPR_FAIL;
        goto cleanup;
    }

    //run the filter graph
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Running, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else 
    {
        LOG(TEXT("%S: change state to State_Running"), __FUNCTION__);
    }
        
    dwAudioDevices = waveInGetNumDevs();
    // Run a full test for each audio input device
    for( DWORD i =0; i<dwAudioDevices; i++ )
    {
        MMRESULT Res = waveInGetDevCaps( i, &wc, dwSize );
        if(MMSYSERR_NOERROR == Res)
        {
            LOG(TEXT("Testing input %s"),wc.szPname);
            
            //Set as default
            if( FAILED(hr = pAudioCtrl->SetDeviceId(DEFAULT_WAVE_IN_DEVICE_ID, i)) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Set default waveIn device id to %d failed. (0x%08x)" ), 
                                __FUNCTION__, __LINE__, __FILE__, i, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }

            //Get default
            if( FAILED(hr = pAudioCtrl->GetDeviceId(DEFAULT_WAVE_IN_DEVICE_ID, &dwDeviceId)) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Get default waveIn device id failed. (0x%08x)" ), 
                                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }

            if( dwDeviceId != i )
            {
                LOG( TEXT("%S: ERROR %d@%S - Default waveIn device ids not match. (%d, %d)" ), 
                                __FUNCTION__, __LINE__, __FILE__, i, dwDeviceId );
                retval = TPR_FAIL;
                goto cleanup;
           }

            //Set as default
            if( FAILED(hr = pAudioCtrl->SetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, i)) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Set default waveOut device id to %d failed. (0x%08x)" ), 
                                __FUNCTION__, __LINE__, __FILE__, i, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }

            //Get default
            if( FAILED(hr = pAudioCtrl->GetDeviceId(DEFAULT_WAVE_OUT_DEVICE_ID, &dwDeviceId)) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Get default waveOut device id failed. (0x%08x)" ), 
                                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }

            if( dwDeviceId != i )
            {
                LOG( TEXT("%S: ERROR %d@%S - Default waveOut device ids not match. (%d, %d)" ), 
                                __FUNCTION__, __LINE__, __FILE__, i, dwDeviceId );
                retval = TPR_FAIL;
                goto cleanup;
           }
        }
    }
         
cleanup:    
    if(pAudioCtrl)
    {
        pAudioCtrl->Release();
        pAudioCtrl = NULL;
    }

    if ( pGraph )
    {
        pGraph->Release();
        pGraph = NULL;
    }
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    return retval;
}

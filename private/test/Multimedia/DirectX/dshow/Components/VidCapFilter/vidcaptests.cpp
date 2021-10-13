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
#include <windows.h>

#include "TuxMain.h"
#include "TestGraph.h"
#include "TestInterfaces.h"
#include "PropertyBag.h"
#include "CamDriverLibrary.h"
#include <camera.h>
#include "common.h"
#include "VidCapTestSink.h"
#include "VidCapVerifier.h"
#include "vidcaptestuids.h"

GUID CLSID_VCapCameraDriver = DEVCLASS_CAMERA_GUID;

struct QITestStruct {
        IID id;
        BOOL bResult;
};

TESTPROCAPI VidCapQITest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;

    // Create an empty test graph
    LOG(TEXT("VidCapQITest: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***VidCapQITest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("VidCapQITest: calling GetFilter."));
    IBaseFilter* pBaseFilter = pTestGraph->GetFilter();
    if(!pBaseFilter)
    {
        LOG(TEXT("***VidCapQITest: GetFilter failed for the base filter."));
        delete pTestGraph;
        return TPR_FAIL;
    }

    HRESULT hr = S_OK;
    struct QITestStruct QIT[] = { {IID_IBaseFilter, 1},
                                                 {IID_IPersistPropertyBag, 1},
                                                 {IID_IAMCameraControl, 1},
                                                 {IID_IAMDroppedFrames, 1},
                                                 {IID_IAMVideoControl, 1},
                                                 {IID_IAMVideoProcAmp, 1},
                                                 {IID_IKsPropertySet, 1},
                                                 {IID_IVPConfig, 1},
                                                 {0, 0},
                                                 {-1,0} };
    IUnknown *QueriedInterface = NULL;

    for(int i = 0; i < countof(QIT) && retval == TPR_PASS; i++)
    {
        LOG(TEXT("VidCapQITest: Testing entry %d, which is %s"), i, QIT[i].bResult?TEXT("valid"):TEXT("invalid"));

        // first, lets verify we can get it.
        hr = pBaseFilter->QueryInterface(QIT[i].id,(void **) &QueriedInterface);
        if(QIT[i].bResult)
        {
            if(FAILED(hr))
            {
                LOG(TEXT("***VidCapQITest: Failed to query the base filter for the interface requested."));
                retval = TPR_FAIL;
            }
            else
            {
                for(int j = 0; j < countof(QIT) && retval == TPR_PASS; j++)
                {
                    LOG(TEXT("    VidCapQITest: Verifying ability to query the queried interface for the second level interface, entry %d, which is %s."), j, QIT[j].bResult?TEXT("valid"):TEXT("invalid"));
                    IUnknown *SecondLevelInterface = NULL;

                    hr = QueriedInterface->QueryInterface(QIT[j].id, (void **) &SecondLevelInterface);
                    if(QIT[j].bResult)
                    {
                        if(FAILED(hr))
                        {
                            LOG(TEXT("***VidCapQITest: Querying the extended filter for the base filter failed, expect to be able to get from any filter to any filter."));
                            retval = TPR_FAIL;
                        }
                        else
                        {
                            hr = SecondLevelInterface->Release();
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***VidCapQITest: Failed to release the second level base filter interface."));
                                retval = TPR_FAIL;
                            }
                        }
                    }
                    else if(hr != E_NOINTERFACE)
                    {
                        LOG(TEXT("***VidCapQITest: Incorrect return value for the second level invalid value interface test."));
                        retval = TPR_FAIL;
                    }
                }

                // before releasing the queried interface, verify that it handles an invalid pointer
                hr = QueriedInterface->QueryInterface(IID_IBaseFilter, NULL);
                if(SUCCEEDED(hr))
                {
                    LOG(TEXT("***VidCapQITest: QueryInterface succeeded with a NULL pointer."));
                    retval = TPR_FAIL;
                }
                else if(hr != E_POINTER)
                {
                    LOG(TEXT("***VidCapQITest: Incorrect return hresult from the QueryInterface."));
                    retval = TPR_FAIL;
                }

                hr = QueriedInterface->Release();
                if(FAILED(hr))
                {
                    LOG(TEXT("***VidCapQITest: Failed to release the filter interface."));
                    retval = TPR_FAIL;
                }
            }
        }
        else if(hr != E_NOINTERFACE)
        {
            LOG(TEXT("***VidCapQITest: Incorrect return value for an invalid interface test."));
            retval = TPR_FAIL;
        }
    }

    // now lets verify that the base filter handles invalid pointers correctly
    hr = pBaseFilter->QueryInterface(IID_IBaseFilter, NULL);
    if(SUCCEEDED(hr))
    {
        LOG(TEXT("***VidCapQITest: QueryInterface succeeded with a NULL pointer."));
        retval = TPR_FAIL;
    }
    else if(hr != E_POINTER)
    {
        LOG(TEXT("***VidCapQITest: Incorrect return hresult from the QueryInterface."));
        retval = TPR_FAIL;
    }

    // Release reference on the filter created.
    LOG(TEXT("VidCapQITest: releasing filter."));
    if(pBaseFilter)
        pBaseFilter->Release();
    
    // Release the filter - results in the created filter being released
    LOG(TEXT("VidCapQITest: deleting test graph."));
    delete pTestGraph;
    return retval;
}

TESTPROCAPI VidCapPinQITest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;

    // Create an empty test graph
    LOG(TEXT("VidCapPinQITest: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***VidCapPinQITest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("VidCapPinQITest: calling GetFilter."));
    IBaseFilter* pBaseFilter = pTestGraph->GetFilter();
    if(!pBaseFilter)
    {
        LOG(TEXT("***VidCapPinQITest: GetFilter failed."));
        retval = TPR_FAIL;
    }
    else
    {
        if(FAILED(hr = pBaseFilter->EnumPins(&pEnumPins)))
        {
           LOG(TEXT("***VidCapPinQITest: Failed to enumerate the pins."));
           retval = TPR_FAIL;
        }
        else
        {
            if(FAILED(hr = pEnumPins->Reset()))
            {
                LOG(TEXT("***VidCapPinQITest: Failed to reset the enumerated pins."));
                retval = TPR_FAIL;
            }

            struct QITestStruct QIT[] = { {IID_IPin, 1},
                                                         {IID_IAMStreamConfig, 1},
                                                         {IID_IAMBufferNegotiation, 1},
                                                         {IID_IAMStreamControl, 1},
                                                         //{IID_IAMVideoCompression, 1},
                                                         {IID_IKsPropertySet, 1},
                                                         {0, 0},
                                                         {-1,0} };

            IBaseFilter *QueriedInterface = NULL;
            IPin *pPin = NULL;

            while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK)
            {
                for(int i = 0; i < countof(QIT); i++)
                {
                    LOG(TEXT("VidCapPinQITest: Testing entry %d, which is %s"), i, QIT[i].bResult?TEXT("valid"):TEXT("invalid"));

                    // first, lets verify we can get it.
                    hr = pPin->QueryInterface(QIT[i].id,(void **) &QueriedInterface);
                    if(QIT[i].bResult)
                    {
                        if(FAILED(hr))
                        {
                            LOG(TEXT("***VidCapPinQITest: Failed to query the pin for the interface."));
                            retval = TPR_FAIL;
                        }
                        else
                        {
                            for(int j = 0; j < countof(QIT); j++)
                            {
                                LOG(TEXT("    VidCapPinQITest: Verifying ability to query for the base filter from the extended filter, entry %d, which is %s."), j, QIT[j].bResult?TEXT("valid"):TEXT("invalid"));
                                IBaseFilter *SecondLevelBaseFilter = NULL;

                                hr = QueriedInterface->QueryInterface(QIT[j].id, (void **) &SecondLevelBaseFilter);
                                if(QIT[j].bResult)
                                {
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("    ***VidCapPinQITest: Querying the extended filter for second level filter failed when expected to succeed. index %d."), j);
                                        retval = TPR_FAIL;
                                    }
                                    else
                                    {
                                        hr = SecondLevelBaseFilter->Release();
                                        if(FAILED(hr))
                                        {
                                            LOG(TEXT("    ***VidCapPinQITest: Failed to release the second level base filter interface."));
                                            retval = TPR_FAIL;
                                        }
                                    }
                                }
                                else if(hr != E_NOINTERFACE)
                                {
                                    LOG(TEXT("    ***VidCapPinQITest: Incorrect return value for the second level invalid value interface test."));
                                    retval = TPR_FAIL;
                                }
                            }

                            // now lets verify that it handles invalid pointers correctly
                            hr = QueriedInterface->QueryInterface(IID_IPin, NULL);
                            if(SUCCEEDED(hr))
                            {
                                LOG(TEXT("***VidCapPinQITest: QueryInterface succeeded with a NULL pointer."));
                                retval = TPR_FAIL;
                            }
                            else if(hr != E_POINTER)
                            {
                                LOG(TEXT("***VidCapPinQITest: Incorrect return hresult from the QueryInterface."));
                                retval = TPR_FAIL;
                            }

                            hr = QueriedInterface->Release();
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***VidCapPinQITest: Failed to release the filter interface."));
                                retval = TPR_FAIL;
                            }
                        }
                    }
                    else if(hr != E_NOINTERFACE)
                    {
                        LOG(TEXT("***VidCapPinQITest: Incorrect return value for an invalid interface test."));
                        retval = TPR_FAIL;
                    }
                }
                pPin->Release();
            }
            pEnumPins->Release();
        }
        // now lets verify that it handles invalid pointers correctly
        hr = pBaseFilter->QueryInterface(IID_IPin, NULL);
        if(SUCCEEDED(hr))
        {
            LOG(TEXT("***VidCapPinQITest: QueryInterface succeeded with a NULL pointer."));
            retval = TPR_FAIL;
        }
        else if(hr != E_POINTER)
        {
            LOG(TEXT("***VidCapPinQITest: Incorrect return hresult from the QueryInterface."));
            retval = TPR_FAIL;
        }

        // Release reference on the filter created.
        LOG(TEXT("VidCapPinQITest: releasing filter."));

        pBaseFilter->Release();
    }

    // Release the filter - results in the created filter being released
    LOG(TEXT("VidCapPinQITest: deleting test graph."));
    delete pTestGraph;
    return retval;
}


struct DriverLoadTestStruct {
        TCHAR *szDriverName;
        TCHAR *szPropInterfaceName;
        VARTYPE vt;
        BOOL bResult;
};

TESTPROCAPI VidCapDriverLoadTests( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    CAMDRIVERLIBRARY camDriverUtils;
    HRESULT hr = S_OK;
    IPersistPropertyBag* pCapPropertyBag = NULL;
    IBaseFilter* pFilter = NULL;

    CoInitialize(NULL);

    hr = camDriverUtils.Init();
    if(FAILED(hr))
    {
        LOG(TEXT("***VidCapDriverLoadTests: Failed setting up the camera driver."));
        retval = TPR_FAIL;
    }

    // index 0 should always be valid if there's a camera driver in the system.
    TCHAR *CameraName = camDriverUtils.GetDeviceName(0);

    struct DriverLoadTestStruct DLTS[] = {
                                                                {NULL, TEXT("VCapName"), VT_BSTR, 0 },
                                                                {TEXT(""), TEXT("VCapName"), VT_BSTR, 0 },
                                                                {TEXT("InvalidDeviceName:"), TEXT("VCapName"), VT_BSTR, 0 },

                                                                {CameraName, TEXT("vcapname"), VT_BSTR, 0 },
                                                                {CameraName, TEXT("VCAPNAME"), VT_BSTR, 0 },

                                                                // random variant type picked, actual contents irrelevent as long as it's not bstr
                                                                {CameraName, TEXT("VCapName"), VT_INT, 0 },
                                                                {CameraName, TEXT("VCapName"), 0, 0 },

                                                                {CameraName, TEXT("VCapName"), VT_BSTR, 1 },
                                                            };

    for(int i = 0; i < countof(DLTS) && retval == TPR_PASS; i++)
    {
        hr = CoCreateInstance(CLSID_VideoCapture, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pFilter);

        if(!pFilter || FAILED(hr))
        {
            LOG(TEXT("***VidCapDriverLoadTests: CoCreating the vcap filter failed."));
            retval = TPR_FAIL;
        }
        else
        {
            hr = pFilter->QueryInterface(IID_IPersistPropertyBag, (void**)&pCapPropertyBag);
            if(FAILED(hr))
            {
                LOG(TEXT("***VidCapDriverLoadTests: Failed to retrieve the property bag interface."));
                retval = TPR_FAIL;
            }
            else
            {
                LOG(TEXT("VidCapDriverLoadTests: Testing driver name %s, property ID %s, expected to %s."), 
                                                    DLTS[i].szDriverName?DLTS[i].szDriverName:TEXT("NULL"), 
                                                    DLTS[i].szPropInterfaceName?DLTS[i].szPropInterfaceName:TEXT("NULL"), 
                                                    DLTS[i].bResult?TEXT("succeed"):TEXT("fail"));

                IPropertyBag * pPropBag;
                VARIANT varCamName;
                VariantInit(&varCamName);
                VariantClear(&varCamName);

                if(SUCCEEDED(hr = CoCreateInstance( CLSID_PropertyBag, NULL, 
                        CLSCTX_INPROC_SERVER, IID_IPropertyBag, (LPVOID *)&pPropBag )))
                {

                    if(DLTS[i].vt == VT_BSTR)
                    {
                        varCamName.vt = DLTS[i].vt;
                        if(DLTS[i].szDriverName)
                            varCamName.bstrVal = SysAllocString(DLTS[i].szDriverName);
                        else varCamName.bstrVal = NULL;
                    }
                    else if(DLTS[i].vt == VT_INT)
                    {
                        varCamName.vt = DLTS[i].vt;
                        varCamName.intVal = 42;
                    }

                    hr = pPropBag->Write(DLTS[i].szPropInterfaceName, &varCamName);
                    if(FAILED(hr))
                    {
                        LOG(TEXT("***VidCapDriverLoadTests: Failed to write the entry to the property bag."));
                        retval = TPR_FAIL;
                    }
                    else
                    {
                        // Now load the property bag and note the result
                        // parameter 2 is the error log, which is unused.
                        hr = pCapPropertyBag->Load(pPropBag, NULL);
                        if(DLTS[i].bResult)
                        {
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***VidCapDriverLoadTests: couldn't load property bag when expected to succeed."));
                                retval = TPR_FAIL;
                            }
                        }
                        else
                        {
                            if(hr != 0x800700b7 && hr != 0x8007007b && hr != 0x80070005 && hr != 0x8007000d && hr != 0x80070002)
                            {
                                LOG(TEXT("***VidCapDriverLoadTests: Expected ERROR_INVALID_NAME, recieved 0x%08x."), hr);
                                retval = TPR_FAIL;
                            }
                        }
                    }

                    if(varCamName.vt == VT_BSTR && varCamName.bstrVal)
                        SysFreeString(varCamName.bstrVal);

                    pPropBag->Release();
                }
                pCapPropertyBag->Release();
            }
            pFilter->Release();
        }
    }


    // now, verify that loading a NULL property bag results in the correct behavior of loading "CAM1"
    hr = CoCreateInstance(CLSID_VideoCapture, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&pFilter);

    if(!pFilter || FAILED(hr))
    {
        LOG(TEXT("***VidCapDriverLoadTests: CoCreating the vcap filter failed."));
        retval = TPR_FAIL;
    }
    else
    {
        hr = pFilter->QueryInterface(IID_IPersistPropertyBag, (void**)&pCapPropertyBag);
        if(FAILED(hr))
        {
            LOG(TEXT("***VidCapDriverLoadTests: Failed to retrieve the property bag interface."));
            retval = TPR_ABORT;
        }
        else
        {
            // if no property bag is given, then the filter tries "CAM1:"
            if(0==_tcscmp(CameraName, TEXT("CAM1:")))
            {
                LOG(TEXT("Verifying that loading a NULL property bag succeeds"));
                hr = pCapPropertyBag->Load(NULL, NULL);
                if(FAILED(hr))
                {
                    LOG(TEXT("***VidCapDriverLoadTests: couldn't load property bag with a NULL property bag, when expected to succeed."));
                    retval = TPR_FAIL;
                }
            }
            pCapPropertyBag->Release();
        }
        pFilter->Release();
    }

    CoUninitialize();
    
    return retval;
}

TESTPROCAPI GetCLSIDTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    CLSID clsid = {0};

    // Create an empty test graph
    LOG(TEXT("GetCLSIDTest: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***GetCLSIDTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("GetCLSIDTest: calling GetFilter."));
    IBaseFilter* pFilter = pTestGraph->GetFilter();
    if(!pFilter)
    {
        LOG(TEXT("***GetCLSIDTest: GetFilter failed."));
        delete pTestGraph;
        return TPR_FAIL;
    }

    HRESULT hr = S_OK;
    hr = pFilter->GetClassID(&clsid);
    if(FAILED(hr))
    {
        LOG(TEXT("***GetCLSIDTest: Failed to retrieve the class id from the filter."));
        retval = TPR_FAIL;
    }

    if(clsid != CLSID_VideoCapture)
    {
        LOG(TEXT("***GetCLSIDTest: the class id retrieved is incorrect."));
        retval = TPR_FAIL;
    }

    // Release reference on the filter created.
    LOG(TEXT("GetCLSIDTest: releasing filter."));
    if(pFilter)
        pFilter->Release();
    
    // Release the filter - results in the created filter being released
    LOG(TEXT("GetCLSIDTest: deleting test graph."));
    delete pTestGraph;
    return retval;
}

enum{
    DO_START_FIRST = 0,
    DO_STOP_FIRST
};

struct PinStreamingParams {
        TCHAR *tszTestDescription;

        BOOL bUseStart1;
        REFERENCE_TIME rtStart1;
        REFERENCE_TIME *prtStart1;

        BOOL bUseStop1;
        REFERENCE_TIME rtStop1;
        REFERENCE_TIME *prtStop1;
        BOOL bSendExtra1;

        int nOrder1;
        int nDelay1;

        BOOL bUseStart2;
        REFERENCE_TIME rtStart2;
        REFERENCE_TIME *prtStart2;

        BOOL bUseStop2;
        REFERENCE_TIME rtStop2;
        REFERENCE_TIME *prtStop2;
        BOOL bSendExtra2;

        int nOrder2;
        int nDelay2;

        BOOL bUseStart3;
        REFERENCE_TIME rtStart3;
        REFERENCE_TIME *prtStart3;

        BOOL bUseStop3;
        REFERENCE_TIME rtStop3;
        REFERENCE_TIME *prtStop3;
        BOOL bSendExtra3;

        int nOrder3;
        int nDelay3;

        int nFrameCount;
        int nFrameCountVariance;
};

BOOL
CheckStreamInfo(AM_STREAM_INFO *amsi, BOOL bCheckStart, REFERENCE_TIME *rtStart, DWORD dwStartCookie, BOOL bCheckStop, REFERENCE_TIME *rtStop, DWORD dwStopCookie, BOOL bSendExtra, BOOL bDiscarding)
{
    if(bCheckStart && amsi->dwFlags & AM_STREAM_INFO_START_DEFINED)
    {
        if(!rtStart || *rtStart == 0 || *rtStart == MAX_TIME)
        {
            LOG(TEXT("***CheckStreamInfo: Start flag set when not expected to be set, NULL start time pointer, or start time is 0 or max time."));
            return FALSE;
        }

        if(amsi->tStart != *rtStart)
        {
            LOG(TEXT("***CheckStreamInfo: Start time is not the start time set by the user."));
            return FALSE;
        }

        if(amsi->dwStartCookie != dwStartCookie)
        {
            LOG(TEXT("***CheckStreamInfo: Start cookie is incorrect."));
            return FALSE;
        }
    }

    if(bCheckStop && amsi->dwFlags & AM_STREAM_INFO_STOP_DEFINED)
    {
        if(!rtStop || *rtStop == 0 || *rtStop == MAX_TIME)
        {
            LOG(TEXT("***CheckStreamInfo: Stop flag set when not expected to be set, NULL stop time pointer, or stop time is 0 or MAX_TIME."));
            return FALSE;
        }

        if(amsi->tStop != *rtStop)
        {
            LOG(TEXT("***CheckStreamInfo: Stop time is not the start time set by the user."));
            return FALSE;
        }

        if(amsi->dwStopCookie != dwStopCookie)
        {
            LOG(TEXT("***CheckStreamInfo: Stop cookie is incorrect."));
            return FALSE;
        }
    }

    return TRUE;
}

TESTPROCAPI PinFlowingTests( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();
    const int nVidCapPins = 3;
    DWORD retval = TPR_PASS;
    // the whole array is filled in with the last item used by definition, thus a single NULL
    // will intialize the whole array to NULL.
    IAMStreamControl * StreamControls[nVidCapPins] = {NULL};
    IPin *Pins[nVidCapPins] = {NULL};
    int PinCount = 0;
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;
    int index = 0;
    REFERENCE_TIME rtMAX_TIME = MAX_TIME;

    // Create an empty test graph
    LOG(TEXT("PinFlowingTests: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***PinFlowingTests: No pTestGraph given."));
        retval = TPR_FAIL;
    }
    else
    {
        // Ask for a filter - results in the filter being created and addreffed
        LOG(TEXT("PinFlowingTests: calling GetFilter."));

        IMediaControl* pMediaControl = NULL;
        IBaseFilter* pFilter = NULL;
        ITestSink* pTestSink = NULL;
        IVidCapTestSink* pVidCapTestSink = NULL;
        IGraphBuilder *pGraphBuilder = NULL;
        IMediaSeeking *pMediaSeeking = NULL;
        CVidCapVerifier *pVerifier = NULL;
        AM_STREAM_INFO amsi;
        IKsPropertySet *pKs = NULL;
        GUID pinCategory = {0};
        DWORD cbReturned = 0;
        IAMVideoControl *pVideoControl = NULL;

        memset(&amsi, 0, sizeof(AM_STREAM_INFO));

        if(FAILED(pTestGraph->ConnectGraph()))
        {
            LOG(TEXT("***PinFlowingTests: ConnectGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pGraphBuilder = pTestGraph->GetFilterGraphBuilder()))
        {
            LOG(TEXT("***PinFlowingTests: GetFilterGraphBuilder failed."));
            retval = TPR_FAIL;
        }
        else if(!(pMediaControl = pTestGraph->GetMediaControlGraph()))
        {
            LOG(TEXT("***PinFlowingTests: GetMediaControlGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pFilter= pTestGraph->GetFilter()))
        {
            LOG(TEXT("***PinFlowingTests: GetFilter failed."));
            retval = TPR_FAIL;
        }
        else if(!(pTestSink= pTestGraph->GetTestSink()))
        {
            LOG(TEXT("***PinFlowingTests: GetSinkFilter failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pTestSink->QueryInterface(IID_IVidCapTestSink, (void **) &pVidCapTestSink)))
        {
            LOG(TEXT("***PinFlowingTests: Failed to query for the vidcap test sink"));
            retval = TPR_FAIL;
        }
        // necessary in order to retrieve the verifier as a video capture verifier, which it always will be.
        else if(!(pVerifier = pVidCapTestSink->GetVidCapVerifier()))
        {
            LOG(TEXT("***PinFlowingTests: GetVerifier failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pGraphBuilder->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
        {
            LOG(TEXT("***PinFlowingTests: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pFilter->QueryInterface(IID_IAMVideoControl, (void **) &pVideoControl)))
        {
            LOG(TEXT("***PinFlowingTests: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else
        {
            if(FAILED(hr = pFilter->EnumPins(&pEnumPins)))
            {
               LOG(TEXT("***PinFlowingTests: Failed to enumerate the pins."));
               retval = TPR_FAIL;
            }
            else
            {
                // enumerate the pin, putting the interface into the array if it succeeds.
                // on success, increment the pin count to indicate one full and then get the next pin interface.
                // when pin count is 3, then we've retrieved all of the pins
                while (retval == TPR_PASS && PinCount < nVidCapPins && ((hr = pEnumPins->Next(1, &Pins[PinCount], 0)) == S_OK))
                {
                    hr = Pins[PinCount]->QueryInterface(IID_IAMStreamControl, (void **) &StreamControls[PinCount]);
                    if(FAILED(hr))
                    {
                       LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream control interfaces."));
                       retval = TPR_FAIL;
                    }

                    // always increment the pin count regardless of whether or not the interface was allocated,
                    // otherwise we could leak the interface.
                    PinCount++;
                }

                if(retval != TPR_PASS)
                {
                   LOG(TEXT("***PinFlowingTests: interface allocation failed, unable to continue."));
                }
                else if(PinCount < 2)
                {
                   LOG(TEXT("***PinFlowingTests: Incorrect number of pins on the capture filter, unable to continue."));
                   retval = TPR_FAIL;
                }
                else if(FAILED(pMediaControl->Run()))
                {
                    LOG(TEXT("***PinFlowingTests: Failed to run the filtergraph."));
                    retval = TPR_FAIL;
                }
                else
                {
                    // success, we have the interfaces, and everything is ready to go for the test.

                    // first, we initialize the stream controls to stop all of the pins so we don't get any bogus samples flowing
                    for(index = 0; index < PinCount; index++)
                    {
                        if(SUCCEEDED( Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)))
                        {
                            if(SUCCEEDED( pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                            {
                                if(!IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                                {
                                    hr = StreamControls[index]->StartAt(&rtMAX_TIME, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to set the initial start time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    hr = StreamControls[index]->StopAt(0, FALSE, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // retrieve the AM_STREAM_INFO, and verify contents.
                                    hr = StreamControls[index]->GetInfo(&amsi);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream info for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(!CheckStreamInfo(&amsi, TRUE, NULL, 0, TRUE, NULL, 0, FALSE, FALSE))
                                    {
                                        LOG(TEXT("***PinFlowingTests: verifying the stream info for pin %d failed when doing stream initialization."), index);
                                        retval = TPR_FAIL;
                                    }
                                }
                            }
                            else
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to retrieve the pin category."));
                                retval = TPR_FAIL;
                                pKs->Release();
                                continue;
                            }

                            pKs->Release();
                        }
                        else
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the IKsPropertySet interface to determine the pin type."));
                            retval = TPR_FAIL;
                            continue;
                        }
                    }

                    for(index = 0; index < PinCount; index++)
                    {
                        BOOL IsStillPin = FALSE;
                        BOOL IsPreviewPin = FALSE;
                        BOOL IsCapturePin = FALSE;
                        AM_MEDIA_TYPE * pamt = NULL;
                        IAMStreamConfig *pStreamConfig = NULL;
                        int nFormatCount = 0;
                        int nSCCSize = 0;
                        VIDEO_STREAM_CONFIG_CAPS vscc;
                        IPin *ConnectedPin = NULL;
                        long PinCaps = 0;

                        memset(&vscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS));

                        if(SUCCEEDED(Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)) 
                            && SUCCEEDED(pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                        {
                            if(IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                            {
                                LOG(TEXT("PinFlowingTests: Pin under test is the still pin."));
                                IsStillPin = TRUE;
                                pKs->Release();
                                continue;
                            }
                            else if(IsEqualGUID(pinCategory, PIN_CATEGORY_PREVIEW))
                            {
                                LOG(TEXT("PinFlowingTests: Pin under test is the preview pin."));
                                IsPreviewPin = TRUE;
                            }
                            else if(IsEqualGUID(pinCategory, PIN_CATEGORY_CAPTURE))
                            {
                                LOG(TEXT("PinFlowingTests: Pin under test is the capture pin."));
                                IsCapturePin = TRUE;
                            }
                            else 
                            {
                                LOG(TEXT("***PinFlowingTests: Unknown pin enumerated!!!"));
                                retval = TPR_FAIL;
                                pKs->Release();
                                continue;
                            }
                        }
                        else
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the property set interface or pin category."));
                            retval = TPR_FAIL;
                            if(pKs)
                                pKs->Release();
                            continue;
                        }

                        // now that we're done with the Ks property interface, release it.
                        if(pKs)
                        {
                            pKs->Release();
                            pKs = NULL;
                        }

                        // just a quick check that the caps flags are set correctly for this pin.
                        if(FAILED(pVideoControl->GetCaps(Pins[index], &PinCaps)))
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the video control caps."));
                            retval = TPR_FAIL;
                        }

                        if(PinCaps)
                        {
                            LOG(TEXT("***Expect no pin caps to be available on the preview or capture pin. 0x%08x set"), PinCaps);
                            retval = TPR_FAIL;
                        }

                        if(FAILED(Pins[index]->QueryInterface( IID_IAMStreamConfig, (void**) &pStreamConfig)))
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream config interface."));
                            retval = TPR_FAIL;
                            continue;
                        }

                        if(FAILED(pStreamConfig->GetNumberOfCapabilities(&nFormatCount, &nSCCSize)))
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the number of stream capabilities."));
                            retval = TPR_FAIL;
                            continue;
                        }

                        if(nFormatCount == 0)
                        {
                            LOG(TEXT("***PinFlowingTests: Format count is 0, expected atleast one format to test."));
                            retval = TPR_FAIL;
                            continue;
                        }

                        if(nSCCSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                        {
                            LOG(TEXT("***PinFlowingTests: Expected a VIDEO_STREAM_CONFIG_CAPS structure, but the size indicates otherwise."));
                            retval = TPR_FAIL;
                        }

                        // before we disconnect the graph, we want to find out what pin the vcap filter is connected to.
                        if(FAILED(Pins[index]->ConnectedTo(&ConnectedPin)))
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to determine the pin the pin under test was connected to."));
                            retval = TPR_FAIL;
                            continue;
                        }

                        for(int FormatIndex = 0; FormatIndex < nFormatCount; FormatIndex++)
                        {
                            LOG(TEXT("Testing format %d on pin %d"), FormatIndex, index);

                            if(FAILED(pMediaControl->Stop()))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to stop the filtergraph prior to changing the pin format."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            // disconnect the source and sink, so they both know they're disconnected.
                            if(FAILED(Pins[index]->Disconnect()))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to disconnect the pin prior to changing the format."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            if(FAILED(ConnectedPin->Disconnect()))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to disconnect the pin prior to changing the format."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            // retrieve the new format from the source
                            if(FAILED(pStreamConfig->GetStreamCaps(FormatIndex, &pamt, (BYTE *) &vscc)))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to get the stream caps."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            // set the format, reconnect, reinitialize the stream controls for the stream
                            // and then do the test.
                            if(FAILED(pStreamConfig->SetFormat(pamt)))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to set the pin format."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            if(FAILED(Pins[index]->Connect(ConnectedPin, NULL)))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to reconnect the pin for the format change."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            if(FAILED(pMediaControl->Run()))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to restart the filtergraph after changing the format."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            hr = StreamControls[index]->StartAt(&rtMAX_TIME, 0);
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to set the initial start time to 0 on pin %d."), index);
                                retval = TPR_FAIL;
                                continue;
                            }

                            hr = StreamControls[index]->StopAt(0, FALSE, 0);
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                retval = TPR_FAIL;
                                continue;
                            }

                            // retrieve the AM_STREAM_INFO, and verify contents.
                            hr = StreamControls[index]->GetInfo(&amsi);
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream info for pin %d."), index);
                                retval = TPR_FAIL;
                                continue;
                            }

                            if(!CheckStreamInfo(&amsi, TRUE, NULL, 0, TRUE, NULL, 0, FALSE, FALSE))
                            {
                                LOG(TEXT("***PinFlowingTests: verifying the stream info for pin %d failed when doing stream initialization."), index);
                                retval = TPR_FAIL;
                            }

                            // verify that the stream is properly stopped and nothing is flowing
                            // reset the verifier
                            pVerifier->Reset();

                            // delay
                            Sleep(500);

                            // verify that no frames have been processed
                            if(pVerifier->GetSamplesSinked(index) != 0)
                            {
                                LOG(TEXT("***PinFlowingTests: frames processed when the stream was stopped, pin %d."), index);
                                retval = TPR_FAIL;
                            }

                            if(!pamt->bFixedSizeSamples)
                            {
                                LOG(TEXT("***MediaType indicates a non fixed sized sample"));
                                retval = TPR_FAIL;
                            }

                            if(pamt->bTemporalCompression)
                            {
                                LOG(TEXT("***MediaType indicates temporal compression."));
                                retval = TPR_FAIL;
                            }

                            if(pamt->lSampleSize == 0)
                            {
                                LOG(TEXT("***MediaType indicates a sample size of 0."));
                                retval = TPR_FAIL;
                            }

                            REFERENCE_TIME rtAvgTimePerFrame = 0;
                            BITMAPINFOHEADER bih = {0};

                            // retrieve the average time per frame, used later in verification.
                            if(IsEqualGUID(pamt->formattype, FORMAT_VideoInfo))
                            {
                                VIDEOINFOHEADER *pvh = (VIDEOINFOHEADER *) pamt->pbFormat;
                                rtAvgTimePerFrame = pvh->AvgTimePerFrame;
                                bih = pvh->bmiHeader;
                            }
                            else if(IsEqualGUID(pamt->formattype,FORMAT_VideoInfo2))
                            {
                                VIDEOINFOHEADER2 * pvh2 = (VIDEOINFOHEADER2 *) pamt->pbFormat;
                                rtAvgTimePerFrame = pvh2->AvgTimePerFrame;
                                bih = pvh2->bmiHeader;
                            }
                            else
                            {
                                LOG(TEXT("***formattype is not a VideoInfo or a VideoInfo2."));
                                retval = TPR_FAIL;
                            }

                            if(rtAvgTimePerFrame == 0)
                            {
                                LOG(TEXT("***Average time per frame is 0"));
                                retval = TPR_FAIL;
                            }

                            if(bih.biSize != sizeof(BITMAPINFOHEADER))
                            {
                                LOG(TEXT("***Structure size not set in the BITMAPINFOHEADER."));
                                retval = TPR_FAIL;
                            }

                            if(bih.biWidth == 0 || bih.biHeight == 0)
                            {
                                LOG(TEXT("***sample width and height are 0 in the BITMAPINFOHEADER."));
                                retval = TPR_FAIL;
                            }

                            // used as pointers by the array below for indicating relative times for testing.
                            REFERENCE_TIME rtStartTime = 0, rtIntermission = 0, rtStopTime = 0;
                            REFERENCE_TIME rtAdjustedStart1 = 0, rtAdjustedStop1 = 0;
                            REFERENCE_TIME rtAdjustedStart2 = 0, rtAdjustedStop2 = 0;
                            REFERENCE_TIME rtAdjustedStart3 = 0, rtAdjustedStop3 = 0;

                            static DWORD StartDelayFrameCount = 10;
                            static DWORD FrameCount = 10;
                            static DWORD CompletionDelay = 200;

                            // run and start immediately, run until stopped, verify timestamps
                            // run with the start time == the stop time (which delivers 1 sample by definition).
                            // run with bSendExtra set to true, to deliver one extra sample
                            struct PinStreamingParams PSP[] = 
                            { 
                                 { TEXT("start at n, stop at n * 10, no extra frames processed."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 7
                                    },

                                 { TEXT(" stop at n * 10, start at n,no extra frames processed."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 8
                                    },

                                 { TEXT("start at n, stop at n*10, process an extra frame."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop1, TRUE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 8
                                    },

                                 { TEXT("stop at n*10, start at n, process an extra frame."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop1, TRUE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 8
                                    },

                                 { TEXT("start at n, delay n*12, stop now, no extra frames processed."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, FALSE, 0, NULL, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start at n, delay n*12, stop now and process an extra frame."),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, FALSE, 0, NULL, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, TRUE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start at n, cancel start, verify no frames processed"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, FALSE, 0, NULL, FALSE, 
                                    DO_START_FIRST, 0,
                                    TRUE, 0, &rtMAX_TIME, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    0, 0
                                    },

                                 { TEXT("start at n, stop at n, delay n*5"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    0, 2
                                    },

                                 { TEXT("stop at n, start at n, delay n*5"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    0, 2
                                    },

                                 { TEXT("start at n, stop at n with extra frame set, delay n*6"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, TRUE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, &rtMAX_TIME, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    1, 2
                                    },

                                 { TEXT("stop at n with extra frame set, start at n,  delay n*6"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, TRUE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, &rtMAX_TIME, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    1, 2
                                    },

                                 { TEXT("start now, stop at n*10, delay n*15"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame * FrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + FrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 7
                                    },
                                 { TEXT("stop at n*10, start now, delay n*15"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame * FrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (5 + FrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 7
                                    },

                                 { TEXT("start now, stop at n*10 with extra frame set, delay n*16"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame * FrameCount, &rtAdjustedStop1, TRUE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (6 + FrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 8
                                    },

                                 { TEXT("stop at n*10 with extra frame set, start now, delay n*16"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame * FrameCount, &rtAdjustedStop1, TRUE, 
                                    DO_STOP_FIRST, (int) (rtAvgTimePerFrame * (6 + FrameCount))/10000,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    6,11
                                    },

                                 { TEXT("start now, delay n*12, stop now"),
                                    TRUE, 0, NULL, FALSE, 0, NULL, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start now, delay n*12, stop now indicating extra frame"),
                                    TRUE, 0, NULL, FALSE, 0, NULL, FALSE, 
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, TRUE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, FALSE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("stop now, start now, cancel stop, delay n*12, stop now"),
                                    TRUE, 0, NULL, TRUE, 0, NULL, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start now, stop at n, cancel stop, delay n*12, stop now"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame, &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("stop at n, start now, cancel stop, delay n*12"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame, &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("stop at n, start now, cancel stop, stop at n*10, delay n*15"),
                                    TRUE, 0, NULL, TRUE, rtAvgTimePerFrame, &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, rtAvgTimePerFrame * FrameCount, &rtAdjustedStop3, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + FrameCount))/10000,
                                    5, 10
                                    },

                                 { TEXT("start at n, stop now, cancel stop, delay n*12, stop now"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, 0, NULL, FALSE, 
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("stop now, start at n, cancel stop, delay n*12, stop now"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, 0, NULL, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start at n, stop at n, cancel stop, delay n*12, stop now"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("stop at n, start at n, cancel stop, delay n*12, stop now"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (2 + FrameCount + StartDelayFrameCount))/10000,
                                    FALSE, 0, NULL, TRUE, 0, NULL, FALSE,
                                    DO_START_FIRST, CompletionDelay,
                                    5, 17
                                    },

                                 { TEXT("start at n, stop at n +1, cancel stop, stop at n*10, delay n*15"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (StartDelayFrameCount + 1), &rtAdjustedStop1, FALSE, 
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop3, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + StartDelayFrameCount + FrameCount))/10000,
                                    5, 9
                                    },

                                 { TEXT("stop at n +1, start at n, cancel stop, stop at n*10, delay n*15"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, rtAvgTimePerFrame * (StartDelayFrameCount + 1), &rtAdjustedStop1, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop3, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + StartDelayFrameCount + FrameCount))/10000,
                                    5, 9
                                    },

                                 { TEXT("start at n, stop now, cancel stop, stop at n*10, delay n*15"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, 0, NULL, FALSE, 
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop3, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + StartDelayFrameCount + FrameCount))/10000,
                                    5, 9
                                    },

                                 { TEXT("stop now, start at n, cancel stop, stop at n*10, delay n*15"),
                                    TRUE, rtAvgTimePerFrame * StartDelayFrameCount, &rtAdjustedStart1, TRUE, 0, NULL, FALSE, 
                                    DO_STOP_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, 0, &rtMAX_TIME, FALSE,
                                    DO_START_FIRST, 0,
                                    FALSE, 0, NULL, TRUE, rtAvgTimePerFrame * (FrameCount + StartDelayFrameCount), &rtAdjustedStop3, FALSE,
                                    DO_START_FIRST, (int) (rtAvgTimePerFrame * (5 + StartDelayFrameCount + FrameCount))/10000,
                                    5, 9
                                    },
                            };

                            for(int TestID = 0; TestID < countof(PSP); TestID++)
                            {

                                // reset everything for this iteration.
                                if(FAILED(pMediaControl->Stop()))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to stop the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                if(FAILED(pMediaControl->Run()))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to restart the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                REFERENCE_TIME rt = MAX_TIME;

                                hr = StreamControls[index]->StartAt(&rt, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to set the initial start time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                hr = StreamControls[index]->StopAt(0, FALSE, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                // delay 2 media sample to let the camera driver get up and running.
                                // the camera performance certification test controls permitted maximum delays for this.
                                Sleep(2 * (rtAvgTimePerFrame/10000));

                                REFERENCE_TIME rtLastFrameTimestamp = MAX_TIME;
                                REFERENCE_TIME rtFirstFrameStart = 0, rtFirstFrameStop = 0;
                                REFERENCE_TIME rtLastFrameStart = 0, rtLastFrameStop = 0;
                                rtStartTime = 0;
                                rtStopTime = 0;
                                rtAdjustedStart1 = 0;
                                rtAdjustedStop1 = 0;
                                rtAdjustedStart2 = 0;
                                rtAdjustedStop2 = 0;
                                rtAdjustedStart3 = 0;
                                rtAdjustedStop3 = 0;

                                int nFramesFailed = 0, nFramesSinked = 0;
                                unsigned int randVal = 0;
                                rand_s(&randVal);
                                DWORD dwStartCookie = randVal %100;
                                rand_s(&randVal);
                                DWORD dwStopCookie = randVal % 100;

                                LOG(TEXT("PinFlowingTests: Testing streaming for pin %d,  test %d"), index, TestID);
                                LOG(TEXT("PinFlowingTests: Test description: %s"), PSP[TestID].tszTestDescription);

                                pVerifier->Reset();

                                hr = pMediaSeeking->GetCurrentPosition(&rtStartTime);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to retrieve the current position %d."), index);
                                    retval = TPR_FAIL;
                                }

                                // make the time stored in position two the relative time
                                rtAdjustedStart1 = PSP[TestID].rtStart1+ rtStartTime;
                                rtAdjustedStop1 = PSP[TestID].rtStop1+ rtStartTime;

                                // if we're doing the start first, then do the start, then the stop, and continue.
                                // if we're doing the stop first, do the stop, then the start, and continue

                                if(PSP[TestID].nOrder1 == DO_START_FIRST && PSP[TestID].bUseStart1)
                                {
                                    // start the graph streaming for a pin
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart1, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                if(PSP[TestID].bUseStop1)
                                {
                                    hr = StreamControls[index]->StopAt(PSP[TestID].prtStop1, PSP[TestID].bSendExtra1, dwStopCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to set the stop time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // if this is a cancel stop don't set the stop time.
                                    // otherwise the expected stop time is the timestamp passed to StopAt + the avg time per frame or
                                    // 3*avg time per frame if we're sending an extra.  Unless this is a stop now, in which case
                                    // the timestamp should be "now" +/- a couple of frames
                                    if(PSP[TestID].prtStop1 && *PSP[TestID].prtStop1 != MAX_TIME)
                                        rtLastFrameTimestamp = (*PSP[TestID].prtStop1 + (PSP[TestID].bSendExtra1?(3 * rtAvgTimePerFrame):(6*rtAvgTimePerFrame)));
                                    else if(!PSP[TestID].prtStop1)
                                        rtLastFrameTimestamp = rtStartTime + (rtAvgTimePerFrame * PSP[TestID].nFrameCountVariance);
                                }

                                if(PSP[TestID].nOrder1 == DO_STOP_FIRST && PSP[TestID].bUseStart1)
                                {
                                    // start the graph streaming for a pin
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart1, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                // retrieve the AM_STREAM_INFO, and verify contents.
                                hr = StreamControls[index]->GetInfo(&amsi);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream info for pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                if(!CheckStreamInfo(&amsi, PSP[TestID].bUseStart1, PSP[TestID].prtStart1, dwStartCookie, PSP[TestID].bUseStop1, PSP[TestID].prtStop1, dwStopCookie, PSP[TestID].bSendExtra1, FALSE))
                                {
                                    LOG(TEXT("***PinFlowingTests: verifying the stream info for pin %d failed just after stream control set 1."), index);
                                    retval = TPR_FAIL;
                                }

                                // delay for completion
                                if(PSP[TestID].nDelay1)
                                    Sleep(PSP[TestID].nDelay1);

                                // get the time for just after the stop, this will be used for comparisons later
                                hr = pMediaSeeking->GetCurrentPosition(&rtIntermission);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to retrieve the current position, pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                // make the time stored in position two the relative time
                                rtAdjustedStart2 = PSP[TestID].rtStart2 + rtIntermission;
                                rtAdjustedStop2 = PSP[TestID].rtStop2 + rtIntermission;

                                if(PSP[TestID].nOrder2 == DO_START_FIRST && PSP[TestID].bUseStart2)
                                {
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart2, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                if(PSP[TestID].bUseStop2)
                                {
                                    // we've delayed, now stop the stream
                                    hr = StreamControls[index]->StopAt(PSP[TestID].prtStop2, PSP[TestID].bSendExtra2, dwStopCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to set the stop time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // if this is a cancel stop don't set the stop time.
                                    // otherwise the expected stop time is the timestamp passed to StopAt + the avg time per frame or
                                    // 3*avg time per frame if we're sending an extra.  Unless this is a stop now, in which case
                                    // the timestamp should be "now" +/- a couple of frames
                                    if(PSP[TestID].prtStop2 && *PSP[TestID].prtStop2 != MAX_TIME)
                                        rtLastFrameTimestamp = (*PSP[TestID].prtStop2 + (PSP[TestID].bSendExtra2?rtAvgTimePerFrame:(3*rtAvgTimePerFrame)));
                                    else if(!PSP[TestID].prtStop2)
                                        rtLastFrameTimestamp = rtIntermission  + (rtAvgTimePerFrame * PSP[TestID].nFrameCountVariance);
                                }

                                if(PSP[TestID].nOrder2 == DO_STOP_FIRST && PSP[TestID].bUseStart2)
                                {
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart2, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                // in case the driver decides to send more frames after the graph is stopped, make
                                // sure they get processed, and also give the graph enough time to finish the
                                // stream control
                                if(PSP[TestID].nDelay2)
                                    Sleep(PSP[TestID].nDelay2);

                                // retrieve the AM_STREAM_INFO, and verify contents.
                                hr = StreamControls[index]->GetInfo(&amsi);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to retrieve the stream info for pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                if(!CheckStreamInfo(&amsi, PSP[TestID].bUseStart2, PSP[TestID].prtStart2, dwStartCookie, PSP[TestID].bUseStop2, PSP[TestID].prtStop2, dwStopCookie, PSP[TestID].bSendExtra2, TRUE))
                                {
                                    LOG(TEXT("***PinFlowingTests: verifying the stream info for pin %d failed just after stream control set 2."), index);
                                    retval = TPR_FAIL;
                                }

                                // get the time for just after the stop, this will be used for comparisons later
                                hr = pMediaSeeking->GetCurrentPosition(&rtStopTime);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingTests: Failed to retrieve the current position, pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                // make the time stored in position two the relative time
                                rtAdjustedStart3 = PSP[TestID].rtStart3 + rtStopTime;
                                rtAdjustedStop3 = PSP[TestID].rtStop3 + rtStopTime;

                                if(PSP[TestID].nOrder3 == DO_START_FIRST && PSP[TestID].bUseStart3)
                                {
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart3, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                if(PSP[TestID].bUseStop3)
                                {
                                    // we've delayed, now stop the stream
                                    hr = StreamControls[index]->StopAt(PSP[TestID].prtStop3, PSP[TestID].bSendExtra3, dwStopCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to set the stop time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // if this is a cancel stop don't set the stop time.
                                    // otherwise the expected stop time is the timestamp passed to StopAt + the avg time per frame or
                                    // 3*avg time per frame if we're sending an extra.  Unless this is a stop now, in which case
                                    // the timestamp should be "now" +/- a couple of frames
                                    if(PSP[TestID].prtStop3 && *PSP[TestID].prtStop3 != MAX_TIME)
                                        rtLastFrameTimestamp = (*PSP[TestID].prtStop3 + (PSP[TestID].bSendExtra3?rtAvgTimePerFrame:(3*rtAvgTimePerFrame)));
                                    else if(!PSP[TestID].prtStop3)
                                        rtLastFrameTimestamp = rtStopTime  + (rtAvgTimePerFrame * PSP[TestID].nFrameCountVariance);
                                }

                                if(PSP[TestID].nOrder3 == DO_STOP_FIRST && PSP[TestID].bUseStart3)
                                {
                                    hr = StreamControls[index]->StartAt(PSP[TestID].prtStart3, dwStartCookie);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to start time for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }
                                }

                                // arbitrary delay to give everything a chance to finish off
                                if(PSP[TestID].nDelay3)
                                    Sleep(PSP[TestID].nDelay3);

                                // retrieve the AM_STREAM_INFO, and verify contents.
                                hr = StreamControls[index]->GetInfo(&amsi);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("PinFlowingTests: Failed to retrieve the stream info for pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                if(!CheckStreamInfo(&amsi, PSP[TestID].bUseStart3, PSP[TestID].prtStart3, dwStartCookie, PSP[TestID].bUseStop3, PSP[TestID].prtStop3, dwStopCookie, PSP[TestID].bSendExtra3, TRUE))
                                {
                                    LOG(TEXT("***PinFlowingTests: verifying the stream info for pin %d failed just after stream control set 3."), index);
                                    retval = TPR_FAIL;
                                }

                                // now we verify the timestamps given by the driver are valid.
                                // verify the streaming results (get the timestamp for the first frame, the last frame, the number of frames processed)
                                nFramesFailed = pVerifier->GetSamplesFailed(index);
                                nFramesSinked = pVerifier->GetSamplesSinked(index);

                                LOG(TEXT("PinFlowingTests: %d frames processed."), nFramesSinked);

                                if(nFramesSinked > 0)
                                {
                                    if(FAILED(pVerifier->GetTimestampOfFirstFrame(&rtFirstFrameStart, &rtFirstFrameStop, index)))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to retrieve the timestamp of the first frame, pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(rtFirstFrameStart < rtStartTime)
                                    {
                                        LOG(TEXT("***PinFlowingTests: The timestamp on the first frame is before the first frame was triggered to send for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(FAILED(pVerifier->GetTimestampOfLastFrame(&rtLastFrameStart, &rtLastFrameStop, index)))
                                    {
                                        LOG(TEXT("***PinFlowingTests: Failed to retrieve the timestamp of the last frame for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(rtLastFrameTimestamp != MAX_TIME && rtLastFrameStart > rtLastFrameTimestamp)
                                    {
                                        LOG(TEXT("***PinFlowingTests: The timestamp on the last frame frame is after the capture was stopped for pin %d."), index);
                                        LOG(TEXT("***PinFlowingTests: 0x%08x > 0x%08x"), (DWORD) rtLastFrameStart, (DWORD) rtLastFrameTimestamp);
                                        retval = TPR_FAIL;
                                    }

                                    if(nFramesFailed > 0)
                                    {
                                        LOG(TEXT("***PinFlowingTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                        retval = TPR_FAIL;
                                    }
                                }

                                if(PSP[TestID].nFrameCount >= 0)
                                {
                                    if(nFramesSinked < PSP[TestID].nFrameCount || nFramesSinked > (PSP[TestID].nFrameCount + PSP[TestID].nFrameCountVariance))
                                    {
                                        LOG(TEXT("***PinFlowingTests: incorrect number of frames processed, %d processed, expected %d up to %d."), nFramesSinked, PSP[TestID].nFrameCount, PSP[TestID].nFrameCount + PSP[TestID].nFrameCountVariance);
                                        retval = TPR_FAIL;
                                    }
                                }
                            }

                            if(pamt)
                                DeleteMediaType(pamt);
                        }

                        if(ConnectedPin)
                            ConnectedPin->Release();

                        if(pStreamConfig)
                            pStreamConfig->Release();
                    }
                }

                if(FAILED(pMediaControl->Stop()))
                {
                    LOG(TEXT("***PinFlowingTests: Failed to stop the filtergraph."));
                    retval = TPR_FAIL;
                }

                // release the pin interfaces allocated above, regardless of whether or not
                // initialization succeeded.
                // since the queries can stop at any point, 
                // make sure the interface was actually allocated
                // before dereferencing, this will handle the 
                // situation that initialization exited part way through.
                for(index = 0; index < PinCount; index++)
                {
                    if(StreamControls[index])
                        StreamControls[index]->Release();

                    if(Pins[index])
                        Pins[index]->Release();
                }

                pEnumPins->Release();
            }
        }

        // if any of the queries failed in the if/else block
        // then we need to make sure we clean them up outside of the main test block
        if(pVidCapTestSink)
            pVidCapTestSink->Release();
        if(pMediaSeeking)
            pMediaSeeking->Release();
        if(pGraphBuilder)
            pGraphBuilder->Release();
        if(pMediaControl)
            pMediaControl->Release();           
        if(pFilter)
            pFilter->Release();
        if(pTestSink)
            pTestSink->Release();
        if(pVideoControl)
            pVideoControl->Release();

        LOG(TEXT("PinFlowingTests: deleting test graph."));
        delete pTestGraph;
    }

    return retval;
}

TESTPROCAPI PinFlowingGraphStateTests( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();
    const int nVidCapPins = 3;
    DWORD retval = TPR_PASS;
    // the whole array is filled in with the last item used by definition, thus a single NULL
    // will intialize the whole array to NULL.
    IAMStreamControl * StreamControls[nVidCapPins] = {NULL};
    IPin *Pins[nVidCapPins] = {NULL};
    int PinCount = 0;
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;
    int index = 0;

    // Create an empty test graph
    LOG(TEXT("PinFlowingGraphStateTests: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***PinFlowingGraphStateTests: No pTestGraph given."));
        retval = TPR_FAIL;
    }
    else
    {
        // Ask for a filter - results in the filter being created and addreffed
        LOG(TEXT("PinFlowingGraphStateTests: calling GetFilter."));

        IMediaControl* pMediaControl = NULL;
        IBaseFilter* pFilter = NULL;
        ITestSink* pTestSink = NULL;
        IVidCapTestSink* pVidCapTestSink = NULL;
        IGraphBuilder *pGraphBuilder = NULL;
        IMediaSeeking *pMediaSeeking = NULL;
        CVidCapVerifier *pVerifier = NULL;
        AM_STREAM_INFO amsi;
        IKsPropertySet *pKs = NULL;
        GUID pinCategory = {0};
        DWORD cbReturned = 0;
        IAMVideoControl *pVideoControl = NULL;

        if(FAILED(pTestGraph->ConnectGraph()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: ConnectGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pGraphBuilder = pTestGraph->GetFilterGraphBuilder()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: GetFilterGraphBuilder failed."));
            retval = TPR_FAIL;
        }
        else if(!(pMediaControl = pTestGraph->GetMediaControlGraph()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: GetMediaControlGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pFilter= pTestGraph->GetFilter()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: GetFilter failed."));
            retval = TPR_FAIL;
        }
        else if(!(pTestSink= pTestGraph->GetTestSink()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: GetSinkFilter failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pTestSink->QueryInterface(IID_IVidCapTestSink, (void **) &pVidCapTestSink)))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: Failed to query for the vidcap test sink"));
            retval = TPR_FAIL;
        }
        // necessary in order to retrieve the verifier as a video capture verifier, which it always will be.
        else if(!(pVerifier = pVidCapTestSink->GetVidCapVerifier()))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: GetVerifier failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pGraphBuilder->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pFilter->QueryInterface(IID_IAMVideoControl, (void **) &pVideoControl)))
        {
            LOG(TEXT("***PinFlowingGraphStateTests: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else
        {
            if(FAILED(hr = pFilter->EnumPins(&pEnumPins)))
            {
               LOG(TEXT("***PinFlowingGraphStateTests: Failed to enumerate the pins."));
               retval = TPR_FAIL;
            }
            else
            {
                // enumerate the pin, putting the interface into the array if it succeeds.
                // on success, increment the pin count to indicate one full and then get the next pin interface.
                // when pin count is 3, then we've retrieved all of the pins
                while (retval == TPR_PASS && PinCount < nVidCapPins && ((hr = pEnumPins->Next(1, &Pins[PinCount], 0)) == S_OK))
                {
                    hr = Pins[PinCount]->QueryInterface(IID_IAMStreamControl, (void **) &StreamControls[PinCount]);
                    if(FAILED(hr))
                    {
                       LOG(TEXT("***PinFlowingGraphStateTests: Failed to retrieve the stream control interfaces."));
                       retval = TPR_FAIL;
                    }

                    // always increment the pin count regardless of whether or not the interface was allocated,
                    // otherwise we could leak the interface.
                    PinCount++;
                }

                if(retval != TPR_PASS)
                {
                   LOG(TEXT("***PinFlowingGraphStateTests: interface allocation failed, unable to continue."));
                }
                else if(PinCount < 2)
                {
                   LOG(TEXT("***PinFlowingGraphStateTests: Incorrect number of pins on the capture filter, unable to continue."));
                   retval = TPR_FAIL;
                }
                else if(FAILED(pMediaControl->Run()))
                {
                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to run the filtergraph."));
                    retval = TPR_FAIL;
                }
                else
                {
                    // success, we have the interfaces, and everything is ready to go for the test.

                    // first, we initialize the stream controls to stop all of the pins so we don't get any bogus samples flowing
                    for(index = 0; index < PinCount; index++)
                    {
                        if(SUCCEEDED( Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)))
                        {
                            if(SUCCEEDED( pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                            {
                                if(!IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                                {
                                     hr = StreamControls[index]->StartAt(0, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial start time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    hr = StreamControls[index]->StopAt(0, FALSE, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // retrieve the AM_STREAM_INFO, and verify contents.
                                    hr = StreamControls[index]->GetInfo(&amsi);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***PinFlowingGraphStateTests: Failed to retrieve the stream info for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(!CheckStreamInfo(&amsi, TRUE, NULL, 0, TRUE, NULL, 0, FALSE, FALSE))
                                    {
                                        LOG(TEXT("***PinFlowingGraphStateTests: verifying the stream info for pin %d failed when doing stream initialization."), index);
                                        retval = TPR_FAIL;
                                    }
                                }
                            }
                            else
                            {
                                LOG(TEXT("***PinFlowingTests: Failed to retrieve the pin category."));
                                retval = TPR_FAIL;
                                continue;
                            }

                            pKs->Release();
                        }
                        else
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the IKsPropertySet interface to determine the pin type."));
                            retval = TPR_FAIL;
                            continue;
                        }
                    }


                    for(index = 0; index < PinCount; index++)
                    {
                        BOOL IsStillPin = FALSE;
                        BOOL IsPreviewPin = FALSE;
                        BOOL IsCapturePin = FALSE;
                        AM_MEDIA_TYPE * pamt = NULL;
                        IAMStreamConfig *pStreamConfig = NULL;
                        int nFormatCount = 0;
                        int nSCCSize = 0;
                        VIDEO_STREAM_CONFIG_CAPS vscc;
                        IPin *ConnectedPin = NULL;
                        long PinCaps = 0;

                        memset(&vscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS));
                        
                        if(SUCCEEDED(Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)) 
                            && SUCCEEDED(pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                        {
                            if(IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                            {
                                LOG(TEXT("PinFlowingGraphStateTests: Pin under test is the still pin."));
                                IsStillPin = TRUE;
                                pKs->Release();
                                continue;
                            }
                            else if(IsEqualGUID(pinCategory, PIN_CATEGORY_PREVIEW))
                            {
                                LOG(TEXT("PinFlowingGraphStateTests: Pin under test is the preview pin."));
                                IsPreviewPin = TRUE;
                            }
                            else if(IsEqualGUID(pinCategory, PIN_CATEGORY_CAPTURE))
                            {
                                LOG(TEXT("PinFlowingGraphStateTests: Pin under test is the capture pin."));
                                IsCapturePin = TRUE;
                            }
                            else 
                            {
                                LOG(TEXT("***PinFlowingTests: Unknown pin enumerated!!!"));
                                retval = TPR_FAIL;
                                continue;
                            }
                        }
                        else
                        {
                            LOG(TEXT("***PinFlowingTests: Failed to retrieve the property set interface or pin category."));
                            retval = TPR_FAIL;
                            continue;
                        }


                        // now that we're done with the Ks property interface, release it.
                        if(pKs)
                        {
                            pKs->Release();
                            pKs = NULL;
                        }

                        // just a quick check that the caps flags are set correctly for this pin.
                        if(FAILED(pVideoControl->GetCaps(Pins[index], &PinCaps)))
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Failed to retrieve the video control caps."));
                            retval = TPR_FAIL;
                        }

                        if(FAILED(Pins[index]->QueryInterface( IID_IAMStreamConfig, (void**) &pStreamConfig)))
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Failed to retrieve the stream config interface."));
                            retval = TPR_FAIL;
                        }

                        if(FAILED(pStreamConfig->GetNumberOfCapabilities(&nFormatCount, &nSCCSize)))
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Failed to retrieve the number of stream capabilities."));
                            retval = TPR_FAIL;
                        }

                        if(nFormatCount == 0)
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Format count is 0, expected atleast one format to test."));
                            retval = TPR_FAIL;
                        }

                        if(nSCCSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Expected a VIDEO_STREAM_CONFIG_CAPS structure, but the size indicates otherwise."));
                            retval = TPR_FAIL;
                        }

                        // before we disconnect the graph, we want to find out what pin the vcap filter is connected to.
                        if(FAILED(Pins[index]->ConnectedTo(&ConnectedPin)))
                        {
                            LOG(TEXT("***PinFlowingGraphStateTests: Failed to determine the pin the pin under test was connected to."));
                            retval = TPR_FAIL;
                        }

                        for(int FormatIndex = 0; FormatIndex < nFormatCount; FormatIndex++)
                        {
                            LOG(TEXT("Testing format %d on pin %d"), FormatIndex, index);

                            if(FAILED(pMediaControl->Stop()))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to stop the filtergraph prior to changing the pin format."));
                                retval = TPR_FAIL;
                            }

                            // disconnect the source and sink, so they both know they're disconnected.
                            if(FAILED(Pins[index]->Disconnect()))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to disconnect the pin prior to changing the format."));
                                retval = TPR_FAIL;
                            }

                            if(FAILED(ConnectedPin->Disconnect()))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to disconnect the pin prior to changing the format."));
                                retval = TPR_FAIL;
                            }

                            // retrieve the new format from the source
                            if(FAILED(pStreamConfig->GetStreamCaps(FormatIndex, &pamt, (BYTE *) &vscc)))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to get the stream caps."));
                                retval = TPR_FAIL;
                            }

                            // set the format, reconnect, reinitialize the stream controls for the stream
                            // and then do the test.
                            if(FAILED(pStreamConfig->SetFormat(pamt)))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the pin format."));
                                retval = TPR_FAIL;
                            }

                            if(FAILED(Pins[index]->Connect(ConnectedPin, NULL)))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to reconnect the pin for the format change."));
                                retval = TPR_FAIL;
                            }

                            BITMAPINFOHEADER bih = {0};
                            REFERENCE_TIME rtAvgTimePerFrame = 0;
                            DWORD dwMillisecondsPerFrame = 33;
                            // retrieve the average time per frame, used later in verification.
                            if(IsEqualGUID(pamt->formattype, FORMAT_VideoInfo))
                            {
                                VIDEOINFOHEADER *pvh = (VIDEOINFOHEADER *) pamt->pbFormat;
                                rtAvgTimePerFrame = pvh->AvgTimePerFrame;
                                bih = pvh->bmiHeader;
                            }
                            else if(IsEqualGUID(pamt->formattype,FORMAT_VideoInfo2))
                            {
                                VIDEOINFOHEADER2 * pvh2 = (VIDEOINFOHEADER2 *) pamt->pbFormat;
                                rtAvgTimePerFrame = pvh2->AvgTimePerFrame;
                                bih = pvh2->bmiHeader;
                            }
                            else
                            {
                                LOG(TEXT("***formattype is not a VideoInfo or a VideoInfo2."));
                                retval = TPR_FAIL;
                            }

                            if(rtAvgTimePerFrame)
                                dwMillisecondsPerFrame = (DWORD) (rtAvgTimePerFrame / 10000);

                            // verify that the stream is properly stopped and nothing is flowing
                            // reset the verifier
                            pVerifier->Reset();

                            LOG(TEXT("PinFlowingGraphStateTests: running the graph. (0)"));
                            if(FAILED(pMediaControl->Run()))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to restart the filtergraph after changing the format."));
                                retval = TPR_FAIL;
                            }

                            hr = StreamControls[index]->StartAt(0, 0);
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial start time to 0 on pin %d."), index);
                                retval = TPR_FAIL;
                            }

                            REFERENCE_TIME rt = MAX_TIME;
                            
                            hr = StreamControls[index]->StopAt(&rt, FALSE, 0);
                            if(FAILED(hr))
                            {
                                LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                retval = TPR_FAIL;
                            }

                            for(int i = 0; i < 10; i++)
                            {
                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: pausing the graph. (1)"));

                                // pause
                                if(FAILED(pMediaControl->Pause()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to pause the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                // we were running, so check that everything is good
                                int nFramesFailed = pVerifier->GetSamplesFailed(index);
                                int nFramesSinked = pVerifier->GetSamplesSinked(index);

                                LOG(TEXT("PinFlowingTests: %d frames processed."), nFramesSinked);

                                if(nFramesSinked == 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: no frames sinked."));
                                    retval = TPR_FAIL;
                                }

                                if(nFramesFailed > 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                    retval = TPR_FAIL;
                                }

                                // we need to reset because we're going from run to pause to run, which
                                // results in the first sample being tagged as a discontinuity and the
                                // timestamps jumping.
                                pVerifier->Reset();

                                LOG(TEXT("PinFlowingGraphStateTests: running the graph.(2)"));

                                // run
                                if(FAILED(pMediaControl->Run()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to restart the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: stopping the graph.(3)"));

                                // stop
                                if(FAILED(pMediaControl->Stop()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to stop the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                nFramesFailed = pVerifier->GetSamplesFailed(index);
                                nFramesSinked = pVerifier->GetSamplesSinked(index);

                                LOG(TEXT("PinFlowingTests: %d frames processed."), nFramesSinked);

                                if(nFramesSinked == 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: no frames sinked."));
                                    retval = TPR_FAIL;
                                }

                                if(nFramesFailed > 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                pVerifier->Reset();

                                LOG(TEXT("PinFlowingGraphStateTests: running the graph.(4)"));

                                // run
                                if(FAILED(pMediaControl->Run()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to restart the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                hr = StreamControls[index]->StartAt(0, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial start time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                rt = MAX_TIME;
                                
                                hr = StreamControls[index]->StopAt(&rt, FALSE, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: pausing the graph.(5)"));

                                // pause
                                if(FAILED(pMediaControl->Pause()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to pause the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: stopping the graph.(6)"));

                                // stop
                                if(FAILED(pMediaControl->Stop()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to stop the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                nFramesFailed = pVerifier->GetSamplesFailed(index);
                                nFramesSinked = pVerifier->GetSamplesSinked(index);

                                LOG(TEXT("PinFlowingTests: %d frames processed."), nFramesSinked);

                                if(nFramesSinked == 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: no frames sinked."));
                                    retval = TPR_FAIL;
                                }

                                if(nFramesFailed > 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                    retval = TPR_FAIL;
                                }

                                pVerifier->Reset();

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: running the graph.(7)"));

                                // run
                                if(FAILED(pMediaControl->Run()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to restart the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                hr = StreamControls[index]->StartAt(0, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial start time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                rt = MAX_TIME;
                                
                                hr = StreamControls[index]->StopAt(&rt, FALSE, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: stopping the graph.(8)"));

                                // stop
                                if(FAILED(pMediaControl->Stop()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to stop the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                nFramesFailed = pVerifier->GetSamplesFailed(index);
                                nFramesSinked = pVerifier->GetSamplesSinked(index);

                                LOG(TEXT("PinFlowingTests: %d frames processed."), nFramesSinked);

                                if(nFramesSinked == 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: no frames sinked."));
                                    retval = TPR_FAIL;
                                }

                                if(nFramesFailed > 0)
                                {
                                    LOG(TEXT("***PinFlowingTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                    retval = TPR_FAIL;
                                }

                                pVerifier->Reset();

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: pausing the graph.(9)"));

                                // pause
                                if(FAILED(pMediaControl->Pause()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to pause the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                Sleep(10 * dwMillisecondsPerFrame);

                                LOG(TEXT("PinFlowingGraphStateTests: running the graph.(10)"));

                                // run
                                if(FAILED(pMediaControl->Run()))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to restart the filtergraph."));
                                    retval = TPR_FAIL;
                                }

                                hr = StreamControls[index]->StartAt(0, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial start time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }

                                rt = MAX_TIME;
                                
                                hr = StreamControls[index]->StopAt(&rt, FALSE, 0);
                                if(FAILED(hr))
                                {
                                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                    retval = TPR_FAIL;
                                }
                            }

                            if(pamt)
                                DeleteMediaType(pamt);
                        }

                        if(ConnectedPin)
                            ConnectedPin->Release();

                        if(pStreamConfig)
                            pStreamConfig->Release();
                    }
                }

                LOG(TEXT("PinFlowingGraphStateTests: stopping the graph.(11)"));
                if(FAILED(pMediaControl->Stop()))
                {
                    LOG(TEXT("***PinFlowingGraphStateTests: Failed to stop the filtergraph."));
                    retval = TPR_FAIL;
                }

                // release the pin interfaces allocated above, regardless of whether or not
                // initialization succeeded.
                // since the queries can stop at any point, 
                // make sure the interface was actually allocated
                // before dereferencing, this will handle the 
                // situation that initialization exited part way through.
                for(index = 0; index < PinCount; index++)
                {
                    if(StreamControls[index])
                        StreamControls[index]->Release();

                    if(Pins[index])
                        Pins[index]->Release();
                }

                pEnumPins->Release();
            }
        }

        // if any of the queries failed in the if/else block
        // then we need to make sure we clean them up outside of the main test block
        if(pVidCapTestSink)
            pVidCapTestSink->Release();
        if(pMediaSeeking)
            pMediaSeeking->Release();
        if(pGraphBuilder)
            pGraphBuilder->Release();
        if(pMediaControl)
            pMediaControl->Release();           
        if(pFilter)
            pFilter->Release();
        if(pTestSink)
            pTestSink->Release();
        if(pVideoControl)
            pVideoControl->Release();

        LOG(TEXT("PinFlowingGraphStateTests: deleting test graph."));
        delete pTestGraph;
    }

    return retval;
}


TESTPROCAPI StillPinTriggerTests( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();
    const int nVidCapPins = 3;
    DWORD retval = TPR_PASS;
    // the whole array is filled in with the last item used by definition, thus a single NULL
    // will intialize the whole array to NULL.
    IAMStreamControl * StreamControls[nVidCapPins] = {NULL};
    IPin *Pins[nVidCapPins] = {NULL};
    int PinCount = 0;
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;
    int index = 0;

    // Create an empty test graph
    LOG(TEXT("StillPinTriggerTests: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***StillPinTriggerTests: No pTestGraph given."));
        retval = TPR_FAIL;
    }
    else
    {
        // Ask for a filter - results in the filter being created and addreffed
        LOG(TEXT("StillPinTriggerTests: calling GetFilter."));

        IMediaControl* pMediaControl = NULL;
        IBaseFilter* pFilter = NULL;
        ITestSink* pTestSink = NULL;
        IVidCapTestSink* pVidCapTestSink = NULL;
        IGraphBuilder *pGraphBuilder = NULL;
        IAMVideoControl *pVideoControl = NULL;
        CVidCapVerifier *pVerifier = NULL;
        AM_STREAM_INFO amsi;

        memset(&amsi, 0, sizeof(AM_STREAM_INFO));

        if(FAILED(pTestGraph->ConnectGraph()))
        {
            LOG(TEXT("***StillPinTriggerTests: ConnectGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pGraphBuilder = pTestGraph->GetFilterGraphBuilder()))
        {
            LOG(TEXT("***StillPinTriggerTests: GetFilterGraphBuilder failed."));
            retval = TPR_FAIL;
        }
        else if(!(pMediaControl = pTestGraph->GetMediaControlGraph()))
        {
            LOG(TEXT("***StillPinTriggerTests: GetMediaControlGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pFilter= pTestGraph->GetFilter()))
        {
            LOG(TEXT("***StillPinTriggerTests: GetFilter failed."));
            retval = TPR_FAIL;
        }
        else if(!(pTestSink= pTestGraph->GetTestSink()))
        {
            LOG(TEXT("***StillPinTriggerTests: GetSinkFilter failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pTestSink->QueryInterface(IID_IVidCapTestSink, (void **) &pVidCapTestSink)))
        {
            LOG(TEXT("***StillPinTriggerTests: Failed to query for the vidcap test sink"));
            retval = TPR_FAIL;
        }
        // necessary in order to retrieve the verifier as a video capture verifier, which it always will be.
        else if(!(pVerifier = pVidCapTestSink->GetVidCapVerifier()))
        {
            LOG(TEXT("***StillPinTriggerTests: GetVerifier failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pFilter->QueryInterface(IID_IAMVideoControl, (void **) &pVideoControl)))
        {
            LOG(TEXT("***StillPinTriggerTests: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else
        {
            if(FAILED(hr = pFilter->EnumPins(&pEnumPins)))
            {
               LOG(TEXT("***StillPinTriggerTests: Failed to enumerate the pins."));
               retval = TPR_FAIL;
            }
            else
            {
                // enumerate the pin, putting the interface into the array if it succeeds.
                // on success, increment the pin count to indicate one full and then get the next pin interface.
                // when pin count is 3, then we've retrieved all of the pins
                while (retval == TPR_PASS && PinCount < nVidCapPins && ((hr = pEnumPins->Next(1, &Pins[PinCount], 0)) == S_OK))
                {
                    hr = Pins[PinCount]->QueryInterface(IID_IAMStreamControl, (void **) &StreamControls[PinCount]);
                    if(FAILED(hr))
                    {
                       LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the stream control interfaces."));
                       retval = TPR_FAIL;
                    }

                    // always increment the pin count regardless of whether or not the interface was allocated,
                    // otherwise we could leak the interface.
                    PinCount++;
                }

                if(retval != TPR_PASS)
                {
                   LOG(TEXT("***StillPinTriggerTests: interface allocation failed, unable to continue."));
                }
                else if(PinCount < 2)
                {
                   LOG(TEXT("***StillPinTriggerTests: Incorrect number of pins on the capture filter, unable to continue."));
                   retval = TPR_FAIL;
                }
                else if(FAILED(pMediaControl->Run()))
                {
                    LOG(TEXT("***StillPinTriggerTests: Failed to run the filtergraph."));
                    retval = TPR_FAIL;
                }
                else
                {
                    // success, we have the interfaces, and everything is ready to go for the test.
                    IKsPropertySet *pKs = NULL;
                    GUID pinCategory = {0};
                    DWORD cbReturned = 0;

                    // initialize the stream controls
                    for(index = 0; index < PinCount; index++)
                    {
                        if(SUCCEEDED( Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)))
                        {
                            if(SUCCEEDED( pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                            {
                                if(!IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                                {
                                     hr = StreamControls[index]->StartAt(0, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to set the initial start time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    hr = StreamControls[index]->StopAt(0, FALSE, 0);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to set the initial stop time to 0 on pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    // retrieve the AM_STREAM_INFO, and verify contents.
                                    hr = StreamControls[index]->GetInfo(&amsi);
                                    if(FAILED(hr))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the stream info for pin %d."), index);
                                        retval = TPR_FAIL;
                                    }

                                    if(!CheckStreamInfo(&amsi, TRUE, NULL, 0, TRUE, NULL, 0, FALSE, FALSE))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: verifying the stream info for pin %d failed when doing stream initialization."), index);
                                        retval = TPR_FAIL;
                                    }
                                }
                            }
                            pKs->Release();
                        }
                    }

                    for(index = 0; index < PinCount; index++)
                    {
                        BOOL IsStillPin = FALSE;
                        BOOL IsPreviewPin = FALSE;
                        BOOL IsCapturePin = FALSE;
                        AM_MEDIA_TYPE * pamt = NULL;
                        IAMStreamConfig *pStreamConfig = NULL;
                        int nFormatCount = 0;
                        int nSCCSize = 0;
                        VIDEO_STREAM_CONFIG_CAPS vscc;
                        IPin *ConnectedPin = NULL;

                        memset(&vscc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS));

                        if(SUCCEEDED( Pins[index]->QueryInterface( IID_IKsPropertySet, (void**) &pKs)))
                        {
                            if(SUCCEEDED( pKs->Get( AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &pinCategory, sizeof( GUID ), &cbReturned )))
                            {
                                if(IsEqualGUID(pinCategory, PIN_CATEGORY_STILL))
                                {
                                    LOG(TEXT("StillPinTriggerTests: Pin under test is the still pin."));
                                    IsStillPin = TRUE;

                                    if(FAILED(Pins[index]->QueryInterface( IID_IAMStreamConfig, (void**) &pStreamConfig)))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the stream config interface."));
                                        retval = TPR_FAIL;
                                    }

                                    if(FAILED(pStreamConfig->GetNumberOfCapabilities(&nFormatCount, &nSCCSize)))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the number of stream capabilities."));
                                        retval = TPR_FAIL;
                                    }

                                    if(nFormatCount == 0)
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Format count is 0, expected atleast one format to test."));
                                        retval = TPR_FAIL;
                                    }

                                    if(nSCCSize != sizeof(VIDEO_STREAM_CONFIG_CAPS))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Expected a VIDEO_STREAM_CONFIG_CAPS structure, but the size indicates otherwise."));
                                        retval = TPR_FAIL;
                                    }

                                    // before we disconnect the graph, we want to find out what pin the vcap filter is connected to.
                                    if(FAILED(Pins[index]->ConnectedTo(&ConnectedPin)))
                                    {
                                        LOG(TEXT("***StillPinTriggerTests: Failed to determine the pin the pin under test was connected to."));
                                        retval = TPR_FAIL;
                                    }

                                    for(int FormatIndex = 0; FormatIndex < nFormatCount; FormatIndex++)
                                    {
                                        LOG(TEXT("Testing format %d on pin %d"), FormatIndex, index);

                                        if(FAILED(pMediaControl->Stop()))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to stop the filtergraph prior to changing the pin format."));
                                            retval = TPR_FAIL;
                                        }

                                        // disconnect the source and sink, so they both know they're disconnected.
                                        if(FAILED(Pins[index]->Disconnect()))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to disconnect the pin prior to changing the format."));
                                            retval = TPR_FAIL;
                                        }

                                        if(FAILED(ConnectedPin->Disconnect()))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to disconnect the pin prior to changing the format."));
                                            retval = TPR_FAIL;
                                        }

                                        // retrieve the new format from the source
                                        if(FAILED(pStreamConfig->GetStreamCaps(FormatIndex, &pamt, (BYTE *) &vscc)))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to get the stream caps."));
                                            retval = TPR_FAIL;
                                        }

                                        // set the format, reconnect, reinitialize the stream controls for the stream
                                        // and then do the test.
                                        if(FAILED(pStreamConfig->SetFormat(pamt)))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to set the pin format."));
                                            retval = TPR_FAIL;
                                        }

                                        if(FAILED(Pins[index]->Connect(ConnectedPin, NULL)))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to reconnect the pin for the format change."));
                                            retval = TPR_FAIL;
                                        }

                                        if(FAILED(pMediaControl->Run()))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to restart the filtergraph after changing the format."));
                                            retval = TPR_FAIL;
                                        }

                                        // retrieve the AM_STREAM_INFO, and verify contents.
                                        hr = StreamControls[index]->GetInfo(&amsi);
                                        if(FAILED(hr))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the stream info for pin %d."), index);
                                            retval = TPR_FAIL;
                                        }

                                        if(!CheckStreamInfo(&amsi, TRUE, NULL, 0, TRUE, NULL, 0, FALSE, FALSE))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: verifying the stream info for pin %d failed when doing stream initialization."), index);
                                            retval = TPR_FAIL;
                                        }

                                        if(!pamt->bFixedSizeSamples)
                                        {
                                            LOG(TEXT("***MediaType indicates a non fixed sized sample"));
                                            retval = TPR_FAIL;
                                        }

                                        if(pamt->bTemporalCompression)
                                        {
                                            LOG(TEXT("***MediaType indicates temporal compression."));
                                            retval = TPR_FAIL;
                                        }


                                        if(pamt->lSampleSize == 0)
                                        {
                                            LOG(TEXT("***MediaType indicates a sample size of 0."));
                                            retval = TPR_FAIL;
                                        }

                                        BITMAPINFOHEADER bih = {0};
                                        REFERENCE_TIME rtAvgTimePerFrame = 0;
                                        // retrieve the average time per frame, used later in verification.
                                        if(IsEqualGUID(pamt->formattype, FORMAT_VideoInfo))
                                        {
                                            VIDEOINFOHEADER *pvh = (VIDEOINFOHEADER *) pamt->pbFormat;
                                            rtAvgTimePerFrame =  pvh->AvgTimePerFrame;

                                            bih = pvh->bmiHeader;
                                        }
                                        else if(IsEqualGUID(pamt->formattype,FORMAT_VideoInfo2))
                                        {
                                            VIDEOINFOHEADER2 * pvh2 = (VIDEOINFOHEADER2 *) pamt->pbFormat;
                                            rtAvgTimePerFrame = pvh2->AvgTimePerFrame;

                                            bih = pvh2->bmiHeader;
                                        }
                                        else
                                        {
                                            LOG(TEXT("***formattype is not a VideoInfo or a VideoInfo2."));
                                            retval = TPR_FAIL;
                                        }

                                        if(rtAvgTimePerFrame == 0)
                                        {
                                            LOG(TEXT("***Average time per frame is 0"));
                                            retval = TPR_FAIL;
                                        }

                                        if(bih.biSize != sizeof(BITMAPINFOHEADER))
                                        {
                                            LOG(TEXT("***Structure size not set in the BITMAPINFOHEADER."));
                                            retval = TPR_FAIL;
                                        }


                                        if(bih.biWidth == 0 || bih.biHeight == 0)
                                        {
                                            LOG(TEXT("***sample width and height are 0 in the BITMAPINFOHEADER."));
                                            retval = TPR_FAIL;
                                        }

                                        // if this is an RGB format, then enforce the bit depth and number of planes
                                        if((bih.biCompression & !BI_SRCPREROTATE) == BI_RGB)
                                        {
                                            if(bih.biPlanes != 1)
                                            {
                                                LOG(TEXT("***BITMAPINFOHEADER planes is not 1, only 1 is supported."));
                                                retval = TPR_FAIL;
                                            }

                                            if(bih.biBitCount != 1 && bih.biBitCount != 2 &&
                                                bih.biBitCount != 4 &&bih.biBitCount != 8 &&
                                                bih.biBitCount != 16 && bih.biBitCount != 24 &&
                                                bih.biBitCount != 32)
                                            {
                                                LOG(TEXT("***BITMAPINFOHEADER bit depth %d is invalid."), bih.biBitCount);
                                                retval = TPR_FAIL;
                                            }
                                        }

                                        // check GetCaps
                                        long CapsFlags;
                                        if(FAILED(pVideoControl->GetCaps(Pins[index], &CapsFlags)))
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Failed to retrieve the device caps."));
                                            retval = TPR_FAIL;
                                        }

                                        if(CapsFlags & ~(VideoControlFlag_Sample_Scanned_Notification | VideoControlFlag_Trigger | VideoControlFlag_ExternalTriggerEnable) )
                                        {
                                            LOG(TEXT("***StillPinTriggerTests: Still pin capability flags incorrect, expect trigger, external trigger enable, and potentially sample scanned notification support, recieved 0x%08x."), CapsFlags);
                                            retval = TPR_FAIL;
                                        }

                                        // let the driver get running completely, so 
                                        // it's ready to deliver the first image when it's time.
                                        Sleep(2000);

                                        for(int i = 0; i < 10; i++)
                                        {
                                            // verify that the stream is properly stopped and nothing is flowing
                                            // reset the verifier
                                            pVerifier->Reset();

                                            // capture a frame
                                            hr = pVideoControl->SetMode( Pins[index], VideoControlFlag_Trigger );
                                            if(FAILED(hr))
                                            {
                                                LOG(TEXT("***StillPinTriggerTests: Failed to trigger the still image."));
                                                retval = TPR_FAIL;
                                            }

                                            // wait 1.5 seconds, this isn't a perf test, we
                                            // just want to confirm the driver sends exactly 1 sample, 
                                            // no more, no less. at normal frame rates, 1.5 seconds should be
                                            // enough for driver setup and 3-30 frames.
                                            Sleep(1500);

                                            // check the number of frames processed.
                                            int nFramesFailed = pVerifier->GetSamplesFailed(index);
                                            int nFramesSinked = pVerifier->GetSamplesSinked(index);

                                            LOG(TEXT("StillPinTriggerTests: %d frames processed."), nFramesSinked);

                                            if(nFramesSinked != 1)
                                            {
                                                LOG(TEXT("***StillPinTriggerTests: Incorrect number of frames sinked, expected %d, recieved %d."), 1, nFramesSinked);
                                                retval = TPR_FAIL;
                                            }

                                            if(nFramesFailed > 0)
                                            {
                                                LOG(TEXT("***StillPinTriggerTests: %d frames failed in the vidcap verifier."), nFramesFailed);
                                                retval = TPR_FAIL;
                                            }
                                        }
                                        if(pamt)
                                            DeleteMediaType(pamt);
                                    }

                                    if(ConnectedPin)
                                        ConnectedPin->Release();

                                    if(pStreamConfig)
                                        pStreamConfig->Release();
                                }
                            }
                            pKs->Release();
                        }
                    }
                }
            }

            // stop the graph (even if we didn't successfully start it
            if(FAILED(pMediaControl->Stop()))
            {
                LOG(TEXT("***StillPinTriggerTests: Failed to stop the filtergraph."));
                retval = TPR_FAIL;
            }

            // release the pin interfaces allocated above, regardless of whether or not
            // initialization succeeded.
            // since the queries can stop at any point, 
            // make sure the interface was actually allocated
            // before dereferencing, this will handle the 
            // situation that initialization exited part way through.
            for(index = 0; index < PinCount; index++)
            {
                if(StreamControls[index])
                    StreamControls[index]->Release();

                if(Pins[index])
                    Pins[index]->Release();
            }

            pEnumPins->Release();
        }

        // if any of the queries failed in the if/else block
        // then we need to make sure we clean them up outside of the main test block
        if(pVidCapTestSink)
            pVidCapTestSink->Release();
        if(pVideoControl)
            pVideoControl->Release();
        if(pGraphBuilder)
            pGraphBuilder->Release();
        if(pMediaControl)
            pMediaControl->Release();           
        if(pFilter)
            pFilter->Release();
        if(pTestSink)
            pTestSink->Release();

        LOG(TEXT("StillPinTriggerTests: deleting test graph."));
        delete pTestGraph;
    }

    return retval;
}

TESTPROCAPI VCapMetadataTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Create an empty test graph
    LOG(TEXT("VCapMetadataTest: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if(!pTestGraph)
    {
        LOG(TEXT("***VCapMetadataTest: No pTestGraph given."));
        retval = TPR_FAIL;
    }
    else
    {
        // Ask for a filter - results in the filter being created and addreffed
        LOG(TEXT("VCapMetadataTest: calling GetFilter."));

        IBaseFilter* pFilter = NULL;
        IGraphBuilder *pGraphBuilder = NULL;
        ICaptureMetadata *pVideoCaptureMetadata = NULL;

        if(FAILED(pTestGraph->ConnectGraph()))
        {
            LOG(TEXT("***VCapMetadataTest: ConnectGraph failed."));
            retval = TPR_FAIL;
        }
        else if(!(pGraphBuilder = pTestGraph->GetFilterGraphBuilder()))
        {
            LOG(TEXT("***VCapMetadataTest: GetFilterGraphBuilder failed."));
            retval = TPR_FAIL;
        }
        else if(!(pFilter= pTestGraph->GetFilter()))
        {
            LOG(TEXT("***VCapMetadataTest: GetFilter failed."));
            retval = TPR_FAIL;
        }
        else if(FAILED(pFilter->QueryInterface(IID_ICaptureMetadata, (void **) &pVideoCaptureMetadata)))
        {
            LOG(TEXT("***VCapMetadataTest: Retrieving the media seeking interface failed."));
            retval = TPR_FAIL;
        }
        else
        {
            PMetadataBuffer Buffer = NULL;
            DWORD dwExpectedSize = 0;
            DWORD dwSize = 0;

            // first, standard bad parameter tests, verify it fails if both params are NULL
            if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(NULL, NULL)))
            {
                LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(NULL, NULL) succeeded. 0x%08x"), hr);
                retval = TPR_FAIL;
            }

            // pointer param should be irrelevent if size is bogus
            if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(NULL, (PMetadataBuffer) 0xBAD)))
            {
                LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(NULL, 0Xbad) succeeded. 0x%08x"), hr);
                retval = TPR_FAIL;
            }

            // if the retrieve size is 0, then the pointer param is irrelevent also, but give it some space just in case it writes something there.
            dwSize = 0;
            dwExpectedSize = 0;
            if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, (PMetadataBuffer) &dwExpectedSize)))
            {
                LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(NULL, NULL) succeeded. 0x%08x"), hr);
                retval = TPR_FAIL;
            }

            if(dwExpectedSize != 0)
            {
                LOG(TEXT("***VCapMetadataTest: GetAllPropertyItems modified the metadata buffer when given a requested fill in size of 0"));
                retval = TPR_FAIL;
            }

            if(FAILED(hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwExpectedSize, NULL)) || 0 == dwExpectedSize)
            {
                LOG(TEXT("***VCapMetadataTest: retrieved the expected property item size failed, feature not supported on the capture driver."));
            }
            else
            {
                // allocate a full buffer, plus a bit. Put some check bits before the pointer passed to dshow, 
                // after, and in the middle.
                Buffer = (PMetadataBuffer) new BYTE[dwExpectedSize + 2*sizeof(DWORD)];
                memset(Buffer, 0, dwExpectedSize + 2*sizeof(DWORD));
                Buffer = (PMetadataBuffer) (((DWORD *) Buffer) + 1);
                *(((DWORD *) Buffer) -1) = 0xDDDDDDDD;
                // 0 to dwExpectedSize - 1, so the sentinal is at dwExpectedSize exactly.
                *((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) = 0xDDDDDDDD;

                //////////////////////////////////////////
                // check that it fails retrieving 1 byte
                dwSize = 1;
                *(((BYTE *) Buffer) + dwSize) = 0xDD;
                
                if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, Buffer)))
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(1, pBuffer) succeeded. 0x%08x"), hr);
                    retval = TPR_FAIL;
                }

                // check the check bits.
                if(*(((BYTE *) Buffer) + dwSize) != 0xDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(1, pBuffer) modified the memory buffer"));
                    retval = TPR_FAIL;
                }

                if(*(((DWORD *) Buffer) - 1) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(1, pBuffer) buffer underrun"));
                    retval = TPR_FAIL;
                }

                if(*((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(1, pBuffer) buffer overrun"));
                    retval = TPR_FAIL;
                }


                //////////////////////////////////////////////////////////////
                // check bits in the middle for the retrieve half way test
                dwSize = dwExpectedSize/2;
                *(((BYTE *) Buffer) + dwSize) = 0xDD;
                
                if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, Buffer)))
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size/2, pBuffer) succeeded. 0x%08x"), hr);
                    retval = TPR_FAIL;
                }

                // check the check bits.
                if(*(((BYTE *) Buffer) + dwSize) != 0xDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size/2, pBuffer) modified the memory buffer"));
                    retval = TPR_FAIL;
                }

                if(*(((DWORD *) Buffer) -1) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size/2, pBuffer) buffer underrun"));
                    retval = TPR_FAIL;
                }

                if(*((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size/2, pBuffer) buffer overrun"));
                    retval = TPR_FAIL;
                }

                /////////////////////////////////////////////////////////////////////
                // check that it fails if retrieving 1 less than the expected size
                dwSize = dwExpectedSize - 1;
                *(((BYTE *) Buffer) + dwSize) = 0xDD;
                
                if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, Buffer)))
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size - 1, pBuffer) succeeded. 0x%08x"), hr);
                    retval = TPR_FAIL;
                }

                // check the check bits.
                if(*(((BYTE *) Buffer) + dwSize) != 0xDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size - 1, pBuffer) modified the memory buffer"));
                    retval = TPR_FAIL;
                }

                if(*(((DWORD *) Buffer) - 1) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size - 1, pBuffer) buffer underrun"));
                    retval = TPR_FAIL;
                }

                if(*((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems(size - 1, pBuffer) buffer overrun"));
                    retval = TPR_FAIL;
                }

                ////////////////////////////////////////////////////////////////////////////////////////
                // Now retrieve everything, make sure it doesn't overrun or underrun the buffer
                dwSize = dwExpectedSize;
                memset(Buffer, 0, dwExpectedSize);
                Buffer->dwCount = MAXDWORD;
                if(S_OK != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, Buffer)))
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems failed when expected to succeed. 0x%08x"), hr);
                    retval = TPR_FAIL;
                }

                if(dwSize != dwExpectedSize)
                {
                    LOG(TEXT("***VCapMetadataTest: size of returned property items (%d) doesn't match original reported size (%d)"), dwSize, dwExpectedSize);
                    retval = TPR_FAIL;
                }

                if(*(((DWORD *) Buffer) - 1) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems buffer underrun"));
                    retval = TPR_FAIL;
                }

                if(*((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems buffer overrun"));
                    retval = TPR_FAIL;
                }

                if(dwSize < Buffer->dwCount * sizeof(PropertyItem) + sizeof(DWORD))
                {
                    LOG(TEXT("***VCapMetadataTest: retrieved item count invalid or size incorrect, buffer size %d is smaller than count * property item size + sizeof 1 dword (%d)"), dwSize, Buffer->dwCount * sizeof(PropertyItem) + sizeof(DWORD));
                    retval = TPR_FAIL;
                }

                //////////////////////////////////////////////////////////////////////////////////////
                // tell dshow the buffer is bigger than it needs (it really isn't though), confirm
                // that it doesn't overrun or underrun the buffer.
                dwSize = dwExpectedSize + 10;
                memset(Buffer, 0, dwExpectedSize);
                Buffer->dwCount = MAXDWORD;
                if(S_OK != (hr = pVideoCaptureMetadata->GetAllPropertyItems(&dwSize, Buffer)))
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems failed when expected to succeed. 0x%08x"), hr);
                    retval = TPR_FAIL;
                }

                if(dwSize != dwExpectedSize)
                {
                    LOG(TEXT("***VCapMetadataTest: size of returned property items (%d) doesn't match original reported size (%d)"), dwSize, dwExpectedSize);
                    retval = TPR_FAIL;
                }

                // check the leading and trailing check bits.
                if(*(((DWORD *) Buffer) -1) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems buffer underrun"));
                    retval = TPR_FAIL;
                }

                if(*((DWORD *) (((BYTE *) Buffer) + dwExpectedSize)) != 0xDDDDDDDD)
                {
                    LOG(TEXT("***VCapMetadataTest: ICaptureMetadata::GetAllPropertyItems buffer overrun"));
                    retval = TPR_FAIL;
                }

                if(dwSize < Buffer->dwCount * sizeof(PropertyItem) + sizeof(DWORD))
                {
                    LOG(TEXT("***VCapMetadataTest: retrieved item count invalid or size incorrect, buffer size %d is smaller than count * property item size + sizeof 1 dword (%d)"), dwSize, Buffer->dwCount * sizeof(PropertyItem) + sizeof(DWORD));
                    retval = TPR_FAIL;
                }

                Buffer = (PMetadataBuffer) (((DWORD *) Buffer) -1);
                delete[] Buffer;
            }
        }

        // if any of the queries failed in the if/else block
        // then we need to make sure we clean them up outside of the main test block
        if(pVideoCaptureMetadata)
            pVideoCaptureMetadata->Release();
        if(pGraphBuilder)
            pGraphBuilder->Release();
        if(pFilter)
            pFilter->Release();

        LOG(TEXT("VCapMetadataTest: deleting test graph."));
        delete pTestGraph;
    }

    return retval;
}

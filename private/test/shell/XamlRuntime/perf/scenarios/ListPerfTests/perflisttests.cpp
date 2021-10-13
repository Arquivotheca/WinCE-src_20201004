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

//******************************************************************************
//***** Includes
//******************************************************************************
#include "PerfListTests.h"
#include "Globals.h"
#include "ResManager.h"
#include <GSEHelper2.h>
#include <GestureWnd2.h>
#include <GetGuidUtil.h>

#define FLICK_DOWN  90
#define FLICK_UP   270

//////////////////////////////////////////////////////////////////////////
// Set the ExitTest flag and kill the timer when the time is up
//////////////////////////////////////////////////////////////////////////
VOID CALLBACK TimerProc_EndTest(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    EnterCriticalSection(&g_cs); // the g_bExitThread is not thread-safe, need to take precaution
    g_bExitTest = true;
    LeaveCriticalSection(&g_cs);

    if (KillTimer(hwnd, idEvent))
    {
        g_pKato->Log(LOG_COMMENT, TEXT("TimerProc_EndTest::gTimer successfully destroyed."));
    }
}

VOID CALLBACK TimerProc_ListScrollPropBag(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
    HRESULT hr = E_FAIL;
    IXRPropertyBagPtr pValue = NULL;
    int Count = 0;
    IXRItemCollectionPtr pCollection;

    CHK_HR( g_eventHandler.WaitForLayoutEvent(), L"Initial WaitForLayoutEvent", g_dwRet ); //Initial display of the listbox

    CHK_PTR(g_pListBox, L"IXRListBox", g_dwRet);
    CHK_HR(g_pListBox->GetItems(&pCollection), L"GetItems", g_dwRet);
    CHK_DO(pCollection, L"IXRItemCollection", g_dwRet);
    CHK_HR(pCollection->GetCount(&Count), L"GetCount()", g_dwRet );

    CHK_HR(pCollection->GetItem(g_CurrentIndex, &pValue), L"GetItem", g_dwRet);
    CHK_PTR(pValue, L"pValue", g_dwRet);
    CHK_HR(g_pListBox->ScrollIntoView(pValue), L"ScrollIntoView", g_dwRet);

    //figure out if we're going up or down:
    if (g_bFirstRep)
    {
        g_bFirstRep = false;
    }
    else if (g_CurrentIndex + 50 >= Count)
    {
        g_bGoingUp = true;
    }
    else if (!g_bFirstRep && (g_CurrentIndex - 50 <= 0))
    {
        g_bGoingUp = false;
    }

    if (g_bGoingUp)
    {
        g_CurrentIndex -= 50;
    }
    else
    {
        g_CurrentIndex += 50;
    }

    g_dwRet = TPR_PASS;

Exit:

    EnterCriticalSection(&g_cs);
    int temp = g_bExitTest;
    LeaveCriticalSection(&g_cs);

    if (temp || (g_dwRet != TPR_PASS))
    {
        if (KillTimer(hwnd, idEvent))
        {
            g_pKato->Log(LOG_COMMENT, L"TimerProc_ListScrollPropBag::gTimer successfully destroyed.");
        }

        g_pKato->Log(LOG_COMMENT, L"Test finished", g_Reps);

        g_pHost->EndDialog(0);
    }
}


VOID CALLBACK TimerProc_ListScrollBSTR(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
    HRESULT hr = E_FAIL;
    ce::auto_bstr pValue;
    int Count = 0;
    IXRItemCollectionPtr pCollection;

    CHK_HR( g_eventHandler.WaitForLayoutEvent(), L"Initial WaitForLayoutEvent", g_dwRet ); //Initial display of the listbox

    CHK_PTR(g_pListBox, L"IXRListBox", g_dwRet);
    CHK_HR(g_pListBox->GetItems(&pCollection), L"GetItems", g_dwRet);
    CHK_DO(pCollection, L"IXRItemCollection", g_dwRet);
    CHK_HR(pCollection->GetCount(&Count), L"GetCount()", g_dwRet );

    CHK_HR(pCollection->GetItem(g_CurrentIndex, &pValue), L"GetItem", g_dwRet);
    CHK_PTR(pValue, L"pValue", g_dwRet);
    CHK_HR(g_pListBox->ScrollIntoView(pValue), L"ScrollIntoView", g_dwRet);

    //figure out if we're going up or down:
    if (g_bFirstRep)
    {
        g_bFirstRep = false;
    }
    else if (g_CurrentIndex + 50 >= Count)
    {
        g_bGoingUp = true;
    }
    else if (!g_bFirstRep && (g_CurrentIndex - 50 <= 0))
    {
        g_bGoingUp = false;
    }

    if (g_bGoingUp)
    {
        g_CurrentIndex -= 50;
    }
    else
    {
        g_CurrentIndex += 50;
    }

    g_dwRet = TPR_PASS;

Exit:

    EnterCriticalSection(&g_cs);
    int temp = g_bExitTest;
    LeaveCriticalSection(&g_cs);

    if (temp || (g_dwRet != TPR_PASS))
    {
        if (KillTimer(hwnd, idEvent))
        {
            g_pKato->Log(LOG_COMMENT, L"TimerProc_ListScrollPropBag::gTimer successfully destroyed.");
        }

        g_pKato->Log(LOG_COMMENT, L"Test finished", g_Reps);

        g_pHost->EndDialog(0);
    }
}

////////////////////////////////////////////////////////////////////////////////
//
//  RunScrollingPerfTest
//
//   Brings up an Xamlruntime window, loads the target xaml and runs scrolling tests
//
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI RunScrollingPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    TUX_EXECUTE;
    LPCTSTR lpTestDescription = lpFTE->lpDescription;
    int TestNumber  = lpFTE->dwUserData;
    DWORD dwRet     = TPR_FAIL;
    HRESULT hr      = S_OK;
    HWND HostHWND   = NULL;

    WCHAR szNamespace[MAX_PATH];
    HANDLE hThread = NULL;
    ResourceHandler resource;
    bool bLoadImagesFromFiles = false;
 
    XRWindowCreateParams    WindowParameters;
    WCHAR szName[MAX_PATH] = {0};
    IXRDependencyObjectPtr pDO;
    IXRFrameworkElementPtr  pRootElement;
    IXRGridPtr pGrid;
    XRPtr<IXRDelegate<XREventArgs>> pDelegateLayoutUpdated;
    WCHAR szFillMethod[MAX_PATH] = {0};
    WCHAR szVirtualizationMode[MAX_PATH] = {0};
    WCHAR szOperation[MAX_PATH] = {0};
    WCHAR szXamlFileName[SMALL_STRING_LEN] = {0};
    XRXamlSource XamlSource;
    int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int WindowTitleHeight = 25;

    WCHAR szResultXMLFileName[MAX_PATH] = {0};

    bool bPerfScenarioOpened = false;

    int NumItems = 1;
    int BamlId = 0;
    GUID scenarioGUID = {0};

    //init the globals for this test
    g_dwRet = TPR_FAIL;
    g_dwImageRet = TPR_PASS;

    CHK_HR( PerfScenarioOpenSession( XRPERF_API_PATH, true ), L"PerfScenarioOpenSession", dwRet );
    bPerfScenarioOpened = true;

    CHK_HR(Getint(L"NumItems", &NumItems), L"GetInt NumItems", dwRet); // retrieve value from xml file

    hr = Getbool(L"LoadImagesFromFiles", &bLoadImagesFromFiles);    //optional, defaults to false
    if( hr != S_OK )
    {
        g_pKato->Log(LOG_COMMENT, L"LoadImagesFromFiles parameter is optional, defaults to false" );         
    }

    resource.SetLoadImagesFromFiles( bLoadImagesFromFiles );

    if( bLoadImagesFromFiles )
    {   //Make sure the files have been created
        CHK_BOOL( CreateImageCopies( NumItems ), L"Error creating image copies", dwRet );
    }

    for (DWORD Repeats = 0; Repeats < g_OptionRepeats; Repeats++)
    {
        // setup the default window (don't specify a size - let it come from XAML)
        //
        ZeroMemory(&WindowParameters, sizeof(WindowParameters));
        WindowParameters.Style       = WS_POPUP | WS_SYSMENU | WS_CAPTION;  //creates a window frame with a Close button
        WindowParameters.pTitle      = lpTestDescription;
        WindowParameters.Left        = 0;
        WindowParameters.Top         = 0;
        WindowParameters.pHookProc   = HostCallBackHook;


        CHK_HR(g_pTestApplication->RegisterResourceManager(&resource), L"RegisterResourceManager", dwRet);
        CHK_HR( g_eventHandler.CreateLayoutEventHandle(), L"CreateLayoutEventHandle", dwRet );

        if (g_UseBaml)
        {
            CHK_HR(Getint(L"baml", &BamlId), L"Getint BamlId", dwRet); // retrieve value from xml file

            XamlSource.SetResource(g_hInstance, BamlId);
        }
        else
        {
            // Get the XAML file name from the xml file
            //
            CHK_HR( GetWCHAR(L"xaml", szXamlFileName), L"GetWCHAR szXamlFileName", dwRet );

            if( wcsnicmp( L"vsp\\VSP_ListboxDB.xaml", szXamlFileName, SMALL_STRING_LEN ) == 0 )
            {
                XamlSource.SetResource(g_hInstance, ID_XAML_VSP_ListboxDB);
            }
            else if( wcsnicmp( L"vsp\\VSP_listboxDBTemplate.xaml", szXamlFileName, SMALL_STRING_LEN ) == 0 )
            {
                XamlSource.SetResource(g_hInstance, ID_XAML_VSP_VSP_listboxDBTemplate);
            }
            else if( wcsnicmp( L"vsp\\VSP_listboxDBTemplateImageOnly.xaml", szXamlFileName, SMALL_STRING_LEN ) == 0 )
            {
                XamlSource.SetResource(g_hInstance, ID_XAML_VSP_VSP_listboxDBTemplateImageOnly);
            }
            else
            {
                //invalid xaml file
                g_pKato->Log(LOG_COMMENT, L"Invalid xaml file name:%s", szXamlFileName );
                goto Exit;
            }
        }

        // Create the Visual Host that corresponds to the XAML file implementation
        hr = g_pTestApplication->CreateHostFromXaml(&XamlSource, &WindowParameters, &g_pHost);
        if (FAILED(hr) || (!g_pHost))
        {
            g_pKato->Log(LOG_COMMENT, L"Failed to create visual host with 0x%x result.", hr);
            goto Exit;
        }

        g_pHost->SetSize( ScreenWidth, ScreenHeight-WindowTitleHeight );

        //did all the images get loaded?
        if( g_dwImageRet != TPR_PASS )
        {   //This only happens if the ResManager.h failed to load a file
            g_pKato->Log(LOG_COMMENT, L"Failed to load all the images in the xaml");
            dwRet = g_dwImageRet;
            goto Exit;
        }

        CHK_HR(GetTargetElementName(szName, MAX_PATH), L"GetTargetElementName", dwRet); // retrieve object name from xml file
        CHK_HR(GetTargetElement(g_pHost, szName, &pDO), L"GetTargetElement", dwRet); // FindName
        CHK_DO(pDO, L"IXRDependencyObjectPtr pDO", dwRet);
        CHK_HR(pDO->QueryInterface(IID_IXRListBox, (void**)&g_pListBox), L"QueryInterface", dwRet);
        CHK_DO(g_pListBox, L"IXRListBox", dwRet);

        CHK_HR( g_pListBox->SetHeight( (float)ScreenHeight - 2.0f * WindowTitleHeight ), L"SetHeight", dwRet ); 

        CHK_HR(GetWCHAR(L"FillMethod", szFillMethod), L"GetWCHAR szFillMethod", dwRet); // retrieve value from xml file
        CHK_HR(GetWCHAR(L"Mode", szVirtualizationMode), L"GetWCHAR szVirtualizationMode", dwRet); // retrieve value from xml file

        hr = GetWCHAR(L"Operation", szOperation);  //Optional parameter
        if( hr != S_OK )
        {
            g_pKato->Log(LOG_COMMENT, L"Operation parameter is optional, defaults to Thumb" );         
        }

        //Set the virtualization mode
        if(wcscmp(szVirtualizationMode, L"Standard")==0)
        {
            CHK_HR(g_pListBox->SetAttachedProperty(L"VirtualizingStackPanel.VirtualizationMode",NULL,XRVirtualizationMode_Standard), L"SetAttachedProperty(VirtualizationMode)", dwRet );
        }
        else if(wcscmp(szVirtualizationMode, L"Recycling")==0)
        {
            CHK_HR(g_pListBox->SetAttachedProperty(L"VirtualizingStackPanel.VirtualizationMode",NULL,XRVirtualizationMode_Recycling), L"SetAttachedProperty(VirtualizationMode)", dwRet );
        }

        //Fill the listbox with items
        if(wcscmp(szFillMethod, L"DataBindingSimple")==0)
        {
            InitListBoxDataBindingStringCollection( g_pListBox, NumItems );
        }
        else if(wcscmp(szFillMethod, L"DataBindingTemplate")==0)
        {
            InitListBoxDataBindingTemplate( g_pListBox, NumItems, bLoadImagesFromFiles );
        }
        else if(wcscmp(szFillMethod, L"DataBindingTemplateImageOnly")==0)
        {
            InitListBoxDataBindingTemplateImageOnly( g_pListBox, NumItems );
        }

        if (TC_GESTURE_END >= TestNumber)
        {
            if( g_bManualMode == false )
            {
                hThread =  CreateThread(NULL, 0, RunGestureThreadProc, g_pHost, 0, 0); 
                if (!hThread)
                {
                    // Error starting thread.
                    g_pKato->Log(LOG_COMMENT, L"Error starting monitoring thread");
                    goto Exit;      //Cause test to fail
                }
            }
        }
        else if (TC_SCROLLING_END >= TestNumber)
        {
            //provide for the tests exit:
            g_bExitTest = false;
            InitializeCriticalSection(&g_cs);

            if (SetTimer(HostHWND, g_TimerID, g_OptionSeconds*1000, TimerProc_EndTest) == 0)
            {
                g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
                goto Exit;
            }

            //run the test
            if (21 == TestNumber)
            {
                if (SetTimer(HostHWND, 100, 20, TimerProc_ListScrollBSTR) == 0)
                {
                    g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
                    goto Exit;
                }
            }
            else
            {
                if (SetTimer(HostHWND, 100, 20, TimerProc_ListScrollPropBag) == 0)
                {
                    g_pKato->Log(LOG_COMMENT, L"SetTimer Failed");
                    goto Exit;
                }
            }
        }

        //The tests wait on this event to control their timing
        CHK_HR(CreateDelegate(&g_eventHandler, &EventHandler::OnLayoutUpdatedHandler, &pDelegateLayoutUpdated), L"CreateDelegate(OnLayoutUpdatedHandler)", dwRet);
        CHK_PTR(pDelegateLayoutUpdated, L"pDelegateLayoutUpdated", dwRet);
        CHK_HR(g_pListBox->AddLayoutUpdatedEventHandler(pDelegateLayoutUpdated), L"AddLayoutUpdatedEventHandler", dwRet);

        // show a modal dialog
        //
        CHK_HR(g_pHost->StartDialog( NULL), L"StartDialog Error", dwRet);
        CHK_HR(g_eventHandler.CloseLayoutEventHandle(), L"CloseLayoutEventHandle", dwRet);
        CHK_HR( g_pListBox->RemoveLayoutUpdatedEventHandler(pDelegateLayoutUpdated), L"RemoveLayoutUpdatedEventHandler", dwRet );

        CleanupGlobals();
        if( hThread )
        {
            CloseHandle( hThread );
            hThread = NULL;
        }

        //did all the images get loaded during runtime?
        if( g_dwImageRet != TPR_PASS )
        {   //This only happens if the ResManager.h failed to load a file
            g_pKato->Log(LOG_COMMENT, L"Failed to load all the images in the xaml");
            dwRet = g_dwImageRet;
            goto Exit;
        }

        //did the test pass?
        if (g_dwRet != TPR_PASS)
        {
            dwRet = g_dwRet;
            g_pKato->Log(LOG_COMMENT, L"Test run failed");
            goto Exit;
        }
    } //Repeats

    StringCchCopy( szResultXMLFileName, MAX_PATH, g_bRunningUnderCTK?L"\\" XRPERFLISTRESULTSFILE : RELEASE_DIR XRPERFLISTRESULTSFILE );
    g_pKato->Log(LOG_COMMENT, L"Using output file: %s", szResultXMLFileName );

    //Make a copy of lpTestDescription and append (baml) if necessary
    StringCchCopy( szName, MAX_PATH, lpTestDescription );
    if( g_UseBaml )
    {
        StringCchCat( szName, MAX_PATH, L" (baml)" );
    }

    StringCchPrintfW( szNamespace, MAX_PATH, XR_TESTNAME_FORMAT, szName );
    scenarioGUID = GetGUID( szNamespace );

    CHK_HR( PerfScenarioFlushMetrics(
        false,
        &scenarioGUID,          // (Scenario GUID) Unique GUID for the Xamlruntime performance tests
        szNamespace,            // Namespace
        szName,                 // Session, can include \ to nest: _T("Normal\\Composited")
        szResultXMLFileName,    // Result XML 
        NULL,                   // Input XML file regarding CPU usage
        NULL                    // (Instance GUID) Unique GUID for the Xamlruntime performance tests
        )
        , L"PerfScenarioFlushMetrics", dwRet );

    dwRet = TPR_PASS;

Exit:

    if( bPerfScenarioOpened == true )
    {
        hr = PerfScenarioCloseSession(XRPERF_API_PATH);
        if( FAILED(hr) )
        {
            g_pKato->Log(LOG_COMMENT, L"PerfScenarioCloseSession failed with 0x%x result.", hr);
        }
    }

    if( bLoadImagesFromFiles )
    {   //Make sure the files have been cleaned up
        if( !DeleteImageCopies( NumItems ) )
        {
            g_pKato->Log(LOG_COMMENT, L"Error deleting image copies");
        }
    }

    g_eventHandler.CloseLayoutEventHandle();

    if( g_pListBox )
    {
        g_pListBox->RemoveLayoutUpdatedEventHandler(pDelegateLayoutUpdated);
    }

    CleanupGlobals();
    if( hThread )
    {
        CloseHandle( hThread );
        hThread = NULL;
    }

    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//
//  CleanupGlobals
//
//   releases the global handles
//
////////////////////////////////////////////////////////////////////////////////
void CleanupGlobals()
{
    if( g_TimerID )
    {
        KillTimer( 0, g_TimerID );
        g_TimerID = 0;
    }

    g_pTestApplication->RegisterResourceManager(NULL);  //releases the resource manager
    SAFE_RELEASE(g_pListBox);
    SAFE_RELEASE(g_pHost);
}


////////////////////////////////////////////////////////////////////////////////
//
//  HostCallBackHook
//
//   Closes the Xamlruntime window when the user clicks the X on the window frame
//
////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK HostCallBackHook(VOID* UserParam, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* ReturnValue)
{
    if( message == WM_CLOSE )
    {
        if( g_pHost )
        {
            g_pHost->EndDialog(0);
            *ReturnValue = 0;
            g_dwRet = TPR_PASS;
            return true;    //tells Xamlruntime the message has been handled
        }
    }
    return false;   //lets Xamlruntime process the message
}

//Panning 50% of the list over 3 seconds. If we do more than 50% or go faster then it will trigger a Flick.
#define msForPanGesture 3000
#define MY_MAX_PATH 25

//////////////////////////////////////////////////////////////////////////
// Runs the tests while the window is open and running, then closes the window
//////////////////////////////////////////////////////////////////////////
DWORD WINAPI RunGestureThreadProc (LPVOID lpParameter)
{
    HRESULT hr                      = E_FAIL;
    HWND NativeHWND                 = NULL;
    WCHAR szString[MY_MAX_PATH]     = {0};
    float fWidth                    = 0.0;
    float fHeight                   = 0.0;
    int i                           = 0;
    int FlickCount                  = 0;
    int NumEvents                   = 0;
    GSEHelper* pGSE                 = new GSEHelper();
    GestureDataPoint gdp[5];
    XRPoint PtTarget;
    IXRDependencyObjectPtr pDO;
    IXRFrameworkElementPtr pFrameworkElement;

    g_pHost->GetContainerHWND( &NativeHWND );
    CHK_HR(GetTargetElementName(szString,MY_MAX_PATH),L"GetTargetElementName",g_dwRet);
    CHK_HR(GetTargetElement(g_pHost,szString,&pDO), L"GetTargetElement", g_dwRet);
    CHK_DO(pDO, L"IXRDependencyObject", g_dwRet);
    pFrameworkElement = pDO;
    CHK_DO(pFrameworkElement, L"IXRFrameworkElement", g_dwRet);

    CHK_HR(GetWCHAR(L"Operation", szString), L"GetWCHAR Operation", g_dwRet); // retrieve value from xml file

    CHK_HR( g_eventHandler.WaitForLayoutEvent(), L"Initial WaitForLayoutEvent", g_dwRet ); //Initial display of the listbox

    //Get the coordinates of the target element
    CHK_HR(pFrameworkElement->GetActualWidth(&fWidth), L"GetWidth", g_dwRet);
    CHK_HR(pFrameworkElement->GetActualHeight(&fHeight), L"GetHeight", g_dwRet);
    CHK_HR(pFrameworkElement->GetActualX(&PtTarget.x), L"Get Actual X Position", g_dwRet);
    CHK_HR(pFrameworkElement->GetActualY(&PtTarget.y), L"Get Actual Y Position", g_dwRet);

    //Get the middle POINT of the application window - for gestures
    gdp[0].point.x = (int)PtTarget.x + (int)fWidth/2;
    gdp[0].point.y = (int)PtTarget.y + (int)fHeight/2;

    CHK_PTR(pGSE, L"GSEHelper", g_dwRet);

    if( wcscmp(szString, L"GesturePAN")==0 )
    {
        NumEvents = g_OptionSeconds/(msForPanGesture*2/1000);       // *2 because we have both pan up and pan down events.  /1000 because msForPanGesture is in ms but g_OptionSeconds is in seconds
        //Note:Pan expects at least 2 data point structures per move!
        gdp[0].timeBetweenCurrAndNextPoint = msForPanGesture; 
        gdp[1].timeBetweenCurrAndNextPoint = 0;
        gdp[0].point.y = (int)PtTarget.y + (int)(fHeight*0.6);      //Scrolling farther or faster than halfway in 3 seconds will trigger a small flick
        gdp[1].point.x = gdp[0].point.x;
        gdp[1].point.y = (int)PtTarget.y + (int)(fHeight*0.1);

        for( i = 0; i < NumEvents; i++ )
        {
            CHK_HR(pGSE->DoGesture(GESTURETYPE_PAN, 2, gdp), L"DoGesture", g_dwRet);
        }

        //Now PAN the list back up
        gdp[0].point.y = (int)PtTarget.y + (int)(fHeight*0.1);
        gdp[1].point.y = (int)PtTarget.y + (int)(fHeight*0.6);       //Scrolling farther or faster than halfway in 3 seconds will trigger a small flick

        for( i = 0; i < NumEvents; i++ )
        {
            CHK_HR(pGSE->DoGesture(GESTURETYPE_PAN, 2, gdp), L"DoGesture", g_dwRet);
        }
    }
    else
    {       //GestureFLICK
        NumEvents = g_OptionSeconds/5;          //Sleeping 5 seconds between flicks

        //Note: Flick expects a single data point structure per move!
        gdp[0].lParam2 = MAKELONG(0, 3);                         //Creates a flick of 3200ms
        for( i = 0; i < NumEvents; i++ )
        {
            //The list is 60,000 pixels long, each flick moves the list about 5500 pixels.
            //10 flicks in a row gets us close to the bottom.
            if( FlickCount < 10 )               
                gdp[0].lParam1 = FLICK_DOWN;                     //Flick Down
            else
                gdp[0].lParam1 = FLICK_UP;                       //Flick up

            CHK_HR(pGSE->DoGesture(GESTURETYPE_FLICK, 1, gdp), L"DoGesture", g_dwRet);
            FlickCount++;
            if( FlickCount >= 20 )
                FlickCount = 0;

            //Note: The FPS performance counter only runs while an animation is running. So gaps between flicks won't change the results.
            //      However, cutting off and animation before it finishes will cause it to jump to the end and distort the FPS count.
            Sleep(5000);        //ListBox does not block on FLICK animation
        }
    }

    g_dwRet = TPR_PASS;

Exit:
    if( pGSE )
    {
        delete pGSE;
    }

    g_pHost->EndDialog(0);
    return 0;
}


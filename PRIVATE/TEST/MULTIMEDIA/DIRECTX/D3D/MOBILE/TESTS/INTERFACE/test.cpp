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
#include <tchar.h>
#include <katoutils.h>
#include <clparse.h>   // Command-line parsing utility
#include "ft.h"

#include "TestWindow.h"
#include "Initializer.h"

#include "IDirect3DMobile.h"
#include "IDirect3DMobileDevice.h"
#include "IDirect3DMobileIndexBuffer.h"
#include "IDirect3DMobileSurface.h"
#include "IDirect3DMobileTexture.h"
#include "IDirect3DMobileVertexBuffer.h"
#include "IDirect3DMobileSwapChain.h"

#include "DebugOutput.h"

//Avoid C4100
#define UNUSED_PARAM(_a) _a

HANDLE g_hInstance = NULL;
 
//
// Storage for test case argument string field:
//
//      * TEST_CASE_ARGS.pwchSoftwareDeviceFilename
//

static WCHAR g_pwchSoftwareDeviceFilename[MAX_PATH]; // Unused in case of D3DMADAPTER_DEFAULT ( pwchSoftwareDeviceFilename

//
// Any cmd-line argument value string that exceeds 100 characters
// will be truncated.
//
#define D3DQA_MAX_OPT_LEN 100

CKato *g_pKato;
//
// To be updated based on command-line arguments
//
static TEST_CASE_ARGS g_TestCaseArgs =
{
    0,                            // UINT uiAdapterOrdinal;
    g_pwchSoftwareDeviceFilename, // PWCHAR pwchSoftwareDeviceFilename;     
    NULL,                         // PWCHAR pwchImageComparitorDir;
    NULL,                         // PWCHAR pwchTestDependenciesDir;        
};
static BOOL g_bDebug = FALSE;                        // Run debug middleware tests?



BOOL ParseCommonCmdLineArgs(LPTEST_CASE_ARGS pTestCaseArgs, BOOL* pbDebug);

//
// DllMain
//
//   Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Return Value:
// 
//   BOOL
//
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
	UNUSED_PARAM(hInstance);
	UNUSED_PARAM(dwReason);
	UNUSED_PARAM(lpReserved);

	g_hInstance = hInstance;

    return TRUE;
}

//
// ShellProc
//
//   Processes messages from the TUX shell.
//
// Arguments:
//
//   uMsg            Message code.
//   spParam         Additional message-dependent data.
//
// Return value:
//
//   Depends on the message.
//
SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // This message is sent once to the DLL immediately after it is loaded.
        // The ShellProc must set the fUnicode member of this structure to TRUE
		// if the DLL was built with the UNICODE flag set.
        DebugOut(L"ShellProc(SPM_LOAD_DLL, ...) called");

#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
		g_pKato = (CKato*)KatoGetDefaultObject();
		SetKato(g_pKato);
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        DebugOut(L"ShellProc(SPM_UNLOAD_DLL, ...) called");
        SetKato(NULL);
        break;

    case SPM_SHELL_INFO:
        // This message is sent once to the DLL immediately following the
		// SPM_LOAD_DLL message to give the DLL information about its parent shell,
		// via SPS_SHELL_INFO.
        DebugOut(L"ShellProc(SPM_SHELL_INFO, ...) called");

        // Store a pointer to our shell info, in case it is useful later on
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
		if (FALSE == ParseCommonCmdLineArgs(&g_TestCaseArgs,             // LPTEST_CASE_ARGS pTestCaseArgs
		                                    &g_bDebug))                  // BOOL* pbDebug
		{
		    DebugOut(DO_ERROR, L"Command line parsing failed. Skipping.");
			return SPR_FAIL;  // Cmd-line-arg parser requested a halt of test execution
		}
        break;

    case SPM_REGISTER:
        // This message is sent to query the DLL for its function table. 
        DebugOut(L"ShellProc(SPM_REGISTER, ...) called");
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // This message is sent to the DLL immediately before a script is started
        DebugOut(L"ShellProc(SPM_START_SCRIPT, ...) called");
        break;

    case SPM_STOP_SCRIPT:
        // This message is sent to the DLL after the script has stopped.
        DebugOut(L"ShellProc(SPM_STOP_SCRIPT, ...) called");
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow.
        DebugOut(L"ShellProc(SPM_BEGIN_GROUP, ...) called");
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXTEST.DLL"));
		break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        DebugOut(L"ShellProc(SPM_END_GROUP, ...) called");
        g_pKato->EndLevel(TEXT("END GROUP: TUXTEST.DLL"));
		break;

    case SPM_BEGIN_TEST:
        // This message is sent to the DLL before a single test or group of
		// tests from that DLL is executed. 
       
		DebugOut(L"ShellProc(SPM_BEGIN_TEST, ...) called");
        // Start our logging level.
        pBT = (LPSPS_BEGIN_TEST)spParam;
        g_pKato->BeginLevel(
            pBT->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
            pBT->lpFTE->lpDescription,
            pBT->dwThreadCount,
            pBT->dwRandomSeed);
		break;

    case SPM_END_TEST:
        // This message is sent to the DLL after a single test case from
		// the DLL executes. 
		
        DebugOut(L"ShellProc(SPM_END_TEST, ...) called");
        // End our logging level.
        pET = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(
            TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
		break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        DebugOut(L"ShellProc(SPM_EXCEPTION, ...) called");
        g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
		break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}


//
// ParseCommonCmdLineArgs
//
//   Parse cmd-line args that are consistent for all tests.  Args that are specific
//   to one particular test case are handled in the specific TESTPROCAPI that defines
//   that case.
//
// Arguments:
//  
// Return Value:
//
//  BOOL:  FALSE indicates that execution should be aborted (for example, if usage
//         was requested), TRUE indicates that execution should continue
//
BOOL ParseCommonCmdLineArgs(LPTEST_CASE_ARGS pTestCaseArgs, BOOL* pbDebug)
{
	//
	// Temporary storage for use with cmd-line parser
	//
	WCHAR wszArg[D3DQA_MAX_OPT_LEN];

	//
	// Arg validation
	//
	if ((NULL == pTestCaseArgs) || (NULL == pbDebug))
	{
		DebugOut(DO_ERROR, L"ParseCommonCmdLineArgs aborting due to invalid arguments.");
		return false;
	}

	//
	// Command-line parsing
	//
	if ((NULL == g_pShellInfo) || (NULL == g_pShellInfo->szDllCmdLine))
	{
		DebugOut(DO_ERROR, L"No command-line arguments specified.  Using defaults.");
		return false;
	}
	else
	{
		CClParse CmdLineParse(g_pShellInfo->szDllCmdLine);

		//
		// If the "-?" command-line argument exists, show the usage information,
		// and skip the test.
		//
		if (CmdLineParse.GetOpt(_T("?")))
		{
			g_pKato->Log(LOG_COMMENT,_T(""));
			g_pKato->Log(LOG_COMMENT,_T("Description:"));
			g_pKato->Log(LOG_COMMENT,_T("\tDirect3D Mobile interface tests."));
			g_pKato->Log(LOG_COMMENT,_T("\t"));
			g_pKato->Log(LOG_COMMENT,_T("Sample command line:"));
			g_pKato->Log(LOG_COMMENT,_T("\t"));
			g_pKato->Log(LOG_COMMENT,_T("\ts tux -o -d D3DM_Interface.dll -x 1"));
			g_pKato->Log(LOG_COMMENT,_T("\t"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t/s: DLL filename of software device to register (if present test loads the DLL, calls RegisterSoftwareDevice,"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t    and creates the device with D3DMADAPTER_REGISTERED_DEVICE; otherwise, the test uses the system's default"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t    adapter by creating the device with D3DMADAPTER_DEFAULT)."));
			g_pKato->Log(LOG_COMMENT,_T("\t"));
			g_pKato->Log(LOG_COMMENT,_T("\t\t/d: If specified; this indicates that the underlying middleware is a debug build"));
			g_pKato->Log(LOG_COMMENT,_T("\t"));

			//
			// Abort execution; user merely wanted to see the usage information
			//
			return FALSE;
		}

		//
		// Register a software device, or use default adapter?
		//
		if (CmdLineParse.GetOptString(_T("s"), wszArg, D3DQA_MAX_OPT_LEN))
		{
			StringCchCopy(pTestCaseArgs->pwchSoftwareDeviceFilename, D3DQA_FILELENGTH,  wszArg);
			pTestCaseArgs->uiAdapterOrdinal = D3DMADAPTER_REGISTERED_DEVICE;
		}
		else // default adapter
		{
			pTestCaseArgs->pwchSoftwareDeviceFilename = NULL;
			pTestCaseArgs->uiAdapterOrdinal = D3DMADAPTER_DEFAULT;
		}

        //
        // Include tests for debug middleware?
        //
        if (CmdLineParse.GetOpt(_T("d")))
        {
            *pbDebug = TRUE;
        }
	}

	return TRUE;
}

//
// TuxTest
// 
//   This TESTPROCAPI is the sole one for all test case ordinals in this binary.  Very little
//   test-specific code is contained herein.  This test merely branches to the correct
//   member function for the specific test ordinal.
//
// Arguments:
//
//   (As defined by tux interface)
//
// Return Value:
//
//  TPR_PASS, TPR_FAIL, or TPR_SKIP, depending on test result
//
TESTPROCAPI TuxTest(UINT uTuxMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HRESULT hr;
	ULONG ulTestOrdinal;                        // Identifies specific test case to be executed

	//
	// This test does not have any particular requirements
	// regarding window configuration/extents
	//
	WINDOW_ARGS WindowArgs = 
	{
		GetSystemMetrics(SM_CXSCREEN) / 2, // UINT uiWindowWidth;
		GetSystemMetrics(SM_CYSCREEN) / 2, // UINT uiWindowHeight;		
		WS_OVERLAPPED,                 // DWORD uiWindowStyle;
		g_lpClassName,                 // LPCTSTR lpClassName;
		g_lpWindowName,                // LPCTSTR lpWindowName;
		TRUE                           // BOOL bPParmsWindowed;
	};

	if(uTuxMsg == TPM_QUERY_THREAD_COUNT)
    {
		//
		// Set default number of threads to 1
		//
		((TPS_QUERY_THREAD_COUNT*)(tpParam))->dwThreadCount = 1;
		return TPR_HANDLED;
    }
    else if(uTuxMsg == TPM_EXECUTE)
	{
		if (NULL == lpFTE)
		{
			DebugOut(DO_ERROR, L"Invalid function table entry; Aborting.");
			return TPR_ABORT;
		}

		ulTestOrdinal = lpFTE->dwUniqueID;

		//
		// Pick a test class to instantiate; call a member therein
		//
		if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILE_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILE_MAX))
		{

			IDirect3DMobileTest OBJTest;
			if (FAILED(hr = OBJTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                        &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                        g_bDebug,         // BOOL bDebug
			                        ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDeviceTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_OBJ_QUERYINTERFACE:
				return OBJTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_OBJ_ADDREF:                     
				return OBJTest.ExecuteAddRefTest();
				break;
			case D3DQAID_OBJ_RELEASE:                    
				return OBJTest.ExecuteReleaseTest();
				break;
			case D3DQAID_OBJ_REGISTERSOFTWAREDEVICE:     
				return OBJTest.ExecuteRegisterSoftwareDeviceTest();
				break;
			case D3DQAID_OBJ_GETADAPTERCOUNT:            
				return OBJTest.ExecuteGetAdapterCountTest();
				break;
			case D3DQAID_OBJ_GETADAPTERIDENTIFIER:       
				return OBJTest.ExecuteGetAdapterIdentifierTest();
				break;
			case D3DQAID_OBJ_GETADAPTERMODECOUNT:        
				return OBJTest.ExecuteGetAdapterModeCountTest();
				break;
			case D3DQAID_OBJ_ENUMADAPTERMODES:           
				return OBJTest.ExecuteEnumAdapterModesTest();
				break;
			case D3DQAID_OBJ_GETADAPTERDISPLAYMODE:      
				return OBJTest.ExecuteGetAdapterDisplayModeTest();
				break;
			case D3DQAID_OBJ_CHECKDEVICETYPE:            
				return OBJTest.ExecuteCheckDeviceTypeTest();
				break;
			case D3DQAID_OBJ_CHECKDEVICEFORMAT:          
				return OBJTest.ExecuteCheckDeviceFormatTest();
				break;
			case D3DQAID_OBJ_CHECKDEVICEMULTISAMPLETYPE: 
				return OBJTest.ExecuteCheckDeviceMultiSampleTypeTest();
				break;
			case D3DQAID_OBJ_CHECKDEPTHSTENCILMATCH:     
				return OBJTest.ExecuteCheckDepthStencilMatchTest();
				break;
			case D3DQAID_OBJ_CHECKPROFILE:               
				return OBJTest.ExecuteCheckProfileTest();
				break;
			case D3DQAID_OBJ_GETDEVICECAPS:              
				return OBJTest.ExecuteGetDeviceCapsTest();
				break;
			case D3DQAID_OBJ_CREATEDEVICE:            
				return OBJTest.ExecuteCreateDeviceTest();
				break;
			case D3DQAID_OBJ_CHECKDEVICEFORMATCONVERSION:            
				return OBJTest.ExecuteCheckDeviceFormatConversionTest();
				break;
            case D3DQAID_OBJ_CHECKD3DMREFNOTINCLUDED:
                return OBJTest.ExecuteCheckD3DMRefNotIncludedTest();
                break;
		    case D3DQAID_OBJ_MULTIPLEINSTANCES:
		        return OBJTest.ExecuteMultipleInstancesTest(&WindowArgs, (HINSTANCE)g_hInstance, g_pwchSoftwareDeviceFilename, ulTestOrdinal);
		        break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}


		} else
		if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILEDEVICE_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILEDEVICE_MAX))
		{

			IDirect3DMobileDeviceTest DeviceTest;
			if (FAILED(hr = DeviceTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                           &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                           g_bDebug,         // BOOL bDebug
			                           ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDeviceTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_DEV_QUERYINTERFACE:
				return DeviceTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_DEV_TESTCOOPERATIVELEVEL:
				return DeviceTest.ExecuteDeviceTestCooperativeLevelTest();
				break;
			case D3DQAID_DEV_GETAVAILABLEPOOLMEM:
				return DeviceTest.ExecuteGetAvailablePoolMemTest();
				break;
			case D3DQAID_DEV_RESOURCEMANAGERDISCARDBYTES:
				return DeviceTest.ExecuteResourceManagerDiscardBytesTest();
				break;
			case D3DQAID_DEV_GETDIRECT3D:
				return DeviceTest.ExecuteGetDirect3DTest();
				break;
			case D3DQAID_DEV_GETDEVICECAPS:
				return DeviceTest.ExecuteGetDeviceCapsTest();
				break;
			case D3DQAID_DEV_GETDISPLAYMODE:
				return DeviceTest.ExecuteGetDisplayModeTest();
				break;
			case D3DQAID_DEV_GETCREATIONPARAMETERS:
				return DeviceTest.ExecuteGetCreationParametersTest();
				break;
			case D3DQAID_DEV_CREATEADDITIONALSWAPCHAIN:
				return DeviceTest.ExecuteCreateAdditionalSwapChainTest();
				break;
			case D3DQAID_DEV_RESET:
				return DeviceTest.ExecuteResetTest();
				break;
			case D3DQAID_DEV_PRESENT:                  
				return DeviceTest.ExecutePresentTest();
				break;
			case D3DQAID_DEV_GETBACKBUFFER:	        
				return DeviceTest.ExecuteGetBackBufferTest();
				break;
			case D3DQAID_DEV_CREATETEXTURE:            
				return DeviceTest.ExecuteCreateTextureTest();
				break;
			case D3DQAID_DEV_CREATEVERTEXBUFFER:       
				return DeviceTest.ExecuteCreateVertexBufferTest();
				break;
			case D3DQAID_DEV_CREATEINDEXBUFFER:        
				return DeviceTest.ExecuteCreateIndexBufferTest();
				break;
			case D3DQAID_DEV_CREATERENDERTARGET:       
				return DeviceTest.ExecuteCreateRenderTargetTest();
				break;
			case D3DQAID_DEV_CREATEDEPTHSTENCILSURFACE:
				return DeviceTest.ExecuteCreateDepthStencilSurfaceTest();
				break;
			case D3DQAID_DEV_CREATEIMAGESURFACE:       
				return DeviceTest.ExecuteCreateImageSurfaceTest();
				break;
			case D3DQAID_DEV_COPYRECTS:                
				return DeviceTest.ExecuteCopyRectsTest();
				break;
			case D3DQAID_DEV_UPDATETEXTURE:            
				return DeviceTest.ExecuteUpdateTextureTest();
				break;
			case D3DQAID_DEV_GETFRONTBUFFER:           
				return DeviceTest.ExecuteGetFrontBufferTest();
				break;
			case D3DQAID_DEV_SETRENDERTARGET:          
				return DeviceTest.ExecuteSetRenderTargetTest();
				break;
			case D3DQAID_DEV_GETRENDERTARGET:          
				return DeviceTest.ExecuteGetRenderTargetTest();
				break;
			case D3DQAID_DEV_GETDEPTHSTENCILSURFACE:   
				return DeviceTest.ExecuteGetDepthStencilSurfaceTest();
				break;
			case D3DQAID_DEV_BEGINSCENE:               
				return DeviceTest.ExecuteBeginSceneTest();
				break;
			case D3DQAID_DEV_ENDSCENE:                 
				return DeviceTest.ExecuteEndSceneTest();
				break;
			case D3DQAID_DEV_CLEAR:                    
				return DeviceTest.ExecuteClearTest();
				break;
			case D3DQAID_DEV_SETTRANSFORM:             
				return DeviceTest.ExecuteSetTransformTest();
				break;
			case D3DQAID_DEV_GETTRANSFORM:             
				return DeviceTest.ExecuteGetTransformTest();
				break;
			case D3DQAID_DEV_SETVIEWPORT:              
				return DeviceTest.ExecuteSetViewportTest();
				break;
			case D3DQAID_DEV_GETVIEWPORT:              
				return DeviceTest.ExecuteGetViewportTest();
				break;
			case D3DQAID_DEV_SETMATERIAL:              
				return DeviceTest.ExecuteSetMaterialTest();
				break;
			case D3DQAID_DEV_GETMATERIAL:              
				return DeviceTest.ExecuteGetMaterialTest();
				break;
			case D3DQAID_DEV_SETLIGHT:                 
				return DeviceTest.ExecuteSetLightTest();
				break;
			case D3DQAID_DEV_GETLIGHT:                 
				return DeviceTest.ExecuteGetLightTest();
				break;
			case D3DQAID_DEV_LIGHTENABLE:              
				return DeviceTest.ExecuteLightEnableTest();
				break;
			case D3DQAID_DEV_GETLIGHTENABLE:           
				return DeviceTest.ExecuteGetLightEnableTest();
				break;
			case D3DQAID_DEV_SETRENDERSTATE:           
				return DeviceTest.ExecuteSetRenderStateTest();
				break;
			case D3DQAID_DEV_GETRENDERSTATE:           
				return DeviceTest.ExecuteGetRenderStateTest();
				break;
			case D3DQAID_DEV_SETCLIPSTATUS:            
				return DeviceTest.ExecuteSetClipStatusTest();
				break;
			case D3DQAID_DEV_GETCLIPSTATUS:            
				return DeviceTest.ExecuteGetClipStatusTest();
				break;
			case D3DQAID_DEV_GETTEXTURE:               
				return DeviceTest.ExecuteGetTextureTest();
				break;
			case D3DQAID_DEV_SETTEXTURE:               
				return DeviceTest.ExecuteSetTextureTest();
				break;
			case D3DQAID_DEV_GETTEXTURESTAGESTATE:     
				return DeviceTest.ExecuteGetTextureStageStateTest();
				break;
			case D3DQAID_DEV_SETTEXTURESTAGESTATE:     
				return DeviceTest.ExecuteSetTextureStageStateTest();
				break;
			case D3DQAID_DEV_VALIDATEDEVICE:           
				return DeviceTest.ExecuteValidateDeviceTest();
				break;
			case D3DQAID_DEV_GETINFO:                  
				return DeviceTest.ExecuteGetInfoTest();
				break;
			case D3DQAID_DEV_SETPALETTEENTRIES:        
				return DeviceTest.ExecuteSetPaletteEntriesTest();
				break;
			case D3DQAID_DEV_GETPALETTEENTRIES:        
				return DeviceTest.ExecuteGetPaletteEntriesTest();
				break;
			case D3DQAID_DEV_SETCURRENTTEXTUREPALETTE: 
				return DeviceTest.ExecuteSetCurrentTexturePaletteTest();
				break;
			case D3DQAID_DEV_GETCURRENTTEXTUREPALETTE: 
				return DeviceTest.ExecuteGetCurrentTexturePaletteTest();
				break;
			case D3DQAID_DEV_DRAWPRIMITIVE:            
				return DeviceTest.ExecuteDrawPrimitiveTest();
				break;
			case D3DQAID_DEV_DRAWINDEXEDPRIMITIVE:     
				return DeviceTest.ExecuteDrawIndexedPrimitiveTest();
				break;
			case D3DQAID_DEV_PROCESSVERTICES:          
				return DeviceTest.ExecuteProcessVerticesTest();
				break;
			case D3DQAID_DEV_SETSTREAMSOURCE:          
				return DeviceTest.ExecuteSetStreamSourceTest();
				break;
			case D3DQAID_DEV_GETSTREAMSOURCE:          
				return DeviceTest.ExecuteGetStreamSourceTest();
				break;
			case D3DQAID_DEV_SETINDICES:               
				return DeviceTest.ExecuteSetIndicesTest();
				break;
			case D3DQAID_DEV_GETINDICES:               
				return DeviceTest.ExecuteGetIndicesTest();
				break;
			case D3DQAID_DEV_STRETCHRECT:              
				return DeviceTest.ExecuteStretchRectTest();
				break;
			case D3DQAID_DEV_COLORFILL:
				return DeviceTest.ExecuteColorFillTest();
				break;
			// Misc tests:
			case D3DQAID_DEV_VERIFYAUTOMIPLEVELEXTENTS:
				return DeviceTest.ExecuteVerifyAutoMipLevelExtentsTest();
				break;
			case D3DQAID_DEV_PRESENTATIONINTERVAL:
				return DeviceTest.ExecuteDevicePresentationIntervalTest();
				break;
			case D3DQAID_DEV_SWAPEFFECTMULTISAMPLEMISMATCH:
				return DeviceTest.ExecuteSwapEffectMultiSampleMismatchTest();
				break;
			case D3DQAID_DEV_PRESENTFLAGMULTISAMPLEMISMATCH:
				return DeviceTest.ExecuteAttemptPresentFlagMultiSampleMismatchTest();
				break;
			case D3DQAID_DEV_VERIFYBACKBUFLIMITSWAPEFFECTCOPY:
				return DeviceTest.ExecuteVerifyBackBufLimitSwapEffectCopyTest();
				break;
			case D3DQAID_DEV_SWAPCHAINMANIPDURINGRESET:
				return DeviceTest.ExecuteSwapChainManipDuringResetTest();
				break;
			case D3DQAID_DEV_COMMANDBUFREFCOUNTING:
				return DeviceTest.ExecuteInterfaceCommandBufferRefTest();
				break;
			case D3DQAID_DEV_OBJECTCREATE:
				return DeviceTest.ExecuteDirect3DMobileCreateTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}

		} else
		if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILEINDEXBUFFER_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILEINDEXBUFFER_MAX))
		{

			IndexBufferTest IBTest;
			if (FAILED(hr = IBTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                       &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                       g_bDebug,         // BOOL bDebug
			                       ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"IndexBufferTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_IB_QUERYINTERFACE:
				return IBTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_IB_ADDREF:
				return IBTest.ExecuteAddRefTest();
				break;
			case D3DQAID_IB_RELEASE:
				return IBTest.ExecuteReleaseTest();
				break;
			case D3DQAID_IB_GETDEVICE:
				return IBTest.ExecuteGetDeviceTest();
				break;
			case D3DQAID_IB_SETPRIORITY:
				return IBTest.ExecuteSetPriorityTest();
				break;
			case D3DQAID_IB_GETPRIORITY:
				return IBTest.ExecuteGetPriorityTest();
				break;
			case D3DQAID_IB_PRELOAD:
				return IBTest.ExecutePreLoadTest();
				break;
			case D3DQAID_IB_GETTYPE:
				return IBTest.ExecuteGetTypeTest();
				break;
			case D3DQAID_IB_LOCK:
				return IBTest.ExecuteLockTest();
				break;
			case D3DQAID_IB_UNLOCK:
				return IBTest.ExecuteUnlockTest();
				break;
			case D3DQAID_IB_GETDESC:
				return IBTest.ExecuteGetDescTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}
		} else
		if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILESURFACE_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILESURFACE_MAX))
		{

			SurfaceTest SurfTest;
			if (FAILED(hr = SurfTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                         &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                         g_bDebug,         // BOOL bDebug
			                         ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"SurfaceTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_SRF_QUERYINTERFACE:
				return SurfTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_SRF_ADDREF:
				return SurfTest.ExecuteAddRefTest();
				break;
			case D3DQAID_SRF_RELEASE:
				return SurfTest.ExecuteReleaseTest();
				break;
			case D3DQAID_SRF_GETDEVICE:
				return SurfTest.ExecuteGetDeviceTest();
				break;
			case D3DQAID_SRF_GETCONTAINER:
				return SurfTest.ExecuteGetContainerTest();
				break;
			case D3DQAID_SRF_GETDESC:
				return SurfTest.ExecuteGetDescTest();
				break;
			case D3DQAID_SRF_LOCKRECT:
				return SurfTest.ExecuteLockRectTest();
				break;
			case D3DQAID_SRF_UNLOCKRECT:
				return SurfTest.ExecuteUnlockRectTest();
				break;
			case D3DQAID_SRF_GETDC:
				return SurfTest.ExecuteGetDCTest();
				break;
			case D3DQAID_SRF_RELEASEDC:
				return SurfTest.ExecuteReleaseDCTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}
		}
		else if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILETEXTURE_BASE) &&
			     (ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILETEXTURE_MAX))
		{
			TextureTest TexTest;
			if (FAILED(hr = TexTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                        &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                        g_bDebug,         // BOOL bDebug
			                        ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"TextureTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_TEX_QUERYINTERFACE:
				return TexTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_TEX_ADDREF:
				return TexTest.ExecuteAddRefTest();
				break;
			case D3DQAID_TEX_RELEASE:
				return TexTest.ExecuteReleaseTest();
				break;
			case D3DQAID_TEX_GETDEVICE:
				return TexTest.ExecuteGetDeviceTest();
				break;
			case D3DQAID_TEX_SETPRIORITY:
				return TexTest.ExecuteSetPriorityTest();
				break;
			case D3DQAID_TEX_GETPRIORITY:
				return TexTest.ExecuteGetPriorityTest();
				break;
			case D3DQAID_TEX_PRELOAD:
				return TexTest.ExecutePreLoadTest();
				break;
			case D3DQAID_TEX_GETTYPE:
				return TexTest.ExecuteGetTypeTest();
				break;
			case D3DQAID_TEX_SETLOD:
				return TexTest.ExecuteSetLODTest();
				break;
			case D3DQAID_TEX_GETLOD:
				return TexTest.ExecuteGetLODTest();
				break;
			case D3DQAID_TEX_GETLEVELCOUNT:
				return TexTest.ExecuteGetLevelCountTest();
				break;
			case D3DQAID_TEX_GETLEVELDESC:
				return TexTest.ExecuteGetLevelDescTest();
				break;
			case D3DQAID_TEX_GETSURFACELEVEL:
				return TexTest.ExecuteGetSurfaceLevelTest();
				break;
			case D3DQAID_TEX_LOCKRECT:
				return TexTest.ExecuteLockRectTest();
				break;
			case D3DQAID_TEX_UNLOCKRECT:
				return TexTest.ExecuteUnlockRectTest();
				break;
			case D3DQAID_TEX_ADDDIRTYRECT:
				return TexTest.ExecuteAddDirtyRectTest();
				break;
			case D3DQAID_TEX_SRFQUERYINTERFACE:    
				return TexTest.ExecuteTexSurfQueryInterfaceTest();
				break;
			case D3DQAID_TEX_SRFADDREF:            
				return TexTest.ExecuteTexSurfAddRefTest();
				break;
			case D3DQAID_TEX_SRFRELEASE:           
				return TexTest.ExecuteTexSurfReleaseTest();
				break;
			case D3DQAID_TEX_SRFGETDEVICE:         
				return TexTest.ExecuteTexSurfGetDeviceTest();
				break;
			case D3DQAID_TEX_SRFGETCONTAINER:      
				return TexTest.ExecuteTexSurfGetContainerTest();
				break;
			case D3DQAID_TEX_SRFGETDESC:           
				return TexTest.ExecuteTexSurfGetDescTest();
				break;
			case D3DQAID_TEX_SRFLOCKRECT:          
				return TexTest.ExecuteTexSurfLockRectTest();
				break;
			case D3DQAID_TEX_SRFUNLOCKRECT:        
				return TexTest.ExecuteTexSurfUnlockRectTest();
				break;
			case D3DQAID_TEX_SRFGETDC:             
				return TexTest.ExecuteTexSurfGetDCTest();
				break;
			case D3DQAID_TEX_SRFRELEASEDC:         
				return TexTest.ExecuteTexSurfReleaseDCTest();
				break;
			case D3DQAID_TEX_SRFSTRETCHRECT:
				return TexTest.ExecuteTexSurfStretchRectTest();
				break;
			case D3DQAID_TEX_SRFCOLORFILL:
				return TexTest.ExecuteTexSurfColorFillTest();
				break;

			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}
		}
		else if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILEVERTEXBUFFER_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILEVERTEXBUFFER_MAX))
		{
			VertexBufferTest VBTest;
			if (FAILED(hr = VBTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                       &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                       g_bDebug,         // BOOL bDebug
			                       ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"VertexBufferTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_VB_QUERYINTERFACE:
				return VBTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_VB_ADDREF:
				return VBTest.ExecuteAddRefTest();
				break;
			case D3DQAID_VB_RELEASE:
				return VBTest.ExecuteReleaseTest();
				break;
			case D3DQAID_VB_GETDEVICE:
				return VBTest.ExecuteGetDeviceTest();
				break;
			case D3DQAID_VB_SETPRIORITY:
				return VBTest.ExecuteSetPriorityTest();
				break;
			case D3DQAID_VB_GETPRIORITY:
				return VBTest.ExecuteGetPriorityTest();
				break;
			case D3DQAID_VB_PRELOAD:
				return VBTest.ExecutePreLoadTest();
				break;
			case D3DQAID_VB_GETTYPE:
				return VBTest.ExecuteGetTypeTest();
				break;
			case D3DQAID_VB_LOCK:
				return VBTest.ExecuteLockTest();
				break;
			case D3DQAID_VB_UNLOCK:
				return VBTest.ExecuteUnlockTest();
				break;
			case D3DQAID_VB_GETDESC:
				return VBTest.ExecuteGetDescTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}
		} 		
		else if ((ulTestOrdinal >  D3DQAID_IDIRECT3DMOBILESWAPCHAIN_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DMOBILESWAPCHAIN_MAX))
		{
			SwapChainTest SCTest;
			if (FAILED(hr = SCTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                       &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                       g_bDebug,         // BOOL bDebug
			                       ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"VertexBufferTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_SC_QUERYINTERFACE:
				return SCTest.ExecuteQueryInterfaceTest();
				break;
			case D3DQAID_SC_ADDREF:
				return SCTest.ExecuteAddRefTest();
				break;
			case D3DQAID_SC_RELEASE:
				return SCTest.ExecuteReleaseTest();
				break;
            case D3DQAID_SC_GETBACKBUFFER:
                return SCTest.ExecuteGetBackBufferTest();
                break;
			case D3DQAID_SC_PRESENT:
				return SCTest.ExecutePresentTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}
		} else
		if ((ulTestOrdinal >  D3DQAID_IDIRECT3DPSL_BASE) &&
			(ulTestOrdinal <= D3DQAID_IDIRECT3DPSL_MAX))
		{

			IDirect3DMobileDeviceTest DeviceTest;
			if (FAILED(hr = DeviceTest.Init(&g_TestCaseArgs,  // LPTEST_CASE_ARGS pTestCaseArgs
			                           &WindowArgs,    // LPWINDOW_ARGS pWindowArgs
			                           g_bDebug,         // BOOL bDebug
			                           ulTestOrdinal)))// UINT uiTestOrdinal
			{
				DebugOut(DO_ERROR, L"IDirect3DMobileDeviceTest test initialization failure. (hr = 0x%08x) Skipping.", hr);
				return TPR_SKIP;
			}

			PostQuitMessage(0); // MessageLoop is not needed

			//
			// Pick a member function to call
			//
			switch(ulTestOrdinal)
			{
			case D3DQAID_DEV_PSLRANDOMIZER:
				return DeviceTest.ExecutePSLRandomizerTest();
				break;
			default:
				DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
				return TPR_FAIL;
				break;
			}

		}
		else
		{
			DebugOut(DO_ERROR, L"Invalid test case ordinal. Failing.");
			return TPR_FAIL;
		}

	}
	else
	{
	    return TPR_NOT_HANDLED;
	}
}

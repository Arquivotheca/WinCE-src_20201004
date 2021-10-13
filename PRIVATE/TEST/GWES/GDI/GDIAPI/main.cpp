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
#include <windows.h>
#include <kato.h>
#include "global.h"
#include "main.h"
#include "clparse.h"
#ifdef UNDER_CE
#include "aygshell.h"
#endif

#ifndef SHFS_HIDESIPBUTTON
#define SHFS_HIDESIPBUTTON 0x0008
#endif
#ifndef SHFS_HIDETASKBAR
#define SHFS_HIDETASKBAR 0x0002
#endif
#ifndef SHFS_SHOWSIPBUTTON
#define SHFS_SHOWSIPBUTTON 0x0004
#endif
#ifndef SHFS_SHOWTASKBAR
#define SHFS_SHOWTASKBAR 0x0001
#endif


// Statics
static const TCHAR  *gszClassName = TEXT("GDI: gdit");

// shell and log vars
SPS_SHELL_INFO g_spsShellInfo;
CKato  *aLog = NULL;

// Shared globals
HWND    ghwndTopMain;
BOOL    gSanityChecks = 0;
PFNGRADIENTFILL gpfnGradientFill;
PFNALPHABLEND gpfnAlphaBlend;
PFNSTARTDOC gpfnStartDoc;
PFNENDDOC gpfnEndDoc;
PFNSTARTPAGE gpfnStartPage;
PFNENDPAGE gpfnEndPage;
PFNABORTDOC gpfnAbortDoc;
PFNSETABORTPROC gpfnSetAbortProc;

static HANDLE ghPowerManagement;
static HINSTANCE ghinstCoreDLL;
BOOL gResizeAvail = FALSE;
BOOL gResizeDisabledByUser = FALSE;
RECT grcOriginal;
BOOL gRotateAvail = FALSE;
BOOL gRotateDisabledByUser = FALSE;
DWORD gdwCurrentOrientation = DMDO_DEFAULT;
// default, we don't know the name of the current angle
DWORD gdwCurrentAngle = -1;
int gBpp;
DWORD gWidth;
DWORD gHeight;
DWORD gMonitorToUse;
BOOL g_bMonitorSpread;
BOOL g_bRTL = FALSE;            // by default use normal windows.  Set TRUE for RTL

#define USPMODE_MODE  0x0F      // low nybble is current mode
#define USPMODE_FORCE 0xF0      // high nybble set if user forced this mode
BOOL g_bUSP = FALSE;            // by default there is no Uniscribe

// allow the user to specify a secondary display driver
BOOL g_bUseDefaultDisplay = TRUE;
TCHAR gszDisplayPath[MAX_PATH];

// our module's current location, and where bitmaps are stored by default.
TCHAR gszModulePath[MAX_PATH];
TCHAR gszBitmapDestination[MAX_PATH];
TCHAR gszFontLocation[MAX_PATH];
#define VERIFYDRIVERDLLNAME TEXT("ddi_test.dll")
// gszVerifyDriverName is globally used and set up with the correct path in InitVerify
TCHAR gszVerifyDriverName[MAX_PATH];

// Configuration
int     DCFlag;
int     verifyFlag;
int     DriverVerify;
BOOL bUseAllSurfaces = FALSE;
RECT grcTopMain;

HPEN g_hpenStock;
HBRUSH g_hbrushStock;
HPALETTE g_hpalStock;
HBITMAP g_hbmpStock;
HFONT g_hfontStock;

// dummy main
BOOL    WINAPI
DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
#ifdef UNDER_CE
        // Need to disable IME so that the toolbar
        // does not interfere with the screen.
        ImmDisableIME(0);
#endif
    }

    return 1;

}

BOOL InitTopMainWindow();
void DestroyTopMainWindow();
void IncreaseProgramMemory();
void RestoreProgramMemory();
void InitFunctionPointers();
void FreeFunctionPointers();
void InitDevice();
void FreeDevice();
void InitShell(HWND);
void ReleaseShell(HWND);
void InitializeModulePaths();
void InitGlobalStockObjects();
BOOL InitUSPMode(void);
void ForceUSPMode(BOOL bForce);

/***********************************************************************************
***
***   Log Definitions
***
************************************************************************************/

#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

//******************************************************************************
//***** Support
//******************************************************************************

void
initAll(void)
{
    BOOL bOldVerify;

    aLog->BeginLevel(0, TEXT("BEGIN GROUP: ******* GDI Testing *******"));
#ifdef UNDER_CE
    info(ECHO, TEXT("GDI Test suite version %d.%d.%d.%d"), CE_MAJOR_VER, CE_MINOR_VER, CE_BUILD_VER, GDITEST_CHANGE_VER);
#endif

    // turn off verification during initialization
    InitDevice();
    IncreaseProgramMemory(); // adjust program memory before initializing verification
    InitFunctionPointers(); // function pointers must be initialized before verification
    InitGlobalStockObjects();
    InitVerify();
    InitUSPMode();          // determine if Uniscribe is loaded by the OS

    // now that verification is initialized, turn it off for the rest of initialization.
    bOldVerify = SetSurfVerify(FALSE);
    InitTopMainWindow();
    InitShell(ghwndTopMain);
    InitSurf();
    initFonts();
    SetSurfVerify(bOldVerify);
}

void
closeAll(void)
{
    aLog->EndLevel(TEXT("END GROUP: ******* GDI Testing *******"));
    FreeVerify();
    freeFonts();
    FreeSurf();
    ReleaseShell(ghwndTopMain);
    DestroyTopMainWindow();
    FreeFunctionPointers();
    FreeDevice();
    RestoreProgramMemory();
}

void
OutputCommandOptions()
{
    info(ECHO, TEXT(""));
    info(ECHO, TEXT("/? outputs the usage information."));
    info(ECHO, TEXT("/OutputBitmaps sets the test to output bitmaps of the failures."));
    info(ECHO, TEXT("/BitmapDestination <path to destination> sets the test to output bitmaps of the failures to the specified location."));
    info(ECHO, TEXT("/FontLocation <path to source> sets the location the test suite should look for extra font files, defaults to the current directory if not specified."));
    info(ECHO, TEXT("/NoVerify turns off driver to driver verification, default is to use driver verification if it's available."));
    info(ECHO, TEXT("/NoRotate turns off display rotation if it's available."));
    info(ECHO, TEXT("/ForceVerify forces verification on even in low memory situations, fails tests if ddi_test.dll is unavailable."));
    info(ECHO, TEXT("/AlwaysVerify forces a surface verification to be performed on every step.  NOTE: This will take a significant amount of time."));
    info(ECHO, TEXT("/NoResize turns off display resize if it's available."));
    info(ECHO, TEXT("/SetDisplay <display> forces a secondary display driver to be tested instead of the default display driver."));
    info(ECHO, TEXT("/ManualVerify turns on manual verification.  Hit the left shift key to continue when prompted."));
    info(ECHO, TEXT("/Monitor <1-4> to select a monitor to run on, applicable only to a window DC."));
    info(ECHO, TEXT("/MonitorSpread to spread the test suite across multiple monitors, applicable to both a window DC and a Default DC."));
    info(ECHO, TEXT("/Width <size> /Height <size> sets the size of surface to use, %dx%d minimum, must be divisible by 2."), MINSCREENSIZE, MINSCREENSIZE);
    info(ECHO, TEXT("/Surface All  - cycles through all known surfaces.  NOTE: This will take a significant amount of time."));
    info(ECHO, TEXT("/Surface <surfname> sets the surface type to be tested."));
    info(DETAIL, TEXT("     <surfname> can be one of the following entries."));

    for(int i = 1; DCEntryStruct[i].szName; i++)
        info(DETAIL, TEXT("     %s"), DCEntryStruct[i].szName);

    info(ECHO, TEXT("/RTL creates all windows as RTL and performs tests with them."));
    info(ECHO, TEXT("/USP [on|off] forces Uniscribe test run mode on or off."));
}

void
InitializeModulePaths()
{
    // initialize our module path data.
    if(GetModuleFileName(globalInst, gszModulePath, countof(gszModulePath)) < MAX_PATH)
    {
        // start at the first non null character, an emtpy string results in index being less than 0
        // which skips the loop
        int index = _tcslen(gszModulePath) - 1;
        for(; gszModulePath[index] != '\\' && index > 0; index--);

        // if the index is less than 0, then it's an "empty" path.
        // if the index is 0, then it's a single character
        if(index >= 0 && gszModulePath[index] == '\\')
            gszModulePath[index + 1] = NULL;
    }
    else gszModulePath[0] = NULL;

    // initialize the "Verification driver" dll name and path
    _tcsncpy(gszVerifyDriverName, gszModulePath, MAX_PATH - 1);
    gszVerifyDriverName[MAX_PATH-1] = NULL;
    _tcsncat(gszVerifyDriverName, VERIFYDRIVERDLLNAME, MAX_PATH - _tcslen(gszVerifyDriverName) - 1);
}

void
RemoveQuotes(TCHAR *tszInputString)
{
    int Index1 = 0, Index2 = 0;
    // Index 1 is the current location being tested and furthest down the string,
    // index 2 is the current location of the characters copied
    if(tszInputString)
    {
        while(tszInputString[Index1] != NULL)
        {
            // index 1 is always going to be further on in the string than index 2
            // because it's walking ahead and skipping the ' characters.
            // if index 1 is a ', then we skip it and move it on leaving index 2 at the next
            // location needing a character.
            if(tszInputString[Index1] != '\'')
            {
                tszInputString[Index2] = tszInputString[Index1];
                Index2++;
            }
            Index1++;
        }
        // when the index1 hit's null, then we've copied over all of the non ' characters
        // so index 2 needs to be nulled out.
        tszInputString[Index2] = NULL;
    }
}

BOOL
ProcessCommandLine()
{
    TCHAR tcString[MAXSTRINGLENGTH];
    class CClParse cCLPparser(g_spsShellInfo.szDllCmdLine);

    DCFlag = EWin_Primary;
    verifyFlag = EVerifyAuto;
    DriverVerify = EVerifyDDITEST;
    g_OutputBitmaps = FALSE;
    g_bMonitorSpread = FALSE;

    if(cCLPparser.GetOpt(TEXT("?")))
    {
        OutputCommandOptions();
        return FALSE;
    }
    else
    {
        // get the surface name
        if(cCLPparser.GetOptString(TEXT("Surface"), tcString, MAXSTRINGLENGTH))
        {
            RemoveQuotes(tcString);

            if(!_tcsicmp(tcString, TEXT("All")))
                bUseAllSurfaces = TRUE;
            else
             {
                DCFlag = DCName(tcString);

                // if the entry wasn't found, output an error.
                if(DCFlag == 0)
                {
                    info(DETAIL, TEXT("Invalid surface name given."));
                    OutputCommandOptions();
                    return FALSE;
                }
                else if(DCFlag == EGDI_Default || DCFlag == EGDI_Display)
                {
                    // test that origin is @ 0,0 for us or tests will fail for GDI_Default
                    // this can happen if there is a taskbar or some other control taking up
                    // this area on the desktop.
                    RECT rc;
                    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
                    if((rc.left != 0) || (rc.top != 0))
                    {
                        info(DETAIL, TEXT("The taskbar or some other control is at the origin of the desktop surface."));
                        info(DETAIL, TEXT("GDI_Default param cannot be used on this particular OS Image"));
                        info(DETAIL, TEXT("GDI_Default can only be used when the origin of the usable desktop area is (0,0)."));
                        info(FAIL, TEXT("*** Stopping test run ***"));
                        return FALSE;
                    }
                }
             }
        }

        if(!cCLPparser.GetOptVal(TEXT("Width"), &gWidth))
            gWidth = gHeight = 0;
        else if(!cCLPparser.GetOptVal(TEXT("Height"), &gHeight))
            gWidth = gHeight = 0;
        else if(gWidth < MINSCREENSIZE || gHeight < MINSCREENSIZE)
        {
            info(FAIL, TEXT("invalid width/height given."));
            OutputCommandOptions();
            gWidth = gHeight = 0;
            return FALSE;
        }
        else
        {
            // make the width/height divisible by 2.
            gWidth = (gWidth >> 1) << 1;
            gHeight = (gHeight >> 1) << 1;
        }

        // if a string is available, then load the string into the bitmap destination string.
        if(cCLPparser.GetOptString(TEXT("BitmapDestination"), gszBitmapDestination, MAX_PATH))
        {
            RemoveQuotes(gszBitmapDestination);

            // this will never be a 0 length string, thus we can subtract by 1 safely here.
            // if it were a 0 length string, then GetOptString would return false.
            if(gszBitmapDestination[_tcslen(gszBitmapDestination) - 1] != '\\')
                _tcsncat(gszBitmapDestination, TEXT("\\"), countof(gszBitmapDestination) - _tcslen(gszBitmapDestination) - 1);
            g_OutputBitmaps = TRUE;
        }
        // determine if the user wants bitmaps
        else if(cCLPparser.GetOpt(TEXT("OutputBitmaps")))
        {
            // string copy over the module's location into the bitmap destination location
            _tcsncpy(gszBitmapDestination, gszModulePath, MAX_PATH);
            g_OutputBitmaps = TRUE;
        }

        if(cCLPparser.GetOptString(TEXT("FontLocation"), gszFontLocation, MAX_PATH))
        {
            RemoveQuotes(gszFontLocation);

            int nStringLength = _tcslen(gszFontLocation);
            // if the string has atleast 1 character in it (so we don't buffer underrun), and the last character isn't
            // a \ (completing the directory path), then append a \.
            if(nStringLength > 0 && gszFontLocation[nStringLength - 1] != '\\')
                _tcsncat(gszFontLocation, TEXT("\\"), MAX_PATH - _tcslen(gszFontLocation));
        }
        // if the path wasn't given, then just use the current module's location
        else
            _tcsncpy(gszFontLocation, gszModulePath, MAX_PATH);

        // If there is a user requested driver for driver verification, then use it instead.
        // This is primarily used for Microsoft internal testing only, as the driver specified by the user
        // may not exhibit the same behavior as the ddi_test.dll driver in some circumstances, which could cause test failures.
        // For example, some display drivers do not behave well when loaded and unloaded repeatedly,
        // which is necessary in this scenario.
        if(cCLPparser.GetOptString(TEXT("VerifyDriver"), tcString, MAXSTRINGLENGTH))
        {
            _tcsncpy(gszVerifyDriverName, tcString, min(MAX_PATH, MAXSTRINGLENGTH));
        }

        if(cCLPparser.GetOpt(TEXT("NoRotate")))
            gRotateDisabledByUser = TRUE;

        if(cCLPparser.GetOpt(TEXT("NoResize")))
            gResizeDisabledByUser = TRUE;

        // user wants to test RTL under a LTR or RTL environment
        if(cCLPparser.GetOpt(TEXT("RTL")))
            g_bRTL = TRUE;

        // user wants to test LTR under a LTR or RTL environment
        if(cCLPparser.GetOpt(TEXT("LTR")))
            g_bRTL = FALSE;

        // user wants to force the Uniscribe mode on or off
        if(cCLPparser.GetOptString(TEXT("USP"), tcString, MAXSTRINGLENGTH))
        {
            if(!_tcscmp(tcString, TEXT("off")))
                ForceUSPMode(FALSE);
            else if (!_tcscmp(tcString, TEXT("on")))
                ForceUSPMode(TRUE);
        }

        // if the user gave a monitor number, pass it along, otherwise default to 0.
        if(!cCLPparser.GetOptVal(TEXT("Monitor"), &gMonitorToUse))
            gMonitorToUse = 0;

        if(cCLPparser.GetOpt(TEXT("NoVerify")))
            DriverVerify = EVerifyNone;

        if(cCLPparser.GetOpt(TEXT("ForceVerify")))
            DriverVerify = EVerifyForceDDITEST;

        if(cCLPparser.GetOpt(TEXT("AlwaysVerify")))
            verifyFlag = EVerifyAlways;

        if(cCLPparser.GetOpt(TEXT("ManualVerify")))
            verifyFlag = EVerifyManual;

        // Allow the user to specify a different display driver to test. The EGDI_Display mode
        // is only allowed on the primary because it uses createdc(...) with the primary.
        if(cCLPparser.GetOptString(TEXT("SetDisplay"), gszDisplayPath, MAX_PATH) && DCFlag != EGDI_Display)
            g_bUseDefaultDisplay = FALSE;

        if(cCLPparser.GetOpt(TEXT("MonitorSpread")))
            g_bMonitorSpread = TRUE;

    }
    return TRUE;
}

//******************************************************************************
//***** ShellProc
//******************************************************************************

SHELLPROCAPI
ShellProc(UINT uMsg, SPPARAM spParam)
{
    SPS_BEGIN_TEST *pBeginTestInfo;

    switch (uMsg)
    {

        case SPM_REGISTER:
            ((LPSPS_REGISTER) spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
            return SPR_HANDLED;
#endif // UNICODE

        case SPM_LOAD_DLL:
            aLog = (CKato *) KatoGetDefaultObject();

#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif

            // We need to keep the IME toolbar from interfering with the test.
#ifdef UNDER_CE
            ImmDisableIME(0);
#endif
        case SPM_UNLOAD_DLL:
        case SPM_START_SCRIPT:
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

        case SPM_SHELL_INFO:
            g_spsShellInfo = *(LPSPS_SHELL_INFO) spParam;
            globalInst = g_spsShellInfo.hLib;

            // retrieve our current module's path
            InitializeModulePaths();

            // if the process command line fails, then that means the user requested the command
            // line options, so exit the dll.
            if(!ProcessCommandLine())
            {
                info(DETAIL, TEXT("Invalid command line params, failing the test suite so it won't continue"));
                return SPR_FAIL;
            }
            return SPR_HANDLED;

        case SPM_BEGIN_GROUP:
            initAll();
            return SPR_HANDLED;

        case SPM_END_GROUP:
            closeAll();
            return SPR_HANDLED;

        case SPM_BEGIN_TEST:
#ifdef UNDER_CE
            // As this test suite has GDI allocate very large and varying amounts of memory for
            // surfaces in our process space, the heap can become very fragmented.  To prevent this,
            // we'll compact our heaps before each test case.
            CompactAllHeaps();
#endif
            if(spParam!=NULL)
            {
                pBeginTestInfo = reinterpret_cast<SPS_BEGIN_TEST *>(spParam);
                srand(pBeginTestInfo->dwRandomSeed);
                aLog->BeginLevel(((LPSPS_BEGIN_TEST) spParam)->lpFTE->dwUniqueID, TEXT("BEGIN TEST: <%s>, Thds=%u,"),
                                 ((LPSPS_BEGIN_TEST) spParam)->lpFTE->lpDescription, ((LPSPS_BEGIN_TEST) spParam)->dwThreadCount);
            }
            else return SPR_FAIL;

            return SPR_HANDLED;

        case SPM_END_TEST:
            aLog->EndLevel(TEXT("END TEST: <%s>"), ((LPSPS_END_TEST) spParam)->lpFTE->lpDescription);
            return SPR_HANDLED;

        case SPM_EXCEPTION:
            aLog->Log(MY_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
    }
    return SPR_NOT_HANDLED;
}

/***********************************************************************************
***
***   Log Functions
***
************************************************************************************/

//******************************************************************************
void
info(infoType iType, LPCTSTR szFormat, ...)
{
    TCHAR   szBuffer[1024] = {NULL};

    va_list pArgs;

    va_start(pArgs, szFormat);
    // _vsntprintf deprecated
    if(FAILED(StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szFormat, pArgs)))
       OutputDebugString(TEXT("StringCbVPrintf failed"));
    va_end(pArgs);

    switch (iType)
    {
        case FAIL:
            aLog->Log(MY_FAIL, TEXT("FAIL: %s"), szBuffer);
            break;
        case ECHO:
            aLog->Log(MY_PASS, szBuffer);
            break;
        case DETAIL:
            aLog->Log(MY_PASS + 1, TEXT("    %s"), szBuffer);
            break;
        case ABORT:
            aLog->Log(MY_ABORT, TEXT("Abort: %s"), szBuffer);
            break;
        case SKIP:
            aLog->Log(MY_SKIP, TEXT("Skip: %s"), szBuffer);
            break;
    }
}


//******************************************************************************
void
OutputLogInfo(cCompareResults * cCr)
{
    TCHAR *tszLoginfo = NULL;

    for(int i = 0; tszLoginfo = cCr->GetLogLine(i); i++)
        info(DETAIL, tszLoginfo);
}

//******************************************************************************
TESTPROCAPI
getCode(void)
{

    for (int i = 0; i < 15; i++)
        if (aLog->GetVerbosityCount((DWORD) i) > 0)
            switch (i)
            {
                case MY_EXCEPTION:
                    return TPR_HANDLED;
                case MY_FAIL:
                    return TPR_FAIL;
                case MY_ABORT:
                    return TPR_ABORT;
                case MY_SKIP:
                    return TPR_SKIP;
                case MY_NOT_IMPLEMENTED:
                    return TPR_HANDLED;
                case MY_PASS:
                    return TPR_PASS;
                default:
                    return TPR_NOT_HANDLED;
            }
    return TPR_PASS;
}

//***********************************************************************************
// query all display modes until we fail to determine the max supported resolutions.
DWORD
gdwMaxDisplayModes(void)
{
#ifdef UNDER_CE
    static DWORD maxModeIndex = 0;

    if (0 == maxModeIndex)
    {
        info(DETAIL, TEXT("Enumerating all available display modes:"));

        DEVMODE devMode;
        memset(&devMode,0x00,sizeof(devMode));
        devMode.dmSize = sizeof(devMode);

        // save our initial display mode
        if (TRUE == EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode))
        {
            grcOriginal.right = devMode.dmPelsWidth;
            grcOriginal.bottom = devMode.dmPelsHeight;
        }
        else
        {
            info(DETAIL, TEXT("    Failed to retrieve current display settings."));
        }

        // find number of available modes
        while (TRUE == EnumDisplaySettings(NULL, maxModeIndex, &devMode))
        {
            info(DETAIL, TEXT("    Mode %d: %dx%dx%d"), maxModeIndex, devMode.dmPelsWidth,
                devMode.dmPelsHeight, devMode.dmBitsPerPel);
            maxModeIndex++;

            // request a test for this mode
            devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
            if(ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_TEST,NULL) != DISP_CHANGE_SUCCESSFUL)
            {
                info(DETAIL, TEXT("    Mode %d: Display mode resolution test failed."), maxModeIndex);
            }
        }
    }
    return maxModeIndex;
#else
    return 1;
#endif
}

//***********************************************************************************
void
ResetResolutionSize(void)
{
#ifdef UNDER_CE
    DEVMODE devMode;
    memset(&devMode,0x00,sizeof(devMode));
    devMode.dmSize=sizeof(devMode);
    devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
    devMode.dmPelsWidth = grcOriginal.right;
    devMode.dmPelsHeight = grcOriginal.bottom;
    if(ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL) != DISP_CHANGE_SUCCESSFUL)
    {
        info(DETAIL, TEXT("Display mode resolution reset failed."));
    }
#endif
}

//***********************************************************************************
BOOL
// determine whether or not rotation is available, and if it is
// save the current orientation.
CheckRotation()
{
    BOOL bRet=FALSE;    // Rotation isn't supported.
#ifdef UNDER_CE
    DEVMODE devMode;

    memset(&devMode,0x00,sizeof(devMode));
    devMode.dmSize=sizeof(devMode);

    devMode.dmFields = DM_DISPLAYQUERYORIENTATION;
    if (ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_TEST,NULL) == DISP_CHANGE_SUCCESSFUL)
    {
        if (devMode.dmDisplayOrientation != 0)
        {
            bRet=TRUE;
            // retrieve our current angle
            // CDS_TEST with a DM_DISPLAYORIENTATION will retrieve the current orientation.
            devMode.dmFields = DM_DISPLAYORIENTATION;
            if (ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_TEST,NULL) == DISP_CHANGE_SUCCESSFUL)
            {
                gdwCurrentOrientation = devMode.dmDisplayOrientation;
                switch(gdwCurrentOrientation)
                {
                    case DMDO_0:
                        gdwCurrentAngle = 0;
                        break;
                    case DMDO_90:
                        gdwCurrentAngle = 90;
                        break;
                    case DMDO_180:
                        gdwCurrentAngle = 180;
                        break;
                    case DMDO_270:
                        gdwCurrentAngle = 270;
                        break;
                }
            }
        }
        else
            bRet=FALSE;
    }
#endif
    return bRet;
}

void
// return to the orientation that was set when the test was started.
ResetRotation()
{
#ifdef UNDER_CE

    DEVMODE devMode;
    memset(&devMode,0x00,sizeof(devMode));
    devMode.dmSize=sizeof(devMode);
    devMode.dmFields = DM_DISPLAYORIENTATION;
    devMode.dmDisplayOrientation = gdwCurrentOrientation;
    if(ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL) != DISP_CHANGE_SUCCESSFUL)
    {
        info(DETAIL, TEXT("Display mode orientation reset failed."));
    }
#endif
}

BOOL
// check for the presence of Uniscribe.
IsUniscribeImage(void)
{
    BOOL fOk = TRUE;
    HKEY hKey;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                    TEXT("System\\Gdi\\Uniscribe"),
                    0,
                    0,
                    &hKey) == ERROR_SUCCESS)
    {
        // If regkey exists, verify Uniscribe is not disabled
        DWORD dwDisableUSPCE = 0;
        DWORD dwSize = sizeof(dwDisableUSPCE);

        if (RegQueryValueEx(hKey,
                            TEXT("DisableUniscribe"),
                            NULL,
                            NULL,
                            (BYTE*)&dwDisableUSPCE,
                            &dwSize) == ERROR_SUCCESS)
        {
            if (dwDisableUSPCE)
                fOk = FALSE;
        }
        RegCloseKey(hKey);
    }
    else
    {

        // Check if USPCE is in the release dir
        HINSTANCE hInst = LoadLibrary(
            L"uspce.dll"
            );
        
        if( !hInst )
            fOk = FALSE;

        FreeLibrary(hInst);
    }

    return fOk;
}

LONG    CALLBACK
WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return (DefWindowProc(hwnd, message, wParam, lParam));
}

BOOL
InitTopMainWindow()
{
    TDC  hdc;
    RECT rcWorkArea;

    if (g_bUseDefaultDisplay)
    {
        // retrive the work area to create the window in.
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);

        WNDCLASS wc;
        wc.style = 0;
        wc.lpfnWndProc = (WNDPROC) WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = globalInst;
        wc.hIcon = 0;
        wc.hCursor = 0;
        wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName = 0;
        wc.lpszClassName = gszClassName;

        if (!RegisterClass(&wc))
        {
            info(DETAIL, TEXT("InitTopMainWindow(): RegisterClass() Failed: err=%ld\n"), GetLastError());
            return FALSE;
        }

        LONG dwStyleEx;
        if(g_bRTL)
            dwStyleEx = WS_EX_LAYOUTRTL;
        else
            dwStyleEx = NULL;

        // create a window that fills the work area.
        ghwndTopMain =
            CreateWindowEx(dwStyleEx, gszClassName, NULL, WS_POPUP, rcWorkArea.left, rcWorkArea.top, rcWorkArea.right - rcWorkArea.left,
                           rcWorkArea.bottom - rcWorkArea.top, NULL, NULL, globalInst, NULL);

        if (ghwndTopMain == 0)
        {
            info(DETAIL, TEXT("InitTopMainWindow(): CreateWindow Failed: err=%ld\n"), GetLastError());
            return (FALSE);
        }

        CheckNotRESULT(FALSE, SetForegroundWindow(ghwndTopMain));
        ShowWindow(ghwndTopMain, SW_SHOWNORMAL);
        CheckNotRESULT(FALSE, UpdateWindow(ghwndTopMain));
        // retrieve the client area of the top main window.
        CheckNotRESULT(FALSE, GetClientRect(ghwndTopMain, &grcTopMain));

        // Verify presence of mouse
        if(GetCursor() == NULL)
        {
            info(DETAIL, TEXT("Mouse not present on device."));
        }
        else
        {
            Sleep(500);
            CheckNotRESULT(FALSE, SetCursorPos(grcTopMain.left , grcTopMain.top ));
            Sleep(500);
            CheckNotRESULT(FALSE, SetCursor(NULL));
        }

        gRotateAvail = CheckRotation();
        if(gRotateAvail)
        {
            if(gdwCurrentAngle == -1)
                info(DETAIL, TEXT("Display rotation available, current angle unknown."));
            else
                info(DETAIL, TEXT("Display rotation available, current angle is %d"), gdwCurrentAngle);
        }
        else info(DETAIL, TEXT("Display rotation unavailable."));
    }
    else
    {
        gRotateAvail = FALSE;
        info(DETAIL, TEXT("Mouse not present on secondary display."));
        info(DETAIL, TEXT("Display rotation unavailable on secondary display."));
    }

    if(g_OutputBitmaps)
    {
        info(DETAIL, TEXT("Bitmap output on error enabled"));
        info(DETAIL, TEXT("Bitmaps will be saved to %s"), gszBitmapDestination);
        info(DETAIL, TEXT("WARNING: Bitmap saving takes a significant amount of space, please use /BitmapDestination to specify a location"));
        info(DETAIL, TEXT("WARNING: if the current path has insufficient space."), gszBitmapDestination);
    }

    DWORD dwModesAvail = gdwMaxDisplayModes();
    if (dwModesAvail > 1)
    {
        gResizeAvail = TRUE;
        if (!gResizeDisabledByUser)
            info(DETAIL, TEXT("Display resolution resize: enabled. %d modes available."), dwModesAvail);
        else
            info(DETAIL, TEXT("Display resolution resize: disabled by user."));
    }
    else
        info(DETAIL, TEXT("Display resolution resize: disabled. Only one mode supported by driver."));

    if(!bUseAllSurfaces)
        info(DETAIL, TEXT("DC Type: %s"), DCName[DCFlag]);
    else
    {
        info(DETAIL, TEXT("Using Dc types:"));
        for(int i = 1; DCEntryStruct[i].szName; i++)
            info(DETAIL, TEXT("    %s"), DCEntryStruct[i].szName);
    }
    info(DETAIL, TEXT("Driver verify set to: %s"), funcName[DriverVerify]);
    if(DriverVerify == EVerifyDDITEST || DriverVerify == EVerifyForceDDITEST)
        info(DETAIL, TEXT("Using driver %s as the verification driver"), gszVerifyDriverName);

    info(DETAIL, TEXT("User Verify set to: %s"), funcName[verifyFlag]);

    CheckNotRESULT(NULL, hdc = myGetDC(ghwndTopMain));
    if(hdc)
    {
        // retrive the work area based on the DC from the secondary display
        if (!g_bUseDefaultDisplay)
        {
            rcWorkArea.left   = 0;
            rcWorkArea.top    = 0;
            rcWorkArea.right  = GetDeviceCaps(hdc, HORZRES);
            rcWorkArea.bottom = GetDeviceCaps(hdc, VERTRES);
        }

        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        info(DETAIL, TEXT("Work area = [%d %d %d %d]"), rcWorkArea.left, rcWorkArea.top, rcWorkArea.right, rcWorkArea.bottom);
        if (g_bUseDefaultDisplay)
            info(DETAIL, TEXT("TopMainWindow: Window position = [%d %d %d %d]"), grcTopMain.left, grcTopMain.top, grcTopMain.right, grcTopMain.bottom);
        else
            info(DETAIL, TEXT("TopMainWindow: Window unavailable on secondary display"));

        if(gWidth >= MINSCREENSIZE && gHeight >= MINSCREENSIZE)
            info(DETAIL, TEXT("Width and Height in use when possible: [%d %d]"), gWidth, gHeight);
        OutputDeviceCapsInfo(hdc);
        CheckNotRESULT(0, myReleaseDC(ghwndTopMain, hdc));
    }

    return TRUE;
}

void
DestroyTopMainWindow()
{
    if (g_bUseDefaultDisplay)
    {
        CheckNotRESULT(FALSE, DestroyWindow(ghwndTopMain));
        ghwndTopMain = NULL;
        CheckNotRESULT(FALSE, UnregisterClass(gszClassName, globalInst));
    }
    else
    {
        // blt whiteness to secondary display
        TDC hdc = NULL;
        CheckNotRESULT(NULL, hdc = myGetDC(NULL));
        CheckNotRESULT(FALSE, PatBlt(hdc, 0, 0, zx, zy, WHITENESS));
        CheckNotRESULT(0, myReleaseDC(NULL, hdc));

        // cleanup regkey
        CheckForRESULT(ERROR_SUCCESS, RegDeleteKey(HKEY_LOCAL_MACHINE, TEXT("CETK")));
    }

    if (gRotateAvail)
        ResetRotation();

    if (gResizeAvail)
        ResetResolutionSize();
}

void
IncreaseProgramMemory()
{
#ifdef UNDER_CE
    // leave atleast 1M of free storage, for the log file when running the CETK.
    DWORD dwStorageNeeded = 1000000;
    // 256k is the absolute minimum storage that's possible, so 260 should work.
    DWORD dwMinStorage = 260000;
    DWORD dwStorageToSet;

    DWORD   dwStorePages,
                dwRamPages,
                dwPageSize;
    DWORD   dwTmp;
    STORE_INFORMATION si;

    if(!GetStoreInformation(&si))
    {
        info(FAIL, TEXT("gdit: GetStoreInformation(): FAIL: err=%ld\n"), GetLastError());
        return;
    }

    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
    {
        info(FAIL, TEXT("gdit: GetSystemMemoryDivision(): FAIL: err=%ld\n"), GetLastError());
        return;
    }
    dwStorageToSet = max(((si.dwStoreSize - si.dwFreeSize) + dwStorageNeeded), dwMinStorage);

    info(DETAIL, TEXT("System Info: Storage in use is %d"), si.dwStoreSize - si.dwFreeSize);

    // leave 256k of storage or (used + 100k), whichever is larger.
    dwTmp = SetSystemMemoryDivision((DWORD) ceil(dwStorageToSet/dwPageSize));

    // Apps are not allowed to adjust the system memory division on trusted systems,
    // if this calls fails it's benign.
    if (dwTmp == SYSMEM_FAILED)
        return;

    GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize);
    info(DETAIL, TEXT("System Info: After increasing: dwStorePage=%lu  dwRamPages=%lu dwPageSize=%lu\r\n"), dwStorePages, dwRamPages, dwPageSize);
#endif
}

void
RestoreProgramMemory()
{
#ifdef UNDER_CE
    DWORD   dwStorePages,
            dwRamPages,
            dwPageSize;
    DWORD   dwTmp;

    if (!GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize))
    {
        info(FAIL, TEXT("gdit: GetSystemMemoryDivision(): FAIL: err=%ld\n"), GetLastError());
        return;
    }

    // set the division at the center now
    dwTmp = SetSystemMemoryDivision((dwStorePages + dwRamPages)/2);

    // Apps are not allowed to adjust the system memory division on trusted systems,
    // if this calls fails it's benign.
    if (dwTmp == SYSMEM_FAILED)
        return;

    GetSystemMemoryDivision(&dwStorePages, &dwRamPages, &dwPageSize);
    info(DETAIL, TEXT("System Info: After restoring: dwStorePage=%lu  dwRamPages=%lu dwPageSize=%lu\r\n"), dwStorePages, dwRamPages, dwPageSize);
#endif
}

void
InitFunctionPointers()
{
    gpfnGradientFill = NULL;
    gpfnAlphaBlend = NULL;
    ghinstCoreDLL = NULL;
    gpfnStartDoc = NULL;
    gpfnEndDoc = NULL;
    gpfnStartPage = NULL;
    gpfnEndPage = NULL;
    gpfnAbortDoc = NULL;
    gpfnSetAbortProc = NULL;
#ifdef UNDER_CE

    ghinstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));

    if(ghinstCoreDLL)
    {
        // if these fail, then the function is not available.  That is handled by the API wrappers
        gpfnGradientFill = (PFNGRADIENTFILL) GetProcAddress(ghinstCoreDLL, TEXT("GradientFill"));
        gpfnAlphaBlend = (PFNALPHABLEND) GetProcAddress(ghinstCoreDLL, TEXT("AlphaBlend"));
        gpfnStartDoc = (PFNSTARTDOC) GetProcAddress(ghinstCoreDLL, TEXT("StartDocW"));
        gpfnEndDoc = (PFNENDDOC) GetProcAddress(ghinstCoreDLL, TEXT("EndDoc"));
        gpfnStartPage = (PFNSTARTPAGE) GetProcAddress(ghinstCoreDLL, TEXT("StartPage"));
        gpfnEndPage = (PFNENDPAGE) GetProcAddress(ghinstCoreDLL, TEXT("EndPage"));
        gpfnAbortDoc = (PFNABORTDOC) GetProcAddress(ghinstCoreDLL, TEXT("AbortDoc"));
        gpfnSetAbortProc = (PFNSETABORTPROC) GetProcAddress(ghinstCoreDLL, TEXT("SetAbortProc"));
    }
    else info(DETAIL, TEXT("Failed to load CoreDLL.DLL"));
#else
    // set the function pointer to be the desktop gradient fill function
    gpfnGradientFill = &GradientFill;
    gpfnAlphaBlend = &AlphaBlend;
    gpfnStartDoc = &StartDoc;
    gpfnEndDoc = &EndDoc;
    gpfnStartPage = &StartPage;
    gpfnEndPage = &EndPage;
    gpfnAbortDoc = &AbortDoc;
    gpfnSetAbortProc = &SetAbortProc;
#endif
}

void
FreeFunctionPointers()
{
#ifdef UNDER_CE
    if(ghinstCoreDLL)
        FreeLibrary(ghinstCoreDLL);
#endif
}

void
InitDevice()
{
#ifdef UNDER_CE

    HKEY hKey;
    TCHAR szBuffer[256] = {NULL};
    TCHAR szBufferPM[256] = {NULL};
    ULONG nDriverName = countof(szBuffer);

    ghPowerManagement = NULL;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("System\\GDI\\Drivers"),
                        0, // Reseved. Must == 0.
                        0, // Must == 0.
                        &hKey) == ERROR_SUCCESS)
    {
        if (RegQueryValueEx(hKey,
                            TEXT("MainDisplay"),
                            NULL,  // Reserved. Must == NULL.
                            NULL,  // Do not need type info.
                            (BYTE *)szBuffer,
                            &nDriverName) == ERROR_SUCCESS)
        {            
            // the guid and device name has to be separated by a '\' and also there has to be the leading
            // '\' in front of windows, so the name is {GUID}\\Windows\ddi_xxx.dll
            _sntprintf(szBufferPM, countof(szBufferPM) - 1, TEXT("%s\\\\Windows\\%s"), PMCLASS_DISPLAY, szBuffer);
            info(DETAIL, TEXT("Driver in use as primary is %s"), szBufferPM);

            // only modify power manager on primary display if we will be testing on the primary
            if (g_bUseDefaultDisplay)
                ghPowerManagement = SetPowerRequirement(szBufferPM, D0, POWER_NAME, NULL, 0);
        }
        RegCloseKey(hKey);
    }

    if (!g_bUseDefaultDisplay)
    {
        info(DETAIL, TEXT("User specified driver is %s"), gszDisplayPath);

        // verify the user specified driver is not actually the primary
        if(CompareString(   LOCALE_INVARIANT,
                            NORM_IGNORECASE,
                            gszDisplayPath,
                            -1,
                            szBuffer,
                            -1) == CSTR_EQUAL)
        {
            info(DETAIL, TEXT("WARNING: %s is in use as the primary. Defaulting back to primary."), gszDisplayPath);
            g_bUseDefaultDisplay = true;
            ghPowerManagement = SetPowerRequirement(szBufferPM, D0, POWER_NAME, NULL, 0);
            return;
        }

        // verify the user specified driver is not the test display driver
        if(CompareString(   LOCALE_INVARIANT,
                            NORM_IGNORECASE,
                            gszDisplayPath,
                            -1,
                            VERIFYDRIVERDLLNAME,
                            -1) == CSTR_EQUAL)
        {
            info(DETAIL, TEXT("WARNING: %s is the test display driver. Defaulting back to primary."), gszDisplayPath);
            g_bUseDefaultDisplay = true;
            ghPowerManagement = SetPowerRequirement(szBufferPM, D0, POWER_NAME, NULL, 0);
            return;
        }

        // set the power management requirements for our secondary driver
        _sntprintf(szBufferPM, countof(szBufferPM) - 1, TEXT("%s\\\\Windows\\%s"), PMCLASS_DISPLAY, gszDisplayPath);
        ghPowerManagement = SetPowerRequirement(szBufferPM, D0, POWER_NAME, NULL, 0);

        // store the secondary display driver name in the registry for communication with the 
        // verification display driver (ddi_test.dll)
        DWORD dwDisposition;
        if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("CETK"),
                            0, 0, 0, NULL, 0,
                            &hKey,
                            &dwDisposition) == ERROR_SUCCESS)
        {
            if (!RegSetValueEx(hKey,
                                TEXT("DisplayDriver"),
                                NULL,
                                REG_SZ,
                                (BYTE *)gszDisplayPath,
                                (sizeof(szBuffer) / sizeof(szBuffer[0]))) == ERROR_SUCCESS)
            {
                info(DETAIL, TEXT("RegSetValueEx failed to set secondary driver name."));
            }
            RegCloseKey(hKey);
        }
        else
        {
        info(DETAIL, TEXT("RegCreateKeyEx failed to open key for secondary driver name."));
        }
    }
#endif
}

void
FreeDevice()
{
#ifdef UNDER_CE
    if(ghPowerManagement)
        ReleasePowerRequirement(ghPowerManagement);
#else
    // reset the whole desktop
    CheckNotRESULT(FALSE, InvalidateRect(NULL, NULL, TRUE));
#endif
}

TESTPROCAPI
LoopPrimarySurface_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;
    TDC hdc;

    // default, aFont is the last font in the list (the first font added to the system).
    aFont = rand() % numFonts;

    if(bUseAllSurfaces)
    {
        for(int i = 1; DCEntryStruct[i].szName; i++)
        {
            DCFlag = DCEntryStruct[i].dwVal;
            aLog->BeginLevel(0, TEXT("Testing surface %s, font %s."), DCName[DCFlag], fontArray[aFont].tszFullName);
            hdc = myGetDC();
            // if surface creation failed, then skip this surface, but continue the test on the rest of the surfaces, if there are any.
            if(!hdc)
                info(ABORT, TEXT("Failed to retrieve the surface required for testing.  Skipping this surface and continuing the test"));
            else
            {
                myReleaseDC(hdc);
                ((TESTPROC) lpFTE->dwUserData)(uMsg, tpParam, lpFTE);
            }
            aLog->EndLevel(TEXT("Testing surface %s completed."), DCName[DCFlag]);
        }
    }
    else
    {
        aLog->BeginLevel(0, TEXT("Testing surface %s, font %s."), DCName[DCFlag],  fontArray[aFont].tszFullName);
        hdc = myGetDC();
        // if surface creation failed, then skip this surface, but continue the test on the rest of the surfaces, if there are any.
        if(!hdc)
            info(ABORT, TEXT("Failed to retrieve the surface required for testing."));
        else
        {
            myReleaseDC(hdc);
            ((TESTPROC) lpFTE->dwUserData)(uMsg, tpParam, lpFTE);
        }
        aLog->EndLevel(TEXT("Testing surface %s completed."), DCName[DCFlag]);
    }

    return getCode();
}

typedef int (WINAPI * PFNSHFULLSCREEN)(HWND hwndRequester, DWORD dwState);
HINSTANCE g_hinstAygShell = NULL;
PFNSHFULLSCREEN g_pfnSHFullScreen = NULL;

void
InitShell(HWND hwndRequestor)
{
#ifdef UNDER_CE
    if (hwndRequestor)
    {
        g_hinstAygShell = LoadLibrary(TEXT("\\aygshell.dll"));

        if(g_hinstAygShell)
        {
            g_pfnSHFullScreen = (PFNSHFULLSCREEN) GetProcAddress(g_hinstAygShell, TEXT("SHFullScreen"));
            if(g_pfnSHFullScreen)
            {
                info(DETAIL, TEXT("Hiding the SIP button on the shell if it's available."));
                // on PPC, the SIP button can be an issue.  This call will fail on the standard shell because it's unsupported.
                if(!g_pfnSHFullScreen(hwndRequestor, SHFS_HIDESIPBUTTON))
                    info(DETAIL, TEXT("Failed to hide the sip button"));

                info(DETAIL, TEXT("Hiding the task bar on the shell if it's available."));

                if(!g_pfnSHFullScreen(hwndRequestor, SHFS_HIDETASKBAR))
                    info(DETAIL, TEXT("Hiding the task bar failed."));
            }
            else info(DETAIL, TEXT("SHFullScreen unavailable"));

        }
        else info(DETAIL, TEXT("Not running with aygshell."));
    }
    else info(DETAIL, TEXT("Not running on primary display."));
#endif
}

void
RehideShell(HWND hwndRequestor)
{
#ifdef UNDER_CE
    if(g_hinstAygShell && g_pfnSHFullScreen)
    {
        g_pfnSHFullScreen(hwndRequestor, SHFS_HIDESIPBUTTON);
        g_pfnSHFullScreen(hwndRequestor, SHFS_HIDETASKBAR);
    }
#endif
}

void
ReleaseShell(HWND hwndRequestor)
{
#ifdef UNDER_CE
    if(g_hinstAygShell)
    {
        if(g_pfnSHFullScreen)
        {
            info(DETAIL, TEXT("Restoring the SIP button on the shell if it's available."));
            g_pfnSHFullScreen(hwndRequestor, SHFS_SHOWSIPBUTTON);

            info(DETAIL, TEXT("Restoring the task bar on the shell if it's available."));
            g_pfnSHFullScreen(hwndRequestor, SHFS_SHOWTASKBAR);
        }
        FreeLibrary(g_hinstAygShell);
    }
#endif
}

void
InitGlobalStockObjects()
{
    HDC hdc = CreateCompatibleDC((HDC) NULL);

    g_hpenStock = (HPEN) GetStockObject(BLACK_PEN);
    g_hbrushStock = (HBRUSH) GetStockObject(BLACK_BRUSH);
    g_hpalStock = (HPALETTE) GetStockObject(DEFAULT_PALETTE);
    g_hbmpStock = (HBITMAP) GetCurrentObject(hdc, OBJ_BITMAP);
    g_hfontStock = (HFONT) GetStockObject(SYSTEM_FONT);

    DeleteObject(hdc);
}


//
// Sets Uniscribe mode to match
// the OS Uniscribe mode w/o actually
// hooking USP into the GDI subsystem.
//
// Returns the OS Uniscribe mode
//

BOOL
InitUSPMode(void)
{
    BOOL bMode = g_bUSP & USPMODE_MODE;
    BOOL bForce = g_bUSP & USPMODE_FORCE;
    if(bForce)
    {
        info(DETAIL, TEXT("Test Uniscribe mode forced to be %s"), bMode ? TEXT("ON") : TEXT("OFF"));
    }

    bMode = FALSE;     // assume uniscribe off/not present
    if(HMODULE hMod = LoadLibraryEx(TEXT("USPCE.DLL"), NULL, LOAD_LIBRARY_AS_DATAFILE))
    {
        // Uniscribe found & loaded
        // so is it enabled?
        bMode = TRUE;      // assume uniscribe is not disabled
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("System\\GDI\\Uniscribe"),
                            0, // Reseved. Must == 0.
                            0, // Must == 0.
                            &hKey) == ERROR_SUCCESS)
        {
            DWORD dwDisabled = 0x00;
            DWORD dwSize = sizeof(dwDisabled);
            if (RegQueryValueEx(hKey,
                                TEXT("DisableUniscribe"),
                                NULL,  // Reserved. Must == NULL.
                                NULL,  // Do not need type info.
                                (BYTE *) &dwDisabled,
                                &dwSize) == ERROR_SUCCESS)
            {
                if(dwDisabled != 0x00)
                    bMode = FALSE;
            }
            RegCloseKey(hKey);
        }
        FreeLibrary(hMod);
    }

    if(!bForce)
        g_bUSP = bMode;

    info(DETAIL, TEXT("OS Uniscribe mode is %s"), bMode ? TEXT("ON") : TEXT("OFF"));
    return bMode;
}


BOOL
GetUSPMode(void)
{
    return g_bUSP & USPMODE_MODE;
}


//
// Allows us to force the Uniscribe mode from the command
// line etc for testing tests in a mixed environment.
//

void
ForceUSPMode(BOOL bForce)
{
    g_bUSP = bForce | USPMODE_FORCE;
}

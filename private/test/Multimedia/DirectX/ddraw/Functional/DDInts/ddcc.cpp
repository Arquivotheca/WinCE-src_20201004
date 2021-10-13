////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: ddcc.cpp
//          BVT for the IDirectDrawColorControl interface functions.
//
//  Revision History:
//      07/08/97    lblanco     Created.
//      08/03/99    a-shende    Changed global member to match bvt convention.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDDCC

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

bool    Help_CreateColorControl(void);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module
static IDDCC            g_iddcc = { 0 };
static DDCAPS           g_ddcp;
static CFinishManager   *g_pfmDDCC = NULL;

#define CC_PRIMARYSUPPORT (g_ddcp.dwMiscCaps & DDMISCCAPS_COLORCONTROLPRIMARY)\
    ? TRUE : FALSE
#define CC_OVERLAYSUPPORT (g_ddcp.dwMiscCaps & DDMISCCAPS_COLORCONTROLOVERLAY)\
    ? TRUE : FALSE

////////////////////////////////////////////////////////////////////////////////
// InitDirectDrawColorControl
//  Creates various DirectDraw surface objects and gets the
//  IDirectDrawColorControl interface to all of them.
//
// Parameters:
//  pIDDCC          Pointer to the structure that will hold the interfaces.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitDirectDrawColorControl(LPIDDCC &pIDDCC, FNFINISH pfnFinish)
{
    // Clean out the pointer
    pIDDCC = NULL;

    // Create the objects, if necessary
    if(!g_iddcc.m_bValid && !Help_CreateColorControl())
    {
        FinishDirectDrawColorControl();
        return false;
    }

    // Do we have a CFinishManager object?
    if(!g_pfmDDCC)
    {
        g_pfmDDCC = new CFinishManager;
        if(!g_pfmDDCC)
        {
            FinishDirectDrawColorControl();
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmDDCC->AddFunction(pfnFinish);
    pIDDCC = &g_iddcc;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Help_CreateColorControl
//  Creates various DirectDraw surface objects and obtains the
//  IDirectDrawColorControl interface for them.
//
// Parameters:
//  None.
//
// Return value:
//  true if successful, false otherwise

bool Help_CreateColorControl(void)
{
    IDDS    *pIDDS;
    HRESULT hr;
    HRESULT hrResult = S_OK;

    // Clear out the structure
    memset(&g_iddcc, 0, sizeof(g_iddcc));

    // Create the surfaces
    if(!InitDirectDrawSurface(pIDDS, FinishDirectDrawColorControl))
        return false;

    Debug(LOG_COMMENT, TEXT("Running InitDirectDrawColorControl..."));

    // We'll do the same thing for every surface pointer. The macro below will
    // guarantee that we don't have any typos (or, if we do, everybody has the
    // same typo :)).
#define GET_IDDCC(a, b, c, d)\
    g_iddcc.b = (IDirectDrawColorControl *)PRESET;\
    hr = pIDDS->a->QueryInterface(\
        IID_IDirectDrawColorControl,\
        (void **)&g_iddcc.b);\
    if(E_NOINTERFACE == d && E_NOINTERFACE == hr) ;\
    else if(!CheckReturnedPointer(\
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,\
        g_iddcc.b,\
        c_szIDirectDrawSurface,\
        c_szQueryInterface,\
        hr,\
        c_szIID_IDirectDrawColorControl,\
        TEXT(c)))\
    {\
        g_iddcc.b = NULL;\
        return false;\
    }


    // Get Caps for ColorControl info
    memset(&g_ddcp, 0x0, sizeof(DDCAPS));
    g_ddcp.dwSize = sizeof(DDCAPS);

    // Get driver caps for check
    hr = pIDDS->m_pDD->GetCaps(&g_ddcp, NULL);
    if(FAILED(hr))
    {
        Debug(
            LOG_FAIL,
            TEXT("Could not get caps: 0x%08X"),
            hr);
        return false;
    }

#ifndef KATANA // not KATANA
    if(! CC_PRIMARYSUPPORT)
    {
        hrResult = E_NOINTERFACE;
    }
#endif  // not KATANA

    GET_IDDCC(m_pDDSPrimary, m_pDDCCPrimary, "for primary", hrResult)

#if QAHOME_OVERLAY
    if(pIDDS->m_bOverlaysSupported)
    {

#ifndef KATANA // not KATANA
        if(! CC_OVERLAYSUPPORT)
        {
            hrResult = E_NOINTERFACE;
        }
#endif  // not KATANA
        GET_IDDCC(m_pDDSOverlay1, m_pDDCCOverlay1, "for overlay", hrResult)
        g_iddcc.m_bOverlaysSupported = true;
    }
    else
    {
        g_iddcc.m_bOverlaysSupported = false;
    }
#endif // QAHOME_OVERLAY

    g_iddcc.m_bValid = true;

    // We don't need that macro anymore
#undef GET_IDDCC

    Debug(LOG_COMMENT, TEXT("Done with InitDirectDrawColorControl"));

    // If we got here, everything succeeded
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishDirectDrawColorControl
//  Releases all resources created by InitDirectDrawColorControl.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishDirectDrawColorControl(void)
{
    // Terminate any dependent objects
    if(g_pfmDDCC)
    {
        g_pfmDDCC->Finish();
        delete g_pfmDDCC;
        g_pfmDDCC = NULL;
    }

    if(g_iddcc.m_pDDCCPrimary)
        g_iddcc.m_pDDCCPrimary->Release();

#if QAHOME_OVERLAY
    if(g_iddcc.m_pDDCCOverlay1)
        g_iddcc.m_pDDCCOverlay1->Release();
#endif // QAHOME_OVERLAY

    memset(&g_iddcc, 0, sizeof(g_iddcc));
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDCC_AddRef_Release
//  Tests the IDirectDrawColorControl::AddRef and
//  IDirectDrawColorControl::Release methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDCC_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDCC   *pIDDCC;
    ITPR    nITPR = ITPR_PASS;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawColorControl(pIDDCC))
        return TPRFromITPR(g_tprReturnVal);

#ifndef KATANA // not KATANA
    // Get driver caps for check
    if(! CC_PRIMARYSUPPORT)
    {
        // Skip this test (no interface or object is available)
        nITPR |= ITPR_SKIP;
    }
    else
#endif  // not KATANA
    // Primary
    if(!Test_AddRefRelease(
        pIDDCC->m_pDDCCPrimary,
        c_szIDirectDrawColorControl))
    {
        nITPR |= ITPR_FAIL;
    }

#if QAHOME_OVERLAY
#ifndef KATANA // not KATANA
    // Get driver caps for check
    if(! CC_OVERLAYSUPPORT)
    {
        // Skip this test (no interface or object is available)
        nITPR |= ITPR_SKIP;
    }
    else
#endif  // not KATANA
    if(!Test_AddRefRelease(
        pIDDCC->m_pDDCCOverlay1,
        c_szIDirectDrawColorControl))
    {
        nITPR |= ITPR_FAIL;
    }
#endif // QAHOME_OVERLAY

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawColorControl,
            TEXT("AddRef and Release"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDCC_Get_SetColorControls
//  Tests the IDirectDrawColorControl::GetColorControls and
//  IDirectDrawColorControl::SetColorControls methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDCC_Get_SetColorControls(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDCC   *pIDDCC;
    ITPR    nITPR = ITPR_PASS;
#if SUPPORT_IDDCC_METHODS
    DWORD   dwFlags;
#endif // SUPPORT_IDDCC_METHODS
    HRESULT hr;
#if QAHOME_OVERLAY
    bool    bPrimary;
    UINT    nIndex;
#endif // QAHOME_OVERLAY

    IDirectDrawColorControl *pDDCC;
    DDCOLORCONTROL          ddcc1,
                            ddcc2;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawColorControl(pIDDCC))
        return TPRFromITPR(g_tprReturnVal);

    // If we have overlays, do a for loop. Otherwise, just to the primary
    // in one pass
#if QAHOME_OVERLAY
    for(nIndex = 0; nIndex < 2; nIndex++)
    {    
        // Primary goes first
        bPrimary = (0 == nIndex);
        if(bPrimary)
        {
            if(CC_PRIMARYSUPPORT)
            {
#endif // QAHOME_OVERLAY
                pDDCC = pIDDCC->m_pDDCCPrimary;
#if QAHOME_OVERLAY
            }
            else
            {
                nITPR |= ITPR_SKIP;
                continue;
            }
        }
        else
        {
            if(CC_OVERLAYSUPPORT)
            {
                pDDCC = pIDDCC->m_pDDCCOverlay1;
            }
            else
            {
                nITPR |= ITPR_SKIP;
                continue;
            }
        }
#endif // QAHOME_OVERLAY


        memset(&ddcc1, 0, sizeof(ddcc1));
        ddcc1.dwSize = sizeof(ddcc1);
        hr = pDDCC->GetColorControls(&ddcc1);

        // If the hardware supports color controls, we expect success.
        // Otherwise, we expect DDERR_UNSUPPORTED
#if SUPPORT_IDDCC_METHODS
        if(FAILED(hr))
        {
#ifdef KATANA
#define DB      RAID_KEEP
#define BUGID   487
#else // KATANA
#define DB      RAID_HPURAID
#define BUGID   0
#endif // KATANA
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawColorControl,
                c_szGetColorControls,
                hr,
                NULL,
#if QAHOME_OVERLAY
                bPrimary ?
#endif // QAHOME_OVERLAY
                    TEXT("for primary before call to SetColorControls")
#if QAHOME_OVERLAY
                    : TEXT("for overlay before call to SetColorControls")
#endif // QAHOME_OVERLAY
                ,
                DD_OK,
                DB,
                BUGID);
#undef DB
#undef BUGID
            nITPR |= ITPR_FAIL;
        }
        else
        {
            dwFlags = ddcc1.dwFlags;
            DebugBeginLevel(
                0,
                TEXT("Color controls for %s:"),
#if QAHOME_OVERLAY
                bPrimary ?
#endif // QAHOME_OVERLAY
                TEXT("primary")
#if QAHOME_OVERLAY
                : TEXT("overlay")
#endif // QAHOME_OVERLAY
                );

            // The macro below will make it easier to interpret the available
            // color controls. For each possible flag, if the flag is present
            // the macro sends a text representation of the flag to Kato.
#define CHECK_FLAG(x)   \
            if(dwFlags & (x)) \
                Debug(LOG_DETAIL, TEXT(#x));

            CHECK_FLAG(DDCOLOR_BRIGHTNESS)
            CHECK_FLAG(DDCOLOR_CONTRAST)
            CHECK_FLAG(DDCOLOR_HUE)
            CHECK_FLAG(DDCOLOR_SATURATION)
            CHECK_FLAG(DDCOLOR_SHARPNESS)
            CHECK_FLAG(DDCOLOR_GAMMA)
            CHECK_FLAG(DDCOLOR_COLORENABLE)

            // Check for unknown flags:
            dwFlags &= ~(
                DDCOLOR_BRIGHTNESS |
                DDCOLOR_CONTRAST |
                DDCOLOR_HUE |
                DDCOLOR_SATURATION |
                DDCOLOR_SHARPNESS |
                DDCOLOR_GAMMA |
                DDCOLOR_COLORENABLE);

        // We don't need that macro anymore
#undef CHECK_FLAG

            if(dwFlags)
            {
                // This is bad. What are those flags? At this point, we'll
                // consider this an error
                Debug(
                    LOG_FAIL,
                    TEXT("Other unknown flags: 0x%08X"),
                    dwFlags);
                nITPR |= ITPR_FAIL;
            }

            DebugEndLevel(TEXT(""));

            // Now, if we increase the value of every member by one, the ones
            // that are supported should change, while the others should remain
            // the same.
            ddcc2.dwSize        = sizeof(ddcc2);
            ddcc2.dwFlags       = DDCOLOR_BRIGHTNESS |DDCOLOR_CONTRAST |
                                  DDCOLOR_COLORENABLE | DDCOLOR_GAMMA |
                                  DDCOLOR_HUE | DDCOLOR_SATURATION |
                                  DDCOLOR_SHARPNESS;
            ddcc2.lBrightness   = ddcc1.lBrightness + 1;
            ddcc2.lContrast     = ddcc1.lContrast + 1;
            ddcc2.lHue          = ddcc1.lHue + 1;
            ddcc2.lSaturation   = ddcc1.lSaturation + 1;
            ddcc2.lSharpness    = ddcc1.lSharpness + 1;
            ddcc2.lGamma        = ddcc1.lGamma + 1;
            ddcc2.lColorEnable  = ddcc1.lColorEnable + 1;

            hr = pDDCC->SetColorControls(&ddcc2);
            if(FAILED(hr))
            {
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawColorControl,
                    c_szSetColorControls,
                    hr,
                    NULL,
#if QAHOME_OVERLAY
                    bPrimary ?
#endif // QAHOME_OVERLAY
                        TEXT("for primary")
#if QAHOME_OVERLAY
                        : TEXT("for overlays")
#endif // QAHOME_OVERLAY
                    );
                nITPR |= ITPR_FAIL;
            }
            else
            {
                // What changed?
                memset(&ddcc2, 0, sizeof(ddcc2));
                ddcc2.dwSize = sizeof(ddcc2);
                hr = pDDCC->GetColorControls(&ddcc2);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_FAILURE,
                        c_szIDirectDrawColorControl,
                        c_szGetColorControls,
                        hr,
                        NULL,
#if QAHOME_OVERLAY
                        bPrimary ?
#endif // QAHOME_OVERLAY
                            TEXT("for primary after call to SetColorControls")
#if QAHOME_OVERLAY
                            : TEXT("for overlay after call to SetColorControls")
#endif // QAHOME_OVERLAY
                        );
                    nITPR |= ITPR_FAIL;
                }
                else
                {
                    // The macro below will help us determine what values
                    // changed. For each possible flag, if the flag is present
                    // the macro checks that the associated value changed. If
                    // it is not present, the macro checks that the value did
                    // not change.
#define CHECK_FLAG(x, y) \
                    if(ddcc2.dwFlags & (x))\
                    {\
                        if(ddcc2.y != ddcc1.y + 1)\
                        {\
                            Debug(\
                                LOG_FAIL,\
                                FAILURE_HEADER TEXT("%s::%s was expected to")\
                                TEXT(" return a value of %ld for the %s")\
                                TEXT(" member since it set the %s flag")\
                                TEXT(" (returned %ld)"),\
                                c_szIDirectDrawColorControl,\
                                c_szGetColorControls,\
                                ddcc1.y + 1,\
                                TEXT(#y),\
                                TEXT(#x),\
                                ddcc2.y);\
                            nITPR |= ITPR_FAIL;\
                        }\
                    }\
                    else\
                    {\
                        if(ddcc2.y != ddcc1.y)\
                        {\
                            Debug(\
                                LOG_FAIL,\
                                FAILURE_HEADER TEXT("%s::%s was expected to")\
                                TEXT(" return a value of %ld for the %s")\
                                TEXT(" member since it did not set the %s")\
                                TEXT(" flag (returned %ld)"),\
                                c_szIDirectDrawColorControl,\
                                c_szGetColorControls,\
                                ddcc1.y,\
                                TEXT(#y),\
                                TEXT(#x),\
                                ddcc2.y);\
                            nITPR  |= ITPR_FAIL;\
                        }\
                    }

                    CHECK_FLAG(DDCOLOR_BRIGHTNESS, lBrightness)
                    CHECK_FLAG(DDCOLOR_CONTRAST, lContrast)
                    CHECK_FLAG(DDCOLOR_HUE, lHue)
                    CHECK_FLAG(DDCOLOR_SATURATION, lSaturation)
                    CHECK_FLAG(DDCOLOR_SHARPNESS, lSharpness)
                    CHECK_FLAG(DDCOLOR_GAMMA, lGamma)
                    CHECK_FLAG(DDCOLOR_COLORENABLE, lColorEnable)

                    // We don't need this macro anymore
#undef CHECK_FLAG
                }
            }
        }
#else // SUPPORT_IDDCC_METHODS
        if(hr != DDERR_UNSUPPORTED && hr != DDERR_INVALIDOBJECT)
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawColorControl,
                c_szGetColorControls,
                hr,
                NULL,
#if QAHOME_OVERLAY
                bPrimary ?
#endif // QAHOME_OVERLAY
                    TEXT("for primary")
#if QAHOME_OVERLAY
                    : TEXT("for overlay")
#endif // QAHOME_OVERLAY
                ,
                DDERR_UNSUPPORTED
#ifndef KATANA // not KATANA
                ,RAID_HPURAID,
                5815
#endif
                );


            nITPR |= ITPR_FAIL;
        }

        // Try the other method
        memset(&ddcc2, 0, sizeof(ddcc2));
        ddcc2.dwSize = sizeof(ddcc2);
        hr = pDDCC->SetColorControls(&ddcc2);
        if(hr != DDERR_UNSUPPORTED && hr != DDERR_INVALIDOBJECT)
        {
            Report(
                RESULT_UNEXPECTED,
                c_szIDirectDrawColorControl,
                c_szSetColorControls,
                hr,
                NULL,
#if QAHOME_OVERLAY
                bPrimary ?
#endif // QAHOME_OVERLAY
                    TEXT("for primary")
#if QAHOME_OVERLAY
                    : TEXT("for overlay")
#endif // QAHOME_OVERLAY
                ,
                DDERR_UNSUPPORTED
#ifndef KATANA // not KATANA
                , RAID_HPURAID,
                5815
#endif
                );

            nITPR |= ITPR_FAIL;
        }
#endif // SUPPORT_IDDCC_METHODS
#if QAHOME_OVERLAY
    }
#endif // QAHOME_OVERLAY

    if(nITPR == ITPR_PASS)
    {
        Report(
            RESULT_SUCCESS,
            c_szIDirectDrawColorControl,
            TEXT("GetColorControls and SetColorControls"));
    }

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDCC_QueryInterface
//  Tests the IDirectDrawColorControl::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDCC_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDCC       *pIDDCC;
    ITPR        nITPR = ITPR_PASS;
    INDEX_IID   iidValid[] = {
        INDEX_IID_IDirectDrawSurface,
        INDEX_IID_IDirectDrawColorControl
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawColorControl(pIDDCC))
        return TPRFromITPR(g_tprReturnVal);

    // Primary
#ifndef KATANA // not KATANA
    if(! CC_PRIMARYSUPPORT)
    {
        nITPR |= ITPR_SKIP;
    }
    else
#endif
        if(!Test_QueryInterface(
            pIDDCC->m_pDDCCPrimary,
            c_szIDirectDrawColorControl,
            iidValid,
            countof(iidValid),
            SPECIAL_IDDS_QI))
        {
            nITPR |= ITPR_FAIL;
        }
    
/*
    // Overlay (on hold until we figure this out)
#ifndef KATANA // not KATANA
    if(CC_OVERLAYSUPPORT)
    {
        nITPR |= ITPR_SKIP;
    }
    else
#endif
        if(!Test_QueryInterface(
            pIDDCC->m_pDDCCOverlay1,
            c_szIDirectDrawColorControl,
            iidValid,
            countof(iidValid)))
        {
            nITPR |= ITPR_FAIL;
        }
*/

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawColorControl, c_szQueryInterface);

    return TPRFromITPR(nITPR);
}

////////////////////////////////////////////////////////////////////////////////
// Help_GetColorControl
//  Creates a surface and returns its IDirectDrawColorControl interface.
//
// Parameters:
//  bPrimary        True to create a primary surface, false to create an
//                  overlay.
//  ppDDCC          Location where we'll return the created interface. If the
//                  function fails, we'll return NULL in this location.
//
// Return value:
//  true if successful, false otherwise.

bool Help_GetColorControl(bool bPrimary, IDirectDrawColorControl **ppDDCC)
{
    IDirectDraw             *pDD;
    IDirectDrawSurface      *pDDS;
    IDirectDrawColorControl *pDDCC;
    DDSURFACEDESC           ddsd;
    HRESULT                 hr;

    // Sanity check
    if(!ppDDCC)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Help_GetSurface was passed NULL"));
        return false;
    }
    *ppDDCC = NULL;

    if(!InitDirectDraw(pDD))
        return false;

    // Create the surface
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize     = sizeof(ddsd);
    if(bPrimary)
    {
        ddsd.dwFlags        = DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    }
    else
    {
        ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT;
        ddsd.dwWidth        = 640;
        ddsd.dwHeight       = 480;
    }

    pDDS = (IDirectDrawSurface *)PRESET;
    hr = pDD->CreateSurface(&ddsd, &pDDS, NULL);
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDS,
        c_szIDirectDraw,
        c_szCreateSurface,
        hr,
        bPrimary ? TEXT("primary") : TEXT("overlay")))
    {
        return false;
    }

    // Get the Color Control interface
    pDDCC = (IDirectDrawColorControl *)PRESET;
    hr = pDDS->QueryInterface(IID_IDirectDrawColorControl, (void **)&pDDCC);
    pDDS->Release();
    if(!CheckReturnedPointer(
        CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,
        pDDCC,
        c_szIDirectDrawSurface,
        c_szQueryInterface,
        hr,
        c_szIID_IDirectDrawColorControl))
    {
        return false;
    }

    *ppDDCC = pDDCC;

    return true;
}
#endif // TEST_IDDCC

////////////////////////////////////////////////////////////////////////////////

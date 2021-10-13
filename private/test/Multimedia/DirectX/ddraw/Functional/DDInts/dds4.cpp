////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dds4.cpp
//          BVTs for the IDirectDrawSurface4 interface functions.
//
//  Revision History:
//      08/06/99    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if 0

////////////////////////////////////////////////////////////////////////////////
// Local types

typedef INT (WINAPI *DDS4TESTPROC)(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
struct DDS4TESTINFO
{
    IDDS               *m_pIDDS4;
    IDirectDrawSurface *m_pDDS4;
    LPCTSTR             m_pszName;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

INT     Help_DDSIterate(DDS4TESTPROC, LPFUNCTION_TABLE_ENTRY);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDDS            g_idds4 = { 0 };
static CFinishManager   *g_pfmDDS4 = NULL;

////////////////////////////////////////////////////////////////////////////////
// InitDirectDrawSurface4
//  Creates various DirectDraw surface objects and gets the IDirectDrawSurface4
//  interface to all of them.
//
// Parameters:
//  pIDDS4          Pointer to the structure that will hold the interfaces.
//  pfnFinish       Pointer to a FinishXXX function. If this parameter is not
//                  NULL, the system understands that whenever this object is
//                  destroyed, the FinishXXX function must be called as well.
//
// Return value:
//  true if successful, false otherwise.

bool InitDirectDrawSurface4(LPIDDS4 &pIDDS4, FNFINISH pfnFinish)
{
    // Clean out the pointer
    pIDDS4 = NULL;

    // Create the objects, if necessary
    if(!g_idds4.m_pDDS4Primary && !Help_CreateSurfaces4())
    {
        FinishDirectDrawSurface4();
        return false;
    }

    // Do we have a CFinishManager object?
    if(!g_pfmDDS4)
    {
        g_pfmDDS4 = new CFinishManager;
        if(!g_pfmDDS4)
        {
            FinishDirectDrawSurface4();
            return false;
        }
    }

    // Add the FinishXXX function to the chain.
    g_pfmDDS4->AddFunction(pfnFinish);
    pIDDS4 = &g_idds4;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// FinishDirectDrawSurface4
//  Releases all resources created by InitDirectDrawSurface4.
//
// Parameters:
//  None.
//
// Return value:
//  None.

void FinishDirectDrawSurface4(void)
{
    // Terminate any dependent objects
    if(g_pfmDDS4)
    {
        g_pfmDDS4->Finish();
        delete g_pfmDDS4;
        g_pfmDDS4 = NULL;
    }

    if(g_idds4.m_pDDS4Primary)
        g_idds4.m_pDDS4Primary->Release();

    if(g_idds4.m_pDDS4SysMem)
        g_idds4.m_pDDS4SysMem->Release();

    if(g_idds4.m_pDDS4VidMem)
        g_idds4.m_pDDS4VidMem->Release();

#if TEST_ZBUFFER
    if(g_idds4.m_pDDS4ZBuffer)
        g_idds4.m_pDDS4ZBuffer->Release();
#endif // TEST_ZBUFFER

#if QAHOME_OVERLAY
    if(g_idds4.m_pDDS4Overlay1)
        g_idds4.m_pDDS4Overlay1->Release();

    if(g_idds4.m_pDDS4Overlay2)
        g_idds4.m_pDDS4Overlay2->Release();
#endif // QAHOME_OVERLAY

    memset(&g_idds4, 0, sizeof(g_idds4));
}

////////////////////////////////////////////////////////////////////////////////
// Help_CreateSurfaces4
//  Creates various DirectDraw surface objects and obtains the v4 interface for
//  all of them.
//
// Parameters:
//  None.
//
// Return value:
//  true if successful, false otherwise

bool Help_CreateSurfaces4(void)
{
    IDDS    *pIDDS;
    HRESULT hr;

    // Clear out the structure
    memset(&g_idds4, 0, sizeof(g_idds4));

    // Create the surfaces
    if(!InitDirectDrawSurface(pIDDS, FinishDirectDrawSurface4))
        return false;

    Debug(LOG_COMMENT, TEXT("Running InitDirectDrawSurface4..."));

    g_idds4.m_pDD = pIDDS->m_pDD;

    // We'll do the same thing for every surface pointer. The macro below will
    // guarantee that we don't have any typos (or, if we do, everybody has the
    // same typo :)).
#define GET_IDDS4(a, b, c)\
    if(pIDDS->a) \
    { \
        g_idds4.b = (IDirectDrawSurface4 *)PRESET;\
        hr = pIDDS->a->QueryInterface(\
            IID_IDirectDrawSurface4,\
            (void **)&g_idds4.b);\
        if(!CheckReturnedPointer(\
            CRP_RELEASEONFAILURE | CRP_ABORTFAILURES | CRP_ALLOWNONULLOUT,\
            g_idds4.b,\
            c_szIDirectDrawSurface,\
            c_szQueryInterface,\
            hr,\
            c_szIID_IDirectDrawSurface4,\
            TEXT(c)))\
        {\
            g_idds4.b = NULL;\
            return false;\
        } \
    }

    GET_IDDS4(m_pDDSPrimary, m_pDDS4Primary, "for primary")
    GET_IDDS4(m_pDDSSysMem, m_pDDS4SysMem, "for off-screen in system memory")
    GET_IDDS4(m_pDDSVidMem, m_pDDS4VidMem, "for off-screen in video memory")
#if TEST_ZBUFFER
    GET_IDDS4(m_pDDSZBuffer, m_pDDS4ZBuffer, "for Z Buffer")
#endif // TEST_ZBUFFER

#if QAHOME_OVERLAY
    if(pIDDS->m_bOverlaysSupported)
    {
        GET_IDDS4(m_pDDSOverlay1, m_pDDS4Overlay1, "for overlay #1")
        GET_IDDS4(m_pDDSOverlay2, m_pDDS4Overlay2, "for overlay #2")
        g_idds4.m_bOverlaysSupported = true;
    }
    else
    {
        g_idds4.m_bOverlaysSupported = false;
    }
#endif // QAHOME_OVERLAY

    // We don't need that macro anymore
#undef GET_IDDS4

    Debug(LOG_COMMENT, TEXT("Done with InitDirectDrawSurface4"));

    // If we got here, everything succeeded
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS4_AddRef_Release
//  Tests the IDirectDrawSurface4::AddRef and IDirectDrawSurface4::Release
//  methods.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS4_AddRef_Release(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    IDDS4   *pIDDS4;

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    if(!InitDirectDrawSurface4(pIDDS4))
        return TPRFromITPR(g_tprReturnVal);

    if(!Test_AddRefRelease(
        pIDDS4->m_pDDS4Primary,
        c_szIDirectDrawSurface4))
    {
        return TPR_FAIL;
    }

    Report(RESULT_SUCCESS, c_szIDirectDrawSurface4, TEXT("AddRef and Release"));

    return TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS4_QueryInterface
//  Tests the IDirectDrawSurface4::QueryInterface method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS4_QueryInterface(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    DDS4TESTINFO    *pDDS4TI;
    INT             nResult;
    INDEX_IID   iidValid[] = {
        INDEX_IID_IDirectDrawSurface,
        INDEX_IID_IDirectDrawSurface2,
        INDEX_IID_IDirectDrawSurface3,
        INDEX_IID_IDirectDrawSurface4,
#ifndef KATANA
        INDEX_IID_IDirectDrawSurface5,
        INDEX_IID_IDirectDrawColorControl
#endif // KATANA
    };

    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;


    pDDS4TI = (DDS4TESTINFO *)lpFTE->dwUserData;
    if(!pDDS4TI)
    {
        nResult = Help_DDS4Iterate(Test_IDDS4_QueryInterface, lpFTE);
        if(nResult == TPR_PASS)
            Report(RESULT_SUCCESS, c_szIDirectDrawSurface4, c_szQueryInterface);
    }
    else
    {
        BYTE bCnt = countof(iidValid);
        SPECIAL spcFlag = SPECIAL_IDDS_QI;

#ifndef KATANA
        // ColorControl support must be checked 
        DDSCAPS2 ddscp2;
        DDCAPS ddcp;
        HRESULT hr;

        // Don't flag for KATANA errors
        spcFlag = SPECIAL_NONE;

        memset(&ddscp2, 0x0, sizeof(DDSCAPS2));
        memset(&ddcp, 0x0, sizeof(DDCAPS));
        ddcp.dwSize = sizeof(DDCAPS);

        // Get surface caps for ColorControlSupport check
        hr = pDDS4TI->m_pDDS4->GetCaps(&ddscp2);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDrawSurface,
                c_szGetCaps,
                hr,
                NULL,
                pDDS4TI->m_pszName);
            return(ITPR_FAIL);
        }

        // Get driver caps for check
        hr = pDDS4TI->m_pIDDS4->m_pDD->GetCaps(&ddcp, NULL);
        if(FAILED(hr))
        {
            Report(
                RESULT_FAILURE,
                c_szIDirectDraw,
                c_szGetCaps,
                hr,
                NULL,
                pDDS4TI->m_pszName);
            return(ITPR_FAIL);
        }

        // Check our capabilities
        if(((ddscp2.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
            (ddcp.dwCaps2 & DDCAPS2_COLORCONTROLPRIMARY)) ||
           ((ddscp2.dwCaps & DDSCAPS_OVERLAY) &&
            (ddcp.dwCaps2 & DDCAPS2_COLORCONTROLOVERLAY)))
        {
            // Capability is supported, do nothing
            ;
        }
        else
        {
            // Remove ColorControl from the list
            bCnt--;
        }
#endif // not KATANA

        if(!Test_QueryInterface(
            pDDS4TI->m_pDDS4,
            c_szIDirectDrawSurface4,
            iidValid,
            bCnt,
            spcFlag))
        {
            nResult = ITPR_FAIL;
        }
        else
        {
            nResult = ITPR_PASS;
        }
    }
    
    return nResult;
}

////////////////////////////////////////////////////////////////////////////////
// Help_DDS4Iterate
//  Iterates through all surfaces and runs the specified test for each one.
//
// Parameters:
//  pfnTestProc     Address of the test function. This should be the same as the
//                  test function in the test function table. If the user data
//                  parameter is not NULL, the call originated from this
//                  function. Otherwise, it originated from the Tux shell.
//
// Return value:
//  The code returned by the test function, or TPR_FAIL if some other error
//  condition arises.

INT Help_DDS4Iterate(DDS4TESTPROC pfnTestProc, LPFUNCTION_TABLE_ENTRY pFTE)
{
    int     index;
    int     nCases;
    ITPR    nITPR = ITPR_PASS;

    DDS4TESTINFO            dds4ti;
    FUNCTION_TABLE_ENTRY    fte;

    if(!pfnTestProc || !pFTE)
    {
        Debug(
            LOG_ABORT,
            FAILURE_HEADER TEXT("Help_DDS4Iterate was passed NULL"));
        return TPR_ABORT;
    }

    if(!InitDirectDrawSurface4(dds4ti.m_pIDDS4))
        return TPRFromITPR(g_tprReturnVal);

    fte = *pFTE;
    fte.dwUserData  = (DWORD)&dds4ti;
#if QAHOME_OVERLAY
    if(dds4ti.m_pIDDS4->m_bOverlaysSupported)
        nCases = 6;
    else
#if TEST_ZBUFFER
        nCases = 4;
#else // TEST_ZBUFFER
        nCases = 3;
#endif // TEST_ZBUFFER
#else // QAHOME_OVERLAY
#if TEST_ZBUFFER
    nCases = 4;
#else // TEST_ZBUFFER
    nCases = 3;
#endif // TEST_ZBUFFER
#endif // QAHOME_OVERLAY

    for(index = 0; index < nCases; index++)
    {
        switch(index)
        {
        case 0:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4Primary;
            dds4ti.m_pszName    = TEXT("primary");
            break;

        case 1:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4SysMem;
            dds4ti.m_pszName    = TEXT("off-screen in system memory");
            break;

        case 2:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4VidMem;
            dds4ti.m_pszName    = TEXT("off-screen in video memory");
            break;

#if TEST_ZBUFFER
        case 3:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4ZBuffer;
            dds4ti.m_pszName    = TEXT("Z Buffer");
            break;
#endif // TEST_ZBUFFER

#if QAHOME_OVERLAY
        case 4:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4Overlay1;
            dds4ti.m_pszName    = TEXT("overlay #1");
            break;

        case 5:
            dds4ti.m_pDDS4      = dds4ti.m_pIDDS4->m_pDDS4Overlay2;
            dds4ti.m_pszName    = TEXT("overlay #2");
            break;
#endif // QAHOME_OVERLAY
        }

        DebugBeginLevel(0, TEXT("Testing %s..."), dds4ti.m_pszName);
        if(!dds4ti.m_pDDS4)
        {
            Debug(
                LOG_COMMENT,
                TEXT("Surface not created, skipping"));
        }
        else
        {
            __try
            {
                nITPR |= pfnTestProc(TPM_EXECUTE, 0, &fte);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Debug(
                    LOG_EXCEPTION,
                    FAILURE_HEADER TEXT("An exception occurred during the")
                    TEXT(" execution of this test"));
                nITPR |= ITPR_FAIL;
            }
        }
        DebugEndLevel(TEXT("Done with %s"), dds4ti.m_pszName);
    }

    return TPRFromITPR(nITPR);
}

#endif // TEST_IDDS4

////////////////////////////////////////////////////////////////////////////////

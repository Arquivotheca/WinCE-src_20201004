////////////////////////////////////////////////////////////////////////////////
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: dds3.cpp
//          BVTs for the IDirectDrawSurface interface functions.
//
//  Revision History:
//      07/08/97    lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#if TEST_IDDS3

////////////////////////////////////////////////////////////////////////////////
// Local types

typedef INT (WINAPI *DDS3TESTPROC)(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
struct DDS3TESTINFO
{
    IDDS               *m_pIDDS3;
    IDirectDrawSurface *m_pDDS3;
    LPCTSTR             m_pszName;
};

////////////////////////////////////////////////////////////////////////////////
// Local prototypes

INT     Help_DDS3Iterate(DDS3TESTPROC, LPFUNCTION_TABLE_ENTRY);

////////////////////////////////////////////////////////////////////////////////
// Globals for this module

static IDDS            g_idds3 = { 0 };
static CFinishManager   *g_pfmDDS3 = NULL;

////////////////////////////////////////////////////////////////////////////////
// Test_IDDS3_SetSurfaceDesc
//  Tests the IDirectDrawSurface::SetSurfaceDesc method.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passes, TPR_PASS if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IDDS3_SetSurfaceDesc(
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE)
{
    
    if(uMsg != TPM_EXECUTE)
        return TPR_NOT_HANDLED;

    return TPR_SKIP;

/*
    IDDS3   *pIDDS3;
    HRESULT hr;
    DWORD   dwSize;
    char    *pMemory = NULL;
    int     nIndex;
    ITPR    nITPR = ITPR_PASS;

    DDSURFACEDESC   ddsd;

    if(!InitDirectDrawSurface3(pIDDS3))
        return TPRFromITPR(g_tprReturnVal);

#if QAHOME_INVALIDPARAMS
    // Attempt to call the method for the primary
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    INVALID_CALL_E(
        pIDDS3->m_pDDS3Primary->SetSurfaceDesc(&ddsd, 0),
        c_szIDirectDrawSurface,
        c_szSetSurfaceDesc,
        TEXT("for primary surface"),
        DDERR_INVALIDSURFACETYPE);
#endif // QAHOME_INVALIDPARAMS


    // Init struct
    memset(&ddsd, 0x0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);


    // Call the method on an off-screen surface
    hr = pIDDS3->m_pDDS3SysMem->GetSurfaceDesc(&ddsd);
    if(FAILED(hr))
    {
        Report(
            RESULT_ABORT,
            c_szIDirectDrawSurface,
            c_szGetSurfaceDesc,
            hr,
            NULL,
            TEXT("for off-screen surface before call to")
            TEXT(" SetSurfaceDesc"));
        nITPR |= ITPR_ABORT;
    }
    else
    {
        // Calculate the amount of memory needed to hold this surface
        dwSize = ddsd.lPitch*ddsd.dwHeight;

        // Allocate a block of memory that large
        pMemory = new char[dwSize];
        if(!pMemory)
        {
            Debug(
                LOG_ABORT,
                FAILURE_HEADER TEXT("Not enough memory to allocate a")
                TEXT(" new surface area"));
            nITPR |= ITPR_ABORT;
        }
        else
        {
            // Write some data that we can read later
            for(nIndex = 0; nIndex < 10; nIndex++)
            {
                ((DWORD *)pMemory)[nIndex] = (DWORD)(PRESET*nIndex);
            }

            ddsd.dwFlags = DDSD_LPSURFACE;
            ddsd.lpSurface = pMemory;
            hr = pIDDS3->m_pDDS3SysMem->SetSurfaceDesc(&ddsd, 0);
            if(FAILED(hr))
            {
#ifdef KATANA
#define DB      RAID_KEEP
#define BUGID   488
#else // KATANA
#define DB      RAID_HPURAID
#define BUGID   0
#endif // KATANA
                Report(
                    RESULT_FAILURE,
                    c_szIDirectDrawSurface,
                    c_szSetSurfaceDesc,
                    hr,
                    NULL,
                    TEXT("for off-screen surface"),
                    0,
                    DB,
                    BUGID);
#undef DB
#undef BUGID
                nITPR |= ITPR_FAIL;
            }
            else
            {
                // Verify that the surface is pointing to the new
                // memory
                hr = pIDDS3->m_pDDS3SysMem->Lock(
                    NULL,
                    &ddsd,
                    DDLOCK_SURFACEMEMORYPTR,
                    NULL);
                if(FAILED(hr))
                {
                    Report(
                        RESULT_ABORT,
                        c_szIDirectDrawSurface,
                        c_szLock,
                        hr,
                        NULL,
                        TEXT("for off-screen surface after call to")
                        TEXT(" SetSurfaceDesc"));
                    nITPR |= ITPR_ABORT;
                }
                else
                {
#if 0
                    if(ddsd.lpSurface != pMemory)
                    {
                        Debug(
                            LOG_FAIL,
                            FAILURE_HEADER
#ifdef KATANA
                            TEXT("(Keep #488) ")
#else // KATANA
                            TEXT("(HPURAID #3239) ")
#endif // KATANA
                            TEXT("%s::%s was")
                            TEXT(" expected to return a surface location of")
                            TEXT(" 0x%08x after %s::%s returned")
                            TEXT(" success (returned 0x%08x)"),
                            c_szIDirectDrawSurface,
                            c_szLock,
                            pMemory,
                            c_szIDirectDrawSurface,
                            c_szSetSurfaceDesc,
                            ddsd.lpSurface);
                        nITPR |= ITPR_FAIL;
                    }
                    else
                    {
#endif // 0
                        // Read off the data
                        for(nIndex = 0; nIndex < 10; nIndex++)
                        {
                            if(((DWORD *)ddsd.lpSurface)[nIndex] !=
                               (DWORD)(PRESET*nIndex))
                            {
                                Debug(
                                    LOG_FAIL,
                                    FAILURE_HEADER TEXT("The data in the")
                                    TEXT(" surface after call to %s is")
                                    TEXT(" incorrect"),
                                    c_szSetSurfaceDesc);
                                nITPR |= ITPR_FAIL;
                                break;
                            }
                        }
                        if(nIndex == 10)
                        {
                            Debug(
                                LOG_PASS,
                                TEXT("The data in the surface")
                                TEXT(" after call to %s is correct"),
                                c_szSetSurfaceDesc);
                        }
#if 0
                    }
#endif // 0

                    pIDDS3->m_pDDS3SysMem->Unlock(ddsd.lpSurface);
                }
            }

            // Since we called SetSurfaceDesc, we need to release this surface
            // before any more tests can be run on it. In particular, we need
            // to release it before we free our memory. The easiest way to do
            // this is to reset all surfaces. We do this even if the call to
            // SetSurfaceDesc appeared to fail, just in case.
            FinishDirectDrawSurface();
        }
    }

    if(pMemory)
        delete[] pMemory;

    if(nITPR == ITPR_PASS)
        Report(RESULT_SUCCESS, c_szIDirectDrawSurface, c_szSetSurfaceDesc);

    return TPRFromITPR(nITPR);
*/
}


#endif // TEST_IDDS3

////////////////////////////////////////////////////////////////////////////////

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
//

#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>

#include "IDDrawTests.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_IDirectDraw
{

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::EnumModesCallback
    //  Callback function.
    //
    HRESULT WINAPI CTest_EnumDisplayModes::Callback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext)
    {
        CheckCondition(lpContext == NULL, "Callback Called with invalid Context", (HRESULT)DDENUMRET_CANCEL);

        // Get a pointer to the test object that we passed in
        CTest_EnumDisplayModes *ptest = static_cast<CTest_EnumDisplayModes*>(lpContext);

        // Do a number of sanity checks
        CheckCondition(!PresetUty::IsPreset(ptest->m_dwPreset), "DirectDrawEnumerate Callback called with invalid Context", (HRESULT)DDENUMRET_CANCEL);
        QueryCondition(!ptest->m_fCallbackExpected, "DirectDrawEnumerate Callback called when not expected", ptest->m_tr |= trFail);
        QueryCondition(lpDDSurfaceDesc == NULL, "DirectDrawEnumerate Callback received lpcstrDescription == NULL", ptest->m_tr |= trFail);

        // Log the callback information
        dbgout
            << "Surface Description #" << ptest->m_nCallbackCount
            << ":" << *lpDDSurfaceDesc << endl;

        // Increment our callback count
        ptest->m_nCallbackCount++;

        // We don't try creating because that is not supported (the pixel format is not defined, other than bpp)

        // Even if we've failed, keep iterating (since we won't
        // loose the failure on the next iteration).
        return (HRESULT)DDENUMRET_OK;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_EnumDisplayModes::Test
    //  Tests the EnumDisplayModes Method.
    //
    eTestResult CTest_EnumDisplayModes::TestIDirectDraw()
    {
        HRESULT hr;

        m_tr = trPass;
        m_nCallbackCount = 0;
        dbgout << "Calling EnumDisplayModes with DDEDM_REFRESHRATES and NULL SurfaceDesc" << endl;

        m_fCallbackExpected = true;
        hr = m_piDD->EnumDisplayModes(0, NULL, this, Callback);
        m_fCallbackExpected = false;
        CheckHRESULT(hr, "EnumDisplayModes Failed", trFail);

        // Output the number of callbacks we got
        dbgout(LOG_DETAIL) << "Enumerated " << m_nCallbackCount << " modes(s)" << endl;

        // Make sure the callback was called at least once
        QueryCondition(m_nCallbackCount < 1, "EnumDisplayModes failed to call callback", trFail);

        return m_tr;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // CTest_GetCaps::Test
    //
    eTestResult CTest_GetCaps::TestIDirectDraw()
    {
        CDDCaps ddcapsHAL1,
                ddcapsHAL2;
        eTestResult tr = trPass;
        HRESULT hr;

        // HAL Only
        dbgout << "Getting Caps for HAL only" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL1, NULL);
        CheckHRESULT(hr, "GetCaps HAL Only", trFail);

        // Both HEL and HAL
        dbgout << "Getting Caps for HAL again" << endl;
        hr = m_piDD->GetCaps(&ddcapsHAL2, NULL);
        CheckHRESULT(hr, "GetCaps HAL second time", trFail);

        // Compare the results from the two calls
        dbgout << "Comparing results from different calls" << endl;
        QueryCondition(memcmp(&ddcapsHAL1, &ddcapsHAL2, sizeof(ddcapsHAL1)), "HAL CAPs differed between calls", tr |= trFail);

        // Display the caps that we recieved
        dbgout(LOG_DETAIL)
            << indent
            << "HAL Caps:" << ddcapsHAL1 << endl
            << unindent;
        // Do some Caps checking

        // if overlay surfaces are supported, make sure dwMaxVisibleOverlays is not zero
        if (ddcapsHAL1.ddsCaps.dwCaps & DDSCAPS_OVERLAY && ddcapsHAL1.dwMaxVisibleOverlays == 0)
        {
            dbgout(LOG_FAIL)
                << "If Overlay surfaces are supported, dwMaxVisibleOverlays must be greater than 0"
                << endl;
            tr |= trFail;
        }

         return tr;
    }

    const DWORD VBID_MINIMUMWIDTH = 4;
    const DWORD VBID_MAXIMUMWIDTH = 4;
    const DWORD VBID_MINIMUMHEIGHT = 1;
    const DWORD VBID_MAXIMUMHEIGHT = 1;
    const DWORD VBID = MAKEFOURCC('V', 'B', 'I', 'D');

//    // Maximum Lines: An upper bound on the number of possible lines in a display.
//    // This isn't meant to be a hard upper bound; rather it is meant to be used
//    // when generating random invalid data.
//    const DWORD VBID_MAXIMUMLINES = 4096;
//
//    const int VALIDTYPES = 4;

    namespace priv_VBIDHelpers
    {
        bool IsFourCCSupported(IDirectDraw * piDD, DWORD dwFourCC)
        {
            // Determine if VBID is a supported FOURCC code
                // get the list of the supported fourccs
            DWORD FourCCCount = 0;
            DWORD * pFourCCs = NULL;
            bool SupportsVBID = false;
            HRESULT hr;


            hr = piDD->GetFourCCCodes(&FourCCCount, NULL);
            CheckHRESULT(hr, "GetFourCCCodes(NULL)", false)
            else if (FourCCCount > 0)
            {
                pFourCCs = new DWORD[FourCCCount];
                CheckCondition(NULL==pFourCCs, "FourCC Allocation for " << FourCCCount << " entries", false);

                hr = piDD->GetFourCCCodes(&FourCCCount, pFourCCs);
                QueryHRESULT(hr, "GetFourCCCodes", /*nothing*/)
                else
                {
                    // search the list for VBID
                    for (UINT i = 0; i < FourCCCount; ++i)
                    {
                        if (dwFourCC == pFourCCs[i])
                        {
                            SupportsVBID = true;
                            break;
                        }
                    }
                }

            }
            if (pFourCCs)
            {
                delete[] pFourCCs;
                pFourCCs = NULL;
            }
            return SupportsVBID;
        }

        void GenerateValidVBIDDesc(CDDSurfaceDesc * pcddsd)
        {
            pcddsd->Clear();
            pcddsd->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
            pcddsd->ddsCaps.dwCaps = 0;
            pcddsd->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
            pcddsd->ddpfPixelFormat.dwFlags = DDPF_FOURCC;
            pcddsd->ddpfPixelFormat.dwFourCC = VBID;
            pcddsd->dwWidth = VBID_MINIMUMWIDTH;
            pcddsd->dwHeight = VBID_MINIMUMHEIGHT;
        }
    }

    eTestResult CTest_VBIDSurfaceCreation::TestIDirectDraw()
    {
        eTestResult tr = trPass;
        HRESULT hr;
        CComPointer<IDirectDrawSurface> piVbidSurface;
        CComPointer<IDirectDrawSurface> piOtherSurface;
        CDDSurfaceDesc SurfaceDesc;
        CDDSurfaceDesc ValidSurfaceDesc;

        priv_VBIDHelpers::GenerateValidVBIDDesc(&ValidSurfaceDesc);

        memset(&SurfaceDesc, 0x00, sizeof(SurfaceDesc));
        SurfaceDesc.dwSize = sizeof(SurfaceDesc);

        bool SupportsVBID = priv_VBIDHelpers::IsFourCCSupported(m_piDD.AsInParam(), VBID);

        // If vbid is supported run through test cases
        if (SupportsVBID)
        {
            // Invalid creation:
            // try creating with height = 2 (width = 3)
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwHeight = VBID_MAXIMUMHEIGHT + 1;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid height "
                    << SurfaceDesc.dwHeight
                    << " (height must be 1)"
                    << endl;
                tr |= trFail;
            }

            // try creating with height = -1
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwHeight = static_cast<DWORD> (-1); 
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid height "
                    << SurfaceDesc.dwHeight
                    << " (height must be 1)"
                    << endl;
                tr |= trFail;
            }
            // try creating with height = 0
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwHeight = 0;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid height "
                    << SurfaceDesc.dwHeight
                    << " (height must be 1)"
                    << endl;
                tr |= trFail;
            }

            // try creating with width = 1
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwWidth = 1;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid width "
                    << SurfaceDesc.dwWidth
                    << " (width must be >= "
                    << VBID_MINIMUMWIDTH
                    << ")"
                    << endl;
                tr |= trFail;
            }

            // try creating with width = minimum - 1
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwWidth = VBID_MINIMUMWIDTH-1;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid width "
                    << SurfaceDesc.dwWidth
                    << " (width must be >= "
                    << VBID_MINIMUMWIDTH
                    << ")"
                    << endl;
                tr |= trFail;
            }

            // try creating with width = 0
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwWidth = 0;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid width "
                    << SurfaceDesc.dwWidth
                    << " (width must be >= "
                    << VBID_MINIMUMWIDTH
                    << ")"
                    << endl;
                tr |= trFail;
            }

            // try creating with width = -1
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwWidth = static_cast<DWORD>(-1); 
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid width "
                    << SurfaceDesc.dwWidth
                    << endl;
                tr |= trFail;
            }

            // try creating with width = maximum + 1
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwWidth = static_cast<DWORD>(-1); 
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with invalid width "
                    << SurfaceDesc.dwWidth
                    << endl;
                tr |= trFail;
            }

            // try creating with backbuffers (as flipping chain)
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.dwFlags |= DDSD_BACKBUFFERCOUNT;
            SurfaceDesc.dwBackBufferCount = 1;
            SurfaceDesc.ddsCaps.dwCaps |= DDSCAPS_FLIP;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface with backbuffer (VBID surfaces cannot have backbuffers)"
                    << endl;
                tr |= trFail;
            }

            // try creating as overlay
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            SurfaceDesc.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created VBID surface as overlay (VBID surfaces cannot be overlays)"
                    << endl;
                tr |= trFail;
            }

            // Valid creation
                // create with Height = 1, width = 3
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            hr = m_piDD->CreateSurface(&SurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            CheckHRESULT(hr,
                "Driver failed creating valid VBID surface" << endl
                << SurfaceDesc << endl,
                trFail);

                // try creating a second vbid surface
            memcpy(&SurfaceDesc, &ValidSurfaceDesc, sizeof(ValidSurfaceDesc));
            hr = m_piDD->CreateSurface(&SurfaceDesc, piOtherSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver created more than one VBID surface. Only one VBID surface at a time is allowed" << endl;
                tr |= trFail;
            }

                // try flipping vbid surface
            hr = piVbidSurface->Flip(NULL, DDFLIP_WAITNOTBUSY);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Flip succeeded on VBID surface. This is not supposed to be possible." << endl;
                tr |= trFail;
            }


                    // try blting to/from vbid (and from vbid to vbid)
            RECT rcDest = {0, 0, 3, 1};
            RECT rcSrc = {0, 0, 3, 1};
            // Blt from primary to vbid
            hr = piVbidSurface->Blt(&rcDest, piOtherSurface.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Blting to VBID from primary succeeded. Any Blt involving VBID surface should fail." << endl;
                tr |= trFail;
            }

                // create primary surface
            DDSURFACEDESC PrimarySurfaceDesc;
            memset(&PrimarySurfaceDesc, 0x00, sizeof(PrimarySurfaceDesc));
            PrimarySurfaceDesc.dwFlags = DDSD_CAPS;
            PrimarySurfaceDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
            hr = m_piDD->CreateSurface(&PrimarySurfaceDesc, piOtherSurface.AsOutParam(), NULL);
            CheckHRESULT(hr, "Aborting: Unable to create primary surface for testing with VBID surface.", trAbort);

            // Blt from vbid to primary
            hr = piOtherSurface->Blt(&rcDest, piVbidSurface.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Blting to primary from VBID succeeded. Any Blt involving VBID surface should fail." << endl;
                tr |= trFail;
            }

            // Blt from vbid to vbid
            SetRect(&rcSrc, 0, 0, 2, 1);
            SetRect(&rcDest, 1, 0, 3, 1);
            hr = piVbidSurface->Blt(&rcDest, piVbidSurface.AsInParam(), &rcSrc, DDBLT_WAITNOTBUSY, NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Blting to VBID from VBID succeeded. Any Blt involving VBID surface should fail." << endl;
                tr |= trFail;
            }

            // Colorfill to vbid
            DDBLTFX BltFx;
            memset(&BltFx, 0x00, sizeof(BltFx));
            BltFx.dwSize = sizeof(BltFx);
            BltFx.dwFillColor = 0xab;
            hr = piVbidSurface->Blt(NULL, NULL, NULL, DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, &BltFx);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Colorfill Blt to VBID succeeded. Any Blt involving VBID surface should fail." << endl;
                tr |= trFail;
            }

                // try locking surface
            hr = piVbidSurface->Lock(NULL, &SurfaceDesc, DDLOCK_WAITNOTBUSY, NULL);
            if (FAILED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Lock failed on VBID surface. Locking is necessary. " << hr << endl;
                tr |= trFail;
            }
                // try unlocking surface without modifying data (should return S_OK or S_FALSE.
            hr = piVbidSurface->Unlock(NULL);
            if (FAILED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Unlock failed on locked VBID surface. If data is invalid, Unlock must return S_FALSE instead of failure. " << hr << endl;
                tr |= trFail;
            }

                // try unlocking surface again (should fail)
            hr = piVbidSurface->Unlock(NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Unlock succeeded when surface was not locked. " << hr << endl;
                tr |= trFail;
            }

        }
        else
        {
            // If vbid is not supported, try creating a vbid surface (should fail)
            hr = m_piDD->CreateSurface(&ValidSurfaceDesc, piVbidSurface.AsOutParam(), NULL);
            if (SUCCEEDED(hr))
            {
                dbgout(LOG_FAIL)
                    << "Driver reported that VBID is unsupported FOURCC, but succeeded in created VBID surface." << endl;
                tr |= trFail;
            }
        }

        return tr;
    }


    namespace priv_VBIDSurfaceDataHelpers
    {
        WORD GetRandomGoodData()
        {
            BYTE bMaxValid = 0x3f;
            BYTE bMinValid = 0x20;
            WORD wRetData = 0;
            BYTE * pRetData = (BYTE*)&wRetData;
            UINT uNumber = 0;
            for (int i = 0; i < 2; ++i)
            {
                while (pRetData[i] < bMinValid || pRetData[i] > bMaxValid)
                {
                    rand_s(&uNumber);
                    pRetData[i] = uNumber & 0xff;
                }
            }
            return wRetData;
        }

        WORD GetRandomData()
        {
             UINT uNumber1 = 0;
             UINT uNumber2 = 0;
             rand_s(&uNumber1);
             rand_s(&uNumber2);
                
            WORD wRetData = static_cast<WORD>(uNumber1 | (uNumber2 << 15));
            return wRetData;
        }

        /**
         * SendDataToSurface
         *
         *     Locks a DDraw VBID surface, populates the surface with the
         *     data, and Unlocks the surface.
         *
         * Parameters
         *
         *    pVbidSurface
         *        The DirectDraw surface (created with FOURCC_VBID)
         *
         *    bType
         *        The VBID TypeID to place in the surface
         *
         *    bLine
         *        The line to use
         *
         *    pData
         *        The actual data bytes to copy onto the surface
         *
         *    phr
         *        [out] Pointer to an HRESULT that will receive the value
         *        returned by Unlock if return is trPass, otherwise the
         *        value returned by Lock.
         *
         * Return Value :   eTestResult
         *     The test result of this portion of the test. trFail: phr
         *
         */
        eTestResult SendDataToSurface(IDirectDrawSurface * pVbidSurface, BYTE bType, BYTE bLine, BYTE * pData, int iDataSize, HRESULT * phr)
        {
            DDSURFACEDESC LockedSurfaceDesc;
            // Lock the surface
            memset(&LockedSurfaceDesc, 0x00, sizeof(DDSURFACEDESC));
            LockedSurfaceDesc.dwSize = sizeof(DDSURFACEDESC);

            *phr = pVbidSurface->Lock(NULL, &LockedSurfaceDesc, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(*phr, "VBID Lock", trFail);

            ((BYTE*)LockedSurfaceDesc.lpSurface)[0] = bType;
            ((BYTE*)LockedSurfaceDesc.lpSurface)[1] = bLine;
            memcpy(((BYTE*)LockedSurfaceDesc.lpSurface)+4, pData, iDataSize);

            *phr = pVbidSurface->Unlock(NULL);
            return trPass;
        }

        struct ValidVbid {
            BYTE Value;
            bool AlwaysValid;
        };

        bool IsValid(BYTE bValue, ValidVbid ValidValues[], int cValids)
        {
            for (int i = 0; i < cValids; ++i)
            {
                if (ValidValues[i].Value == bValue && ValidValues[i].AlwaysValid)
                {
                    return true;
                }
            }
            return false;
        }
        bool CouldBeValid(BYTE bValue, ValidVbid ValidValues[], int cValids)
        {
            for (int i = 0; i < cValids; ++i)
            {
                if (ValidValues[i].Value == bValue)
                {
                    return true;
                }
            }
            return false;
        }
    }

    eTestResult CTest_VBIDSurfaceData::TestIDirectDraw()
    {
        using namespace priv_VBIDSurfaceDataHelpers;
        const int DATAITERS = 1000;
        eTestResult tr = trPass;
        CDDSurfaceDesc cddsd;
        CComPointer<IDirectDrawSurface> piDDSVbid;
        // i will be used for iterating through the valid structures
        int i;
        WORD wData;
        BYTE * pData = (BYTE*)&wData;
        HRESULT hr;

        // Values to use when checking if the VBID data was properly handled
        eTestResult trTemp;
        HRESULT hrTemp;

        ValidVbid ValidTypes[] = {
            0, true,
            1, false,
            0xff, true,
        };

        ValidVbid ValidFields[] = {
            1, true,
            2, false,
            0xff, true,
        };

        bool SupportsVBID = priv_VBIDHelpers::IsFourCCSupported(m_piDD.AsInParam(), VBID);

        if (!SupportsVBID)
        {
            dbgout(LOG_SKIP) << "Driver does not support VBID FourCC, skipping" << endl;
            return trSkip;
        }

        priv_VBIDHelpers::GenerateValidVBIDDesc(&cddsd);

        hr = m_piDD->CreateSurface(&cddsd, piDDSVbid.AsOutParam(), NULL);
        CheckHRESULT(hr, "CreateSurface with valid VBID description", trAbort);

        // Run through with invalid type and field data
        // invalid field with valid type
        for(i = 0; i < countof(ValidTypes); ++i)
        {
            for (int iInvalidField = 0; iInvalidField <= 0xff; ++iInvalidField)
            {
                if (CouldBeValid(iInvalidField, ValidFields, countof(ValidFields)))
                {
                    continue;
                }
                wData = GetRandomGoodData();
                trTemp = SendDataToSurface(piDDSVbid.AsInParam(), ValidTypes[i].Value, (BYTE)iInvalidField, (BYTE*)&wData, sizeof(wData), &hrTemp);
                if (trTemp != trPass)
                {
                    CheckHRESULT(hrTemp, "SendDataToSurfac", trAbort);
                }
                QueryHRESULT(hrTemp,
                    "Unlock of VBID surface failed (should never fail): Type (" << ValidTypes[i].Value
                    << ") Field (" << (BYTE)iInvalidField << ") "
                    "Surface Data (" << HEX(pData[0]) << "," << HEX(pData[1]) << ")",
                    tr |= trFail)
                else QueryCondition(hrTemp == S_OK,
                    "Unlock returned S_OK with invalid field number " << iInvalidField << " (should have returned S_FALSE)",
                    tr|=trFail);
            }
        }
        // invalid type with valid field
        for(i = 0; i < countof(ValidFields); ++i)
        {
            for (int iInvalidType = 0; iInvalidType <= 0xff; ++iInvalidType)
            {
                if (CouldBeValid(iInvalidType, ValidTypes, countof(ValidTypes)))
                {
                    continue;
                }
                wData = GetRandomGoodData();
                trTemp = SendDataToSurface(piDDSVbid.AsInParam(), ValidTypes[i].Value, (BYTE)iInvalidType, (BYTE*)&wData, sizeof(wData), &hrTemp);
                if (trTemp != trPass)
                {
                    CheckHRESULT(hrTemp, "SendDataToSurface", trAbort);
                }
                QueryHRESULT(hrTemp,
                    "Unlock of VBID surface failed (should never fail): Type (" << (BYTE)iInvalidType
                    << ") Field (" << ValidFields[i].Value << ") "
                    "Surface Data (" << HEX(pData[0]) << "," << HEX(pData[1]) << ")",
                    tr |= trFail)
                else QueryCondition(hrTemp == S_OK,
                    "Unlock returned S_OK with invalid type number " << iInvalidType << " (should have returned S_FALSE)",
                    tr|=trFail);
            }
        }

        // Run through with valid type and field data with random bytes.
        for(int iType = 0; iType < countof(ValidTypes); ++iType)
        {
            for(int iField = 0; iField < countof(ValidFields); ++iField)
            {
                for(i = 0; i < DATAITERS; ++i)
                {
                    wData = GetRandomData();
                    trTemp = SendDataToSurface(piDDSVbid.AsInParam(), ValidTypes[iType].Value, ValidFields[iField].Value, (BYTE*)&wData, sizeof(wData), &hrTemp);
                    if (trTemp != trPass)
                    {
                        CheckHRESULT(hrTemp, "SendDataToSurface", trAbort);
                    }
                    QueryHRESULT(hrTemp,
                        "Unlock of VBID surface failed (should never fail): Type (" << ValidTypes[iType].Value
                        << ") Field (" << ValidFields[iField].Value << ") "
                        "Surface Data (" << HEX(pData[0]) << "," << HEX(pData[1]) << ")",
                        tr |= trFail);
                }
            }
        }

        // Run through with valid type and field and valid bytes.
        for(int iType = 0; iType < countof(ValidTypes); ++iType)
        {
            for(int iField = 0; iField < countof(ValidFields); ++iField)
            {
                for(i = 0; i < DATAITERS; ++i)
                {
                    wData = GetRandomGoodData();
                    trTemp = SendDataToSurface(piDDSVbid.AsInParam(), ValidTypes[iType].Value, ValidFields[iField].Value, (BYTE*)&wData, sizeof(wData), &hrTemp);
                    if (trTemp != trPass)
                    {
                        CheckHRESULT(hrTemp, "SendDataToSurface", trAbort);
                    }
                    QueryHRESULT(hrTemp,
                        "Unlock of VBID surface failed (should never fail): Type (" << ValidTypes[iType].Value
                        << ") Field (" << ValidFields[iField].Value << ") "
                        "Surface Data (" << HEX(pData[0]) << "," << HEX(pData[1]) << ")",
                        tr |= trFail)
                    else QueryCondition(hrTemp==S_FALSE && ValidTypes[iType].AlwaysValid && ValidFields[iField].AlwaysValid,
                        "Unlock returned S_FALSE with known valid type and field (should have returned S_OK)"
                        << " Type,Field,Byte1,Byte2: " << ValidTypes[iType].Value << ", " << ValidFields[iField].Value << ", "
                        << HEX(pData[0]) << ", " << HEX(pData[1]),
                        tr|=trFail);
                }
            }
        }
        return tr;
    }

    namespace priv_CreateSurfaceVerification
    {
        bool ConvertColor(DWORD * pdwOutColor, DWORD dwInColor, const DDPIXELFORMAT * pOutFormat)
        {
            bool bPremultAlpha = false;
            bool bAlpha = false;
            BYTE inR = GetRValue(dwInColor);
            BYTE inG = GetGValue(dwInColor);
            BYTE inB = GetBValue(dwInColor);
            BYTE inA = 0xff & (dwInColor >> 24);
            DWORD outR, outG, outB, outA;

            if (!(pOutFormat->dwFlags & DDPF_RGB))
            {
                return false;
            }

            if (!pdwOutColor)
            {
                return false;
            }

            bAlpha = true && (pOutFormat->dwFlags & DDPF_ALPHAPIXELS);
            bPremultAlpha = bAlpha && (pOutFormat->dwFlags & DDPF_ALPHAPREMULT);

            if (0xff == inA)
            {
                bAlpha = bPremultAlpha = false;
            }

            double fTempR, fTempG, fTempB, fTempA;
            fTempR = ((double)inR) * ((double)pOutFormat->dwRBitMask) / ((double)0xff);
            fTempG = ((double)inG) * ((double)pOutFormat->dwGBitMask) / ((double)0xff);
            fTempB = ((double)inB) * ((double)pOutFormat->dwBBitMask) / ((double)0xff);
            fTempA = ((double)inA) * ((double)pOutFormat->dwRGBAlphaBitMask) / ((double)0xff);
            if (bPremultAlpha)
            {
                fTempR *= inA / (double)0xff;
                fTempG *= inA / (double)0xff;
                fTempB *= inA / (double)0xff;
            }

            outR = ((DWORD)fTempR) & pOutFormat->dwRBitMask;
            outG = ((DWORD)fTempG) & pOutFormat->dwGBitMask;
            outB = ((DWORD)fTempB) & pOutFormat->dwBBitMask;
            outA = ((DWORD)fTempA) & pOutFormat->dwRGBAlphaBitMask;

            *pdwOutColor = outR | outG | outB | outA;
            return true;
        }

        int CalculateShift(DWORD dwMask)
        {
            int iShift = 0;

            while (dwMask == ((dwMask >> iShift) << iShift) && iShift < (8 * sizeof(dwMask)))
            {
                ++iShift;
            }

            return iShift - 1;
        }

        eTestResult VerifyBltColors(
            CDDSurfaceDesc * pcddsdLocked,
            const BYTE bOriginalR,
            const BYTE bOriginalG,
            const BYTE bOriginalB,
            const BYTE bOriginalA,
            const int iVerifWidth,
            const int iVerifHeight,
            const bool bSourceAlpha,
            const bool bTestOverlay)
        {
            eTestResult tr = trPass;
            const int iTolerance = 10;
            DWORD dwPrimaryMask =
                pcddsdLocked->ddpfPixelFormat.dwRBitMask |
                pcddsdLocked->ddpfPixelFormat.dwGBitMask |
                pcddsdLocked->ddpfPixelFormat.dwBBitMask |
                pcddsdLocked->ddpfPixelFormat.dwRGBAlphaBitMask;

            int iRShift = CalculateShift(pcddsdLocked->ddpfPixelFormat.dwRBitMask);
            int iGShift = CalculateShift(pcddsdLocked->ddpfPixelFormat.dwGBitMask);
            int iBShift = CalculateShift(pcddsdLocked->ddpfPixelFormat.dwBBitMask);
            int iAShift = CalculateShift(pcddsdLocked->ddpfPixelFormat.dwRGBAlphaBitMask);
            DWORD dwExpectedR = ((DWORD)bOriginalR) * (pcddsdLocked->ddpfPixelFormat.dwRBitMask >> iRShift) / 255;
            DWORD dwExpectedG = ((DWORD)bOriginalG) * (pcddsdLocked->ddpfPixelFormat.dwGBitMask >> iGShift) / 255;
            DWORD dwExpectedB = ((DWORD)bOriginalB) * (pcddsdLocked->ddpfPixelFormat.dwBBitMask >> iBShift) / 255;
            DWORD dwExpectedA = ((DWORD)bOriginalA) * (pcddsdLocked->ddpfPixelFormat.dwRGBAlphaBitMask >> iAShift) / 255;

            if (!bSourceAlpha)
            {
                dwExpectedA = 0;
            }

            for (int y = 0; y < iVerifHeight && trPass == tr; ++y)
            {
                for (int x = 0; x < iVerifWidth && trPass == tr; ++x)
                {
                    // Extract the pixel value from the primary surface
                    DWORD dwVerif;
                    DWORD dwR;
                    DWORD dwG;
                    DWORD dwB;
                    DWORD dwA = 0;
                    int iRDelta;
                    int iGDelta;
                    int iBDelta;
                    int iADelta = 0;
                    // Get the offset
                    BYTE * pbSurface = (BYTE*)pcddsdLocked->lpSurface;
                    pbSurface += y * pcddsdLocked->lPitch + x * pcddsdLocked->lXPitch;
                    memcpy(&dwVerif, pbSurface, sizeof(dwVerif));
                    dwVerif &= dwPrimaryMask;

                    // Extract the R, G, B, and A values from the pixel
                    dwR = dwVerif & (pcddsdLocked->ddpfPixelFormat.dwRBitMask);
                    dwR = dwR >> iRShift;
                    iRDelta = abs(((int)dwR) - ((int)dwExpectedR));

                    dwG = dwVerif & (pcddsdLocked->ddpfPixelFormat.dwGBitMask);
                    dwG = dwG >> iGShift;
                    iGDelta = abs(((int)dwG) - ((int)dwExpectedG));

                    dwB = dwVerif & (pcddsdLocked->ddpfPixelFormat.dwBBitMask);
                    dwB = dwB >> iBShift;
                    iBDelta = abs(((int)dwB) - ((int)dwExpectedB));

                    if (pcddsdLocked->ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS)
                    {
                        dwA = dwVerif & (pcddsdLocked->ddpfPixelFormat.dwRGBAlphaBitMask);
                        dwA = dwA >> iAShift;
                        iADelta = abs(((int)dwA) - ((int)dwExpectedA));
                    }

                    // Compare to the color we expect

                    if (iRDelta > iTolerance ||
                        iGDelta > iTolerance ||
                        iBDelta > iTolerance ||
                        iADelta > iTolerance)
                    {
                        if (!bTestOverlay)
                        {
                            dbgout(LOG_FAIL) << "After Blt, found unexpected colors at ("
                                << x << ", " << y << "). Expected ("
                                << dwExpectedR << ", " << dwExpectedG << ", " << dwExpectedB << ", " << dwExpectedA
                                << ") but found ("
                                << dwR << ", " << dwG << ", " << dwB << ", " << dwA
                                << ") (RGBA)" << endl;
                            tr |= trFail;
                        }
                        else
                        {
                            dbgout << "Warning: After Blt, found unexpected colors at ("
                                << x << ", " << y << "). Expected ("
                                << dwExpectedR << ", " << dwExpectedG << ", " << dwExpectedB << ", " << dwExpectedA
                                << ") but found ("
                                << dwR << ", " << dwG << ", " << dwB << ", " << dwA
                                << ") (RGBA)" << endl;
                            dbgout << "Not failing test because error happened on overlay surface." << endl;
                        }
                    }
                }
            }
            return tr;
        }

        eTestResult VerifySurfaceFormat(HWND hClientWnd, IDirectDrawSurface * pddsPrimary, IDirectDrawSurface * pddsTest)
        {
            using namespace DDrawUty::Surface_Helpers;

            const BYTE bOriginalR = 0xff;
            const BYTE bOriginalG = 0x7f;
            const BYTE bOriginalB = 0x00;
            const BYTE bOriginalA = 0xff;
            const int iVerifWidth = 10;
            const int iVerifHeight = 10;

            eTestResult tr = trPass;
            HRESULT hr;
            RECT rcWorkArea;
            RECT rcClientArea;
            RECT rcTestArea;
            RECT rcVerifyArea;
            RECT rcTemp;
            POINT ptClientToScreen = {0, 0};
            CDDSurfaceDesc cddsdTest;
            CDDSurfaceDesc cddsdPrimary;
            CDDSurfaceDesc cddsdLocked;
            DDBLTFX ddbltfx;
            DWORD dwColor = RGBA(bOriginalR, bOriginalG, bOriginalB, bOriginalA);
            DWORD dwTestColor;
            DWORD dwPrimaryColor;
            DWORD dwPrimaryMask;

            if (!pddsPrimary || !pddsTest)
            {
                dbgout(LOG_FAIL) << "VerifySurfaceFormat received invalid pointers" << endl;
                return trAbort;
            }

            // For a Primary in Windowed mode, use the Window's bounding rectangle in Lock, instead of NULL.
            if(g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)
            {
                RECT rcBoundRect;
                eTestResult tr = GetClippingRectangle(pddsPrimary, rcBoundRect);
                if(trPass != tr)
                {
                    dbgout (LOG_FAIL) << "Failure while trying to retrieve the clipping rectangle" << endl;
                    return tr;
                }

                // Now, retrieve the surface description of the Window surface using a call to Lock, as GetSurfaceDesc
                // returns the surface description of the true primary.
                hr = pddsPrimary->Lock(&rcBoundRect, &cddsdPrimary, DDLOCK_WAITNOTBUSY, NULL);
                if(FAILED(hr))
                {
                    dbgout << "Failure locking primary surface. hr = " 
                           << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                           << "GetLastError() = " << HEX((DWORD)GetLastError())
                           << endl;
                    return trFail;
                }

                hr = pddsPrimary->Unlock(&rcBoundRect);
                if(FAILED(hr))
                {
                    dbgout << "Failure unlocking primary surface. hr = " 
                           << g_ErrorMap[hr] << " (" << HEX((DWORD)hr) << "). "
                           << "GetLastError() = " << HEX((DWORD)GetLastError())
                           << endl;
                    return trFail;
                }
            }

            // pddsTest should never be a Windowed Primary. So use GetSurfaceDesc here.
            pddsTest->GetSurfaceDesc(&cddsdTest);

            if (!ConvertColor(&dwPrimaryColor, dwColor, &cddsdPrimary.ddpfPixelFormat))
            {
                dbgout << "Cannot convert to primary color, skipping this surface" << endl;
                return trPass;
            }
            if (!ConvertColor(&dwTestColor, dwColor, &cddsdTest.ddpfPixelFormat))
            {
                dbgout << "Cannot convert to test color, skipping this surface" << endl;
                return trPass;
            }
            dwPrimaryMask =
                cddsdPrimary.ddpfPixelFormat.dwRBitMask |
                cddsdPrimary.ddpfPixelFormat.dwGBitMask |
                cddsdPrimary.ddpfPixelFormat.dwBBitMask |
                cddsdPrimary.ddpfPixelFormat.dwRGBAlphaBitMask;

            // Determine an appropriate rect
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
            GetClientRect(hClientWnd, &rcClientArea);
            ClientToScreen(hClientWnd, &ptClientToScreen);
            OffsetRect(&rcClientArea, ptClientToScreen.x, ptClientToScreen.y);
            SetRect(&rcTestArea, 0, 0, cddsdTest.dwWidth, cddsdTest.dwHeight);
                // Intersect: workarea, client area of dd window, area of test surface
            IntersectRect(&rcTemp, &rcWorkArea, &rcClientArea);
            IntersectRect(&rcVerifyArea, &rcTemp, &rcTestArea);
                // Choose a 10x10 section
            if (rcVerifyArea.right - rcVerifyArea.left < iVerifWidth ||
                rcVerifyArea.bottom - rcVerifyArea.top < iVerifHeight)
            {
                dbgout << "Unable to find a suitable intersection for testing, skipping this surface: " << rcVerifyArea << endl;
                return trPass;
            }
            rcVerifyArea.right = rcVerifyArea.left + iVerifWidth;
            rcVerifyArea.bottom = rcVerifyArea.top + iVerifHeight;

            // Fill the entire test surface with a suitable solid color (e.g. 0xFF7F00)
            memset(&ddbltfx, 0x00, sizeof(ddbltfx));

            dbgout << "Verifying Colorfill to Test Surface" << endl;
            ddbltfx.dwSize = sizeof(ddbltfx);
            ddbltfx.dwFillColor = dwTestColor;
            hr = pddsTest->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
            CheckHRESULT(hr, "Blt Colorfill to Test Surface", trAbort);

            // Lock the test surface, verify the result is solid and of approximately the correct color
            hr = pddsTest->Lock(&rcVerifyArea, &cddsdLocked, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Test Surface", trAbort);

            tr |= VerifyBltColors(
                &cddsdLocked,
                bOriginalR,
                bOriginalG,
                bOriginalB,
                bOriginalA,
                iVerifWidth,
                iVerifHeight,
                true,
                cddsdTest.ddsCaps.dwCaps & DDSCAPS_OVERLAY);

            pddsTest->Unlock(&rcVerifyArea);

            ddbltfx.dwFillColor = (~dwTestColor) & dwPrimaryMask;
            hr = pddsPrimary->Blt(&rcVerifyArea, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
            CheckHRESULT(hr, "Blt Colorfill to Test Surface", trAbort);

            dbgout << "Verifying Blt from Test surface to Primary surface" << endl;
            // Blt from the test surface to the primary surface.
            hr = pddsPrimary->Blt(&rcVerifyArea, pddsTest, &rcVerifyArea, DDBLT_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Blt from Test to Primary Surface", trFail);

            // Lock the primary surface, verify the result is solid and of approximately the correct color
            hr = pddsPrimary->Lock(&rcVerifyArea, &cddsdLocked, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Primary Surface", trAbort);

            tr |= VerifyBltColors(
                &cddsdLocked,
                bOriginalR,
                bOriginalG,
                bOriginalB,
                bOriginalA,
                iVerifWidth,
                iVerifHeight,
                cddsdTest.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS,
                cddsdTest.ddsCaps.dwCaps & DDSCAPS_OVERLAY);

            pddsPrimary->Unlock(&rcVerifyArea);

            
            dbgout << "Verifying Blt from Primary surface to Test surface" << endl;

            // Fill the primary surface with a test color            
            ddbltfx.dwFillColor = (~dwTestColor);
            hr = pddsTest->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
            CheckHRESULT(hr, "Blt Colorfill to Test Surface", trAbort);

            // Fill the primary surface with a suitable solid color (e.g. 0x00FF7F)
            ddbltfx.dwFillColor = dwPrimaryColor;
            hr = pddsPrimary->Blt(&rcVerifyArea, NULL, NULL, DDBLT_COLORFILL, &ddbltfx);
            CheckHRESULT(hr, "Blt Colorfill to Primary Surface", trAbort);

            // Blt from the primary surface to the test surface
            hr = pddsTest->Blt(&rcVerifyArea, pddsPrimary, &rcVerifyArea, DDBLT_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Blt from Test to Primary Surface", trFail);

            // Lock the test surface, verify the result is solid and of approximately the correct color
            hr = pddsTest->Lock(&rcVerifyArea, &cddsdLocked, DDLOCK_WAITNOTBUSY, NULL);
            CheckHRESULT(hr, "Lock Test Surface", trAbort);

            tr |= VerifyBltColors(
                &cddsdLocked,
                bOriginalR,
                bOriginalG,
                bOriginalB,
                bOriginalA,
                iVerifWidth,
                iVerifHeight,
                cddsdPrimary.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS,
                cddsdTest.ddsCaps.dwCaps & DDSCAPS_OVERLAY);

            pddsTest->Unlock(&rcVerifyArea);


            return tr;
        }

        // VerifyFourccSurfaceCreation
        // Attempts to create the surface types specified in pcstSurfaceTypes in all supported FOURCC
        // pixel formats.
        // dwNumSurfaceTypes contains the number of surface types in the array, pcstSurfaceTypes.
        // Returns trPass if atleast one surface was created, else returns trFail.
        eTestResult VerifyFourccSurfaceCreation(IDirectDraw* pdd, CfgSurfaceType* pcstSurfaceTypes, DWORD dwNumSurfaceTypes)
        {
            eTestResult tr = trFail;
            HRESULT hr = S_OK;
            TCHAR tszDescription[128];
            DDSURFACEDESC ddsdTest;
            CComPointer<IDirectDrawSurface> piDDSTest;

            if(!pdd || !pcstSurfaceTypes || (dwNumSurfaceTypes <= 0))
            {
                return trFail;
            }

            DWORD dwFourccCount = 0;
            hr = pdd->GetFourCCCodes(&dwFourccCount, NULL);
            CheckHRESULT(hr, "Retrieving number of supported FOURCC formats", trFail);
            CheckCondition(0==dwFourccCount, "No FOURCC formats supported on driver", trFail);

            DWORD* rgSupportedFourCCFormats = new DWORD[dwFourccCount];
            CheckCondition(NULL==rgSupportedFourCCFormats, "Allocation for " << dwFourccCount << " FOURCC entries failed.", trFail);

            DWORD dwTemp = dwFourccCount;
            hr = pdd->GetFourCCCodes(&dwTemp, rgSupportedFourCCFormats);
            CheckHRESULT(hr, "Retrieving supported fourcc formats", trFail);

            for(int iNextSurfaceIndex=0; iNextSurfaceIndex<dwNumSurfaceTypes; ++iNextSurfaceIndex)
            {
                hr = g_DDConfig.GetSurfaceDescData(pcstSurfaceTypes[iNextSurfaceIndex], ddsdTest, 
                                                   tszDescription, _countof(tszDescription)
                                                   );
                if(FAILED(hr))
                {
                    dbgout << "Unable to create a surface desc for the surface type at index "
                           << iNextSurfaceIndex << "."
                           << endl
                           << "Moving on to the next surface type."
                           << endl;
                    continue;
                }
                dbgout << "Attempting to create Surface Type : " << tszDescription << endl;
                dbgout.push_indent();

                for(int iNextFourccIndex=0; iNextFourccIndex<dwFourccCount; ++iNextFourccIndex)
                {
                    char buf[5] = {0,};
                    *((DWORD*)buf) = rgSupportedFourCCFormats[iNextFourccIndex];

                    dbgout << "FOURCC Format : " << buf << endl;

                    DDSURFACEDESC ddsdTmp = ddsdTest;
                    ddsdTmp.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
                    ddsdTmp.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
                    ddsdTmp.ddpfPixelFormat.dwFourCC = rgSupportedFourCCFormats[iNextFourccIndex];
                    
                    // Try to create the surface
                    bool bRetrySmaller;
                    do
                    {
                        bRetrySmaller = false;
                        hr = pdd->CreateSurface(&ddsdTmp, piDDSTest.AsOutParam(), NULL);
                       
                        // If failed
                        if (DDERR_TOOBIGSIZE == hr ||
                            DDERR_TOOBIGWIDTH == hr ||
                            DDERR_TOOBIGHEIGHT == hr)
                        {
                            dbgout << "Surface too large (received " << g_ErrorMap[hr] << " [" << HEX((DWORD)hr) << "]). Will attempt smaller surface" << endl;
                            bRetrySmaller = true;
                        }
                        if (DDERR_OUTOFVIDEOMEMORY == hr ||
                            DDERR_OUTOFMEMORY == hr)
                        {
                            dbgout << "Not enough memory (received " << g_ErrorMap[hr] << " [" << HEX((DWORD)hr) << "]). Will attempt smaller surface" << endl;
                            bRetrySmaller = true;
                        }
                        if (bRetrySmaller)
                        {
                            ddsdTmp.dwWidth = (ddsdTmp.dwWidth * 3 / 4) & (~0x3f);
                            ddsdTmp.dwHeight = (ddsdTmp.dwHeight * 3 / 4) & (~0x3f);
                            if (0 == ddsdTmp.dwWidth || 0 == ddsdTmp.dwHeight)
                            {
                                dbgout << "Unable to find small enough surface." << endl;
                                bRetrySmaller = false;
                            }
                            else
                            {
                                dbgout << "Trying with surface of size " << ddsdTmp.dwWidth << "x" << ddsdTmp.dwHeight << endl;
                                bRetrySmaller = true;
                            }
                        }
                    } while (bRetrySmaller);

                    if(DD_OK == hr)
                    {
                        // A surface was successfully created.
                        // Pass the test but continue trying to create more FOURCC surfaces.
                        tr = trPass; 

                        // Destroy the surface.
                        piDDSTest.ReleaseInterface();
                    }
                    else
                    {
                        dbgout << "Unable to create surface. Received " << g_ErrorMap[hr] << " [" << HEX((DWORD)hr) << "]." << endl;
                    }
                }
                dbgout.pop_indent();
            }

            if(rgSupportedFourCCFormats)
            {
                delete[] rgSupportedFourCCFormats;
            }

            return tr;
        }
    }

    eTestResult CTest_CreateSurfaceVerification::TestIDirectDraw()
    {
        using namespace priv_CreateSurfaceVerification;
        using namespace DDrawUty::Surface_Helpers; 

        eTestResult tr = trPass;
        HRESULT hr;
        bool fSuccessExpected;
        bool fOverlayCreated = false;
        TCHAR tszSurface[1024];
        CDDCaps Caps;
        CDDSurfaceDesc cddsdPrimary;
        CDDSurfaceDesc cddsdTest;
        CComPointer<IDirectDrawSurface> piDDSPrimary;
        CComPointer<IDirectDrawSurface> piDDSTest;
        CfgPixelFormat pfCurrentFormat;
        CfgSurfaceType rgTestSurfaces[] = {
            stOffScrVid,
            stOffScrSys,
            stOffScrSysOwnDc,
            stOverlayVid0Back,
            stOverlaySys0Back,
        };

        hr = m_piDD->GetCaps(&Caps, NULL);
        CheckHRESULT(hr, "GetCaps", trAbort);

        // Create the primary surface
        cddsdPrimary.dwFlags = DDSD_CAPS;
        cddsdPrimary.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        hr = m_piDD->CreateSurface(&cddsdPrimary, piDDSPrimary.AsOutParam(), NULL);
        CheckHRESULT(hr, "CreateSurface for Primary", trAbort);

        // Set the main window as the clipping rectangle in the Windowed mode
        if(g_DDConfig.CooperativeLevel() == DDSCL_NORMAL)
        {
            tr = ClipToWindow(m_piDD.AsInParam(), piDDSPrimary.AsInParam(), 
                              CWindowsModule::GetWinModule().m_hWnd
                             );
            if(tr != trPass)
            {
                dbgout(LOG_ABORT) << "Unable to set the clipping window on the Primary." << endl;
                return trAbort;
            }
        }

        hr = piDDSPrimary->GetSurfaceDesc(&cddsdPrimary);
        CheckHRESULT(hr, "piDDSPrimary->GetSurfaceDesc", trAbort);

        // Cycle through the different surface types (disregard what has been reported as being supported)
        for (int iSurfIndex = 0; iSurfIndex < _countof(rgTestSurfaces); ++iSurfIndex)
        {
            for (pfCurrentFormat = pfNone; pfCurrentFormat < pfCount; ++*(int*)&pfCurrentFormat)
            {
                hr = g_DDConfig.GetSurfaceDesc(rgTestSurfaces[iSurfIndex], pfCurrentFormat, cddsdTest, tszSurface, _countof(tszSurface), false);
                CheckHRESULT(hr, "g_DDConfig.GetSurfaceDesc", trAbort);

                dbgout << "Verifying Surface Creation Behavior of surface: " << tszSurface << endl;
                dbgout.push_indent();

                // Determine if success is expected:
                // System Memory and RGB
                fSuccessExpected =
                    ((stOffScrSys == rgTestSurfaces[iSurfIndex]) || (stOffScrSysOwnDc == rgTestSurfaces[iSurfIndex])) &&
                    (pfNone == pfCurrentFormat || (pfCurrentFormat < pfYUVYUYV && pfCurrentFormat > pfPal8));
                // Video Memory and same format as Primary
                fSuccessExpected = fSuccessExpected || (
                    (stOffScrVid == rgTestSurfaces[iSurfIndex]) &&
                    (pfNone == pfCurrentFormat));

                // DDGPE layer does not support BGR formats, so don't expect them to succeed
                if (pfBGR565 == pfCurrentFormat ||
                    pfBGR888 == pfCurrentFormat ||
                    pfBGR0888 == pfCurrentFormat ||
                    pfABGR8888 == pfCurrentFormat ||
                    pfABGR8888pm == pfCurrentFormat)
                {
                    fSuccessExpected = false;
                }

                if ((cddsdTest.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) &&
                    !(Caps.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
                {
                    fSuccessExpected = false;
                }

                // Try to create the surface
                bool bRetrySmaller;
                do
                {
                    bRetrySmaller = false;
                    hr = m_piDD->CreateSurface(&cddsdTest, piDDSTest.AsOutParam(), NULL);

                    if(DD_OK == hr &&
                       ((stOverlayVid0Back == rgTestSurfaces[iSurfIndex]) ||
                       (stOverlaySys0Back == rgTestSurfaces[iSurfIndex]))
                      ) // An overlay surface was successfully created.
                    {
                        fOverlayCreated = true;
                    }

                    // If failed
                    if (DDERR_TOOBIGSIZE == hr ||
                        DDERR_TOOBIGWIDTH == hr ||
                        DDERR_TOOBIGHEIGHT == hr)
                    {
                        dbgout << "Surface too large (received " << g_ErrorMap[hr] << " [" << HEX((DWORD)hr) << "]). Will attempt smaller surface" << endl;
                        bRetrySmaller = true;
                    }
                    if (DDERR_OUTOFVIDEOMEMORY == hr ||
                        DDERR_OUTOFMEMORY == hr)
                    {
                        dbgout << "Not enough memory (received " << g_ErrorMap[hr] << " [" << HEX((DWORD)hr) << "]). Will attempt smaller surface" << endl;
                        bRetrySmaller = true;
                    }
                    if (bRetrySmaller)
                    {
                        cddsdTest.dwWidth = (cddsdTest.dwWidth * 3 / 4) & (~0x3f);
                        cddsdTest.dwHeight = (cddsdTest.dwHeight * 3 / 4) & (~0x3f);
                        if (0 == cddsdTest.dwWidth || 0 == cddsdTest.dwHeight)
                        {
                            dbgout << "Unable to find small enough surface." << endl;
                            bRetrySmaller = false;
                        }
                        else
                        {
                            dbgout << "Trying with surface of size " << cddsdTest.dwWidth << "x" << cddsdTest.dwHeight << endl;
                            bRetrySmaller = true;
                        }
                    }
                } while (bRetrySmaller);
                if (DDERR_OUTOFVIDEOMEMORY == hr || DDERR_OUTOFMEMORY == hr)
                {
                    dbgout << "Out of Memory creating surface, continuing" << endl;
                    dbgout.pop_indent();
                    continue;
                }
                else if (FAILED(hr))
                {
                    // Was succeed expected?
                    if (fSuccessExpected)
                    {
                        QueryHRESULT(hr, "Expected to succeed creating surface, but CreateSurface", tr |= trFail);
                    }
                    else
                    {
                        dbgout << "Could not create surface, continuing" << endl;
                    }
                    // Continue with surface types
                    dbgout.pop_indent();
                    continue;
                }
                dbgout << "Surface Created! Verifying proper Blt behavior" << endl;
                // Run Surface Create Verification function, record result
                tr |= VerifySurfaceFormat(m_hWnd, piDDSPrimary.AsInParam(), piDDSTest.AsInParam());

                // Destroy Surface
                piDDSTest.ReleaseInterface();
                dbgout.pop_indent();
            }
        }

        if((fOverlayCreated == false) && (Caps.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
        {
            // Test if an overlay can be created with one of the supported FOURCC pixel formats.
            // If any of the FOURCC format is supported by the test code, it would have been verified 
            // in the piece of code above, so no surface format verification is carried out here.
            CfgSurfaceType rgOverlaySurfaces[] = {
                stOverlayVid0Back,
                stOverlaySys0Back,
            };
            if(trFail == VerifyFourccSurfaceCreation(m_piDD.AsInParam(), rgOverlaySurfaces, _countof(rgOverlaySurfaces)))
            {
                dbgout(LOG_FAIL) << "Could not create even a single Overlay surface, even though the driver supports Overlays"
                                 << endl;
                tr |= trFail;
            }
        }

        return tr;
    }
};


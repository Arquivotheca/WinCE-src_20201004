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
#include "storeincludes.hpp"

// Initialize FS detector state with a dll and export. If either the dll
// cannot be loaded or the export cannot be found, this function will fail.
LRESULT DetectorState_t::RunDetector (HDSK hDsk, const WCHAR* pDetectorDll, const WCHAR* pDetectorExport,
    const GUID* pGuid, const BYTE* pBootSector, DWORD SectorSize)
{
    if(m_hDetectorDll) {
        ::FreeLibrary (m_hDetectorDll);
        m_hDetectorDll = NULL;
        m_pfnDetectorExport = NULL;
    }

    LRESULT lResult = ERROR_SUCCESS;

#if DEBUG    
    WCHAR GuidString[128] = L"";
    FsdStringFromGuid(pGuid, GuidString, 128);
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!DetectorState_t::RunDetector - %s::%s GUID=%s\r\n", pDetectorDll, 
        pDetectorExport, GuidString));
#endif // DEBUG

    m_hDetectorDll = ::LoadDriver (pDetectorDll);
    if (m_hDetectorDll) {

        m_pfnDetectorExport = reinterpret_cast<PFN_FSD_DETECT>
            (::FsdGetProcAddress (m_hDetectorDll,pDetectorExport));
        if (m_pfnDetectorExport) {

            if (m_pfnDetectorExport (hDsk, pGuid, pBootSector, SectorSize)) {
                // Detector identified the volume.
                lResult = ERROR_SUCCESS;

            } else {
                // Detector does not recognize the volume.
                lResult = ERROR_UNRECOGNIZED_VOLUME;
            }

        } else {
            // Failed to get the detector export from the dll.
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!DetectorState_t::RunDetector - failed loading detector export \"%s::%s\"\r\n", pDetectorDll, pDetectorExport));
            lResult = ::FsdGetLastError ();
        }

    } else {
        // Failed to load the detector dll.
        DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!DetectorState_t::RunDetector - failed loading detector dll \"%s\"\r\n", pDetectorDll));
        lResult = ::FsdGetLastError ();
    }

    if (ERROR_SUCCESS == lResult) {

#if DEBUG
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!DetectorState_t::RunDetector - identified volume as %s\r\n", GuidString));
#endif
        // Save the GUID.
        m_Guid = *pGuid;

        // On success, we keep the detector dll loaded. This serves two purposes:
        //      1) If the detector dll is the same as the FSD dll, we don't free and
        //         immediately re-load the dll.
        //      2) We may need to re-run the detector on media removal/insertion.

    } else {
        m_pfnDetectorExport = NULL;
        if (m_hDetectorDll) {
            ::FreeLibrary (m_hDetectorDll);
            m_hDetectorDll = NULL;
        }
    }

    return lResult;
}

// Run the detector.
LRESULT DetectorState_t::ReRunDetector (HDSK hDsk, const BYTE* pBootSector, DWORD SectorSize)
{
#if DEBUG    
    WCHAR GuidString[128] = L"";
    FsdStringFromGuid(&m_Guid, GuidString, 128);
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!DetectorState_t::ReRunDetector (0x%x::0x%p) - GUID=%s\r\n", m_hDetectorDll, m_pfnDetectorExport, GuidString));
#endif // DEBUG

    if (m_pfnDetectorExport && m_pfnDetectorExport (hDsk, &m_Guid, pBootSector, SectorSize))
    {
        // Detector identified the volume.
#if DEBUG
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!DetectorState_t::ReRunDetector - identified volume as %s\r\n", GuidString));
#endif
        return ERROR_SUCCESS;

    } else {

        // Detector does not recognize the volume.
        return ERROR_UNRECOGNIZED_VOLUME;
    }
}

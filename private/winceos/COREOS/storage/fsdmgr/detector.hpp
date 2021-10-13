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
#ifndef __DETECTOR_HPP__
#define __DETECTOR_HPP__

// PFN_FSD_DETECT
//
// Function prototype for a file system detector function
// exported by a file system detector driver.
//
typedef BOOL (*PFN_FSD_DETECT) (HDSK, const GUID*, const BYTE* pBootSector, DWORD SectorSize);

class DetectorState_t
{
public:

    DetectorState_t () : 
      m_hDetectorDll (NULL),
      m_pfnDetectorExport (NULL)
    {
        ::ZeroMemory (&m_Guid, sizeof (GUID));
    }

    ~DetectorState_t ()
    {
        if (m_hDetectorDll) {
            ::FreeLibrary (m_hDetectorDll);
            m_hDetectorDll = NULL;
        }
    }

    // Run the detector.
    LRESULT RunDetector (HDSK hDsk, const WCHAR* pDetectorDll, const WCHAR* pDetectorExport,
        const GUID* pGuid, const BYTE* pBootSector, DWORD SectorSize);
    LRESULT ReRunDetector (HDSK hDsk, const BYTE* pBootSector, DWORD SectorSize);
    inline const BOOL IsDetectorPresent () 
    {
        return (NULL != m_hDetectorDll);
    }

protected:

    HINSTANCE m_hDetectorDll;
    PFN_FSD_DETECT m_pfnDetectorExport;
    GUID m_Guid;
};

#endif // __DETECTOR_HPP__

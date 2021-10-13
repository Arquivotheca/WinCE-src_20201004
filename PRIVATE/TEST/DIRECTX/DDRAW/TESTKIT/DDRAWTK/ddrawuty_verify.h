//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//

#pragma once
#if !defined(__DDRAWUTY_VERIFY_H__)
#define __DDRAWUTY_VERIFY_H__

#include <QATestUty/TestUty.h>
#include <DDTestKit_Base.h>
#include <DDrawUty.h>
using namespace DDrawUty;
namespace DDrawVerifyUty
{
    class CDDrawSurfaceVerify
    {
    private:
        BYTE *m_dwSurfaceBackup;
        bool m_bBackupIsValid;
        DWORD m_dwSize;
        RGNDATA *m_rgndClipData;
        bool m_bClipDataValid;
        RECT m_rcSrc;
        RECT m_rcDst;
        IDirectDrawSurface *m_piDDSSrc;
        IDirectDrawSurface *m_piDDSDst;
        
        bool IsClipped(DWORD x, 
                                    DWORD y);
        eTestResult Verify(WORD *dwsrc, 
                                    WORD *dwdest,
                                    CDDSurfaceDesc *ddsdSrc, 
                                    CDDSurfaceDesc *ddsdDst);
        eTestResult Verify(DWORD *dwsrc, 
                                    DWORD *dwdest,
                                    CDDSurfaceDesc *ddsdSrc, 
                                    CDDSurfaceDesc *ddsdDst);


    public:
        eTestResult PreSurfaceVerify(IDirectDrawSurface *piDDSSrc, 
                                                            RECT rcSrc, 
                                                            IDirectDrawSurface *piDDSDst, 
                                                            RECT rcDst);
        eTestResult SurfaceVerify();
        eTestResult PreVerifyColorFill(IDirectDrawSurface *piDDS);
        eTestResult VerifyColorFill(DWORD dwfillcolor);
        CDDrawSurfaceVerify():m_bBackupIsValid(false), m_dwSize(0), m_bClipDataValid(false), m_rgndClipData(NULL) {};
        ~CDDrawSurfaceVerify(){if(m_bBackupIsValid)
                                                            delete[] m_dwSurfaceBackup; 
                                                        if(m_bClipDataValid) 
                                                            delete[] m_rgndClipData;
                                                        };
    
    };
}

#endif // header protection 


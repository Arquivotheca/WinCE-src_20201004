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
        BYTE *m_pBackupSurfaceStart;
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
        eTestResult Verify(const WORD *dwsrc, 
                                    const WORD *dwdest,
                                    CDDSurfaceDesc *ddsdSrc, 
                                    CDDSurfaceDesc *ddsdDst);
        eTestResult Verify(const DWORD *dwsrc, 
                                    const DWORD *dwdest,
                                    CDDSurfaceDesc *ddsdSrc, 
                                    CDDSurfaceDesc *ddsdDst);


    public:
        eTestResult PreSurfaceVerify(IDirectDrawSurface *piDDSSrc, 
                                                            RECT rcSrc, 
                                                            IDirectDrawSurface *piDDSDst, 
                                                            RECT rcDst);
        eTestResult SurfaceVerify();
        eTestResult PreVerifyColorFill(IDirectDrawSurface *piDDS);
        eTestResult VerifyColorFill(DWORD dwfillcolor, const RECT* prcVerifyRect = NULL);
        CDDrawSurfaceVerify():m_bBackupIsValid(false), m_dwSize(0), m_bClipDataValid(false), m_rgndClipData(NULL), m_pBackupSurfaceStart(NULL){};
        ~CDDrawSurfaceVerify()
        {
            if(m_bBackupIsValid)
                delete[] m_dwSurfaceBackup; 
            if(m_bClipDataValid) 
                delete[] m_rgndClipData;
        };
    
    };
}

#endif // header protection 


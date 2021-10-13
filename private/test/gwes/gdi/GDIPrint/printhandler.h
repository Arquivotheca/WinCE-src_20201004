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

#include <windows.h>
#include <commdlg.h>

#ifndef PRINTHANDLER_H
#define PRINTHANDLER_H

typedef struct _DCDATA_ {
        HDC hdcPrint;
        HDC hdcVideoMemory;
        int nWidth, nHeight;
        int nCurrentX, nCurrentY;
        int nVidCurrentX, nVidCurrentY;
        int nVidWidth, nVidHeight;
} DCDATA;

typedef struct _PHPARAMS_ {
        bool bDrawToPrimary;
        DEVMODE dm;
        TCHAR *tszDeviceName;
        TCHAR *tszDriverName;
        TCHAR *tszOutputName;
} PHPARAMS;

typedef class CPrintHandler
{
    public:
        CPrintHandler();
        ~CPrintHandler();

        bool Initialize();
        bool Cleanup();
        bool SetPrintHandlerParams(PHPARAMS *);
        bool SetTestDescription(TCHAR *);
        bool GetDCData(DCDATA *);
        bool TestStart(int, int);
        bool TestFinished();

    private:
        bool InitializeMainWindow();
        bool CleanupMainWindow();

        DEVMODE m_dm;
        TCHAR *m_tszDeviceName;
        TCHAR *m_tszDriverName;
        TCHAR *m_tszOutputName;

        HDC m_hdcPrint;
        HDC m_hdcVideoMemory;
        HBITMAP m_hbmpVideoMemory;
        HBITMAP m_hbmpVideoMemoryStock;
        int m_nWidth;
        int m_nHeight;
        int m_nCurrentX;
        int m_nCurrentY;
        int m_nLeftMargin;
        int m_nTopMargin;
        int m_nPageWidth;
        int m_nPageHeight;
        int m_nVidWidth;
        int m_nVidHeight;

        HWND m_hwndPrimary;
        HDC m_hdcPrimary;
        int m_nPrimaryTop;
        int m_nPrimaryLeft;
        int m_nPrimaryWidth;
        int m_nPrimaryHeight;

        TCHAR *m_tszTestInstructions;
        int m_nInstructionHeight;
        bool m_bDrawToPrimary;
        bool m_bInitialized;
} PrintHandler;

#endif

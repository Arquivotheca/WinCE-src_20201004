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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#include <windows.h>
#include "resource.h"
#include "drv_dlg.h"
#include <sd_tst_cmn.h>
#include <sddrv.h>

BOOL CALLBACK YesNoProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)

{
    LPCTSTR msg;
    LPCTSTR txt;
    PYesNoText pynt = NULL;
    static int seconds;
    static UINT uiTimer;
    static int rate;

    switch (uiMsg)
    {
        case WM_INITDIALOG:
            pynt = (PYesNoText)lParam;
            msg = pynt->question;
            txt = pynt->text;
            rate = (pynt->msgRate > 0) ? pynt->msgRate : 1;
            SetDlgItemText(hDlg,IDC_Text, msg);
            if (txt != NULL)
            {
                SetDlgItemText(hDlg, IDC_Buffer, txt);
            }
            seconds = (pynt->time > 0) ? pynt->time : 0;
            SetDlgItemInt(hDlg, IDC_Count, seconds, FALSE);
            uiTimer = SetTimer(hDlg, ID_TIMER, 1000, NULL);
            if (uiTimer == 0) SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
            return TRUE;
        case WM_TIMER:
            if (seconds > 0)
            {
                seconds = seconds--;
                SetDlgItemInt(hDlg, IDC_Count, seconds, FALSE);
                if ((seconds % rate) == 0)
                {
                    LogDebug((UINT) 0, SDCARD_ZONE_INFO, TEXT("An important message is being displayed on the test machine. Time left to respond =%d seconds..."),seconds);
                }
            }
            else
            {
                KillTimer(hDlg, uiTimer);
                uiTimer = 0;
                SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDYES:
                    if (uiTimer)
                    {
                        KillTimer(hDlg, uiTimer);
                        uiTimer = 0;
                    }
                    EndDialog(hDlg, TRUE);
                    return TRUE;
                case IDNO:
                    if (uiTimer)
                    {
                        KillTimer(hDlg, uiTimer);
                        uiTimer = 0;
                    }
                    EndDialog(hDlg, FALSE);
                    return TRUE;
            };
    }
    return FALSE;
}


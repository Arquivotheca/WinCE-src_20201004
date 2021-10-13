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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
//
// Resource to define the dialog pop up
//
//
#include "globals.h"
#include <windows.h>
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////////
// MessageProc displays a message, pointed to by lParam, on the test machine
// and waits for an acknowledgement via the Ok button.
//
// Parameters: hDlg, uiMsg, wParam, lParam
//
// Return value: TRUE if uiMsg processed, otherwise FALSE
/////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK MessageProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    LPCTSTR msg;
    switch (uiMsg)
    {
        case WM_INITDIALOG:
            g_bMsgBox = TRUE;
            msg = (LPCTSTR)lParam;
            SetDlgItemText(hDlg,IDC_Message_Txt, msg);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    DestroyWindow (hDlg);
                    return TRUE;
            };
            break;

        case WM_CLOSE:
            DestroyWindow (hDlg);
            return TRUE;

        case WM_DESTROY:
            g_bMsgBox = FALSE;
            return TRUE;
    }

    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////
// YesNoProc poses a yes/no question, pointed to by lParam, on the test machine, with
// Yes and No buttons and 60 second count down timer, and waits for an acknowledgement
// via the Yes/No buttons or through the timer expiration (equivalent to No.)
//
// Parameters: hDlg, uiMsg, wParam, lParam
//
// Return value: TRUE if uiMsg processed, otherwise FALSE
/////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK YesNoProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    LPCTSTR msg;
    static int seconds;
    static UINT uiTimer;
    BOOL bReply = FALSE;   

    switch (uiMsg)
    {
        case WM_INITDIALOG:
            msg = (LPCTSTR)lParam;
            SetDlgItemText(hDlg,IDC_Text, msg);
            seconds = 60;
            SetDlgItemInt(hDlg, IDC_Count, seconds, FALSE);
            uiTimer = SetTimer(hDlg, ID_TIMER, 1000, NULL);
            if (uiTimer == 0)
            {
                SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDNO, 0), 0);
            }
            return TRUE;

        case WM_TIMER:
            if (seconds > 0)
            {
                SetDlgItemInt(hDlg, IDC_Count, --seconds, FALSE);
                Debug(TEXT("An important message is being displayed on the test machine. Time left to respond =%d seconds..."),seconds);
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
                    bReply = TRUE;
                    __fallthrough;

                case IDNO:
                    if (uiTimer)
                    {
                        KillTimer(hDlg, uiTimer);
                        uiTimer = 0;
                    }
                    EndDialog(hDlg, bReply);
                    return TRUE;
            };
    }

    return FALSE;
}


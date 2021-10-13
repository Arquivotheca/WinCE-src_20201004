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

/**************************************************************************


Module Name:

   doc.cpp

Abstract:

   Gdi Tests: Printing API's

***************************************************************************/

#include "global.h"

BOOL CALLBACK ap(HDC hdc, int iError)
{
    return FALSE;
}

void
passNull2Doc(int testFunc)
{
    info(ECHO, TEXT("%s - passNull2Doc"), funcName[testFunc]);

    TDC hdc = myGetDC();
    DOCINFO di = {0};
    di.cbSize = sizeof(DOCINFO);

    switch(testFunc)
    {
        case EStartDoc:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartDoc((TDC) NULL, &di));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartDoc(g_hdcBAD, &di));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartDoc(g_hdcBAD2, &di));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartDoc(hdc, NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EEndDoc:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndDoc((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndDoc(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndDoc(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EStartPage:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartPage((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartPage(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnStartPage(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EEndPage:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndPage((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndPage(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnEndPage(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case EAbortDoc:
            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnAbortDoc((TDC) NULL));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnAbortDoc(g_hdcBAD));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnAbortDoc(g_hdcBAD2));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
        case ESetAbortProc:

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnSetAbortProc((TDC) NULL, ap));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnSetAbortProc(g_hdcBAD, ap));
            CheckForLastError(ERROR_INVALID_HANDLE);

            SetLastError(ERROR_SUCCESS);
            CheckForRESULT(SP_ERROR, gpfnSetAbortProc(g_hdcBAD2, ap));
            CheckForLastError(ERROR_INVALID_HANDLE);
            break;
    }

    myReleaseDC(hdc);
}

//***********************************************************************************
TESTPROCAPI StartDoc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnStartDoc)
    {
        // Breadth
        passNull2Doc(EStartDoc);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI EndDoc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnEndDoc)
    {
        // Breadth
        passNull2Doc(EEndDoc);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI StartPage_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnStartPage)
    {
        // Breadth
        passNull2Doc(EStartPage);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI EndPage_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnEndPage)
    {
        // Breadth
        passNull2Doc(EEndPage);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI AbortDoc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnAbortDoc)
    {
        // Breadth
        passNull2Doc(EAbortDoc);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}

//***********************************************************************************
TESTPROCAPI SetAbortProc_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    if(gpfnSetAbortProc)
    {
        // Breadth
        passNull2Doc(ESetAbortProc);

        // Depth
        // None
    }
    else info(SKIP, TEXT("Printing API's unsupported on this image."));
    return getCode();
}


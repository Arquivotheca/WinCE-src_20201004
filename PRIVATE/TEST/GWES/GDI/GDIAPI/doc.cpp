//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

    DOCINFO di;
    TDC hdc = myGetDC();

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



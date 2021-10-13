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
/*--
Module Name: asp.h
Abstract: ASP data block definition
--*/


// struct used to pass data between httpd and ASP.dll, like extension control block
typedef struct _ASP_CONTROL_BLOCK 
{
    DWORD    cbSize;                    // size of this struct.
    HCONN    ConnID;                    // Points to calling request pointer
    HINSTANCE hInst;                // ASP dll handle, used for LoadString.
    DWORD    cbTotalBytes;            // Total bytes indicated from client
    DWORD   cbAvailable;            // Available number of bytes

    WCHAR*    wszFileName;            // name of asp file to execute
    PSTR    pszVirtualFileName;     // Virtual root of file, to display to user on error case.
    PSTR    pszForm;                // raw Form data
    PSTR    pszQueryString;            // raw QueryString data    
    PSTR    pszCookie;                // raw Cookie data, read only from client    

    // These values are read by httpd from the registry and are used  
    // if no ASP processing directive are on the executing page

    SCRIPT_LANG scriptLang;
    UINT        lCodePage;
    LCID        lcid;
    DWORD       fASPVerboseErrorMessages;

    // Familiar ISAPI functions
    BOOL (WINAPI * GetServerVariable) ( HCONN       hConn,
                                        LPSTR       lpszVariableName,
                                        LPVOID      lpvBuffer,
                                        LPDWORD     lpdwSize );

    BOOL (WINAPI * WriteClient)  ( HCONN      ConnID,
                                   LPVOID     Buffer,
                                   LPDWORD    lpdwBytes,
                                   DWORD      dwReserved );


    BOOL (WINAPI * ServerSupportFunction)( HCONN      hConn,
                                           DWORD      dwHSERequest,
                                           LPVOID     lpvBuffer,
                                           LPDWORD    lpdwSize,
                                           LPDWORD    lpdwDataType );

    // ASP specific fcns
    // Acts like AddHeader or SetHeader found in ISAPI filter fcns
    BOOL (WINAPI * AddHeader)(   HCONN hConn,
                                 LPSTR lpszName,
                                 LPSTR lspzValue);    

    // Sends data to client
    BOOL (WINAPI * Flush) ( HCONN hConn);

    // Clears data, if data is being buffered
    BOOL (WINAPI * Clear) ( HCONN hConn);


    // Accessors to whether we buffer data request or not
    BOOL (WINAPI * SetBuffer)    ( HCONN hConn,
                                   BOOL fBuffer);

    BOOL (WINAPI *ReceiveCompleteRequest)(struct _ASP_CONTROL_BLOCK *pcb);

    BOOL (WINAPI *ReadClient)(HCONN hConn, PVOID pv, PDWORD pdw);
} ASP_CONTROL_BLOCK, *PASP_CONTROL_BLOCK;






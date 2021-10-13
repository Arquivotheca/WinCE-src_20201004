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

extern CRITICAL_SECTION g_csMemory;

DllEntry (HINSTANCE hinstDLL,
                  DWORD   Op,
                  LPVOID  lpvReserved)
{
        switch (Op)
        {
        case DLL_PROCESS_ATTACH :
                InitializeCriticalSection(&g_csMemory);
                DisableThreadLibraryCalls((HMODULE)hinstDLL);
                break;
        case DLL_PROCESS_DETACH :
                DeleteCriticalSection(&g_csMemory);
                break;
        case DLL_THREAD_DETACH :
                break;
        case DLL_THREAD_ATTACH :
                break;
        default :
                break;
        }
        return TRUE;
}



//      @func DWORD | OBX_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from OBX_Open call
//  @parm LPCVOID | pBuf | buffer OBXtaining data
//  @parm DWORD | len | maximum length to write [IN BYTES, NOT WORDS!!!]
//  @rdesc      Returns -1 for error, otherwise the number of bytes written.  The
//                      length returned is guaranteed to be the length requested unless an
//                      error OBXdition occurs.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
//
// !!!!NOTE that this function ALWAYS expects ANSI text, NOT UNICODE!!!!
//
extern "C" DWORD
OBX_Write (DWORD dwData, LPCVOID pInBuf, DWORD dwInLen)
{
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return (DWORD)-1;
}


//      @func DWORD | OBX_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from OBX_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read [IN BYTES, not WORDS!!]
//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//                      bytes read.  The length returned is guaranteed to be the length
//                      requested unless end of file or an error OBXdition occurs.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
//
// !!!!NOTE that this function ALWAYS returns ANSI text, NOT UNICODE!!!!
//
extern "C" DWORD
OBX_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return (DWORD)-1;
}

//      @func PVOID | OBX_Open          | Device open routine
//  @parm DWORD | dwData                | value returned from OBX_Init call
//  @parm DWORD | dwAccess              | requested access (combination of GENERIC_READ
//                                                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc      Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                      unable to open device.
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
extern "C" DWORD
OBX_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
        return dwData;
}

//      @func BOOL | OBX_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from OBX_Open call
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//                      in as lpszType in RegisterDevice
extern "C" BOOL
OBX_Close (DWORD dwData)
{
        return TRUE;
}

//      @func DWORD | OBX_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from OBX_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc      Returns current position relative to start of file, or -1 on error
//      @remark Routine exported by a device driver.  "PRF" is the string passed
//               in as lpszType in RegisterDevice

extern "C" DWORD
OBX_Seek (DWORD dwData, long pos, DWORD type)
{
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return (DWORD)-1;
}


//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
////////////////////////////////////////////////////////////////////////////////
//
//  BLKIOCTL TUX DLL
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//  To create the function table and function prototypes, two macros are
//  available:
//
//      FTH(level, description)
//          (Function Table Header) Used for entries that don't have functions,
//          entered only as headers (or comments) into the function table.
//
//      FTE(level, description, code, param, function)
//          (Function Table Entry) Used for all functions. DON'T use this macro
//          if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

BEGIN_FTE
    FTH(0, "block driver standard i/o control code tests")
    FTE(1, "supports IOCTL_DISK_GETINFO", 1001, 0, Tst_IOCTL_DISK_GETINFO)
    FTE(1, "supports IOCTL_DISK_SETINFO", 1002, 0, Tst_IOCTL_DISK_SETINFO)
    FTE(1, "supports IOCTL_DISK_READ", 1003, 0, Tst_IOCTL_DISK_READ)
    FTE(1, "supports IOCTL_DISK_WRITE", 1004, 0, Tst_IOCTL_DISK_WRITE)
    FTE(1, "supports IOCTL_DISK_FORMAT_MEDIA", 1005, 0, Tst_IOCTL_DISK_FORMAT_MEDIA)

    FTH(0, "block driver extended i/o control code tests")
    FTE(1, "supports IOCTL_DISK_GETNAME", 2001, 0, Tst_IOCTL_DISK_GETNAME)
    FTE(1, "supports IOCTL_DISK_DEVICE_INFO", 2002, 0, Tst_IOCTL_DISK_DEVICE_INFO)
    FTE(1, "supports IOCTL_DISK_GET_STORAGEID", 2003, 0, Tst_IOCTL_DISK_GET_STORAGEID)
    FTE(1, "supports IOCTL_DISK_DELETE_CLUSTER", 2004, 0, Tst_IOCTL_DISK_DELETE_CLUSTER)
    FTE(1, "supports IOCTL_DISK_DELETE_SECTORS", 2005, 0, Tst_IOCTL_DISK_DELETE_SECTORS)

    FTH(0, "block driver depricated i/o control code tests")
    FTE(1, "supports DISK_IOCTL_GETINFO", 3001, 0, Tst_DISK_IOCTL_GETINFO)
    FTE(1, "supports DISK_IOCTL_SETINFO", 3002, 0, Tst_DISK_IOCTL_SETINFO)
    FTE(1, "supports DISK_IOCTL_READ", 3003, 0, Tst_DISK_IOCTL_READ)
    FTE(1, "supports DISK_IOCTL_WRITE", 3004, 0, Tst_DISK_IOCTL_WRITE)
    FTE(1, "supports DISK_IOCTL_GETNAME", 3005, 0, Tst_DISK_IOCTL_GETNAME)
    FTE(1, "supports DISK_IOCTL_FORMAT_MEDIA", 3006, 0, Tst_DISK_IOCTL_FORMAT_MEDIA)

END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__

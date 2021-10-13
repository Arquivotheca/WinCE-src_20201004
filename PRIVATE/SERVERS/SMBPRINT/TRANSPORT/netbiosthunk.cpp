//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#define WINCEMACRO 1
#include "nb.h"
#include <windows.h>
#include <afdfunc.h>



//
// NOTE: this is somewhat of a hack.  the reason is that the f() NETbios 
//   requires WINCEMACRO to be set (for the preprocessor to sub with
//   the actual address).  BUT svsutils.hxx has CloseHandle() defined.
//   because of this the CloseHandle() in svsutils.hxx gets substitued
//   with the actual address resulting in chaos.  This file allows for only
//   one small part of the code to actually be sub'ed
DWORD NETbiosThunk(DWORD x1, DWORD dwOpCode, PVOID pNCB, DWORD cBuf1,
              PBYTE pBuf1, DWORD cBuf2, PDWORD pBuf2)
{    
    return NETbios(x1, dwOpCode, pNCB, cBuf1,
              pBuf1, cBuf2, pBuf2);
}

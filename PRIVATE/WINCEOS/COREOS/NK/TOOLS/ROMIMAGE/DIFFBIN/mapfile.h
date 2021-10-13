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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

	mapfile.h

Abstract:

    Class for a memory mapped file

Author:

	Scott Shell (ScottSh)

Environment:

	Win32

Revision History:

    30-Jun-2001 Scott Shell (ScottSh)       Created    

-------------------------------------------------------------------*/
#pragma once
#include <iostream>
#include <string>
#include "ehm.h"

using namespace std;

class CMappedFile
{
public:
    CMappedFile(char *szFilename);
    ~CMappedFile(); 

    HRESULT Open(DWORD dwAccess, DWORD dwCreationDisposition, DWORD dwShareMode);
    HRESULT Close();
    void SetFileSize(DWORD dwSize) { m_dwMapSize = dwSize; }

    LPVOID  GetDataPtr() { return m_pvData; }
    DWORD   GetDataSize() { return m_dwMapSize; }

private:
    string      m_strFilename;
    DWORD       m_dwMapSize;
    HANDLE      m_hFile;
    HANDLE      m_hMapping;
    LPVOID      m_pvData;
};

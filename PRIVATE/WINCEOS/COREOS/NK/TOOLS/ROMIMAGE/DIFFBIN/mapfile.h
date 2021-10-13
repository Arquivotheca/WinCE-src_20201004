//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

	mapfile.h

Abstract:

    Class for a memory mapped file



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

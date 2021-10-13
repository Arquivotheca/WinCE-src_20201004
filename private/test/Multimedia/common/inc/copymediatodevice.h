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

#ifndef _COPYMEDIATODEVICE_
#define _COPYMEDIATODEVICE_

#include <windows.h>

#ifndef UNDER_CE
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#else
#pragma comment(lib, "Ceshell.lib")
#include <storemgr.h>
#endif

#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>

typedef enum MediaType{
    MT_PICTURE = 1,
    MT_MUSIC = 2,
    MT_VIDEO = 4
};

//1==Whitelist dirs on device
//2==Whielist dirs on Storage Card
//4==Non-Whitelist dir on device
//8==Non-Whitelist dir on Storage Card
//16==Copy Single File to whitelists on Device
//32==Copy Single File to whitelists on Storage Card
//64==Copy Single File to Non-Whitelists on Device
//128==Copy Single File to Non-Whitelists on Storage Card
typedef enum StorageType{
    ST_Device = 1,
    ST_External = 2,
    ST_NonLibraryDevice = 4,
    ST_NonLibraryExternal = 8,
    ST_Single_Device = 16,
    ST_Single_External = 32,
    ST_NonLibrary_Single_Device = 64,
    ST_NonLibrary_Single_External = 128,
};

class  CCopyMediaToDevice
{

public:
	CCopyMediaToDevice();
	~CCopyMediaToDevice();
	void SetPathToStore(TCHAR *pszPathToStore);
	void SetStorageType(DWORD dwStorageType);
	void SetDefaultType(DWORD dwDefaultType);
	void SetSourceDir(TCHAR *pszSourceDir);
	void SetRootPath(TCHAR *pszSourceDir);
	TCHAR * GetPathToStore();
	DWORD   GetStorageType();
	DWORD   GetDefaultType();
	TCHAR * GetSourceDir();
	TCHAR * GetRootPath();
	//Determines the destination directories for different media types and creates them if they don't exist
	HRESULT MakeDestDirOnDevice(); 
    //Recurses through the entire path of CreateDirectory and creates all the directories.
    HRESULT RecursiveCreateDirectory(TCHAR * Path);
	//Copies the specified file from the source directory to dest directory based on the media type
	HRESULT CopyFileFromSrcToDest(DWORD dwMediaType, TCHAR *pszFileName);
	//Copies all the files from the source directory to dest directory one by one based on the media type
	HRESULT CopyAllFiles();


public:
	//Path where the media files should be copied to
	TCHAR	*m_PathToStore;
	//Storage Type (For Ex: On Device or External = Storage Card)
	DWORD	m_StorageType;
	//This will be the default media type if the tool is unable to determine a media type for the file
	DWORD	m_DefaultType;
    //This local flag detemines if media needs to be copied to the whitelisted dirs or an external dir
    int     m_fLFlag; 
    //This single flag determines if the copy is a single file copy
    int     m_fSingle;
	TCHAR	*m_SourceDir;
	//Root Path of the Storage
	TCHAR	*m_RootPath;
	TCHAR	m_DestDirPicture[MAX_PATH];
	TCHAR	m_DestDirMusic[MAX_PATH];
	TCHAR	m_DestDirVideo[MAX_PATH];
};

#endif





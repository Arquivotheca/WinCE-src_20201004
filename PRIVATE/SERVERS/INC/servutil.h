//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: servutil.h
Abstract: Common macros for servers project
--*/


#ifndef UNDER_CE
#ifdef DEBUG
#	define DEBUGMSG(x, y)	wprintf y
#	define DEBUGCHK(exp) ((void)((exp)?1: (wprintf(TEXT("DEBUGCHK failed in file %s at line %d \r\n"), TEXT(__FILE__) ,__LINE__ ),0)))
#	define RETAILMSG(x,y)   wprintf y
#else
#	define DEBUGMSG(x, y)	
#	define DEBUGCHK(exp) 
#	define RETAILMSG(x,y)   
#endif	// DEBUG
#endif	// UNDER_CE

// Debug Macros
// Some functions use a local variable err to help with debugging messages,
// -- if err != 0 then there's been an error, which will be print out.
// However we don't want this extra variable and checks in retail mode.

#ifdef DEBUG
#define DEBUG_CODE_INIT		int err = 0;
#define DEBUGMSG_ERR(x,y)    { if (err)  {  DEBUGMSG(x,y); } } 
#define myretleave(r,e)	{ ret=r; err=e; goto done; }
#define myleave(e)		{ err=e; goto done; }
#else
#define DEBUG_CODE_INIT		
#define DEBUGMSG_ERR(x,y)    
#define myretleave(r,e)	{ ret=r; goto done; }
#define myleave(e)		{ goto done; }
#endif



#define ARRAYSIZEOF(x)	(sizeof(x) / sizeof((x)[0]))
#define CCHSIZEOF		ARRAYSIZEOF
#define ZEROMEM(p)		memset(p, 0, sizeof(*(p)))

#define CELOADSZ(ids)		((LPCTSTR)LoadString(g_hInst, ids, NULL, 0)	)

inline void *svsutil_AllocZ (DWORD dwSize) {
    void *pvRes = svsutil_Alloc (dwSize, g_pvAllocData);

    if (pvRes)
        memset (pvRes, 0, dwSize);

    return pvRes;
}

inline void *MyRealloc(DWORD dwSizeOld, DWORD dwSizeNew, BYTE *pvDataOld) {
	DEBUGCHK(dwSizeOld < dwSizeNew);
#if (CE_MAJOR_VERSION >= 4)
	// SVS Realloc capabilities were added in WinCE 4.0
	void *pvRes = g_funcRealloc(pvDataOld,dwSizeNew,g_pvAllocData);
#else
	BYTE *pvRes = (BYTE *) svsutil_Alloc(dwSizeNew, g_pvAllocData);

	if (pvRes) {
		memcpy(pvRes,pvDataOld,dwSizeOld);
		memset(pvRes + dwSizeOld,0,dwSizeNew - dwSizeOld);
		g_funcFree(pvDataOld,g_pvFreeData);
	}
#endif
	if (pvRes) {
		memset(((char*)pvRes)+dwSizeOld,0,dwSizeNew-dwSizeOld);
	}

	return (void*) pvRes;
}

#define MyAllocZ(typ)		((typ*)svsutil_AllocZ(sizeof(typ)))
#define MyAllocNZ(typ)		((typ*)g_funcAlloc(sizeof(typ), g_pvAllocData))
#define MyRgAllocZ(typ, n)	((typ*)svsutil_AllocZ((n)*sizeof(typ)))
#define MyRgAllocNZ(typ, n)	((typ*)g_funcAlloc((n)*sizeof(typ), g_pvAllocData))
#define MyRgReAlloc(typ, p, nOld, nNew)	((typ*) MyRealloc(sizeof(typ)*(nOld), sizeof(typ)*(nNew), (BYTE*) (p))) 
#define MyFree(p)			{ if (p) { g_funcFree ((void *) p, g_pvFreeData); (p)=0;}  }
#define MyFreeNZ(p)         { if (p) { g_funcFree ((void *) p, g_pvFreeData);}  }

#define MySzAllocA(n)		MyRgAllocNZ(CHAR, (1+(n)))
#define MySzAllocW(n)		MyRgAllocNZ(WCHAR, (1+(n)))
#define MySzReAllocA(p, nOld, nNew)	 MyRgReAlloc(CHAR, p, nOld, (1+(n)))



#define ResetString(oldStr, newStr)   { MyFree(oldStr); oldStr = MySzDupA(newStr); }

#define Nstrcpy(szDest, szSrc, nLen)     { memcpy((szDest), (szSrc), (nLen));   \
										   (szDest)[(nLen)] = 0; }

// Copy from pszDest to pszDest, and move 
#define CONSTSIZEOF(x)		(sizeof(x)-1)

#define NTFAILED(x)          (INVALID_HANDLE_VALUE == (x))


#define MyFreeLib(h)		{ if(h) FreeLibrary(h); }
#define MyCloseHandle(h)	{ if(INVALID_HANDLE_VALUE != h) CloseHandle(h); }
#define MyCreateProcess(app, args) CreateProcess(app, args, NULL,NULL,FALSE,0,NULL,NULL,NULL,NULL)
#define MyCreateThread(fn, arg)    CreateThread(NULL, 0, fn, (LPVOID)arg, 0, NULL)
#define MyOpenReadFile(path)       CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)
#define MyOpenAppendFile(path)     CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)
#define MyWriteFile(path)          CreateFile(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)
#define MyOpenQueryFile(path)      CreateFile(path, 0, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)


#define abs(x)		( (x) < 0 ? -(x) : (x) )
#define MyStrlenA(str)   ( str ? strlen(str) : 0 )
#define MyStrlenW(str)   ( str ? wcslen(str) : 0 )
//------------- Error handling macros ------------------------

#define GLE(e)			(e ? GetLastError() : 0)

/////////////////////////////////////////////////////////////////////////////
// Misc string handling helpers
/////////////////////////////////////////////////////////////////////////////

#define MyA2W(psz, wsz, iOutLen) MultiByteToWideChar(CP_ACP, 0, psz, -1, wsz, iOutLen)
#define MyW2A(wsz, psz, iOutLen) WideCharToMultiByte(CP_ACP, 0, wsz, -1, psz, iOutLen, 0, 0)
#define MyW2ACP(wsz, psz, iOutLen, lCodePage) WideCharToMultiByte(lCodePage, 0, wsz, -1, psz, iOutLen, 0, 0)


extern BOOL g_fUTF8;

// Tries UTF8 first, if not on system falls back to CP_ACP
inline DWORD MyW2UTF8(const WCHAR *wsz, PSTR psz, int iOutLen) {
	return MyW2ACP(wsz,psz,iOutLen,(g_fUTF8 ? CP_UTF8 : CP_ACP));
}

inline BOOL DoesSystemSupportUTF8(void) {
	CHAR szBuf[100];
	WCHAR *wsz = L"Test";
	
	g_fUTF8 = (0 != MyW2ACP(wsz,szBuf,sizeof(szBuf),CP_UTF8));
	return g_fUTF8;
}

inline BOOL GetUTF8OrACP(void) { 
	return g_fUTF8 ? CP_UTF8 : CP_ACP; 
}


#ifdef OLD_CE_BUILD
#define _stricmp(sz1, sz2) _memicmp(sz1, sz2, 1+min(strlen(sz1), strlen(sz2)))
#define _strnicmp(sz1, sz2, len) _memicmp(sz1, sz2,len)
#endif

#define strcmpi	_stricmp

//  max # of times we try to get our server going in device.exe
#define MAX_SERVER_STARTUP_TRIES   60



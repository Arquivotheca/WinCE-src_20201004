//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Module Name:

     fpfperp.h  

Abstract:
Functions:
Notes:
--*/
//
// fpfperp.h
//
// implement fopen, fclose, fprintf, printf, and freopen for PeRP
//
// History:
//		95-Jul-27	mtoepke		created
//

#ifndef _FPFPERP_H_
#define _FPFPERP_H_


#include <tchar.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
#ifdef UNDER_CE

//
// misc stuff we'd normally get from stdio.h.
//

#define EOF			(-1)

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2


#endif // UNDER_CE
*/


#ifdef UNDER_CE

typedef struct _PTU_FILE {
	HANDLE hFile;
	DWORD dwFlags;
	DWORD dwPos;
	DWORD dwBufPos;
	DWORD dwBufSize;
	unsigned char *pbBuf;
} PTU_FILE;

#else

#define PTU_FILE FILE

#endif  // UNDER_CE

BOOL fpfperpInit( void );
void fpfperpClose( void );

#ifdef UNDER_CE
#define ptu_stdin	(NULL)
#define ptu_stdout	(NULL)
#define ptu_stderr	(NULL)
#else
#define ptu_stdin	stdin
#define ptu_stdout	stdout
#define ptu_stderr  stderr
#endif

#define ptu_stdres	(NULL)
#define ptu_stdsum	(NULL)


#ifdef DEBUG
LONG fpfperpGetInitCount();
#endif

//
// tchar defines may not be in CE if sysgened out
//

#ifdef UNDER_CE

#define ptu_tprintf	        ptu_wprintf
#define ptu_ftprintf        ptu_fwprintf
#define ptu_tfopen          ptu_wfopen
#define ptu_tfreopen        ptu_wfreopen

#else

#define ptu_tprintf         _tprintf
#define ptu_ftprintf        _ftprintf

#ifdef DEBUG
PTU_FILE    *ptu_tfopen( const TCHAR *filename, const TCHAR *mode );
#else
#define ptu_tfopen          _tfopen
#endif

#define ptu_tfreopen        _tfreopen

#endif // UNDER_CE


#ifndef UNDER_CE


#define ptu_wfopen( a, b )          ASSERT(fpfperpGetInitCount()),_wfopen( a, b )
#define ptu_fclose( a )             fclose( a )
#define ptu_wfreopen( a, b, c )     _wfreopen( a, b, c )
#define ptu_fwprintf                fwprintf
#define ptu_wprintf                 wprintf
#define ptu_fread( a, b, c, d )     fread( a, b, c, d )
#define ptu_fwrite( a, b, c, d )    fwrite( a, b, c, d )
#define ptu_ftell( a )              ftell( a )
#define ptu_fseek( a, b, c )        fseek( a, b, c )
#define ptu_fflush( a )             fflush( a )

#else

PTU_FILE    *ptu_wfopen( const TCHAR *filename, const TCHAR *mode );
int         ptu_fclose( PTU_FILE *stream );
PTU_FILE    *ptu_wfreopen( const TCHAR *path, const TCHAR *mode, PTU_FILE *stream );
int         ptu_fwprintf( PTU_FILE *stream, const TCHAR *format, ... );
int         ptu_wprintf( const TCHAR *format, ... );
size_t      ptu_fread( void *buffer, size_t size, size_t count, PTU_FILE *stream );
size_t      ptu_fwrite( void *buffer, size_t size, size_t count, PTU_FILE *stream );
long        ptu_ftell( PTU_FILE *stream );
int         ptu_fseek( PTU_FILE *stream, long offset, int origin );
int         ptu_fflush( PTU_FILE *stream );

#endif  // UNDER_CE

BOOL _tfcopy( const TCHAR *oldPath, const TCHAR *newPath );

#define LOG		ptu_ftprintf


#ifdef __cplusplus
}
#endif

#endif // _FPFPERP_H_

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

    qautils.h
Abstract:

    Includes all other header files.  Will be used to build the qautils.lib

--*/

#ifndef __QAUTILS_H__
#define __QAUTILS_H__

#include <windows.h>
#ifdef UNDER_NT
#include <sehmap.h>
#endif
#include <tchar.h>
#include "testlog.h"
#include "random.h"



#ifdef   __cplusplus
extern "C" {
#endif

// @doc QAUTILS

// -------------------------------------------------------------------	
//
// CRC32.C
//

unsigned int  
CRC_Compute32( unsigned int CrcSeed, char* Buffer, unsigned Count );


// -------------------------------------------------------------------	
//
// STRING.C
//

BOOL  
GetRandomExtendedString(
    LPTSTR ptszBuffer,
    UINT uBufferLen, 
    WORD wCType
	);

BOOL  
GetRandomExtendedStringEx(
    LPTSTR ptszBuffer,
    UINT uBufferLen,
    WORD wCType, 
	LPTSTR ptszExcl
    );

int  
RemoveCharFromString ( LPTSTR ptszStr, TCHAR tch );


// -------------------------------------------------------------------	
//
// COMMAND.C
//

int  
CreateArgvArgc(TCHAR *pProgName, TCHAR *argv[20], TCHAR *pCmdLine);



// -------------------------------------------------------------------	
//
// MISC.C
//

LPTSTR  
GetPlatform(
    LPTSTR tszBuffer,       
    INT nBuffLen            
    );

//
// Generic error reporter.
//
DWORD  ErrorOut( LPTSTR szFmt, ...  );
DWORD  ErrorCodeOut( LPTSTR szFmt, ...  );
void  DebugOut( LPTSTR szFmt, ...  );


// Force use of our random number generator for results that are
// consistent across all platforms.
//
#define srand(seed)     RND_srand(NULL, seed)
#define rand()          RND_rand(NULL)


#ifdef   __cplusplus
}
#endif


#endif /* __QAUTILS_H__ */

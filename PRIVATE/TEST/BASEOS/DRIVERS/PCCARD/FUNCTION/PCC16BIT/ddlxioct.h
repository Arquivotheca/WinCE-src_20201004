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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	ddlxioct.h

Abstract:

    definition of ddlx interface

--*/


#ifndef __DDLXIOCT_H__
#define __DDLXIOCT_H__

typedef enum DRIVECOMMAND{
    DRV_IOCTL_CMD_NONE,
    DRV_IOCTL_CMD_SHELL,
    DRV_IOCTL_CMD_TEST,
    DRV_IOCTL_CMD_PING,
	DRV_IOCTL_CMD_DEBUG,
    DRV_IOCTL_CMD_MAX
}DRIVECOMMAND;

typedef enum KATOCOMMAND {
	C_BeginLevel,
	C_EndLevel,
	C_Log,
	C_Comment
}KATOCOMMAND;

#define	QUEUENAME	_T("DDLXKatoQueue")
#define	LISTENTHREAD_TIME_OUT		6000	//3    // 6 sec
#define	LISTEN_WAIT_TIME			2000	// 2sec
#define	TALK_WAIT_TIME			1000	// 3sec

//message structure
typedef struct _KATOMSGFORMAT{
	DWORD dwCommand;	//command type
	DWORD	dwOption;		//option
	TCHAR	szMsg[1023];		//debugoutput body
}KATOMSGFORMAT, *PKATOMSGFORMAT;

// Passed from the calling shell on SPM_SHELL_INFO.
typedef struct {
    BOOL fUsingServer;
    TCHAR tDllCmdLine[256];
} SPI_SHELL_INFO, *LPSPI_SHELL_INFO;

// Local copy made by ioctl
typedef struct {
    SPI_SHELL_INFO PassedShellInfo;
    SPS_SHELL_INFO LocalShellInfo;
} SPL_SHELL_INFO, *LPSPL_SHELL_INFO;

typedef union {
    FUNCTION_TABLE_ENTRY    FTE;
    BYTE                    Bf[6*16*1024];
} GLOBAL_FTE, *LPGLOBAL_FTE;

//
//Kato logging class
//

class DDLXKato_Talk{

public:
	DDLXKato_Talk(BOOL bUse =FALSE){bUseMsgQueue = bUse;};
	~DDLXKato_Talk(){;};
	BOOL Init();
	BOOL Stop();
	VOID  UseMsgQueue(BOOL	bUse){bUseMsgQueue = bUse;};

    	INT BeginLevel (DWORD dwLevelID, LPCWSTR wszFormat, ...);
   	INT  EndLevel (LPCWSTR wszFormat, ...);
   	BOOL Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...);
   	BOOL Comment (DWORD dwVerbosity, LPCWSTR wszFormat, ...);

private:
	HANDLE	hMsgQueue;
	CKato*	pKato;
	BOOL	bUseMsgQueue;
};



#endif // __DDLXIOCT_H__

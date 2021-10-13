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
#if 0
-------------------------------------------------------------------------------

Copyright (c) 1998-1999, Microsoft Corporation

Module Name:

	ddlxioct.h

Description:

	Contains the driver ioctl defines

-------------------------------------------------------------------------------
#endif


#pragma once

typedef enum DRIVE_COMMAND{
    DRV_IOCTL_CMD_NONE,
    DRV_IOCTL_CMD_SHELL,
    DRV_IOCTL_CMD_TEST,
    DRV_IOCTL_CMD_PING,
	DRV_IOCTL_CMD_DEBUG,
	DRV_IOCTL_CMD_LOADTEST,
	DRV_IOCTL_CMD_UNLOADTEST,
    DRV_IOCTL_CMD_MAX
}DRIVE_COMMAND;

typedef enum KATOCOMMAND {
	C_BeginLevel,
	C_EndLevel,
	C_Log,
	C_Comment
}KATOCOMMAND;

#define	QUEUENAME	_T("DDLXKatoQueue")
#define	LISTENTHREAD_TIME_OUT		6000	// 6sec
#define	LISTEN_WAIT_TIME			2000	// 2sec

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





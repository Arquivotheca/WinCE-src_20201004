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
/*---------------------------------------------------------------------------*\
 *
 *  module: shell.h
 *
\*---------------------------------------------------------------------------*/
#ifndef __SHELL_H__
#define __SHELL_H__

typedef enum tagCMDS
{
  CMDS_EXIT = 0,
  CMDS_START,
  CMDS_S,
  CMD_GI,
  CMD_ZO,
  CMD_BREAK,
  CMD_RESDUMP,
  CMD_RESFILTER,
  CMD_KILLPROC,
  CMD_HELP,
  CMD_DUMPDWORDS,
  CMD_DUMPFILE,
  CMD_MEMTOOL,
  CMD_RUN,
  CMDS_DISCARD,
  CMDS_WINLIST,
  CMDS_LOG,
  CMDS_MEMTRACK,
  CMDS_HEAPDUMP,
  CMDS_OPTIONS,
  CMDS_SUSPEND,
  CMDS_PROFILE,
  CMDS_LOADEXT,
  CMDS_THREADPRIO,
  CMDS_FINDFILE,
  CMDS_MAX,
  CMDS_NO_COMMAND = -2,
  CMDS_UNKNOWN = -1
} CMDS;

#define IsValidCommand(x)  (((x)>CMDS_UNKNOWN)&&((x)<CMDS_MAX))
#define MAX_CMD_LINE 2304  // Minimum required for passing URLs to iexplore

typedef BOOL (* CMDPROC)(PTSTR cmdline);
typedef struct tagCMDCLASS
{
  DWORD   dwFlags; /* should be zero for now */
  TCHAR   szCommand[32];
  CMDPROC lpfnCmdProc;
} CMDCLASS;
typedef CMDCLASS* LPCMDCLASS;

CMDS  FindCommand(PTSTR *cmdline, PTSTR *cmd);
BOOL  DoCommand(PTSTR cmdline);

BOOL  DoHelp(PTSTR cmdline);
BOOL  DoDumpDwords(PTSTR cmdline);
BOOL  DoDumpFile(PTSTR cmdline);
BOOL  DoExit(PTSTR cmdline);
BOOL  DoStartProcess(PTSTR cmdline);
BOOL DoSetLog (PTSTR cmdline);
BOOL DoGetInfo (PTSTR cmdline);
BOOL DoZoneOps (PTSTR cmdline);
BOOL DoBreak (PTSTR cmdline);
BOOL DoResDump (PTSTR cmdline);
BOOL DoResFilter (PTSTR cmdline);
BOOL DoKillProc (PTSTR cmdline);
BOOL DoMemtool (PTSTR cmdline);
BOOL DoRunBatchFile(PTSTR cmdline);
BOOL DoDiscard(PTSTR cmdline);
BOOL DoWinList(PTSTR cmdline);
BOOL DoLog (PTSTR cmdline);
BOOL DoMemTrack (PTSTR cmdline);
BOOL DoHeapDump (PTSTR cmdline);
BOOL DoOptions (PTSTR cmdline);
BOOL DoSuspend (PTSTR cmdline);
BOOL DoProfile (PTSTR cmdline);
BOOL DoLoadExt (PTSTR cmdline);
BOOL DoThreadPrio (PTSTR cmdline);
BOOL DoFindFile (PCTSTR cmdline);
BOOL DoDeviceInfo (PCTSTR cmdline);
BOOL DoAcctList (PTSTR cmdline);

#define SHELL_EXTENSION_FUNCTION    TEXT("ParseCommand")
#define TXTSHELL_REG                TEXT("Software\\Microsoft\\TxtShell")
#define TSH_EXT_REG                 TXTSHELL_REG TEXT("\\Extensions")
#define MAINTHDPRIO_RGVAL           TEXT("MainThreadPrio")

typedef void (*PFN_FmtPuts)(TCHAR *s,...);
typedef TCHAR * (*PFN_Gets)(TCHAR *s, int cch);
typedef BOOL (*PFN_ParseCommand)(LPCTSTR szCmd, LPCTSTR szCmdLine, PFN_FmtPuts pfnFmtPuts, PFN_Gets pfnGets);

typedef struct _SHELL_EXTENSION {
    struct _SHELL_EXTENSION     *pNext;
    LPTSTR                      szName;
    LPTSTR                      szDLLName;
    HMODULE                     hModule;
    PFN_ParseCommand            pfnParseCommand;
} SHELL_EXTENSION, *PSHELL_EXTENSION;

#endif

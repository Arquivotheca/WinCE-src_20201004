//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
  CMDS_LMEM,
  CMDS_OPTIONS,
  CMDS_SUSPEND,
  CMDS_PROFILE,
  CMDS_LOADEXT,
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
BOOL DoLMem (PTSTR cmdline);
BOOL DoProfile (PTSTR cmdline);
BOOL DoLoadExt (PTSTR cmdline);

#define SHELL_EXTENSION_FUNCTION    TEXT("ParseCommand")
#define TXTSHELL_REG                TEXT("Software\\Microsoft\\TxtShell")
#define TSH_EXT_REG                 TXTSHELL_REG TEXT("\\Extensions")
#define MAINTHDPRIO_RGVAL           TEXT("MainThreadPrio")

typedef void (*PFN_FmtPuts)(TCHAR *s,...);
typedef TCHAR * (*PFN_Gets)(TCHAR *s, int cch);
typedef BOOL (*PFN_ParseCommand)(LPTSTR szCmd, LPTSTR szCmdLine, PFN_FmtPuts pfnFmtPuts, PFN_Gets pfnGets);

typedef struct _SHELL_EXTENSION {
    struct _SHELL_EXTENSION     *pNext;
    LPTSTR                      szName;
    LPTSTR                      szDLLName;
    HMODULE                     hModule;
    PFN_ParseCommand            pfnParseCommand;
} SHELL_EXTENSION, *PSHELL_EXTENSION;

#endif

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

#if ! defined (__cmdHXX__)
#define __cmdHXX__  1

//
//  Includes
//
#include <windows.h>
#include <tchar.h>

//
//  Global defines
//
#define CMD_PARSE_ERROR         0x80000000

#define CMD_EXEC_QUIET          1
#define CMD_EXEC_ONCE           2

#define CMD_SUCCESS             0
#define CMD_ERROR               1
#define CMD_ERROR_FNF           2
#define CMD_ERROR_TOOBIG        3
#define CMD_ERROR_CTRLC         4

#define CMD_ECHO_ON             1
#define CMD_EXEC_CTRLC          2
#define CMD_EXEC_EXIT           4
#define CMD_EXEC_SKIPDOWN       8

#define CMD_BUF_HUGE            1024
#define CMD_BUF_BIG             512
#define CMD_BUF_REG             256
#define CMD_BUF_SMALL           128
#define CMD_BUF_TINY            64
#define CMD_BUF_COMMAND         10
#define CMD_BUF_INTEGER         80

#define CMD_BUF_INCREMENT       CMD_BUF_REG

#define CMD_EE_SORTED_SIZE      128

#define CMD_EHASH_BUCKETS       31
#define CMD_EHASH_ROL           7
#define CMD_EHASH_ROL2          (sizeof(unsigned int) * 8 - CMD_EHASH_ROL)

#define CMD_PARSER_HASWILDCARDS 0x40000000
#define CMD_PARSER_FILENUM      0x00000fff

#define CMD_TYPE_MULTIPLE       0
#define CMD_TYPE_SINGLE         1

#define CMD_DEL_FORCE           1
#define CMD_DEL_ASK             2

#define CMD_DIR_PAUSE           0x00000001
#define CMD_DIR_WIDE            0x00000002
#define CMD_DIR_BARE            0x00000004
#define CMD_DIR_LOWER           0x00000008
#define CMD_DIR_SEP             0x00000010
#define CMD_DIR_TIME_CREAT      0x00000100
#define CMD_DIR_TIME_ACC        0x00000200
#define CMD_DIR_TIME_WRITE      0x00000000
#define CMD_DIR_TIME_MASK       0x00000300
#define CMD_DIR_ERROR           -1

#define CMD_FOUND_NONE          0
#define CMD_FOUND_EXE           1
#define CMD_FOUND_BATCH         2
#define CMD_FOUND_SHELLEXEC     3

#define REDIR_TYPE_NONE         0
#define REDIR_TYPE_APPEND       1
#define REDIR_TYPE_ERASE        2

#define DIRORDER_FILES          1
#define DIRORDER_PRE            2
#define DIRORDER_POST           4

#define PATTERN_ALL         TEXT("\\*.*")
#define PATTERN_ALL_CHARS   (sizeof(PATTERN_ALL) / sizeof(PATTERN_ALL[0]))

#define CMDIO_IN    0
#define CMDIO_OUT   1
#define CMDIO_ERR   2

#if defined (UNICODE)
#define _ttoupper(c) (towupper(c))
#else
#define _ttoupper(c) (toupper(c))
#endif

//
//  Function prototypes
//
void cmdutil_Abort (int iFormat, ...);
void cmdutil_Complain (int iFormat, ...);
int cmdutil_Confirm (int iFormat, ...);
void cmdutil_ConsolePause (void);

void *cmdutil_Alloc (int iSize);
void *cmdutil_ReAlloc (void *pvBlock, int iNewSize);
void cmdutil_Free (void *lpBlock);
TCHAR *cmdutil_tcsdup (TCHAR *lpStr);
int cmdutil_CountNonWhite (TCHAR *lpString);
int cmdutil_CountNonWhiteWithQuotes (TCHAR *lpString);
int cmdutil_CountWhite (TCHAR *lpString);
void cmdutil_Trim (TCHAR *lpStr);
int cmdutil_HasWildCards (TCHAR *lpName);
int cmdutil_NeedQuotes (TCHAR *lpString);
void cmdutil_RemoveQuotes (TCHAR *lpStart);
int cmdutil_HierarchyMarker (TCHAR *name);
int cmdutil_MaskAll (TCHAR *lpName);

int cmdutil_ExistsFile (TCHAR *lpDirName);
int cmdutil_ExistsDir (TCHAR *lpDirName);
int cmdutil_DeleteAll (TCHAR *lpDirName, int iBufSize);

int cmdutil_FormPath (TCHAR *lpBuffer, int iBufSize, TCHAR *lpOffset, TCHAR *lpCwd);

void *cmdutil_GetFMem (int size);
void cmdutil_ReleaseFMem (void *pvBlock);
void *cmdutil_FMemAlloc (void *pvBlock);
void cmdutil_FMemFree (void *pvBlock, void *pvData);

struct PointerList;
void cmdutil_FixedFreePointerList (PointerList *ppl);
PointerList *cmdutil_NewPointerListItem (void *pvData, PointerList *ppl);
PointerList *cmdutil_InvertPointerList (PointerList *ppl);

int cmdutil_ParseCommandParameters (PointerList *&, PointerList *&, TCHAR *);
int cmdutil_ParseParamsUniform (PointerList *&pplList, TCHAR *pString);
int cmdutil_ExecuteForPattern (TCHAR *lpName, int iBufSize, void *pvData, int fRecurse, int (*fpWhat)(TCHAR *lpFile, void *pvArg));
int cmdutil_ExecuteForPatternRecursively (TCHAR *, int, TCHAR *, TCHAR *, int, void *, int, int (*)(TCHAR *, void *), int* pError);
int cmdutil_GetOptionListFromEnv (TCHAR *&pBuf, PointerList *&ppl, TCHAR *lpEnvName);

int   cmdexec_Execute (void);

int cmdutil_FindInPath (TCHAR *lpBuffer, int iBufSize, TCHAR *lpName);

TCHAR *cmdutil_DayOfWeek (int iDay);

int cmdutil_IsInvisible (TCHAR *lpName);

void cmd_InputLoop (void);
int cmd_GetInput (void);

//
//  Command table
//
struct Command {
    TCHAR lpCmdName[CMD_BUF_COMMAND];
    int   iMsgHelp;
    int   iMsgExtHelp;
    int  (*proc) (TCHAR *lpCmdName, TCHAR *lpArgString);
};

//
//  Environment
//
struct EnvEntry;
struct EnvEntry {
    TCHAR       *lpName;
    TCHAR       *lpValue;
    EnvEntry    *pNextEE;
};

class EnvHash {
private:

    EnvEntry    *eeHashTable[CMD_EHASH_BUCKETS];
    int         iEENum;

    TCHAR       *lpEnv;

    int hashval (TCHAR *lpVarName) {
        unsigned int res = 0;

        while (*lpVarName)
            res = ((res << CMD_EHASH_ROL) | (res >> CMD_EHASH_ROL2)) +
                                                            *lpVarName++;

        return (int)(res % CMD_EHASH_BUCKETS);
    }

    void free_env (void) {
        if (lpEnv) {
            cmdutil_Free (lpEnv);
            lpEnv = NULL;
        }
    }

public:
    EnvEntry    *hash_set (TCHAR *lpVarName, TCHAR *lpValue);
    TCHAR       *hash_get (TCHAR *lpVarName);
    TCHAR       *build (void);
};

struct PointerList {
    void        *pData;
    PointerList *pNext;
};

typedef BOOL (WINAPI *SEE_t)(LPSHELLEXECUTEINFO lpExecInfo);

//
//  Global state
//
struct CmdState {
    HINSTANCE       hInstance;
    unsigned int    uiFlags;
    int             iError;
    int             fCtrlC;

    int             xMax;
    int             yMax;

    int             iBufInputChars;
    TCHAR           *lpBufInput;
    
    int             iBufCmdChars;
    TCHAR           *lpBufCmd;
    
    int             iBufProcChars;
    TCHAR           *lpBufProc;

    TCHAR           *lpCmdStart;
    TCHAR           *lpArgStart;

    TCHAR           *lpIORedir[3];
    int             iIORedirType[3];

    TCHAR           *lpIO[3];
    FILE            *fpIO[3];

    TCHAR           *lpCommandInput;
    FILE            *fpCommandInput;

    PointerList     *pplBatchArgs;

    void            *pListMem;

    HANDLE          hExecutingProc;

    SEE_t           ShellExecuteEx;

    TCHAR           sCwd[CMD_BUF_REG];
    TCHAR           sPrompt[CMD_BUF_SMALL];
    EnvHash         ehHash;

};

extern TCHAR    gYes[CMD_BUF_COMMAND];

extern FILE     *gIO[3];
extern TCHAR    *gIOM[3][2];

extern BOOL     gRunForever;

#define STDIN   (pCmdState->fpIO[CMDIO_IN])
#define STDOUT  (pCmdState->fpIO[CMDIO_OUT])
#define STDERR  (pCmdState->fpIO[CMDIO_ERR])

#endif      /* __cmdHXX__ */



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 2000-2001 Microsoft Corporation

Module Name:
    cmdline.h

Abstract:
    This is a stub of the real cmdline.h located in test\inc
	created to make the separation of cmdline.lib from netcmn.lib
	seamless. It must, however, be updated with any changes to
	cmdline.h.

-------------------------------------------------------------------*/
#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

void SetOptionChars(int NumChars, TCHAR *CharArray);
int WasOption(int argc, LPTSTR argv[], LPCTSTR pszOption);
void StrictOptionsOnly(BOOL bStrict);
int GetOption(int argc, LPTSTR argv[], LPCTSTR pszOption, LPTSTR *ppszArgument);
BOOL IsStringInt( LPCTSTR szNum );
int GetOptionAsInt(int argc, LPTSTR argv[], LPCTSTR pszOption, int *piArgument);
int GetOptionAsDWORD(int argc, LPTSTR argv[], LPCTSTR pszOption, DWORD *pdwArgument);
void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);

#ifdef __cplusplus
}
#endif

#endif // __COMMAND_LINE_H__
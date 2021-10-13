#ifndef __UTIL_H__
#define __UTIL_H__

#include <windows.h>
#include <kato.h>
#include <main.h>

extern CKato* g_pKato;

void PrintCommStatus( DWORD );
void PrintCommTimeouts( LPCOMMTIMEOUTS );
void PrintCommProperties( LPCOMMPROP );
void ErrorMessage( DWORD );

#endif

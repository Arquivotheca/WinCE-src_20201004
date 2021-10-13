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
#ifndef _FONT_H
#define _FONT_H

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

#define MAXHEIGHT 100
#define MAXWIDTH 100
#define TEXT_COUNT 3

#define RANDOM_CHOICE (rand() % 2)
// I use S2FONTZONES not only to initialize the stress logger,
// but also in LogEx to display all the font parameters.
#define S2FONTZONES SLOG_FONT | SLOG_DRAW

struct SCBParams
{
    DWORD dwChosen;
    LOGFONT logfont;
    DWORD dwCurrent;
};

int CALLBACK EnumerateFace(ENUMLOGFONT *lpelf, const NEWTEXTMETRIC *lpntm,DWORD dwType, LPARAM lparam);
void LogFont(DWORD dwLevel, LOGFONT* plogfont);
BOOL OpenWindow(void);
HFONT GetRandomFont(HDC hdc, SCBParams* pParams);
UINT CallExtTextOut(HDC hdc, RECT* prc, SCBParams* pParams);
UINT CallDrawText(HDC hdc, RECT* prc, SCBParams* pParams);
DWORD TerminateStressModule(void);

#endif

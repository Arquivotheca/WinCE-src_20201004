//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#endif

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
#pragma once

#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define WAVE_FORMAT_MIDI 0x3000

// Deprecated messages, please do not use
#define MM_MOM_MIDIMESSAGE  (WM_USER+0x100)
#define MM_MOM_SETMIDITICKS (WM_USER+0x101)

#define MIDI_MESSAGE_UPDATETEMPO 0x10000000
#define MIDI_MESSAGE_FREQGENON   0x20000000
#define MIDI_MESSAGE_FREQGENOFF  0x30000000

typedef struct _WAVEFORMAT_MIDI
{
    WAVEFORMATEX wfx;
    UINT32 USecPerQuarterNote;
    UINT32 TicksPerQuarterNote;
} WAVEFORMAT_MIDI, *LPWAVEFORMAT_MIDI;
#define WAVEFORMAT_MIDI_EXTRASIZE (sizeof(WAVEFORMAT_MIDI)-sizeof(WAVEFORMATEX))

typedef struct _WAVEFORMAT_MIDI_MESSAGE
{
    UINT32 DeltaTicks;
    DWORD  MidiMsg;
} WAVEFORMAT_MIDI_MESSAGE;

#ifdef __cplusplus
}
#endif // __cplusplus


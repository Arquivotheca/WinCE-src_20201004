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
//------------------------------------------------------------------------------
// File: PerfStruct.h
//
// Desc: Structures for DirectShow performance logging.
//
//@@BEGIN_MSINTERNAL
//
//     1.0     15-NOV-1999     clorton     Created.
//
//@@END_MSINTERNAL
//------------------------------------------------------------------------------


#ifndef _PERFSTRUCT_H_
#define _PERFSTRUCT_H_

#ifndef UNDER_CE
#include <wmistr.h>
#include <evntrace.h>
#else
// taken from envtrace.h 
typedef struct _EVENT_TRACE_HEADER {        // overlays WNODE_HEADER
    USHORT          Size;                   // Size of entire record
    union {
        USHORT      FieldTypeFlags;         // Indicates valid fields
        struct {
            UCHAR   HeaderType;             // Header type - internal use only
            UCHAR   MarkerFlags;            // Marker - internal use only
        };
    };
    union {
        ULONG       Version;
        struct {
            UCHAR   Type;                   // event type
            UCHAR   Level;                  // trace instrumentation level
            USHORT  Version;                // version of trace record
        } Class;
    };
    ULONG           ThreadId;               // Thread Id
    ULONG           ProcessId;              // Process Id
    LARGE_INTEGER   TimeStamp;              // time when event happens
    union {
        GUID        Guid;                   // Guid that identifies event
        ULONGLONG   GuidPtr;                // use with WNODE_FLAG_USE_GUID_PTR
    };
    union {
        struct {
            ULONG   ClientContext;          // Reserved
            ULONG   Flags;                  // Flags for header
        };
        struct {
            ULONG   KernelTime;             // Kernel Mode CPU ticks
            ULONG   UserTime;               // User mode CPU ticks
        };
        ULONG64     ProcessorTime;          // Processor Clock
    };
} EVENT_TRACE_HEADER, *PEVENT_TRACE_HEADER;
#endif // !UNDER_CE

// {28CF047A-2437-4b24-B653-B9446A419A69}
DEFINE_GUID(GUID_DSHOW_CTL,
0x28cf047a, 0x2437, 0x4b24, 0xb6, 0x53, 0xb9, 0x44, 0x6a, 0x41, 0x9a, 0x69);

// {D0DA7AD6-AE80-4de5-AAFC-C126711E7593}
DEFINE_GUID(GUID_VIDEOREND,
0xd0da7ad6, 0xae80, 0x4de5, 0xaa, 0xfc, 0xc1, 0x26, 0x71, 0x1e, 0x75, 0x93);

// {DC70AC3E-93E5-48db-88AB-E42064EC276A}
DEFINE_GUID(GUID_DSOUNDGLITCH,
0xdc70ac3e, 0x93e5, 0x48db, 0x88, 0xab, 0xe4, 0x20, 0x64, 0xec, 0x27, 0x6a);

// {3d7e7d93-2fc8-4a07-a719-e0922ff2899}
DEFINE_GUID(GUID_STREAMTRACE,
0x3d7e7d93, 0x2fc8, 0x4a07, 0xa7, 0x19, 0xe0, 0x92, 0x2f, 0xf2, 0x89, 0x9e);

// AZFIX: the following GUIDs aren't useful right now.

// {3C33F7F5-EE54-493c-BA25-1656539C05AC}
DEFINE_GUID(GUID_GETTIME,
0x3c33f7f5, 0xee54, 0x493c, 0xba, 0x25, 0x16, 0x56, 0x53, 0x9c, 0x5, 0xac);

// {CC44B44D-8169-4952-9E4A-A4E13295E492}
DEFINE_GUID(GUID_AUDIOREND,
0xcc44b44d, 0x8169, 0x4952, 0x9e, 0x4a, 0xa4, 0xe1, 0x32, 0x95, 0xe4, 0x92);

// {775D19BF-4D8B-4de6-8DC9-66BAC7B310A2}
DEFINE_GUID(GUID_FRAMEDROP,
0x775d19bf, 0x4d8b, 0x4de6, 0x8d, 0xc9, 0x66, 0xba, 0xc7, 0xb3, 0x10, 0xa2);

// {56D29065-EFBE-42dc-8C29-E325DC9C27D5}
DEFINE_GUID(GUID_AUDIOBREAK,
0x56d29065, 0xefbe, 0x42dc, 0x8c, 0x29, 0xe3, 0x25, 0xdc, 0x9c, 0x27, 0xd5);

#define GLITCHTYPE_DSOUNDFIRSTGOOD 0
#define GLITCHTYPE_DSOUNDFIRSTBAD  1

typedef struct PERFINFO_DSHOW_AUDIOGLITCH {
    ULONGLONG   cycleCounter;
    DWORD       glitchType;
    LONGLONG   sampleTime;
    LONGLONG   previousTime;
    ULONG_PTR       instanceId;
} PERFINFO_DSHOW_AUDIOGLITCH, *PPERFINFO_DSHOW_AUDIOGLITCH;

typedef struct PERFINFO_WMI_AUDIOGLITCH {
    EVENT_TRACE_HEADER          header;
    PERFINFO_DSHOW_AUDIOGLITCH  data;
} PERFINFO_WMI_AUDIO_GLITCH, *PPERFINFO_WMI_AUDIOGLITCH;

typedef struct PERFINFO_DSHOW_GETTIME {
    ULONGLONG    cycleCounter;
    ULONGLONG    dshowClock;
} PERFINFO_DSHOW_GETTIME, *PPERFINFO_DSHOW_GETTIME;

typedef struct PERFINFO_WMI_GETTIME {
    EVENT_TRACE_HEADER        header;
    PERFINFO_DSHOW_GETTIME    data;
} PERFINFO_WMI_GETTIME, *PPERFINFO_WMI_GETTIME;

typedef struct PERFINFO_DSHOW_AVREND {
    ULONGLONG    cycleCounter;
    ULONGLONG    dshowClock;
    ULONGLONG    sampleTime;
} PERFINFO_DSHOW_AVREND, *PPERFINFO_DSHOW_AVREND;

typedef struct PERFINFO_WMI_AVREND {
    EVENT_TRACE_HEADER      header;
    PERFINFO_DSHOW_AVREND   data;
} PERFINFO_WMI_AVREND, *PPERFINFO_WMI_AVREND;

typedef struct PERFINFO_DSHOW_AUDIOBREAK {
    ULONGLONG    cycleCounter;
    ULONGLONG    dshowClock;
    ULONGLONG    sampleTime;
    ULONGLONG    sampleDuration;
} PERFINFO_DSHOW_AUDIOBREAK, *PPERFINFO_DSHOW_AUDIOBREAK;

typedef struct PERFINFO_WMI_AUDIOBREAK {
    EVENT_TRACE_HEADER          header;
    PERFINFO_DSHOW_AUDIOBREAK   data;
} PERFINFO_WMI_AUDIOBREAK, *PPERFINFO_WMI_AUDIOBREAK;

typedef struct PERFINFO_DSHOW_FRAMEDROP {
    ULONGLONG    cycleCounter;
    ULONGLONG    dshowClock;
    ULONGLONG    frameTime;
} PERFINFO_DSHOW_FRAMEDROP, *PPERFINFO_DSHOW_FRAMEDROP;

typedef struct PERFINFO_WMI_FRAMEDROP {
    EVENT_TRACE_HEADER          header;
    PERFINFO_DSHOW_FRAMEDROP    data;
} PERFINFO_WMI_FRAMEDROP, *PPERFINFO_WMI_FRAMEDROP;

#define PERFINFO_STREAMTRACE_MPEG2DEMUX_PTS_TRANSLATION     1
#define PERFINFO_STREAMTRACE_MPEG2DEMUX_SAMPLE_RECEIVED     2
#define PERFINFO_STREAMTRACE_VMR_BEGIN_ADVISE               3
#define PERFINFO_STREAMTRACE_VMR_END_ADVISE                 4
#define PERFINFO_STREAMTRACE_VMR_RECEIVE                    5
#define PERFINFO_STREAMTRACE_VMR_BEGIN_DEINTERLACE          6
#define PERFINFO_STREAMTRACE_VMR_END_DEINTERLACE            7
#define PERFINFO_STREAMTRACE_VMR_BEGIN_DECODE               8
#define PERFINFO_STREAMTRACE_VMR_END_DECODE                 9
#define PERFINFO_STREAMTRACE_VMR_DROPPED_FRAME              10
#define PERFINFO_STREAMTRACE_ENCDEC_DTFILTERINPUT           11
#define PERFINFO_STREAMTRACE_ENCDEC_DTFILTEROUTPUT          12
#define PERFINFO_STREAMTRACE_ENCDEC_ETFILTERINPUT           13
#define PERFINFO_STREAMTRACE_ENCDEC_ETFILTEROUTPUT          14
#define PERFINFO_STREAMTRACE_ENCDEC_XDSCODECINPUT           15
#define PERFINFO_STREAMTRACE_SBE_DVRANALYSISINPUT_RECEIVE   16
#define PERFINFO_STREAMTRACE_SBE_DVRANALYSISINPUT_DELIVER   17
#define PERFINFO_STREAMTRACE_SBE_DVRINPUTPIN_RECEIVE        18
#define PERFINFO_STREAMTRACE_SBE_DVROUTPUTPIN_RECEIVE       19
#define PERFINFO_STREAMTRACE_VMR_RENDER_TIME                20

typedef struct _PERFINFO_DSHOW_STREAMTRACE {
    ULONG        id;
    ULONG        reserved;
    ULONGLONG    dshowClock;
    ULONGLONG    data[ 4 ];
} PERFINFO_DSHOW_STREAMTRACE, *PPERFINFO_DSHOW_STREAMTRACE;

typedef struct _PERFINFO_WMI_STREAMTRACE {
    EVENT_TRACE_HEADER          header;
    PERFINFO_DSHOW_STREAMTRACE  data;
} PERFINFO_WMI_STREAMTRACE, *PPERFINFO_WMI_STREAMTRACE;

#endif // _PREFSTRUCT_H_

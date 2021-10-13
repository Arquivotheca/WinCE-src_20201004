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

#ifndef __NTEVENTLOG_H__
#define __NTEVENTLOG_H__
#include <windows.h>
#include <wmistr.h>
#include <Evntrace.h>
#include <evntprov.h>
#include "..\fszones.h"


//
// Start WPP Predfines.
// There are some predefined values which are created with every WPP compilation. 
// These values are copies for consistency.
//

#define WPP_REG_GLOBALLOGGER_FLAGS             L"Flags"
#define WPP_REG_GLOBALLOGGER_LEVEL             L"Level"
#define WPP_REG_GLOBALLOGGER_START             L"Start"

#define WPP_TEXTGUID_LEN  38
#define WPP_REG_GLOBALLOGGER_KEY            L"SYSTEM\\CurrentControlSet\\Control\\Wmi\\GlobalLogger"

#define WPP_GUID_FORMAT     "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"
#define WPP_GUID_ELEMENTS(p) \
    p->Data1,                 p->Data2,    p->Data3,\
    p->Data4[0], p->Data4[1], p->Data4[2], p->Data4[3],\
    p->Data4[4], p->Data4[5], p->Data4[6], p->Data4[7]

//
// End WPP Predfines.
//


/* e8908abc-aa84-11d2-9a93-00805f85d7c6 */
/*EXTERN_C 
const GUID  DECLSPEC_SELECTANY  GlobalLoggerGuid \
                    = { 0xe8908abc, 0xaa84, 0x11d2, { 0x9a, 0x93, 0x00, 0x80, 0x5f, 0x85, 0xd7, 0xc6 } }
*/


#define ETW_BUFFER_TYPE_GENERIC             0
#define ETW_BUFFER_TYPE_RUNDOWN             1
#define ETW_BUFFER_TYPE_CTX_SWAP            2
#define ETW_BUFFER_TYPE_REFTIME             3
#define ETW_BUFFER_TYPE_HEADER              4
#define ETW_BUFFER_TYPE_BATCHED             5
#define ETW_BUFFER_TYPE_MAXIMUM             6

//
// The highest order bit of a data block is set if trace, WNODE otherwise
//
#define TRACE_HEADER_FLAG                   0x80000000

// Header type for tracing messages
// | Marker(8) | Reserved(8)  | Size(16) | MessageNumber(16) | Flags(16)
#define TRACE_MESSAGE                       0x10000000

//
// The second bit is set if the trace is used by PM & CP (fixed headers)
// If not, the data block is used by for finer data for performance analysis
//
#define TRACE_HEADER_EVENT_TRACE            0x40000000
//
// If set, the data block is SYSTEM_TRACE_HEADER
//
#define TRACE_HEADER_ENUM_MASK              0x00FF0000


//
// The following are various header type
//
#define TRACE_HEADER_TYPE_SYSTEM32          1
#define TRACE_HEADER_TYPE_SYSTEM64          2
#define TRACE_HEADER_TYPE_FULL_HEADER32     10
#define TRACE_HEADER_TYPE_INSTANCE32        11
#define TRACE_HEADER_TYPE_TIMED             12  // Not used
#define TRACE_HEADER_TYPE_ERROR             13  // Error while logging event
#define TRACE_HEADER_TYPE_WNODE_HEADER      14  // Not used
#define TRACE_HEADER_TYPE_MESSAGE           15
#define TRACE_HEADER_TYPE_PERFINFO32        16
#define TRACE_HEADER_TYPE_PERFINFO64        17
#define TRACE_HEADER_TYPE_EVENT_HEADER32    18
#define TRACE_HEADER_TYPE_EVENT_HEADER64    19
#define TRACE_HEADER_TYPE_FULL_HEADER64     20
#define TRACE_HEADER_TYPE_INSTANCE64        21


#define TRACE_MESSAGE_VALID_USER_FLAGS  (TRACE_MESSAGE_SEQUENCE |\
                                         TRACE_MESSAGE_GUID |\
                                         TRACE_MESSAGE_COMPONENTID |\
                                         TRACE_MESSAGE_TIMESTAMP |\
                                         TRACE_MESSAGE_PERFORMANCE_TIMESTAMP |\
                                         TRACE_MESSAGE_SYSTEMINFO)



#define SYSTEM_TRACE_VERSION                 2

//
// The following two are used for defining LogFile layout version
//
#define TRACE_VERSION_MAJOR             1
#define TRACE_VERSION_MINOR             4

#define SYSTEM_TRACE_MARKER32     (TRACE_HEADER_FLAG | TRACE_HEADER_EVENT_TRACE \
                                    | (TRACE_HEADER_TYPE_SYSTEM32 << 16) | SYSTEM_TRACE_VERSION)

#define SYSTEM_TRACE_MARKER64     (TRACE_HEADER_FLAG | TRACE_HEADER_EVENT_TRACE \
                                    | (TRACE_HEADER_TYPE_SYSTEM64 << 16) | SYSTEM_TRACE_VERSION)


#ifdef _WIN64
#define PERFINFO_TRACE_MARKER     (TRACE_HEADER_FLAG | TRACE_HEADER_EVENT_TRACE \
                                    | (TRACE_HEADER_TYPE_PERFINFO64 << 16) | SYSTEM_TRACE_VERSION)

#define SYSTEM_TRACE_MARKER       SYSTEM_TRACE_MARKER64

#else

#define PERFINFO_TRACE_MARKER     (TRACE_HEADER_FLAG | TRACE_HEADER_EVENT_TRACE \
                                    | (TRACE_HEADER_TYPE_PERFINFO32 << 16) | SYSTEM_TRACE_VERSION)

#define SYSTEM_TRACE_MARKER       SYSTEM_TRACE_MARKER32
#endif


// Alignment macros
//
// DEFAULT_TRACE_ALIGNMENT is also known as WmiTraceAlignment and EtwTraceAlignment
//
#define DEFAULT_TRACE_ALIGNMENT 8              // 8 byte alignment
#define ALIGN_TO_POWER2( x, n ) (((ULONG)(x) + ((n)-1)) & ~((ULONG)(n)-1))



//
// The predefined event groups or families for NT subsystems
//

#define EVENT_TRACE_GROUP_HEADER               0x0000
#define EVENT_TRACE_GROUP_IO                   0x0100
#define EVENT_TRACE_GROUP_MEMORY               0x0200
#define EVENT_TRACE_GROUP_PROCESS              0x0300
#define EVENT_TRACE_GROUP_FILE                 0x0400
#define EVENT_TRACE_GROUP_THREAD               0x0500
#define EVENT_TRACE_GROUP_TCPIP                0x0600
#define EVENT_TRACE_GROUP_SPARE0               0x0700   // Spare0
#define EVENT_TRACE_GROUP_UDPIP                0x0800
#define EVENT_TRACE_GROUP_REGISTRY             0x0900
#define EVENT_TRACE_GROUP_DBGPRINT             0x0A00
#define EVENT_TRACE_GROUP_CONFIG               0x0B00
#define EVENT_TRACE_GROUP_SPARE1               0x0C00   // Spare1
#define EVENT_TRACE_GROUP_SPARE2               0x0D00   // Spare2
#define EVENT_TRACE_GROUP_POOL                 0x0E00
#define EVENT_TRACE_GROUP_PERFINFO             0x0F00
#define EVENT_TRACE_GROUP_HEAP                 0x1000
#define EVENT_TRACE_GROUP_OBJECT               0x1100
#define EVENT_TRACE_GROUP_POWER                0x1200
#define EVENT_TRACE_GROUP_MODBOUND             0x1300
#define EVENT_TRACE_GROUP_IMAGE                0x1400
#define EVENT_TRACE_GROUP_DPC                  0x1500
#define EVENT_TRACE_GROUP_SPARE3               0x1600   // Spare3
#define EVENT_TRACE_GROUP_CRITSEC              0x1700
#define EVENT_TRACE_GROUP_STACKWALK            0x1800
#define EVENT_TRACE_GROUP_SPARE4               0x1900   // Spare4
#define EVENT_TRACE_GROUP_ALPC                 0x1A00
#define EVENT_TRACE_GROUP_SPLITIO              0x1B00
#define EVENT_TRACE_GROUP_THREAD_POOL          0x1C00
#define EVENT_TRACE_GROUP_HYPERVISOR           0x1D00
#define EVENT_TRACE_GROUP_HYPERVISORX          0x1E00

typedef enum _ETW_BUFFER_STATE {
   EtwBufferStateFree,
   EtwBufferStateGeneralLogging,
   EtwBufferStateCSwitch,
   EtwBufferStateFlush,
   EtwBufferStateMaximum //MaxState should always be the last enum
} ETW_BUFFER_STATE, *PETW_BUFFER_STATE;


typedef struct _WMI_TRACE_PACKET {   // must be ULONG!!
    USHORT  Size;
    union{
        USHORT  HookId;
        struct {
            UCHAR   Type;
            UCHAR   Group;
        }GroupType;
    };
} WMI_TRACE_PACKET, *PWMI_TRACE_PACKET;

typedef struct _WMI_TRACE_MESSAGE_PACKET {  // must be ULONG!!
    USHORT  MessageNumber;                  // The message Number, index of messages by GUID
                                            // Or ComponentID
    USHORT  OptionFlags ;                   // Flags associated with the message
} WMI_TRACE_MESSAGE_PACKET, *PWMI_TRACE_MESSAGE_PACKET;



typedef enum tagWMI_HEADER_TYPE {
    WMIHT_NONE,
    WMIHT_UNKNOWN,
    WMIHT_SYSTEM32,
    WMIHT_SYSTEM64,
    WMIHT_EVENT_TRACE32,
    WMIHT_EVENT_INSTANCE32,
    WMIHT_TIMED,                // Not Used
    WMIHT_ERROR,                // Not Used
    WMIHT_WNODE,                // NOt Used
    WMIHT_MESSAGE,
    WMIHT_PERFINFO32,
    WMIHT_PERFINFO64,
    WMIHT_EVENT32,
    WMIHT_EVENT64,
    WMIHT_EVENT_TRACE64,
    WMIHT_EVENT_INSTANCE64,
    WMIHT_MAX
} WMI_HEADER_TYPE;

typedef struct _TIME_FIELDS {
    short Year;        // range [1601...]
    short Month;       // range [1..12]
    short Day;         // range [1..31]
    short Hour;        // range [0..23]
    short Minute;      // range [0..59]
    short Second;      // range [0..59]
    short Milliseconds;// range [0..999]
    short Weekday;     // range [0..6] == [Sunday..Saturday]
} TIME_FIELDS;
typedef TIME_FIELDS *PTIME_FIELDS;
typedef struct _RTL_TIME_ZONE_INFORMATION {
    LONG Bias;
    WCHAR StandardName[ 32 ];
    TIME_FIELDS StandardStart;
    LONG StandardBias;
    WCHAR DaylightName[ 32 ];
    TIME_FIELDS DaylightStart;
    LONG DaylightBias;
} RTL_TIME_ZONE_INFORMATION, *PRTL_TIME_ZONE_INFORMATION;



//
// 64-bit Trace header for kernel events
//
typedef struct _SYSTEM_TRACE_HEADER {
    union {
        ULONG       Marker;
        struct {
            USHORT  Version;
            UCHAR   HeaderType;
            UCHAR   Flags;
        }Version;
    };
    union {
        ULONG            Header;    // both sizes must be the same!
        WMI_TRACE_PACKET Packet;
    };
    ULONG           ThreadId;
    ULONG           ProcessId;
    LARGE_INTEGER   SystemTime;
    ULONG           KernelTime;
    ULONG           UserTime;
} SYSTEM_TRACE_HEADER, *PSYSTEM_TRACE_HEADER;



typedef struct _MESSAGE_TRACE_HEADER {
    union {
        ULONG       Marker;
        struct {
            USHORT  Size;                           // Total Size of the message including header
            UCHAR   Reserved;               // Unused and reserved
            UCHAR   Version;                // The message structure type (TRACE_MESSAGE_FLAG)
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    union {
        ULONG            Header;            // both sizes must be the same!
        WMI_TRACE_MESSAGE_PACKET Packet;
    } DUMMYUNIONNAME2;
} MESSAGE_TRACE_HEADER, *PMESSAGE_TRACE_HEADER;

//typedef struct _WMI_BUFFER_HEADER  *PWMI_BUFFER_HEADER;

typedef struct _WMI_BUFFER_HEADER {
    union {
        WNODE_HEADER                           Wnode;              // To make kd extension work
        struct {
            ULONG                              BufferSize;         // BufferSize
            ULONG                              SavedOffset;        // Temp saved offset
            volatile ULONG                     CurrentOffset;      // Current offset
            volatile LONG                      ReferenceCount;     // Reference count
            union {
                LARGE_INTEGER                  TimeStamp;          // Flush time stamp
                LARGE_INTEGER                  StartPerfClock;     // ReferenceTimeStamp
            } DUMMYUNIONNAME;

            LONGLONG                           SequenceNumber;     // Buffer sequence number

            union {
               ULONG                           Padding0[2];
               SINGLE_LIST_ENTRY               SlistEntry;         // Local list when flushing
               VOID *              NextBuffer;         // FlushList
            } DUMMYUNIONNAME2;

            ETW_BUFFER_CONTEXT                 ClientContext;
            union {
                ETW_BUFFER_STATE               State;              // (Free/GeneralLogging/Flush)
                ULONG                          Flags;              // um logger
            } DUMMYUNIONNAME3;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    ULONG                                      Offset;             // Offset when flushing (can overlap SavedOffset)
    USHORT                                     BufferFlag;         // (flush marker, events lost)
    USHORT                                     BufferType;         // (generic/rundown/cswitch/reftime)
    union {
        ULONG                                  Padding1[4];
        LARGE_INTEGER                          StartTime;          // ReferenceTimeStamp
        LIST_ENTRY                             Entry;              // um logger
        struct {
            PVOID                              Padding2;
            SINGLE_LIST_ENTRY                  GlobalEntry;        // Global list entry
        } DUMMYSTRUCTNAME;
        struct {
            PVOID                              Pointer0;
            PVOID                              Pointer1;
        } DUMMYSTRUCTNAME2;
    } DUMMYUNIONNAME2;
} WMI_BUFFER_HEADER, *PWMI_BUFFER_HEADER;


#ifndef ETW_BUFFER_HEADER
#define ETW_BUFFER_HEADER WMI_BUFFER_HEADER
#endif
#ifndef PETW_BUFFER_HEADER
#define PETW_BUFFER_HEADER PWMI_BUFFER_HEADER
#endif
//
// ETL logfile header
//

typedef struct _ETW_LOGFILE_HEADER {
    ETW_BUFFER_HEADER    BufferHeader;
    SYSTEM_TRACE_HEADER  SystemHeader;
    TRACE_LOGFILE_HEADER TraceLogfileHeader;
} ETW_LOGFILE_HEADER, *PETW_LOGFILE_HEADER;


#endif





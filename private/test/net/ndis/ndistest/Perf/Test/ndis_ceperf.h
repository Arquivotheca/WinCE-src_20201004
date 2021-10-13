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
#include <ceperf.h>

enum PERF_ITEMS {
    PERF_ITEM_NDIS_SEND_TIME = 0,
    PERF_ITEM_NDIS_SEND_BYTES,
    PERF_ITEM_NDIS_SEND_RATE,
    PERF_ITEM_NDIS_SEND_PPS,
    PERF_ITEM_NDIS_PACKETS_RECV,
    PERF_ITEM_NDIS_RECV_TIME,
    PERF_ITEM_NDIS_RECV_BYTES,
    PERF_ITEM_NDIS_RECV_RATE,
    PERF_ITEM_NDIS_RECV_PPS,
    PERF_ITEM_NDIS_PACKETS_LOST,
    PERF_ITEM_NDIS_MAX_SEND_THROUGHPUT_DAEMON
};

#define PERF_SESSION L"NDIS\\NDIS Perf"
#define DEFAULTSTATUSFLAGS (CEPERF_STATUS_RECORDING_ENABLED | CEPERF_STATUS_STORAGE_ENABLED)


typedef HRESULT (WINAPI* PFN_PERFSCEN_OPENSESSION)(LPCTSTR lpszSessionName, BOOL fStartRecordingNow);
typedef HRESULT (WINAPI* PFN_PERFSCEN_CLOSESESSION)(LPCTSTR lpszSessionName);
typedef HRESULT (WINAPI* PFN_PERFSCEN_ADDAUXDATA)(LPCTSTR lpszLabel, LPCTSTR lpszValue);
typedef HRESULT (WINAPI* PFN_PERFSCEN_FLUSHMETRICS)(BOOL fCloseAllSessions, GUID* scenarioGuid, LPCTSTR lpszScenarioNamespace, LPCTSTR lpszScenarioName, LPCTSTR lpszLogFileName, LPCTSTR lpszTTrackerFileName, GUID* instanceGuid);
   
// GUID for all user selected packet sizes (Default GUID)
// {BF60EFD7-A03B-4DF1-B87A-8B0748276B0F}
static GUID s_guidNdisPerfDefault = 
{ 0xbf60efd7, 0xa03b, 0x4df1, { 0xb8, 0x7a, 0x8b, 0x7, 0x48, 0x27, 0x6b, 0xf } };

//Unique GUID for each Packet size. i.e., for default packet sizes.
static GUID s_guidNdisPerfPacketSizes[] = {
       // GUID for Packet Size 64 bytes
    // {1D410101-6893-4ced-81DA-F8EC5147E1D9}
        { 0x1d410101, 0x6893, 0x4ced, { 0x81, 0xda, 0xf8, 0xec, 0x51, 0x47, 0xe1, 0xd9 } },
       // GUID for Packet Size 128 bytes
    // {4E958FB4-CEE6-4f4e-A117-E0CF0C191C6F}
        { 0x4e958fb4, 0xcee6, 0x4f4e, { 0xa1, 0x17, 0xe0, 0xcf, 0xc, 0x19, 0x1c, 0x6f } },
       // GUID for Packet Size 192 bytes
    // {9F836A0D-A7EC-4702-B62C-68D6D992D471}
        { 0x9f836a0d, 0xa7ec, 0x4702, { 0xb6, 0x2c, 0x68, 0xd6, 0xd9, 0x92, 0xd4, 0x71 } },
       // GUID for Packet Size 256 bytes
    // {583CD621-AFF2-451f-BBE2-1E60DBE7C79F}
        { 0x583cd621, 0xaff2, 0x451f, { 0xbb, 0xe2, 0x1e, 0x60, 0xdb, 0xe7, 0xc7, 0x9f } },
       // GUID for Packet Size 320 bytes
    // {1A089223-A245-4a46-AA5F-44704651BB2C}
        { 0x1a089223, 0xa245, 0x4a46, { 0xaa, 0x5f, 0x44, 0x70, 0x46, 0x51, 0xbb, 0x2c } },
       // GUID for Packet Size 384 bytes
    // {8DE05130-676B-4cde-909F-048998F72487}
        { 0x8de05130, 0x676b, 0x4cde, { 0x90, 0x9f, 0x4, 0x89, 0x98, 0xf7, 0x24, 0x87 } },
       // GUID for Packet Size 448 bytes
    // {D721D001-A70F-4dd5-AE7C-26297DCC5FF0}
        { 0xd721d001, 0xa70f, 0x4dd5, { 0xae, 0x7c, 0x26, 0x29, 0x7d, 0xcc, 0x5f, 0xf0 } },
       // GUID for Packet Size 512 bytes
    // {63C07C3D-F9F3-4969-9CE3-A797379362CC}
        { 0x63c07c3d, 0xf9f3, 0x4969, { 0x9c, 0xe3, 0xa7, 0x97, 0x37, 0x93, 0x62, 0xcc } },
       // GUID for Packet Size 576 bytes
    // {3DEACEFC-5F97-4b28-8995-449BB43BA7ED}
        { 0x3deacefc, 0x5f97, 0x4b28, { 0x89, 0x95, 0x44, 0x9b, 0xb4, 0x3b, 0xa7, 0xed } },
       // GUID for Packet Size 640 bytes
    // {F6215D68-41F6-46d7-B4FF-2FE0E017D7FF}
        { 0xf6215d68, 0x41f6, 0x46d7, { 0xb4, 0xff, 0x2f, 0xe0, 0xe0, 0x17, 0xd7, 0xff } },
       // GUID for Packet Size 704 bytes
    // {35D2BF3F-6D12-40a9-9FDD-C513BFB686EB}
        { 0x35d2bf3f, 0x6d12, 0x40a9, { 0x9f, 0xdd, 0xc5, 0x13, 0xbf, 0xb6, 0x86, 0xeb } },
       // GUID for Packet Size 768 bytes
    // {401104AE-BD83-40a4-9133-9A85CE4D57EF}
        { 0x401104ae, 0xbd83, 0x40a4, { 0x91, 0x33, 0x9a, 0x85, 0xce, 0x4d, 0x57, 0xef } },
       // GUID for Packet Size 832 bytes
    // {C89958CD-7169-4add-97C4-32EE0B9434C5}
        { 0xc89958cd, 0x7169, 0x4add, { 0x97, 0xc4, 0x32, 0xee, 0xb, 0x94, 0x34, 0xc5 } },
       // GUID for Packet Size 896 bytes
    // {F1B7F764-B520-400f-9F3A-CCD067D3DDE7}
        { 0xf1b7f764, 0xb520, 0x400f, { 0x9f, 0x3a, 0xcc, 0xd0, 0x67, 0xd3, 0xdd, 0xe7 } },
       // GUID for Packet Size 960 bytes
    // {919C2478-D03A-4caf-8DF1-AE3AA4C0F67D}
        { 0x919c2478, 0xd03a, 0x4caf, { 0x8d, 0xf1, 0xae, 0x3a, 0xa4, 0xc0, 0xf6, 0x7d } },
       // GUID for Packet Size 1024 bytes
    // {837FA641-3949-4000-B073-733357252E5F}
        { 0x837fa641, 0x3949, 0x4000, { 0xb0, 0x73, 0x73, 0x33, 0x57, 0x25, 0x2e, 0x5f } },
       // GUID for Packet Size 1088 bytes
    // {6401DBD8-469A-455d-B07D-5C0A75CC2A31}
        { 0x6401dbd8, 0x469a, 0x455d, { 0xb0, 0x7d, 0x5c, 0xa, 0x75, 0xcc, 0x2a, 0x31 } },
       // GUID for Packet Size 1152 bytes
    // {B0DF9409-2A98-4843-8E24-2D0595122769}
        { 0xb0df9409, 0x2a98, 0x4843, { 0x8e, 0x24, 0x2d, 0x5, 0x95, 0x12, 0x27, 0x69 } },
       // GUID for Packet Size 1216 bytes
    // {0FC5F98D-9B89-45dc-B226-E8C0A1D8D216}
        { 0xfc5f98d, 0x9b89, 0x45dc, { 0xb2, 0x26, 0xe8, 0xc0, 0xa1, 0xd8, 0xd2, 0x16 } },
       // GUID for Packet Size 1280 bytes
    // {2DD6EB97-36B5-4284-8ACC-64082E6B6727}
        { 0x2dd6eb97, 0x36b5, 0x4284, { 0x8a, 0xcc, 0x64, 0x8, 0x2e, 0x6b, 0x67, 0x27 } },
       // GUID for Packet Size 1344 bytes
    // {EAF8A986-449D-45d5-A558-8DADB7ED41C4}
        { 0xeaf8a986, 0x449d, 0x45d5, { 0xa5, 0x58, 0x8d, 0xad, 0xb7, 0xed, 0x41, 0xc4 } },
       // GUID for Packet Size 1408 bytes
    // {F51D44DD-EE23-4ffa-89C2-A3E868FE2827}
        { 0xf51d44dd, 0xee23, 0x4ffa, { 0x89, 0xc2, 0xa3, 0xe8, 0x68, 0xfe, 0x28, 0x27 } },
       // GUID for Packet Size 1472 bytes
    // {5C56EC34-7221-498e-9E01-9181C0D81A43}
        { 0x5c56ec34, 0x7221, 0x498e, { 0x9e, 0x1, 0x91, 0x81, 0xc0, 0xd8, 0x1a, 0x43 } },
       // GUID for Packet Size 1514 bytes
    // {FBEC2EC2-7245-48c0-8567-DE9A9708ED30}
        { 0xfbec2ec2, 0x7245, 0x48c0, { 0x85, 0x67, 0xde, 0x9a, 0x97, 0x8, 0xed, 0x30 } },
};

static GUID GetGUIDforPacketSize(DWORD dwPacketSize);
static void PerfScenarioEnd();

static CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems[] = {
        {
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Send Time (ms)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Send Bytes (MB)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Send Rate (Mbps)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Send PPS (pps)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Packets Recv",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Recv Time (ms)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Recv Bytes (MB)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Recv Rate (Mbps)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Recv PPS (pps)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {   
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Packets Lost (%)",
            CEPERF_STATISTIC_RECORD_MIN
        },
        {
            INVALID_HANDLE_VALUE,
            CEPERF_TYPE_STATISTIC,
            L"Max Send Thrput daemon (bps)",
            CEPERF_STATISTIC_RECORD_MIN
        }
};

#define NUM_PERF_ITEMS (sizeof(g_rgPerfItems) / sizeof(CEPERF_BASIC_ITEM_DESCRIPTOR))

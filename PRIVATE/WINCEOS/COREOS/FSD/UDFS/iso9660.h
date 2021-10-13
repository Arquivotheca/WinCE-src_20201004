//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//
//
//  File:       iso9660.h
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------


#pragma pack(1)
typedef struct tagISO9660DirectoryRecord
{
    BYTE    RecordLength;
    BYTE    Fill;
    DWORD   ExtentLocation;
    DWORD   Fill1;
    DWORD   ExtentSize;
    DWORD   Fill2;
    BYTE    Time[7];
    BYTE    Flags;
    BYTE    Fill3[6];
    BYTE    StrLen;
    BYTE    FileName[1];

} ISO9660_DIRECTORY_RECORD, *PISO9660_DIRECTORY_RECORD;


typedef struct tagISO9660VolumeDescriptor
{
    BYTE    Type;
    BYTE    ID[5];
    BYTE    Fill1[82];
    CHAR    EscapeSequence[32];
    BYTE    Fill3[36];

    ISO9660_DIRECTORY_RECORD RootDirectory;

} ISO9660_VOLUME_DESCRIPTOR, *PISO9660_VOLUME_DESCRIPTOR;


typedef struct tagISO9660PathTableRecord
{
    BYTE    RecordLength;
    BYTE    EALength;
    DWORD   Location;
    WORD    Parent;
    WCHAR   DirId[1];

} ISO9660_PATH_TABLE_RECORD, *PISO9660_PATH_TABLE_RECORD;
#pragma pack()


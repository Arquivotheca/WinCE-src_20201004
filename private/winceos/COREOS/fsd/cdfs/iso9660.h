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
    BYTE    EALength;
    DWORD   ExtentLocation;
    DWORD   Fill1;
    DWORD   ExtentSize;
    DWORD   Fill2;
    BYTE    Time[7];
    BYTE    Flags;
    BYTE    UnitSize;
    BYTE    GapSize;
    BYTE    Fill3[4];
    BYTE    StrLen;
    BYTE    FileName[1];

} ISO9660_DIRECTORY_RECORD, *PISO9660_DIRECTORY_RECORD;


typedef struct tagISO9660VolumeDescriptor
{
    BYTE    Type;                  // 1 - PVD, 2 - SVD
    BYTE    ID[5];                 // CD001
    BYTE    Fill1[82];
    CHAR    EscapeSequence[32];    // Only used for SVD
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

//
//  System use are for XA data.  The following is the system use area for
//  directory entries on XA data disks.
//
typedef struct _XASystemUse
{
    //
    //  Owner ID.  Not used in this version.
    //

    UCHAR OwnerId[4];

    //
    //  Extent attributes.  Only interested if mode2 form2 or digital audio.
    //  This is stored big endian.  We will define the attribute flags so
    //  we can ignore this fact.
    //

    USHORT Attributes;

    //
    //  XA signature.  This value must be 'XA'.
    //

    USHORT Signature;

    //
    //  File Number.
    //

    UCHAR FileNumber;

    //
    //  Not used in this version.
    //

    UCHAR Reserved[5];

} XA_SYSTEM_USE, *PXA_SYSTEM_USE;
#pragma pack()

#define SYSTEM_USE_XA_FORM1             (0x0008)
#define SYSTEM_USE_XA_FORM2             (0x0010)
#define SYSTEM_USE_XA_DA                (0x0040)

#define SYSTEM_XA_SIGNATURE             (0x4158)


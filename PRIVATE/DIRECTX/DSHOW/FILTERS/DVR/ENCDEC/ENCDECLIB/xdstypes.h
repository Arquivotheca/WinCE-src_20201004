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
#ifndef __EncDec__XDSTypes_h
#define __EncDec__XDSTypes_h


enum XDS_Classes 
{
    XDS_Cls_Invalid                 = 0x0,
    XDS_Cls_Current_Start           = 0x1,
    XDS_Cls_Current_Continue        = 0x2,
    XDS_Cls_Future_Start            = 0x3,
    XDS_Cls_Future_Continue         = 0x4,
    XDS_Cls_Channel_Start           = 0x5,
    XDS_Cls_Channel_Continue        = 0x6,
    XDS_Cls_Miscellaneous_Start     = 0x7,
    XDS_Cls_Miscellaneous_Continue  = 0x8,
    XDS_Cls_PublicService_Start     = 0x9,
    XDS_Cls_PublicService_Continue  = 0xA,
    XDS_Cls_Reserved_Start          = 0xB,
    XDS_Cls_Reserved_Continue       = 0xC,
    XDS_Cls_PrivateData_Start       = 0xD,      
    XDS_Cls_PrivateData_Continue    = 0xE,
    XDS_Cls_End                     = 0xF
};

const DWORD XDS_Type_Invalid            = 0x0;

    // types as ints, since different classes use the same flags
const DWORD XDS_1Type_PID               = 0x1;      // program identification number
const DWORD XDS_1Type_Length            = 0x2;      // length, time in show
const DWORD XDS_1Type_ProgramName       = 0x3;      // title
const DWORD XDS_1Type_ProgramType       = 0x4;      // program type
const DWORD XDS_1Type_ContentAdvisory   = 0x5;      // Content Advisory
const DWORD XDS_1Type_AudioServices     = 0x6;
const DWORD XDS_1Type_CaptionServices   = 0x7;
const DWORD XDS_1Type_CGMS_A            = 0x8;      // Copy Generation Management Services
const DWORD XDS_1Type_AspectRatio       = 0x9;      // Aspect Ration Information
const DWORD XDS_1Type_CompositePacket1  = 0xC;
const DWORD XDS_1Type_CompositePacket2  = 0xD;
const DWORD XDS_1Type_ProgDescription1  = 0x10;
const DWORD XDS_1Type_ProgDescription2  = 0x11;
const DWORD XDS_1Type_ProgDescription3  = 0x12;
const DWORD XDS_1Type_ProgDescription4  = 0x13;
const DWORD XDS_1Type_ProgDescription5  = 0x14;
const DWORD XDS_1Type_ProgDescription6  = 0x15;
const DWORD XDS_1Type_ProgDescription7  = 0x16;
const DWORD XDS_1Type_ProgDescription8  = 0x17;

#endif // __EncDec__XDSTypes_h

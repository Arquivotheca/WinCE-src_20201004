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
//
// define the product identifiers and tags used to identify
// which MS tool built any particular object file
//
#pragma once
#if !defined(_prodids_h)
#define _prodids_h

// define the product ids, encodes version + language

enum PRODID {
    prodidUnknown             = 0x0000,
    prodidImport0             = 0x0001,   // Import object (Version 0)
    prodidLinker510           = 0x0002,   // LINK 5.10
    prodidCvtomf510           = 0x0003,   // LINK 5.10 (CVTOMF)
    prodidLinker600           = 0x0004,   // LINK 6.00
    prodidCvtomf600           = 0x0005,   // LINK 6.00 (CVTOMF)
    prodidCvtres500           = 0x0006,   // CVTRES 5.00
    prodidUtc11_Basic         = 0x0007,   // Visual Basic (UTC 11 native)
    prodidUtc11_C             = 0x0008,   // Visual C++ 5.0 C/C++
    prodidUtc12_Basic         = 0x0009,   // Visual Basic (UTC 12 native)
    prodidUtc12_C             = 0x000a,   // Visual C++ 6.0 C
    prodidUtc12_CPP           = 0x000b,   // Visual C++ 6.0 C++
    prodidAliasObj60          = 0x000c,   // Visual C++ 6.0 (ALIASOBJ.EXE)
    prodidVisualBasic60       = 0x000d,   // Visual Basic 6.0
    prodidMasm613             = 0x000e,   // MASM 6.13
    prodidMasm710             = 0x000f,   // MASM 7.10
    prodidLinker511           = 0x0010,   // LINK 5.11
    prodidCvtomf511           = 0x0011,   // LINK 5.11 (CVTOMF)
    prodidMasm614             = 0x0012,   // MASM 6.14
    prodidLinker512           = 0x0013,   // LINK 5.12
    prodidCvtomf512           = 0x0014,   // LINK 5.12 (CVTOMF)
    prodidUtc12_C_Std         = 0x0015,   // Visual C++ 6.0 C (Standard Edition)
    prodidUtc12_CPP_Std       = 0x0016,   // Visual C++ 6.0 C++ (Standard Edition)
    prodidUtc12_C_Book        = 0x0017,   // Visual C++ 6.0 C (Book Edition)
    prodidUtc12_CPP_Book      = 0x0018,   // Visual C++ 6.0 C++ (Book Edition)
    prodidImplib700           = 0x0019,   // LINK 7.00 (Import library)
    prodidCvtomf700           = 0x001a,   // LINK 7.00 (CVTOMF)
    prodidUtc13_Basic         = 0x001b,   // Visual Basic (UTC 13 native)
    prodidUtc13_C             = 0x001c,   // Visual C++ 7.0 C
    prodidUtc13_CPP           = 0x001d,   // Visual C++ 7.0 C++
    prodidLinker610           = 0x001e,   // LINK 6.10
    prodidCvtomf610           = 0x001f,   // LINK 6.10 (CVTOMF)
    prodidLinker601           = 0x0020,   // LINK 6.01
    prodidCvtomf601           = 0x0021,   // LINK 6.01 (CVTOMF)
    prodidUtc12_1_Basic       = 0x0022,   // Visual Basic (UTC 12.10 native)
    prodidUtc12_1_C           = 0x0023,   // Visual C++ 6.1 C
    prodidUtc12_1_CPP         = 0x0024,   // Visual C++ 6.1 C++
    prodidLinker620           = 0x0025,   // LINK 6.20
    prodidCvtomf620           = 0x0026,   // LINK 6.20 (CVTOMF)
    prodidAliasObj70          = 0x0027,   // Visual C++ 7.0 (ALIASOBJ.EXE)
    prodidLinker621           = 0x0028,   // LINK 6.21
    prodidCvtomf621           = 0x0029,   // LINK 6.21 (CVTOMF)
    prodidMasm615             = 0x002a,   // MASM 6.15
    prodidUtc13_LTCG_C        = 0x002b,   // Visual C++ 7.0 C (LTCG)
    prodidUtc13_LTCG_CPP      = 0x002c,   // Visual C++ 7.0 C++ (LTCG)
    prodidMasm620             = 0x002d,   // MASM 6.20
    prodidILAsm100            = 0x002e,   // IL Assembler 1.00
    prodidUtc12_2_Basic       = 0x002f,   // VB 6.0 native code w/Processor Pack
    prodidUtc12_2_C           = 0x0030,   // VC++ 6.0 Processor Pack C
    prodidUtc12_2_CPP         = 0x0031,   // VC++ 6.0 Processor Pack C++
    prodidUtc12_2_C_Std       = 0x0032,   // VC++ 6.0 Processor Pack C standard edition
    prodidUtc12_2_CPP_Std     = 0x0033,   // VC++ 6.0 Processor Pack C++ standard edition
    prodidUtc12_2_C_Book      = 0x0034,   // VC++ 6.0 Processor Pack C book edition
    prodidUtc12_2_CPP_Book    = 0x0035,   // VC++ 6.0 Processor Pack C++ book edition
    prodidImplib622           = 0x0036,   // LINK 6.22 (Import library)
    prodidCvtomf622           = 0x0037,   // LINK 6.22 (CVTOMF)
    prodidCvtres501           = 0x0038,   // CVTRES 5.01
    prodidUtc13_C_Std         = 0x0039,   // Visual C++ 7.0 C (Standard Edition)
    prodidUtc13_CPP_Std       = 0x003a,   // Visual C++ 7.0 C++ (Standard Edition)
    prodidCvtpgd1300          = 0x003b,   // CVTPGD 13.00
    prodidLinker622           = 0x003c,   // LINK 6.22
    prodidLinker700           = 0x003d,   // LINK 7.00
    prodidExport622           = 0x003e,   // LINK 6.22 (EXP file)
    prodidExport700           = 0x003f,   // LINK 7.00 (EXP file)
    prodidMasm700             = 0x0040,   // MASM 7.00
    prodidUtc13_POGO_I_C      = 0x0041,   // Visual C++ 7.0 C (LTCG) (POGO instrumentation)
    prodidUtc13_POGO_I_CPP    = 0x0042,   // Visual C++ 7.0 C++ (LTCG) (POGO instrumentation)
    prodidUtc13_POGO_O_C      = 0x0043,   // Visual C++ 7.0 C (LTCG) (POGO optimization)
    prodidUtc13_POGO_O_CPP    = 0x0044,   // Visual C++ 7.0 C++ (LTCG) (POGO optimization)
    prodidCvtres700           = 0x0045,   // CVTRES 7.00
    prodidCvtres710p          = 0x0046,   // CVTRES 7.10 (Prerelease)
    prodidLinker710p          = 0x0047,   // LINK 7.10 (Prerelease)
    prodidCvtomf710p          = 0x0048,   // LINK 7.10 (Prerelease) (CVTOMF)
    prodidExport710p          = 0x0049,   // LINK 7.10 (Prerelease) (EXP file)
    prodidImplib710p          = 0x004a,   // LINK 7.10 (Prerelease) (Import library)
    prodidMasm710p            = 0x004b,   // MASM 7.10 (Prerelease)
    prodidUtc1310p_C          = 0x004c,   // Visual C++ 7.10 (Prerelease) C
    prodidUtc1310p_CPP        = 0x004d,   // Visual C++ 7.10 (Prerelease) C++
    prodidUtc1310p_C_Std      = 0x004e,   // Visual C++ 7.10 (Prerelease) C (Standard Edition)
    prodidUtc1310p_CPP_Std    = 0x004f,   // Visual C++ 7.10 (Prerelease) C++ (Standard Edition)
    prodidUtc1310p_LTCG_C     = 0x0050,   // Visual C++ 7.10 (Prerelease) C (LTCG)
    prodidUtc1310p_LTCG_CPP   = 0x0051,   // Visual C++ 7.10 (Prerelease) C++ (LTCG)
    prodidUtc1310p_POGO_I_C   = 0x0052,   // Visual C++ 7.10 (Prerelease) C (LTCG) (POGO instrumentation)
    prodidUtc1310p_POGO_I_CPP = 0x0053,   // Visual C++ 7.10 (Prerelease) C++ (LTCG) (POGO instrumentation)
    prodidUtc1310p_POGO_O_C   = 0x0054,   // Visual C++ 7.10 (Prerelease) C (LTCG) (POGO optimization)
    prodidUtc1310p_POGO_O_CPP = 0x0055,   // Visual C++ 7.10 (Prerelease) C++ (LTCG) (POGO optimization)
    prodidLinker624           = 0x0056,   // LINK 6.24
    prodidCvtomf624           = 0x0057,   // LINK 6.24 (CVTOMF)
    prodidExport624           = 0x0058,   // LINK 6.24 (EXP file)
    prodidImplib624           = 0x0059,   // LINK 6.24 (Import library)
    prodidLinker710           = 0x005a,   // LINK 7.10
    prodidCvtomf710           = 0x005b,   // LINK 7.10 (CVTOMF)
    prodidExport710           = 0x005c,   // LINK 7.10 (EXP file)
    prodidImplib710           = 0x005d,   // LINK 7.10 (Import library)
    prodidCvtres710           = 0x005e,   // CVTRES 7.10
    prodidUtc1310_C           = 0x005f,   // Visual C++ 7.10 C
    prodidUtc1310_CPP         = 0x0060,   // Visual C++ 7.10 C++
    prodidUtc1310_C_Std       = 0x0061,   // Visual C++ 7.10 C (Standard Edition)
    prodidUtc1310_CPP_Std     = 0x0062,   // Visual C++ 7.10 C++ (Standard Edition)
    prodidUtc1310_LTCG_C      = 0x0063,   // Visual C++ 7.10 C (LTCG)
    prodidUtc1310_LTCG_CPP    = 0x0064,   // Visual C++ 7.10 C++ (LTCG)
    prodidUtc1310_POGO_I_C    = 0x0065,   // Visual C++ 7.10 C (LTCG) (POGO instrumentation)
    prodidUtc1310_POGO_I_CPP  = 0x0066,   // Visual C++ 7.10 C++ (LTCG) (POGO instrumentation)
    prodidUtc1310_POGO_O_C    = 0x0067,   // Visual C++ 7.10 C (LTCG) (POGO optimization)
    prodidUtc1310_POGO_O_CPP  = 0x0068,   // Visual C++ 7.10 C++ (LTCG) (POGO optimization)
    prodidAliasObj710         = 0x0069,   // Visual C++ 7.10 (ALIASOBJ.EXE)
    prodidAliasObj710p        = 0x006a,   // Visual C++ 7.10 (Prerelease) (ALIASOBJ.EXE)
    prodidCvtpgd1310          = 0x006b,   // CVTPGD 13.10
    prodidCvtpgd1310p         = 0x006c,   // CVTPGD 13.10 (Prerelease)
    prodidUtc1400_C           = 0x006d,   // Visual C++ 8.00 C
    prodidUtc1400_CPP         = 0x006e,   // Visual C++ 8.00 C++
    prodidUtc1400_C_Std       = 0x006f,   // Visual C++ 8.00 C (Standard Edition)
    prodidUtc1400_CPP_Std     = 0x0070,   // Visual C++ 8.00 C++ (Standard Edition)
    prodidUtc1400_LTCG_C      = 0x0071,   // Visual C++ 8.00 C (LTCG)
    prodidUtc1400_LTCG_CPP    = 0x0072,   // Visual C++ 8.00 C++ (LTCG)
    prodidUtc1400_POGO_I_C    = 0x0073,   // Visual C++ 8.00 C (LTCG) (POGO instrumentation)
    prodidUtc1400_POGO_I_CPP  = 0x0074,   // Visual C++ 8.00 C++ (LTCG) (POGO instrumentation)
    prodidUtc1400_POGO_O_C    = 0x0075,   // Visual C++ 8.00 C (LTCG) (POGO optimization)
    prodidUtc1400_POGO_O_CPP  = 0x0076,   // Visual C++ 8.00 C++ (LTCG) (POGO optimization)
    prodidCvtpgd1400          = 0x0077,   // CVTPGD 14.00
    prodidLinker800           = 0x0078,   // LINK 8.00
    prodidCvtomf800           = 0x0079,   // LINK 8.00 (CVTOMF)
    prodidExport800           = 0x007a,   // LINK 8.00 (EXP file)
    prodidImplib800           = 0x007b,   // LINK 8.00 (Import library)
    prodidCvtres800           = 0x007c,   // CVTRES 8.00
    prodidMasm800             = 0x007d,   // MASM 8.00
    prodidAliasObj800         = 0x007e,   // Visual C++ 8.00 (ALIASOBJ.EXE)
    prodidPhoenixPrerelease   = 0x007f,   // Pre-release Phoenix compiled
    prodidUtc1400_CVTCIL_C    = 0x0080,   // Visual C++ 8.00 C (CVTCIL)
    prodidUtc1400_CVTCIL_CPP  = 0x0081,   // Visual C++ 8.00 C++ (CVTCIL)
    prodidUtc1400_LTCG_MSIL   = 0x0082,   // Visual C++ 8.00 MSIL NetModule (LTCG)
    prodidUtc1500_C           = 0x0083,   // Visual C++ 9.00 C
    prodidUtc1500_CPP         = 0x0084,   // Visual C++ 9.00 C++
    prodidUtc1500_C_Std       = 0x0085,   // Visual C++ 9.00 C (Standard Edition)
    prodidUtc1500_CPP_Std     = 0x0086,   // Visual C++ 9.00 C++ (Standard Edition)
    prodidUtc1500_CVTCIL_C    = 0x0087,   // Visual C++ 9.00 C (CVTCIL)
    prodidUtc1500_CVTCIL_CPP  = 0x0088,   // Visual C++ 9.00 C++ (CVTCIL)
    prodidUtc1500_LTCG_C      = 0x0089,   // Visual C++ 9.00 C (LTCG)
    prodidUtc1500_LTCG_CPP    = 0x008a,   // Visual C++ 9.00 C++ (LTCG)
    prodidUtc1500_LTCG_MSIL   = 0x008b,   // Visual C++ 9.00 MSIL NetModule (LTCG)
    prodidUtc1500_POGO_I_C    = 0x008c,   // Visual C++ 9.00 C (LTCG) (POGO instrumentation)
    prodidUtc1500_POGO_I_CPP  = 0x008d,   // Visual C++ 9.00 C++ (LTCG) (POGO instrumentation)
    prodidUtc1500_POGO_O_C    = 0x008e,   // Visual C++ 9.00 C (LTCG) (POGO optimization)
    prodidUtc1500_POGO_O_CPP  = 0x008f,   // Visual C++ 9.00 C++ (LTCG) (POGO optimization)
    prodidCvtpgd1500          = 0x0090,   // CVTPGD 15.00
    prodidLinker900           = 0x0091,   // LINK 9.00
    prodidExport900           = 0x0092,   // LINK 9.00 (EXP file)
    prodidImplib900           = 0x0093,   // LINK 9.00 (Import library)
    prodidCvtres900           = 0x0094,   // CVTRES 9.00
    prodidMasm900             = 0x0095,   // MASM 9.00
    prodidAliasObj900         = 0x0096,   // Visual C++ 9.00 (ALIASOBJ.EXE)
    prodidResource            = 0x0097,   // RES file (not COFF output of CVTRES)
    prodidAliasObj1000        = 0x0098,   // Visual C++ 10.00 (ALIASOBJ.EXE)
    prodidCvtpgd1600          = 0x0099,   // CVTPGD 16.00
    prodidCvtres1000          = 0x009A,   // CVTRES 10.00
    prodidExport1000          = 0x009B,   // LINK 10.00 (EXP file)
    prodidImplib1000          = 0x009C,   // LINK 10.00 (Import library)
    prodidLinker1000          = 0x009D,   // LINK 10.00
    prodidMasm1000            = 0x009E,   // MASM 10.00
    prodidPhx1600_C           = 0x009F,   // Visual C++ 10.00 (Phoenix) C
    prodidPhx1600_CPP         = 0x00A0,   // Visual C++ 10.00 (Phoenix) C++
    prodidPhx1600_CVTCIL_C    = 0x00A1,   // Visual C++ 10.00 (Phoenix) C (CVTCIL)
    prodidPhx1600_CVTCIL_CPP  = 0x00A2,   // Visual C++ 10.00 (Phoenix) C++ (CVTCIL)
    prodidPhx1600_LTCG_C      = 0x00A3,   // Visual C++ 10.00 (Phoenix) C (LTCG)
    prodidPhx1600_LTCG_CPP    = 0x00A4,   // Visual C++ 10.00 (Phoenix) C++ (LTCG)
    prodidPhx1600_LTCG_MSIL   = 0x00A5,   // Visual C++ 10.00 (Phoenix) MSIL NetModule (LTCG)
    prodidPhx1600_POGO_I_C    = 0x00A6,   // Visual C++ 10.00 (Phoenix) C (LTCG) (POGO instrumentation)
    prodidPhx1600_POGO_I_CPP  = 0x00A7,   // Visual C++ 10.00 (Phoenix) C++ (LTCG) (POGO instrumentation)
    prodidPhx1600_POGO_O_C    = 0x00A8,   // Visual C++ 10.00 (Phoenix) C (LTCG) (POGO optimization)
    prodidPhx1600_POGO_O_CPP  = 0x00A9,   // Visual C++ 10.00 (Phoenix) C++ (LTCG) (POGO optimization)
    prodidUtc1600_C           = 0x00AA,   // Visual C++ 10.00 (UTC) C
    prodidUtc1600_CPP         = 0x00AB,   // Visual C++ 10.00 (UTC) C++
    prodidUtc1600_CVTCIL_C    = 0x00AC,   // Visual C++ 10.00 (UTC) C (CVTCIL)
    prodidUtc1600_CVTCIL_CPP  = 0x00AD,   // Visual C++ 10.00 (UTC) C++ (CVTCIL)
    prodidUtc1600_LTCG_C      = 0x00AE,   // Visual C++ 10.00 (UTC) C (LTCG)
    prodidUtc1600_LTCG_CPP    = 0x00AF,   // Visual C++ 10.00 (UTC) C++ (LTCG)
    prodidUtc1600_LTCG_MSIL   = 0x00B0,   // Visual C++ 10.00 (UTC) MSIL NetModule (LTCG)
    prodidUtc1600_POGO_I_C    = 0x00B1,   // Visual C++ 10.00 (UTC) C (LTCG) (POGO instrumentation)
    prodidUtc1600_POGO_I_CPP  = 0x00B2,   // Visual C++ 10.00 (UTC) C++ (LTCG) (POGO instrumentation)
    prodidUtc1600_POGO_O_C    = 0x00B3,   // Visual C++ 10.00 (UTC) C (LTCG) (POGO optimization)
    prodidUtc1600_POGO_O_CPP  = 0x00B4,   // Visual C++ 10.00 (UTC) C++ (LTCG) (POGO optimization)
    // Version 16.1 PRODIDs start here
    prodidAliasObj1010        = 0x00B5,   // Visual C++ 10.10 (ALIASOBJ.EXE)
    prodidCvtpgd1610          = 0x00B6,   // CVTPGD 16.10
    prodidCvtres1010          = 0x00B7,   // CVTRES 10.10
    prodidExport1010          = 0x00B8,   // LINK 10.10 (EXP file)
    prodidImplib1010          = 0x00B9,   // LINK 10.10 (Import library)
    prodidLinker1010          = 0x00BA,   // LINK 10.10
    prodidMasm1010            = 0x00BB,   // MASM 10.10
    prodidUtc1610_C           = 0x00BC,   // Visual C++ 10.10 (UTC) C
    prodidUtc1610_CPP         = 0x00BD,   // Visual C++ 10.10 (UTC) C++
    prodidUtc1610_CVTCIL_C    = 0x00BE,   // Visual C++ 10.10 (UTC) C (CVTCIL)
    prodidUtc1610_CVTCIL_CPP  = 0x00BF,   // Visual C++ 10.10 (UTC) C++ (CVTCIL)
    prodidUtc1610_LTCG_C      = 0x00C0,   // Visual C++ 10.10 (UTC) C (LTCG)
    prodidUtc1610_LTCG_CPP    = 0x00C1,   // Visual C++ 10.10 (UTC) C++ (LTCG)
    prodidUtc1610_LTCG_MSIL   = 0x00C2,   // Visual C++ 10.10 (UTC) MSIL NetModule (LTCG)
    prodidUtc1610_POGO_I_C    = 0x00C3,   // Visual C++ 10.10 (UTC) C (LTCG) (POGO instrumentation)
    prodidUtc1610_POGO_I_CPP  = 0x00C4,   // Visual C++ 10.10 (UTC) C++ (LTCG) (POGO instrumentation)
    prodidUtc1610_POGO_O_C    = 0x00C5,   // Visual C++ 10.10 (UTC) C (LTCG) (POGO optimization)
    prodidUtc1610_POGO_O_CPP  = 0x00C6,   // Visual C++ 10.10 (UTC) C++ (LTCG) (POGO optimization)
    // Version 17 PRODIDs start here
    prodidAliasObj1100        = 0x00C7,   // Visual C++ 11.00 (ALIASOBJ.EXE)
    prodidCvtpgd1700          = 0x00C8,   // CVTPGD 17.00
    prodidCvtres1100          = 0x00C9,   // CVTRES 11.00
    prodidExport1100          = 0x00CA,   // LINK 11.00 (EXP file)
    prodidImplib1100          = 0x00CB,   // LINK 11.00 (Import library)
    prodidLinker1100          = 0x00CC,   // LINK 11.00
    prodidMasm1100            = 0x00CD,   // MASM 11.00
    prodidUtc1700_C           = 0x00CE,   // Visual C++ 11.00 (UTC) C
    prodidUtc1700_CPP         = 0x00CF,   // Visual C++ 11.00 (UTC) C++
    prodidUtc1700_CVTCIL_C    = 0x00D0,   // Visual C++ 11.00 (UTC) C (CVTCIL)
    prodidUtc1700_CVTCIL_CPP  = 0x00D1,   // Visual C++ 11.00 (UTC) C++ (CVTCIL)
    prodidUtc1700_LTCG_C      = 0x00D2,   // Visual C++ 11.00 (UTC) C (LTCG)
    prodidUtc1700_LTCG_CPP    = 0x00D3,   // Visual C++ 11.00 (UTC) C++ (LTCG)
    prodidUtc1700_LTCG_MSIL   = 0x00D4,   // Visual C++ 11.00 (UTC) MSIL NetModule (LTCG)
    prodidUtc1700_POGO_I_C    = 0x00D5,   // Visual C++ 11.00 (UTC) C (LTCG) (POGO instrumentation)
    prodidUtc1700_POGO_I_CPP  = 0x00D6,   // Visual C++ 11.00 (UTC) C++ (LTCG) (POGO instrumentation)
    prodidUtc1700_POGO_O_C    = 0x00D7,   // Visual C++ 11.00 (UTC) C (LTCG) (POGO optimization)
    prodidUtc1700_POGO_O_CPP  = 0x00D8,   // Visual C++ 11.00 (UTC) C++ (LTCG) (POGO optimization)
    // Version 18 PRODIDs start here
    prodidAliasObj1200        = 0x00D9,   // Visual C++ 12.00 (ALIASOBJ.EXE)
    prodidCvtpgd1800          = 0x00DA,   // CVTPGD 18.00
    prodidCvtres1200          = 0x00DB,   // CVTRES 12.00
    prodidExport1200          = 0x00DC,   // LINK 12.00 (EXP file)
    prodidImplib1200          = 0x00DD,   // LINK 12.00 (Import library)
    prodidLinker1200          = 0x00DE,   // LINK 12.00
    prodidMasm1200            = 0x00DF,   // MASM 12.00
    prodidUtc1800_C           = 0x00E0,   // Visual C++ 12.00 (UTC) C
    prodidUtc1800_CPP         = 0x00E1,   // Visual C++ 12.00 (UTC) C++
    prodidUtc1800_CVTCIL_C    = 0x00E2,   // Visual C++ 12.00 (UTC) C (CVTCIL)
    prodidUtc1800_CVTCIL_CPP  = 0x00D3,   // Visual C++ 12.00 (UTC) C++ (CVTCIL)
    prodidUtc1800_LTCG_C      = 0x00E4,   // Visual C++ 12.00 (UTC) C (LTCG)
    prodidUtc1800_LTCG_CPP    = 0x00E5,   // Visual C++ 12.00 (UTC) C++ (LTCG)
    prodidUtc1800_LTCG_MSIL   = 0x00E6,   // Visual C++ 12.00 (UTC) MSIL NetModule (LTCG)
    prodidUtc1800_POGO_I_C    = 0x00E7,   // Visual C++ 12.00 (UTC) C (LTCG) (POGO instrumentation)
    prodidUtc1800_POGO_I_CPP  = 0x00E8,   // Visual C++ 12.00 (UTC) C++ (LTCG) (POGO instrumentation)
    prodidUtc1800_POGO_O_C    = 0x00E9,   // Visual C++ 12.00 (UTC) C (LTCG) (POGO optimization)
    prodidUtc1800_POGO_O_CPP  = 0x00EA,   // Visual C++ 12.00 (UTC) C++ (LTCG) (POGO optimization)
};


#define DwProdidFromProdidWBuild(prodid, wBuild) ((((unsigned long) (prodid)) << 16) | (wBuild))
#define ProdidFromDwProdid(dwProdid)             ((PRODID) ((dwProdid) >> 16))
#define WBuildFromDwProdid(dwProdid)             ((dwProdid) & 0xFFFF)


    // Symbol name and attributes in coff symbol table (requires windows.h)

#define symProdIdentName    "@comp.id"
#define symProdIdentClass   IMAGE_SYM_CLASS_STATIC
#define symProdIdentSection IMAGE_SYM_ABSOLUTE


    // Define the image data format

typedef struct PRODITEM {
    unsigned long   dwProdid;          // Product identity
    unsigned long   dwCount;           // Count of objects built with that product
} PRODITEM;


enum {
    tagEndID    = 0x536e6144,
    tagBegID    = 0x68636952,
};

/*
  Normally, the DOS header and PE header are contiguous.  We place some data
  in between them if we find at least one tagged object file.

  struct {
    IMAGE_DOS_HEADER dosHeader;
    BYTE             rgbDosStub[N];          // MS-DOS stub
    PRODITEM         { tagEndID, 0 };        // start of tallies (Masked with dwMask)
    PRODITEM         { 0, 0 };               // end of tallies   (Masked with dwMask)
    PRODITEM         rgproditem[];           // variable sized   (Masked with dwMask)
    PRODITEM         { tagBegID, dwMask };   // end of tallies
    PRODITEM         { 0, 0 };               // variable sized
    IMAGE_PE_HEADER  peHeader;
    };

*/


#endif

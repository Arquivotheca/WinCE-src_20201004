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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#ifndef CIQ_HLP_H
#define CIQ_HLP_H

#include <windows.h>
#include <sddrv.h>

float DecipherMinCurrent(UCHAR val);

USHORT DecipherMaxCurrent(UCHAR val);

LPTSTR DecipherFlagsB(FT_TYPE ft, BOOLEAN b);

LPTSTR DecipherFlagsU(FT_TYPE ft, UCHAR uc);

LPTSTR fsDecipherFS(SD_FS_TYPE fs);

LPTSTR ucDecipherFS(UCHAR uc);

DOUBLE DecipherTAAC(UCHAR uc);

ULONG DecipherTRAN_SPEED(UCHAR uc);

ULONGLONG CalculateCapacity(UCHAR const *buff, PTEST_PARAMS pTestParams);

#endif //CIQ_HLP_H


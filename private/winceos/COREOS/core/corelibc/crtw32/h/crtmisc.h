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
#ifndef _INC_CRTMISC
#define _INC_CRTMISC

// from sys/stat.h
#define _S_IREAD    0000400     /* read permission, owner */
#define _S_IWRITE   0000200     /* write permission, owner */

// from cvt.h
#define MUL10(x)    ( (((x)<<2) + (x))<<1 )

int __cdecl __internal_wctomb_s(int *pRetValue, char *dst, size_t sizeInBytes, wchar_t wchar);

#define change_to_widechar(pwc, sz, len) MultiByteToWideChar(CP_ACP, 0, sz, len, pwc, 1)

#endif

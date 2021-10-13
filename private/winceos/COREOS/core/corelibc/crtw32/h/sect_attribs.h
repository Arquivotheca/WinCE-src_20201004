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
/***
*sect_attribs.h - section attributes for CRTs
*
****/

#if (_MSC_VER >= 1400)

#define _ATTRIBUTES read
#define CRTMERGESECT ".rdata"

#else

#define _ATTRIBUTES read,write
#define CRTMERGESECT ".data"

#endif

#pragma section(".CRT$XCA",_ATTRIBUTES)
#pragma section(".CRT$XCAA",_ATTRIBUTES)
#pragma section(".CRT$XCC",_ATTRIBUTES)
#pragma section(".CRT$XCZ",_ATTRIBUTES)
#pragma section(".CRT$XIA",_ATTRIBUTES)
#pragma section(".CRT$XIC",_ATTRIBUTES)
#pragma section(".CRT$XIY",_ATTRIBUTES)
#pragma section(".CRT$XIZ",_ATTRIBUTES)
#pragma section(".CRT$XLA",_ATTRIBUTES)
#pragma section(".CRT$XLZ",_ATTRIBUTES)
#pragma section(".CRT$XPA",_ATTRIBUTES)
#pragma section(".CRT$XPX",_ATTRIBUTES)
#pragma section(".CRT$XPXA",_ATTRIBUTES)
#pragma section(".CRT$XPZ",_ATTRIBUTES)
#pragma section(".CRT$XTA",_ATTRIBUTES)
#pragma section(".CRT$XTB",_ATTRIBUTES)
#pragma section(".CRT$XTX",_ATTRIBUTES)
#pragma section(".CRT$XTZ",_ATTRIBUTES)
#pragma section(".rdata$T",_ATTRIBUTES)
#pragma section(".rtc$IAA",_ATTRIBUTES)
#pragma section(".rtc$IZZ",_ATTRIBUTES)
#pragma section(".rtc$TAA",_ATTRIBUTES)
#pragma section(".rtc$TZZ",_ATTRIBUTES)

#define _CRTALLOC(x) __declspec(allocate(x))

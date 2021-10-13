//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef _NTCOMPAT_H_
#define _NTCOMPAT_H_

#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#endif

#endif // _NTCOMPAT_H_

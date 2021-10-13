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
#ifndef INCLUDED_SRE_H
#define INCLUDED_SRE_H

#include "headers.h"

#include "config.h"

bool write_sre(wstring input_file, string memory_type, DWORD rom_offset, DWORD virtual_base, bool launch);

#endif

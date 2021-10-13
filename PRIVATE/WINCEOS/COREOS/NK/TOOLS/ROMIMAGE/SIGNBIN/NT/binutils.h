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
#ifndef INCLUDED_BINPARSE_H
#define INCLUDED_BINPARSE_H

#include <pehdr.h>
#include <romldr.h>

#include "traverse.h"

bool process_image(const ROMHDR *promhdr, DWORD _rom_offset, SIGNING_CALLBACK _call_back);
bool process_image(const TCHAR *_file_name, SIGNING_CALLBACK _call_back);
bool read_image(void *pDestBuffer, DWORD dwAddress, DWORD dwLength, bool use_rom_offset = true);

#endif

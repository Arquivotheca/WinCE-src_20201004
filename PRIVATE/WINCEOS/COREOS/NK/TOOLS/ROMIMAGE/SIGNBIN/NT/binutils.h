//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

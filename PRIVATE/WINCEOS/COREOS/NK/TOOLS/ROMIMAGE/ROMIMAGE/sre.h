//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_SRE_H
#define INCLUDED_SRE_H

#include "headers.h"

#include "config.h"

bool write_sre(string input_file, string memory_type, DWORD rom_offset, DWORD virtual_base, bool launch);

#endif

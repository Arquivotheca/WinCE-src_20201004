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
#ifndef INCLUDED_PARSE_H
#define INCLUDED_PARSE_H

#include "headers.h"

#include "..\parser\parser.h"

#include "memory.h"
#include "config.h"
#include "file.h"
#include "module.h"

bool ParseBIBFile(string &file_name,
                MemoryList &memory_list,
                MemoryList &reserve_list,
                ModuleList &module_list,
                FileList   &file_list,
                Config     &config_settings);

#endif


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
#ifndef INCLUDED_BIN_H
#define INCLUDED_BIN_H

#include "headers.h"

#include "memory.h"
#include "config.h"
#include "file.h"
#include "module.h"

bool write_bin(AddressList &hole_list, CopyList &copy_list,
                ModuleList &module_list, FileList &file_list, 
                MemoryList &memory_list, MemoryList &reserve_list,
                ModuleList::iterator &kernel, Config &config, MemoryList::iterator xip_mem);

bool write_chain(const Config &config, const MemoryList &memory_list);

#endif

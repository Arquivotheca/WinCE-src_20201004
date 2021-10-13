//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

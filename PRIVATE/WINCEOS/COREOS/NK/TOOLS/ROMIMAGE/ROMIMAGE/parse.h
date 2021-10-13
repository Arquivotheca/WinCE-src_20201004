//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef INCLUDED_PARSE_H
#define INCLUDED_PARSE_H

#include "headers.h"

#include "memory.h"
#include "config.h"
#include "file.h"
#include "module.h"

bool ParseBIBFile(const string &file_name, 
                MemoryList &memory_list, 
                MemoryList &reserve_list, 
                ModuleList &module_list, 
                FileList   &file_list,
                Config     &config_settings);

#endif

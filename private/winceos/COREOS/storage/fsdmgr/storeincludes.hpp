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
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef __STOREINCLUDES_HPP__
#define __STOREINCLUDES_HPP__

#ifndef UNDER_CE
    #include "fsdmgrp.h"
    #include "fsddbg.h"
    #include "fsdhelper.hpp"
    #include "storemain_nt.hpp"
    #include "mountedvolume.hpp"
    #include "mounttable.hpp"
    #include "filesystem.hpp"
    #include "logicaldisk.hpp"
    #include "handle.hpp"
    #include "virtroot_nt.hpp"
    #include "fileapi.hpp"
    #include "searchapi.hpp"
    #include "volumeapi.hpp"
    #include "fsdmgrapi.hpp"
    #include "blockdevice.hpp"
    #include "partition_nt.hpp"
    #include "storedisk_nt.hpp"
#else
    #include "fsdmgrp.h"
    #include "fsddbg.h"
    #include "fsdhelper.hpp"
    #include "storemain.hpp"
    #include "mountedvolume.hpp"
    #include "mounttable.hpp"
    #include "filesystem.hpp"
    #include "logicaldisk.hpp"
    #include "handle.hpp"
    #include "virtroot.hpp"
    #include "fileapi.hpp"
    #include "searchapi.hpp"
    #include "volumeapi.hpp"
    #include "fsdmgrapi.hpp"
    #include "blockdevice.hpp"
    #include "partition.hpp"
    #include "storedisk.hpp"
#endif

#include <string.hxx>
#include <dbgapi.h>

#include "canonicalizer.hpp"

#include <pathapi.hpp>
#include <fsdmain.hpp>
#include <afsapi.hpp>

#endif // __STOREINCLUDES_HPP__


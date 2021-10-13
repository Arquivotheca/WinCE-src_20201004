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
////////////////////////////////////////////////////////////////////////////////
//
//  MemDivider.h
//    Provides programmatic control of storage/RAM division on CE device.
//
//  Revision History:
//      Jonathan Leonard (a-joleo) : 11/30/2009 - Created.
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

class MemDivider
{
public:
    // scoped (i.e., RAII) when !bPermanent
    MemDivider(unsigned __int64 bytesRam, bool bPermanent = false);
    MemDivider(float percentRam, bool bPermanent = false);
    ~MemDivider();

private:
    void SetMemoryDivision(unsigned __int64 bytesRam, DWORD curRamPages,
        DWORD curStoragePages, DWORD dwPageSize, bool bPermanent);
    DWORD origStoragePages;
};
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
#ifndef __READERWRITERLOCK_H
#define __READERWRITERLOCK_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

class CReaderWriterLock : public CObject
{
private:
    // Number of readers
    ULONG m_ulReadCounter;
    // Does the writer have the lock?
    BOOL m_bWriter;
    //Guard to serialize consumers
    NDIS_SPIN_LOCK m_spinLockReadWrite;
    
public:
    CReaderWriterLock();
    virtual ~CReaderWriterLock();
    void Read();
    void ReadComplete();
    void Write();
    void WriteComplete();
};

//------------------------------------------------------------------------------

#endif


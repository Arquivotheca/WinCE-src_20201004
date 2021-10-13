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
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "StdAfx.h"
#include "ReaderWriterLock.h"

CReaderWriterLock::CReaderWriterLock()
{
            m_ulReadCounter = 0;
    m_bWriter = FALSE;
    NdisAllocateSpinLock(&m_spinLockReadWrite);

}

CReaderWriterLock:: ~CReaderWriterLock()
{
    ASSERT(m_ulReadCounter == 0);
    //we don't check if there was a pause without a restart, since the final unbind pauses the driver stack without restarting it
    NdisFreeSpinLock(&m_spinLockReadWrite);
}

void CReaderWriterLock::Read()
{
    for(;;)
    {
        NdisAcquireSpinLock(&m_spinLockReadWrite);
        if (m_bWriter)
        {
            //A writer has already taken the lock, retry
            NdisReleaseSpinLock(&m_spinLockReadWrite);
        }
        else
        {
            m_ulReadCounter++;
            break;
        }
    }
    
    NdisReleaseSpinLock(&m_spinLockReadWrite);
}

void CReaderWriterLock::ReadComplete()
{
    NdisAcquireSpinLock(&m_spinLockReadWrite);
    ASSERT(m_bWriter == FALSE);
    ASSERT(m_ulReadCounter > 0);
    m_ulReadCounter--;
    NdisReleaseSpinLock(&m_spinLockReadWrite);
}

void CReaderWriterLock::Write()
{
    for(;;)
    {
        NdisAcquireSpinLock(&m_spinLockReadWrite);
        if (m_ulReadCounter > 0)
        {
            //Readers have already taken the lock, retry
            NdisReleaseSpinLock(&m_spinLockReadWrite);
        }
        else
        {
            m_bWriter = TRUE;
            break;
        }
    }
    
    NdisReleaseSpinLock(&m_spinLockReadWrite);
}

void CReaderWriterLock::WriteComplete()
{
    NdisAcquireSpinLock(&m_spinLockReadWrite);
    ASSERT(m_ulReadCounter == 0);
    m_bWriter = FALSE;
    NdisReleaseSpinLock(&m_spinLockReadWrite);
}


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
#include "driverenumerator.h"

CDriverEnumerator::CDriverEnumerator()
{
    m_ptszDevices = NULL;
    m_nDeviceCount = 0;
}

CDriverEnumerator::~CDriverEnumerator()
{
    Cleanup();
}

void
CDriverEnumerator::Cleanup()
{
    if(m_ptszDevices)
    {
        for(int i = 0; i < m_nDeviceCount; i++)
        {
            delete[] m_ptszDevices[i];
            m_ptszDevices[i] = NULL;
        }

        delete[] m_ptszDevices;
        m_ptszDevices = NULL;
        m_nDeviceCount = 0;
    }

}

HRESULT
CDriverEnumerator::Init(const GUID *DriverGuid)
{
    // we assume failure waiting for the message queue, 
    // if we succeed then we change the result to success.
    HRESULT hr = E_FAIL;
    HANDLE hNotification = NULL, hMsgQueue = NULL;
    DWORD dwWaitResult;
    MSGQUEUEOPTIONS msgQOptions ;
    int nMsgQueueTimout = 500;
    int nMaxMessageSize = (MAX_DEVCLASS_NAMELEN * sizeof(TCHAR))+ sizeof( DEVDETAIL );
    memset(&msgQOptions, 0, sizeof(MSGQUEUEOPTIONS));
    msgQOptions.dwSize = sizeof( MSGQUEUEOPTIONS ) ;
    msgQOptions.cbMaxMessage = nMaxMessageSize;
    msgQOptions.bReadAccess = TRUE ;

    // before we allocate anything new, make sure all of our data structures are cleaned up.
    Cleanup();

    hMsgQueue = CreateMsgQueue( NULL, &msgQOptions ) ;
    if ( NULL == hMsgQueue )
    {
        hr = E_FAIL;
        goto cleanup;
    }

    hNotification = RequestDeviceNotifications(DriverGuid, hMsgQueue, TRUE);

    if ( NULL == hNotification )
    {
        hr = E_FAIL;
        goto cleanup;
    }

    DEVDETAIL *ddDeviceInformation = NULL;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;

    ddDeviceInformation = (DEVDETAIL *) new(BYTE[nMaxMessageSize]);

    if(NULL == ddDeviceInformation)
    {
        hr = E_FAIL;
        goto cleanup;
    }

    do
    {

        // enumerate camera drivers in the system
        dwWaitResult = WaitForSingleObject ( hMsgQueue, nMsgQueueTimout) ;

        if(WAIT_OBJECT_0 == dwWaitResult)
        {
            memset( ddDeviceInformation, 0x0, nMaxMessageSize);

            if ( FALSE == ReadMsgQueue( hMsgQueue, ddDeviceInformation, nMaxMessageSize, &dwBytesRead, nMsgQueueTimout, &dwFlags ) )
            {
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                hr = E_FAIL;
                goto cleanup;
            }

            if(0 == ddDeviceInformation->cbName || NULL == ddDeviceInformation->szName )
            {
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                hr = E_FAIL;
                goto cleanup;
            }

            // realloc if necessary, otherwise do the initial allocation
            if(m_ptszDevices)
            {
                TCHAR **ptszTemp = NULL;

                ptszTemp = new(TCHAR *[m_nDeviceCount+1]);
                if(ptszTemp)
                {
                    memcpy(ptszTemp, m_ptszDevices, sizeof(TCHAR *) * m_nDeviceCount);
                    delete[] m_ptszDevices;
                    m_ptszDevices = ptszTemp;
                }
                else
                {
                    // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                    hr = E_FAIL;
                    goto cleanup;
                }
            }
            else
            {
                m_ptszDevices = new(TCHAR *[1]);
            }

            if(!m_ptszDevices)
            {
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                hr = E_FAIL;
                goto cleanup;
            }

            // copy over the name given.
            m_ptszDevices[m_nDeviceCount] = new(TCHAR[ddDeviceInformation->cbName]);

            if(!m_ptszDevices[m_nDeviceCount])
            {
                // unexpected failure while partially initizlized, the destructor will handle cleanup properly.
                hr = E_FAIL;
                goto cleanup;
            }

            // copy the string over, our maximum length is the size allocated.
            _tcsncpy(m_ptszDevices[m_nDeviceCount], ddDeviceInformation->szName, ddDeviceInformation->cbName/sizeof(TCHAR));

            // we successfully gathered a device name, so the call is a success (so far)
            hr = S_OK;
            // increment the number of successful allocations.
            m_nDeviceCount++;
        }
    // if we get anything else, then we failed.
    }while(WAIT_OBJECT_0 == dwWaitResult);

cleanup:

    if(ddDeviceInformation)
        delete[] ddDeviceInformation;

    if(hNotification && !StopDeviceNotifications(hNotification))
    {
        hr = E_FAIL;
    }
    hNotification = NULL;

    if(hMsgQueue && !CloseMsgQueue(hMsgQueue))
    {
        hr = E_FAIL;
    }

    hMsgQueue = NULL;

    return hr;
}

HRESULT
CDriverEnumerator::GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount)
{
    if(tszCamDeviceName && nEntryCount)
    {
        if(m_ptszDevices && m_nDeviceCount > 0)
        {
            *tszCamDeviceName = m_ptszDevices;
            *nEntryCount = m_nDeviceCount;
            return S_OK;
        }
    }

    return E_FAIL;
}

int
CDriverEnumerator::GetDeviceIndex(TCHAR *tszCamDeviceName)
{
    if(tszCamDeviceName)
    {
        if(m_ptszDevices && m_nDeviceCount > 0)
        {
            for(int i =0; i < m_nDeviceCount; i++)
            {
                if(0 == _tcscmp(m_ptszDevices[i], tszCamDeviceName))
                    return i;
            }
        }
    }
    return -1;
}

TCHAR *
CDriverEnumerator::GetDeviceName(int nDeviceIndex)
{
    if(nDeviceIndex >= 0 && nDeviceIndex < m_nDeviceCount)
    {
        if(m_ptszDevices)
            return m_ptszDevices[nDeviceIndex];
    }
    return NULL;
}

int
CDriverEnumerator::GetMaxDeviceIndex()
{
    // count-1 is the maximum valid index
    return m_nDeviceCount - 1;
}
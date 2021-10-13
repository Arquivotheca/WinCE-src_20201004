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
/*++
Module Name:

     ifdrdr.cpp

Abstract:
Functions:
Notes:
--*/

#ifndef UNDER_CE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>

#include <afx.h>
#include <afxtempl.h>
#else
#include <windows.h>
#include "afxutil.h"
#endif

#include <winioctl.h>
#include <winsmcrd.h>

#include "ifdtest.h"


ULONG CReaderList::m_uRefCount     = 0;
ULONG CReaderList::m_uNumReaders   = 0;
CReaderList **CReaderList::m_pList = 0;
static CString l_CEmpty("");

void
DumpData(
    __in LPCTSTR in_pchCaption,
    ULONG in_uIndent,
    __in_bcount(in_uLength) const BYTE *in_pbData,
    ULONG in_uLength)
{
    ULONG l_uIndex, l_uLine, l_uCol;

    wprintf(L"%s\n%*s%04x: ", in_pchCaption, in_uIndent, L"", 0);

    // show rows of 8 bytes

    for (l_uLine = 0, l_uIndex = 0;
        l_uLine < ((in_uLength - 1) / 8) + 1;
        l_uLine++)
    {
        for (l_uCol = 0, l_uIndex = l_uLine * 8;
            (l_uCol < 8) && ( l_uIndex < in_uLength);
            l_uCol++, l_uIndex++)
        {
            wprintf(
                l_uIndex < in_uLength ? L"%02x " : L"   ",
                in_pbData[l_uIndex]
                );
        }

        putchar(' ');

        // show the ascii

        for (l_uCol = 0, l_uIndex = l_uLine * 8;
            (l_uCol < 8) && ( l_uIndex < in_uLength);
            l_uCol++, l_uIndex++)
        {
            wprintf(
                l_uIndex < in_uLength ? L"%c" : L" ",
                isprint(in_pbData[l_uIndex]) ? in_pbData[l_uIndex] : L'.'
                );
        }

        putchar('\n');

        // if not done, another preamble

        if (l_uIndex  < in_uLength)
        {
            wprintf(L"%*s%04x: ", in_uIndent, L"", l_uIndex + 1);
        }
    }
}

CReaderList::CReaderList(
    const CString &in_CDeviceName,
    const CString &in_CPnPType,
    const CString &in_CVendorName,
    const CString &in_CIfdType
    )
{
    m_CDeviceName += in_CDeviceName;
    m_CPnPType += in_CPnPType;
    m_CVendorName += in_CVendorName;
    m_CIfdType += in_CIfdType;
}

CString &
CReaderList::GetDeviceName(
    ULONG in_uIndex
    )
/*++

Routine Description:
    Retrieves the device name of a reader

Arguments:
    in_uIndex - index to reader list

Return Value:
    The device name that can be used to open the reader

--*/
{
    if (in_uIndex >= m_uNumReaders)
    {
        return l_CEmpty;
    }

    return m_pList[in_uIndex]->m_CDeviceName;
}

CString &
CReaderList::GetIfdType(
    ULONG in_uIndex
    )
{
    if (in_uIndex >= m_uNumReaders)
    {
        return l_CEmpty;
    }

    return m_pList[in_uIndex]->m_CIfdType;
}

CString &
CReaderList::GetPnPType(
    ULONG in_uIndex
    )
{
    if (in_uIndex >= m_uNumReaders)
    {
        return l_CEmpty;
    }

    return m_pList[in_uIndex]->m_CPnPType;
}

CString &
CReaderList::GetVendorName(
    ULONG in_uIndex
    )
{
    if (in_uIndex >= m_uNumReaders)
    {
        return l_CEmpty;
    }

    return m_pList[in_uIndex]->m_CVendorName;
}

void
CReaderList::AddDevice(
    CString in_CDeviceName,
    CString in_CPnPType
    )
/*++

Routine Description:
    This functions tries to open the reader device supplied by
    in_pchDeviceName. If the device exists it adds it to the list
    of installed readers

Arguments:
    in_pchDeviceName - reader device name
    in_pchPnPType - type of reader (wdm-pnp, nt, win9x)

--*/
{
    CReader l_CReader;

    if (l_CReader.Open(in_CDeviceName))
    {
        if (l_CReader.GetVendorName().IsEmpty())
        {
            LogMessage(
                LOG_CONSOLE,
                IDS_VENDORNAME,
                (LPCTSTR) in_CDeviceName
                );

        }
        else if (l_CReader.GetIfdType().IsEmpty())
        {
            LogMessage(
                LOG_CONSOLE,
                IDS_IFDTYPE,
                (LPCTSTR) in_CDeviceName
                );

        }
        else
        {
            if (m_uNumReaders >= MAXIMUM_SMARTCARD_READERS)
            {
                return; 
            }

            CReaderList *l_CReaderList = new CReaderList(
                in_CDeviceName,
                in_CPnPType,
                l_CReader.GetVendorName(),
                l_CReader.GetIfdType()
                );
            
            if(!l_CReaderList)
            {
                return; 
            }


            // extend the device list array by one
            CReaderList **l_pList = new CReaderList *[m_uNumReaders + 1];

            if(!l_pList)
            {
                return;
            }

            if (m_pList)
            {

                // copy old list of readers to new list of readers
                memcpy(
                    l_pList,
                    m_pList,
                    m_uNumReaders * sizeof(CReaderList *)
                    );

                delete m_pList;
                m_pList = 0;
            }

            m_pList = l_pList;
            m_pList[m_uNumReaders++] = l_CReaderList;
        }

        l_CReader.Close();
    }
}

CReaderList::CReaderList()
/*++

Routine Description:
    Constructor for CReaderList.
    Builds a list of currently installed and running smart card readers.
    It first tries to find all WDM PnP drivers. These should be registered
    in the registry under the class guid for smart card readers.

    Then it looks for all 'old style' reader names like \\.\SCReaderN

    And then it looks for all Windows 9x VxD style readers, which are
    registered in the registry through smclib.vxd

--*/
{     
    ULONG l_uIndex;

    m_uCurrentReader = (ULONG) -1;

    if (m_uRefCount++ != 0)
    {
        return;
    }

    const UCHAR MAX_NAME_LENGTH = 128;
    TCHAR l_rgchDeviceName[MAX_NAME_LENGTH];

    // Recall that CE drivers are numbered 1, 2, ... 9, 0.
    for (l_uIndex = 1; l_uIndex <= MAXIMUM_SMARTCARD_READERS; l_uIndex++) {

        if (MAXIMUM_SMARTCARD_READERS != l_uIndex)
        {
            StringCchPrintf(
                (LPTSTR) l_rgchDeviceName, 
                MAX_NAME_LENGTH,
                L"SCR%d:", 
                l_uIndex
                );
        }
        else
        {
            StringCchPrintf(
                (LPTSTR) l_rgchDeviceName, 
                MAX_NAME_LENGTH,
                L"SCR0:");
        }

        AddDevice(CString(l_rgchDeviceName), READER_TYPE_CE);
    }

}

CReaderList::~CReaderList()
{
    ULONG l_uIndex;

    if (m_uRefCount == 0)
    {
        // This case will be the deletion of an element of the list.
        // There is nothing to be done here.
        return;
    }

    if (--m_uRefCount != 0)
    {
        // The is the case where we delete a reference to the list -
        // not the "base" reference that would require deletion of 
        // the list content.
        return;
    }

    ASSERT(m_uRefCount >= 0);

    for (l_uIndex = 0; l_uIndex < m_uNumReaders; l_uIndex++)
    {
        m_uNumReaders--;
        delete m_pList[l_uIndex];
        m_pList[l_uIndex] = 0;
    }

    if (m_pList)
    {
        delete [] m_pList;
        m_pList = 0;
    }
}

// ****************************************************************************
// CReader methods
// ****************************************************************************

CReader::CReader(
    void
    )
{
    m_uReplyBufferSize = sizeof(m_rgbReplyBuffer);


    m_ScardIoRequest.dwProtocol = 0;
    m_ScardIoRequest.cbPciLength = sizeof(m_ScardIoRequest);

    m_fDump = FALSE;
}

BOOL
CReader::Close(
    void
    )
{
#ifndef SIMULATE
    LogMessage(LOG_DETAIL, 
               TestLoadStringResource(IDS_CLOSE_DRIVER),
               (LPCTSTR)m_CDeviceName);
    return CloseHandle(m_hReader);
#endif
}

CString &
CReader::GetIfdType(
    void
    )
{
    ULONG l_uAttr = SCARD_ATTR_VENDOR_IFD_TYPE;

#ifdef SIMULATE
    m_CIfdType = TestLoadStringResource(IDS_DEBUG_IFDTYPE).GetBuffer(0);
#endif

    BOOL l_bResult = DeviceIoControl(
        m_hReader,
        IOCTL_SMARTCARD_GET_ATTRIBUTE,
        (void *) &l_uAttr,
        sizeof(ULONG),
        m_rgbReplyBuffer,
        sizeof(m_rgbReplyBuffer),
        &m_uReplyLength,
        NULL
        );


    if (l_bResult)
    {
        m_rgbReplyBuffer[m_uReplyLength] = '\0';
        m_CIfdType = (LPCSTR)m_rgbReplyBuffer; // String class can take
                                               // an ASCII array.
    }

    return m_CIfdType;
}

LONG
CReader::GetState(
    PULONG out_puState
    )
{
    SetLastError(0);

    BOOL l_bResult = DeviceIoControl(
        m_hReader,
        IOCTL_SMARTCARD_GET_STATE,
        NULL, 
        0,
        (void *) out_puState,
        sizeof(ULONG),
        &m_uReplyLength,
        NULL
        );


    return GetLastError();
}

CString &
CReader::GetVendorName(
    void
    )
{
    ULONG l_uAttr = SCARD_ATTR_VENDOR_NAME;

#ifdef SIMULATE
    m_CVendorName = TestLoadStringResource(IDS_DEBUG_VENDOR).GetBuffer(0);
#endif

    BOOL l_bResult = DeviceIoControl(
        m_hReader,
        IOCTL_SMARTCARD_GET_ATTRIBUTE,
        (void *) &l_uAttr,
        sizeof(ULONG),
        m_rgbReplyBuffer,
        sizeof(m_rgbReplyBuffer),
        &m_uReplyLength,
        NULL
        );


    if (l_bResult)
    {
        m_rgbReplyBuffer[m_uReplyLength] = '\0';
        m_CVendorName = (LPCSTR)m_rgbReplyBuffer; // String class can take
                                                  // an ASCII array.
    }

    return m_CVendorName;
}

ULONG
CReader::GetDeviceUnit(
    void
    )
{
    ULONG l_uAttr = SCARD_ATTR_DEVICE_UNIT;

    BOOL l_bResult = DeviceIoControl(
        m_hReader,
        IOCTL_SMARTCARD_GET_ATTRIBUTE,
        (void *) &l_uAttr,
        sizeof(ULONG),
        m_rgbReplyBuffer,
        sizeof(m_rgbReplyBuffer),
        &m_uReplyLength,
        NULL
        );


    return (ULONG) *m_rgbReplyBuffer;
}

BOOLEAN
CReader::Open(
    void
    )
{
    if (m_CDeviceName.IsEmpty())
    {
        return FALSE;
    }
    LogMessage(LOG_DETAIL, 
               TestLoadStringResource(IDS_OPEN_DRIVER),
               (LPCTSTR)m_CDeviceName);

    // Try to open the reader.
    m_hReader = CreateFile(
        (LPCTSTR) m_CDeviceName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
        );

    if (m_hReader == INVALID_HANDLE_VALUE )
    {
        LogMessage(LOG_DETAIL,
                   TestLoadStringResource(IDS_CANNOT_OPEN_DRIVER),
                   (LPCTSTR)m_CDeviceName);
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
CReader::Open(
    const CString & in_CDeviceName
    )
{
    // save the reader name
    m_CDeviceName = in_CDeviceName;

#ifdef SIMULATE
    return TRUE;
#endif

    return Open();
}

LONG
CReader::PowerCard(
    ULONG in_uMinorIoControl
    )
/*++

Routine Description:

    Cold resets the current card and sets the ATR
    of the card in the reader class.

Return Value:

    Returns the result of the DeviceIoControl call

--*/
{
    BOOL l_bResult;
    ULONG l_uReplyLength;
    CHAR l_rgbAtr[SCARD_ATR_LENGTH];

    SetLastError(0);

       l_bResult = DeviceIoControl (
        m_hReader,
        IOCTL_SMARTCARD_POWER,
        &in_uMinorIoControl,
        sizeof(in_uMinorIoControl),
        l_rgbAtr,
        sizeof(l_rgbAtr),
        &l_uReplyLength,
        NULL
        );     


    if (GetLastError() == ERROR_SUCCESS)
    {
        SetAtr((PBYTE) l_rgbAtr, l_uReplyLength);
    }

    return GetLastError();
}

LONG
CReader::SetProtocol(
    const ULONG in_uProtocol
    )
{
    BOOL l_bResult;

     m_ScardIoRequest.dwProtocol = in_uProtocol;
    m_ScardIoRequest.cbPciLength = sizeof(SCARD_IO_REQUEST);

    SetLastError(0);

    l_bResult = DeviceIoControl (
        m_hReader,
        IOCTL_SMARTCARD_SET_PROTOCOL,
        (void *) &in_uProtocol,
        sizeof(ULONG),
        m_rgbReplyBuffer,
        sizeof(m_rgbReplyBuffer),
        &m_uReplyLength,
        NULL
        );     
    return GetLastError();
}

DWORD
CReader::Transmit(
    const UCHAR * in_pchApdu,
    ULONG in_uApduLength,
    PUCHAR *out_pchReply,
    PULONG out_puReplyLength
    )
/*++

Routine Description:
    Transmits an apdu using the currently connected reader

Arguments:
    in_pchApdu - the apdu to send
    in_uApduLength - the length of the apdu
    out_pchReply - result returned from the reader/card
    out_puReplyLength - pointer to store number of bytes returned

Return Value:
    The nt-status code returned by the reader

--*/
{
    BOOL l_bResult;
    ULONG l_uBufferLength = m_ScardIoRequest.cbPciLength + in_uApduLength;

    if (l_uBufferLength == 0 ||
        l_uBufferLength < m_ScardIoRequest.cbPciLength || 
        l_uBufferLength < in_uApduLength)
    {
        ASSERT(!L"ERROR: CReader::Transmit has overflow!");
        return ERROR_BUFFER_OVERFLOW;
    }


    PUCHAR l_pchBuffer = new UCHAR [l_uBufferLength];
    DWORD l_dwError;

    if(NULL == l_pchBuffer) 
    {
        return GetLastError();
    }
        
    // Copy io-request header to request buffer
    memcpy(
        l_pchBuffer,
        &m_ScardIoRequest,
        m_ScardIoRequest.cbPciLength
        );

    // copy io-request header to reply buffer
    memcpy(
        m_rgbReplyBuffer,
        &m_ScardIoRequest,
        m_ScardIoRequest.cbPciLength
        );

    // append apdu to buffer
    if(m_ScardIoRequest.cbPciLength + in_uApduLength <= l_uBufferLength)    
    {
        memcpy(
            l_pchBuffer + m_ScardIoRequest.cbPciLength,
            in_pchApdu,
            in_uApduLength
            );
    }
    else
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    if (m_fDump)
    {
        DumpData(
            TestLoadStringResource(IDS_REQ_DATA).GetBuffer(0),
            3,
            l_pchBuffer,
            l_uBufferLength
            );
    }

    SetLastError(0);
    // send the request to the card
    l_bResult = DeviceIoControl (
        m_hReader,
        IOCTL_SMARTCARD_TRANSMIT,
        l_pchBuffer,
        l_uBufferLength,
        m_rgbReplyBuffer,
        m_uReplyBufferSize,
        &m_uReplyLength,
        NULL
        );     

    l_dwError = GetLastError();
    if (m_fDump)
    {
        wprintf(
            TestLoadStringResource(IDS_IOCTL_RET_VAL).GetBuffer(0),
            l_dwError
            );

        if (l_bResult)
        {
            DumpData(
                TestLoadStringResource(IDS_REPLY_DATA).GetBuffer(0),
                3,
                m_rgbReplyBuffer,
                m_uReplyLength
                );
        }
        wprintf(L"%*s", 53, L"");
    }

    *out_pchReply = (PUCHAR) m_rgbReplyBuffer + m_ScardIoRequest.cbPciLength;
    *out_puReplyLength = m_uReplyLength - m_ScardIoRequest.cbPciLength;

    delete [] l_pchBuffer;
    return l_dwError;
}

LONG
CReader::VendorIoctl(
    PUCHAR* o_ppInternalBuffer,
    ULONG* o_uLen
    )
{
    BOOL l_bResult = DeviceIoControl(
        m_hReader,
        CTL_CODE(FILE_DEVICE_SMARTCARD, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS),
        NULL,
        NULL, 
        m_rgbReplyBuffer,
        sizeof(m_rgbReplyBuffer),
        &m_uReplyLength,
        NULL
        );


    if (l_bResult)
    {
        m_rgbReplyBuffer[m_uReplyLength] = '\0';
        (*o_ppInternalBuffer)= m_rgbReplyBuffer;
        (*o_uLen)= m_uReplyLength;
    }

    return GetLastError();
}

LONG
CReader::WaitForCard(
    const ULONG in_uWaitFor
    )
{
    BOOL l_bResult;
    ULONG l_uReplyLength;

    SetLastError(0);
        
    // In Windows Embedded CE we are expected to send an
    // IOCTL_SMARTCARD_RESET_EVENTS before doing a
    // wait call.  This detail is handled by SMCLIB
      l_bResult = DeviceIoControl (
        m_hReader,
        IOCTL_SMARTCARD_RESET_EVENTS,
        NULL,
        0,
        NULL,
        0,
        &l_uReplyLength,
        NULL
        );

       l_bResult = DeviceIoControl (
        m_hReader,
        in_uWaitFor,
        NULL,
        0,
        NULL,
        0,
        0,
        NULL
        );     

    // if the IOCTL returns FALSE, check GetLastError()
    // if there is an error GetLastError() != 0
    if (!l_bResult)
    {
        return GetLastError();
    }
    return ERROR_SUCCESS;
}

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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the TelnetPort_t class.
//
// ----------------------------------------------------------------------------

#include "TelnetPort_t.hpp"

#include <assert.h>
#include <inc/auto_xxx.hxx>
#include "Utils.hpp"
#include <strsafe.h>

// Define this if you want LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

/* ============================== TelnetHandle =============================== */

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to one of the comm ports.
//
class ce::qa::TelnetHandle_t
{
private:

    // Port number and name:
    int   m_PortNumber;
    TCHAR m_pPortName[20];

    // Port handle:
    ce::auto_handle m_PortHandle;

    // Configuration structures:
    DCB          m_DCB;
    COMMTIMEOUTS m_Timeouts;
    DCB          m_OldDCB;
    COMMTIMEOUTS m_OldTimeouts;
    
    // Number connected TelnetPorts:
    int m_NumberConnected;

    // Number TelnetPorts which have opened the port:
    int m_NumberOpened;

    // Synchronization object:
    ce::critical_section m_Locker;

    // Pointer to next handle in list:
    TelnetHandle_t *m_Next;

    // Construction is done by AttachHandle:
    TelnetHandle_t(int PortNumber);

public:

    // Destructor:
   ~TelnetHandle_t(void);

    // Attaches or releases the port connection:
    static TelnetHandle_t *
    AttachHandle(int PortNumber);
    static void
    DetachHandle(const TelnetHandle_t *pHandle);

    // Retrieves an object which can be locked to prevent other threads
    // from using the port:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void) {
        return m_Locker;
    }

    // Connects to the port and configures it for serial communication:
    DWORD
    Open(
        int BaudRate,
        int ByteSize,
        int Parity,
        int StopBits);

    // Closes the port connection:
    DWORD
    Close(void);
    
    // Determines whether the comm-port is open and configured:
    bool
    IsOpened(void) const {
        return m_PortHandle.valid() && INVALID_HANDLE_VALUE != m_PortHandle;
    }
    
    // Reads ASCII from the comm port and inserts it into the specified
    // ASCII or (after translation) Unicode buffer:
    DWORD
    Read(
        ce::string *pBuffer,
        int         MaxCharsToRead,
        int         MaxMillisToWait);
    DWORD
    Read(
        ce::wstring *pBuffer,
        int          MaxCharsToRead,
        int          MaxMillisToWait);

    // If necessary, converts the specified string to ASCII and writes
    // it to the port:
    DWORD
    Write(
        const ce::string &Message);
    DWORD
    Write(
        const ce::wstring &Message);
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
TelnetHandle_t::
TelnetHandle_t(int PortNumber)
    : m_PortNumber(PortNumber),
      m_NumberConnected(1),
      m_NumberOpened(0),
      m_Next(NULL)
{
    HRESULT hr = StringCchPrintf(m_pPortName, COUNTOF(m_pPortName),
                                 TEXT("COM%d:"), m_PortNumber);
    assert(SUCCEEDED(hr));
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
TelnetHandle_t::
~TelnetHandle_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Existing handles:
//

static TelnetHandle_t        *TelnetHandles = NULL;
static ce::critical_section TelnetHandlesLocker;

// ----------------------------------------------------------------------------
//
// Attaches the port connection.
//
TelnetHandle_t *
TelnetHandle_t::
AttachHandle(int PortNumber)
{
    ce::gate<ce::critical_section> locker(TelnetHandlesLocker);

    TelnetHandle_t *hand = TelnetHandles;
    for (;;)
    {
        if (NULL == hand)
        {
            hand = new TelnetHandle_t(PortNumber);
            if (NULL == hand)
            {
                LogError(TEXT("[AC] Can't allocate TelnetHandle for COM%d"), 
                         PortNumber);
            }
            else
            {
                hand->m_Next = TelnetHandles;
                TelnetHandles = hand;
            }
            break;
        }
        if (PortNumber == hand->m_PortNumber)
        {
            hand->m_NumberConnected++;
            break;
        }
        hand = hand->m_Next;
    }

    return hand;
}

// ----------------------------------------------------------------------------
//
// Releases the port connection.
//
void
TelnetHandle_t::
DetachHandle(const TelnetHandle_t *pHandle)
{
    ce::gate<ce::critical_section> locker(TelnetHandlesLocker);

    TelnetHandle_t **parent = &TelnetHandles;
    for (;;)
    {
        TelnetHandle_t *hand = *parent;
        if (NULL == hand)
        {
            assert(NULL == "Tried to detach unknown TelnetHandle");
            break;
        }
        if (hand == pHandle)
        {
            if (--hand->m_NumberConnected <= 0)
            {
               *parent = hand->m_Next;
                delete hand;
            }
            break;
        }
        parent = &(hand->m_Next);
    }
}

// ----------------------------------------------------------------------------
//
// Connects to the port and configures it for serial communication.
//
DWORD
TelnetHandle_t::
Open(
    int BaudRate,
    int ByteSize,
    int Parity,
    int StopBits)
{
    DWORD result;
    
    // If it's already open, just increase the count of "openers".
    if (IsOpened())
    {
        m_NumberOpened++;
        return ERROR_SUCCESS;
    }

    LogDebug(TEXT("[AC] Connecting to \"%s\""), m_pPortName);

    // Open the port.
    m_PortHandle = CreateFile(m_pPortName,
                              GENERIC_READ | GENERIC_WRITE,
                              0,    // must be opened with exclusive-access
                              NULL, // no security attributes
                              OPEN_EXISTING, // must use OPEN_EXISTING
                              0,    // not overlapped I/O
                              NULL  // hTemplate must be NULL for comm devices
                              );
                              
    if (!m_PortHandle.valid() || INVALID_HANDLE_VALUE == m_PortHandle)
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't open %s: %s"), 
                 m_pPortName, Win32ErrorText(result));
        return result;
    }

    // Get the current port configuration settings.
    if (!GetCommState(m_PortHandle, &m_OldDCB))
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't get comm settings for %s: %s"), 
                 m_pPortName, Win32ErrorText(result));
        m_PortHandle.close();
        return result;
    }
    m_DCB = m_OldDCB;

    if (!GetCommTimeouts(m_PortHandle, &m_OldTimeouts))
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't get timeouts for %s: %s"),
                 m_pPortName, Win32ErrorText(result));
        return result;
    }
    m_Timeouts = m_OldTimeouts;

    // Customize the configuration settings.
    m_DCB.BaudRate          = BaudRate;           // Current baud 
    m_DCB.fBinary           = TRUE;               // Binary mode; no EOF check 
    m_DCB.fParity           = TRUE;               // Enable parity checking
    m_DCB.fOutxCtsFlow      = FALSE;              // No CTS output flow ctl 
    m_DCB.fOutxDsrFlow      = FALSE;              // No DSR output flow ctl 
    m_DCB.fDtrControl       = DTR_CONTROL_ENABLE; // DTR flow ctl type 
    m_DCB.fDsrSensitivity   = FALSE;              // DSR sensitivity 
    m_DCB.fTXContinueOnXoff = TRUE;               // XOFF continues Tx 
    m_DCB.fOutX             = FALSE;              // No XON/XOFF out flow ctl 
    m_DCB.fInX              = FALSE;              // No XON/XOFF in flow ctl 
    m_DCB.fErrorChar        = FALSE;              // Disable error replacement
    m_DCB.fNull             = FALSE;              // Disable null stripping
    m_DCB.fRtsControl       = RTS_CONTROL_ENABLE; // RTS flow ctl 
    m_DCB.fAbortOnError     = FALSE;              // Don't abort I/O on error
    m_DCB.ByteSize          = ByteSize;           // Number of bits/bytes, 4-8 
    m_DCB.Parity            = Parity;             // 0-4=no,odd,even,mark,space 
    m_DCB.StopBits          = StopBits;           // 0,1,2 = 1, 1.5, 2 
    if (!SetCommState(m_PortHandle, &m_DCB))
    {
        result = GetLastError();                       
        LogError(TEXT("[AC] Can't set comm settings for %s: %s"), 
                 m_pPortName, Win32ErrorText(result));
        m_PortHandle.close();
        return result;
    }

#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] Connected to %s w/ handle 0x%X"), 
             m_pPortName, (HANDLE)m_PortHandle);
#endif

    m_NumberOpened = 1;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Closes the port connection.
//
DWORD
TelnetHandle_t::
Close(void)
{
    if (IsOpened() && --m_NumberOpened <= 0)
    {
        // Reset the port configuration.
        if (!SetCommTimeouts(m_PortHandle, &m_OldTimeouts)
         || !SetCommState   (m_PortHandle, &m_OldDCB))
        {
            LogWarn(TEXT("[AC] Can't reset comm settings for %s: %s"), 
                     m_pPortName, Win32ErrorText(GetLastError()));
        }

        // Close the port.
        m_PortHandle.close();
        LogDebug(TEXT("[AC] Disconnected \"%s\""), m_pPortName);
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Reads ASCII from the comm port and inserts it into the specified
// ASCII or (after translation) Unicode buffer.
//
DWORD
TelnetHandle_t::
Read(
    ce::string *pBuffer,
    int         MaxCharsToRead,
    int         MaxMillisToWait)
{
    DWORD result;

    if (!IsOpened())
        return ERROR_INVALID_HANDLE;

    // Set the comm-timeouts.
    m_Timeouts.ReadIntervalTimeout        = MaxMillisToWait / 2;
    m_Timeouts.ReadTotalTimeoutMultiplier = 0;
    m_Timeouts.ReadTotalTimeoutConstant   = MaxMillisToWait;
    if (!SetCommTimeouts(m_PortHandle, &m_Timeouts))
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't set read timeouts for %s: %s"),
                 m_pPortName, Win32ErrorText(result));
        return result;
    }
    
    // Read the ASCII data.
    ce::string readBuffer;
    if (!readBuffer.reserve(MaxCharsToRead))
        return ERROR_OUTOFMEMORY;
    DWORD readed;
    if (!ReadFile(m_PortHandle, readBuffer.get_buffer(), 
                                readBuffer.capacity(), &readed, NULL))
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't read from %s: %s"),
                 m_pPortName, Win32ErrorText(result));
        return result;
    }

    // Copy the data to the output buffer.
    if (0 == readed)
    {
        pBuffer->clear();
    }
    else
    if (!pBuffer->assign(readBuffer.get_buffer(), readed))
    {
        return ERROR_OUTOFMEMORY;
    }

    return ERROR_SUCCESS;
}

DWORD
TelnetHandle_t::
Read(
    ce::wstring *pBuffer,
    int          MaxCharsToRead,
    int          MaxMillisToWait)
{
    // Read the ASCII data.
    ce::string mbBuffer;
    DWORD result = Read(&mbBuffer, MaxCharsToRead, MaxMillisToWait);
    if (ERROR_SUCCESS != result)
        return result;

    // Convert to Unicode.
    HRESULT hr = WiFUtils::ConvertString(pBuffer, mbBuffer, NULL,
                                                  mbBuffer.length());
    if (FAILED(hr))
    {
        LogError(TEXT("[AC] Can't convert ASCII to Unicode: %s"),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }           

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// If necessary, converts the specified string to ASCII and writes
// it to the port.
//
DWORD
TelnetHandle_t::
Write(
    const ce::string &Message)
{
    DWORD result;

    if (!IsOpened())
        return ERROR_INVALID_HANDLE;

#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] writing \"%hs\" (%d bytes) to %s w/ handle 0x%X"),
            &Message[0], Message.length(), 
            m_pPortName, (HANDLE)m_PortHandle);
#endif

    // Write the ASCII data.
    ce::string &mutableMessage = const_cast<ce::string &>(Message);
    DWORD written;
    if (!WriteFile(m_PortHandle, mutableMessage.get_buffer(), 
                                 mutableMessage.length(), &written, NULL))
    {
        result = GetLastError();
        LogError(TEXT("[AC] Can't write to %s: %s"),
                 m_pPortName, Win32ErrorText(result));
        return result;
    }

    // Make sure it was all written.
    if (Message.length() != written)
    {
        LogError(TEXT("[AC] Can't write to %s wrote %u - wanted %u"),
                 m_pPortName, written, Message.length());
        return ERROR_WRITE_FAULT;
    }
    
    return ERROR_SUCCESS;
}

DWORD
TelnetHandle_t::
Write(
    const ce::wstring &Message)
{
    // Convert to ASCII.
    ce::string mbBuffer;
    HRESULT hr = WiFUtils::ConvertString(&mbBuffer, Message, NULL,
                                                    Message.length());
    if (FAILED(hr))
    {
        LogError(TEXT("[AC] Can't convert Unicode to ASCII: %s"),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    // Write the ASCII data.
    return Write(mbBuffer);
}

/* =============================== TelnetPort ================================ */

// ----------------------------------------------------------------------------
//
// Constructor.
//
TelnetPort_t::
TelnetPort_t(int PortNumber)
    : m_pHandle(TelnetHandle_t::AttachHandle(PortNumber)),
      m_Opened(false)
{
    assert(NULL != m_pHandle);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
TelnetPort_t::
~TelnetPort_t(void)
{
    if (NULL != m_pHandle)
    {
        Close();
        TelnetHandle_t::DetachHandle(m_pHandle);
        m_pHandle = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Retrieves an object which can be locked to prevent other threads
// from using the port.
//
ce::critical_section &
TelnetPort_t::
GetLocker(void)
{
    return m_pHandle->GetLocker();
}

// ----------------------------------------------------------------------------
//
// Connects to the port and configures it for serial communication.
//
DWORD
TelnetPort_t::
Open(
    int BaudRate,
    int ByteSize,
    int Parity,
    int StopBits)
{
    DWORD result = ERROR_SUCCESS;
    if (!m_Opened)
    {
        if (NULL == m_pHandle)
        {
            result = ERROR_OUTOFMEMORY;
        }
        else
        {
            result = m_pHandle->Open(BaudRate, ByteSize, Parity, StopBits);
            if (ERROR_SUCCESS == result)
            {
                m_Opened = true;
            }
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Closes the port connection.
//
DWORD
TelnetPort_t::
Close(void)
{
    DWORD result = ERROR_SUCCESS;
    if (m_Opened)
    {
        result = (NULL == m_pHandle)? ERROR_OUTOFMEMORY 
                                    : m_pHandle->Close();
        m_Opened = false;
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Determines whether the comm port is opened and configured.
//
bool
TelnetPort_t::
IsOpened(void) const
{
    return (NULL != m_pHandle && m_pHandle->IsOpened());
}

// ----------------------------------------------------------------------------
//
// Reads ASCII from the comm port and inserts it into the specified
// ASCII or (after translation) Unicode buffer.
//
DWORD
TelnetPort_t::
Read(
    ce::string *pBuffer,
    int         MaxCharsToRead,
    int         MaxMillisToWait)
{
    return (NULL == m_pHandle)? ERROR_OUTOFMEMORY
                              : m_pHandle->Read(pBuffer, 
                                                MaxCharsToRead, 
                                                MaxMillisToWait);
}
DWORD
TelnetPort_t::
Read(
    ce::wstring *pBuffer,
    int          MaxCharsToRead,
    int          MaxMillisToWait)
{
    return (NULL == m_pHandle)? ERROR_OUTOFMEMORY
                              : m_pHandle->Read(pBuffer, 
                                                MaxCharsToRead, 
                                                MaxMillisToWait);
}

// ----------------------------------------------------------------------------
//
// If necessary, converts the specified string to ASCII and writes
// it to the port.
//
DWORD
TelnetPort_t::
Write(
    const ce::string &Message)
{
    return (NULL == m_pHandle)? ERROR_OUTOFMEMORY
                              : m_pHandle->Write(Message);
}
DWORD
TelnetPort_t::
Write(
    const ce::wstring &Message)
{
    return (NULL == m_pHandle)? ERROR_OUTOFMEMORY
                              : m_pHandle->Write(Message);
}

// ----------------------------------------------------------------------------

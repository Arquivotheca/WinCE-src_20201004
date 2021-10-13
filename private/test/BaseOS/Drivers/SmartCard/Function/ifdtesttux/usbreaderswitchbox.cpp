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
#include "USBReaderSwitchBox.h"

USBReaderSwitchBox::USBReaderSwitchBox()
{
    m_hSerial = INVALID_HANDLE_VALUE;
}

USBReaderSwitchBox::~USBReaderSwitchBox()
{
    Close();
}

//***************************************************************************
// See header file for description.
//***************************************************************************
BOOL USBReaderSwitchBox::Initialize(BYTE in_dwPortNum)
{
    const UCHAR MAX_NAME_LENGTH = 6;
    TCHAR l_rgchDeviceName[MAX_NAME_LENGTH];
  
    // If port number is not in allowed range, attempt to use the default.
    if (in_dwPortNum >= MAX_COMM_PORT_NUM)
    {
        in_dwPortNum = 1;
    }
    StringCchPrintf(
                (LPTSTR) l_rgchDeviceName, 
                MAX_NAME_LENGTH,
                L"COM%d:", 
                in_dwPortNum
                );

    // If we're already open, then cleanup first.
    if (INVALID_HANDLE_VALUE != m_hSerial)
    {
        Close();
    }

    // Try to open the serial port.
    m_hSerial = CreateFile (l_rgchDeviceName,
                            GENERIC_WRITE, // Access mode
                            0,             // Share mode
                            NULL,          // Pointer to the security attribute
                            OPEN_EXISTING, // How to open the serial port
                            0,             // Port attributes
                            NULL);         // Handle to port with attribute
                                           // to copy

    if (INVALID_HANDLE_VALUE == m_hSerial)
    {
        DEBUGMSG(1,
                 (L"ERROR USBReaderSwitchBox: Failed to open serial port [%s].",
                 l_rgchDeviceName));
        return FALSE;
    }

    DCB l_dcb;
    l_dcb.DCBlength = sizeof(l_dcb);
    if (!GetCommState(m_hSerial, &l_dcb))
    {
        DEBUGMSG(1, (L"ERROR USBReaderSwitchBox: Failed to get comm state."));
        goto cleanup;
    }
    l_dcb.BaudRate = CBR_57600;
    l_dcb.ByteSize = 8;
    l_dcb.Parity   = NOPARITY;
    l_dcb.StopBits = ONESTOPBIT;

    if (!SetCommState(m_hSerial, &l_dcb))
    {
        DEBUGMSG(1, (L"ERROR USBReaderSwitchBox: Failed to set comm state."));
        goto cleanup;
    }

    COMMTIMEOUTS l_cto;
    if (!GetCommTimeouts(m_hSerial, &l_cto))
    {
        DEBUGMSG(1, (L"ERROR USBReaderSwitchBox: Failed to get comm timeouts."));
        goto cleanup;
    }
    l_cto.WriteTotalTimeoutMultiplier = 10;
    l_cto.WriteTotalTimeoutConstant   = 1000;
    if (!SetCommTimeouts(m_hSerial, &l_cto))
    {
        DEBUGMSG(1, (L"ERROR USBReaderSwitchBox: Failed to set comm timeouts."));
        goto cleanup;
    }

    // Initialize all ports as off.
    if (ERROR_SUCCESS == Configure(0,0,0,0))
    {
        return TRUE;
    }
    

cleanup:
    Close();
    return FALSE;   
}

//***************************************************************************
// See header file for description.
//***************************************************************************
void USBReaderSwitchBox::Close()
{
    if (INVALID_HANDLE_VALUE != m_hSerial)
    {

        if (!CloseHandle(m_hSerial))
        {
            DEBUGMSG(1, (L"ERROR USBReaderSwitchBox: Problems closing m_hSerial."));
        }

        m_hSerial = INVALID_HANDLE_VALUE;
    }
}
//***************************************************************************
// See header file for description.
//***************************************************************************
DWORD USBReaderSwitchBox::Configure(const BYTE in_dwPortA, 
                                    const BYTE in_dwPortB, 
                                    const BYTE in_dwPortC,
                                    const BYTE in_dwPortD)
{
    // First, a sanity check
    if (INVALID_HANDLE_VALUE == m_hSerial)
    {
        return ERROR_BAD_ARGUMENTS;
    }

    DWORD l_dwTmp = 0;
    BYTE l_rgbTmp[2] = {0, 0};
    
    // Set up the bitmasks appropriately
    switch (in_dwPortA)
    {
    case 1:
        l_rgbTmp[0] |= PORT_A1; break;
    case 2:
        l_rgbTmp[0] |= PORT_A2; break;
    case 3:
        l_rgbTmp[0] |= PORT_A3; break;
    case 4:
        l_rgbTmp[0] |= PORT_A4; break;
    default:
        l_rgbTmp[0] |= 0;
    }

    switch (in_dwPortB)
    {
    case 1:
        l_rgbTmp[0] |= PORT_B1; break;
    case 2:
        l_rgbTmp[0] |= PORT_B2; break;
    case 3:
        l_rgbTmp[0] |= PORT_B3; break;
    case 4:
        l_rgbTmp[0] |= PORT_B4; break;
    default:
        l_rgbTmp[0] |= 0;
    }

    switch (in_dwPortC)
    {
    case 1:
        l_rgbTmp[1] |= PORT_C1; break;
    case 2:
        l_rgbTmp[1] |= PORT_C2; break;
    case 3:
        l_rgbTmp[1] |= PORT_C3; break;
    case 4:
        l_rgbTmp[1] |= PORT_C4; break;
    default:
        l_rgbTmp[1] |= 0;
    }

    switch (in_dwPortD)
    {
    case 1:
        l_rgbTmp[1] |= PORT_D1; break;
    case 2:
        l_rgbTmp[1] |= PORT_D2; break;
    case 3:
        l_rgbTmp[1] |= PORT_D3; break;
    case 4:
        l_rgbTmp[1] |= PORT_D4; break;
    default:
        l_rgbTmp[1] |= 0;
    }


    if (!SetCommMask(m_hSerial, EV_TXEMPTY))
    {
        DEBUGMSG(1, (L"ERROR: SetCommMask failed, %d", GetLastError()));
        return GetLastError();
    }

    if (!WriteFile(m_hSerial,
                   l_rgbTmp,
                   sizeof(l_rgbTmp),
                   &l_dwTmp, 
                   0)
        || sizeof(l_rgbTmp) != l_dwTmp)
    {
        DEBUGMSG(1, (L"ERROR: Could not write to COM port"));
        return GetLastError();
    }

    if (!WaitCommEvent(m_hSerial, &l_dwTmp, 0))
    {
        DEBUGMSG(1, (L"ERROR: WaitCommEvent failed, %d", GetLastError()));
        return GetLastError();
    }

    return ERROR_SUCCESS;   
}
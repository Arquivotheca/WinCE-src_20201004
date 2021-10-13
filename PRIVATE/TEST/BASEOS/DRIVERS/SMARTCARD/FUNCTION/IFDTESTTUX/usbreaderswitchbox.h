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
#include <windows.h>

class USBReaderSwitchBox 
{
public:
    USBReaderSwitchBox();
    ~USBReaderSwitchBox();

    //************************************************************************
    // Purpose:  Opens a serial port connection to communicate with the
    //           USB Switch Box.
    //
    // Arguments:
    //   __in  DWORD in_dwPortNum - COM number used by the serial port.
    //
    // Returns:
    //   BOOL - TRUE if success; FALSE otherwise.
    //************************************************************************
    BOOL Initialize(BYTE in_dwPortNum);

    //************************************************************************
    // Purpose:  Configure the USB Switch Box ports.
    //
    // Arguments:
    //   BYTE in_dwPortA - Which of A1-A4 to connect [1..4].
    //                     A value of 0 will disconnect all A ports.
    //   BYTE in_dwPortB - Which of B1-B4 to connect [1..4].
    //                     A value of 0 will disconnect all B ports.
    //   BYTE in_dwPortC - Which of C1-C4 to connect [1..4].
    //                     A value of 0 will disconnect all C ports.
    //   BYTE in_dwPortD - Which of D1-D4 to connect [1..4].
    //                     A value of 0 will disconnect all D ports.
    //
    // Returns:
    //   DWORD - ERROR_SUCCESS if successful, a Win32 error code otherwise.
    //************************************************************************
    DWORD Configure(const BYTE in_dwPortA, 
                    const BYTE in_dwPortB, 
                    const BYTE in_dwPortC,
                    const BYTE in_dwPortD);

    //************************************************************************
    // Purpose:  Closes the serial connection to the USB switch box.
    //
    // Arguments:
    //   none
    //
    // Returns:
    //   void
    //************************************************************************
    void Close();
  

private: 
    HANDLE m_hSerial;

    enum{PORT_A1=0x0C, PORT_A2=0x05, PORT_A3=0x06, PORT_A4=0x0F,
         PORT_B1=0xC0, PORT_B2=0x50, PORT_B3=0x60, PORT_B4=0xF0,
         PORT_C1=0x0C, PORT_C2=0x05, PORT_C3=0x06, PORT_C4=0x0F,
         PORT_D1=0xC0, PORT_D2=0x50, PORT_D3=0x60, PORT_D4=0xF0};

    // Assume the serial port that communicates with the USBReaderSwitchBox
    // will be between COM1: - COM0: (recall WinCE numbering convention).
    enum{MAX_COMM_PORT_NUM=10};
};
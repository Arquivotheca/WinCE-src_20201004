// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

// contents: comm port test utility functions

#include "UTIL.H"

const UINT     MAX_DEVICE_INDEX =      9;
const UINT     MAX_DEVICE_NAMELEN =    16;
const TCHAR    SZ_COMM_PORT_FMT[] =    TEXT("COM%u:");

// BAUD RATES
//
const DWORD    BAUD_RATES[] =   
{
    BAUD_110, BAUD_300, BAUD_600, BAUD_1200, BAUD_2400, BAUD_4800, BAUD_9600, 
    BAUD_14400, BAUD_19200, BAUD_38400, BAUD_56K, BAUD_57600, BAUD_115200, 
    BAUD_128K 
};
const LPTSTR   SZ_BAUD_RATES[] =
{
    TEXT("110"), TEXT("300"), TEXT("600"), TEXT("1200"), TEXT("2400"), TEXT("4800"), 
    TEXT("9600"), TEXT("14400"), TEXT("19200"), TEXT("38400"), TEXT("56K"), 
    TEXT("57600"),  TEXT("115200"), TEXT("128K")
};
const DWORD    BAUD_RATES_VAL[] =
{
    CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800, CBR_9600, CBR_14400,
    CBR_19200, CBR_38400, CBR_56000, CBR_57600, CBR_115200, CBR_128000
};
const UINT     NUM_BAUD_RATES =        sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);

// DATA BITS
//
const WORD     DATA_BITS[] =
{
    DATABITS_5, DATABITS_6, DATABITS_7, DATABITS_8, DATABITS_16
};
const LPTSTR   SZ_DATA_BITS[] =
{
    TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("16")
};
const BYTE     DATA_BITS_VAL[] = 
{
    5, 6, 7, 8, 16
};
const UINT     NUM_DATA_BITS =         sizeof(DATA_BITS) / sizeof(DATA_BITS[0]);

// STOP BITS
//
const WORD     STOP_BITS[] =
{
    STOPBITS_10, STOPBITS_15, STOPBITS_20
};
const LPTSTR   SZ_STOP_BITS[] =
{
    TEXT("1"), TEXT("1.5"), TEXT("2")
};
const BYTE     STOP_BITS_VAL[] = 
{
    ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS
};
const UINT     NUM_STOP_BITS =         sizeof(STOP_BITS) / sizeof(STOP_BITS[0]);

// PARITY
//
const WORD     PARITY[] =
{
    PARITY_NONE, PARITY_ODD, PARITY_EVEN, PARITY_MARK, PARITY_SPACE
};
const LPTSTR   SZ_PARITY[] = 
{
    TEXT("No"), TEXT("Odd"), TEXT("Even"), TEXT("Mark"), TEXT("Space")
};
const BYTE     PARITY_VAL[] =
{
    NOPARITY, ODDPARITY, EVENPARITY, MARKPARITY, SPACEPARITY
};
const UINT     NUM_PARITY =            sizeof(PARITY) / sizeof(PARITY[0]);

// PROVIDER CAPABILITIES
//
const DWORD    PROVIDER_CAPS[] =
{
    PCF_16BITMODE, PCF_DTRDSR, PCF_INTTIMEOUTS, PCF_PARITY_CHECK, PCF_RLSD,
    PCF_RTSCTS, PCF_SETXCHAR, PCF_SPECIALCHARS, PCF_TOTALTIMEOUTS, PCF_XONXOFF
};
const LPTSTR   SZ_PROVIDER_CAPS[] = 
{
    TEXT("Special 16-Bit Mode"), TEXT("DTR/DSR"), TEXT("Interval time-outs"),
    TEXT("Parity Checking"), TEXT("RLSD"), TEXT("RTS/CTS"), TEXT("Settable XON/XOFF"),
    TEXT("Special character support"), TEXT("Total time-outs supported"), 
    TEXT("XON/XOFF flow control")
};
const UINT     NUM_PROVIDER_CAPS =   sizeof(PROVIDER_CAPS) / sizeof(PROVIDER_CAPS[0]);

// SETTABLE PARAMETERS
//
const DWORD    SETTABLE_PARAMS[] = 
{
    SP_BAUD, SP_DATABITS, SP_HANDSHAKING, SP_PARITY, SP_PARITY_CHECK, SP_RLSD, 
    SP_STOPBITS
};
const LPTSTR   SZ_SETTABLE_PARAMS[] = 
{
    TEXT("Baud Rate"), TEXT("Data Bits"), TEXT("Handshaking"), TEXT("Parity"),
    TEXT("Parity checking"), TEXT("RLSD"), TEXT("Stop Bits")
};
const UINT     NUM_SETTABLE_PARAMS =   sizeof(SETTABLE_PARAMS) / sizeof(SETTABLE_PARAMS[0]);

// COMM EVENTS for SetCommMask()
//
const DWORD    COMM_EVENTS[] =
{
    EV_BREAK, EV_CTS, EV_DSR, EV_ERR, EV_RING, EV_RLSD, EV_RXCHAR, EV_RXFLAG, EV_TXEMPTY
};
const LPTSTR   SZ_COMM_EVENTS[] = 
{
    TEXT("Break"), TEXT("CTS"), TEXT("DSR"), TEXT("Error"), TEXT("Ring"), TEXT("RLSD"), 
    TEXT("RxChar"), TEXT("RxFlag"), TEXT("TxEmpty")
};
const UINT     NUM_COMM_EVENTS =       sizeof(COMM_EVENTS) / sizeof(COMM_EVENTS[0]);

// COMM FUNCTIONS for EscapeCommFunction()
//
const DWORD    COMM_FUNCTIONS[] =
{
    SETIR, CLRIR, CLRDTR, CLRRTS, SETRTS, SETXOFF, SETXON, SETBREAK, CLRBREAK
};
const LPTSTR   SZ_COMM_FUNCTIONS[] = 
{
    TEXT("SETIR"), TEXT("CLRIR"), TEXT("CLRDTR"), TEXT("CLRRTS"), TEXT("SETRTS"), 
    TEXT("SETXOFF"), TEXT("SETXON"), TEXT("SETBREAK"), TEXT("CLRBREAK")
};
const UINT     NUM_COMM_FUNCTIONS =    sizeof(COMM_FUNCTIONS) / sizeof(COMM_FUNCTIONS[0]);

// FUNCITONS
//

// --------------------------------------------------------------------
HANDLE Util_OpenCommPort(UINT unCommPort)
// --------------------------------------------------------------------
{
    TCHAR szCommPort[MAX_DEVICE_NAMELEN] = TEXT("");
    
    _sntprintf(szCommPort, MAX_DEVICE_NAMELEN, SZ_COMM_PORT_FMT, unCommPort);

    return CreateFile(szCommPort, GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

// --------------------------------------------------------------------
UINT Util_QueryCommPortCount(VOID)
// --------------------------------------------------------------------
{
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    UINT unIndex = 0;
    
    // try to open comm port at each index, return index of first failure
    for(unIndex = 1; unIndex <= MAX_DEVICE_INDEX; unIndex++)
    {
        hCommPort = Util_OpenCommPort(unIndex);
        if(INVALID_HANDLE(hCommPort))
        {
            break;
        }
        CloseHandle(hCommPort);
    }
    return unIndex-1;
}

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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
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
typedef struct _PROVIDER_CAPABILITIES_STRUCT {
    DWORD   dwProvCapabilities;
    LPCTSTR szProvCapsNames;
    LPCTSTR szProvCapsFriendlyNames; // Not currently displayed
} PROVIDER_CAPABILITIES_STRUCT;

PROVIDER_CAPABILITIES_STRUCT p_c_s[] = 
{
    { PCF_16BITMODE,     TEXT("PCF_16BITMODE"),     TEXT("Special 16-Bit Mode")       },
    { PCF_DTRDSR,        TEXT("PCF_DTRDSR"),        TEXT("DTR/DSR")                   },
    { PCF_INTTIMEOUTS,   TEXT("PCF_INTTIMEOUTS"),   TEXT("Interval time-outs")        },
    { PCF_PARITY_CHECK,  TEXT("PCF_PARITY_CHECK")   TEXT("Parity Checking")           },
    { PCF_RLSD,          TEXT("PCF_RLSD"),          TEXT("RLSD")                      },
    { PCF_RTSCTS,        TEXT("PCF_RTSCTS"),        TEXT("RTS/CTS")                   },
    { PCF_SETXCHAR,      TEXT("PCF_SETXCHAR"),      TEXT("Settable XON/XOFF")         },
    { PCF_SPECIALCHARS,  TEXT("PCF_SPECIALCHARS"),  TEXT("Special character support") },
    { PCF_TOTALTIMEOUTS, TEXT("PCF_TOTALTIMEOUTS"), TEXT("Total time-outs supported") },
    { PCF_XONXOFF,       TEXT("PCF_XONXOFF"),       TEXT("XON/XOFF flow control")     }
};

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
    TEXT("Break"),
    TEXT("CTS"),
    TEXT("DSR"),
    TEXT("Error"),
    TEXT("Ring"),
    TEXT("RLSD"), 
    TEXT("RxChar"),
    TEXT("RxFlag"),
    TEXT("TxEmpty")
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

BOOL QueryKeyValue(__in HKEY hKey, __in LPCTSTR lpValueName, __out_opt LPBYTE lpData, __inout_opt LPDWORD lpcbData)
{
    BOOL rv = FALSE;
    HRESULT hr = 0;
    DWORD  cbValueSize = 0;

    if (hKey == NULL ||
        lpValueName == NULL ||
        lpData == NULL ||
        lpcbData == NULL)
    {
        return rv;
    }

    hr=RegQueryValueEx(hKey, lpValueName, NULL, NULL, NULL, &cbValueSize);
    if (hr != ERROR_SUCCESS || cbValueSize == 0)
    {
        return rv;
    }

    cbValueSize += sizeof(TCHAR);
    lpData = (LPBYTE) malloc(cbValueSize);
    if (lpData == NULL)
    {
        return rv;
    }

    *lpcbData = cbValueSize;
    hr=RegQueryValueEx(hKey, lpValueName, NULL, NULL, lpData, lpcbData);
    if(hr == ERROR_SUCCESS && *lpcbData < cbValueSize)
    {
        rv = TRUE;
    }

    return rv;
}

// --------------------------------------------------------------------
HANDLE Util_OpenCommPort(INT unCommPort)
// --------------------------------------------------------------------
{
        
    TCHAR szCommPort[MAX_DEVICE_NAMELEN] = {NULL};

    INT commPort = (COMx == -1) ? unCommPort : COMx;
    
    //Trying to Open a IR COMM Port 
    if(commPort == irCOMx)
    {
         g_pKato->Log( LOG_FAIL, TEXT("FAIL:COM%u is IR COMM Port, it is not Valid port for the SerDrvBVT Test \n"), commPort);
         return INVALID_HANDLE_VALUE;  
    }
    
    swprintf_s(szCommPort, _countof(szCommPort), SZ_COMM_PORT_FMT, commPort);

    //to remove the buffer overrun prefast bug
    szCommPort[MAX_DEVICE_NAMELEN-1]=NULL;
     
    HANDLE handle =  CreateFile(szCommPort, 
                                GENERIC_READ | GENERIC_WRITE, 
                                0, 
                                NULL,
                                OPEN_EXISTING, 
                                FILE_ATTRIBUTE_NORMAL, 
                                NULL);
    if(INVALID_HANDLE_VALUE != handle)
    {
        // We only test valid, connected serial ports
        DWORD dwErrors = 0;
        if(0 == ClearCommError(handle, &dwErrors, NULL))
        {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }

    return handle;

}

// --------------------------------------------------------------------
UINT Util_QueryCommPortCount(VOID)
// --------------------------------------------------------------------
{    
    if(COMx!=-1)
        return 1;
    
    else
        return MAX_DEVICE_INDEX;

}


/*++
Util_QueryCommIRCOMMPort:

Arguments:

Return Value:
TRUE for success else FALSE

Author:
Unknown(unknown)
 
Notes:
Checks if a COM port associated with IrCOMM 
sets the irCOMx to the corresponding Serial Port
--*/

UINT Util_QueryCommIRCOMMPort(VOID)
{
    BOOL bRtn   = TRUE;
    BOOL retval = FALSE;
    HKEY hActive = NULL, 
         hDriver = NULL;
    HRESULT hr = S_FALSE;
    DWORD i = 0;
    DWORD dwPort = 0,
          dwSizeDriverName = 0, 
          dwSizeKeyName = 0, 
          dwCOMSize = 0;
    TCHAR szKeyName[MAX_PATH] = {0};
    TCHAR szDriverName[MAX_PATH] = {0};

    LOG(TEXT("**Find  IR COMM Serial port**"));

    //is there an  IRCOMM Serial driver available?
    hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, IR_COMM_PORT_CLASS, 0, 0, &hActive);
    RegCloseKey(hActive);
    if(hr != ERROR_SUCCESS)
    {
        LOG(TEXT("**COMM IR Port Not Present"));
        bRtn = FALSE;
    }
    else
    {
        //find our IR COMM Serial port.
        hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_HKEY_ACTIVE, 0, 0, &hActive);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("Unable to open REG_HKEY_ACTIVE, assuming no IR COMM ports");
            bRtn = FALSE;
        }
    }

    while(bRtn)
    {
        dwSizeDriverName=MAX_PATH;
        hr=RegEnumKeyEx(hActive, i, szDriverName, &dwSizeDriverName, NULL, NULL, NULL, NULL);

        if(hr==(HRESULT)ERROR_NO_MORE_ITEMS)
            break;

        if(hr != ERROR_SUCCESS)
        {
            LOG(TEXT("**COMM IR Port Not Present"));
            break;
        }

        if(hDriver)
        {
            CloseHandle(hDriver);
            hDriver = NULL;
        }
        
        hr=RegOpenKeyEx(hActive, szDriverName, 0, 0, &hDriver);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("Error opening REG_HKEY_ACTIVE's driver sub keys, assuming no IR COMM ports");
            break;
        }

        dwSizeKeyName=sizeof(szDriverName);
        hr=RegQueryValueEx(hDriver,TEXT("Key"), NULL, NULL, (PBYTE)szKeyName, &dwSizeKeyName);
        szKeyName[MAX_PATH - 1] = L'\0';

        if(hr==ERROR_SUCCESS)
        {
            if(_tcscmp(IR_COMM_PORT_CLASS,szKeyName)==0) //we have a matching IRCOMM  device
            {
                LONG lRet = 0;
                HKEY hPort = NULL;

                lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IR_COMM_PORT_CLASS, 0, 0, &hPort);
                if (ERROR_SUCCESS != lRet || NULL == hPort)
                {
                    continue;
                }

                //get IRCOMM COMn:
                dwCOMSize=sizeof(dwCOMSize);

                lRet=RegQueryValueEx(hPort, TEXT("Index"), NULL, NULL, (LPBYTE)&dwPort, &dwCOMSize);
                CloseHandle(hPort);

                if(lRet == ERROR_SUCCESS)
                {
                    LOG(TEXT("ProcessCmdLine: IRCOMM Serial Port found at %d\n"), dwPort);  

                    if(dwPort>=0 && dwPort <=9)
                        irCOMx = dwPort;

                    retval=TRUE;
                    break;
                }
            }
        }

        i++;//Increment the Index of the Index of the Subkey 
    }

    if(hActive)
    {
        RegCloseKey(hActive);
    }

    if(hDriver)
    {
        RegCloseKey(hDriver);
    }

    return retval;
}



/*++
Util_QueryCommIRRAWPort:

Arguments:

Return Value:
TRUE for success else FALSE

Author:
Unknown (unknown)
 
Notes:
Search for the RAW Ir Cort and set Comx to the corresponding port 
--*/
UINT Util_QueryCommIRRAWPort(VOID)
{
    
    BOOL bRtn   = TRUE ;
    BOOL retval = FALSE;
    HKEY hActive = NULL, 
         hDriver = NULL;
    HRESULT hr = S_FALSE;
    DWORD i = 0;
    DWORD dwSizeDriverName = 0, 
          dwSizeKeyName = 0, 
          dwCOMSize = 0,
          dwPort = 0;
    TCHAR szKeyName[MAX_PATH] = {0};
    TCHAR szDriverName[MAX_PATH] = {0};
    
    LOG(TEXT("**Find RAW IR  port**"));
    //is there an RAW IR Port ?

    hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, IR_RAW_PORT_CLASS, 0, 0, &hActive);
    RegCloseKey(hActive);
    hActive = NULL;

    if(hr != ERROR_SUCCESS)
    {
        LOG(TEXT("**RAW IR Port Not Present "));
        bRtn = FALSE;
    }
    else
    {
        //find our RAW IR Serial port.
        hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_HKEY_ACTIVE, 0, 0, &hActive);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("RAW IR Port Not Active");
            bRtn = FALSE;
        }
    }

    while(bRtn)
    {
        dwSizeDriverName=MAX_PATH;
        hr=RegEnumKeyEx(hActive, i, szDriverName, &dwSizeDriverName, NULL, NULL, NULL, NULL);
                        
        if(hr==(HRESULT)ERROR_NO_MORE_ITEMS)
            break;
                        
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("RegEnumKeyEx() failed when enumerating the REG_HKEY_ACTIVE key");
            break;
        }
        
        hr=RegOpenKeyEx(hActive, szDriverName, 0, 0, &hDriver);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("RegOpenKeyEx() failed to open a REG_HKEY_ACTIVE sub key");
            break;
        }
                
        dwSizeKeyName=sizeof(szDriverName);
        hr=RegQueryValueEx(hDriver, TEXT("Key"), NULL, NULL, (PBYTE)szKeyName, &dwSizeKeyName);
        szKeyName[MAX_PATH - 1] = L'\0';
        if(hr==ERROR_SUCCESS)
        {
            if(_tcscmp(IR_RAW_PORT_CLASS,szKeyName)==0) //we have a matching RAW IR  serial device
            {
                //get COMn of IR Serial Port
                dwCOMSize=sizeof(dwPort);
                hr=RegQueryValueEx(hDriver,TEXT("Port"), NULL, NULL,(LPBYTE)&dwPort, &dwCOMSize);
                if(hr == ERROR_SUCCESS)
                {
                    NKDbgPrintfW(L"ProcessCmdLine: RAW IR  Serial Port found at %s\n", dwPort);  
                    if(dwPort>=0 && dwPort <=9)
                        COMx = dwPort;

                    retval=TRUE;
                    break;
                }
            }
        }

        RegCloseKey(hDriver);
        hDriver = NULL;

        i++;//Increment the Index of the Index of the Subkey 
    }

    if(hActive)
    {
        RegCloseKey(hActive);
    }

    if(hDriver)
    {
        RegCloseKey(hDriver);
    }

    return retval;

}


/*++
Util_QueryUSBSerialPort:
Arguments:

Return Value:
TRUE for success else FALSE

Author:
Unknown (unknown)
 
Notes:
Search for USB Serial Port and set Comx to the corresponding port 

--*/
UINT Util_QueryUSBSerialPort(VOID)
{
    BOOL bRtn   = TRUE ;
    BOOL retval = FALSE;
    HKEY hActive = NULL, 
         hDriver = NULL;
    HRESULT hr;
    DWORD i = 0,
          dwSizeDriverName = 0,
          dwSizeKeyName = 0;
    TCHAR* pszKeyName = NULL;
    TCHAR szDriverName[MAX_PATH];

    //Is there an USBFN Serial driver available?
    LOG(TEXT("**Find USBFN Serial port**"));
    
    hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, USBFN_SERIAL_CLASS, 0, 0, &hActive);
    RegCloseKey(hActive);
    hActive = NULL;

    if(hr != ERROR_SUCCESS)
    {
        ERRFAIL("USB FN Serial Not Supported in the Device");
        bRtn = FALSE;
    }
    else
    {
        //Find our USBFN Serial port.
        hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE, REG_HKEY_ACTIVE, 0, 0, &hActive);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("Unable to open REG_HKEY_ACTIVE and find the USBFN Serial port");
            bRtn = FALSE;
        }
    }

    while(bRtn)
    {
        dwSizeDriverName=MAX_PATH;
        hr=RegEnumKeyEx(hActive, i, szDriverName, &dwSizeDriverName, NULL, NULL, NULL, NULL);
            
        if(hr==(HRESULT)ERROR_NO_MORE_ITEMS)
            break;
                        
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("ProcessCmdLine - RegEnumKeyEx(REG_HKEY_ACTIVE)");
            retval = NULL;
            break;

        }
        
        hr=RegOpenKeyEx(hActive, szDriverName, 0, 0, &hDriver);
        if(hr != ERROR_SUCCESS)
        {
            ERRFAIL("ProcessCmdLine - RegOpenKeyEx(REG_HKEY_ACTIVE - DriverName)");
            retval = NULL;
            break;
        }
            
        if(QueryKeyValue(hDriver, TEXT("Key"), (PBYTE)pszKeyName, &dwSizeKeyName))
        {
            if(_tcscmp(USBFN_SERIAL_CLASS, pszKeyName)==0) //we have a matching USBFN serial device
            {
                free(pszKeyName);
                pszKeyName = NULL;

                //get USBFN COMn:
                if(QueryKeyValue(hDriver, TEXT("Name"), (PBYTE)pszKeyName, &dwSizeKeyName))
                {
                    NKDbgPrintfW(L"ProcessCmdLine: USBFN Serial Port found at %s\n", pszKeyName);  
                    if(_tcscmp(COM0, pszKeyName)==0) COMx = 0;
                    if(_tcscmp(COM1, pszKeyName)==0) COMx = 1;
                    if(_tcscmp(COM2, pszKeyName)==0) COMx = 2;
                    if(_tcscmp(COM3, pszKeyName)==0) COMx = 3;
                    if(_tcscmp(COM4, pszKeyName)==0) COMx = 4;
                    if(_tcscmp(COM5, pszKeyName)==0) COMx = 5;
                    if(_tcscmp(COM6, pszKeyName)==0) COMx = 6;
                    if(_tcscmp(COM7, pszKeyName)==0) COMx = 7;
                    if(_tcscmp(COM8, pszKeyName)==0) COMx = 8;
                    if(_tcscmp(COM9, pszKeyName)==0) COMx = 9;
                        
                    retval=TRUE;
                    break;
                }
            }
        }

        i++; //Increment the Index of the Index of the Subkey 
    }

    if(hActive)
    {
        RegCloseKey(hActive);
    }

    if(hDriver)
    {
        RegCloseKey(hDriver);
    }

    if (pszKeyName)
    {
        free(pszKeyName);
    }

    return retval;
}

/*++

Util_SetSerialPort:

Arguments:
LPCTSTR          cmdline - Command Line Arguments
TCHAR *  pg_lpszCommPort - Pointer to the Global Comm Port VAriable 

Return Value:
TRUE for success else FALSE

Author:
Unknown (unknown)
 
Notes:
Search for Serial Port and set Comx to the corresponding port 

--*/
UINT Util_SetSerialPort(_In_z_ LPCTSTR *pcmdline , _Out_cap_(cch) TCHAR *pg_lpszCommPort, size_t cch)
{
    
    while(CmdSpace == **pcmdline)
         (*pcmdline)++;       

    //Must use strlen because WinCE uses longer names.
    wcsncpy_s(pg_lpszCommPort, (COM_PORT_SIZE+1), *pcmdline, wcslen(*pcmdline) );

    pg_lpszCommPort[COM_PORT_SIZE]=0;
            
    //Increment the Command Line parameter
    (*pcmdline)+=COM_PORT_SIZE ;    

    g_pKato->Log( LOG_DETAIL, TEXT("Option set port == %s"),pg_lpszCommPort );
    
    if(_tcscmp(COM0, pg_lpszCommPort)==0) COMx = 0;
    if(_tcscmp(COM1, pg_lpszCommPort)==0) COMx = 1;
    if(_tcscmp(COM2, pg_lpszCommPort)==0) COMx = 2;
    if(_tcscmp(COM3, pg_lpszCommPort)==0) COMx = 3;
    if(_tcscmp(COM4, pg_lpszCommPort)==0) COMx = 4;
    if(_tcscmp(COM5, pg_lpszCommPort)==0) COMx = 5;
    if(_tcscmp(COM6, pg_lpszCommPort)==0) COMx = 6;
    if(_tcscmp(COM7, pg_lpszCommPort)==0) COMx = 7;
    if(_tcscmp(COM8, pg_lpszCommPort)==0) COMx = 8;
    if(_tcscmp(COM9, pg_lpszCommPort)==0) COMx = 9;

    //Invalid COM Port was specified 
    if(-1==COMx)
    {
        LOG(L" COM Port specified is not valid \n");
        return FALSE;
    }    

    return TRUE;

}
/*++
Util_GetValidSerialPorts:

Arguments:

Return Value:

Author:
Unknown (unknown)
 
Notes:
Checks how many Valid Serial Ports are present 
--*/
VOID Util_GetValidSerialPorts(BOOL bVerbose)
{
    LONG index = 0;
    TCHAR szCommPort[MAX_DEVICE_NAMELEN] = {NULL};
    HANDLE handle = NULL;

    LOG(TEXT("** Util_GetValidSerialPorts **\n"));

    for(index = 0; index <=MAX_DEVICE_INDEX; index++)
    {
        //Create a COMx Device Name
        swprintf_s(szCommPort, _countof(szCommPort), SZ_COMM_PORT_FMT, index);
        szCommPort[MAX_DEVICE_NAMELEN-1]=NULL;

        if(bVerbose)
        {
            LOG(TEXT("Checking to see if %s is a serial valid port"), szCommPort);
        }

        handle = CreateFile(szCommPort, 
                            GENERIC_READ | GENERIC_WRITE, 
                            0, 
                            NULL,
                            OPEN_EXISTING, 
                            0, 
                            NULL);

        if(INVALID_HANDLE_VALUE != handle)
        {
            COMMPROP cp = {0};

            if( GetCommProperties( handle, &cp ))
            {
                if(bVerbose)
                {                    
                    LOG(TEXT("GetCommProperties was successful on %s. dwProvSubType == 0x%04x dwProvCapabilities == 0x%04x"), 
                        szCommPort, 
                        cp.dwProvSubType, 
                        cp.dwProvCapabilities);

                    for(int j = 0; j < _countof(p_c_s); j++)
                    {
                        LOG(TEXT("Checking %s"), p_c_s[j].szProvCapsNames);

                        if( p_c_s[j].dwProvCapabilities & cp.dwProvCapabilities )
                        {
                            LOG(TEXT("%s supports %s"), szCommPort, p_c_s[j].szProvCapsNames);
                        }
                    }
                }

                if (index == irCOMx)
                {
                    LOG(TEXT("%s is NOT a valid serial port because it is an IR port"), szCommPort);
                }
                else
                {
                    DWORD dwErrors = 0;
                    if(0 == ClearCommError(handle, &dwErrors, NULL))
                    {
                        LOG(TEXT("%s is a valid serial port but will not be tested because it is not connected"), szCommPort);
                    }
                    else
                    {
                        COMMask[index] = 1;
                        LOG(TEXT("%s is a Valid serial port"), szCommPort);
                    }
                }
            }
            else if(bVerbose)
            {
                LOG(TEXT("GetCommProperties was not successful on %s and it will not be tested, GLE == %d"), szCommPort, GetLastError());
            }

            CloseHandle(handle);
        }
        else if(bVerbose)
        {
            LOG(TEXT("%s is NOT a valid serial port"), szCommPort);
        }
    }

    return;
}

/*++
Util_SerialPortConfig:

Arguments:

Return Value:
TRUE for success else FALSE

Author:
Unknown (unknown)
 
Notes:
Checks for the Configuration of the Serial Port Which helps the 
tests to run selectively.
--*/
VOID Util_SerialPortConfig(VOID)
{
    //Check if there is a COMM IR Port 
     Util_QueryCommIRCOMMPort();   

     //Identify which serial Ports are Valid
     Util_GetValidSerialPorts(FALSE);

     //TBD:Checking the Configuration and performing Smart Serial Tests
     return ;
}

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
#include "I2CConfigParser.h"

static DWORD DeinitI2CConfigParser()
{
    DWORD dwRet=SUCCESS;
    //todo error checking
    if(SUCCESS!=FreeParams())
    {
        dwRet=FAILURE;
    }
    SAFE_CLOSE(I2CTestParserDeviceNameListCreateEvent);
    SAFE_CLOSE(I2CTestParserBufferListCreateEvent);
    SAFE_CLOSE(I2CTestParserExitEvent);
    SAFE_CLOSE(I2CTestParserHeapCreateEvent );
    SAFE_CLOSE(I2CTestParserSizeSignalEvent);
    DeleteCriticalSection(&gI2CParserCriticalSection);
    return dwRet;
  
}
static DWORD InitI2CConfigParser()
{
    DWORD dwRet=SUCCESS;
    do
    { 
        InitializeCriticalSection( &gI2CParserCriticalSection);
        I2CTestParserExitEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_PARSER_EXIT);
        if(!I2CTestParserExitEvent)
        {
            NKDbgPrintfW(_T("I2CConfigParser: Failed to create Parser exit event"));
            dwRet=FAILURE;
            break;
        }
        I2CTestParserHeapCreateEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_HEAP_CREATE);
        if(!I2CTestParserHeapCreateEvent)
        {
            NKDbgPrintfW(_T("I2CConfigParser: Failed to create Parser heap create event"));
            dwRet=FAILURE;
            break;
        }
        I2CTestParserSizeSignalEvent=CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_SIZE_SIGNAL);
        if(!I2CTestParserSizeSignalEvent)
        {
            NKDbgPrintfW(_T("I2CConfigParser: Failed to create Parser size signal event"));
            dwRet=FAILURE;
            break;
        }
        I2CTestParserBufferListCreateEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_BUFFER_LIST_CREATE);
        if(!I2CTestParserBufferListCreateEvent)
        {
            NKDbgPrintfW(_T("I2CConfigParser: Failed to create Parser buffer list create event"));
            dwRet=FAILURE;
            break;
        }
        I2CTestParserDeviceNameListCreateEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_DEVICE_NAME_LIST_CREATE);
        if(!I2CTestParserDeviceNameListCreateEvent)
        {
            NKDbgPrintfW(_T("I2CConfigParser: Failed to create Parser device name list create event"));
            dwRet=FAILURE;
            break;
        }
    }while(0);
    if(FAILURE==dwRet)
    {
        DeinitI2CConfigParser();
    }
    return dwRet;
}

int WINAPI WinMain (
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPTSTR      lpCmdLine,
    int         nCmdShow
    )
{
    DWORD dwRet=SUCCESS;
    DWORD dwHeapAddress=0;
    DWORD dwBufferAddress=0;
    DWORD dwDeviceNameAddress=0;
    DWORD dwSizeAddress=0;
       
    if(SUCCESS ==InitI2CConfigParser())
    {
        if(getParams(lpCmdLine))
        {
            //if successful in parsing the data and allocating on heap, send the heap start address to i2c test
            dwHeapAddress=reinterpret_cast<DWORD>(gI2CData);
            dwBufferAddress=reinterpret_cast<DWORD>(BufferPtr);
            dwDeviceNameAddress=reinterpret_cast<DWORD>(DeviceNamePtr);
            dwSizeAddress=reinterpret_cast<DWORD>(&gI2CDataSize);
            if(dwHeapAddress && dwBufferAddress && dwDeviceNameAddress && dwSizeAddress)
            {

                NKDbgPrintfW(_T("I2CConfigParser: Parsed and Allocated I2c Test data"));
                PrintI2CDataList();
                SetEventData(I2CTestParserHeapCreateEvent,dwHeapAddress);
                SetEvent(I2CTestParserHeapCreateEvent);
                SetEventData(I2CTestParserBufferListCreateEvent,dwBufferAddress);
                SetEvent(I2CTestParserBufferListCreateEvent);
                SetEventData(I2CTestParserDeviceNameListCreateEvent,dwDeviceNameAddress);
                SetEvent(I2CTestParserDeviceNameListCreateEvent);
                SetEventData(I2CTestParserSizeSignalEvent,dwSizeAddress);
                SetEvent(I2CTestParserSizeSignalEvent);
                NKDbgPrintfW(_T("I2CConfigParser: Parsed and Allocated I2c Test data, Length of List =%d"),gI2CDataSize);
                NKDbgPrintfW(_T("**************************************************************************"));
            }
            else
            {
                NKDbgPrintfW(_T("I2CConfigParser: Failed to allocate heap memory =>0x%x"), gI2CData);                
                dwRet=FAILURE;
            }
          
            
            if(WAIT_OBJECT_0!= WaitForSingleObject( I2CTestParserExitEvent,10000))
            {
                
                NKDbgPrintfW(_T("I2CConfigParser: Failed wait for exit event"));
                dwRet=FAILURE;
            }
            else
            {
                NKDbgPrintfW(_T("I2CConfigParser: Exiting Config Parser Process"));
            }
        }
        //wait for test to exit, heap data is being used by test, once process exits heap will be invalid
       
    }
       
    DeinitI2CConfigParser();
    
    return dwRet;
}

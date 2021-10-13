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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: HelperFuncs.cpp
//          Contains the test helper functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "I2CTest.h"
#include "HelperFuncs.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// PrintUsage
//Prints the usage of the test
//
// Parameters:
// None 
//
// Return value:
//  Void
////////////////////////////////////////////////////////////////////////////////
void PrintUsage()
{
        NKDbgPrintfW( L"The I2C Test is a data driven test.");
        NKDbgPrintfW( L"It uses a configuration file \"\\release\\I2C_Test_Config.xml\", ");
        NKDbgPrintfW( L"to get the i2c data specific to a platform.");
        NKDbgPrintfW( L"Please edit the config file \"\\release\\I2C_Test_Config.xml\",  ");
        NKDbgPrintfW( L"to use I2C transaction details specific to the platform under test.");
        NKDbgPrintfW( L"User can also specify another config file through command line params");
        NKDbgPrintfW( L"The input configuration file has a list of transaction nodes,  ");
        NKDbgPrintfW( L"that are used for testing the bus.");
        NKDbgPrintfW( L"A valid transaction node contains the mandatory fields as shown below:");
        NKDbgPrintfW( L"\t\t<Transaction ID=\"<Transaction ID\"> ");
        NKDbgPrintfW( L"\t\t\t<BusName>[Bus name e.g I2C1:]</BusName> ");
        NKDbgPrintfW( L"\t\t\t<SubordinateAddress>[Subordinate address e.g 0x30]</SubordinateAddress>  ");
        NKDbgPrintfW( L"\t\t\t<Operation>[operation: write /read/loopback]</Operation> ");
        NKDbgPrintfW( L"\t\t\t<Data>[data: space seperated hex bytes]</Data> ");
        NKDbgPrintfW( L"\t\t\t<RegisterAddress>[register address e.g 0x01]</RegisterAddress> ");
        NKDbgPrintfW( L"\t\t\t<NumberOfBytes>[number of bytes to read/write]</NumberOfBytes> ");
        NKDbgPrintfW( L"\t\t</Transaction>");
        NKDbgPrintfW( L"Additionally a valid transaction may also have following optional fields: ");
        NKDbgPrintfW( L"\t<SubordinateAdressSize>[size of Subordinate address]</SubordinateAdressSize> ");
        NKDbgPrintfW( L"\t<RegisterAdressSize>[size of register address]</RegisterAdressSize> ");
        NKDbgPrintfW( L"\t<Speed>[speed of transaction]</Speed> ");
        NKDbgPrintfW( L"\t<TimeOut>[timeout for a single transaction]</TimeOut>");
        NKDbgPrintfW( L"The test needs to run in kernel mode, please make sure to run the test with -n option. ");
        NKDbgPrintfW( L"The test comprises of two processes:  ");
        NKDbgPrintfW( L"\t1. Tux process that loads the kernel mode test=> testi2c.dll  ");
        NKDbgPrintfW( L"\t2. User mode configuration parser, that parses the xml file=> \"I2CConfigParser.exe\"  ");
                

}
////////////////////////////////////////////////////////////////////////////////
// PrintI2CDataList
//Prints the data that the test populated based on parser process scan of xml
//
// Parameters:
//  None
//
// Return value:
//  Void
////////////////////////////////////////////////////////////////////////////////
void PrintI2CDataList()
{
    for(long BusIndex=0;BusIndex<gI2CTransSize;BusIndex++)
    {
        
        NKDbgPrintfW( L"I2CData[%d].dwTransactionID=%d",BusIndex, gI2CTrans[BusIndex].dwTransactionID);
        NKDbgPrintfW( L"I2CData[%d].pwszDeviceName=%s",BusIndex, gI2CTrans[BusIndex].pwszDeviceName);
        NKDbgPrintfW( L"I2CData[%d].Operation=%d",BusIndex, gI2CTrans[BusIndex].Operation );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddress=%x", BusIndex,gI2CTrans[BusIndex].dwSubordinateAddress );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddress=%x",BusIndex,gI2CTrans[BusIndex].dwRegisterAddress );
        NKDbgPrintfW( L"I2CData[%d].dwLoopbackAddress=%x", BusIndex, gI2CTrans[BusIndex].dwLoopbackAddress );
        NKDbgPrintfW( L"I2CData[%d].dwNumOfBytes=%",BusIndex,gI2CTrans[BusIndex].dwNumOfBytes );
        NKDbgPrintfW( L"I2CData[%d].BufferPtr Start=%x",BusIndex, gI2CTrans[BusIndex].BufferPtr[0]);
        NKDbgPrintfW( L"I2CData[%d].dwTimeOut=%d",BusIndex, gI2CTrans[BusIndex].dwTimeOut);
        NKDbgPrintfW( L"I2CData[%d].dwSpeedOfTransaction=%d", BusIndex,gI2CTrans[BusIndex].dwSpeedOfTransaction );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddressSize=%d",BusIndex,gI2CTrans[BusIndex].dwSubordinateAddressSize );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddressSize=%d",BusIndex,gI2CTrans[BusIndex].dwRegisterAddressSize );
        
    }
}

////////////////////////////////////////////////////////////////////////////////
// PrintI2CData
//Prints A transaction based on index in the global data that 
//the test populated based on parser process scan of xml
//
// Parameters:
//  I2CTRANS *I2CTestData - I2C Transaction List
//  long BusIndex -            Transaction index
//
// Return value:
//  Void
////////////////////////////////////////////////////////////////////////////////
void PrintI2CData(I2CTRANS *I2CTestData, long BusIndex)
{
        
        NKDbgPrintfW( L"I2CData[%d].dwTransactionID=%d",BusIndex, I2CTestData[BusIndex].dwTransactionID);
        NKDbgPrintfW( L"I2CData[%d].pwszDeviceName=%s",BusIndex, I2CTestData[BusIndex].pwszDeviceName);
        NKDbgPrintfW( L"I2CData[%d].Operation=%d",BusIndex, I2CTestData[BusIndex].Operation );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddress=%x", BusIndex,I2CTestData[BusIndex].dwSubordinateAddress );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddress=%x",BusIndex,I2CTestData[BusIndex].dwRegisterAddress );
        NKDbgPrintfW( L"I2CData[%d].dwLoopbackAddress=%x", BusIndex, I2CTestData[BusIndex].dwLoopbackAddress );
        NKDbgPrintfW( L"I2CData[%d].dwNumOfBytes=%d",BusIndex,I2CTestData[BusIndex].dwNumOfBytes );
        if(I2CTestData[BusIndex].BufferPtr)
        {
            NKDbgPrintfW( L"I2CData[%d].BufferPtr Start=%p",BusIndex, I2CTestData[BusIndex].BufferPtr[0]);
        }
        NKDbgPrintfW( L"I2CData[%d].dwTimeOut=%d",BusIndex, I2CTestData[BusIndex].dwTimeOut);
        NKDbgPrintfW( L"I2CData[%d].dwSpeedOfTransaction=%d", BusIndex,I2CTestData[BusIndex].dwSpeedOfTransaction );
        NKDbgPrintfW( L"I2CData[%d].dwSubordinateAddressSize=%d",BusIndex,I2CTestData[BusIndex].dwSubordinateAddressSize );
        NKDbgPrintfW( L"I2CData[%d].dwRegisterAddressSize=%d",BusIndex,I2CTestData[BusIndex].dwRegisterAddressSize );
        
}
////////////////////////////////////////////////////////////////////////////////
//AllocateI2CTransList
//Allocates on heap three lists:
//pI2CTRANSNode: main list of structs that has all transactions
//BufferPtr: Pointer to list of buffers for all transactions
//DeviceNamePtr: Pointer to list of Device names for all transactions
// Parameters:
//  I2CTRANS **pI2CTRANSNode -  pointer to I2C Transaction List
//  LONG Length -                   Number of transactions
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
static DWORD AllocateI2CTransList(LONG Length, I2CTRANS **pI2CTRANSNode )
{
    DWORD dwRet=FAILURE;
    AcquireI2CTestDataLock();  
    //allocating more in each struct, for embedded pointers
    *pI2CTRANSNode=(I2CTRANS *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((sizeof(I2CTRANS))*Length));
    gBufferPtrList=(BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((I2C_MAX_LENGTH*sizeof(BYTE))*Length));
    gDeviceNamePtrList=(BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ((MAX_PATH*sizeof(TCHAR))*Length));
    if (*pI2CTRANSNode!=NULL)
    {
        //just allocate we will fix up the pointers after the data is copied over from user process
        dwRet=SUCCESS;
    }
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//FixUpI2CTransList
//Fixes the embedded pointers in every transaction
//pI2CTRANSNode: main list of structs that has all transactions
//BufferPtr: Pointer to list of buffers for all transactions
//DeviceNamePtr: Pointer to list of Device names for all transactions
// Parameters:
//  
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
static DWORD FixUpI2CTransList()
{
    DWORD dwRet=FAILURE;
    BYTE *BPtr;
    AcquireI2CTestDataLock();  
    ASSERT(gI2CTrans && gI2CTransSize && gBufferPtrList && gDeviceNamePtrList);
    if (gI2CTrans && gI2CTransSize && gBufferPtrList && gDeviceNamePtrList)
    {
        gI2CTrans[0].BufferPtr=reinterpret_cast<BYTE *>(gBufferPtrList);
        BPtr=reinterpret_cast<BYTE *>(gBufferPtrList);
        for(long i=1;i<gI2CTransSize;i++)
        {
             BPtr=reinterpret_cast<BYTE *>(BPtr +I2C_MAX_LENGTH);
             gI2CTrans[i].BufferPtr=reinterpret_cast<BYTE *>(BPtr);

        }
        gI2CTrans[0].pwszDeviceName=reinterpret_cast<TCHAR *>(gDeviceNamePtrList);
        BPtr=reinterpret_cast<BYTE *>(gDeviceNamePtrList);
        for(long i=1;i<gI2CTransSize;i++)
        {
             BPtr=reinterpret_cast<BYTE *>(BPtr +MAX_PATH);
             gI2CTrans[i].pwszDeviceName=reinterpret_cast<TCHAR *>(BPtr);

        }
        dwRet=SUCCESS;
    }
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//LookUpBusHandle
//Looks up for a bus handle based on bus name and if a handle is already 
//open returns that
//
// Parameters:
//  LONG Length             Number of transactions
//  I2CTRANS *pI2CTRANSNode  Pointer to list of transactions
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
static DWORD LookUpBusHandle(LONG Length, I2CTRANS *pI2CTRANSNode)
{
    BOOL bFound=FALSE;
    DWORD dwRet=SUCCESS;
    for(int i=0;i<Length; i++)
    {
        bFound=FALSE;
        for(DWORD j=0;j<(dwI2CHandleTableLen);j++)
        {
            if(!_wcsnicmp(pI2CTRANSNode[i].pwszDeviceName,  I2CHandleTable[j].pwszDeviceName,COUNTOF( I2CHandleTable[j].pwszDeviceName)))
                {
                    pI2CTRANSNode[i].HandleId=j;
                    NKDbgPrintfW( TEXT("Mapping to existing Handle from Handle List")); 
                    bFound=TRUE;
                    break;
                    
                }                         
        }
        if(TRUE!=bFound)
        {
            NKDbgPrintfW( TEXT("Not Found for i=%d"), i); 
            if(dwI2CHandleTableLen<I2C_MAX_NUM_BUSES)
            {
                StringCbCatN(pI2CTRANSNode[i].pwszDeviceName, MAX_PATH, L"\0", 1);
                StringCbCopyN( I2CHandleTable[j].pwszDeviceName,MAX_PATH-1,pI2CTRANSNode[i].pwszDeviceName, MAX_PATH-1);
                pI2CTRANSNode[i].HandleId=j;
                (dwI2CHandleTableLen)++;
                NKDbgPrintfW( TEXT("Created new Handle Entry in Handle list")); 
                bFound=TRUE;
                    
            }
        }

    }
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//FreeParams
//Returns a read or write transaction index from the list for bvt
//gI2CTrans: main list of structs that has all transactions
//gBufferPtrList: Pointer to list of buffers for all transactions
//gDeviceNamePtrList: Pointer to list of Device names for all transactions
// Parameters:
//  None
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD FreeParams()
{
    DWORD dwRet=SUCCESS;
    AcquireI2CTestDataLock();  
    if(gDeviceNamePtrList)
    {
        //since we allocate a whole chunk of memory freeing that will also free locations pointed by embedded pointers
        if(!HeapFree(GetProcessHeap(), 0, gDeviceNamePtrList))
           {
                dwRet=FAILURE;
           }
    }
    if(gBufferPtrList)
    {
        //since we allocate a whole chunk of memory freeing that will also free locations pointed by embedded pointers
        if(!HeapFree(GetProcessHeap(), 0, gBufferPtrList))
           {
                dwRet=FAILURE;
           }
    }
    if(gI2CTrans)
    {   
         if(!HeapFree(GetProcessHeap(), 0, gI2CTrans))
           {
                dwRet=FAILURE;
           }
    }
    
    ReleaseI2CTestDataLock();  
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//TestInit
//Initializes the test and the data this only executes once to get all the data from 
//parser process, once the data is allocated copied and fixed up, the parser is signaled to exit
//gI2CTrans: main list of structs that has all transactions
//gBufferPtrList: Pointer to list of buffers for all transactions
//gDeviceNamePtrList: Pointer to list of Device names for all transactions
// Parameters:
// None 
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD TestInit()
{
    DWORD dwRet=SUCCESS;
    DWORD dwAddress=0, dwSizeInBytes=0, dwActualSize=0;
    I2CTRANS *lpAddress=NULL;
    long *pSize=NULL;
    BYTE *BufferPtrAddress=NULL;
    BYTE *DeviceNamePtrAddress=NULL;
    
    AcquireI2CTestInitLock(); 
    AcquireI2CTestDataLock();  
    if(!gdwI2CTRANSInitialized && !gdwInitFailed)
    { 
        if(!IsKModeAddr((DWORD)TestInit))
        {
            gdwI2CTRANSInitialized=0;
            gdwInitFailed=1;
            NKDbgPrintfW(_T("TestInit: All tests will skip, please run the test in kernel mode"));
            ReleaseI2CTestDataLock();
            return FAILURE;


        }
              
        I2CTestHeapCreateEvent= CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_HEAP_CREATE);
        I2CTestParserExitEvent= CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_PARSER_EXIT);
        I2CTestSizeSignalEvent= CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_SIZE_SIGNAL);   
        I2CTestBufferListCreateEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_BUFFER_LIST_CREATE);
        I2CTestDeviceNameListCreateEvent = CreateEvent(NULL, FALSE, FALSE, I2CTEST_NAMED_EVENT_DEVICE_NAME_LIST_CREATE);
        if(INVALID_HANDLE_VALUE==I2CTestHeapCreateEvent || INVALID_HANDLE_VALUE==I2CTestParserExitEvent)
        {
            gdwI2CTRANSInitialized=0;
            gdwInitFailed=1;
            NKDbgPrintfW(_T("TestInit: All tests will skip, Error creating global event handles"));
            ReleaseI2CTestDataLock();
            return FAILURE;
        }
        gI2CTrans=NULL;
        gI2CTransSize=0;
        ReleaseI2CTestDataLock();
        if (!CreateProcess( _T("I2CConfigParser.exe"), g_strConfigFile, NULL, NULL, FALSE,0, NULL, NULL, NULL, &gProcInfo))
            {
                NKDbgPrintfW(_T("TestInit: Failed to create Config Parser process"));
                gdwInitFailed=1;
                dwRet=FAILURE;
            } 
        else
        {
            NKDbgPrintfW(_T("TestInit:Successfully Created Config Parser Process"));
             ghUserProcess= OpenProcess( 0, FALSE,gProcInfo.dwProcessId);
             if(INVALID_HANDLE_VALUE!=ghUserProcess)
             {
                 if(WAIT_OBJECT_0== WaitForSingleObject( I2CTestHeapCreateEvent,100000))
                 {
                     if(WAIT_OBJECT_0== WaitForSingleObject( I2CTestSizeSignalEvent,100000))
                     {
                        //getEventData passes address of global data from user process
                        lpAddress=reinterpret_cast<I2CTRANS *> (GetEventData(I2CTestHeapCreateEvent));
                        //GetEventData passes size in number of structs for global test data array
                        //todo chane this pass as pointer as data type is different.
                        pSize=reinterpret_cast<long *>(GetEventData(I2CTestSizeSignalEvent));
                        ReadProcessMemory(ghUserProcess, pSize, &gI2CTransSize,sizeof(long),&dwActualSize);
                        if(dwActualSize!=sizeof(long))
                        {
                                NKDbgPrintfW(_T("TestInit: Error in number of bytes read for Global Struct array size"));
                        }
                        //allocate in test the same array, so we can copy and exit parser code
                        AllocateI2CTransList(gI2CTransSize, &gI2CTrans);
                        //get sizeinbytes, to use for readprocessmemory
                        dwSizeInBytes=(sizeof(I2CTRANS))*gI2CTransSize;
                        ReadProcessMemory(ghUserProcess, lpAddress, gI2CTrans,dwSizeInBytes,&dwActualSize);
                        if(dwActualSize!=dwSizeInBytes)
                        {
                            NKDbgPrintfW(_T("TestInit: Error in number of bytes read for Global Struct array"));
                        }
                        if(WAIT_OBJECT_0== WaitForSingleObject( I2CTestBufferListCreateEvent,100000))
                        {
                            if(WAIT_OBJECT_0== WaitForSingleObject( I2CTestDeviceNameListCreateEvent, 100000))
                            {

                                BufferPtrAddress=reinterpret_cast<BYTE *>(GetEventData(I2CTestBufferListCreateEvent));
                                dwActualSize=0;
                                dwSizeInBytes=((I2C_MAX_LENGTH)*gI2CTransSize);
                                ReadProcessMemory(ghUserProcess, BufferPtrAddress, gBufferPtrList, dwSizeInBytes, &dwActualSize);
                                if(dwActualSize!=dwSizeInBytes)
                                {
                                    NKDbgPrintfW(_T("TestInit: Error in number of bytes read for Global Buffer array"));
                                }
                                DeviceNamePtrAddress=reinterpret_cast<BYTE *>(GetEventData(I2CTestDeviceNameListCreateEvent));
                                dwSizeInBytes=((MAX_PATH)*gI2CTransSize);
                                dwActualSize=0;
                                ReadProcessMemory(ghUserProcess, DeviceNamePtrAddress, gDeviceNamePtrList, dwSizeInBytes, &dwActualSize);
                                if(dwActualSize!=dwSizeInBytes)
                                {
                                    NKDbgPrintfW(_T("TestInit: Error in number of bytes read for Global Device name array"));
                                }
                                //fixup embedded pointers in the data
                                FixUpI2CTransList();
                                LookUpBusHandle(gI2CTransSize, gI2CTrans);
                                NKDbgPrintfW(_T("TestInit: Success Initializing data arrays"));
                                
                            }
                        }
                        else
                         {
                            dwRet=FAILURE;
                            gdwInitFailed=1;
                            NKDbgPrintfW(_T("TestInit: Failed Wait for Buffer ptr creation event")); 
                         }
                        
                     }
                     else
                     {
                        dwRet=FAILURE;
                        gdwInitFailed=1;
                        NKDbgPrintfW(_T("TestInit: Failed Wait for Length data event")); 
                     }
                    
                  
                 }
                 else
                 {
                    dwRet=FAILURE;
                    gdwInitFailed=1;
                    NKDbgPrintfW(_T("TestInit: Failed Wait for Heap Create event")); 
                 }
             }//if(INVALID_HANDLE_VALUE!=hUserProcess)
             SetEvent(I2CTestParserExitEvent);  
        }//createprocess
        
        
    }
    else //that means we have already initialized test data
    {
        ReleaseI2CTestDataLock();          
        dwRet=SUCCESS;
    }
    //cleanup incase we failed
    if(FAILURE==dwRet)
    {
        gdwInitFailed=1;
        g_pKato->Log(LOG_DETAIL,L"TestInit: Failure encountered while test initialization, will skip all tests");
        if(INVALID_HANDLE_VALUE!=I2CTestBufferListCreateEvent)
        {
           CloseHandle(I2CTestBufferListCreateEvent);
           I2CTestBufferListCreateEvent=INVALID_HANDLE_VALUE;
        }
         if(INVALID_HANDLE_VALUE!=I2CTestDeviceNameListCreateEvent)
        {
           CloseHandle(I2CTestDeviceNameListCreateEvent);
           I2CTestDeviceNameListCreateEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestHeapCreateEvent)
        {
            CloseHandle(I2CTestHeapCreateEvent);
            I2CTestHeapCreateEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestParserExitEvent)
        {
            CloseHandle(I2CTestParserExitEvent);
            I2CTestParserExitEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestSizeSignalEvent)
        {
            CloseHandle(I2CTestSizeSignalEvent);
            I2CTestSizeSignalEvent=INVALID_HANDLE_VALUE;
        }
        if(!FreeParams())
            {
                g_pKato->Log(LOG_DETAIL,L"TestInit: Error freeing up resources" );

                //todo print message: error trying to free up resources
            }
             
    }
    else
    {
        AcquireI2CTestDataLock();  
        gdwI2CTRANSInitialized=1;
        ReleaseI2CTestDataLock();
    }
ReleaseI2CTestInitLock(); 
return dwRet;    
}
////////////////////////////////////////////////////////////////////////////////
//TestDeinit
//Frees the global lists and closes all event handles
//
// Parameters:
//  None
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD TestDeinit()
{
    DWORD dwRet=FAILURE;
    AcquireI2CTestDataLock(); 
    if(gdwI2CTRANSInitialized)
    {
        if(!FreeParams())
        {
            g_pKato->Log(LOG_DETAIL,L"TestDeinit:Error freeing up resources" );

            //todo print message: error trying to free up resources
        }
        //SetEvent( I2CTestParserExitEvent);
        gdwI2CTRANSInitialized=0;
        if(INVALID_HANDLE_VALUE!=I2CTestBufferListCreateEvent)
        {
           CloseHandle(I2CTestBufferListCreateEvent);
           I2CTestBufferListCreateEvent=INVALID_HANDLE_VALUE;
        }
         if(INVALID_HANDLE_VALUE!=I2CTestDeviceNameListCreateEvent)
        {
           CloseHandle(I2CTestDeviceNameListCreateEvent);
           I2CTestDeviceNameListCreateEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestHeapCreateEvent)
        {
            CloseHandle(I2CTestHeapCreateEvent);
            I2CTestHeapCreateEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestParserExitEvent)
        {
            CloseHandle(I2CTestParserExitEvent);
            I2CTestParserExitEvent=INVALID_HANDLE_VALUE;
        }
        if(INVALID_HANDLE_VALUE!=I2CTestSizeSignalEvent)
        {
            CloseHandle(I2CTestSizeSignalEvent);
            I2CTestSizeSignalEvent=INVALID_HANDLE_VALUE;
        }

        if(INVALID_HANDLE_VALUE != ghUserProcess)
        {
            CloseHandle(ghUserProcess);
            ghUserProcess=INVALID_HANDLE_VALUE;
        }

        if(INVALID_HANDLE_VALUE != gProcInfo.hThread)
        {
            CloseHandle(gProcInfo.hThread);
            gProcInfo.hThread = INVALID_HANDLE_VALUE;
        }

        if(INVALID_HANDLE_VALUE != gProcInfo.hProcess)
        {
            CloseHandle(gProcInfo.hProcess);
            gProcInfo.hProcess = INVALID_HANDLE_VALUE;
        }
        
        ReleaseI2CTestDataLock();
        dwRet=SUCCESS;
    }
    else //not initialized, so return success
    {
        ReleaseI2CTestDataLock();
        SetEvent( I2CTestParserExitEvent);
        dwRet=SUCCESS;
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_OpenI2CHandle
//Test Helper for open I2c Handle, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD Hlp_OpenI2CHandle(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
   
    if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        int HandleId=I2CTestData[BusIndex].HandleId;
        if((HandleId>=0) && (HandleId< I2C_MAX_NUM_BUSES))
        {
            if(!I2CHandleTable[HandleId].hI2C)
            {
                I2CHandleTable[HandleId].hI2C=I2cOpen(I2CHandleTable[HandleId].pwszDeviceName);
                
            }
        }
        if(I2CHandleTable[HandleId].hI2C)
        {
            dwRet=SUCCESS;
            g_pKato->Log(LOG_DETAIL,L"Hlp_OpenI2CHandle: Opened I2C handle :%s",I2CHandleTable[HandleId].pwszDeviceName);
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,L"Hlp_OpenI2CHandle: Failed to open I2c handle, Please Validate Device name:%s",I2CTestData[BusIndex].pwszDeviceName);
        }

    }
    return dwRet;

}
////////////////////////////////////////////////////////////////////////////////
//Hlp_CloseI2CHandle
//Test Helper for close I2c Handle, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD Hlp_CloseI2CHandle(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=SUCCESS;
   //todo put error check from mdd
    if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        int HandleId=I2CTestData[BusIndex].HandleId;
        if((HandleId>=0) && (HandleId< I2C_MAX_NUM_BUSES))
        {
            if((I2CHandleTable[HandleId].hI2C))
            {
                I2cClose(I2CHandleTable[HandleId].hI2C);
                   I2CHandleTable[HandleId].hI2C=NULL;
                    g_pKato->Log(LOG_DETAIL,L"Hlp_CloseI2CHandle:Closed I2C handle :%s",I2CHandleTable[HandleId].pwszDeviceName );
                
                
            }

        }
    }
    return dwRet;

}
////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CSubordinateAddrSize
//Test Helper for I2cSetSlaveAddrSize, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CSubordinateAddrSize(I2CTRANS *I2CTestData, long BusIndex)
{   
    DWORD dwRet=FAILURE;
     if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL) &&
            (I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
    {
        if(TRUE==I2cSetSlaveAddrSize(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, I2CTestData[BusIndex].dwSubordinateAddressSize))
        {
                    dwRet=SUCCESS;
        }
    }
         
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CSubordinate
//Test Helper for I2cSetSlaveAddress, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CSubordinate(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
    if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL) &&
            (I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
    {
        
            if(TRUE==I2cSetSlaveAddress(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, I2CTestData[BusIndex].dwSubordinateAddress))
            dwRet=SUCCESS;
        
    }
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CRegAddrSize
//Test Helper for I2cSetRegAddrSize, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CRegAddrSize(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
    if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL) &&
            (I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
    {       
        if(TRUE==I2cSetRegAddrSize(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, I2CTestData[BusIndex].dwRegisterAddressSize))
        {
            dwRet=SUCCESS;
        }      
           
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CSpeed
//Test Helper for I2cSetSpeed, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CSpeed(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet = SUCCESS;
    DWORD dwActualSpeedSet=0;
    
    if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        if((I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
        {
            dwActualSpeedSet=I2cSetSpeed(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, I2CTestData[BusIndex].dwSpeedOfTransaction);
            if(dwActualSpeedSet)
            {
                if((dwActualSpeedSet == I2C_STANDARD_SPEED) || (dwActualSpeedSet == I2C_FAST_SPEED)
                    || (dwActualSpeedSet == I2C_HIGH_SPEED))
                {
                    g_pKato->Log(LOG_DETAIL,L"Actual I2C Transaction Speed set to %d", dwActualSpeedSet);
                    I2CTestData[BusIndex].dwSpeedOfTransaction = dwActualSpeedSet;
                }
                else
                {   
                    //The actual speed set is not a valid I2C Transaction speed
                    g_pKato->Log(LOG_FAIL,L"FAIL: Actual speed set %d is not a valid I2C Transaction Speed", dwActualSpeedSet);
                    dwRet = FAILURE;
                }
            }
            else
            {
                //driver didnt set the speed to the speed requested by test. I2C uses the standard speed in that case
                g_pKato->Log(LOG_DETAIL, L"WARNING: I2CSetSpeed failed. I2C will use the default Transaction Speed %d", I2C_STANDARD_SPEED);
                I2CTestData[BusIndex].dwSpeedOfTransaction = I2C_STANDARD_SPEED;
            }
        }
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CSpeed
//Test Helper for I2cSetSpeed, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CSpeed(HANDLE hI2C, DWORD dwSpeed)
{
    DWORD dwRet = SUCCESS;
    DWORD dwActualSpeedSet=0;

    if(hI2C) 
    {
        dwActualSpeedSet = I2cSetSpeed(hI2C, dwSpeed);
        
        if(dwActualSpeedSet)
        {
            if((dwActualSpeedSet == I2C_STANDARD_SPEED) || (dwActualSpeedSet == I2C_FAST_SPEED)
                || (dwActualSpeedSet == I2C_HIGH_SPEED))
            {
                g_pKato->Log(LOG_DETAIL,L"Actual I2C Transaction Speed set to %d", dwActualSpeedSet);
            }
            else
            {   
                //The actual speed set is not a valid I2C Transaction speed
                g_pKato->Log(LOG_FAIL,L"FAIL: Actual speed set %d is not a valid I2C Transaction Speed", dwActualSpeedSet);
                dwRet = FAILURE;
            }
        }
        else
        {
            //driver didnt set the speed to the speed requested by test. I2C uses the standard speed in that case
            g_pKato->Log(LOG_DETAIL, L"WARNING: I2CSetSpeed failed. I2C will use the default Transaction Speed %d", I2C_STANDARD_SPEED);
        }
    }

    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//Hlp_SetI2CTimeOut
//Test Helper for I2cSetTimeOut, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetI2CTimeOut(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
    //note this function can set timeout value to 0, 
    //its the responsibility of driver to error check and ensure timeout cannot be 0
     if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        if( (I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C!=INVALID_HANDLE_VALUE))
        {
            if(TRUE == I2cSetTimeOut(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, I2CTestData[BusIndex].dwTimeOut))
            {
                dwRet=SUCCESS;
            }
            
        }
    }
    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CWrite
//Test Helper for I2c bus write, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CWrite(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
         
     if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        if((I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
        {
          if(I2CTestData[BusIndex].dwNumOfBytes <= I2C_MAX_LENGTH)
            {
                // Before writing new value to register, save the current value

                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Reading the current value =>");
                if(TRUE== I2cRead(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                                I2CTestData[BusIndex].dwRegisterAddress,
                                bBuffer, 
                                I2CTestData[BusIndex].dwNumOfBytes))
                {   
                    PrintBufferData(bBuffer, I2CTestData[BusIndex].dwNumOfBytes);
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to read the current value of register");
                }

                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing=>");
                PrintBufferData(I2CTestData[BusIndex].BufferPtr, I2CTestData[BusIndex].dwNumOfBytes);
                
                if(TRUE== I2cWrite(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                        I2CTestData[BusIndex].dwRegisterAddress,
                        I2CTestData[BusIndex].BufferPtr, 
                        I2CTestData[BusIndex].dwNumOfBytes))
                {
                        dwRet=SUCCESS;
                }
                else
                {
                    g_pKato->Log(LOG_FAIL,L"Failed to write to device");
                }

                // Restore the original value of the register
                
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing the original value =>");
                PrintBufferData(bBuffer, I2CTestData[BusIndex].dwNumOfBytes);
                
                if(TRUE != I2cWrite(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                        I2CTestData[BusIndex].dwRegisterAddress,
                        bBuffer, 
                        I2CTestData[BusIndex].dwNumOfBytes))
                {
                    g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to write the original value to register");
                }
                
            }
            else
            {
                g_pKato->Log(LOG_FAIL,L"Hlp_I2CWrite: FAIL: Please check Data size, size cant be greater than %d", I2C_MAX_LENGTH );
            }
        }
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CWrite
//Test Helper for I2c bus write, calls into the i2c MDD API
//
// Parameters:
//  HANDLE hI2C                 Handle of I2C 
//  I2CTRANS I2CTestData         Transaction Details
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CWrite(HANDLE hI2C, I2CTRANS I2CTestData)
{
    DWORD dwRet=FAILURE;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
         
   
    if(I2CTestData.dwNumOfBytes <= I2C_MAX_LENGTH)
    {
        // Before writing new value to register, save the current value

        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Reading the current value =>");
        if(TRUE== I2cRead(hI2C, 
                        I2CTestData.dwRegisterAddress,
                        bBuffer, 
                        I2CTestData.dwNumOfBytes))
        {   
            PrintBufferData(bBuffer, I2CTestData.dwNumOfBytes);
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to read the current value of register");
        }

        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing=>");
        PrintBufferData(I2CTestData.BufferPtr, I2CTestData.dwNumOfBytes);
        
        if(TRUE== I2cWrite(hI2C, 
                I2CTestData.dwRegisterAddress,
                I2CTestData.BufferPtr, 
                I2CTestData.dwNumOfBytes))
        {
                dwRet=SUCCESS;
        }
        else
        {
            g_pKato->Log(LOG_FAIL,L"Failed to write to device");
        }

        // Restore the original value of the register
        
        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing the original value =>");
        PrintBufferData(bBuffer, I2CTestData.dwNumOfBytes);
        
        if(TRUE != I2cWrite(hI2C, 
                I2CTestData.dwRegisterAddress,
                bBuffer, 
                I2CTestData.dwNumOfBytes))
        {
            g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to write the original value to register");
        }
        
    }
    else
    {
        g_pKato->Log(LOG_FAIL,L"Hlp_I2CWrite: FAIL: Please check Data size, size cant be greater than %d", I2C_MAX_LENGTH );
    }
    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CLoopback
//Test Helper for I2c bus write followed by read and data verification
//calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CLoopback(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=SUCCESS; 
    size_t Count=0;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
    
     if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL))
    {
        if((I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
        {
            if((I2CTestData[BusIndex].dwNumOfBytes>0) && (I2CTestData[BusIndex].dwNumOfBytes<I2C_MAX_LENGTH))
            {
                BYTE bWriteBuffer[I2C_MDD_BUFFER_LENGTH]={0};
                BYTE bReadBuffer[I2C_MDD_BUFFER_LENGTH]={0};
                    
                            
                // Before writing new value to register, save the current value

                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:Reading the current value =>");
                if(TRUE== I2cRead(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                        I2CTestData[BusIndex].dwRegisterAddress,
                        bBuffer, 
                        I2CTestData[BusIndex].dwNumOfBytes))
                {
                    
                    PrintBufferData(bBuffer, I2CTestData[BusIndex].dwNumOfBytes);
                }
                else
                {
                    g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to read the current value of register");
                }

                g_pKato->Log(LOG_DETAIL, L"Performing WRITE operation ...");
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:writing=>");
                PrintBufferData(I2CTestData[BusIndex].BufferPtr, I2CTestData[BusIndex].dwNumOfBytes);   

                if(TRUE== I2cWrite(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                        I2CTestData[BusIndex].dwRegisterAddress,
                        I2CTestData[BusIndex].BufferPtr, 
                        I2CTestData[BusIndex].dwNumOfBytes))
                {
                    g_pKato->Log(LOG_DETAIL, L"Performing READ operation ...");
                    if(TRUE== I2cRead(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                                    I2CTestData[BusIndex].dwLoopbackAddress,
                                    bReadBuffer, 
                                    I2CTestData[BusIndex].dwNumOfBytes))
                    {
                        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:Read Data=>");
                        PrintBufferData(bReadBuffer, I2CTestData[BusIndex].dwNumOfBytes);   
                        
                        //compare strings and then return success
                        g_pKato->Log(LOG_DETAIL, L"Comparing the read data with written data ...");
                        if(memcmp(
                                   I2CTestData[BusIndex].BufferPtr,
                                   bReadBuffer,
                                   I2CTestData[BusIndex].dwNumOfBytes))
                        {
                            g_pKato->Log(LOG_FAIL, L"FAIL: Read data not same as written data");
                            dwRet = FAILURE;
                        }
                    }
                    else
                    {
                        g_pKato->Log(LOG_FAIL,L"Failed to Read from Device");
                        dwRet = FAILURE;
                    }

                    // Restore the original value of the register
                
                    g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing the original value =>");
                    PrintBufferData(bBuffer, I2CTestData[BusIndex].dwNumOfBytes);
                    
                    if(TRUE != I2cWrite(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                            I2CTestData[BusIndex].dwRegisterAddress,
                            bBuffer, 
                            I2CTestData[BusIndex].dwNumOfBytes))
                    {
                            g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to write original value to register");
                    }
                }
                else
                {
                    g_pKato->Log(LOG_FAIL,L"Failed to write to Device");
                    dwRet = FAILURE;
                }
            }
            else
            {
                g_pKato->Log(LOG_FAIL,L"Hlp_I2CLoopback: FAIL: Please check Data size, size should be between 0 and %d", I2C_MDD_BUFFER_LENGTH-1);
                dwRet = FAILURE;
            }
        }
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CLoopback
//Test Helper for I2c bus write followed by read and data verification
//calls into the i2c MDD API
//
// Parameters:
//  HANDLE hI2C                 I2C device handle
//  I2CTRANS I2CTestData         Transaction Details
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CLoopback(HANDLE hI2C, I2CTRANS I2CTestData)
{
    DWORD dwRet=SUCCESS; 
    size_t Count=0;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
    
    if((I2CTestData.dwNumOfBytes>0) && (I2CTestData.dwNumOfBytes<I2C_MAX_LENGTH))
    {
        BYTE bWriteBuffer[I2C_MDD_BUFFER_LENGTH]={0};
        BYTE bReadBuffer[I2C_MDD_BUFFER_LENGTH]={0};
            
                    
        // Before writing new value to register, save the current value

        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:Reading the current value =>");
        if(TRUE== I2cRead(hI2C, 
                I2CTestData.dwRegisterAddress,
                bBuffer, 
                I2CTestData.dwNumOfBytes))
        {
            
            PrintBufferData(bBuffer, I2CTestData.dwNumOfBytes);
        }
        else
        {
            g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to read the current value of register");
        }

        g_pKato->Log(LOG_DETAIL, L"Performing WRITE operation ...");
        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:writing=>");
        PrintBufferData(I2CTestData.BufferPtr, I2CTestData.dwNumOfBytes);   

        if(TRUE== I2cWrite(hI2C, 
                I2CTestData.dwRegisterAddress,
                I2CTestData.BufferPtr, 
                I2CTestData.dwNumOfBytes))
        {
            g_pKato->Log(LOG_DETAIL, L"Performing READ operation ...");
            if(TRUE== I2cRead(hI2C, 
                            I2CTestData.dwLoopbackAddress,
                            bReadBuffer, 
                            I2CTestData.dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CLoopback:Read Data=>");
                PrintBufferData(bReadBuffer, I2CTestData.dwNumOfBytes);   
                
                //compare strings and then return success
                g_pKato->Log(LOG_DETAIL, L"Comparing the read data with written data ...");
                if(memcmp(
                           I2CTestData.BufferPtr,
                           bReadBuffer,
                           I2CTestData.dwNumOfBytes))
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: Read data not same as written data");
                    dwRet = FAILURE;
                }
            }
            else
            {
                g_pKato->Log(LOG_FAIL,L"Failed to Read from Device");
                dwRet = FAILURE;
            }

            // Restore the original value of the register
        
            g_pKato->Log(LOG_DETAIL,L"Hlp_I2CWrite:Writing the original value =>");
            PrintBufferData(bBuffer, I2CTestData.dwNumOfBytes);
            
            if(TRUE != I2cWrite(hI2C, 
                    I2CTestData.dwRegisterAddress,
                    bBuffer, 
                    I2CTestData.dwNumOfBytes))
            {
                    g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to write original value to register");
            }
        }
        else
        {
            g_pKato->Log(LOG_FAIL,L"Failed to write to Device");
            dwRet = FAILURE;
        }
    }
    else
    {
        g_pKato->Log(LOG_FAIL,L"Hlp_I2CLoopback: FAIL: Please check Data size, size should be between 0 and %d", I2C_MDD_BUFFER_LENGTH-1);
        dwRet = FAILURE;
    }
            
    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CRead
//Test Helper for I2c Bus Read, calls into the i2c MDD API
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CRead(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
    if(I2CTestData!=NULL)
    {
        if(I2CTestData!=NULL && ((&I2CTestData[BusIndex])!=NULL) &&
            (I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C) )
        {
            if(TRUE== I2cRead(I2CHandleTable[I2CTestData[BusIndex].HandleId].hI2C, 
                I2CTestData[BusIndex].dwRegisterAddress,
                bBuffer, 
                I2CTestData[BusIndex].dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read =>");
                PrintBufferData(bBuffer, I2CTestData[BusIndex].dwNumOfBytes);

                //not validating the case where data to be read is 0
                if((I2CTestData[BusIndex].BufferPtr[0]!=0))
                {
                    g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Validating the data read against data expected ...");
                    PrintBufferData(I2CTestData[BusIndex].BufferPtr, I2CTestData[BusIndex].dwNumOfBytes);
                    
                    if(!_memicmp(I2CTestData[BusIndex].BufferPtr, bBuffer, I2CTestData[BusIndex].dwNumOfBytes))
                    {
                        dwRet=SUCCESS;
                        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read data matches the expected data");
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read data doesn't match the expected data");
                    }
                }
                else
                {
                    //since there is no buffer data to validate, if read succeeds return success
                    dwRet=SUCCESS;
                    g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:No data to validate the data read");
                }
   
            }
        }
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_I2CRead
//Test Helper for I2c Bus Read, calls into the i2c MDD API
//
// Parameters:
//  HANDLE hI2C                 I2C Device Handle
//  I2CTRANS I2CTestData          I2C Transaction Details
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_I2CRead(HANDLE hI2C, I2CTRANS I2CTestData)
{
    DWORD dwRet=FAILURE;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
    
    if(TRUE== I2cRead(hI2C, 
                I2CTestData.dwRegisterAddress,
                bBuffer, 
                I2CTestData.dwNumOfBytes))
    {
        g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read =>");
        PrintBufferData(bBuffer, I2CTestData.dwNumOfBytes);

        //not validating the case where data to be read is 0
        if((I2CTestData.BufferPtr[0]!=0))
        {
            g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Validating the data read against data expected ...");
            PrintBufferData(I2CTestData.BufferPtr, I2CTestData.dwNumOfBytes);
            
            if(!_memicmp(I2CTestData.BufferPtr, bBuffer, I2CTestData.dwNumOfBytes))
            {
                dwRet=SUCCESS;
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read data matches the expected data");
            }
            else
            {
                g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:Read data doesn't match the expected data");
            }
        }
        else
        {
            //since there is no buffer data to validate, if read succeeds return success
            dwRet=SUCCESS;
            g_pKato->Log(LOG_DETAIL,L"Hlp_I2CRead:No data to validate the data read");
        }

    }
    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
//Hlp_SetupBusData
//Test Helper for Setting up the bus details, like open handle, calls into test
//helper functions
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////

DWORD Hlp_SetupBusData(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=FAILURE;

    dwRet=Hlp_OpenI2CHandle(I2CTestData, BusIndex);

    return dwRet;
}
////////////////////////////////////////////////////////////////////////////////
//Hlp_SetupDeviceData
//Test Helper for Setting up the Device details, like speed timeout Subordinate address etc, 
//calls into test helper functions
//
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  long BusIndex               Transaction Index
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD Hlp_SetupDeviceData(I2CTRANS *I2CTestData, long BusIndex)
{
    DWORD dwRet=SUCCESS;
    if(SUCCESS != Hlp_SetI2CSubordinateAddrSize(I2CTestData,BusIndex))
    {
        g_pKato->Log(LOG_DETAIL,L"Hlp_SetupDeviceData: Failure setting I2C Subordinate Addr Size");
        g_pKato->Log(LOG_DETAIL,L"I2C Driver will use the default Subordinate Addr Size");
        
    }
    if(SUCCESS != Hlp_SetI2CRegAddrSize(I2CTestData,BusIndex))
    {
        g_pKato->Log(LOG_DETAIL,L"Hlp_SetupDeviceData: Failure setting I2C Reg Addr Size");
        g_pKato->Log(LOG_DETAIL,L"I2C Driver will use the default Register Addr Size");
        
    }
    if(SUCCESS != Hlp_SetI2CSubordinate(I2CTestData,BusIndex))
    {
        g_pKato->Log(LOG_DETAIL,L"Hlp_SetupDeviceData: Failure setting I2C Subordinate");
        dwRet=FAILURE;

    }
     if(SUCCESS != Hlp_SetI2CSpeed(I2CTestData,BusIndex))
    {
        g_pKato->Log(LOG_FAIL,L"Hlp_SetupDeviceData: Failure setting I2C Speed");
        dwRet=FAILURE;
    }
     if(SUCCESS != Hlp_SetI2CTimeOut(I2CTestData,BusIndex))
    {
        g_pKato->Log(LOG_DETAIL,L"Hlp_SetupDeviceData: Failure setting I2C Time Out");
        g_pKato->Log(LOG_DETAIL,L"I2C Driver will use the default I2C Transaction Timeout");
        
    }
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//Hlp_GetNextTransIndex
//Test Helper for Getting the transaction for particular operation
// Parameters:
//  I2CTRANS *I2CTestData       Transaction List
//  I2C_OP OperationToFind      Operation to Find
//  long *pIndex                Transaction Index containing the operation
//
// Return value:
//  SUCCESS if successful, FAILURE to indicate an error condition.
////////////////////////////////////////////////////////////////////////////////
DWORD Hlp_GetNextTransIndex( I2CTRANS *I2CTestData, I2C_OP OperationToFind, long *pIndex)
{
    DWORD dwRet=FAILURE;
    BOOL bFound=FALSE;
    if(I2CTestData!=NULL)
    {
       for(int i=*pIndex; i<gI2CTransSize;i++)
         {
             if(&I2CTestData[i]!=NULL)
             {
                 if(I2CTestData[i].Operation==OperationToFind)
                 {
                      bFound=TRUE;
                      *pIndex=i;
                      g_pKato->Log(LOG_DETAIL,L"Hlp_GetNextTransIndex: Found Transaction with Trans ID: %d", I2CTestData[i].dwTransactionID);
                      dwRet=SUCCESS;
                      break;
                 }
             }
                
        } 

   }
   return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
//PrintBufferData
//Prints the buffer data
// Parameters:
//  BYTE *buffer -      Data buffer
//  DWORD dwLength - Data Size
// Return value:
//  None
////////////////////////////////////////////////////////////////////////////////
VOID PrintBufferData(BYTE *buffer, DWORD dwLength)
{
    for(UINT i=0; i< dwLength; i++)
    {
        g_pKato->Log(LOG_DETAIL,L"Byte %d :%x ", i, buffer[i]);
    }
}


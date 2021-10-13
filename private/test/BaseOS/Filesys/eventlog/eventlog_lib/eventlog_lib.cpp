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
#include <evntprov.h>
#include <evntcons.h>
#include <evntrace.h>
#include "main.h"
#include "Manifest\TestProv1.h"
#include "EventLog_globals.h"




static BOOL verifyMedStruct(CKato * g_pKato, MedStruct * s){
    DETAIL("Verifying MedStruct was not changed by EventLog");

    if(!s || s->nShort != 170){
        FAIL("MedStruct was modified!!  Failing!");
        return FALSE;
    }
    int i;
    for(i = 0; i < MEDSTRUCT_SIZEOF_ARRAY; i++)
    {
        if(s->arrByte[i] != g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE]){
            FAIL("MedStruct's byte array was modified!!  Failing!");
            return FALSE;
        }
    }

    DETAIL("MedStruct was not modified between eventwrite/eventread");
    
    return TRUE;
}

static BOOL initMedStruct(CKato * g_pKato, MedStruct * s){
    
    if(!s){
        return FALSE;
    }
    s->nShort = 170;
    for(int i = 0; i < MEDSTRUCT_SIZEOF_ARRAY; i++)
    {
        s->arrByte[i] = g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE];
    }
    
    return TRUE;
}
static BOOL verifySmallStruct(CKato * g_pKato, SmallStruct * s){
    DETAIL("Verifying SmallStruct was not changed by EventLog");

    if(!s || s->nShort != 170){
        FAIL("SmallStruct was modified!!  Failing!");
        return FALSE;
    }
    for(int i = 0; i < SMALLSTRUCT_SIZEOF_ARRAY; i++)
    {
        if(s->arrByte[i] != g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE]){
            FAIL("SmallStruct's byte array was modified!!  Failing!");
            return FALSE;
        }
    }

    DETAIL("SmallStruct was not modified between eventwrite/eventread");
    
    return TRUE;
}

static BOOL initSmallStruct(CKato * g_pKato, SmallStruct * s){
    if(!s){
        return FALSE;
    }
    s->nShort = 170;
    for(int i = 0; i < SMALLSTRUCT_SIZEOF_ARRAY; i++)
    {
        s->arrByte[i] = g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE];
    }
    
    return TRUE;
}

//returns required buffer size
DWORD WaitOnEvent(PVOID lpParameter)
{
    HANDLE hWaitHandle = lpParameter;
    EVENT_DESCRIPTOR EventDescriptor;
    DWORD cbEventData = 0;
    DWORD cEventDataDescriptors;
    while(WAIT_OBJECT_0 == WaitForSingleObject(hWaitHandle, INFINITE))
    {

        NKDbgPrintfW(L"Event Subscriber received event");
        LRESULT lResult = CeEventRead(
                hWaitHandle, 
                &EventDescriptor,
                NULL,
                0,
                &cbEventData, 
                &cEventDataDescriptors);
        if(lResult!=ERROR_SUCCESS || cbEventData <= 0){
            ASSERT(0);
            return 0;
        }
        
        break;
    }

    return cbEventData;
    
}


//////////////////////////////////////////////
//  Provider API Wrappers                   //
//////////////////////////////////////////////
static BOOL tstEventRegister(CKato * g_pKato, PREGHANDLE RegisteredHandle, int nProviderIndex, LONG lExpected)
{
    
    LONG lRetVal = ERROR_SUCCESS;
    DETAIL("Registering test provider");
    lRetVal = EventRegister(&g_arrProviderGUIDs[nProviderIndex], NULL, NULL, RegisteredHandle);
    if(lRetVal != lExpected){
        g_pKato->Log(LOG_FAIL, TEXT("Unexpcted return from EventRegister!, Expected: %d, Got: %d"), lExpected, lRetVal);
        return FALSE;
    }
    if(lExpected == ERROR_SUCCESS && *RegisteredHandle == NULL){
        ERRFAIL("EventRegister set RegisteredHandle to NULL, failing, error:%d", lRetVal);
        return FALSE;
    }
    g_pKato->Log(LOG_PASS, TEXT("EventRegister returned expected result (%d)"), lRetVal);
    return TRUE;
    
}
static BOOL tstEventRegister(CKato * g_pKato, PREGHANDLE RegisteredHandle, int nProviderIndex)
{
    return tstEventRegister(g_pKato, RegisteredHandle, nProviderIndex, ERROR_SUCCESS);
}
static BOOL tstEventRegister(CKato * g_pKato, PREGHANDLE RegisteredHandle)
{
    return tstEventRegister(g_pKato, RegisteredHandle, 0);
}

/*static BOOL tstEventRegister_P2C1(CKato * g_pKato, PREGHANDLE RegisteredHandle)
{
    BOOL bRetVal = TRUE;
    ULONG lRetVal = ERROR_SUCCESS;
    DETAIL("Registering test provider2 to channel 1");
    lRetVal = EventRegister(&P2C1_PROVIDER_GUID, NULL, NULL, RegisteredHandle);
    if(lRetVal != ERROR_SUCCESS){
        ERRFAIL("Failure registering test provider!, Error: %d", lRetVal);
        bRetVal = FALSE;
    }
    PASS("EventRegister returned no error");
    return bRetVal;
}*/


static BOOL tstEventUnRegister(CKato * g_pKato, PREGHANDLE RegisteredHandle, LONG lExpected)
{
    if(*RegisteredHandle == NULL){
        FAIL("tstEventUnRegsiter passed NULL handle, returning FALSE");
        return FALSE;
    }
    BOOL bRetVal = TRUE;
    LONG lRetVal = ERROR_SUCCESS;
    g_pKato->Log(LOG_DETAIL, TEXT("UnRegistering test provider, expect return of %d"), lExpected);
    lRetVal = EventUnregister((*RegisteredHandle));
    if(lRetVal != lExpected){
        ERRFAIL("EventUnregister did not return expected value!", lRetVal);
        bRetVal = FALSE;
    }
    PASS("EventUnregister returned expected value");
    return bRetVal;
}
static BOOL tstEventUnRegister(CKato * g_pKato, PREGHANDLE RegisteredHandle)
{
    return tstEventUnRegister(g_pKato, RegisteredHandle, ERROR_SUCCESS);
}


static BOOL tstEventEnabled(CKato * g_pKato, PREGHANDLE RegisteredHandle, PCEVENT_DESCRIPTOR EventDescriptor)
{

    BOOL bFuncReturn = TRUE;
    DETAIL("Test EventEnabled");
    bFuncReturn = EventEnabled(*RegisteredHandle, EventDescriptor);
    if(bFuncReturn == FALSE){
        DETAIL("EventEnabled returned false");
        return FALSE;
    }
    PASS("EventEnabled returned true");

    return TRUE;


}

static BOOL tstEventProviderEnabled(CKato * g_pKato, PREGHANDLE RegisteredHandle, UCHAR Level, ULONGLONG Keyword)
{
    BOOL bFuncReturn = TRUE;
    DETAIL("Test EventProviderEnabled");
    bFuncReturn = EventProviderEnabled(*RegisteredHandle, Level, Keyword);
    if(bFuncReturn == FALSE){
        PASS("EventProviderEnabled returned FALSE");
        return FALSE;
    }
    PASS("EventProviderEnabled returned TRUE");
    return TRUE;


}
static BOOL tstEventProviderEnabled(CKato * g_pKato, PREGHANDLE RegisteredHandle)
{
    DETAIL("Test EventProviderEnabled with default test keyword and level");
    return tstEventProviderEnabled(g_pKato, RegisteredHandle, DEFAULT_TEST_LEVEL, NO_KEYWORDS);
}

static BOOL tstEventWrite(CKato * g_pKato, 
                          PREGHANDLE RegisteredHandle, 
                          PCEVENT_DESCRIPTOR EventDescriptor, 
                          ULONG UserDataCount, 
                          PEVENT_DATA_DESCRIPTOR UserData,
                          ULONG lExpected)
{
    BOOL bRetVal = TRUE;
    ULONG lRetVal = ERROR_SUCCESS;
    DETAIL("Test EventWrite");
    lRetVal = EventWrite(*RegisteredHandle, EventDescriptor, UserDataCount, UserData);
    if(lRetVal != lExpected){
        ERRFAIL("Unexpected return from EventWrite!", lRetVal);
        bRetVal = FALSE;
        goto done;
    }
    PASS("Got expected return value from EventWrite");
done:
    return bRetVal;

}

static BOOL tstEventWrite(CKato * g_pKato, PREGHANDLE RegisteredHandle, int nTestEventIndex, int nChannelIndex, ULONG lExpected)
{
    g_pKato->Log(LOG_DETAIL, TEXT("Writing test message %d to channel %d using EventWrite.  Expecting %d"), nTestEventIndex, nChannelIndex, lExpected);
    int nDataCount = 0;
    
    //data
    DWORD MyDWORD = DEFAULT_TEST_USER_DATA;
    
    SmallStruct mySmallStruct;
    mySmallStruct.nShort = 34;
    //mySmallStruct.arrByte = {170};
    BigStruct myBigStruct;
    myBigStruct.nShort = 34;
    //myBigStruct.arrByte = {170};
    MedStruct myMedStruct;
    myMedStruct.nShort = 34;
    //mySmallStruct.arrByte = {170};

    SmallStruct mySmallStructArray[SMALL_STRUCT_ARRAY_COUNT];

    const EVENT_DESCRIPTOR * pEventDescriptor;
    PEVENT_DATA_DESCRIPTOR pDescriptorList = NULL;
    PEVENT_DATA_DESCRIPTOR pCurrDescriptor = NULL;
/*declare event descriptors
typedef struct _EVENT_DESCRIPTOR {

    USHORT      Id;
    UCHAR       Version;
    UCHAR       Channel;
    UCHAR       Level;
    UCHAR       Opcode;
    USHORT      Task;
    ULONGLONG   Keyword;

} EVENT_DESCRIPTOR, *PEVENT_DESCRIPTOR;*/
    EVENT_DESCRIPTOR midEvent;
    midEvent.Id = 3333;
    midEvent.Keyword = 0x0;

    switch(nTestEventIndex)
    {
        case TEST_EVENT_DWORD: //0
            nDataCount = 1;
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            EventDataDescCreate(pDescriptorList, &MyDWORD, sizeof(DWORD));
            break;
        case TEST_EVENT_STRUCTARRAY: //6
            nDataCount = 1;
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            for(int i = 0; i < SMALL_STRUCT_ARRAY_COUNT; i++){
                initSmallStruct(g_pKato, &mySmallStructArray[i]);
            }
            EventDataDescCreate(pDescriptorList, mySmallStructArray, sizeof(SmallStruct) * SMALL_STRUCT_ARRAY_COUNT);
            break;
        case TEST_EVENT_DWORDANDSTRINGANDSTRUCT: //3
            initSmallStruct(g_pKato, &mySmallStruct);
            nDataCount = 3;
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            EventDataDescCreate(pCurrDescriptor, &MyDWORD, sizeof(DWORD));
            pCurrDescriptor = (PEVENT_DATA_DESCRIPTOR)((BYTE *)pDescriptorList + sizeof(EVENT_DATA_DESCRIPTOR));
            EventDataDescCreate(pCurrDescriptor, gc_pMyString, 52);
            pCurrDescriptor = (PEVENT_DATA_DESCRIPTOR)((BYTE *)pCurrDescriptor + sizeof(EVENT_DATA_DESCRIPTOR));
            EventDataDescCreate(pCurrDescriptor, &mySmallStruct, sizeof(SmallStruct));
            break;
        case TEST_EVENT_DWORDANDSTRING: //1
            nDataCount = 2;
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            
            pCurrDescriptor = pDescriptorList;
            EventDataDescCreate(pCurrDescriptor, &MyDWORD, sizeof(DWORD));
            pCurrDescriptor = (PEVENT_DATA_DESCRIPTOR)((BYTE *)pDescriptorList + sizeof(EVENT_DATA_DESCRIPTOR));
            EventDataDescCreate(pCurrDescriptor, gc_pMyString, 52);
            break;
        case TEST_EVENT_TOOBIG: //4
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            nDataCount = 1;
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            EventDataDescCreate(pCurrDescriptor, &myBigStruct, sizeof(BigStruct));
            break;
        case TEST_EVENT_SMALLEVENT: //2
            initSmallStruct(g_pKato, &mySmallStruct);
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            nDataCount = 1;
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            EventDataDescCreate(pCurrDescriptor, &mySmallStruct, sizeof(SmallStruct));
            break;
        case TEST_EVENT_MEDEVENT: //5
            initMedStruct(g_pKato, &myMedStruct);
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            nDataCount = 1;
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            EventDataDescCreate(pCurrDescriptor, &myMedStruct, sizeof(MedStruct));
            break;
        case TEST_EVENT_MAXDATACOUNT: //7
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][nTestEventIndex];
            nDataCount = MAX_EVENT_DATA_DESCRIPTORS;
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            for(int i = 0; i < nDataCount; i++){
                EventDataDescCreate(pCurrDescriptor, &g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE], sizeof(BYTE));
                pCurrDescriptor = (PEVENT_DATA_DESCRIPTOR)((BYTE *)pCurrDescriptor + sizeof(EVENT_DATA_DESCRIPTOR));
            }
            break;
        case TEST_EVENT_OVERMAXDATACOUNT: //8
            pEventDescriptor = &g_arrDescriptors[nChannelIndex][TEST_EVENT_MAXDATACOUNT];
            nDataCount = MAX_EVENT_DATA_DESCRIPTORS + 1;
            pDescriptorList = (PEVENT_DATA_DESCRIPTOR)LocalAlloc(LPTR, nDataCount*sizeof(EVENT_DATA_DESCRIPTOR));
            if(NULL == pDescriptorList)
            {
                FAIL("Out of memory!");
                return FALSE;
            }
            pCurrDescriptor = pDescriptorList;
            for(int i = 0; i < nDataCount; i++){
                EventDataDescCreate(pCurrDescriptor, &g_arrTestBytes[i%TEST_BYTES_ARRAY_SIZE], sizeof(BYTE));
                pCurrDescriptor = (PEVENT_DATA_DESCRIPTOR)((BYTE *)pCurrDescriptor + sizeof(EVENT_DATA_DESCRIPTOR));
            }
            break;
        default:
            FAIL("Unrecognized event index passed to tstEventWrite, failing");
            return FALSE;
    }
    
    BOOL bRetVal = tstEventWrite(   g_pKato, 
                            RegisteredHandle, 
                            pEventDescriptor, 
                            nDataCount,
                            pDescriptorList,
                            lExpected);
    
    if(lExpected == ERROR_SUCCESS && bRetVal == TRUE){
        g_pKato->Log(LOG_DETAIL, TEXT("EventWritten  -TestEventIndex %d -ChannelIndex %d"), nTestEventIndex, nChannelIndex);
    }
    
    LocalFree(pDescriptorList);

    return bRetVal;
}
//////////////////////////////////////////////
//  Subscriber API Wrappers                 //
//////////////////////////////////////////////

static BOOL tstCeEventSubscribe(CKato * g_pKato, 
                         HANDLE * hOut,
                         LPCWSTR szChannelPath, 
                         DWORD grfSubscribeFlags, 
                         DWORD dwLevel, 
                         ULONGLONG ullMatchAnyKeywordMask, 
                         ULONGLONG ullMatchAllKeywordMask,
                         LONG lExpected)
{
    DETAIL("Calling CeEventSubscribe");
    (*hOut) = CeEventSubscribe(szChannelPath, grfSubscribeFlags, dwLevel, ullMatchAnyKeywordMask, ullMatchAllKeywordMask);
    if((*hOut) == NULL)
    {
        if(lExpected == ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, TEXT("CeEventSubscribe returned NULL handle!, error: %d"), GetLastError());
            return FALSE;
        }
        else if(lExpected == (LONG) GetLastError()){
            g_pKato->Log(LOG_FAIL, TEXT("CeEventSubscribe returned NULL handle, error: %d (as expected)"), GetLastError());
            return TRUE;
        }
        else{
            g_pKato->Log(LOG_FAIL, TEXT("CeEventSubscribe returned NULL handle, error: %d rather then error:%d as expected"), GetLastError(), lExpected);
            return FALSE;
        }
    }
    else{
        if(lExpected == ERROR_SUCCESS){
            g_pKato->Log(LOG_FAIL, TEXT("CeEventSubscribe returned valid handle as expected"), GetLastError());
            return TRUE;
        }
        else{
            g_pKato->Log(LOG_FAIL, TEXT("CeEventSubscribe returned valid handle rather then error:%d as expected"), lExpected);
            return FALSE;
        }
    }

}
static BOOL tstCeEventSubscribe(CKato * g_pKato, 
                         HANDLE * hOut,
                         LPCWSTR szChannelPath, 
                         DWORD grfSubscribeFlags, 
                         DWORD dwLevel, 
                         ULONGLONG ullMatchAnyKeywordMask, 
                         ULONGLONG ullMatchAllKeywordMask)
{
    return tstCeEventSubscribe( g_pKato, 
                                hOut, 
                                szChannelPath, 
                                grfSubscribeFlags, 
                                dwLevel, 
                                ullMatchAnyKeywordMask, 
                                ullMatchAllKeywordMask, 
                                ERROR_SUCCESS);
}

static BOOL tstCeEventSubscribe_System(CKato * g_pKato, HANDLE * hOut)
{
    DETAIL("Subscribing to built in System Channel");
    return tstCeEventSubscribe(g_pKato, hOut, L"System", NULL, 0x5, MATCH_ANY_KEYWORD_VERBOSE, MATCH_ALL_KEYWORD_VERBOSE);

}
/*static BOOL tstCeEventSubscribe_P2C1(CKato * g_pKato, HANDLE *hOut)
{
    DETAIL("Subscribing to Provider2, Channel1");
    return tstCeEventSubscribe(g_pKato,
                                hOut,
                                L"Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1",
                                DEFAULT_TEST_GRF_FLAGS,
                                DEFAULT_TEST_LEVEL,
                                DEFAULT_TEST_ANY_KEYWORD,
                                DEFAULT_TEST_ALL_KEYWORD);

}*/
static BOOL tstCeEventSubscribe(CKato * g_pKato, HANDLE * hOut, int nChannelIndex, LONG lExpected){
    //DETAIL("Calling CeEventSubscribe(default params)");
    return tstCeEventSubscribe(g_pKato, 
                        hOut, 
                        g_arrChannelPaths[nChannelIndex],
                        DEFAULT_TEST_GRF_FLAGS,
                        DEFAULT_TEST_LEVEL,
                        DEFAULT_TEST_ANY_KEYWORD,
                        DEFAULT_TEST_ALL_KEYWORD,
                        lExpected);
}


static BOOL tstCeEventSubscribe(CKato * g_pKato, HANDLE * hOut){
    DETAIL("Calling CeEventSubscribe(default test channel and params)");
    return tstCeEventSubscribe(g_pKato, 
                        hOut, 
                        DEFAULT_TEST_CHANNEL_PATH,
                        DEFAULT_TEST_GRF_FLAGS,
                        DEFAULT_TEST_LEVEL,
                        DEFAULT_TEST_ANY_KEYWORD,
                        DEFAULT_TEST_ALL_KEYWORD);
}

static BOOL tstCeEventSubscribe_level(CKato * g_pKato, HANDLE * hOut, DWORD dwLevel){
    DETAIL("Calling CeEventSubscribe(default test channel), user specified level");
    return tstCeEventSubscribe(g_pKato, 
                        hOut, 
                        DEFAULT_TEST_CHANNEL_PATH,
                        DEFAULT_TEST_GRF_FLAGS,
                        dwLevel,
                        DEFAULT_TEST_ANY_KEYWORD,
                        DEFAULT_TEST_ALL_KEYWORD);
}


static BOOL tstCeEventNext(CKato * g_pKato, HANDLE hSub, ULONG lExpected)
{
    DETAIL("Calling CeEventNext");
    ULONG lRetVal = ERROR_SUCCESS;

    lRetVal = CeEventNext(hSub);
    if(lRetVal != lExpected)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Unexpected return value from CeEventNext, expected: %d, got: %d"), lExpected, lRetVal);
        return FALSE;
    }
    PASS("Got expected return value from CeEventNext");
    return TRUE;
}

static BOOL tstCeEventRead(CKato * g_pKato, 
                           HANDLE hSub, 
                           PEVENT_DESCRIPTOR pEventDescriptor, 
                           PEVENT_DATA_DESCRIPTOR pUserData, 
                           ULONG cUserData,
                           ULONG * pcUserData,
                           ULONG * pUserDataBytes,
                           ULONG lExpected,
                           const int nExpectedEventIndex,
                           const int nExpectedChannelIndex)
{
    DETAIL("Calling CeEventRead");
    ULONG lRetVal = ERROR_SUCCESS;

    lRetVal = CeEventRead(hSub, pEventDescriptor, pUserData, cUserData, pUserDataBytes, pcUserData);
    if(lRetVal != lExpected)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Unexpected return value from CeEventRead, expected: %d, got: %d"), lExpected, lRetVal);
        return FALSE;
    }
    if(lRetVal == ERROR_SUCCESS){
        switch (nExpectedEventIndex)
        {
            case TEST_EVENT_NO_VERIFY:
                DETAIL("no verification specified, will not verify event data");
                break;
            case TEST_EVENT_DWORD:
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR))!=0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                if(*(DWORD*)(pUserData->Ptr) != DEFAULT_TEST_USER_DATA){
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                break;
            case TEST_EVENT_STRUCTARRAY:
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR)) != 0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                for(int i = 0; i < SMALL_STRUCT_ARRAY_COUNT; i++){
                    if(!verifySmallStruct(g_pKato, (SmallStruct*) (pUserData->Ptr + sizeof(SmallStruct)*i))){
                        FAIL("Verification of small struct failed, failing");
                        return FALSE;
                    }
                }
                break;
            case TEST_EVENT_DWORDANDSTRINGANDSTRUCT: 
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR)) != 0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                if(*(DWORD*)(pUserData->Ptr) != DEFAULT_TEST_USER_DATA){
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                if(memcmp((VOID*)(pUserData->Ptr + sizeof(DWORD)), gc_pMyString, 52)!=0 ){
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                if(!verifySmallStruct(g_pKato, (SmallStruct*) (pUserData->Ptr + sizeof(DWORD) + 52))){
                    FAIL("Verification of small struct failed, failing");
                    return FALSE;
                }
                break;

            case TEST_EVENT_DWORDANDSTRING:
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR)) != 0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                if(*(DWORD*)(pUserData->Ptr) != DEFAULT_TEST_USER_DATA){
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                if(memcmp((VOID*)(pUserData->Ptr + sizeof(DWORD)), gc_pMyString, 52)!=0 ){
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                break;

            case TEST_EVENT_MEDEVENT:
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR)) != 0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                if(!verifyMedStruct(g_pKato, (MedStruct*) pUserData->Ptr))
                {
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                break;
            case TEST_EVENT_SMALLEVENT:
                
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR))!=0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                if(!verifySmallStruct(g_pKato, (SmallStruct*) pUserData->Ptr))
                {
                    FAIL("Event data did not match expected!!!!, failing");
                    return FALSE;
                }
                break;
            case TEST_EVENT_MAXDATACOUNT:
                if(memcmp(pEventDescriptor, &g_arrDescriptors[nExpectedChannelIndex][nExpectedEventIndex], sizeof(EVENT_DESCRIPTOR))!=0){
                    FAIL("Event descriptor did not match expected, failing");
                    return FALSE;
                }
                for(int i = 0; i < MAX_EVENT_DATA_DESCRIPTORS; i++){
                    if(*(DWORD*)(pUserData[i].Ptr) != DEFAULT_TEST_USER_DATA){
                        g_pKato->Log(LOG_FAIL, TEXT("Data verification of max data descrip event failed on element %d"), i);
                        return FALSE;
                    }
                }

            default:
                FAIL("Invalid nExpectedEventIndex passed to tstCeEventRead, failing!");
                return FALSE;
                break;
        }
        DETAIL("Event Verification succeeded.  Got what was expected");
    }
    else{
        DETAIL("CeEventRead call not expected to pass, skipping verification of event");
    }
    PASS("Got expected return value from CeEventRead");
    return TRUE;
}
static BOOL tstCeEventRead_NoWait(CKato * g_pKato,
                           HANDLE hSub,
                           ULONG lExpected,
                           const int nTestEventIndex,
                           const int nTestChannelIndex)
{
    g_pKato->Log(LOG_DETAIL, TEXT("Calling CeEventRead with default test args, expect return of %d"), lExpected);
    EVENT_DESCRIPTOR Descriptor;
    BYTE DataBuffer[MED_STRUCT_SIZE + 2000]; 
    DWORD cbEventData = 0;
    DWORD cEventDataDescriptors = 0;
    LRESULT lResult = tstCeEventRead(  
                        g_pKato,
                        hSub,
                        &Descriptor,
                        (PEVENT_DATA_DESCRIPTOR) DataBuffer,
                        MED_STRUCT_SIZE + 2000,
                        &cbEventData,
                        &cEventDataDescriptors,
                        lExpected,
                        nTestEventIndex,
                        nTestChannelIndex);
    if(lResult == ERROR_MORE_DATA){
        g_pKato->Log(LOG_DETAIL, TEXT("EventRead says a buffer of %d is required for this event"), cbEventData);
    }
    return lResult;
}

static BOOL tstCeEventRead(CKato * g_pKato, HANDLE hSub, ULONG lExpected, const int nTestEventIndex, const int nTestChannelIndex){
    g_pKato->Log(LOG_DETAIL, TEXT("tstCeEventRead: Read event index %d, expect return of %d"), nTestEventIndex, lExpected);
    return tstCeEventRead_NoWait(g_pKato, hSub, lExpected, nTestEventIndex, nTestChannelIndex);
}

static BOOL tstCeEventClose(CKato * g_pKato,
                            HANDLE hSub)
{
    if(hSub == NULL){
        FAIL("tstCeEventClose was passed a null handle, returning false");
        return FALSE;
    }
    DETAIL("Calling CeEventClose");
    ULONG lRetVal = ERROR_SUCCESS;
    lRetVal = CeEventClose(hSub);
    if(lRetVal != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Non 0 return from CeEventClose, expected: %d, got: %d"), ERROR_SUCCESS, lRetVal);
        return FALSE;
    }
    PASS("Got expected return value from CeEventClose");
    return TRUE;
}

/*static BOOL removeChannelKey(CKato * g_pKato, LPTSTR szChannelName)
{
    LONG lResult = 0;
    TCHAR szTemp[MAX_PATH];

    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s"), szChannelName);

    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail");
    
    lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp);
    if(lResult != ERROR_SUCCESS){
        return FALSE;
    }

    DETAIL("CAUTION (end): Registry modification complete.");

    return TRUE;


}*/


static BOOL addChannelKeys(
                           CKato * g_pKato,
                           LPTSTR szChannelName, 
                           DWORD dwLevel, 
                           DWORD dwChannelType, 
                           DWORD dwChannelValue, 
                           DWORD dwMaxSize, 
                           DWORD dwMaxBuffers,
                           DWORD dwDefaultEnable,
                           LONG ulAnyKeywordHigh, 
                           LONG ulAnyKeywordLow, 
                           LONG ulAllKeywordHigh, 
                           LONG ulAllKeywordLow)
{
    LONG lResult = 0;
    HKEY hCurr;
    DWORD dwDisposition = 0;
    DWORD dwData = 1;
    TCHAR szTemp[MAX_PATH];

    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s"), szChannelName);

    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail")
    
    lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwChannelType;
        lResult = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwDefaultEnable;
        lResult = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwChannelValue;
        lResult = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
    RegCloseKey(hCurr);

    //if(bLoggingEnabled){ always log for API and BVT suite
        StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s\\Logging"), szChannelName);

            lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            lResult = RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwMaxSize, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            dwData = dwLevel;
            lResult = RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            dwData = ulAllKeywordHigh;
            lResult = RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            dwData = ulAllKeywordLow;
            lResult = RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            dwData = ulAnyKeywordHigh;
            lResult = RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }
            dwData = ulAnyKeywordLow;
            lResult = RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
            if(lResult != ERROR_SUCCESS){
                return FALSE;
            }

        RegCloseKey(hCurr);
    //}

    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s\\Publishing"), szChannelName);
    lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwLevel;
        lResult = RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = ulAllKeywordHigh;
        lResult = RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = ulAllKeywordLow;
        lResult = RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = ulAnyKeywordHigh;
        lResult = RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = ulAnyKeywordLow;
        lResult = RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = 0;
        lResult = RegSetValueEx(hCurr, L"ClockType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = 2;
        lResult = RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwMaxBuffers;
        lResult = RegSetValueEx(hCurr, L"MaxBuffers", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }

    RegCloseKey(hCurr);

    DETAIL("CAUTION (end): Registry modification complete.");

    return TRUE;


}

                           




///////////////////
static BOOL verifyProviderRegKeys(CKato * g_pKato, int /*nProvider*/) //nProvider is not honored
{
    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail")
    LONG funcRet = 0;
    HKEY hCurr;
    DWORD dwDisposition = 0;
    DWORD dwData = 1;

    

//Provider 3  - invalid reg settings test
    //Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 1;
    funcRet = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 105;
    funcRet = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1\Logging
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1\\Logging", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 8192+40;
    funcRet = RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 5;
    funcRet = RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 2;
    funcRet = RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));

    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1\Publishing
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1\\Publishing", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"ClockType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = TEST_CHANNEL_1_BUFFER_SIZE;
    funcRet = RegSetValueEx(hCurr, L"MaxBuffers", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));

    RegCloseKey(hCurr);
//Don't add values for several of these keys to verify correct default values used
//MyChannel
    //\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 1;
    //funcRet = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    //funcRet = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 101;
    funcRet = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational\Logging
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational\\Logging", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 8192+40;
    funcRet = RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational\Publishing
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational\\Publishing", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"ClockType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 2;
    //funcRet = RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = TEST_CHANNEL_1_BUFFER_SIZE;
    funcRet = RegSetValueEx(hCurr, L"MaxBuffers", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));

    RegCloseKey(hCurr);

//TestProvider2 Channel 1
    //\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 1;
    funcRet = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 103;
    funcRet = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1\Logging
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1\\Logging", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 8192+40;
    funcRet = RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 5;
    funcRet = RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 2;
    funcRet = RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    
    RegCloseKey(hCurr);

//\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1\Publishing
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1\\Publishing", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"ClockType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = TEST_PROV2_CHANNEL_1_BUFFER_SIZE;
    funcRet = RegSetValueEx(hCurr, L"MaxBuffers", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

//TestProvider2 Channel 2
    //\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 1;
    funcRet = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 104;
    funcRet = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

    //\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2\Logging
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2\\Logging", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    dwData = 8192+40;
    funcRet = RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 0x03;

    funcRet = RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));

        dwData = 0x200;

    
    funcRet = RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 0; 
    funcRet = RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 0;
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    funcRet = RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = 2;
    funcRet = RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    
    RegCloseKey(hCurr);

    //\System\EventLog\Channel\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2\Publishing
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Channel\\Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2\\Publishing", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);

    funcRet = RegSetValueEx(hCurr, L"ClockType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    dwData = TEST_PROV2_CHANNEL_2_BUFFER_SIZE;
    //funcRet = RegSetValueEx(hCurr, L"MaxBuffers", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    RegCloseKey(hCurr);

    for(int i = 0; i < VALID_TEST_PROVIDER_COUNT; i++)
    {
        funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, g_arrProviderRegPaths[i], 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)g_arrProviderChannelNameValues[i], MAX_PATH);
    RegCloseKey(hCurr);
    }
    /*
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{272BDCE3-4689-4b8b-8169-E5B2AAF8A0A1}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("System\0Microsoft-WindowsCE-EventLogTestPublisher-Operational\0\0"), MAX_PATH);
    RegCloseKey(hCurr);

    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{694A54A6-80E1-42b4-AF1E-0369F22FFB14}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov2-C1\0Microsoft-WindowsCE-EventLogTestPublisher-Analytic-Prov2-C2\0\0"), MAX_PATH);
    RegCloseKey(hCurr);

    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{80945A9C-F7B4-4c2b-B628-259724433ACC}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("Microsoft-WindowsCE-EventLogTestPublisher-Operational-Prov3-C1\0\0"), MAX_PATH);
    RegCloseKey(hCurr);
    //application channel provider
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{CB707539-FFD6-4840-A023-8C9AAEAA142D}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("Application\0\0"), MAX_PATH);
    RegCloseKey(hCurr);
    //system channel provider
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{544A3F3D-03C1-4a7a-9019-D91F14E9C530}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("System\0\0"), MAX_PATH);
    RegCloseKey(hCurr);
    //security channel provider
    funcRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, L"System\\EventLog\\Provider\\{E6043603-7F37-472e-9EDB-7B84E19E93BE}\\Channels", 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    funcRet = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE)TEXT("Security\0\0"), MAX_PATH);
    RegCloseKey(hCurr);*/

    DETAIL("CAUTION (end): Registry modification complete.");
    return TRUE;


}


BOOL tstLogFileACL(CKato * g_pKato, WCHAR * szLogFileName, LRESULT lExpected)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    TCHAR szLog[MAX_PATH] = {0};
    StringCchPrintf(szLog, MAX_PATH, TEXT("%s\\%s"), LOGLOCATION, szLogFileName);

    hFile = CreateFile(szLog, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    if(hFile == INVALID_HANDLE_VALUE && lExpected == ERROR_SUCCESS){
        ERRFAIL("Failed to open handle to log file!", GetLastError());
        return FALSE;
    }
    else if(hFile == INVALID_HANDLE_VALUE && lExpected == ERROR_ACCESS_DENIED){
        DETAIL("Failed to open handle to log file as expected");
        return TRUE;
    }
    else if(hFile != INVALID_HANDLE_VALUE && lExpected == ERROR_SUCCESS){
        DETAIL("Successfully opened handle to log file as expected");
        CloseHandle(hFile);
        return TRUE;
    }
    else if(hFile != INVALID_HANDLE_VALUE && lExpected == ERROR_ACCESS_DENIED){
        ERRFAIL("Successfully opened handle to log file! This was not expected!", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }
    if(hFile != INVALID_HANDLE_VALUE){
        CloseHandle(hFile);
    }
    FAIL("Unhandled result in tstLogFileACL!");
    return FALSE;
    
}
#ifdef EVENTLOG_API
static BOOL removePublishingKey(CKato * g_pKato, int nChannelIndex)
{
    BOOL bRetVal = TRUE;
        
    TCHAR szTemp[MAX_PATH];

    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s\\Publishing"), g_arrChannelPaths[nChannelIndex]);
    if(RegDeleteKey(HKEY_LOCAL_MACHINE, &szTemp[0]) != ERROR_SUCCESS){
        return FALSE;
    }
    return TRUE;
}

#endif


#ifdef EVENTLOG_BVT

//helper functions
static BOOL flushEvents(CKato * g_pKato, HANDLE hSub)
{
    while(CeEventNext(hSub) == ERROR_SUCCESS)
    {DETAIL("flushed event");}
    return TRUE;

}


static BOOL changeLogging(CKato * g_pKato, DWORD dwChannelIndex, BOOL bLogOn){
    TCHAR szTemp[MAX_PATH];
    LONG lResult = ERROR_SUCCESS;

    StringCchPrintf((LPTSTR) szTemp, MAX_PATH, TEXT("System\\Eventlog\\Channel\\%s\\Logging"), g_arrChannelPaths[dwChannelIndex]);

    if(!bLogOn){
        lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, &szTemp[0]);
        if(lResult != ERROR_SUCCESS && lResult != ERROR_FILE_NOT_FOUND){
            ERRFAIL("RegDeleteKey failed", GetLastError());
            return FALSE;
        }
        return TRUE;
    }
    else{
        HKEY hCurr;
        DWORD dwDisposition = 0;
        DWORD dwData = 0;
        if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, szTemp, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition) != ERROR_SUCCESS){
            ERRFAIL("RegCreateKeyFailed", GetLastError());
            return FALSE;
        }
        
        dwData = 8192+40;
        RegSetValueEx(hCurr, L"MaxSize", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        dwData = 5;
        RegSetValueEx(hCurr, L"Level", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
    
        dwData = 0;
        RegSetValueEx(hCurr, L"AllKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        RegSetValueEx(hCurr, L"AllKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        RegSetValueEx(hCurr, L"AnyKeywordHigh", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        RegSetValueEx(hCurr, L"AnyKeywordLow", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        dwData = 2;
        RegSetValueEx(hCurr, L"Latency", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));

        RegCloseKey(hCurr);
    }
    return TRUE;
}

static BOOL changeProviderKey_PublishTo(CKato * g_pKato, WCHAR * szKeyPath, WCHAR * szChannels)
{
    BOOL bRetVal = TRUE;
    HKEY hCurr;
    DWORD dwDisposition = 0;
    
    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail")
    
    bRetVal = RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKeyPath, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
    if(!bRetVal){goto done;}
    bRetVal = RegSetValueEx(hCurr, L"ChannelNames", 0, REG_MULTI_SZ, (LPBYTE) szChannels, MAX_PATH);
    if(!bRetVal){goto done;}
done:
    DETAIL("CAUTION(end): Registry modification complete.");
    bRetVal = RegCloseKey(hCurr);
    return bRetVal;
    
    

}

static BOOL removeOptionalChannelValues(CKato * g_pKato, LPTSTR szChannelName, DWORD dwChannelValue)
{
    LONG lResult = 0;
    HKEY hCurr;
    DWORD dwDisposition = 0;
    DWORD dwData = 1;
    TCHAR szTemp[MAX_PATH];

    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s"), szChannelName);

    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail");
    
    lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp);
    if(lResult != ERROR_SUCCESS){
        return FALSE;
    }
    lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp, 0, L"REG_BINARY", 0, 0, NULL, &hCurr, &dwDisposition);
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        /*dwData = dwChannelType;
        lResult = RegSetValueEx(hCurr, L"ChannelType", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
        dwData = dwDefaultEnable;
        lResult = RegSetValueEx(hCurr, L"DefaultEnable", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }*/
        dwData = dwChannelValue;
        lResult = RegSetValueEx(hCurr, L"Value", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
        if(lResult != ERROR_SUCCESS){
            return FALSE;
        }
    RegCloseKey(hCurr);

    DETAIL("CAUTION (end): Registry modification complete.");

    return TRUE;


}



static BOOL disableChannelLogging(CKato * g_pKato, LPTSTR szChannelName)
{
    LONG lResult = 0;
    WCHAR szTemp[MAX_PATH];
    
    StringCchPrintf((LPTSTR)szTemp, MAX_PATH, TEXT("System\\EventLog\\Channel\\%s\\Logging"), szChannelName);
    DETAIL("CAUTION: Registry is being modified.  Stopping on a break point during this phase ");
    DETAIL("         may cause timing problems and cause test to fail")
    
    lResult = RegDeleteKey(HKEY_LOCAL_MACHINE, (LPTSTR) szTemp);
    if(lResult != ERROR_SUCCESS){
        return FALSE;
    }
    DETAIL("CAUTION(end): Registry modification complete");
    
    return TRUE;
}
static BOOL addChannelKeys(CKato * g_pKato,
                           LPTSTR szChannelName, 
                           int nChannelId,
                           DWORD dwDefaultEnable)
{
    return addChannelKeys(     g_pKato,
                                szChannelName, 
                               DEFAULT_TEST_LEVEL,
                               1, 
                               nChannelId, 
                               MAX_EVENTLOG_CHANNEL,
                               MAX_USER_DATACOUNT, 
                               dwDefaultEnable,
                               0, 
                               0,
                               0,
                               0);


}



static BOOL addChannelKeys(CKato * g_pKato,
                           LPTSTR szChannelName, 
                           int nChannelId,
                           DWORD dwLevel, 
                           ULONGLONG ullAnyKeyword, 
                           ULONGLONG ullAllKeyword,
                           DWORD dwChannelSize,
                           DWORD dwMaxBuffers)
{
return addChannelKeys(     g_pKato, 
                           szChannelName, 
                           dwLevel, 
                           1, 
                           nChannelId, 
                           dwChannelSize, 
                           dwMaxBuffers, 
                           1,
                           (ULONG) ullAnyKeyword, 
                           (ULONG) (ullAnyKeyword << sizeof(ULONG)), 
                           (ULONG) ullAllKeyword, 
                           (ULONG) (ullAllKeyword << sizeof(ULONG)));

}
static BOOL tstCeEventSubscribe(CKato * g_pKato, HANDLE * hOut, ULONGLONG ullMatchAnyKeywordMask, ULONGLONG ullAllKeywordMask){
    DETAIL("Calling CeEventSubscribe(default test channel), user specified keyword combo");
    return tstCeEventSubscribe(g_pKato, 
                        hOut, 
                        DEFAULT_TEST_CHANNEL_PATH,
                        DEFAULT_TEST_GRF_FLAGS,
                        DEFAULT_TEST_LEVEL,
                        ullMatchAnyKeywordMask,
                        ullAllKeywordMask);
}
static BOOL tstCeEventSubscribe(CKato * g_pKato, HANDLE * hOut, int nChannelIndex){
    DETAIL("Calling CeEventSubscribe(default params)");
    return tstCeEventSubscribe(g_pKato, 
                        hOut, 
                        nChannelIndex,
                        ERROR_SUCCESS);
}

static BOOL tstEventProviderEnabled(CKato * g_pKato, PREGHANDLE RegsiteredHandle, BOOL bExpected)
{
    BOOL bRetVal = tstEventProviderEnabled(g_pKato, RegsiteredHandle);
    if(bRetVal == bExpected){
        return TRUE;
    }
    return FALSE;
}

static BOOL tstEventEnabled(CKato * g_pKato, PREGHANDLE RegisteredHandle, int nChannelIndex, int nEventIndex, BOOL bExpected)
{
    BOOL bRetVal = tstEventEnabled(g_pKato, RegisteredHandle, &g_arrDescriptors[nChannelIndex][nEventIndex]);
    if(bRetVal == bExpected){
        DETAIL("EventEnabled returned expected results");
        return TRUE;
    }
    ERRFAIL("EventEnabled returned unexpected result", (int) bRetVal);
    return FALSE;
}
#endif
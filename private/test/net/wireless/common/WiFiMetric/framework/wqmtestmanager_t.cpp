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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) Microsoft Corporation. All rights reserved.

-----------------------------------------------------------------------------

File Name:	WQMTestManager_t.cpp


-----------------------------------------------------------------------------
--*/

#include <windows.h>
#include "timer.h"
#include <Msgqueue.h>
#include <Winbase.h>
#include <tchar.h>
#include <stdio.h>
#include "WiFUtils.hpp"
#include <CmdArgList_t.hpp>
#include <tux.h>
#include "LogWrap.h"
#include "CmdArg_t.hpp"
#include "WQMUtil.hpp"
#include "WQMTestSuiteBase_t.hpp"
#include "WQMTestCase_t.hpp"
#include "WQMTuxTestSuite_t.hpp"
#include "WQMTestManager_t.hpp"
#include "Cmnprint.h"

TCHAR* g_pTestLogFile  = TEXT("WiFiMetrics");

using namespace litexml;
using namespace ce;
using namespace ce::qa;

#define EXTRA_TUX_FUNC_ENTRY    (int)10

// Header blk options
#define WIFI_METRIC_STR_ISRANDOM            (_T("IsRandom"))
#define WIFI_METRIC_MULTI_THRD              (_T("IsMultiThrd"))
#define WIFI_METRIC_STR_BVT                 (_T("BvtTestRun"))
#define WIFI_METRIC_STR_FAILCOUNT           (_T("MaxFailCount"))
#define WIFI_METRIC_STR_TUXWRAP             (_T("IsTuxWrap"))
#define WIFI_DEFAULT_CONFIG_FILE            (_T("\\release\\WQMConfigs\\WiFiMetrics.xml"))


// Sleep time
#define WIFI_METRIC_WORKERTHRD_SLEEP_TIME   ((int) 100) // 100 millisec
#define WIFI_METRIC_MAX_MSGQ_SIZE           ((int) 20)

static WQMTestManager_t*  s_SingleTon = NULL;
static TCHAR* s_Pass        = TEXT("PASSED");
static TCHAR* s_Skip        = TEXT("SKIPPED");
static TCHAR* s_Fail        = TEXT("FAILED");
static TCHAR* s_Mix         = TEXT("MIXED"); 
static TCHAR* s_TestResult  = TEXT("TestResult");
static TCHAR* s_PassPerc    = TEXT("PassPercent");
static TCHAR* s_TotalPass   = TEXT("TotalPass");
static TCHAR* s_TotalFail   = TEXT("TotalFail");
static TCHAR* s_TotalSkip   = TEXT("TotalSkip");


// Register the default logging verbosity level:
static int s_LogVerbosity = LibPrint_SetLogLevel("WM", LOG_DETAIL-1);

/*
		LibPrint("WM",PT_FAIL, TEXT("[BC] Couldn't resolve the server name/address!( %s )"), ErrorToText( iGAIResult) );
		LibPrint("WM",PT_LOG, TEXT("[BC] Server - %hs, Port - %hs"), szServer, szPort);

*/
// -----------------------------------------------------------------------------
//
// Pretty-prints the specified XML document.
//
static DWORD
FormatXmlDocument(
    const XmlDocument_t &Doc,
    XmlString_t         *pOut)
{
    // Convert to text.
    XmlString_t xmlText;
    Doc.ToString(xmlText);
    const WCHAR *pXml = &xmlText[0];
    const WCHAR *pEnd = &xmlText[xmlText.length()];

    // Reserve space for the output.
    pOut->empty();
    if (!pOut->reserve(xmlText.length() * 3))
    {
	 LibPrint("WM",PT_FAIL, TEXT("[WM] Can't allocate %ld bytes for output buffer"),
                 xmlText.length() * 3 );		
        return ERROR_OUTOFMEMORY;
    }

    // Split into lines and indent properly.
    static const int MaxIndent = 20;
    static const int IndentSize = 4;
    WCHAR prefix[MaxIndent * IndentSize + 4];
    int indent = 0;
    prefix[0] = 0;

    enum State_e { Idle, Tag, EndTag, Text } state = Idle;

    while (pXml < pEnd)
    {
        WCHAR lineBuffer[210];
        WCHAR *pLine = &lineBuffer[0];
        WCHAR *pLEnd = &lineBuffer[COUNTOF(lineBuffer)-10];

        while (pLine < pLEnd && pXml < pEnd)
        {
            switch (pXml[0])
            {
                case L'\\':
                    *(pLine++) = *(pXml++);
                    if (pXml < pEnd)
                        *(pLine++) = *(pXml++);
                    break;

                case L'<':
                    if (Text == state && &lineBuffer[0] != pLine)
                        goto appendLine;
                   *(pLine++) = *(pXml++);
                    if (L'?' != pXml[0])
                        state = Tag;
                    break;

                case L'>':
                   *(pLine++) = *pXml;
                    if (&lineBuffer[1] != pLine)
                        goto appendLine;
                    pLine--; pXml++;
                    if (Tag == state)
                    {
                        if (MaxIndent > indent)
                        {
                            WCHAR *pPref = &prefix[indent++ * IndentSize];
                            for (int ix = 0 ; ix < IndentSize ; ++ix, ++pPref)
                               *pPref = L' ';
                           *pPref = 0;
                        }
                        state = Text;
                    }
                    break;

                case L'/':
                   *(pLine++) = *(pXml++);
                    if (Tag == state)
                    {
                        if (0 < indent)
                            prefix[--indent * IndentSize] = 0;
                        state = EndTag;
                    }
                    break;

                default:
                   *(pLine++) = *(pXml++);
                    break;
            }
        }

appendLine:
        if (&lineBuffer[0] != pLine)
        {
           *pLine = 0;
            if (!pOut->append(prefix)
             || !pOut->append(lineBuffer)
             || !pOut->append(L"\n"))
            {


		 LibPrint("WM",PT_FAIL, _T("[WM] Can't append %ld bytes for output buffer"),
                         pOut->capacity() );	
                         
                return ERROR_OUTOFMEMORY;
            }
        }
    }

    return NO_ERROR;
}

static DWORD
WriteResultXmlFile(
    const XmlDocument_t& doc, 
    const TCHAR *pFileName)
{
    HRESULT hr;
    DWORD result;
    
    XmlString_t xmlText;
    result = FormatXmlDocument(doc, &xmlText);
    if (NO_ERROR != result)
        return result;

    ce::string mbText;
    hr = WiFUtils::ConvertString(&mbText, xmlText, "XML document text",
                                          xmlText.length(), CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    LibPrint("WM",PT_LOG, TEXT("[WM] Writing %lu byte XML document to \"%s\""),
             mbText.length(), pFileName);

    errno_t err;
    FILE* pFile    = NULL;      

    err = _tfopen_s(&pFile, pFileName,_T("w"));    
    if(!pFile || err !=0)
    {
        result = GetLastError();

        LibPrint("WM",PT_FAIL, TEXT("[WM] Can't open output file \"%s\": %s"),
                 pFileName, Win32ErrorText(result));
        return result;
    }
    
    size_t written = fwrite(&mbText[0], 1, mbText.length(), pFile);
    if (written != mbText.length())
    {
        result = GetLastError();
        LibPrint("WM",PT_FAIL, TEXT("[WM] Can't write %lu bytes to file \"%s\": %s"),
                 mbText.length(), pFileName, Win32ErrorText(result));	
        fclose(pFile);
        return result;
    }

    LibPrint("WM",PT_LOG,TEXT("[WM] Output to file \"%s\" succeeded"), pFileName);
    fclose(pFile);
    return NO_ERROR;
}

// Ctor/Dtor
WQMTestManager_t*
ce::qa::    
GetTestManager(void)
{   
    if(!s_SingleTon)
    {
        s_SingleTon = new WQMTestManager_t();
        assert(s_SingleTon);
    }

    return s_SingleTon;
}

void
ce::qa::    
DestroyTestManager(void)
{   
    if(s_SingleTon)
    {
        delete s_SingleTon;
        s_SingleTon = NULL;
    }

    return;
}

static void WorkerThreadProc(WQMThread* threadData)
{

    while(!threadData->toExit)
    {
        WQMTestCase_t* testCase = NULL;
        DWORD          readSize = 0;
        DWORD          flag     = 0;

        
        // None blocking call
        if(!ReadMsgQueue(threadData->msgQRHandle,
                                    &testCase,
                                    sizeof(testCase),
                                    &readSize,
                                    0,
                                    &flag))
        {
            // Only quit when error is not time out error
            if(GetLastError() != ERROR_TIMEOUT)
            {
                LibPrint("WM",PT_FAIL,_T("[WM] Read Msg failed "));
                threadData->bad = TRUE;
                return;
            }
            else
            {
                // Will sleep for a while before try again
                Sleep(WIFI_METRIC_WORKERTHRD_SLEEP_TIME);
                continue;
            }
        }


        if(readSize != sizeof(testCase))
        {
            LibPrint("WM", PT_WARN1, _T("[WM] Read Invalid message"));
            continue;
        }
       
        if(testCase->Execute() != TPR_PASS)
            threadData->interimResult = FALSE;
          
    }

    // This means the Main thread want to kill this thread
    MSGQUEUEINFO msgQInfo;
    if(GetMsgQueueInfo(threadData->msgQRHandle,
                                   &msgQInfo))
    {
        if(msgQInfo.dwCurrentMessages)
        {
             LibPrint("WM", PT_WARN1,_T("[WM] There are still %d msgs on the queue, will quit the thread anyway"), msgQInfo.dwCurrentMessages);
        }
    }

    return;
}

enum Node_Pos_Enum
{
    Node_Pos_Pre =0,
    Node_Pos_At,
    Node_Pos_Post
};

static BOOL GetNode(WQMTstCaseNode* list, int index, Node_Pos_Enum pos,WQMTstCaseNode** out)
{
    if(!list)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] GetNode: invalid input param"));
        return FALSE;
    }

    WQMTstCaseNode* tmpNode = NULL;
   
    while(index)
    {
        tmpNode = list;

        if(!list)
        {
            LibPrint("WM",PT_FAIL,_T("[WM] GetNode: this list ran out before reach the intended index"));
            return FALSE;
        }

        list = list->nextUnit;
        index --;
    }

    if(!list)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] GetNode: this list ran out just before reach the intended index"));
        return FALSE;
    }

    if(pos == Node_Pos_Pre)
    {
        
    }
    else if(pos == Node_Pos_At)
    {
        *out = list;
    }
    else
        *out =  list->nextUnit;
    return TRUE;
}

// Swap two nodes within a linked list
static BOOL SwapNode(WQMTstCaseNode** list, int nodeIndex1, int nodeIndex2)
{
    if(!list)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] SwapNode: invalid input param"));
        return FALSE;
    }

    if(!(*list))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] SwapNode: invalid input list"));
        return FALSE;
    }

    if(nodeIndex1 == nodeIndex2)
    {
        LibPrint("WM",PT_LOG,_T("[WM] SwapNode: same index, no need to swap"));
        return TRUE;
    }


    int firstNodeIndex = nodeIndex1;
    int secNodeIndex = nodeIndex2;

    if(nodeIndex1 > nodeIndex2)
    {
        firstNodeIndex = nodeIndex2;
        secNodeIndex = nodeIndex1;
    }

    // Make sure node1 always points to the first node
    WQMTstCaseNode* node1    = NULL;
    WQMTstCaseNode* node2    = NULL;
    WQMTstCaseNode* nodePre1 = NULL;
    WQMTstCaseNode* nodePre2 = NULL;
     
    if(!GetNode(*list,firstNodeIndex, Node_Pos_At,&node1) ||  !GetNode(*list,secNodeIndex,Node_Pos_At, &node2))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] SwapNode: fail in getting the node for index %d and %d"),firstNodeIndex,secNodeIndex);
        return FALSE;
    }

    if(!GetNode(*list,firstNodeIndex,Node_Pos_Pre, &nodePre1) ||  !GetNode(*list,secNodeIndex,Node_Pos_Pre,&nodePre2))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] SwapNode: fail in getting the pre node for index %d and %d"),firstNodeIndex,secNodeIndex);
        return FALSE;
    }

    WQMTstCaseNode* tmpNode1 = node1->nextUnit;
    WQMTstCaseNode* tmpNode2 = node2->nextUnit;
      
    // Be careful about the case where the two nodes are adjacent to each other
    if(tmpNode1 == node2)
        node2->nextUnit = node1;
    else
        node2->nextUnit = tmpNode1;

    node1->nextUnit = tmpNode2;

    if(nodePre1)
        nodePre1->nextUnit = node2;
    else
    {
        // Means node1 is the header node
        *list = node2;
    }

    //Check for adjacent node
    if(nodePre2 != node1)
    {
        nodePre2->nextUnit = node1;
    }

    return TRUE;
}

WQMTestManager_t::
WQMTestManager_t()
:   m_pCurTestSuite(NULL),
    m_TotalTestSuite(0),
    m_pTestList(NULL),
    m_pCurTestUnit(NULL),
    m_TotalExeUnits(0),
    m_pWorkerThread(NULL),
    m_StartTime(0),
    m_IsRandom(WIFI_METRIC_STR_ISRANDOM,
               WIFI_METRIC_STR_ISRANDOM,
               TEXT("Random Test"),
               false), 
    m_MultiThrd(WIFI_METRIC_MULTI_THRD,
               WIFI_METRIC_MULTI_THRD,
               TEXT("Multithread Test"),
               false), 
    m_BvtTestRun(WIFI_METRIC_STR_BVT,
                WIFI_METRIC_STR_BVT,
                TEXT("BVT Test"),
                false),
    m_PassCriteria(WIFI_METRIC_STR_TEST_PASS,
                   WIFI_METRIC_STR_TEST_PASS,
                   TEXT("Test run Passing Percentage"),
                   WIFI_METRIC_TEST_PASS_CRI,
                   0,
                   100),
    m_FailCriteria(WIFI_METRIC_STR_TEST_FAIL,
                    WIFI_METRIC_STR_TEST_FAIL,
                    TEXT("Test run fail Percentage"),
                    WIFI_METRIC_TEST_FAIL_CRI,
                    0,
                    100),                         
    m_MaxContFailCount(WIFI_METRIC_STR_FAILCOUNT,
                   WIFI_METRIC_STR_FAILCOUNT,
                   TEXT("Maxium Consecutive fails before fail the test run"),
                   10,
                   1,
                   500),
    m_LocalFileName(TEXT("ConfigFileName"),
                    TEXT("tConfigFileName"),
                    TEXT("The name of the configuration file"),
                    WIFI_DEFAULT_CONFIG_FILE),  
    m_DeviceName(TEXT("DeviceName"),
                 TEXT("DeviceName"),
                 TEXT("The name of the device under test"),
                 TEXT("NoNameDevice")),   
    m_ContFailCount(0),
    m_pFunctionTable(NULL),
    m_FunctionTableSize(0),
    m_FunctionTableEnd(0),
    m_NoOfTstSuiteLkUpTables(0)
{
    for(int index = 0; index < MAX_TEST_SUITE_NO; index++)
    {
        m_pTestSuiteList[index] = NULL;
    }
}

WQMTestManager_t::
~WQMTestManager_t()
{
    Cleanup();
}

DWORD
WQMTestManager_t::
ParseCommandLine(int argc, TCHAR *argv[])
{

    DWORD result = NO_ERROR;  
    CmdArgList_t  testArgList;

    result = testArgList.Add(&m_LocalFileName, FALSE);
    if(result != NO_ERROR)
        return result;    


    result = testArgList.ParseCommandLine(argc, argv);
    if(result != NO_ERROR)
        return result;

    // Create hash tables from command lines
    if(!FillCmdLineHashTables(argc,argv))
        result = ERROR_ACCESS_DENIED;
    
    return result;
}


// Print the usage info
void
WQMTestManager_t::
PrintUsage(void)
{

    DWORD result = NO_ERROR;  
    CmdArgList_t  testArgList;

    result = testArgList.Add(&m_LocalFileName, FALSE);
    if(result == NO_ERROR)
        testArgList.PrintUsage(LogAlways);
}

BOOL
WQMTestManager_t::
Init(void)
{      

    DWORD result = NO_ERROR;
    
    if(*m_LocalFileName == _T('\0'))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init failed, no local file to start with"));
        return FALSE;
    }

    if(!m_FileParser.ParseFile((LPTSTR)m_LocalFileName.GetValue(),
        m_TstSetupCmdLineLkUpTable,
        m_TstSuiteCmdLineLkUpTables,
        m_NoOfTstSuiteLkUpTables))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init failed, parse input file failed %s"),m_LocalFileName.GetValue());
        return FALSE;
    }

    m_StartTime = GetTickCount();
    m_TotalExeUnits  = m_FileParser.GetTestUnits(&m_pTestList);
    m_pCurTestUnit   = m_pTestList;

    WQMParamList* setupBlk = m_FileParser.GetSetupUnit();
    LibPrint("WM",PT_LOG,_T("[WM] ===== Setup Config Params: ======"));
    PrintNodeEx(setupBlk);

    CmdArgList_t  testArgList;
    if(testArgList.Add(&m_IsRandom, FALSE) != NO_ERROR)
        return FALSE;
    if(testArgList.Add(&m_MultiThrd, FALSE) != NO_ERROR)
        return FALSE;
    if(testArgList.Add(&m_BvtTestRun, FALSE) != NO_ERROR)
        return FALSE; 
    if(testArgList.Add(&m_PassCriteria, FALSE) != NO_ERROR)
        return FALSE;   
    if(testArgList.Add(&m_FailCriteria, FALSE) != NO_ERROR)
        return FALSE;   
    if(testArgList.Add(&m_MaxContFailCount, FALSE) != NO_ERROR)
        return FALSE; 
    if(testArgList.Add(&m_DeviceName, FALSE) != NO_ERROR)
        return FALSE;    

    if(setupBlk)
    {
        result = ParseConfigurationNode(setupBlk, &testArgList);
        if (NO_ERROR != result)
        {
            LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init failed in parse the headblk"));
            return FALSE;
        }
    }
    else
    {
        LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init couldn't find header block within file %s"),m_LocalFileName.GetValue());
        return FALSE;
    }
        
    if(m_IsRandom)
    {
        // Shuffle around all the test cases
        for(int index = 0; index < m_TotalExeUnits; index ++)
        {
            int tmpIndex1 = rand() % m_TotalExeUnits;
            int tmpIndex2 = rand() % m_TotalExeUnits;
            SwapNode(&m_pTestList, tmpIndex1, tmpIndex2);
        }
    }

    // Get the logging context
    HANDLE logHandle = NetLogIsInitialized();
    //HANDLE logHandle = NULL; 


    if(!logHandle)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init couldn't get current logging context "));
        //return FALSE;
    }
   
    // Now will try to step through the TstSuite blk to load in all the test dlls
    WQMTstGroupNode* tstGroups = m_FileParser.GetTstGroups();
    
    WQMTstSuiteNode* tstSuites = NULL;

    while(tstGroups)
    {
        LibPrint("WM",PT_LOG,_T("[WM] ====== Config Params for test group: %s: ======="),tstGroups->TstGroupName);
        PrintNodeEx(tstGroups->ParamNode);
        tstSuites = tstGroups->TstSuiteList;
        
        while(tstSuites)
        {
            CmdArgList_t   testSuiteArgList;
            
            CmdArgBool_t   tuxWrapper(WIFI_METRIC_STR_TUXWRAP,
                                      WIFI_METRIC_STR_TUXWRAP,
                                      TEXT("Tux Wrapper flag"),
                                      false);

            if(testSuiteArgList.Add(&tuxWrapper, FALSE) != NO_ERROR)
                return FALSE;            
            
    
            CmdArgString_t testSuiteName(WIFI_METRIC_STR_SUITENAME,
                                           WIFI_METRIC_STR_SUITENAME,
                                           TEXT("Test suite name"),
                                           TEXT(""));   

            if(testSuiteArgList.Add(&testSuiteName, FALSE) != NO_ERROR)
                return FALSE;             

            result = ParseConfigurationNode(tstSuites->ParamNode, &testSuiteArgList);
            if (NO_ERROR != result)
            {
                LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init failed in parse the test suite node"));
                return FALSE;
            }            

            LibPrint("WM",PT_LOG,_T("[WM] \t====== Config Params for test Suite: %s: ======="),testSuiteName.GetValue());
            PrintNodeEx(tstSuites->ParamNode, 1);
            
            if(tuxWrapper)
            {
                LibPrint("WM",PT_LOG,_T("[WM] Found legecy tux test, create wrapper for it"));
                m_pTestSuiteList[m_TotalTestSuite] = new WQMTuxTestSuite_t(NULL, logHandle);
            }
            else
            {
                m_pTestSuiteList[m_TotalTestSuite] = InstantiateTstSuiteObj(tstSuites->ParamNode,logHandle);
            }
            
            if(!m_pTestSuiteList[m_TotalTestSuite])
            {
                LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init couldn't Create TstSuiteObject "));
                return FALSE;
            }

            if(!m_pTestSuiteList[m_TotalTestSuite]->Init(tstSuites))
            {
                LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init couldn't initialize TstSuiteObject"));
                return FALSE;
            }

            // Add reference to the test group
            tstSuites->TstSuite = m_pTestSuiteList[m_TotalTestSuite];
            tstSuites = tstSuites->nextUnit;

            m_TotalTestSuite ++;
        }    

        tstGroups = tstGroups->nextGroup;
    }
    
    if(m_MultiThrd)
    {
        m_pWorkerThread = new WQMThread;

        if(!m_pWorkerThread)
        {
             LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init: Couldn't Create Worker thread structure "));
             return FALSE;
        }

        MSGQUEUEOPTIONS msgOption;
        msgOption.dwSize = sizeof(MSGQUEUEOPTIONS);
        msgOption.dwFlags = MSGQUEUE_NOPRECOMMIT | MSGQUEUE_ALLOW_BROKEN;
        msgOption.dwMaxMessages = WIFI_METRIC_MAX_MSGQ_SIZE;
        msgOption.cbMaxMessage   = sizeof(WQMTestCase_t*);


        TCHAR msgName[] = _T("WiFiTstMsgQ");

        // Create read Q
        msgOption.bReadAccess     = TRUE;
        m_pWorkerThread->msgQRHandle = CreateMsgQueue(msgName,&msgOption);

        // Create Write Q
        msgOption.bReadAccess     = FALSE;
        m_pWorkerThread->msgQWHandle = CreateMsgQueue(msgName,&msgOption);

        if(!m_pWorkerThread->msgQRHandle || !m_pWorkerThread->msgQWHandle)
        {
           LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init: Couldn't Create msgQ for thread"));
           return FALSE;
        }

        m_pWorkerThread->bad              = FALSE;
        m_pWorkerThread->interimResult    = TRUE;
        m_pWorkerThread->toExit           = FALSE;

        m_pWorkerThread->threadHandle = CreateThread(NULL,
                                                  0,
                                                  (LPTHREAD_START_ROUTINE)WorkerThreadProc,
                                                  m_pWorkerThread,
                                                  0,
                                                  NULL);

        if(!m_pWorkerThread->threadHandle)
        {
           LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::Init: Failed in creating worker thread"));
           return FALSE;
        }

        m_pWorkerThread = m_pWorkerThread;
    }
    
    return TRUE;
}


// Generate the Tux function tables from all the test cases
DWORD    
WQMTestManager_t::
GetFunctionTable(FUNCTION_TABLE_ENTRY **ppFunctionTable)
{

    DWORD result = NO_ERROR;
    
    if(!Init())
    {
        LibPrint("WM",PT_FAIL,_T("[WM] WQMTestManager_t::GetFunctionTable: Init() failed "));
        return ERROR_INVALID_PARAMETER;
    }


    WQMTstCaseNode* nextTest = GetNextTestUnit();
    WQMParamList* curSuiteNode  = NULL;
    WQMParamList* curGrpNode    = NULL;

    while(nextTest)
    {
        WQMTestCase_t* tmpTestCase = nextTest->TstCaseNode;
        assert(tmpTestCase);
        assert(tmpTestCase->m_pParamList);
            
        WQMParamList* suiteNode  = tmpTestCase->m_pParamList->pParent;
            
        if(suiteNode)
        {
            WQMParamList* grpNode    = suiteNode->pParent;  
            assert(grpNode);
            if(grpNode != curGrpNode)
            {
                // Adding a dummy group node level
                FUNCTION_TABLE_ENTRY* funcTable = MakeFuncTable();
                if(!funcTable)
                    return ERROR_OUTOFMEMORY;
                
                funcTable->lpTestProc = NULL;     
                funcTable->uDepth = 0;
                funcTable->dwUserData = NULL;       
                funcTable->dwUniqueID = 0;

                CmdArgString_t testGrpDesc(WIFI_METRIC_STR_GRPNAME,
                                            WIFI_METRIC_STR_GRPNAME,
                                            TEXT("Test Group name description"),
                                            TEXT(""));    
                    
                result = GetValueFromConfigNode(grpNode, &testGrpDesc);
                if(result != NO_ERROR)
                    return result;

                if(testGrpDesc[0] != _T('\0'))
                {
                    result = CopyDesc(&(funcTable->lpDescription),testGrpDesc.GetValue());
                    if(result != NO_ERROR)
                        return result;
                }                

                m_FunctionTableEnd ++;
                curGrpNode = grpNode;

            }

            if(suiteNode != curSuiteNode)
            {
                // Adding a dummy test suite node level
                FUNCTION_TABLE_ENTRY* funcTable = MakeFuncTable();
                if(!funcTable)
                    return ERROR_OUTOFMEMORY;
                
                funcTable->lpTestProc = NULL;     
                funcTable->uDepth = 1;
                funcTable->dwUserData = NULL;       
                funcTable->dwUniqueID = 0;

                CmdArgString_t testSuiteDesc(WIFI_METRIC_STR_SUITENAME,
                                            WIFI_METRIC_STR_SUITENAME,
                                            TEXT("Test Suite name description"),
                                            TEXT(""));    
                    
                result = GetValueFromConfigNode(suiteNode, &testSuiteDesc);
                if(result != NO_ERROR)
                    return result;

                result = CopyDesc(&(funcTable->lpDescription),testSuiteDesc.GetValue());
                if(result != NO_ERROR)
                    return result;                 

                m_FunctionTableEnd ++;
                curSuiteNode = suiteNode;

            }     
        }       
        
        DWORD  tuxId = 0;
        if(tmpTestCase->GetTuxTestID(&tuxId, m_BvtTestRun.GetValue()) != ERROR_CALL_NOT_IMPLEMENTED)
        {
            FUNCTION_TABLE_ENTRY* funcTable = MakeFuncTable();
            if(!funcTable)
                return ERROR_OUTOFMEMORY;
            
            funcTable->lpTestProc = TestProc;     
            funcTable->uDepth = 2;
            funcTable->dwUserData = (DWORD)tmpTestCase;       
            funcTable->dwUniqueID = tuxId;
            
            result = tmpTestCase->GetTuxDesc(&(funcTable->lpDescription));
            if(result != NO_ERROR)
                break;                
            
            for(int index = 0; index < m_TotalTestSuite; index ++)
            {
                if(m_pTestSuiteList[index]->AddFunctionTableEntry(funcTable) == NO_ERROR) 
                    break;
            }
            
            m_FunctionTableEnd ++;
        }

        nextTest = GetNextTestUnit();
    }
    
    // Double check on this last test func
    FUNCTION_TABLE_ENTRY* funcTable = MakeFuncTable();
    funcTable->uDepth = 0;
    funcTable->dwUserData = 0;
    funcTable->dwUniqueID = 0;
    funcTable->lpDescription = NULL;
    funcTable->lpTestProc = NULL;

    if(result != NO_ERROR)
        free(m_pFunctionTable);
    else   
        *ppFunctionTable = (FUNCTION_TABLE_ENTRY*)m_pFunctionTable;    

    return result;
}

int
WQMTestManager_t::
RunTest(FUNCTION_TABLE_ENTRY *pFTE) // test to be executed
{
    // If meet m_MaxContFailCount consecutive failure
    // Abort the test
    if(m_ContFailCount >= m_MaxContFailCount)
        return TPR_ABORT;
    
    assert(NULL != pFTE);

    WQMTestCase_t* testUnit = (WQMTestCase_t*)pFTE->dwUserData;

    if(testUnit->IsParallel())
    {
        // Add this test case to the worker thread
        if(m_pWorkerThread)
        {
            if(!WriteMsgQueue(m_pWorkerThread->msgQWHandle,
                              testUnit,
                              sizeof(testUnit),
                              INFINITE,
                              0))
            {
               LibPrint("WM",PT_FAIL,_T("[WM] WriteMsgQueue write to msg Q failed "));
               return TPR_FAIL;
            }
            else
            {
                // Will always return pass to tux for paralle case
                return TPR_PASS;
            }
        }
    }

    WQMTestSuiteBase_t* tmpSuite = testUnit->m_pTstSuite;

    if(m_pCurTestSuite)
    {
        if(tmpSuite)
        {
            if(tmpSuite != m_pCurTestSuite)
            {
                // Means entering another suite
                // Needs to cleanup the current test suite
                m_pCurTestSuite->Cleanup();
                m_pCurTestSuite = tmpSuite;
            }
        }
    }
    else
        m_pCurTestSuite = tmpSuite;
        
    int retval = testUnit->Execute();

    if(retval == TPR_FAIL)
        m_ContFailCount ++;
    else
        m_ContFailCount = 0;  // Reset count

    return retval;
}

 
WQMTstCaseNode*   
WQMTestManager_t::
GetNextTestUnit()
{
    WQMTstCaseNode* tmpNode = m_pCurTestUnit;
    if(m_pCurTestUnit)
    {
        m_pCurTestUnit = m_pCurTestUnit->nextUnit;
    }

    return tmpNode;
}

void    
WQMTestManager_t::
ReportTestResult(void)
{

    BOOL result = TRUE;

    // Closeup the log file first

    WQMTestSuite_Info* testSuiteInfo = NULL;
    int totalTestCase = 0;
    int totalPass = 0;
    int totalFail = 0;
    int totalSkip = 0;
    Status::Code storeResult;
    XmlElement_t* tempEle = NULL;
    
    for(int index = 0; index <  m_TotalTestSuite; index ++)
    {
        if(m_pTestSuiteList[index])
        {
            testSuiteInfo = m_pTestSuiteList[index]->GetTestSuiteInfo();
            m_pTestSuiteList[index]->ReportTestResult();
            
            totalTestCase   += testSuiteInfo->TotalTestCase;
            totalPass       += testSuiteInfo->TotalTestPassed;
            totalFail       += testSuiteInfo->TotalTestFailed;
            totalSkip       += testSuiteInfo->TotalTestSkiped;
        }
    }

    LibPrint("WM",PT_VERBOSE,_T("[WM]    Total Test case Executed: %d"),totalTestCase);
    LibPrint("WM",PT_VERBOSE,_T("[WM]    Total Test case Passed: %d"),totalPass);
    LibPrint("WM",PT_VERBOSE,_T("[WM]    Total Test case Failed: %d"),totalFail);
    LibPrint("WM",PT_VERBOSE,_T("[WM]    Total Test case Skiped: %d"),totalSkip);
 
    LibPrint("WM",PT_VERBOSE,_T("[WM] ==== Done Reporting Overall Test Results ===="));    

    // Start test result report
    XmlElement_t* rootElement = new XmlElement_t(WQM_FILE_ROOT_TAG);
    if(!rootElement)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Can't create root element"));
        return;
    }
    
    SYSTEMTIME sysTime;
    TCHAR tmpStr[128];
    HRESULT hr;
    double overAllTestPerc = 0.0;
    double weightSum = 0.0; 
    WQMTstGroupNode* testGroups = NULL;


    XmlMetaElement_t* pMeta = new XmlMetaElement_t;
    if (!pMeta)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Can't allocate XmlMetaElement"));
        delete rootElement;
        return;
    }

    pMeta->SetAttribute(L"version",   L"1.0");
    pMeta->SetAttribute(L"standalone",L"yes");

    // Time to save the doc
    XmlDocument_t   xmlDoc(rootElement,pMeta);
    
    CmdArgString_t reportPath(_T("RootDirName"),
                               _T("RootDirName"),
                               TEXT("Test result path"),
                               TEXT("\\Windows"));    
    
    if(!rootElement)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't create root element for test result reporting"));
        goto Cleanup;
    }    

    DWORD majorVer, minorVer, oSBldNo, romBldNo;
    _tcscpy_s(tmpStr, _countof(tmpStr), _T("0:0:0:0"));
    
    if(GetBuildInfo(&majorVer, &minorVer, &oSBldNo, &romBldNo))
        _stprintf_s(tmpStr, _countof(tmpStr), _T("%d:%d:%d:%d"), majorVer,minorVer,oSBldNo,romBldNo);

    hr = WiFUtils::AddXmlStringElement(rootElement,
                                     _T("Build_Info"),
                                     tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Test run type element"));
        goto Cleanup;
    }    
    

    _tcscpy_s(tmpStr, _countof(tmpStr), _T("Full_TestRun"));
    if(m_BvtTestRun)
        _tcscpy_s(tmpStr, _countof(tmpStr), _T("BVT_TestRun"));

    hr = WiFUtils::AddXmlStringElement(rootElement,
                                     _T("TestRun_Type"),
                                     tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Test run type element"));
        goto Cleanup;
    }
    
    
    GetSystemTime(&sysTime);
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d-%d-%d"), sysTime.wMonth,sysTime.wDay,sysTime.wYear);

    hr = WiFUtils::AddXmlStringElement(rootElement,
                                     _T("Date"),
                                     tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Date element"));
        goto Cleanup;
    }

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d:%d:%d"), sysTime.wHour,sysTime.wMinute,sysTime.wSecond);
    hr = WiFUtils::AddXmlStringElement(rootElement,
                                      _T("Time"),
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Time element"));
        goto Cleanup;
    }   

    hr = WiFUtils::AddXmlStringElement(rootElement,
                                      _T("Device"),
                                      m_DeviceName);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Device element"));
        goto Cleanup;
    }    

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), m_PassCriteria.GetValue());
    hr = WiFUtils::AddXmlStringElement(rootElement,
                                      WIFI_METRIC_STR_TEST_PASS,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add PassCriteria element for TestRun"));
        goto Cleanup;
    }    

    // Try to remember position of this element
    XmlElement_t* markEle = NULL;
    
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), m_FailCriteria.GetValue());
    hr = WiFUtils::AddXmlStringElement(rootElement,
                                      WIFI_METRIC_STR_TEST_FAIL,
                                      tmpStr,
                                      &markEle);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add FailCriteria element for TestRun"));
        goto Cleanup;
    } 

    testGroups = m_FileParser.GetTstGroups();

    while(testGroups)
    {
        int groupPerc = 0;
        XmlElement_t* tmpElement = ParseTestGroupResult(testGroups, &groupPerc);
        
        if(!tmpElement)
        {   
            LibPrint("WM",PT_WARN1,_T("[WM] Failed in generate result report for test group: %s"),testGroups->TstGroupName);
            testGroups = testGroups->nextGroup;
            continue;
        }

        CmdArgDouble_t groupWeight(WIFI_METRIC_STR_TEST_WEIGHT,
                                    WIFI_METRIC_STR_TEST_WEIGHT,
                                    TEXT("Test Group Weight"),
                                    1.0,
                                    0.5,
                                    WIFI_METRIC_TEST_WEIGHT_MAX);

        GetValueFromConfigNode(testGroups->ParamNode, &groupWeight);
    
        overAllTestPerc += groupPerc * groupWeight;
        weightSum       += groupWeight;
        
        rootElement->AppendChild(tmpElement);

        testGroups = testGroups->nextGroup;
    }
   
    int totalPerc = 0;
    if(weightSum > 0.0)
        totalPerc = (int)(overAllTestPerc / weightSum);
 
    TCHAR* tResult = s_Pass;

    if(m_FailCriteria >= totalPerc)
        tResult = s_Fail;
    else if(m_PassCriteria > totalPerc)
        tResult = s_Mix;

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), totalPerc);
    tempEle = new XmlElement_t(s_PassPerc);
    if(!tempEle)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed to create new element for PassPercent for TestRun"));
        goto Cleanup;
    }
    tempEle->SetText(tmpStr);
    result = rootElement->InsertAfter(markEle,tempEle);
    if(!result)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed in inserting overall pass percentage tag"));
        goto Cleanup;
    }

    tempEle = new XmlElement_t(s_TestResult);
    if(!tempEle)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed to create new element for TestResult for TestRun"));
        goto Cleanup;
    }
    tempEle->SetText(tResult);
    result = rootElement->InsertAfter(markEle,tempEle);
    if(!result)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed in inserting overall test result tag"));
        goto Cleanup;
    }    

    tempEle = NULL;

    GetValueFromConfigNode(m_FileParser.GetSetupUnit(), &reportPath);

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%s\\TempResult.xml"),reportPath.GetValue());
    DeleteFile(tmpStr);
    storeResult = xmlDoc.Save(tmpStr);

    if(storeResult != Status::Success)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed to save final result to file: %s"), tmpStr);
        goto Cleanup;
    }    
    
    //Create a new log file, without delete any old log files
    int fileExt = 0;
    while(TRUE)
    {  
        _stprintf_s(tmpStr, _countof(tmpStr), _T("%s\\WiFiMetric_Result%d.xml"),reportPath.GetValue(),fileExt);
        HANDLE hFile = CreateFile(tmpStr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if(INVALID_HANDLE_VALUE != hFile)
        {
            // Close the file and increment the file counter
            CloseHandle(hFile);
            fileExt ++;
        }
        else
            break;
    }
 
    if(WriteResultXmlFile(xmlDoc,tmpStr) != NO_ERROR)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed to format end result file into better shape"));
    }

Cleanup:

    if(tempEle)
        delete tempEle;
        
    return;
    
}

void
WQMTestManager_t::Cleanup()
{

    // First destroy all the test suites
    for(int index = 0; index <  m_TotalTestSuite; index ++)
    {
        if(m_pTestSuiteList[index] )
        {
            DestroyTstSuite(m_pTestSuiteList[index]);
            m_pTestSuiteList[index]  = NULL;
        }
    }

    if(m_pWorkerThread)
    {
        if(m_pWorkerThread->threadHandle)
        {
            m_pWorkerThread->toExit = TRUE;
            WaitForSingleObject(m_pWorkerThread->threadHandle, 5000);
            CloseHandle(m_pWorkerThread->threadHandle);
        }


        if(m_pWorkerThread->msgQRHandle)
            CloseMsgQueue(m_pWorkerThread->msgQRHandle);
        if(m_pWorkerThread->msgQWHandle)
            CloseMsgQueue(m_pWorkerThread->msgQWHandle);

        delete m_pWorkerThread;
    }

    if (m_pFunctionTable)
    {
        for (int fpx = m_FunctionTableEnd ; --fpx >= 0 ;)
        {
            FUNCTION_TABLE_ENTRY *pFPE = &m_pFunctionTable[fpx];
            if (pFPE->lpDescription)
            {
                free((void *)(pFPE->lpDescription));
                pFPE->lpDescription = NULL;
            }
        }
        free(m_pFunctionTable);
        m_pFunctionTable = NULL;
    }

    return;
}

// Parse the -v command line args and create hash tables for future use
BOOL
WQMTestManager_t::
FillCmdLineHashTables(
    int argc, 
    TCHAR* argv[])
{

    TCHAR  *ptszStart, *ptszSuitePtr, *ptszBuff;
    for(int index = 0; index < argc; index ++)
    {
        if((*argv[index] == _T('-')) &&
           (*(argv[index]+1) == _T('v') ||
            *(argv[index]+1) == _T('V')))
        {
            // Find -v flag
            // Assume name=value pair in one word
            if(index == argc - 1)
            {
                LibPrint("WM",PT_FAIL,_T("[WM] CmdLine has in-complete -v cmd line arg"));
                return FALSE;
            }

            ptszBuff =  argv[index + 1];
            TCHAR*  tmpPtr = ptszBuff;
            WQMNameHash*  tmpHash = NULL;
            
            // Trying to find a test suite delimit first           
            ptszSuitePtr = _tcschr( ptszBuff, _T(':'));
            if(ptszSuitePtr)
            {
                // Found a test suite cmd arg
                *ptszSuitePtr++ = _T('\0');
                // Loop through existing tstsuite to see a hashtable has
                // already been booked for this suite
                int eye = 0;
                for(; eye < m_NoOfTstSuiteLkUpTables; eye ++)
                {
                    if(_tcsicmp(m_TstSuiteCmdLineLkUpTables[eye].TstSuiteName, ptszBuff) == 0)
                    {
                        // Found the suite;
                        break;
                    }
                }

                if(eye == m_NoOfTstSuiteLkUpTables)
                {
                    // Book one more suites
                    m_NoOfTstSuiteLkUpTables ++;
                    if(m_NoOfTstSuiteLkUpTables > MAX_TEST_SUITE_NO)
                    {
                        LibPrint("WM",PT_FAIL,_T("[WM] Too many cmd arg hash table for test suites"));
                        return FALSE;
                    }

                    _tcsncpy_s(m_TstSuiteCmdLineLkUpTables[eye].TstSuiteName, _countof(m_TstSuiteCmdLineLkUpTables[eye].TstSuiteName),ptszBuff,MAX_TSTSUITENAME_LEN);
                    m_TstSuiteCmdLineLkUpTables[eye].TstSuiteName[MAX_TSTSUITENAME_LEN -1] = _T('\0');
                }

                tmpHash = &(m_TstSuiteCmdLineLkUpTables[eye].TstHashTable);
                tmpPtr  = ptszSuitePtr;
                
            }

            // Now will try to seperate the name value pair, devided by '='
            ptszStart = _tcschr(tmpPtr, _T('='));
            if(!ptszStart)
            {
                LibPrint("WM",PT_FAIL,_T("[WM] Incomplete name value pair found in commad line: %s"),tmpPtr);
                return FALSE;
            }

            *ptszStart++ = _T('\0');

            if(!tmpHash)
            {
                // Top level, Setup hashtable
                tmpHash = &m_TstSetupCmdLineLkUpTable;
            }

            ce::wstring key = tmpPtr;
            ce::wstring value = ptszStart;
            
            tmpHash->insert(key,value);
        }
    }
    
    return TRUE;
}

TESTPROCAPI
WQMTestManager_t::
TestProc(
    UINT uMsg,                  // TPM_QUERY_THREAD_COUNT or TPM_EXECUTE
    TPPARAM tpParam,            // TPS_QUERY_THREAD_COUNT or not used
    FUNCTION_TABLE_ENTRY *pFTE) // test to be executed
{
    if (uMsg == TPM_QUERY_THREAD_COUNT)
    {
        assert(NULL != (TPS_QUERY_THREAD_COUNT *)tpParam);
        ((TPS_QUERY_THREAD_COUNT *)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    assert(NULL != pFTE);

    WQMTestManager_t* tstManager = GetTestManager();

    return tstManager->RunTest(pFTE);
}

// Generate the Tux function tables from all the test cases
FUNCTION_TABLE_ENTRY*    
WQMTestManager_t::
MakeFuncTable(void)
{
    if (m_FunctionTableSize <= m_FunctionTableEnd)
    {
        int newAlloc = m_FunctionTableSize * 2;
        if (newAlloc == 0)
            newAlloc = 64;

        size_t allocSize = newAlloc * sizeof(FUNCTION_TABLE_ENTRY);
        void *newTable = malloc(allocSize);

        if (newTable == NULL)
        {
            LibPrint("WM",PT_FAIL,_T("[WM] Can't allocate %u bytes for function table"),
                     allocSize);
            return NULL;
        }

        if (m_pFunctionTable)
        {
            allocSize = m_FunctionTableSize * sizeof(FUNCTION_TABLE_ENTRY);
            memcpy(newTable, m_pFunctionTable, allocSize);
            free(m_pFunctionTable);
        }

        m_pFunctionTable = (FUNCTION_TABLE_ENTRY *)newTable;
        m_FunctionTableSize = newAlloc;
    }    

    return &(m_pFunctionTable[m_FunctionTableEnd]);
    
}

XmlElement_t*
WQMTestManager_t::
ParseTestGroupResult(WQMTstGroupNode* testGroup, int* groupPerc)
{
    XmlElement_t* grpElement = new XmlElement_t(_X("TestGroup"));    
    if(!grpElement)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't create group element for test result reporting"));
        return NULL;
    }     

    // Setup the name attr of the group
    grpElement->SetAttribute(_X("name"), testGroup->TstGroupName);

    TCHAR tmpStr[128];

    WQMParamList* grpNode = testGroup->ParamNode;

    CmdArgLong_t grpPassCri(WIFI_METRIC_STR_TEST_PASS,
                           WIFI_METRIC_STR_TEST_PASS,
                           TEXT("Test group Passing Percentage"),
                           WIFI_METRIC_TEST_PASS_CRI,
                           0,
                           100);
    CmdArgLong_t grpFailCri(WIFI_METRIC_STR_TEST_FAIL,
                            WIFI_METRIC_STR_TEST_FAIL,
                            TEXT("Test group fail Percentage"),
                            WIFI_METRIC_TEST_FAIL_CRI,
                            0,
                            100);

    CmdArgDouble_t grpWeight(WIFI_METRIC_STR_TEST_WEIGHT,
                            WIFI_METRIC_STR_TEST_WEIGHT,
                            TEXT("Test Group Weight"),
                            1.0,
                            0.5,
                            WIFI_METRIC_TEST_WEIGHT_MAX);
    

    CmdArgList_t  testArgList;
    if(testArgList.Add(&grpPassCri, FALSE) != NO_ERROR)
        return FALSE;
    if(testArgList.Add(&grpFailCri, FALSE) != NO_ERROR)
        return FALSE;
    if(testArgList.Add(&grpWeight, FALSE) != NO_ERROR)
        return FALSE; 

    DWORD result= ParseConfigurationNode(grpNode, &testArgList);
    if (NO_ERROR != result)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Failed for parsing a test group node"));
        delete grpElement;
        return NULL;
    }
    
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), grpPassCri.GetValue());
    HRESULT hr = WiFUtils::AddXmlStringElement(grpElement,
                                              WIFI_METRIC_STR_TEST_PASS,
                                              tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add PassCriteria element for TestGroup"));
        delete grpElement;
        return NULL;
    } 

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), grpFailCri.GetValue());
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      WIFI_METRIC_STR_TEST_FAIL,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add FailCriteria element for TestGroup"));
        delete grpElement;
        return NULL;
    } 

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%f"), grpWeight.GetValue());
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      WIFI_METRIC_STR_TEST_WEIGHT,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add TestWeight element for TestGroup"));
        delete grpElement;
        return NULL;
    }     

    int totalPass = 0;
    int totalFail = 0;
    int totalSkip = 0;

    int testPerc = 0;
    double overAllTestPerc = 0.0;
    double weightSum = 0.0;

    WQMTstSuiteNode* testSuites = testGroup->TstSuiteList;
    WQMTstSuiteNode* tmpTestSuite = testSuites;

    while(tmpTestSuite)
    {
        if(tmpTestSuite->TstSuite)
        {
            WQMTestSuite_Info* suiteInfo = tmpTestSuite->TstSuite->GetTestSuiteInfo();
            totalPass += suiteInfo->TotalTestPassed;
            totalFail += suiteInfo->TotalTestFailed;
            totalSkip += suiteInfo->TotalTestSkiped;
            if(suiteInfo->TotalTestPassed || suiteInfo->TotalTestFailed)
            {
                weightSum += suiteInfo->TestWeight;
                overAllTestPerc += suiteInfo->PassPercentage * suiteInfo->TestWeight;
            }
        }

        tmpTestSuite = tmpTestSuite->nextUnit;
    }

    if((totalPass + totalFail) == 0)
    {
        // There is no test case been run, skip this group
        LibPrint("WM",PT_VERBOSE,_T("[WM] Test Group: %s has 0 test case run"),testGroup->TstGroupName);
        delete grpElement;
        return NULL;
    }

    if(weightSum > 0.0)
        testPerc = (int)(overAllTestPerc / weightSum);
        
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), testPerc);
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      s_PassPerc,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Pass percentage element for TestGroup"));
        delete grpElement;
        return NULL;
    }    

    TCHAR* testResult = s_Pass;
    if(grpFailCri >= testPerc)
        testResult = s_Fail;
    else if(grpPassCri > testPerc)
        testResult = s_Mix;   

    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      s_TestResult,
                                      testResult);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Test Result element for TestGroup"));
        delete grpElement;
        return NULL;
    }    

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), totalPass);
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      s_TotalPass,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add total pass element for TestGroup"));
        delete grpElement;
        return NULL;
    } 

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), totalSkip);
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      s_TotalSkip,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add total skip element for TestGroup"));
        delete grpElement;
        return NULL;
    }

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), totalFail);
    hr = WiFUtils::AddXmlStringElement(grpElement,
                                      s_TotalFail,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add total fail element for TestGroup"));
        delete grpElement;
        return NULL;
    }    

    // Second loop, figuring out test suites
    tmpTestSuite = testSuites;
    while(tmpTestSuite)
    {
        if(tmpTestSuite->TstSuite)
        {
            XmlElement_t* suiteEle = ParseTestSuiteResult(tmpTestSuite->TstSuite);

            if(suiteEle)
            {
                grpElement->AppendChild(suiteEle);
            }
        }

        tmpTestSuite = tmpTestSuite->nextUnit;
    }    
    
    *groupPerc = testPerc;
    return grpElement;
}

XmlElement_t*
WQMTestManager_t::
ParseTestSuiteResult(WQMTestSuiteBase_t* testSuite)
{
    XmlElement_t* suiteElement = new XmlElement_t(_X("TestSuite"));   
    if(!suiteElement)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't create suite element for test result reporting"));
        return NULL;
    }
    
    WQMTestSuite_Info*  suiteInfo = testSuite->GetTestSuiteInfo();

    if(suiteInfo->TotalTestFailed + suiteInfo->TotalTestPassed == 0)
    {
        LibPrint("WM",PT_VERBOSE,_T("[WM] test suite: %s has 0 test case run, skip"),suiteInfo->TestSuiteName);
        delete suiteElement;
        return NULL;
    }
    
    // Setup the name attr of the suite
    suiteElement->SetAttribute(_X("name"), suiteInfo->TestSuiteName);

    TCHAR tmpStr[128];
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->PassCri);
    HRESULT hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      WIFI_METRIC_STR_TEST_PASS,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add PassCriteria element for TestSuite"));
        delete suiteElement;
        return NULL;
    }  

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->FailCri);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      WIFI_METRIC_STR_TEST_FAIL,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add FailCriteria element for TestSuite"));
        delete suiteElement;
        return NULL;
    }      

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%f"), suiteInfo->TestWeight);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      WIFI_METRIC_STR_TEST_WEIGHT,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add TestWeight element for TestSuite"));
        delete suiteElement;
        return NULL;
    }  

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->PassPercentage);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      s_PassPerc,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Pass Percentage element for TestSuite"));
        delete suiteElement;
        return NULL;
    }      

    TCHAR* testResult = s_Pass;
    if(suiteInfo->FailCri >= suiteInfo->PassPercentage)
        testResult = s_Fail;
    else if(suiteInfo->PassCri > suiteInfo->PassPercentage)
        testResult = s_Mix;

    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      s_TestResult,
                                      testResult);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Test Result element for TestSuite"));
        delete suiteElement;
        return NULL;
    }         

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->TotalTestPassed);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      s_TotalPass,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Total test pass element for TestSuite"));
        delete suiteElement;
        return NULL;
    }   

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->TotalTestSkiped);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      s_TotalSkip,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Total test skip element for TestSuite"));
        delete suiteElement;
        return NULL;
    }

    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), suiteInfo->TotalTestFailed);
    hr = WiFUtils::AddXmlStringElement(suiteElement,
                                      s_TotalFail,
                                      tmpStr);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Total test fail element for TestSuite"));
        delete suiteElement;
        return NULL;
    }    

    WQMTestCase_Info*  testCaseList = testSuite->GetTestCasesInfo();

    while(testCaseList)
    {
        XmlElement_t* testCaseEle = ParseTestCaseResult(testCaseList);

        if(!testCaseEle)
        {
            LibPrint("WM",PT_FAIL,_T("[WM] ParseTestCaseResult Failed"));
            delete suiteElement;
            return NULL;
        }

        suiteElement->AppendChild(testCaseEle);
        testCaseList = testCaseList->NextTestCaseInfo;
    }
        
    return suiteElement;
}

XmlElement_t*
WQMTestManager_t::
ParseTestCaseResult(WQMTestCase_Info* testCase)
{
    XmlElement_t* caseElement = new XmlElement_t(_X("TestCase"));   
    if(!caseElement)
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't create test case element for test result reporting"));
        return NULL;
    }

    TCHAR tmpStr[128];
    _stprintf_s(tmpStr, _countof(tmpStr), _T("%d"), testCase->TestId);
    caseElement->SetAttribute(_X("ID"),tmpStr);
    caseElement->SetAttribute(_X("name"),testCase->TestCaseDesc);

    TCHAR* testResult = s_Pass;
    
    if(testCase->FailedTestRun)
        testResult = s_Fail;
    else if(testCase->SkipTestRun)
        testResult = s_Skip;

    HRESULT hr = WiFUtils::AddXmlStringElement(caseElement,
                                              s_TestResult,
                                              testResult);

    if (FAILED(hr))
    {
        LibPrint("WM",PT_FAIL,_T("[WM] Couldn't add Test Result element for Test case"));
        delete caseElement;
        return NULL;
    }    

    return caseElement;
}



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
// This is the APControl tool.
//
// This program provides command-line configuration of Access Points and
// RF Attenuators by interacting with the APControl server.
//
// Usage:
//      APCTool [-v] [-z] [-?] [others-commands]
//      options:
//         -mt     Use multiple logging files, one for each thread the test forks off.
//         -z      Output test messages to the console.
//         -fl     Output test messages to a file on the CE device.
//         -fr     Output test messages to a PPSH file on the NT device.
//         -v      Specifies the system to log with full verbosity.
//         -v[int] Specifies the system log with the given verbosity.
//
//         APControl options:
//           -lHost      LAN server name/address (default "localhost")
//           -lPort      LAN server port (default "33331")
//
//         APCTool commands (default -q):
//           -q          Query current AP configs (default false)
//
// ----------------------------------------------------------------------------

#include <APService_t.hpp>
#include <NetMain_t.hpp>
#include <WiFUtils.hpp>

#include <litexml.h>
#include <vector.hxx>

using namespace ce::qa;
using namespace litexml;

// ----------------------------------------------------------------------------
//
// External variables for NetMain:
//
WCHAR *gszMainProgName = L"APCTool";
DWORD  gdwMainInitFlags = 0;

// -----------------------------------------------------------------------------
//
// XML element and attribute names:
//
static const WCHAR s_RootTagName[] = L"APPool";

// -----------------------------------------------------------------------------
//
// Extends NetMain to add our customized run methods.
//
class MyNetMain_t : public NetMain_t
{
private:

    // APService singleton:
    APService_t *m_pAPs;

    // Run-time configuration objects:
    CmdArgList_t *m_pCmdArgList;

  // Commands:

    // Display the current AP configurations:
    CmdArgFlag_t *m_pQueryAPConfigs;

    // Read configurations from specified file:
    CmdArgString_t *m_pReadAPConfigs;

    // Write configurations to the specified file:
    CmdArgString_t *m_pWriteAPConfigs;

public:

    // Constructor/destructor:
    MyNetMain_t(void) :
        m_pAPs(NULL),
        m_pCmdArgList(NULL),
        m_pQueryAPConfigs(NULL),
        m_pReadAPConfigs (NULL),
        m_pWriteAPConfigs(NULL)
    {
    }
  __override virtual
   ~MyNetMain_t(void)
    {
        m_pAPs            = NULL;
        m_pCmdArgList     = NULL;
        m_pQueryAPConfigs = NULL;
        m_pReadAPConfigs  = NULL;
        m_pWriteAPConfigs = NULL;
    }

    // The actualt program:
  __override virtual DWORD
    Run(void);

    // Startup initialization and shutdown cleanup:
    DWORD
    StartupInitialize(void);
    DWORD
    ShutdownCleanup(void);

protected:

    // Displays the program's command-line arguments:
  __override virtual void
    DoPrintUsage(void)
    {
        LogAlways(TEXT("\n"));
        NetMain_t::DoPrintUsage();
        CmdArgList_t *pArgList = GetCmdArgList();
        if (pArgList)
            pArgList->PrintUsage(LogAlways);
    }

    // Parses the program's command-line arguments:
 __override virtual DWORD
    DoParseCommandLine(
        int    argc,
        TCHAR *argv[])
    {
        CmdArgList_t *pArgList = GetCmdArgList();
        return (NULL == pArgList)? ERROR_OUTOFMEMORY
                                 : pArgList->ParseCommandLine(argc, argv);
    }

private:

    // Generates or retrieves the command-arg list:
    CmdArgList_t *
    GetCmdArgList(void)
    {
        if (NULL == m_pCmdArgList)
        {
            // Get the APService command-args.
            assert (NULL != m_pAPs);
            m_pCmdArgList = m_pAPs->GetCmdArgList();
            if (NULL == m_pCmdArgList)
                return NULL;

            // Add the local configuration variables.
            m_pCmdArgList->SetSubListName(TEXT("APCTool commands (default -q)"));

            m_pQueryAPConfigs
                = new CmdArgFlag_t(TEXT("QueryAPConfigs"),
                                   TEXT("q"),
                                   TEXT("Queries current AP configs"));
            if (NO_ERROR != m_pCmdArgList->Add(m_pQueryAPConfigs))
                return NULL;

            m_pReadAPConfigs
                = new CmdArgString_t(TEXT("ReadAPConfigs"),
                                     TEXT("i"),
                                     TEXT("Reads and sets AP config(s) from file"));
            if (NO_ERROR != m_pCmdArgList->Add(m_pReadAPConfigs))
                return NULL;

            m_pWriteAPConfigs
                = new CmdArgString_t(TEXT("WriteAPConfigs"),
                                     TEXT("o"),
                                     TEXT("Writes current AP configs to file"));
            if (NO_ERROR != m_pCmdArgList->Add(m_pWriteAPConfigs))
                return NULL;

        }
        return m_pCmdArgList;
    }
};

// -----------------------------------------------------------------------------
//
// Waits for one of the specified threads to finish.
// Displays an error if there's a problem and the op-name is supplied.
// Returns the thread's result and inserts it's offset in "pFinished".
// If the wait fails, sets "pFinished" to -1.
//
static DWORD
WaitForThread(
    HANDLE *threads,
    int  numThreads,
    int         *pFinished,
    const TCHAR *pOpName)
{
    DWORD result = NO_ERROR;
    
    DWORD waitId = WaitForMultipleObjects(numThreads, threads, FALSE, INFINITE);
    int finished = waitId - WAIT_OBJECT_0;
    
    if ((WAIT_FAILED == waitId)
     || (0 > finished || finished >= numThreads))
    {
        result = GetLastError();
        if (NO_ERROR == result)
            result = waitId;
        if (pOpName)
        {
            LogError(TEXT("Error waiting for %s: %s"),
                     pOpName, Win32ErrorText(result));
        }
       *pFinished = -1;
        return result;
    }

   *pFinished = finished;

    if (!GetExitCodeThread(threads[finished], &result))
    {
        result = GetLastError();
        if (pOpName)
        {
            LogError(TEXT("Error getting %s results: %s"),
                     pOpName, Win32ErrorText(result));
        }
    }

    CloseHandle(threads[finished]);
    threads[finished] = NULL;

    return result;
}

// -----------------------------------------------------------------------------
//
// Thread proc to generate a XML blob from an APs configuration state.
//
struct APGetThreadData_t
{
    APController_t *pAP;
    XmlElement_t  **ppElem;
};

static DWORD WINAPI
APGetThreadProc(
    void* pvParam)
{
    assert(NULL != pvParam);
    APGetThreadData_t *pData = reinterpret_cast<APGetThreadData_t*>(pvParam);

    HRESULT hr;
    hr = pData->pAP->EncodeToXml(pData->ppElem);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    LogDebug(TEXT("Retrieved information for AP \"%s\""),
             pData->pAP->GetAPName()); 
    return NO_ERROR;
}

// -----------------------------------------------------------------------------
//
// Generates an XML document containing the current AP configurations.
//
static DWORD
GenerateAPConfigsDoc(
    APService_t    *pAPs,
    XmlDocument_t **ppDoc)
{
    HRESULT hr;
    DWORD result;
    DWORD startTime = GetTickCount();
    
    // Connect to the AP-Control server.
    result = pAPs->Connect();
    if (NO_ERROR != result)
        return result;

    // Generate the root element.
    ce::auto_ptr<XmlElement_t> pRoot;
    hr = WiFUtils::CreateXmlElement(s_RootTagName, &pRoot);
    if (FAILED(hr))
        return HRESULT_CODE(hr);
    
    // Keep track of the sub-thread handles.
    APGetThreadData_t threadData[MAXIMUM_WAIT_OBJECTS];
    XmlElement_t     *threadElem[MAXIMUM_WAIT_OBJECTS];
    HANDLE threads[MAXIMUM_WAIT_OBJECTS];
    int numThreads = 0;

    // Get the current AP configurations.
    for (int apx = 0 ; apx < pAPs->size() ; ++apx)
    {
        // Create a separate thread to retrieve each AP.
        APGetThreadData_t *pData = &threadData[apx];
        pData->pAP = pAPs->GetAP(apx);
        pData->ppElem = &threadElem[apx];
        
        threads[apx] = CreateThread(NULL, 0, APGetThreadProc, pData, 0, NULL);
        if (NULL == threads[apx])
        {
            // If creating a thread fails, we will fail this operation,
            // but we must wait for existing threads to finish.
            result = GetLastError();
            break;
        }

        if (++numThreads >= MAXIMUM_WAIT_OBJECTS)
            break; 
    }

    // Wait for the sub-threads to finish.
    while (0 < numThreads)
    {
        int finished;
        DWORD apResult = WaitForThread(threads, numThreads, &finished, TEXT("AP retrieval"));
        if (0 > finished)
            break;

        APGetThreadData_t *pData = &threadData[finished];

        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        LogDebug(TEXT("Finished retrieving AP \"%s\" after %.2f seconds: %s"),
                 pData->pAP->GetAPName(),
                 (double)runTime / 1000.0,
                 Win32ErrorText(apResult));

        // Save the generated XML element.
        result |= apResult;
        if (NO_ERROR == apResult)
        {
            assert(NULL != *(pData->ppElem));
            pRoot->AppendChild(*(pData->ppElem));
        }
        
        if (--numThreads != finished)
        {
            threadData[finished] = threadData[numThreads];
            threads   [finished] = threads   [numThreads];
        }
    }

    // Generate the document.
    if (NO_ERROR == result)
    {
        hr = WiFUtils::CreateXmlDocument(pRoot, ppDoc);
        if (FAILED(hr))
            return HRESULT_CODE(hr);
        pRoot.release();
    }
    
    return result;
}

// -----------------------------------------------------------------------------
//
// Queries the current AP configs.
//
static DWORD
QueryAPConfigs(
    APService_t *pAPs)
{
    ce::auto_ptr<XmlDocument_t> pDoc = NULL;
    DWORD result = GenerateAPConfigsDoc(pAPs, &pDoc);
    if (NO_ERROR != result)
        return result;
    assert(pDoc.valid());

    ce::tstring xmlText;
    result = WiFUtils::FormatXmlDocument(*pDoc, &xmlText);
    if (NO_ERROR != result)
        return result;

    LogAlways(xmlText);
    return NO_ERROR;
}

// -----------------------------------------------------------------------------
//
// Updates the specified AP's configuration in a sub-thread.
//
struct APSetThreadData_t
{
    APController_t *pAP;
    const XmlElement_t *pElem;
};

static DWORD WINAPI
APSetConfigProc(
    IN LPVOID pParameter)
{
    HRESULT hr;
    APSetThreadData_t *pData = reinterpret_cast<APSetThreadData_t *>(pParameter);

    LogDebug(TEXT("Updating AP \"%s\" configuration"),
             pData->pAP->GetAPName());
    
    hr = pData->pAP->DecodeFromXml(*(pData->pElem));
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    hr = pData->pAP->SaveConfiguration();
    if (FAILED(hr))
    {
        LogError(TEXT("Can't update AP %s configuration: %s"),
                 pData->pAP->GetAPName(),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    return NO_ERROR;
}
    
// -----------------------------------------------------------------------------
//
// Reads and sets the AP config(s) from the specified file.
//
static DWORD
ReadAPConfigs(
    APService_t *pAPs,
    const TCHAR *pFileName)
{
    HRESULT hr;
    DWORD result;
    DWORD startTime = GetTickCount();

    // Parse the file.
    XmlDocument_t doc;
    DocumentError_t error;
    if (!doc.Load(pFileName, error))
    {
        LogError(TEXT("Error parsing XML file \"%s\":")
                 TEXT(" line=%d column=%u error-code=%x: %ls"),
                 pFileName, error.Loc.LineNum, error.Loc.ColumnNum,
                 LXML_EXTRACT_ERROR(error.Code), &(error.Details)[0]);
        return LXML_EXTRACT_ERROR(error.Code);
    }
    
    // Connect to the AP-Control server.
    result = pAPs->Connect();
    if (NO_ERROR != result)
        return result;

    // Keep track of the sub-thread handles.
    APSetThreadData_t threadData[MAXIMUM_WAIT_OBJECTS];
    HANDLE threads[MAXIMUM_WAIT_OBJECTS];
    int numThreads = 0;

    // Look up and update each of the AP-config(s).
    result = NO_ERROR;
    XmlChildWalker_t iter(doc.GetRoot());
    for (; !iter.End() && numThreads < MAXIMUM_WAIT_OBJECTS ; iter.Next())
    {
        const XmlBaseElement_t *pBase = iter.Get();
        if (XmlBaseElement_t::EtElement != pBase->GetType())
            continue;
        const XmlElement_t *pElem = (const XmlElement_t *)pBase;

        XmlString_t xmlName = pElem->GetName();
        if (xmlName != L"APController")
        {
            LogError(TEXT("Unrecognized XML element \"%s\" in file \"%s\""),
                    &xmlName[0], pFileName);
            result = ERROR_INVALID_DATA;
            break;
        }

        if (!pElem->GetAttributeText(L"name", &xmlName))
        {
            LogError(TEXT("Can't find \"name\" in APController tag in file \"%s\""),
                     pFileName);
            result = ERROR_INVALID_DATA;
            break;
        }

        ce::tstring apName;
        hr = WiFUtils::ConvertString(&apName, xmlName, "AP-name");
        if (FAILED(hr))
        {
            result = HRESULT_CODE(hr);
            break;
        }

        APController_t *pAP = pAPs->FindAP(apName);
        if (NULL == pAP)
        {
            LogError(TEXT("Can't find AP \"%s\" in APControl database"),
                     apName);
            result = ERROR_INVALID_DATA;
            break;
        }

        APSetThreadData_t *pData = &threadData[numThreads];
        pData->pAP = pAP;
        pData->pElem = pElem;
        
        threads[numThreads] = CreateThread(NULL, 0, APSetConfigProc, pData, 0, NULL);
        if (NULL == threads[numThreads])
        {
            result = GetLastError();
            LogError(TEXT("Can't start thread to configure Access Point: %s"),
                     Win32ErrorText(result));
        }

        numThreads++;
    }

    // Wait for the ap-configuration threads to finish.
    while (0 < numThreads)
    {
        int finished;
        DWORD apResult = WaitForThread(threads, numThreads, &finished, TEXT("AP configuration"));
        if (0 > finished)
            break;

        APSetThreadData_t *pData = &threadData[finished];

        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        LogDebug(TEXT("Finished configuring AP \"%s\" after %.2f seconds: %s"),
                 pData->pAP->GetAPName(),
                 (double)runTime / 1000.0,
                 Win32ErrorText(apResult));
        
        result |= apResult;
        
        if (--numThreads != finished)
        {
            threadData[finished] = threadData[numThreads];
            threads   [finished] = threads   [numThreads];
        }
    }
    
    return result;
}

// -----------------------------------------------------------------------------
//
// Writes the current AP configs to the specified file.
//
static DWORD
WriteAPConfigs(
    APService_t *pAPs,
    const TCHAR *pFileName)
{
    HRESULT hr;
    DWORD result;
    
    ce::auto_ptr<XmlDocument_t> pDoc = NULL;
    result = GenerateAPConfigsDoc(pAPs, &pDoc);
    if (NO_ERROR != result)
        return result;
    assert(pDoc.valid());

    XmlString_t xmlText;
    result = WiFUtils::FormatXmlDocument(*pDoc, &xmlText);
    if (NO_ERROR != result)
        return result;

    ce::string mbText;
    hr = WiFUtils::ConvertString(&mbText, xmlText, "XML document text",
                                          xmlText.length(), CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    LogDebug(TEXT("Writing %lu byte XML document to \"%s\""),
             mbText.length(), pFileName);

    FILE *pFile = NULL;
    errno_t err = _tfopen_s(&pFile,pFileName, TEXT("w"));
    if (NULL == pFile || err != 0)
    {
        result = GetLastError();
        LogError(TEXT("Can't open output file \"%s\": %s"),
                 pFileName, Win32ErrorText(result));
        return result;
    }
    
    size_t written = fwrite(&mbText[0], 1, mbText.length(), pFile);
    if (written != mbText.length())
    {
        result = GetLastError();
        LogError(TEXT("Can't write %lu bytes to file \"%s\": %s"),
                 mbText.length(), pFileName, Win32ErrorText(result));
        fclose(pFile);
        return result;
    }

    LogDebug(TEXT("Output to file \"%s\" succeeded"), pFileName);
    fclose(pFile);
    return NO_ERROR;
}

// -----------------------------------------------------------------------------
//
// Actual program.
//
DWORD
MyNetMain_t::
Run(void)
{
    // Process the appropriate command.
    bool commandDone = false;
    DWORD result = NO_ERROR;

    if (m_pQueryAPConfigs->GetValue())
    {
        result = QueryAPConfigs(m_pAPs);
        commandDone = true;
    }
    
    const TCHAR *pInputName = m_pReadAPConfigs->GetValue();
    if (pInputName && pInputName[0])
    {
        result = ReadAPConfigs(m_pAPs, pInputName);
        commandDone = true;
    }
    
    const TCHAR *pOutputName = m_pWriteAPConfigs->GetValue();
    if (pOutputName && pOutputName[0])
    {
        result = WriteAPConfigs(m_pAPs, pOutputName);
        commandDone = true;
    }
    
    // Default to query.
    if (!commandDone)
    {
        result = QueryAPConfigs(m_pAPs);
    }
    
    return result;
}

// -----------------------------------------------------------------------------
//
// Startup intialization and shutdown cleanup.  We need to pair winsock 
// cleanup with APService_t cleanup, as APService_t may expect winsock to
// still be running when it shuts down.
//
DWORD
MyNetMain_t::
StartupInitialize(void)
{
    DWORD result = StartupWinsock(2,2);
    if (NO_ERROR != result)
        return result;

    m_pAPs = APService_t::GetInstance();
    if (NULL == m_pAPs)
        return ERROR_OUTOFMEMORY;

    return NO_ERROR;
}

// -----------------------------------------------------------------------------

DWORD
MyNetMain_t::
ShutdownCleanup(void)
{
    if (NULL != m_pAPs)
    {
        m_pAPs->DeleteInstance();
        m_pAPs = NULL;
    }
    return CleanupWinsock();
}

// -----------------------------------------------------------------------------
//
// Called by netmain. Generates the MyNetMain object and runs it.
//
int
mymain(
    int    argc,
    TCHAR *argv[])
{
#ifdef DEBUG
//    Sleep(20000);
#endif

    MyNetMain_t realMain;
    DWORD result;

    result = realMain.StartupInitialize();
    if (NO_ERROR == result)
    {
        result = realMain.Init(argc,argv);
        if (NO_ERROR == result)
        {
            result = realMain.Run();
        }
        DWORD cleanupResult = realMain.ShutdownCleanup();
        if (NO_ERROR == result)
            result = cleanupResult;
    }
    return int(result);
}

// -----------------------------------------------------------------------------

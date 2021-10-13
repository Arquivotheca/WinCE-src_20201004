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
// Implementation of the NetlogAnalyser_t class.
//
// ----------------------------------------------------------------------------

#include "NetlogAnalyser_t.hpp"
#include "WiFiRoam_t.hpp"

#include <Utils.hpp>

#include <assert.h>
#include <inc/sync.hxx>
#include <EAPOLParser.h>
#include <NetlogManager_t.hpp>

using namespace ce::qa;

// Syncronization object:
static ce::critical_section s_NetlogLocker;

// Flag indicating whether program is running:
static LONG s_fProgramRunning = 0;
inline bool
IsProgramRunning(void)
{
    return 1 == InterlockedCompareExchange(&s_fProgramRunning, 0, 0);
}

// List of netlog packet-capture files awaiting cleanup:
struct CaptureFile_t
{
    TCHAR *pFileName;
    long    FileTime;
    CaptureFile_t *pNext;
};
static CaptureFile_t *s_pCaptureFiles = NULL;

// Normal capture-file lifespan:
// (This was chosen to keep approximately the last 10 files.)
static const long s_CaptureFileLifespan = 10*90*1000L;

// ----------------------------------------------------------------------------
//
// Adds the specified packet-capture file to be list to be be managed.
//
static void
AddCaptureFile(
    IN TCHAR *pFileName)
{
    CaptureFile_t **pParent = &s_pCaptureFiles;
    for (;;)
    {
        CaptureFile_t *pFile = *pParent;
        if (NULL == pFile)
        {
            pFile = new CaptureFile_t;
            if (NULL == pFile)
            {
                DeleteFile(pFileName);
                free(pFileName);
            }
            else
            {
                pFile->pFileName = pFileName;
                pFile->FileTime = GetTickCount();
                pFile->pNext = NULL;
               *pParent = pFile;
            }
            break;
        }
        pParent = &(pFile->pNext);
    }
}

// ----------------------------------------------------------------------------
//
// Deletes all files which have been waiting longer than the specified time.
//
static void
DeleteCaptureFiles(
    IN const long MaximumLifespan)
{
    CaptureFile_t *pFile;
    while ((pFile = s_pCaptureFiles) != NULL)
    {
        long lifespan = WiFUtils::SubtractTickCounts(pFile->FileTime,
                                                     GetTickCount());
        if (MaximumLifespan > lifespan)
            break;

        s_pCaptureFiles = pFile->pNext;
       
        DeleteFile(pFile->pFileName);
        free(pFile->pFileName);
        delete pFile;
    }
}

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
NetlogAnalyser_t::
StartupInitialize(void)
{
    InterlockedExchange(&s_fProgramRunning, 1);
}

void
NetlogAnalyser_t::
ShutdownCleanup(void)
{
    ce::gate<ce::critical_section> netLocker(s_NetlogLocker);
    DeleteCaptureFiles(0);
    InterlockedExchange(&s_fProgramRunning, 0);
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
NetlogAnalyser_t::
NetlogAnalyser_t(const TCHAR *pNetlogBasename)
  : m_pBasename(pNetlogBasename),
    m_pManager(NULL),
    m_NumberCaptures(0)
{
    m_CapFileName[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
NetlogAnalyser_t::
~NetlogAnalyser_t(void)
{
    if (m_pManager)
    {
        m_pManager->Stop();
        delete m_pManager;
        m_pManager = NULL;
    }
    m_NumberCaptures = 0;
}

// ----------------------------------------------------------------------------
//
// Runs a sub-thread to analyse the specified netlog capture file to
// calculate roaming times.
//
struct NetlogArgs
{
    ConnStatus_t conn;
    TCHAR       *file;
};
static DWORD WINAPI
NetlogAnalizer(
    LPVOID pParameter)
{
    DWORD result = ERROR_SUCCESS;
    NetlogArgs args = *((NetlogArgs *)pParameter);
    
    if (IsProgramRunning())
    {
        LogDebug(TEXT("Parsing netlog capture file \"%s\""), args.file);

        EapParser eap;
        MACAddr_t mac = args.conn.wifiConn.GetInterfaceMACAddr();
        eap.SetSrc(mac.m_Addr);
     // LogStatus(TEXT("parsing mac \"%s\""), args.conn.wifiConn.GetInterfaceMAC());
        LogDebug(TEXT("Parsing packets for MAC \"%s\""), args.conn.wifiConn.GetInterfaceMAC());

        HRESULT hr = eap.ParseFromCapFile(args.file);
        if (FAILED(hr))
        {
            LogError(TEXT("Netlog cap-file parsing failed: %s"), HRESULTErrorText(hr));
            LogStatus(TEXT("netlog parse failed: %s"), HRESULTErrorText(hr));
            result = HRESULT_CODE(hr);
            free(args.file);
        }
    }

    if (IsProgramRunning() && ERROR_SUCCESS == result)
    {
        ce::gate<ce::critical_section> netLocker(s_NetlogLocker);
        if (IsProgramRunning())
        {
            if (ERROR_SUCCESS == result)
            {
               _int64 minTime = _I64_MAX;
               _int64 maxTime = 0;
               _int64 totTime = 0;

                EapParser eap;
                int listSize = eap.iter_list.size() - 1;
                for (int ex = 0 ; listSize > ex ; ++ex)
                {
                    Iteration *it = eap.iter_list[ex];
                    if (it->bad_state.size() < 1)
                    {
                        if (minTime > it->roam_time)
                            minTime = it->roam_time;
                        if (maxTime < it->roam_time)
                            maxTime = it->roam_time;
                        totTime += it->roam_time;
                    }
                }

                LogDebug(TEXT("Netlog capture file \"%s\" parsed:"), args.file);
                LogDebug(TEXT("Roam time - min: %I64i"), minTime);
                LogDebug(TEXT("            max: %I64i"), maxTime);
                LogDebug(TEXT("            tot: %I64i"), totTime);
                LogDebug(TEXT("            roams: %i"), eap.total_pass);

                if (eap.total_pass == 0)
                {
                    LogStatus(TEXT("err: no eapol in \"%s\""), args.file);
                }
                else
                {
             //     LogStatus(TEXT("parsed %i roams: %I64i/%.0lf/%I64i"),
             //               eap.total_pass, minTime,
             //               (double)totTime / (double)eap.total_pass,
             //               maxTime);
                    WiFiRoam_t::GetInstance()->AddEAPOLRoamTimes(minTime,
                                                                 maxTime,
                                                                 totTime,
                                                             eap.total_pass);
                }
            }

            // Keep the files around for a while.
            AddCaptureFile(args.file);
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Starts a sub-thread to analyse the specified netlog capture file
// to determine the roaming times.
//
static void
StartNetlogAnalysis(
    const TCHAR *pFileName)
{
    size_t nameLength = (_tcslen(pFileName) + 1) * sizeof(TCHAR);
    TCHAR *pTempName = (TCHAR *) malloc(nameLength);
    if (!pTempName)
    {
        LogError(TEXT("Can't allocate %u bytes for netlog file-name"),
                 nameLength);
        return;
    }
    memcpy(pTempName, pFileName, nameLength);

    static NetlogArgs args;
    args.conn = WiFiRoam_t::GetInstance()->GetStatus();
    args.file = pTempName;
    
    HANDLE hThread = CreateThread(NULL, 0, NetlogAnalizer, &args, 0, NULL);
    if (!hThread)
    {
        LogError(TEXT("Can't create thread to analyze roaming times"));
        free(pTempName);
        return;
    }

    LogDebug(TEXT("Started thread 0x%x to calculate roam times"), hThread);
    CloseHandle(hThread);
}

// ----------------------------------------------------------------------------
//
// Called at the begining of each cycle. If there's an existing netlog
// capture file, stops netlog and fires off a thread to analyse the
// roaming times. In any case, if this isn't the first cycle we've
// seen, starts another capture.
void
NetlogAnalyser_t::
StartCapture(
    DWORD CycleNumber)
{
    m_NumberCaptures++;

    // Delete old capture files.
    {
        ce::gate<ce::critical_section> netLocker(s_NetlogLocker);
        DeleteCaptureFiles(s_CaptureFileLifespan);
    }
    
    if (!m_pManager)
    {
        m_pManager = new NetlogManager_t;
        if (!m_pManager)
        {
            LogError(TEXT("Can't allocate NetlogManager object"));
            return;
        }
        if (m_pManager->Load()
         && m_pManager->SetMaximumFileSize(1000L*1000L)
         && m_pManager->SetMaximumPacketSize(2L*1000L))
        {
            LogStatus(TEXT("loaded netlog driver"));
        }
        else
        {
            LogStatus(TEXT("err: can't load netlog driver"));
            delete m_pManager;
            m_pManager = NULL;
            return;
        }
    }

    m_pManager->Stop();

    if (m_CapFileName[0])
    {
        LogDebug(TEXT("Captured netlog output into %s"), m_CapFileName);
        StartNetlogAnalysis(m_CapFileName);
    }

    if (1L < m_NumberCaptures)
    {
        _stprintf(m_CapFileName, TEXT("%s%d"), m_pBasename, CycleNumber);
        if (!m_pManager->SetFileBaseName(m_CapFileName))
        {
            LogError(TEXT("Can't set netlog basename to \"%s\""),
                     m_CapFileName);
            LogStatus(TEXT("err: can't set netlog to \"%s\""),
                      m_CapFileName);
            m_CapFileName[0] = 0;
        }
        else
        if (!m_pManager->Start())
        {
            LogError(TEXT("Can't start netlog capture into \"%s\""),
                     m_CapFileName);
            LogStatus(TEXT("err: can't start netlog \"%s\""),
                      m_CapFileName);
            m_CapFileName[0] = 0;
        }
        else
        {
            _stprintf(m_CapFileName, TEXT("\\%s%d.cap"),
                      m_pManager->GetFileBaseName(),
                      m_pManager->GetCaptureFileIndex());
            LogDebug(TEXT("Capturing netlog output into %s"),
                     m_CapFileName);
        }
    }
}

// ----------------------------------------------------------------------------

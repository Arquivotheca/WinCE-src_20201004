//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <Winbase.h>
#include <CReg.hxx>

#include "SMB_Globals.h"
#include "PrintQueue.h"
#include "ShareInfo.h"
#include "SMBPackets.h"
#include "SMBErrors.h"
#include "ConnectionManager.h"
#include "Cracker.h"
#include "SMBCommands.h"




BOOL PrintQueue::fIsRunning = FALSE;

//blocking writes simulates a stalled printer (to test queue managment)
//#define PRINTQUEUE_BLOCK_WRITES // <-- uses PRINTQUEUE_MS_PER_BYTE (this slows the SMB)
//#define PRINTQUEUE_BLOCK_READS  // <-- uses PRINTQUEUE_MS_PER_BYTE (this slows the printing)
//#define PRINTQUEUE_USE_FILE 
//#define PRINTQUEUE_MS_SLEEP_TO_START_QUEUE 5000
//#define PRINTQUEUE_MS_PER_KILOBYTE 100

#define DEFAULT_SWAP_FILE L"\\Printer.swap"
#define DEFAULT_SWAP_MAX 2000000
#define DEFAULT_SWAP_INC 10000

pool_allocator<10, PrintJob>    SMB_Globals::g_PrintJobAllocator;



//
// Forward declare functions here
VOID FileTimeToDosDateTime(FILETIME * ft,WORD *ddate,WORD *dtime);
DWORD DosToNetTime(WORD time, WORD date);

DWORD 
PrintQueue::SMBSRV_SpoolThread(LPVOID lpvPrintQueue)
{
   UINT uiToBeWrittenSize = 20000;
   BYTE *pToBeWritten = new BYTE[uiToBeWrittenSize]; 
   SMBPrintQueue *pPrintQueue = (SMBPrintQueue *)lpvPrintQueue;
   
   if(NULL == pPrintQueue) { 
        ASSERT(FALSE);
        goto Done;
   }   
   if(NULL == pToBeWritten) {
        goto Done;
   }
   for(;;)
   {        
        PrintJob *pPrintJob = NULL;
        HANDLE hPrinter = INVALID_HANDLE_VALUE;            
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Waiting for new print job to enter the queue... ZZZzzzz"));
        
        if(pPrintQueue->ShuttingDown()) {
             TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Shutting down -- stopping spool thread"));
             break;
        }   
    
        //
        // Get the next job (remember to Release() it!)   
        if(NULL == (pPrintJob = pPrintQueue->WaitForJobThatIsReady())) {            
            TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: we woke up but there isnt anything to print... maybe we were removing a dead job"));
            continue;
        }  
        
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: I'm up... I'm up... Got a job to start printing!"));
        
        
        //
        // See if the job has been aborted, if so kill it off
        if(pPrintJob->GetInternalStatus() & JOB_STATUS_ABORTED) {
            TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: this job was aborted... kill it off"));
             
            //
            //  Release our job, if there are no more refs to it, it will go away
            pPrintJob->Release();
            continue;
        }
        
#if defined (PRINTQUEUE_MS_SLEEP_TO_START_QUEUE)
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: going to sleep for %d ms to simulate slow printer?",PRINTQUEUE_MS_SLEEP_TO_START_QUEUE));
        Sleep(PRINTQUEUE_MS_SLEEP_TO_START_QUEUE);
#endif
        
#if !defined(PRINTQUEUE_USE_FILE) 
        //
        // Open up the parallel port       
        if(NULL == pPrintJob->MyShare()) 
        {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE:  Error opening opening up share"));         
            goto Done;
        }
        if(INVALID_HANDLE_VALUE == (hPrinter = CreateFile(pPrintQueue->GetPortName(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,NULL)))
        //if(INVALID_HANDLE_VALUE == (hPrinter = CreateFile(L"myprinter.file", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,NULL)))
        {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE:  Error opening printer on %s (err=%d)", pPrintQueue->GetPortName(), GetLastError()));           
            Sleep(5000); 
            
            //
            // Thump the queue 
            pPrintQueue->JobReadyForPrint(NULL);
            continue;
        }
#else        
        
        WCHAR TempFileName[MAX_PATH];
        GetTempFileName(L"\\", L"SMBSpool", 0, TempFileName);
        if(INVALID_HANDLE_VALUE == (hPrinter = CreateFile(TempFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,NULL)))
        {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE: Error opening printer on %s (err=%d)", pPrintQueue->GetPortName(), GetLastError()));           
            Sleep(5000); 
            
            //
            // Thump the queue 
            pPrintQueue->JobReadyForPrint(NULL);            
            continue;
        }
#endif      
        
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Job:%d has started printing", pPrintJob->JobID()));
        
        //
        // Mark bit saying the job is printing
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_PRINTING);        
        
        //
        // Loop while there is data
        UINT uiRead;
        UINT uiTotalRead = 0;
        BOOL fDontWrite = FALSE;
        HRESULT hr;
        while(SUCCEEDED(hr = pPrintJob->Read(pToBeWritten, uiToBeWrittenSize, &uiRead))) {
        
            if(0 == uiRead && 
               0 == pPrintJob->BytesLeftInQueue() && 
               (pPrintJob->GetInternalStatus() & JOB_STATUS_FINISHED)) {
                TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: nothing left to read -- we are done, close up"));
                break;
            }
            uiTotalRead += uiRead;
            
            //
            // Write back to printer
            BYTE *pToWrite = pToBeWritten;
            while(uiRead) {
                DWORD dwWritten;
                if(FALSE == fDontWrite && 0 == WriteFile(hPrinter, pToWrite, uiRead, &dwWritten, NULL)) 
                {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE:  Error writing to printer! (err=%d)", GetLastError()));
                    ASSERT(FALSE);
                    fDontWrite = TRUE;
                } else if(TRUE == fDontWrite) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE:  MAJOR Error writing to printer! (err=%d) - lying to flush buffers!", GetLastError()));
                    dwWritten = uiRead;
                }
#if defined(PRINTQUEUE_BLOCK_READS) && defined(DEBUG)  
                Sleep(PRINTQUEUE_MS_PER_KILOBYTE * dwWritten / 1024);
#endif

                TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Just Sent %d bytes to printer", dwWritten));
                pToWrite += dwWritten;
                uiRead -= dwWritten;
           }
        }
        

        //
        // Mark the job saying we are done with it
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Finished print job... closing handle and going to sleep (%d bytes read/written for job)", uiTotalRead));  
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_PRINTED);
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() & (~JOB_STATUS_PRINTING));
        pPrintJob->Release();
        //
        // Close up shop
        CloseHandle(hPrinter);
        hPrinter = INVALID_HANDLE_VALUE;        
        
#if defined (PRINTQUEUE_USE_FILE)
        //
        // If we are spooling to file, delete the file
        DeleteFile(TempFileName);
#endif
   }
   
 
#if !defined(PRINTQUEUE_USE_FILE)       
   Done:    
#endif   
   
    //
    // We need to go and release all jobs that are ours. do this by marking them
    //   invalid, and releasing them
    {  
        PrintJob *pPrintJob = NULL;        
        while(NULL != (pPrintJob = pPrintQueue->GetJobNextJobThatIsReady())) { 
            pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_PRINTED);
            pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() & (~JOB_STATUS_PRINTING));        
            pPrintJob->Release(); 
        }  
    }

    if(NULL != pToBeWritten) {
        delete [] pToBeWritten;
    }
    return 0;
}




HRESULT StartPrintSpool()
{
    CReg RegPrinter;
    HRESULT hr = E_FAIL;
    WCHAR SwapFile[MAX_PATH];
    UINT uiMax, uiInc;


    //
    // Load the CName from the registry
    if(FALSE == RegPrinter.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer")) {  
        swprintf(SwapFile, DEFAULT_SWAP_FILE);
        uiMax = DEFAULT_SWAP_MAX;
        uiInc = DEFAULT_SWAP_INC;
    } else {
        if(FALSE == RegPrinter.ValueSZ(L"PrintSwapFile", SwapFile, MAX_PATH)) {
            swprintf(SwapFile, DEFAULT_SWAP_FILE);
        }
        uiMax = RegPrinter.ValueDW(L"PrintSwapMax", DEFAULT_SWAP_MAX);
        uiInc = RegPrinter.ValueDW(L"PrintSwapInc", DEFAULT_SWAP_INC);
    }

    TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_PRINT:  Configuring swap file."));
    TRACEMSG(ZONE_PRINTQUEUE, (L"            File: %s", SwapFile));
    TRACEMSG(ZONE_PRINTQUEUE, (L"            Max : %d", uiMax));
    TRACEMSG(ZONE_PRINTQUEUE, (L"            Inc : %d", uiInc));


    //
    // Fire up our swap memory
    if(FAILED(hr = SMB_Globals::g_PrinterMemMapBuffers.Open(SwapFile, uiMax, uiInc))) {
        TRACEMSG(ZONE_ERROR, (L"SMB_PRINT: Error Opening memory map! -- cant continue"));
        goto Done;
    }
    ASSERT(FALSE == PrintQueue::fIsRunning);   
    PrintQueue::fIsRunning = TRUE;

    Done:
        return hr;
}


HRESULT StopPrintSpool()
{
    HRESULT hr = E_FAIL;   
    UINT i, uiNumShares; 
    //
    // Verify incoming params
    if(FALSE == PrintQueue::fIsRunning) {        
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE:  stopping print spool not required since it never started!"));
        hr = S_OK;
        goto Done;
    }
        
    //
    // At this point we know that no new shares are going to be ENTERED
    //   so its okay to enumerate through them killing off jobs  
    uiNumShares = SMB_Globals::g_pShareManager->NumShares();
    for(i=0; i<uiNumShares; i++) {
        Share         *pShare = SMB_Globals::g_pShareManager->SearchForShare(i);
        SMBPrintQueue *pQueue;
        if(NULL != pShare && NULL!=(pQueue = pShare->GetPrintQueue())){
            //    
            // Tap the event saying there is a job ready (this is a lie, but 
            //   the thread will check to see if its supposed to shutdown so its 
            //   okay (it saves an event)
            pQueue->SetShutDown(TRUE);
            pQueue->StopAllJobs();
            pQueue->JobReadyForPrint(NULL);
        }
    }    
           
    hr = S_OK;
    ASSERT(TRUE == PrintQueue::fIsRunning);  

    Done:
        if(SUCCEEDED(hr)) {              
             PrintQueue::fIsRunning = FALSE;
        }    
        return hr;
}


HRESULT SetString(CHAR **pOldString, CHAR *pNewString)
{
    HRESULT hr;
    UINT uiLen;
    CHAR *pTemp;
    
    //verify incoming params
    if(NULL == pNewString) {
        hr = E_INVALIDARG;
        goto Done;
    }
    
    //make a new memory buffer for the name
    uiLen = strlen(pNewString) + 1;
    pTemp = new CHAR[uiLen];
    
    //if we are out of memory return
    if(NULL == pTemp) {
        hr = E_OUTOFMEMORY;
        goto Done;
    }
 
    //delete old memory and replace with new
    if(*pOldString)
        delete [] *pOldString;
    *pOldString = pTemp;
    
    //do the copy
    memcpy(*pOldString, pNewString, uiLen);  
    hr = S_OK;
    Done:
        return hr;
}



SMBPrintQueue::SMBPrintQueue()
{
    Priority = 0;
    StartTime = 0;
    UntilTime = 0;
    Pad1 = 0;
    pSepFile = 0;
    pPrProc = 0;
    pParams = 0;
    pComment = 0;
    Status = 0;
    cJobs = 0;
    pPrinters = 0;
    pDriverName = 0;
    pDriverData = 0;
    pName = 0;  
    wPortName = 0;
    fShutDown = FALSE;
    
    ReadyToPrintSemaphore = NULL;
   
    InitializeCriticalSection(&csJobListLock);
    
    //
    // Fire up the spool semaphore
    ReadyToPrintSemaphore = CreateSemaphore(NULL, 0, SMB_Globals::g_uiMaxJobsInPrintQueue ,NULL);
    
    //
    // Spin the print spool thread (this thread does the actual printing 
    //   MUST Be last for error handling
    if(NULL == (PrintThreadHandle = CreateThread(NULL, 0, PrintQueue::SMBSRV_SpoolThread, (VOID*)this, 0, NULL))) {
        TRACEMSG(ZONE_ERROR, (L"SSMBSRV-PRINTQUEUE: cant make spool thread"));
    }
}

SMBPrintQueue::~SMBPrintQueue()
{  
    //
    // Now block for the print thread to stop
    if(WAIT_FAILED == WaitForSingleObject(PrintThreadHandle, INFINITE)) {
        TRACEMSG(ZONE_INIT, (L"SMBSRV-PRINTQUEUE: Waiting for spool thread FAILED!"));
        ASSERT(FALSE);
    } 

    //
    // Destroy all members
    if(pName)
        delete [] pName;
    if(pSepFile)
        delete [] pSepFile;
    if(pPrProc)
        delete [] pPrProc;
    if(pParams)
        delete [] pParams;
    if(pComment)
        delete [] pComment;
    if(pPrinters)
        delete [] pPrinters;
    if(pDriverName)
        delete [] pDriverName;
    if(pDriverData)
        delete [] pDriverData;
    if(wPortName)
        delete [] wPortName;
 
    //
    // Clean up memory    
    DeleteCriticalSection(&csJobListLock);   
    CloseHandle(ReadyToPrintSemaphore);
    CloseHandle(PrintThreadHandle);
    ReadyToPrintSemaphore = INVALID_HANDLE_VALUE;
    PrintThreadHandle = INVALID_HANDLE_VALUE;

    ASSERT(INVALID_HANDLE_VALUE == PrintThreadHandle);
    ASSERT(0 == PrintJobList.size());
}


PrintJob *
SMBPrintQueue::GetJobNextJobThatIsReady()
{
    CCritSection csLock(&csJobListLock);  
    PrintJob *pJob = NULL;
    csLock.Lock();
    if(0 != this->GetNumJobs()) {
        ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator it;
        ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEnd = PrintJobList.end();            

        for(it=PrintJobList.begin(); it!=itEnd; it++) {
            BOOL fInQueue = (*it)->GetInternalStatus() & JOB_STATUS_IN_QUEUE;
            BOOL fAborted = (*it)->GetInternalStatus() & JOB_STATUS_ABORTED;  
            BOOL fPrinted = (*it)->GetInternalStatus() & JOB_STATUS_PRINTED;
            if(fInQueue && 
               !fAborted &&
               !fPrinted) {
                pJob = (*it);
            }
        }    
    }
    return pJob; 
}
 
PrintJob * 
SMBPrintQueue::WaitForJobThatIsReady() 
{
    PrintJob *pPrintJob = NULL;
    
    
    //
    // If we are shutting down return back
    if(TRUE == ShuttingDown()) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: shutting down print spool!"));
        goto Done;        
    }     
    
    //
    // See if we have a job waiting (no need to block then)... the rule on blocking
    //   is you can only do it once...  meaning once a job is put in the queue
    //   and the semaphore is incremented you can only WaitForSingleObject
    //   on that one once.  this makes sense -- but its nice to call GetNextJobWithData()
    //   again in the case of LPT failure (where CreateFile failes).  
    //   so dont block if there is something in the queue already.
    if(NULL != (pPrintJob = GetJobNextJobThatIsReady())) {
        goto Done;
    }
        
    //
    // Now since there isnt anything in the queue, block untile there is
    //  
    if(WAIT_FAILED == WaitForSingleObject(ReadyToPrintSemaphore, INFINITE)) {
        TRACEMSG(ZONE_INIT, (L"SMBSRV-PRINTQUEUE:Waiting for spool semaphore FAILED -- stopping thread!"));
        ASSERT(FALSE);
        goto Done;
    }  

    //
    // The way we signal the print queue to terminate is by setting shutdown 
    //   and settingthe ReadyToPrintSemaphore -- so handle that here
    if(TRUE == ShuttingDown()) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: shutting down print spool!"));            
        goto Done;         
    }
        
    //
    // Since we are awake, there prob is a job... fetch it
    if(0 != this->GetNumJobs()) {
        pPrintJob = GetJobNextJobThatIsReady();
        goto Done;
    }
   
    Done:
        // NOTE: do *NOT* addref this.  when the flag JOB_STATUS_IN_QUEUE
        //   is set, THEN do the addref
        return pPrintJob;
    
}
/*
VOID
SMBPrintQueue::RemoveNextJobWithData() 
{    
    CCritSection csLock(&csPrintJobsWithData);
    PrintJob *pMyJob = NULL;
    csLock.Lock();   
   
    if(NULL != (pMyJob = PrintJobsWithData.front())) {    
        PrintJobsWithData.pop_front();     
        pMyJob->Release();
    }
}       
       
HRESULT
SMBPrintQueue::RemoveJobWithData(USHORT usFID) 
{    
    CCritSection csLockWithData(&csPrintJobsWithData);
    csLockWithData.Lock();
    HRESULT hr = E_FAIL;
    
    //
    // Check jobs that are in the queue
    ce::list<PrintJob *>::iterator itWithData;
    ce::list<PrintJob *>::iterator itEndWithData = PrintJobsWithData.end();
        
    for(itWithData = PrintJobsWithData.begin(); itWithData != itEndWithData; ++itWithData) {
        if(usFID == (*itWithData)->JobID()) {            
            PrintJobsWithData.erase(itWithData++);
            hr = S_OK;
            break;
        }
    } 
   
    return hr;
}              
*/


HRESULT 
SMBPrintQueue::GetJobsInQueue(ce::list<PrintJob *, PRINTJOB_LIST_ALLOC> *pJobList)
{
    CCritSection csLockWithData(&csJobListLock);
    csLockWithData.Lock();
    HRESULT hr = E_FAIL;
    
    //
    // Check jobs that are in the queue
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itWithData;
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEndWithData = PrintJobList.end();
       
    //
    // Verify incoming args
    if(NULL == pJobList) {
        ASSERT(FALSE);
        hr = E_INVALIDARG;
        goto Done;
    }   
    ASSERT(0 == pJobList->size());
        
    for(itWithData = PrintJobList.begin(); itWithData != itEndWithData; ++itWithData) {
        BOOL fInQueue = (*itWithData)->GetInternalStatus() & JOB_STATUS_IN_QUEUE;
        BOOL fAborted = (*itWithData)->GetInternalStatus() & JOB_STATUS_ABORTED;  
        BOOL fPrinted = (*itWithData)->GetInternalStatus() & JOB_STATUS_PRINTED;
        if(fInQueue &&
           !fAborted &&
           !fPrinted)   {   
            pJobList->push_front(*itWithData);
            (*itWithData)->AddRef();
        }
    } 
    
    //
    // Success
    hr = S_OK;
   
    Done:
        return hr;
}



USHORT 
SMBPrintQueue::GetPriority() 
{
    return Priority;
}

VOID
SMBPrintQueue::SetPriority(USHORT prio)
{
    Priority = prio;
}


USHORT 
SMBPrintQueue::GetStartTime() 
{
    return StartTime;
}

VOID
SMBPrintQueue::SetStartTime(USHORT sTime)
{
    StartTime = sTime;
}


USHORT 
SMBPrintQueue::GetUntilTime() 
{
    return UntilTime;
}

VOID
SMBPrintQueue::SetUntilTime(USHORT sUntil)
{
    UntilTime = sUntil;
}




USHORT 
SMBPrintQueue::GetStatus() 
{
    return Status;
}

USHORT 
SMBPrintQueue::GetNumJobs() 
{
    CCritSection csLock(&csJobListLock);
    csLock.Lock();
    
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator it;
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEnd = PrintJobList.end();
    UINT uiCnt = 0;
    
    for(it=PrintJobList.begin(); it!=itEnd; it++) {
        BOOL fInQueue = (*it)->GetInternalStatus() & JOB_STATUS_IN_QUEUE;
        BOOL fAborted = (*it)->GetInternalStatus() & JOB_STATUS_ABORTED;  
        BOOL fPrinted = (*it)->GetInternalStatus() & JOB_STATUS_PRINTED;
        if(fInQueue && 
           !fAborted &&
           !fPrinted) {
             uiCnt ++;
        }
    }    
    return uiCnt;
}

VOID
SMBPrintQueue::SetStatus(USHORT status)
{
    Status = status;
}



const CHAR *
SMBPrintQueue::GetDriverName()
{
    return pDriverName;
}

HRESULT 
SMBPrintQueue::SetDriverName(CHAR *_pName) 
{
   return SetString(&pDriverName, _pName);
}



const CHAR *
SMBPrintQueue::GetName()
{
    return pName;
}

HRESULT 
SMBPrintQueue::SetName(CHAR *_pName) 
{
    return SetString(&pName, _pName);
}


const CHAR *
SMBPrintQueue::GetSepFile()
{
    return pSepFile;
}
HRESULT 
SMBPrintQueue::SetSepFile(CHAR *_pName) 
{
   return SetString(&pSepFile, _pName);
}



const CHAR *
SMBPrintQueue::GetPrProc()
{
    return pPrProc;
}

HRESULT 
SMBPrintQueue::SetPrProc(CHAR *_pName) 
{
   return SetString(&pPrProc, _pName);
}



const CHAR *
SMBPrintQueue::GetParams()
{
    return pParams;
}

HRESULT 
SMBPrintQueue::SetParams(CHAR *_pName) 
{
   return SetString(&pParams, _pName);
}



const CHAR *
SMBPrintQueue::GetComment()
{
    return pComment;
}
HRESULT 
SMBPrintQueue::SetComment(CHAR *_pName) 
{
   return SetString(&pComment, _pName);
}


const CHAR *
SMBPrintQueue::GetPrinters()
{
    return pPrinters;
}

HRESULT 
SMBPrintQueue::SetPrinters(CHAR *_pName) 
{
   return SetString(&pPrinters, _pName);
}


const CHAR *
SMBPrintQueue::GetDriverData()
{
    return pDriverData;
}


const WCHAR *
SMBPrintQueue::GetPortName()
{
    return wPortName;
}

HRESULT 
SMBPrintQueue::SetDriverData(CHAR *_pName) 
{
   return SetString(&pDriverData, _pName);
}

HRESULT 
SMBPrintQueue::SetPortName(WCHAR *_pName)
{
    
    UINT uiSize = wcslen(_pName) + 1;
    wPortName = new WCHAR[uiSize];
    wcscpy(wPortName, _pName);
    return S_OK;
}


HRESULT 
SMBPrintQueue::AddJobToSpool(PrintJob *pPrintJob)
{   
    HRESULT hr;
    
    CCritSection csLock(&csJobListLock);
    csLock.Lock();    
    
    //
    // Add the print job (note: WE DO NOT AddRef()... this is because the
    //  *ONLY* person to be removing us is the PrintJob destructor
    PrintJobList.push_front(pPrintJob);
    
    hr = S_OK;   
        
    return hr;
}

HRESULT
SMBPrintQueue::JobReadyForPrint(PrintJob *pPrintJob)
{
    HRESULT hr = E_FAIL;
   
    if(INVALID_HANDLE_VALUE == ReadyToPrintSemaphore) {
        hr = E_UNEXPECTED;
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE: ERROR!!  print queue semaphore is invalid!!"));
        ASSERT(FALSE);
        goto Done;
    }
    
         
    //
    // NOTE:  NULL is valid for the case of shutdown
    if(pPrintJob) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: Job %d ready for print -- transfering to print queue list", pPrintJob->JobID()));
        ASSERT(FALSE == (pPrintJob->GetInternalStatus() & JOB_STATUS_IN_QUEUE));    
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_IN_QUEUE);
        
        //
        // Give a reference to the print spool
        pPrintJob->AddRef();
    } 
    
    //
    // Note: its OKAY (not cool) to say your ready to print
    //   multiple times... the printing thread will just loop
    ReleaseSemaphore(ReadyToPrintSemaphore, 1, NULL);
    hr = S_OK; 
    
    Done:
        return hr;
}

VOID
SMBPrintQueue::StopAllJobs() 
{
    CCritSection csLock(&csJobListLock);
    csLock.Lock();
    
    //
    // Shut down any print jobs that might be outstanding
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator it;
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEnd = PrintJobList.end();
        
    for(it = PrintJobList.begin(); it != itEnd; ++it) {
        (*it)->ShutDown();
    }   
}


HRESULT 
SMBPrintQueue::FindPrintJob(USHORT Fid, PrintJob **pPrintJob)
{
    CCritSection csLock(&csJobListLock);
    csLock.Lock();
    
    //
    // Loop through all active jobs
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator it;
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEnd = PrintJobList.end();
        
    *pPrintJob = NULL;
    for(it = PrintJobList.begin(); it != itEnd; ++it) {
        if(Fid == (*it)->JobID()) {
            *pPrintJob = *it;
        }
    } 
    
    if(NULL != *pPrintJob) {
        (*pPrintJob)->AddRef();
        return S_OK;
    }    
    else
        return E_FAIL;
}


//
// NOTE: THIS IS ONLY TO BE CALLED BY THE PrintJob DESTRUCTOR!!!
//
HRESULT 
SMBPrintQueue::RemovePrintJob(PrintJob *pPrintJob)
{
    CCritSection csLock(&csJobListLock);
    csLock.Lock();
    HRESULT hr = E_FAIL;
    
    //
    // Loop through all active jobs
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator it;
    ce::list<PrintJob *, PRINTJOB_LIST_ALLOC >::iterator itEnd = PrintJobList.end();
       
    for(it = PrintJobList.begin(); it != itEnd; ++it) {
        if(pPrintJob == (*it)) {
            PrintJobList.erase(it++);
            hr = S_OK;
            break;
        }
    } 
    return hr;
}




HRESULT 
SMBPrintQueue::JobsFinished(PrintJob *pPrintJob)
{   
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV: printqueue -- closing spool for job %d", pPrintJob->JobID()));

    //
    // Here we set its status to finished... 
    if(pPrintJob) {
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_FINISHED);
    }
    return S_OK;    
}


/*
 *
 *  Print Job class information
 *
 */ 
PrintJob::PrintJob(Share *_pMyShare) : 
    m_pMyShare(_pMyShare)
{    
    m_fShutDown = FALSE;
    m_lRefCnt = 1;
    m_uiBytesWritten = 0;
    m_WakeUpThereIsData = CreateEvent(NULL, FALSE, FALSE, NULL);
    
    
    InitializeCriticalSection(&m_csRingBuffer);
    InitializeCriticalSection(&m_csMainLock);

    if(FAILED(SMB_Globals::g_pUniqueFID->GetID(&m_usJobID))) {
        ASSERT(FALSE);
        m_usJobID = 0xFFFF;  
    }
    ASSERT(m_usJobID != 0xFFFF);
    
    this->SetInternalStatus(JOB_STATUS_UNKNOWN);
        
    //
    // Write down the start time (format per spec in cirsprt.doc)
    m_ulStartTime = SecSinceJan1970_0_0_0();
    
    //
    // Add ourselves to our print queue
    SMBPrintQueue *pMyQueue = NULL;
    if(m_pMyShare) {
        pMyQueue = m_pMyShare->GetPrintQueue();
    }
    ASSERT(NULL != pMyQueue);
    
    if(pMyQueue) {
        pMyQueue->AddJobToSpool(this);
    }       
}



PrintJob::~PrintJob()
{
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-PRINT: Job:%d finished", JobID()));
    
    //
    // Remove the print job from our queue
    SMBPrintQueue *pMyQueue = NULL;
    if(m_pMyShare) {
        pMyQueue = m_pMyShare->GetPrintQueue();
    }
    ASSERT(NULL != pMyQueue);
    
    if(pMyQueue) {
        pMyQueue->RemovePrintJob(this);
    }    
   
    if(0xFFFF != m_usJobID) {
       SMB_Globals::g_pUniqueFID->RemoveID(m_usJobID);
    }
    
    DeleteCriticalSection(&m_csRingBuffer);
    DeleteCriticalSection(&m_csMainLock);
    CloseHandle(m_WakeUpThereIsData);
}



VOID
PrintJob::SetInternalStatus(USHORT s)
{
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();
    
    m_usJobInternalStatus = s; 
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV: PrintQUEUE -- setting internal status on job %d -- status=(0x%x)!", JobID(), s));
        
    IFDBG(if(s&JOB_STATUS_UNKNOWN)TRACEMSG(ZONE_PRINTQUEUE, (L"      Unknown")));
    IFDBG(if(s&JOB_STATUS_HAS_DATA)TRACEMSG(JOB_STATUS_HAS_DATA, (L"      Has Data")));
    IFDBG(if(s&JOB_STATUS_FINISHED)TRACEMSG(JOB_STATUS_FINISHED, (L"      Finished")));
    IFDBG(if(s&JOB_STATUS_FINISHED)TRACEMSG(JOB_STATUS_FINISHED, (L"      In Print Queue")));
    IFDBG(if(s&JOB_STATUS_ABORTED)TRACEMSG(JOB_STATUS_ABORTED, (L"      Aborted")));
    IFDBG(if(s&JOB_STATUS_PAUSED)TRACEMSG(JOB_STATUS_PAUSED, (L"      Paused")));

    //
    // If we have been aborted, purge out the ringbuffer (this will happen on delete etc)
    if(0 != (s&JOB_STATUS_ABORTED)) {
        CCritSection csRingLock(&m_csRingBuffer);
        csRingLock.Lock();
        m_QueuedData.Purge();
    }

    //if they set the finished bit, wake up the writer
    if(0 != (s & JOB_STATUS_FINISHED)) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"....setting finished bit!"));
        SetEvent(m_WakeUpThereIsData);       
    }    
}

HRESULT 
PrintJob::SetComment(const WCHAR *_pComment)
{
    HRESULT hr;
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();    
    
    if(FAILED(hr = m_Comment.Clear())) {
        return hr;
    }
    return m_Comment.append(_pComment);    
}

HRESULT
PrintJob::SetQueueName(const WCHAR *pName)
{
    HRESULT hr;
    CCritSection csLock(&m_csMainLock);
    csLock.Lock(); 
    
    if(FAILED(hr = m_QueueName.Clear())) {
        return hr;
    }
    return m_QueueName.append(pName);    
}

HRESULT
PrintJob::SetOwnerName(const WCHAR * pName)
{
    HRESULT hr;
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();
    
    if(FAILED(hr = m_OwnerName.Clear())) {
        return hr;
    }
    return m_OwnerName.append(pName);
}


HRESULT 
PrintJob::Read(BYTE *pDest, UINT uiSize, UINT *pRead)
{
    HRESULT hr;
    BOOL fNeedToBlock = FALSE;
    
    CCritSection csLock(&m_csRingBuffer);
    csLock.Lock();
    *pRead = 0;
        
    //
    // if there isnt any data to read, we need to block until there is data
    if(0 == m_QueuedData.BytesReadyToRead() && 
       0 == (this->GetInternalStatus() & JOB_STATUS_FINISHED)) {
        csLock.UnLock();
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: print spool read()  going to block (queue size %d)", m_QueuedData.BytesReadyToRead()));  
        if(WAIT_FAILED == WaitForSingleObject(m_WakeUpThereIsData, INFINITE)) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-PRINTQUEUE: error waiting for read event!!")); 
            ASSERT(FALSE);
            hr = E_UNEXPECTED;
            goto Done;
        }
        csLock.Lock();
    }
    ASSERT(csLock.IsLocked());
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: print spool read() has %d bytes in queue", m_QueuedData.BytesReadyToRead()));
    
    if(TRUE == this->m_fShutDown) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: we have been told to shut down... canceling the request"));
        return E_FAIL;
    }

    //
    // Do the actual Read()
    hr = m_QueuedData.Read(pDest, uiSize, pRead);
   
    Done:
        ASSERT(SUCCEEDED(hr));
        ASSERT(uiSize >= *pRead);
        return hr;
}

HRESULT
PrintJob::Write(const BYTE *pData, UINT uiSize, ULONG ulOffset, UINT *uiWritten)
{ 
    HRESULT hr = E_FAIL;
    BOOL fWakeupThreadOnExit = ((0==m_uiBytesWritten) && uiSize)?TRUE:FALSE;
#ifdef DEBUG
    UINT uiBytesBefore = 0;
    BOOL fBypassCountCheck = FALSE;
#endif  
    CCritSection csLock(&m_csRingBuffer);
    UINT uiWrittenInPass;
    if(NULL == pData || NULL == uiWritten) {
        hr = E_INVALIDARG;
        goto Done;
    }  
    *uiWritten = 0;    
    csLock.Lock();
    
    //
    // If the job has been ended, this means it was removed 
    //   by someone OTHER than the person writing!  this is okay, but we
    //   have to do something -- tell them its okay
    if(GetInternalStatus() & (JOB_STATUS_FINISHED | JOB_STATUS_ABORTED)) {
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINT: Job:%d has been closed, but TID is still valid (unclosed). -- just pretend that we wrote", JobID()));
        *uiWritten = uiSize;
        hr = S_OK;
#ifdef DEBUG
        fBypassCountCheck = TRUE;
#endif
        goto Done;
    }   
    
#ifdef DEBUG
    uiBytesBefore = this->BytesLeftInQueue();
#endif
    
    //
    // Loop writing data into the spool
    //
    // If this job ISNT in the queue and our buffer is less than a quarter full always return 0 bytes written
    //
    //
    // Here is the buffer layout
    // [JOBS NOT ON QUEUE %25][Job In Queue 55%][Job In Queue Delay buffer   20%]
    //     the delay buffer is to help compensate for slow/jammed printers (and only given to the job thats activly printing)
    //
    //
    // NOTE: this will make 9x clients quit their job, but the problem is if we dont do it
    // jobs not on the queue will starve the job trying to print (causing deadlock)
    if(FALSE == (GetInternalStatus() & JOB_STATUS_PRINTING) && 
        m_QueuedData.BytesRemaining() * 4 <= m_QueuedData.TotalBufferSize()) {
        DEBUGMSG(ZONE_ERROR, (L"SMB_SRV: Buffer for job *NOT PRINTING* has gotten to %d of %d -- not writing!", m_QueuedData.BytesRemaining(), m_QueuedData.TotalBufferSize()));
        uiWrittenInPass = 0;
        hr = S_OK;
    } else {
        do {
            if(SUCCEEDED(hr = m_QueuedData.Write(pData, uiSize, &uiWrittenInPass))) {
                ASSERT(uiSize >= uiWrittenInPass);
                pData += uiWrittenInPass;
                uiSize -= uiWrittenInPass;
                *uiWritten = (*uiWritten) + uiWrittenInPass;  
                m_uiBytesWritten += uiWrittenInPass;
            } else {
                uiWrittenInPass = 0;
                hr = S_OK; //its okay if we didnt write,  this just means buffers were full
            }            
        } while(uiSize && uiWrittenInPass && SUCCEEDED(hr));
    }    
    Done:
        //
        // If we wrote data, wake up the reader
        if(0 != *uiWritten) {    
            TRACEMSG(ZONE_PRINTQUEUE, (L"SMBSRV-PRINTQUEUE: PRINTQUEUE -- writer() waking up reader() for JobID: %d", JobID()));
            TRACEMSG(ZONE_PRINTQUEUE, (L"   ...queue size is %d -- total written %d -- this pass %d\n", m_QueuedData.BytesReadyToRead(), m_uiBytesWritten, *uiWritten));
#ifdef PRINTQUEUE_BLOCK_WRITES            
            Sleep(PRINTQUEUE_MS_PER_KILOBYTE * (*uiWritten)  / 1024);            
#endif            
            SetEvent(m_WakeUpThereIsData);   
            
#ifdef DEBUG
        if(FALSE == fBypassCountCheck) {
            ASSERT(uiBytesBefore + *uiWritten == this->BytesLeftInQueue());
        }
#endif
        }   
        if(m_QueuedData.TotalBufferSize() - m_QueuedData.BytesReadyToRead() <= 1024*1024 ||
           (100 * m_QueuedData.BytesReadyToRead() / m_QueuedData.TotalBufferSize()) >= 80) {
                DEBUGMSG(ZONE_ERROR, (L"SMB_SRV: Buffer size has gotten to %d of %d -- delaying packets", m_QueuedData.BytesReadyToRead(), m_QueuedData.TotalBufferSize()));
                Cracker::g_dwDelayPacket = 1000;
        }        
        return hr;
}


USHORT 
PrintJob::JobID() {
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();
    return m_usJobID;
}

USHORT 
PrintJob::GetInternalStatus() {             
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();
    return m_usJobInternalStatus; 
}

USHORT 
PrintJob::GetStatus() { //this function converts status code to be good for network 
    USHORT Status = GetInternalStatus();
    USHORT usRetStatus = 0;
    
    
    if( Status & JOB_STATUS_PAUSED)
       usRetStatus |= PRJ_QS_PAUSED;
    if( Status & JOB_STATUS_PRINTING)
       usRetStatus |= PRJ_QS_PRINTING;
    else   
       usRetStatus |= PRJ_QS_SPOOLING;
       
    return usRetStatus;
}

UINT 
PrintJob::BytesLeftInQueue() {
    CCritSection csLock(&m_csMainLock);
    csLock.Lock();
    return m_QueuedData.BytesReadyToRead();
}

const WCHAR *
PrintJob::GetQueueName() {
       return m_QueueName.GetString();
}

const WCHAR *
PrintJob::GetComment() {           
    return m_Comment.GetString(); 
}

const WCHAR *
PrintJob::GetOwnerName() {
    return m_OwnerName.GetString();
}


Share *
PrintJob::MyShare() {
    return m_pMyShare;
}

VOID 
PrintJob::Release() {
    if(0 == InterlockedDecrement(&m_lRefCnt)) {   
        delete this;
    }   
#ifdef DEBUG
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_SRV: Release() for job %d to cnt %d", m_usJobID, m_lRefCnt));
#endif
}

VOID 
PrintJob::AddRef() {
    InterlockedIncrement(&m_lRefCnt);
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_SRV: AddRef() for job %d to cnt %d", m_usJobID, m_lRefCnt));
}

VOID 
PrintJob::ShutDown() {
    ASSERT(!m_fShutDown);
    m_fShutDown = TRUE;
    SetEvent(m_WakeUpThereIsData);
}
    
UINT 
PrintJob::TotalBytesWritten() {
    return m_uiBytesWritten;
}

 
ULONG
PrintJob::GetStartTime()
{   
    return m_ulStartTime;
}

DWORD PrintQueue::SMB_Com_Open_Spool(SMB_PACKET *pSMB, 
                                      SMB_PROCESS_CMD *_pRawRequest, 
                                      SMB_PROCESS_CMD *_pRawResponse, 
                                      UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST *pRequest =
            (SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_OPEN_PRINT_SPOOL_SERVER_RESPONSE *pResponse = 
            (SMB_OPEN_PRINT_SPOOL_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    
    StringTokenizer RequestTokenizer;  
    TIDState *pTIDState;
    SMBPrintQueue *pPrintQueue;    
    USHORT usQueueID;
    PrintJob *pNewJob = NULL;
    PrintStream *pPrintStream = NULL;
    ActiveConnection *pMyConnection = NULL;
    
    //
    // Find our connection state        
    if(NULL == (pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = SMB_ERR(ERRSRV, ERRerror);
       goto Done;    
    }    

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- not enough memory for request!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_OPEN_PRINT_SPOOL_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- not enough memory for response!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }        
    
    if(pRequest->SpoolMode != 1) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- we dont support TextMode"));
        ASSERT(FALSE);
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done; 
    }    
        
    //
    // Find a share state 
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, &pTIDState, 0)) || NULL == pTIDState)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- couldnt find share state!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;   
    }
      
    // 
    // Make sure this is actually a print queue
    if(NULL == pTIDState || 
       NULL == pTIDState->GetShare () ||
       NULL == pTIDState->GetShare()->GetPrintQueue()) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- the Tid is for a file -- we only do print!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
    //
    // Fetch the queue
    if(NULL == (pPrintQueue = pTIDState->GetShare()->GetPrintQueue())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- we didnt get a print queue back!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
        
    //
    // Create a new print job    
    if(NULL == (pNewJob = new PrintJob(pTIDState->GetShare()))) {
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }    
    
    //
    // Set our username
    if(FAILED(pNewJob->SetOwnerName(pMyConnection->UserName()))) {
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }
    
    //    
    // Create a new PrintStream and then add the job to TIDState
    if(NULL == (pPrintStream = new PrintStream(pNewJob,pTIDState))) {
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }   
    if(FAILED(pTIDState->AddFileStream(pPrintStream))) {
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    } 
    
     //
     // Init then fetch the file name from the request tokenizer
     RequestTokenizer.Reset((BYTE*)(pRequest+1), _pRawRequest->uiDataSize - sizeof(SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST));

     if(FALSE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) { 
        CHAR *pRequestedFile;
        if(FAILED(RequestTokenizer.GetString(&pRequestedFile)) || 0x04 != pRequestedFile[0]) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- error getting file name"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done; 
        }
        // the +1 on field is to advance beyond the string id
        if(FAILED(pNewJob->SetQueueName(StringConverter(pRequestedFile+1).GetString()))) {
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }        
     } else {
        WCHAR *pRequestedFile;
        if(FAILED(RequestTokenizer.GetUnicodeString(&pRequestedFile))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- error getting file name"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done; 
        }
        // the +1 on field is to advance beyond the string id
        if(FAILED(pNewJob->SetQueueName(pRequestedFile))) {
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }       
    }

    //
    // Set the job ID
    if(0xFFFF == (usQueueID = pNewJob->JobID())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"PRINT QUEUE ERROR!:  got an invalid job ID!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }
        
    //
    // Fill out return params and send back the data
    pResponse->ByteCount = 0;
    pResponse->FileID = usQueueID;
    pResponse->WordCount = (sizeof(SMB_OPEN_PRINT_SPOOL_SERVER_RESPONSE) - 1) / sizeof(WORD);
    *puiUsed = sizeof(SMB_OPEN_PRINT_SPOOL_SERVER_RESPONSE);
    
    Done:
        // 
        // Give back our handle
        if(pNewJob) {
            pNewJob->Release();
            pNewJob = 0;
        }    
        if(0 != dwRet) {            
            if(pPrintStream) {
                delete pPrintStream;
                pPrintStream = NULL;
            }
        }
        return dwRet;
}


DWORD PrintQueue::SMB_Com_Close_Spool(SMB_PROCESS_CMD *_pRawRequest, 
                                      SMB_PROCESS_CMD *_pRawResponse, 
                                      UINT *puiUsed,
                                      SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_CLOSE_PRINT_SPOOL_CLIENT_REQUEST *pRequest =
            (SMB_CLOSE_PRINT_SPOOL_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE *pResponse = 
            (SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    
    StringTokenizer RequestTokenizer;  
    TIDState *pTIDState; 
    ActiveConnection *pMyConnection = NULL;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close_Spool -- not enough memory for request!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close_Spool -- not enough memory for response!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }        
    
    //
    // Find our connection state        
    if(NULL == (pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = SMB_ERR(ERRSRV, ERRerror);
       goto Done;    
    }
    
    //
    // Find a TID state 
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, &pTIDState, 0)) || NULL == pTIDState)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close_Spool -- couldnt find share state!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;   
    }
    
    //
    // Close it up
    if(FAILED(pTIDState->Close(pRequest->FileID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close_Spool -- closing the job failed!!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;   
    }
    
    //
    // Remove the share from the TIDState (to prevent it from being aborted
    //   if the connection goes down)
    if(FAILED(pTIDState->RemoveFileStream(pRequest->FileID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- couldnt find filestream for FID(%d) to remove from share state!", pRequest->FileID));
        ASSERT(FALSE);
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
        
    //
    // Fill out return params and send back the data
    pResponse->ByteCount = 0; 
    pResponse->WordCount = 0;
    *puiUsed = sizeof(SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE);
    
    Done:
        return dwRet;
}


DWORD PrintQueue::SMB_Com_Write_Spool(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_WRITEX_CLIENT_REQUEST *pRequest =
            (SMB_WRITEX_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_WRITEX_SERVER_RESPONSE *pResponse = 
            (SMB_WRITEX_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    
    StringTokenizer RequestTokenizer;  
    TIDState *pTIDState;
    SMBPrintQueue *pPrintQueue = NULL;
    PrintJob   *pPrintJob = NULL;
    UINT uiWritten = 0;
    ActiveConnection *pMyConnection = NULL;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Write_Spool -- not enough memory for request!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Write_Spool -- not enough memory for response!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;
    }        
        
    //
    // Find our connection state        
    if(NULL == (pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = SMB_ERR(ERRSRV, ERRerror);
       goto Done;    
    }
    
    //
    // Find a share state 
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, &pTIDState, 0)) || NULL == pTIDState)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Write_Spool -- couldnt find share state!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;   
    }
           
    // 
    // Make sure this is actually a print queue
    if(NULL == pTIDState || 
       NULL == pTIDState->GetShare () ||
       NULL == pTIDState->GetShare()->GetPrintQueue()) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- the Tid is for a file -- we only do print!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
    //
    // Fetch the queue
    if(NULL == (pPrintQueue = pTIDState->GetShare()->GetPrintQueue())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Open_Spool -- we didnt get a print queue back!!"));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
    //
    // Find our print job
    if(FAILED(pPrintQueue->FindPrintJob(pRequest->FileID, &pPrintJob)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Write_Spool -- couldnt find print job for FID(%d)!", pRequest->FileID));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
    


    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: WriteSpool(FID=0x%x) -- byte=%d\n", pRequest->FileID, pRequest->IOBytes));
    if(FAILED(pPrintJob->Write((BYTE *)(pRequest+1), pRequest->IOBytes, pRequest->FileOffset, &uiWritten))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Write_Spool -- couldnt write %d bytes on FID!", pRequest->IOBytes, pRequest->FileID));
        dwRet = SMB_ERR(ERRSRV, ERRerror);
        goto Done;  
    }
    
    //
    // Fill out return params and send back the data
    pResponse->ByteCount = 0;
    pResponse->DataLength = uiWritten;
    // 3 because of WordCount and ByteCount 
    pResponse->WordCount = (sizeof(SMB_WRITEX_SERVER_RESPONSE) - 3) / sizeof(WORD);
    *puiUsed = sizeof(SMB_WRITEX_SERVER_RESPONSE);
    
    Done:
        if(NULL != pPrintJob) {
             pPrintJob->Release();
        }       
        return dwRet;
}

PrintStream::PrintStream(PrintJob *_pJob, 
                       TIDState *_pMyState) : SMBFileStream(PRINT_STREAM, _pMyState) 
{
    //
    // For a print stream, the FID is the same as the JobID (see the code
    //   for PrintJob to see that it uses the global FID list for its JobID)
    this->SetFID(_pJob->JobID());
    
    
    //
    // PrintJob is something thats ref counted (the print queue owns a copy
    //   too).  So Addref it.  it will be released in the deconstructor
    pPrintJob = _pJob;
    if(pPrintJob) 
        pPrintJob->AddRef();
}
PrintStream::~PrintStream()
{
    //
    // Destroy the print job if it hasnt already been aborted or hasnt finished
    if(pPrintJob && 
      (0 == (pPrintJob->GetInternalStatus() & (JOB_STATUS_FINISHED | JOB_STATUS_ABORTED)))) {
        PrintJob   *pFoundPrintJob = NULL;
        SMBPrintQueue *pPrintQueue = NULL;
        
        TRACEMSG(ZONE_ERROR, (L"remove printjob %d to TIDState:0x%x because of delete of holder\n", pPrintJob->JobID(), (UINT)this));

        if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(pPrintJob->JobID(), &pFoundPrintJob, &pPrintQueue)) ||
           NULL == pFoundPrintJob ||
           NULL == pPrintQueue) 
        {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: ~TIDState -- couldnt find print job %d, maybe the job was deleted?", pPrintJob->JobID()));              
            goto Done;
        }  
        ASSERT(pFoundPrintJob == pPrintJob);

        //
        // Set its abort logic
        //   NOTE: I dont set finished here because JobsFinished does
        //   and thats the proper way to get the FINISHED bit set
        pFoundPrintJob->SetInternalStatus(pFoundPrintJob->GetInternalStatus() | 
                                          JOB_STATUS_ABORTED);
        
        //
        // Kill off the job
        pFoundPrintJob->ShutDown();
        
        //
        // Remove it from the spool
        if(FAILED(pPrintQueue->JobsFinished(pFoundPrintJob))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: ~TIDState -- delete the print job from the queue FID(%d)!", pPrintJob->JobID()));
            ASSERT(FALSE);
        }
        
        if(NULL != pFoundPrintJob) {
            pPrintJob->Release();
        }
        
    }
    
    Done:
    //
    // Release our ref count on the job
    if(pPrintJob)
        pPrintJob->Release();   
}

HRESULT 
PrintStream::Read(BYTE *pDest,  DWORD dwOffset, DWORD dwReqSize, DWORD *pdwRead)
{
    ASSERT(FALSE);
    return E_NOTIMPL;
}

HRESULT 
PrintStream::Write(BYTE *pSrc,  DWORD dwOffset, DWORD dwReqSize, DWORD *pdwWritten)
{
    
    UINT uiWritten; 
    
    //
    // If the job hasnt been sent to the queue, send it now
    if(0 == (pPrintJob->GetInternalStatus() & JOB_STATUS_HAS_DATA)) {    
        SMBPrintQueue *pMyQueue = NULL;
        pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_HAS_DATA);
        
        if(pPrintJob->MyShare()) {
           pMyQueue = pPrintJob->MyShare()->GetPrintQueue();
           pMyQueue->JobReadyForPrint(pPrintJob); 
        } else {
            ASSERT(FALSE);
        }
    }       
    HRESULT hr = pPrintJob->Write(pSrc, dwReqSize, dwOffset, &uiWritten);
    *pdwWritten = uiWritten;
    return hr;
}


HRESULT 
PrintStream::Open(const WCHAR *pFileName,  
                  DWORD dwAccess, 
                  DWORD dwDisposition, 
                  DWORD dwAttributes,
                  DWORD dwShareMode,
                  DWORD *pdwActionTaken)
{
    HRESULT hr = E_FAIL;   
    SMBPrintQueue *pPrintQueue = NULL;
   
    //
    // If the request is for anything but NULL, fail it    
    if(NULL == pFileName || NULL != pFileName[0]) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX Printer -- error filename!"));
        hr = E_ACCESSDENIED;
        goto Done;   
    }   
    if(NULL == pPrintJob->MyShare() || NULL == (pPrintQueue = pPrintJob->MyShare()->GetPrintQueue())) {
        hr = E_ACCESSDENIED;
        goto Done;
    }
    if(FAILED(pPrintJob->SetQueueName(pFileName))) {
        hr = E_FAIL;
        goto Done;
    }
    
    hr = S_OK;
    
    //
    // if we were able to open the file, always say its 2 (2=file didnt exist and was created)
    if(NULL != pdwActionTaken) {
        *pdwActionTaken = 2;
    }
    Done:
        return hr;
}
   
   
HRESULT 
PrintStream::Close() 
{ 
    SMBPrintQueue *pPrintQueue = NULL;
    HRESULT hr = E_FAIL;
    
    //
    // We always should have a print job!!
    if(NULL == pPrintJob) {
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }
    
    //
    // Get the print queue for this guy
    if(NULL == pPrintJob->MyShare() || NULL == (pPrintQueue = pPrintJob->MyShare()->GetPrintQueue())) {
        hr = E_UNEXPECTED;
        goto Done;
    }
   
    //
    // Close the spool
    if(FAILED(pPrintQueue->JobsFinished(pPrintJob))){
        TRACEMSG(ZONE_ERROR, (L"SMB_PrintStream: Close() -- error setting Jobs Finished on print queue!"));
        hr = E_UNEXPECTED;
        goto Done; 
    }
    
    //nobody should set hr to success yet
    ASSERT(FAILED(hr));
    hr = S_OK;
    Done:
        return hr;
   
}


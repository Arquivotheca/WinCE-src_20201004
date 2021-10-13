//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef PRINTQUEUE_H
#define PRINTQUEUE_H

#include <list.hxx>

#include "SMB_Globals.h"
#include "CriticalSection.h"
#include "ShareInfo.h"

struct SMB_PACKET;
class Share;
class PrintJob;
class SMBFileStream;
struct SMB_PROCESS_CMD;


HRESULT StartPrintSpool();
HRESULT StopPrintSpool(); 

#define PRINTJOB_LIST_ALLOC              pool_allocator<10, PrintJob *>


//
// NOTE: if your adding states here make sure to update
//   SetInternalStatus to print them (to make debugging easier)
#define JOB_STATUS_UNKNOWN         0  //<-- use BITs here
#define JOB_STATUS_HAS_DATA        1
#define JOB_STATUS_FINISHED        2
#define JOB_STATUS_PRINTING        4
#define JOB_STATUS_ABORTED         8
#define JOB_STATUS_PAUSED         16
#define JOB_STATUS_IN_QUEUE       32
#define JOB_STATUS_PRINTED        64


#define PRJ_QS_QUEUED              0  //<-- use BITs here
#define PRJ_QS_PAUSED              1
#define PRJ_QS_SPOOLING            2
#define PRJ_QS_PRINTING            4

namespace PrintQueue {
    extern BOOL fIsRunning; 
    DWORD SMBSRV_SpoolThread(LPVOID nothing);  
    
    DWORD SMB_Com_Write_Spool(SMB_PACKET *pSMB,
                              SMB_PROCESS_CMD *_pRawRequest, 
                              SMB_PROCESS_CMD *_pRawResponse, 
                              UINT *puiUsed);
    DWORD SMB_Com_Close_Spool(SMB_PROCESS_CMD *_pRawRequest, 
                              SMB_PROCESS_CMD *_pRawResponse, 
                              UINT *puiUsed,
                              SMB_PACKET *pSMB);
    DWORD SMB_Com_Open_Spool(SMB_PACKET *pSMB, 
                             SMB_PROCESS_CMD *_pRawRequest, 
                             SMB_PROCESS_CMD *_pRawResponse, 
                             UINT *puiUsed);
}



//
// A PrintStream is the generic abstraction for a SMBFileStream.  It 
//   allows a printer to be treated more like a file -- it keeps much of the 
//   code generic as to if its printing to file or printer
class PrintStream : public SMBFileStream 
{
    public:        
        PrintStream(PrintJob *pJob, TIDState *pMyState);                  
        ~PrintStream();

        void * operator new(size_t size) { 
             return SMB_Globals::g_PrintJobAllocator.allocate(size);
        }
        void   operator delete(void *mem) {
            SMB_Globals::g_PrintJobAllocator.deallocate(mem);
        }   

        //
        // Functions required by SMBFileStream      
        HRESULT Read(BYTE *pDest, DWORD dwOffset, DWORD uiReqSize, DWORD *pdwRead);
        HRESULT Write(BYTE *pSrc,  DWORD dwOffset, DWORD uiReqSize, DWORD *pdwWritten);
        HRESULT Open(const WCHAR *pFileName, 
                     DWORD dwAccess = GENERIC_READ, 
                     DWORD dwDisposition = CREATE_ALWAYS,
                     DWORD dwAttributes = FILE_ATTRIBUTE_READONLY, 
                     DWORD dwShareMode = 0,
                     DWORD *pdwActionTaken = NULL);
        HRESULT Close();

        //
        // Functions specific to the PrintStream (not part of SMBFileStream)
        PrintJob *GetPrintJob() {return pPrintJob;}
    private:
        PrintJob *pPrintJob;
};


class PrintJob
{
    public:
        PrintJob(Share *pMyShare);   

        void * operator new(size_t size) { 
             return SMB_Globals::g_PrintStreamAllocator.allocate(size);
        }
        void   operator delete(void *mem) {
            SMB_Globals::g_PrintStreamAllocator.deallocate(mem);
        }   
    
        HRESULT Write(const BYTE *pData, UINT uiSize, ULONG ulOffset, UINT *puiWritten);   
        HRESULT Read(BYTE *pDest, UINT uiSize, UINT *pRead);      

        USHORT JobID();
        USHORT GetInternalStatus();
        USHORT GetStatus(); 

        ULONG  GetStartTime(); 

        const WCHAR *GetQueueName();
        const WCHAR *GetComment(); 
        const WCHAR *GetOwnerName();

        HRESULT SetComment(const WCHAR *pComment);
        HRESULT SetQueueName(const WCHAR *pName);
        HRESULT SetOwnerName(const WCHAR *pName);
        
        VOID SetInternalStatus(USHORT s);
        UINT BytesLeftInQueue();
        
        
        VOID Release();
        VOID AddRef(); 

        VOID ShutDown(); 
        UINT TotalBytesWritten(); 

        Share *MyShare();
    private:
        ~PrintJob(); //note: this is private because you must dec the ref cnt!
        HANDLE               m_WakeUpThereIsData;
        StringConverter      m_QueueName;
        StringConverter      m_OwnerName;
        StringConverter      m_Comment;
        USHORT               m_usJobID;
        USHORT               m_usJobInternalStatus;
        UINT                 m_uiBytesWritten;        
        CRITICAL_SECTION     m_csRingBuffer;        
        RingBuffer           m_QueuedData;
        LONG                 m_lRefCnt;
        BOOL                 m_fShutDown;

        CRITICAL_SECTION     m_csMainLock;
        Share               *m_pMyShare;
        ULONG                m_ulStartTime;
};

class SMBPrintQueue 
{
    public:
        SMBPrintQueue();
        ~SMBPrintQueue();

        VOID SetPriority(USHORT prio);
        VOID SetStartTime(USHORT sTime);
        VOID SetUntilTime(USHORT sUntil);
        VOID SetStatus(USHORT status);

        HRESULT SetDriverName(CHAR *_pName);    
        HRESULT SetName(CHAR *_pName); 
        HRESULT SetSepFile(CHAR *_pName);
        HRESULT SetPrProc(CHAR *_pName);
        HRESULT SetParams(CHAR *_pName);
        HRESULT SetComment(CHAR *_pName);
        HRESULT SetPrinters(CHAR *_pName);
        HRESULT SetDriverData(CHAR *_pName);
        HRESULT SetPortName(WCHAR *_pName);

        
        USHORT GetPriority();    
        USHORT GetStartTime();    
        USHORT GetUntilTime();     
        USHORT GetStatus(); 
        USHORT GetNumJobs();
        
        const CHAR *GetDriverName();    
        const CHAR *GetName(); 
        const CHAR *GetSepFile();
        const CHAR *GetPrProc();
        const CHAR *GetParams();
        const CHAR *GetComment();
        const CHAR *GetPrinters();
        const CHAR *GetDriverData();
        const WCHAR *GetPortName();
        
        //
        // Job manipulation
        HRESULT AddJobToSpool(PrintJob *pJob);
        HRESULT JobReadyForPrint(PrintJob *pJob);
        HRESULT FindPrintJob(USHORT Fid, PrintJob **pJob);
        HRESULT JobsFinished(PrintJob *pJob);
        HRESULT RemovePrintJob(PrintJob *pJob);

        //
        // Print thread functions
        BOOL ShuttingDown() { return fShutDown; }
        VOID SetShutDown(BOOL _fShutDown) { fShutDown = _fShutDown; }
        PrintJob *WaitForJobThatIsReady();
        PrintJob *GetJobNextJobThatIsReady();
        //VOID RemoveNextJobWithData();
        //HRESULT RemoveJobWithData(USHORT usFID);
        HRESULT GetJobsInQueue(ce::list<PrintJob *, PRINTJOB_LIST_ALLOC > *pJobList);
        
        VOID StopAllJobs();
        
    private:
        char *pName;
        USHORT Priority;
        USHORT StartTime;
        USHORT UntilTime;
        USHORT Pad1;
        char *pSepFile;
        char *pPrProc;
        char *pParams;
        char *pComment;
        USHORT Status;
        USHORT cJobs;
        char *pPrinters;
        char *pDriverName;
        char *pDriverData;
        BOOL  fShutDown;
        WCHAR *wPortName;

        //
        // PrintJobList contains all jobs that have not finished
        //    the job might be in PrintJobsWithData too... PrintJobsWithData
        //    are jobs that are ready to go to the printer
        ce::list<PrintJob *, PRINTJOB_LIST_ALLOC> PrintJobList; 
        CRITICAL_SECTION csJobListLock;      

        //
        // PrintJobsWithData is a list that contains all jobs that have 
        //    received data.  This list is what the user will be given
        //    when they query for what jobs are in the queue
        //
        // A job can be here and not in PrintJobList if we have absorbed the
        //    entire job into memory
        //ce::list<PrintJob *> PrintJobsWithData;
        //CRITICAL_SECTION csPrintJobsWithData;

        
        HANDLE ReadyToPrintSemaphore;
        HANDLE PrintThreadHandle;
};
#endif

//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name: 

        PIPEPARE.H

Abstract:

       USB Loopback driver -- class definitions for tranfer management
        
--*/

#ifndef __PIPEPAIR_H__
#define __PIPEPAIR_H__

#include <windows.h>
#include <usbfntypes.h>
#include "config.h"

// **************************************************************************
//                             Definitions & Variables
// **************************************************************************

//Transfer Descriptors to hold isoch/bulk/interrrupt data, and coordinate transfers, they act like 
//circular buffers, each pipepair (one in and one out) has an array of TDs

//the status for each transfer descriptor
#define	TD_STATUS_READABLE	1
#define	TD_STATUS_WRITABLE	2
#define	TD_STATUS_READING	3
#define	TD_STATUS_WRITING	4

//define transfer type
#define	TRANS_TYPE_CONTROL	1
#define	TRANS_TYPE_BULK		2
#define	TRANS_TYPE_ISOCH		3
#define	TRANS_TYPE_INT			4

#define QUEUE_DEPTH	32
#define QUEUE_SIZE	32

//predefined maxiam tranfer sizes for each type of transfer 
#define  	ISOCH_TRANSFER_SIZE		0x8000 // 128k
#define	BULK_TRANSFER_SIZE		0x8000 // 64k
#define	INT_TRANSFER_SIZE		0x8000 // 128k
#define   CTRL_TRANSFER_SIZE      0x40 


typedef struct _STRANSNOTI{
    LPVOID			pClass;	//pointer to the class instance
    UCHAR			uTransIndex;	//index to identify this buffer  
    UFN_TRANSFER        hTransfer;          //Transfer handle
}STRANSNOTI, *PSTRANSNOTI;


//class to manage a pipe pair, control all activities that occur on these two pipes
class CSinglePipe{
public:
    //public functions 
    CSinglePipe(IN UFN_HANDLE hDevice,
                            IN PUFN_FUNCTIONS   pUfnFuncs,
                            IN UCHAR  uQueueDepth,
                            IN UCHAR  uTransType, IN BOOL bIn)
                        :m_hDevice(hDevice), m_pUfnFuncs(pUfnFuncs),
                         m_uQueueDepth(uQueueDepth), m_uPipeType(uTransType), m_bIn(bIn){
    	
        m_dwTransLen = 0;
        m_usTransRounds = m_usTransLeft = 0;

	  //null those handles
        m_hTransThread = NULL;
        m_hTransCompleteThread = NULL;
        m_hTransCompleteEvent = NULL;
        m_hTaskReadyEvent = NULL;
        m_hCompleteEvent = NULL;
        m_hSemaphore = NULL;
	  m_hPipe = NULL;
        m_StartByte = 0;
	  
    };

    ~CSinglePipe(){Cleanup();};
    BOOL Init();
    VOID Cleanup();
    BOOL StartThreads();

    //pipe related functions
    PUFN_PIPE GetPipe(){return &m_hPipe;};
    VOID SetPipe(UFN_PIPE hPipe){ m_hPipe = hPipe;};
    DWORD ClosePipe();

    //transfer related functions	
    BOOL StartNewTransfers( ULONG ulTransLen, USHORT usTransNum, USHORT usAlignment, DWORD dwStartValue);

      //completion notify function
    static DWORD WINAPI CompleteCallback( LPVOID dParam);
    DWORD  CompleteNotify(UCHAR uIndex);
    VOID  ResetPipe();
    DWORD WaitPipeReady(DWORD	dwWaitTime);

	
private:

    //pipe transfer related threads
    static DWORD  WINAPI TransThreadProc( LPVOID dParam);
    DWORD  RunTransThreadProc( );
    static DWORD  WINAPI CompleteThreadProc( LPVOID dParam);
    DWORD  RunCompleteThreadProc( );
    VOID MakeInData(DWORD dwTotalLen, DWORD dwStartVal, USHORT usAlignment);

    UFN_HANDLE  m_hDevice;
    PUFN_FUNCTIONS  m_pUfnFuncs;

    //Pipes
    UFN_PIPE	m_hPipe;
    //type of pipe
    UCHAR	m_uPipeType;
    //pipe direction
    BOOL    m_bIn;
    
    //TD related variables
    UCHAR	m_uQueueDepth; //how many tranfers we can issue on one pipe at one time

    //host request related variables
    USHORT	m_usTransRounds;	//how many OUT transfers for one request from host
    DWORD	m_dwTransLen;	//transfer length
    USHORT	m_usTransLeft;   //
   
    //Threads handlers
    HANDLE	m_hTransThread;
    HANDLE	m_hTransCompleteThread;

    //threads exit flags
    BOOL    m_bThreadsExit;

    //sychronization related variables
    CRITICAL_SECTION	m_Transcs;    //critical section to sync IN transfers
    CRITICAL_SECTION	m_TransCompletioncs;	//critical section to sync IN completion activities

    HANDLE	m_hSemaphore;	//semaphore for IN pipe
    HANDLE	m_hTaskReadyEvent;	//event to notify there's new OUT request from host
    HANDLE	m_hTransCompleteEvent;	//event to notify the completion of an OUT transfer
    
    HANDLE	m_hCompleteEvent;		//event to notify the completion of all transfers 
    HANDLE     m_hAckReadyEvent;		//event to notify that pipes are all set for the next task

    //start data
    BYTE        m_StartByte;

    //data buffer
    PBYTE   m_pBuffer;

    //notifications
    PSTRANSNOTI  m_pTransNoti;
    UCHAR            m_uCurIndex;
    
    
};

typedef struct _TRANSNOTI{
    LPVOID			pClass;	//pointer to the class instance
    UCHAR			uTransIndex;	//index to identify this buffer  
}TRANSNOTI, *PTRANSNOTI;

typedef struct _TRANSDESCRIPTOR{
    PBYTE				pBuff;	//buffers
    DWORD			dwBytes;	//valid bytes of content in each buffer
    CHAR 				uStatus;	//status of each buffer	
    HANDLE			hReadEvent;	//event to coordinate read activities
    HANDLE			hWriteEvent;	//event to coordinate write acitivities
    UFN_TRANSFER       	hTransfer;  //transfer handle
    TRANSNOTI		TransNoti;	//notification paras  
    USHORT                 usLen;       //only used when transfer length is not a fixed value 
    //following members are only used if "pseduo" data is used in the uni-directional transfer
    BYTE                     StartValue;
}TRANSDESCRIPTOR, *PTRANSDESCRIPTOR;


//class to manage a pipe pair, control all activities that occur on these two pipes
class CPipePair{
public:
    //public functions 
    CPipePair( IN UFN_HANDLE hDevice,
                        IN PUFN_FUNCTIONS pUfnFuncs,
                        IN DWORD dwBufLen, 
                        IN UCHAR	uQueueSize,
                        IN UCHAR  uQueueDepth,
                        IN UCHAR  uTransType)
                        :m_hDevice(hDevice), m_pUfnFuncs(pUfnFuncs),
                        m_dwBufLen(dwBufLen), m_uQueueSize(uQueueSize), m_uQueueDepth(uQueueDepth), m_uPipeType(uTransType){
    	
        m_uNextRdIndex = m_uNextWrIndex = 0;
        m_dwTransLen = 0;
        m_uOutIndex = m_uInIndex = 0;
        m_usTransRounds = m_usTransLeft = 0;

	  //null those handles
        m_hOutThread = NULL;
        m_hInThread = NULL;
        m_hOutCompleteThread = NULL;
        m_hInCompleteThread = NULL;
        m_hOutCompleteEvent = NULL;
        m_hInCompleteEvent = NULL;
        m_hOutTaskReadyEvent = NULL;
        m_hInTaskReadyEvent = NULL;
        m_hCompleteEvent = NULL;
        m_hInSemaphore = NULL;
        m_hOutSemaphore = NULL;
	  m_hInPipe = NULL;
	  m_hOutPipe = NULL;
        m_bInFaked = FALSE;
        m_bOutFaked = FALSE;
        m_StartByte = 0;
	  
    };

    ~CPipePair(){Cleanup();};
    BOOL Init();
    VOID Cleanup();
    BOOL StartThreads();

    //pipe related functions
    PUFN_PIPE GetInPipe(){return &m_hInPipe;};
    PUFN_PIPE GetOutPipe(){return &m_hOutPipe;};
    VOID SetInPipe(UFN_PIPE hInPipe){ m_hInPipe = hInPipe;};
    VOID SetOutPipe(UFN_PIPE hOutPipe){ m_hOutPipe = hOutPipe;};
    DWORD CloseInPipe();
    DWORD CloseOutPipe();

    //transfer related functions	
    BOOL StartNewTransfers( ULONG ulTransLen, USHORT usTransNum);

      //completion notify function
    static DWORD WINAPI InCompleteCallback( LPVOID dParam);
    static DWORD WINAPI OutCompleteCallback( LPVOID dParam);
    DWORD  InCompleteNotify(UCHAR uIndex);
    DWORD  OutCompleteNotify(UCHAR uIndex);
    VOID  ResetPipes();
    DWORD WaitPipesReady(DWORD	dwWaitTime);

	
private:
    //TD related functions
    BOOL TD_Init();
    BOOL TD_WaitInTDReady(PBYTE* ppData,  PUCHAR puIndex);
    BOOL TD_WaitOutTDReady(PBYTE* ppData,  PUCHAR puIndex);
    VOID TD_OutTransferComplete(UCHAR uIndex, DWORD dwSize);
    VOID TD_InTransferComplete(UCHAR uIndex, DWORD dwSize);
    VOID TD_Cleanup();

    //pipe transfer related threads
    static DWORD  WINAPI InThreadProc( LPVOID dParam);
    DWORD  RunInThreadProc( );
    static DWORD  WINAPI OutThreadProc( LPVOID dParam);
    DWORD RunOutThreadProc();
    static DWORD  WINAPI InCompleteThreadProc( LPVOID dParam);
    DWORD  RunInCompleteThreadProc( );
    static DWORD  WINAPI OutCompleteThreadProc( LPVOID dParam);
    DWORD  RunOutCompleteThreadProc();

    UFN_HANDLE  m_hDevice;
    PUFN_FUNCTIONS  m_pUfnFuncs;
    //Pipes
    UFN_PIPE	m_hOutPipe;
    UFN_PIPE	m_hInPipe;

    //type of pipe
    UCHAR	m_uPipeType;
	
    //TD related variables
    UCHAR	m_uNextWrIndex;	//next TD to write
    UCHAR	m_uNextRdIndex;	//next TD to read
    DWORD	m_dwBufLen;	//TD buffer length
    UCHAR	m_uOutIndex;	//index for TD related with IN transfers
    UCHAR	m_uInIndex;	//index for TD related with OUT transfers
    UCHAR	m_uQueueSize;	//how many TDs we have
    UCHAR	m_uQueueDepth; //how many tranfers we can issue on one pipe at one time
    PTRANSDESCRIPTOR	m_pTDList;	//TD list 

    //host request related variables
    USHORT	m_usTransRounds;	//how many OUT transfers for one request from host
    DWORD	m_dwTransLen;	//transfer length
    USHORT	m_usTransLeft;   //
   
    //Threads handlers
    HANDLE	m_hOutThread;
    HANDLE	m_hInThread;
    HANDLE	m_hOutCompleteThread;
    HANDLE	m_hInCompleteThread;
    //threads exit flags
    BOOL    m_bThreadsExit;
    //completion functions related vars.
    UCHAR   m_uCurOutIndex;
    UCHAR   m_uCurInIndex;

    //sychronization related variables
    CRITICAL_SECTION	m_TDcs;    //critical section for TD access
    CRITICAL_SECTION	m_Incs;    //critical section to sync IN transfers
    CRITICAL_SECTION	m_Outcs;  //critical section to sync OUT transfers
    CRITICAL_SECTION	m_InCompletioncs;	//critical section to sync IN completion activities
    CRITICAL_SECTION	m_OutCompletioncs;	//critical section to sync OUT completion activities

    HANDLE	m_hInSemaphore;	//semaphore for IN pipe
    HANDLE	m_hOutSemaphore;	//semaphore for OUT pipe
    HANDLE	m_hOutTaskReadyEvent;	//event to notify there's new OUT request from host
    HANDLE	m_hInTaskReadyEvent;	//event to notify there's new IN reqeust 
    HANDLE	m_hOutCompleteEvent;	//event to notify the completion of an OUT transfer
    HANDLE	m_hInCompleteEvent;	//event to notify the completion of an IN transfer
    
    HANDLE	m_hCompleteEvent;		//event to notify the completion of all transfers 
    HANDLE     m_hAckReadyEvent;		//event to notify that pipes are all set for the next task

    //whether in/out data is faked data?
    BOOL        m_bInFaked;
    BOOL        m_bOutFaked;

    //start data
    BYTE        m_StartByte;
};

typedef CPipePair *   PCPipePair;
typedef CSinglePipe * PCSinglePipe;

typedef struct _PIPEPAIR_INFO{
    UCHAR bmAttribute;
    UCHAR bInEPAddress;
    UCHAR bOutEPAddress;
    USHORT wCurPacketSize;
    BOOL    bOpened;
}PIPEPAIR_INFO, *PPIPEPAIR_INFO;

typedef struct _SINGLEPIPE_INFO{
    UCHAR bmAttribute;
    UCHAR bEPAddress;
    USHORT wCurPacketSize;
    BOOL    bOpened;
}SINGLEPIPE_INFO, *PSINGLEPIPE_INFO;

class CPipePairManager{
public:
    CPipePairManager(IN CDeviceConfig* pDevConfig, UFN_HANDLE hDevice, PUFN_FUNCTIONS pFuncs){
        m_pDevConfig = pDevConfig;
        m_hDevice = hDevice;
        m_pUfnFuncs = pFuncs;
        m_pPipePairs = NULL;
        m_pHSConfig = NULL;
        m_pFSConfig = NULL;
        m_pPairsInfo = NULL;
        m_pSinglePipes = NULL;
        m_pSinglesInfo = NULL;
        m_uNumofPairs = 0;
        m_uNumofEPs = 0;
        m_uNumofSPipes = 0;
        m_curSpeed = BS_HIGH_SPEED;
        m_fInitialized = FALSE;
    };
    ~CPipePairManager(){CleanAllPipes();};

BOOL Init();
VOID SendDataLoopbackRequest(UCHAR uOutEP, USHORT wBlockLen, USHORT wNumofBlocks, USHORT usAlign, DWORD dwVal);
VOID SendShortStressRequest(UCHAR uOutEP, USHORT wNumofBlocks);
BOOL OpenAllPipes();
BOOL CleanAllPipes();
BOOL CloseAllPipes();
PPIPEPAIR_INFO GetPairsInfo(){return m_pPairsInfo;};
PSINGLEPIPE_INFO GetSinglePipesInfo() {return m_pSinglesInfo;};
VOID SetCurSpeed(UFN_BUS_SPEED speed){m_curSpeed = speed;};

private:

    CDeviceConfig* m_pDevConfig;
    UFN_HANDLE     m_hDevice;
    PUFN_FUNCTIONS   m_pUfnFuncs;
    
    CPipePair**        m_pPipePairs;
    CSinglePipe**     m_pSinglePipes;
    PUFN_CONFIGURATION  m_pHSConfig;
    PUFN_CONFIGURATION  m_pFSConfig;
    PPIPEPAIR_INFO  m_pPairsInfo;
    PSINGLEPIPE_INFO    m_pSinglesInfo;
    
    UCHAR   m_uNumofPairs;
    UCHAR   m_uNumofEPs;
    UCHAR   m_uNumofSPipes;
    UFN_BUS_SPEED   m_curSpeed;

    BOOL m_fInitialized;
};


// USB function object
extern PUFN_FUNCTIONS                       g_pUfnFuncs;
extern UFN_HANDLE                         g_hDevice;

//some debug output defines
#ifndef SHIP_BUILD
#define STR_MODULE _T("UFNLOOPBACK!")
#define SETFNAME(name) LPCTSTR pszFname = STR_MODULE name _T(":")
#else
#define SETFNAME(name)
#endif



#ifdef DEBUG
#define NKDBG               NKDbgPrintfW
#else
#define NKDBG               (VOID)
#endif

#endif


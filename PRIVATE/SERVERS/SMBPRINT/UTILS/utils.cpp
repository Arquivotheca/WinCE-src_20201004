//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <CReg.hxx>
#include <wincrypt.h>


#include "SMB_Globals.h"
#include "utils.h"
#include "encrypt.h"
#include "pc_net_prog.h"
#include "cracker.h"
#include "connectionmanager.h"
#include "smberrors.h"

#define INCREASE_BUF_UNIT   200

pool_allocator<10, RING_NODE>           g_RingNodeAllocator;


StringConverter::StringConverter()
{
    pMyString = NULL;
    uiLength = 0;
    uiBufLength = 0;
}

StringConverter::StringConverter(const CHAR *pString)
{
    pMyString = NULL;
    uiLength = 0;
    uiBufLength = 0;
    append(pString);
}

StringConverter::StringConverter(const StringConverter &cpy)
{
    pMyString = NULL;
    uiLength = 0;
    uiBufLength = 0;
    append(cpy.pMyString);
}


StringConverter::~StringConverter()
{
    Clear();
}

HRESULT
StringConverter::Clear() 
{
    if(pMyString != MyString) {        
        delete [] pMyString;
    } 
    
    pMyString = NULL;
    uiLength = 0;
    uiBufLength = 0;    
    return S_OK;
}

HRESULT 
StringConverter::append(const CHAR *pString, UINT uiChars)
{
   WCHAR Temp[1024];
   WCHAR *pPtr = Temp;
   
   //PERFPERF:  there could be a nice speed gain here by doing this in place... (to avoid 
   //   a memory copy... check out in profiler)
   UINT uiRequired = MultiByteToWideChar(CP_ACP, 0,pString,-1,0,0) + 1;  
   
   //
   // If we can fit this on the stack, super, else use the heap
   if(uiRequired > sizeof(Temp)/sizeof(Temp[0])) {
       pPtr  = new WCHAR[uiRequired];
       if(NULL == pPtr)
            return E_OUTOFMEMORY;
   }
        
   UINT uiConverted = MultiByteToWideChar(CP_ACP, 0, pString, -1, pPtr, uiRequired-1);
   ASSERT(uiRequired >= uiConverted);
   HRESULT hr = append(pPtr, uiChars);
   if(pPtr != Temp) {
        delete [] pPtr;
   }     
   return hr;
}




HRESULT 
StringConverter::append(const WCHAR *pString, UINT uiChars)
{
    HRESULT hr = S_OK;
    UINT uiSize;
    
    if(NULL == pString) {
        return E_INVALIDARG;
    }
    
    //
    // Make sure we have some memory
    if(NULL == pMyString) {
        pMyString = MyString;
        pMyString[0] = NULL;
        uiLength = 1;
        uiBufLength = sizeof(MyString);
    }   

    ASSERT(uiLength >= 1 && uiBufLength >= 1);
    
    if(0xFFFFFFFF == uiChars) {
        uiSize = wcslen(pString);
    } else {
        uiSize = uiChars;
    }
    
    
    //
    // If we can fit in place WHOO HOO!  do so -- otherwise go to the heap
    if(uiBufLength - uiLength >= (uiSize+1)) {
        //-1 is to compensate for NULL 
        memmove(&pMyString[uiLength-1], pString, sizeof(WCHAR) * uiSize);   
        uiLength += uiSize;
        pMyString[uiLength-1] = NULL;
    }    
    else { 
        WCHAR *pTemp;        
        if(NULL == (pTemp = new WCHAR[Size() + (uiSize * sizeof(WCHAR)) + INCREASE_BUF_UNIT])) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }
        ASSERT(NULL != pMyString);
        memmove(pTemp, pMyString, uiLength * sizeof(WCHAR));
        
        //-1 is to compensate for NULL
        memmove(&pTemp[uiLength-1], pString, sizeof(WCHAR) * uiSize);
        uiLength += uiSize;
        pTemp[uiLength-1] = NULL;        
        uiBufLength += ((uiLength * sizeof(WCHAR)) + INCREASE_BUF_UNIT);        
        delete [] pMyString;
        pMyString = pTemp;    
    }
    
    hr = S_OK;
    Done:
        return hr;
}

const WCHAR *
StringConverter::GetString() 
{
    return pMyString;
}

WCHAR *
StringConverter::GetUnsafeString()
{
    return pMyString;
}

UINT
StringConverter::Size()
{
    return uiLength * sizeof(WCHAR);
}
UINT
StringConverter::Length()
{
    return uiLength;
}
     
//
// Return a blob of memory that is of type STRING (it may or may not be 
//   UNICODE depending on what was negotiated) -- you MUST FREE it with
//   LocalFree
BYTE *
StringConverter::NewSTRING(UINT *_puiSize, BOOL fUnicode)
{
    *_puiSize = 0;
    BYTE *pRet = NULL;
    
    if(FALSE == fUnicode) {
        UINT uiSize; 
        if(0 != (uiSize = WideCharToMultiByte(CP_ACP, 0, pMyString, -1, NULL, 0,NULL,NULL))) 
        { 
            if(NULL != (pRet = (BYTE*)LocalAlloc(LMEM_FIXED, uiSize))) {            
                if(0 != WideCharToMultiByte(CP_ACP, 0, pMyString, -1, (CHAR *)pRet, uiSize, NULL, NULL)) {
                    *_puiSize = uiSize;
                } else if(pRet) {
                    pRet = NULL;
                }   
            }    
        } 
    } else {
        pRet = (BYTE *)LocalAlloc(LMEM_FIXED, Size());
        if(NULL != pRet) {
            (*_puiSize) = Size();
            memcpy(pRet, pMyString, Size());
        }
    }
    return pRet;
}
       
       
       
       
UniqueID::UniqueID() :usNext(0)
{
#ifdef DEBUG
    usNext = 0xFFF0;
#endif  
}
UniqueID::~UniqueID()
{
    IFDBG(if(IDList.size()){TRACEMSG(ZONE_ERROR, (L"SMB_SVR: ~UniqueID() still have %d objects outstanding!  maybe something is leaking!",IDList.size()));})
    ASSERT(0 == IDList.size());
}
        
HRESULT 
UniqueID::GetID(USHORT *uiID)
{
    ce::list<USHORT, UNIQUEID_ID_ALLOC >::iterator it = IDList.begin();
    ce::list<USHORT, UNIQUEID_ID_ALLOC >::iterator itEnd = IDList.end();   
  
    usNext ++;
    
    BOOL fFound = FALSE;
    BOOL fChanged = FALSE;
    
    //
    //  if the size of the list is 0xFFFE (0xFFFF is reserved)
    //     we dont have any more spaces... abort
    if(0xFFFE == IDList.size()) {
       ASSERT(FALSE);
       return E_FAIL;
    } 
   
    while(it != itEnd) {
        if(usNext == *it) {
            fChanged = TRUE;
            usNext++;
        }
        else if(*it > usNext) {
            IDList.insert(it, usNext);
            fFound = TRUE;
            break;
        }
        it++;
   }
   
   if(!fFound && !fChanged) {
        if(0xFFFF != usNext) {
            IDList.push_back(usNext);
        }
        fFound = TRUE;       
   }
   
   if(fFound) {       
       //
       // Reserve 0xFF as an invalid ID
       if(0xFFFF == usNext) {
           return this->GetID(uiID);
       } else {
           *uiID = usNext;
           return S_OK;
       }
   } else {
       return E_FAIL;
   }
    
}
HRESULT UniqueID::RemoveID(USHORT uiID)
{
    ce::list<USHORT, UNIQUEID_ID_ALLOC>::iterator it = IDList.begin();
    ce::list<USHORT, UNIQUEID_ID_ALLOC>::iterator itEnd = IDList.end();  
    BOOL fFound = FALSE;
     
    while(it != itEnd) {
        if(uiID == *it)  {
            IDList.erase(it++);
            fFound = TRUE;
            break;
        }
        it++;
    }
    
    if(fFound)
        return S_OK;
    else {
        ASSERT(FALSE);
        return E_FAIL;
    }
}









RingBuffer::RingBuffer() 
{
    ASSERT(0 == m_BufferList.size());  
    m_uiReadyToRead = 0;
    InitializeCriticalSection(&m_myLock);
}

RingBuffer::~RingBuffer() 
{ 
    Purge();
    DeleteCriticalSection(&m_myLock);
}

HRESULT 
RingBuffer::Purge() {
    CCritSection csLock(&m_myLock);
    csLock.Lock();
    
    //
    // Clean out any buffers we may have
    while(m_BufferList.size()) {   
        RING_NODE *pNode = m_BufferList.front();        
        m_BufferList.pop_front();
        SMB_Globals::g_PrinterMemMapBuffers.Return(pNode->m_pHead);
        delete pNode;
    }   

    return S_OK;
}

HRESULT
RingBuffer::Read(BYTE *pDest, UINT uiRequested, UINT *puiReturned)
{
    EnterCriticalSection(&m_myLock);
    HRESULT hr = E_FAIL;
    
    *puiReturned = 0;
    
    RING_NODE *pLastNode = NULL;
    
#ifdef DEBUG
    {
        //
        // Verify we have the correct amount of memory accounted for
        UINT uiVerify = 0;
        ce::list<RING_NODE *, RING_LIST_NODE_ALLOC >::iterator it = m_BufferList.begin();
        ce::list<RING_NODE *, RING_LIST_NODE_ALLOC >::iterator itEnd = m_BufferList.end();   
        while(it != itEnd) {
            uiVerify += (*it)->m_uiReadyToRead;
            it++;
        }
        ASSERT(uiVerify == m_uiReadyToRead);
    }
#endif

    if(0 == m_BufferList.size()) {
        hr = S_OK;
        goto Done;
    }

    pLastNode = m_BufferList.back();

    //
    //  If there is data remaining on the current block
    //     read it now
    if(pLastNode->m_uiReadyToRead > 0) {
        UINT uiToRead = uiRequested;
        if(uiRequested > pLastNode->m_uiReadyToRead) {
            uiToRead = pLastNode->m_uiReadyToRead;
        }        
        //
        // Do the move
        __try {
            memcpy(pDest, pLastNode->m_pReadBuffer, uiToRead);
        } __except(1) {
               TRACEMSG(ZONE_ERROR, (L"SMB_SRV: RingBuffer caught exception!! this is BAD"));        
               ASSERT(FALSE);
               hr = E_FAIL;
               goto Done;
        }
        //
        // Update structures
        ASSERT(pLastNode->m_uiReadyToRead >= uiToRead);
        ASSERT(m_uiReadyToRead >= uiToRead);
        ASSERT((BYTE*)pLastNode->m_pHead + pLastNode->m_uiBlockSize + 1 >= pLastNode->m_pReadBuffer + uiToRead);
        pLastNode->m_uiReadyToRead -= uiToRead;       
        pLastNode->m_pReadBuffer += uiToRead;
        m_uiReadyToRead -= uiToRead;
        
        //
        // If nothing is left in this buffer and its done being written to, 
        //   return it to the buffer pool
        if(0 == pLastNode->m_uiWriteRemaining &&
           0 == pLastNode->m_uiReadyToRead) {
           m_BufferList.pop_back();
           SMB_Globals::g_PrinterMemMapBuffers.Return(pLastNode->m_pHead);            
           //TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_SRV: Ring buffer using -%d nodes", m_BufferList.size()));           
           delete pLastNode;
        }
        *puiReturned = uiToRead;
    }    
   
    //
    // Success
    hr = S_OK;
    
    Done:
        LeaveCriticalSection(&m_myLock);
        return hr;
}

HRESULT
RingBuffer::Write(const BYTE *pSrc, UINT uiSize, UINT *puiWritten)
{    
    EnterCriticalSection(&m_myLock);
    HRESULT hr = E_FAIL;
    UINT uiToWrite;
    RING_NODE *pFirstNode = NULL;
    BOOL fNewNodeNeeded = FALSE;
    
    *puiWritten = 0;
    
#ifdef DEBUG
    {
        //
        // Verify we have the correct amount of memory accounted for
        UINT uiVerify = 0;
        ce::list<RING_NODE *, RING_LIST_NODE_ALLOC >::iterator it = m_BufferList.begin();
        ce::list<RING_NODE *, RING_LIST_NODE_ALLOC >::iterator itEnd = m_BufferList.end();   
        while(it != itEnd) {
            uiVerify += (*it)->m_uiReadyToRead;
            it ++;
        }
        ASSERT(uiVerify == m_uiReadyToRead);
    }
#endif
    
    
    //
    // If we dont have any buffers in our list or the first one is empty
    //    create  a new one
    if(0 == m_BufferList.size()) {
        fNewNodeNeeded = TRUE;
    } else {
        pFirstNode = m_BufferList.front();
        if(0 == pFirstNode->m_uiWriteRemaining) {
            fNewNodeNeeded = TRUE;
        }
    }
   
    //
    //  If we need a new node, do the creation
    if(TRUE == fNewNodeNeeded) {
        RING_NODE *pNew = new RING_NODE();
        if(NULL == pNew) {
            goto Done;
        }
        pNew->m_pHead = SMB_Globals::g_PrinterMemMapBuffers.Create(&pNew->m_uiBlockSize);
        
        if(NULL == pNew->m_pHead) {
            TRACEMSG(ZONE_ERROR, (L"SMB_SRV: RingBuffer cant get new memory block! -- this is okay but we cant continue"));
            delete pNew;
            hr = S_OK;
            goto Done;
        }
        pNew->m_pReadBuffer = pNew->m_pWriteBuffer = (BYTE *)pNew->m_pHead;
        pNew->m_uiWriteRemaining = pNew->m_uiBlockSize;
        pNew->m_uiReadyToRead = 0;  
        
        m_BufferList.push_front(pNew);
        TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_SRV: Ring buffer using +(%d) nodes", m_BufferList.size()));
    }

    pFirstNode = m_BufferList.front();
    ASSERT(pFirstNode->m_uiWriteRemaining >= 0);

    //
    //  If there is data remaining on the current block
    //     read it now    
    uiToWrite = uiSize;
    if(uiToWrite > pFirstNode->m_uiWriteRemaining) {
        uiToWrite = pFirstNode->m_uiWriteRemaining;
    }        
    //
    // Do the move (note exception handler because this is from 
    //    memmap file
    __try {
        memcpy(pFirstNode->m_pWriteBuffer, pSrc, uiToWrite);
    } __except(1) {
        TRACEMSG(ZONE_ERROR, (L"SMB_SRV: RingBuffer caught exception!! this is BAD"));    
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    //
    // Update structures
    m_uiReadyToRead += uiToWrite;
    pFirstNode->m_uiReadyToRead += uiToWrite;
    pFirstNode->m_uiWriteRemaining -= uiToWrite;
    pFirstNode->m_pWriteBuffer += uiToWrite;    
    *puiWritten = uiToWrite;     
    ASSERT(uiSize >= uiToWrite);
    
    //
    // Success
    hr = S_OK;
    
    Done:
        LeaveCriticalSection(&m_myLock);
        return hr;
}

UINT
RingBuffer::BytesReadyToRead()
{
    CCritSection csLock(&m_myLock);
    csLock.Lock();
    return m_uiReadyToRead;
}

//
// Return the # of seconds since Jan1 1970 at 0:00:00
unsigned int SecSinceJan1970_0_0_0 (void) {
    SYSTEMTIME st;
    GetSystemTime (&st);

    LARGE_INTEGER ft;
    SystemTimeToFileTime (&st, (FILETIME *)&ft);

    ft.QuadPart -= 0x019db1ded53e8000;
    ft.QuadPart /= 10000000;

    return ft.LowPart;
}
 
 
unsigned int SecSinceJan1970_0_0_0 (FILETIME *_ft) {
    LARGE_INTEGER ft; //<-- this copying is done b/c we might not be aligned on 8 bytes!
    memcpy(&ft, _ft, sizeof(LARGE_INTEGER));
    ASSERT(sizeof(FILETIME) == sizeof(LARGE_INTEGER));
    ft.QuadPart -= 0x019db1ded53e8000;
    ft.QuadPart /= 10000000;
    return ft.LowPart;
}
 
BOOL FileTimeFromSecSinceJan1970(UINT uiSec, FILETIME *_ft) {
    LARGE_INTEGER ft;   
    ft.QuadPart = uiSec;
    ft.QuadPart *= 10000000;
    ft.QuadPart += 0x019db1ded53e8000;
    memcpy(_ft, &ft, sizeof(LARGE_INTEGER));
    return 1;
}
MemMappedBuffer::MemMappedBuffer():
                                 m_hFile(INVALID_HANDLE_VALUE),
                                 m_hMapping(INVALID_HANDLE_VALUE),
                                 m_pNext(NULL),
                                 m_uiMaxSize(0),
                                 m_uiInc(0),
                                 m_uiBlocksOutstanding(0),
                                 m_uiUsed(0),
                                 m_fInited(FALSE)
{
    InitializeCriticalSection(&myLock);
}

MemMappedBuffer::~MemMappedBuffer()
{    
    ASSERT(NULL == m_pNext);
    ASSERT(0 == m_uiBlocksOutstanding);
    ASSERT(0 == m_uiUsed);
    
    TRACEMSG(ZONE_PRINTQUEUE, (L"SMB_SRV:  MemMappedBuffer going down\n"));
    TRACEMSG(ZONE_PRINTQUEUE, (L"          %d blocks outstanding", m_uiBlocksOutstanding));
    TRACEMSG(ZONE_PRINTQUEUE, (L"          %d bytes used\n", m_uiUsed));
    
    
    //
    // Close the file mapping
    if(INVALID_HANDLE_VALUE != m_hMapping) {
        CloseHandle(m_hMapping);
    }
    
    //
    // If the file is open close up shop now
    if(INVALID_HANDLE_VALUE != m_hFile) {
        CloseHandle(m_hFile);
    }
    
    DeleteCriticalSection(&myLock);
}

HRESULT
MemMappedBuffer::Close() 
{   
    CCritSection csLock(&myLock);
    csLock.Lock();
    
    ASSERT(NULL == m_pNext);
    ASSERT(0 == m_uiBlocksOutstanding);
    ASSERT(0 == m_uiUsed);        
    
    if(INVALID_HANDLE_VALUE != m_hMapping) {
        CloseHandle(m_hMapping);
    }
    if(INVALID_HANDLE_VALUE != m_hFile) {
        CloseHandle(m_hFile);
    }
    
    m_hFile = INVALID_HANDLE_VALUE;
    m_hMapping =INVALID_HANDLE_VALUE;
    m_pNext = NULL;
    m_uiMaxSize = 0;
    m_uiInc = 0;
    m_uiBlocksOutstanding = 0;
    m_uiUsed = 0;
    m_fInited = FALSE;

    //
    // Success
    return S_OK;
}
HRESULT
MemMappedBuffer::Open(WCHAR *pFile, UINT _uiMaxSize, UINT _uiInc)
{
    EnterCriticalSection(&myLock);   
    HRESULT hr = E_FAIL;
    ASSERT(FALSE == m_fInited);
    
    //
    // If we are in the incorrect state, assert and return unexpected
    if(INVALID_HANDLE_VALUE != m_hFile ||
       INVALID_HANDLE_VALUE != m_hMapping ||
       NULL != m_pNext ||
       0 != m_uiUsed) {
       TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Error Open called twice?"));
       ASSERT(FALSE);
       LeaveCriticalSection(&myLock);
       return E_UNEXPECTED;
    }
    
    m_uiMaxSize = _uiMaxSize;
    m_uiInc = _uiInc;
        
    //
    // Call CreateFileForMapping to open up our file
    m_hFile = CreateFileForMapping(pFile, 
                                   GENERIC_READ | GENERIC_WRITE,
                                   0,
                                   NULL, 
                                   CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, 
                                   NULL);
                                   
    if(INVALID_HANDLE_VALUE == m_hFile) {
       TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Error -- cant open file!: 0x%x", GetLastError()));
       hr = E_UNEXPECTED;
       goto Done;
    }
        
    //
    // Create an unnamed file mapping
    m_hMapping = CreateFileMapping(m_hFile,
                                   NULL,
                                   PAGE_READWRITE, 
                                   NULL,
                                   m_uiMaxSize, 
                                   NULL);
    if(INVALID_HANDLE_VALUE == m_hMapping) {       
       TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Error -- CreateFileMapping failed with 0x%x!", GetLastError()));       
       ASSERT(FALSE);
       hr = E_UNEXPECTED;
       goto Done;
    }  
    
#ifdef DEBUG
    {
    VOID *pRet;
    //
    // Make sure we can actually map the entire contents of the file before
    //   going on.  This is just for debugging because we handle the case 
    //   where it CANT be done later -- but in order for testing to be accurate
    //   we want to acutally have the correct buffer spaces
    pRet = MapViewOfFile(m_hMapping,
                        FILE_MAP_READ | FILE_MAP_WRITE,
                        0,
                        0,
                        m_uiMaxSize);
    
    if(NULL != pRet) {
        __try {
            memset(pRet, 0, m_uiMaxSize);
        } __except(1) {            
            TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Error -- memset on mapping test THREW!"));   
            ASSERT(FALSE);            
        }
        UnmapViewOfFile(pRet);     
    } else {        
        TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Error -- cant map view of file! 0x%x!", GetLastError()));              
        ASSERT(FALSE);
    }
    }
#endif
    
    //
    // At this point we dont have anything memory mapped -- this is to prevent
    //   us from using too much space
    //   
    hr = S_OK;
    
    Done:
        if(FAILED(hr)) {
            //
            // Close the file mapping
            if(INVALID_HANDLE_VALUE != m_hMapping) {
                CloseHandle(m_hMapping);
            }
            
            //
            // If the file is open close up shop now
            if(INVALID_HANDLE_VALUE != m_hFile) {
                CloseHandle(m_hFile);
            }
            
            //
            // Get back into steady state
            m_hMapping = INVALID_HANDLE_VALUE;
            m_hFile = INVALID_HANDLE_VALUE;  
            m_uiMaxSize = 0;
            m_uiInc = 0;
            m_pNext=NULL;
            m_uiUsed=0;
        } else {
            m_uiMaxSize = _uiMaxSize;
            m_uiInc = _uiInc;
            m_fInited = TRUE;
        }
        LeaveCriticalSection(&myLock);
        return hr;
}


VOID *
MemMappedBuffer::Create(UINT *pBlockSize)
{
    EnterCriticalSection(&myLock);
    VOID *pRet = NULL; 
    *pBlockSize = 0;
    
    //
    // If we havent been inited error out
    if(FALSE == m_fInited) {
        ASSERT(FALSE);
        goto Done;
    }
        
    //
    // First thing, see if we have any memory laying around that we can
    //   give out
    if(NULL != m_pNext) {
        //TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: Got a free block to give back -- no work required."));
        pRet = m_pNext;
        m_pNext = (VOID *)(*(UINT *)m_pNext);
        goto Done;
    }
          
    //
    // If there isnt a free block, see if we can map a new one on the tail end of the file
    if(m_uiMaxSize - m_uiUsed >= m_uiInc) {
            
        //
        // Do the file mapping
        pRet = MapViewOfFile(m_hMapping,
                             FILE_MAP_READ | FILE_MAP_WRITE,
                             0,
                             m_uiUsed,
                             m_uiInc);
                             
        if(NULL == pRet) {            
            TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: Couldnt map a file view... this is bad and is an internal error: %d", GetLastError()));
            ASSERT(FALSE);
            goto Done;
        } else {
            //TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: Got file mapping at address 0x%x for %d bytes", (UINT)pRet, m_uiInc));

            //
            // Touch the memory just to verify that its okay
            __try {
                memset(pRet, 0, m_uiInc);
            } __except(1) {                
                TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: touching memory failed: %d used -- inc %d", m_uiUsed, m_uiInc));                
                ASSERT(FALSE);
                pRet = NULL;
                goto Done;
            }           
        }
        m_uiUsed += m_uiInc;
    }
    
    Done:
        //
        // If we succeed, inc the outstanding count
        if(pRet) {
            m_uiBlocksOutstanding ++;
            *pBlockSize = m_uiInc;
        } else {           
           TRACEMSG(ZONE_ERROR, (L"SMB-MEMMAP: Out of memory in spool swap file. failing request"));           
        }
        
        ASSERT((m_uiBlocksOutstanding * m_uiInc) <= m_uiMaxSize);
   
        LeaveCriticalSection(&myLock);
        return pRet;
}

VOID 
MemMappedBuffer::Return(VOID *pRet)
{
    CCritSection csLock(&myLock);
    csLock.Lock();
    
    //
    // If we havent been inited we shouldnd be called!  error out!
    if(FALSE == m_fInited) {
        ASSERT(FALSE);
        goto Done;
    }
    
    //
    // Handle the base case of returning (rechaining)
    *((UINT *)pRet) = (UINT)m_pNext;
    m_pNext = pRet;
    m_uiBlocksOutstanding --;
    
    //
    // If there are no blocks outstanding, we need to truncate the file
    if(0 == m_uiBlocksOutstanding) {
        VOID *pToFree = m_pNext;
        
        //
        // Unmap everything
        while(pToFree) {
            VOID *pNext = (VOID *)(*(UINT *)pToFree);
            
            //
            // Unmap the file view for this one node
            if(0 == UnmapViewOfFile(pToFree)) {                
                TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: Couldnt unmap a file view(0x%x)... this is bad and is an internal error: %d", (UINT)pToFree, GetLastError()));
                ASSERT(FALSE);               
            } else {
                TRACEMSG(ZONE_PRINTQUEUE, (L"SMB-MEMMAP: unmapped a file view(0x%x)...", (UINT)pToFree));
            }
            
            //
            // Get the next pointer
            pToFree = pNext;
        }   
        m_pNext = NULL;
        
        //
        // Set the end of the file to be the beginning.
        /*if(0xFFFFFFFF == ::SetFilePointer(m_hFile, 0, NULL, FILE_BEGIN)) {
            if(ERROR_SUCCESS != GetLastError()) {
                RETAILMSG(1, (L"SMB-MEMMAP: Setting file pointer failed while truncating file-- GetLastError(%d)", GetLastError()));        
                ASSERT(FALSE);
                goto Done;
            }
        }
        
        //
        // Do the set file operation to truncate
        if(0 == SetEndOfFile(m_hFile)) {
            RETAILMSG(1, (L"SMB-MEMMAP: Setting end of file failed while truncating file -- GetLastError(%d)", GetLastError()));           
            ASSERT(FALSE);
            goto Done;
        }*/
        
        m_uiUsed = 0;       
        ASSERT(NULL == m_pNext);
    }
    Done:
    ;
}


BOOL VerifyPassword(ActiveConnection *pMyConnection, BYTE *pBlob, UINT uiBlobLen)
{     
    char p21[21];
    char p24[24];
    
    CReg RegPassword;
    WCHAR wcPassPath[MAX_PATH];
    DWORD dwRegType;
    DWORD dwSizeNeeded = 0;
    BYTE TempBlob[1024];
    BYTE *pBlob2 = TempBlob;
    WCHAR *pCryptString = NULL;
    BYTE *pChallenge = NULL;
    
    DATA_BLOB dataIn = {0, NULL};
    DATA_BLOB dataOut = {0, NULL};
    
    BOOL fAllowAccess = FALSE;
    
    if(sizeof(p24) != uiBlobLen) {
       TRACEMSG(ZONE_SECURITY, (L"SMBSRV Verify password: Error -- invalid password len given (len %d)!!", uiBlobLen)); 
       goto Done;
    }

    swprintf(wcPassPath, L"Comm\\Security\\UserAccounts\\%s", pMyConnection->UserName());
    
    if(FALSE == RegPassword.Open(HKEY_LOCAL_MACHINE, wcPassPath)) {
        RETAILMSG(1, (L"SMBSRV Verify password: Error -- invalid user name %s!!", pMyConnection->UserName())); 
        goto Done;
    }
    
    //
    // See how much data we need for the blob
    if(ERROR_SUCCESS != RegQueryValueEx(RegPassword, L"LM", NULL, &dwRegType, NULL, &dwSizeNeeded)) {
        RETAILMSG(1, (L"SMBSRV Verify password: Error -- invalid user name %s!!", pMyConnection->UserName())); 
        goto Done;
    }
    
    //
    // If we need more than we've got, request it
    if(dwSizeNeeded > sizeof(TempBlob)) {
        pBlob2 = new BYTE[sizeof(TempBlob)];
        if(NULL == pBlob2) {
            goto Done;
        }
    }
    
    //
    // Fetch our blob
    if(ERROR_SUCCESS != RegQueryValueEx(RegPassword, L"LM", NULL, &dwRegType, pBlob2, &dwSizeNeeded)) {
        RETAILMSG(1, (L"SMBSRV Verify password: Error -- invalid user name %s!!", pMyConnection->UserName())); 
        goto Done;
    }    

    //
    // Decrypt it
    dataIn.cbData = dwSizeNeeded;
    dataIn.pbData = pBlob2;
    if(FALSE == CryptUnprotectData(&dataIn, &pCryptString, NULL, NULL, NULL, 0, &dataOut)) {
        RETAILMSG(1, (L"SMBSRV Verify password: Error, cant unprotect hash -- invalid user name %s!!", pMyConnection->UserName())); 
        goto Done;
    }    
    
    //
    // Verify the blobs are okay
    if(0 != _wcsicmp(pCryptString, pMyConnection->UserName())) {
        RETAILMSG(1, (L"SMBSRV Verify password: Error, cant protected names differ! %s %s!!", pMyConnection->UserName(), pCryptString)); 
        goto Done;
    }
    
    //
    // Fetch our challenge from the connection manager
    if(FAILED(SMB_Globals::g_pConnectionManager->FindChallenge(pMyConnection->ConnectionID(), &pChallenge))) {
        TRACEMSG(ZONE_SECURITY, (L"SMBSRV couldnt find challeinge for our connection! CID: %d!!", pMyConnection->ConnectionID())); 
        ASSERT(FALSE);
        goto Done;
    }    
    
    //
    // Verify the security here.  -- make sure their password aligns with 
    //   what we expect       
    memset(p21, 0, sizeof(p21));
    memcpy(p21, dataOut.pbData, dataOut.cbData);
    Encrypt(p21, (CHAR *)pChallenge, p24, sizeof(p24));
    
    if(0 == memcmp(p24, pBlob, sizeof(p24))) {
        fAllowAccess = TRUE;
    }   
    
    Done:
        if(pBlob2 != TempBlob) {
            delete [] pBlob2;
        }
        if(NULL != pCryptString) {
            LocalFree(pCryptString);
        }
        if(NULL != dataOut.pbData) {
            LocalFree(dataOut.pbData);
        }
        return fAllowAccess;         
}


HRESULT ConvertWildCard(const WCHAR *pWild, StringConverter *pNew, BOOL fUnicodeRules)
{
    ASSERT(NULL != pWild);
    ASSERT(NULL != pNew);    
    
    UINT uiWildLen = wcslen(pWild);    
    WCHAR *pWildTmp = NULL;
    HRESULT hr = E_FAIL;
    
    //
    // We cant be 0 (that would mean there is nothing here !?!?)
    if(0 == uiWildLen) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }  
    
    //
    // Copy the passed in string
    if(FAILED(pNew->Clear()) || FAILED(pNew->append(pWild, uiWildLen))) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }         
    
    //
    // First pass on unicode rules, do conversion (per spec in 3.4 of cifs9f.doc)
    //  ? translates to >
    //  , translates to " if not followed by ? or *
    //  * translates to < if not followed by .
    pWildTmp = pNew->GetUnsafeString() + uiWildLen - 1;
    
    if(TRUE == fUnicodeRules) {  
        while(pWildTmp > pNew->GetUnsafeString()) {
            WCHAR wThis = *pWildTmp;
            WCHAR wNext = *(pWildTmp + 1); //this is safe b/c we are null terminated
            
            if(wThis == '>') {
                *pWildTmp = '?';
            }    
            else if (wThis == '"' && (wNext == '?' || wNext == '*')) { 
                *pWildTmp = '.';    
            }
            else if (wThis == '<' && wNext == '.') { 
                *pWildTmp = '*';    
            }
            pWildTmp --;
        }        
    }
           
        
    //
    // Success
    hr = S_OK;
       
    Done:
        return hr;    
}

HRESULT FileTimeToSMBTime(const FILETIME *pFT, SMB_TIME *pSMBTime, SMB_DATE *pSMBDate)
{
    SYSTEMTIME st;
    HRESULT hr;

    if(0 == FileTimeToSystemTime(pFT, &st)) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    
    //
    // Do the time
    pSMBTime->Hours = st.wHour;
    pSMBTime->Minutes = st.wMinute;
    pSMBTime->TwoSeconds = (st.wSecond / 2);
        
    //
    // Do the DATE
    ASSERT(st.wYear >= 1980);
    pSMBDate->Day = st.wDay;
    pSMBDate->Month = st.wMonth;
    pSMBDate->Year = st.wYear - 1980;     
        
    //
    // Success
    hr = S_OK;
    
    Done:
        return hr;
}

BOOL MatchesWildcard(DWORD len, LPCWSTR lpWild, DWORD len2, LPCWSTR lpFile)
{
    while (len && len2) {
        if (*lpWild == L'*') {
            lpWild++;
            len--;
            
            if ((len >= 2) && (lpWild[0] == L'.') && (lpWild[1] == L'*')) {
                len -= 2;
                lpWild += 2;
            }
            
            if (len) {
                while (len2) {
                    if (MatchesWildcard(len, lpWild, len2--, lpFile++))
                        return TRUE;
                }
                return FALSE;
            }
            
            return TRUE;
        
        } else if ((*lpWild != L'?') && _wcsnicmp(lpWild, lpFile, 1)) {
            return FALSE;
        }
        
        len--;
        lpWild++;
        len2--;
        lpFile++;
    }
    
    if (!len && !len2)
        return TRUE;
    
    if (!len)
        return FALSE;
    
    while (len--) {
        if (*lpWild++ != L'*')
            return FALSE;
        if ((len >= 2) && (lpWild[0] == L'.') && (lpWild[1] == L'*')) {
            len -= 2;
            lpWild += 2;
        }
    }
    
    return TRUE;
}

struct SMB_LOOKUP_ERR {
    DWORD dwGLE;
    DWORD dwSMB;
};

SMB_LOOKUP_ERR SMBErrList[] = {
    {ERROR_PATH_NOT_FOUND, SMB_ERR(ERRDOS, ERRbadpath)},
    {ERROR_FILE_NOT_FOUND, SMB_ERR(ERRDOS, ERRbadfile)},
    {ERROR_ACCESS_DENIED, SMB_ERR(ERRDOS, ERRnoaccess)},
    {ERROR_FILE_EXISTS, SMB_ERR(ERRDOS, ERRfilexists)},
    {ERROR_SHARING_VIOLATION, SMB_ERR(ERRDOS, ERRnoaccess)},
    {ERROR_ALREADY_EXISTS, SMB_ERR(ERRDOS, ERRfilexists)},
    {0xFFFF, 0xFFFF}
};

DWORD ConvertGLEToSMB(DWORD dwGLE)
{
    UINT i = 0;
    DWORD dwErr = SMB_ERR(ERRSRV, ERRerror);
    for(;;) {
        if(0xFFFF == SMBErrList[i].dwGLE && 0xFFFF == SMBErrList[i].dwSMB) {
            break;
        }
        else if(dwGLE == SMBErrList[i].dwGLE) {
            dwErr = SMBErrList[i].dwSMB;
            break;
        } else {
            ++i;
        }
    }    
    return dwErr;
}



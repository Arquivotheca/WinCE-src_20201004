//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef UTILS_H
#define UTILS_H

#include <list.hxx>
#include <block_allocator.hxx>

#include "smbpackets.h"
#include "CriticalSection.h"


#ifndef NO_POOL_ALLOC
#define UNIQUEID_ID_ALLOC             ce::singleton_allocator< ce::fixed_block_allocator<30>, sizeof(USHORT) >
#define RING_LIST_NODE_ALLOC          ce::singleton_allocator< ce::fixed_block_allocator<10>, sizeof(RING_NODE*) >
#else
#define UNIQUEID_ID_ALLOC            ce::allocator
#define RING_LIST_NODE_ALLOC         ce::allocator
#endif


struct SMB_PACKET;
class ActiveConnection;
class RING_NODE;

extern ce::fixed_block_allocator<10>           g_RingNodeAllocator;

class StringTokenizer {
    public:
       StringTokenizer(BYTE *_pData, UINT _uiSize) : pData(_pData), uiRemaining(_uiSize) {
       }
       StringTokenizer(){
            Reset(NULL, 0);
       }
       ~StringTokenizer() { }

       VOID Reset(BYTE *_pData, UINT _uiSize) {
            pData = _pData;
            uiRemaining = _uiSize;
       }

        HRESULT GetByteArray(BYTE **pArray, UINT uiSize)
        {
            if(uiSize > uiRemaining)
                return E_FAIL;
            *pArray = pData;
            pData += uiSize;
            uiRemaining -= uiSize;
            return S_OK;
        }

        HRESULT GetUnicodeString(WCHAR **pString, UINT *puiSize = NULL) {
            HRESULT hr = E_FAIL;
            UINT uiSize = 0;

            //
            // All STRINGS must be alligned, if not they are padded with
            //   a NULL
            if((UINT)pData % 2) {
                uiRemaining --;
                pData ++;
            }
            WORD *pStart = (WORD *)pData;

            while(uiRemaining >= sizeof(WCHAR) && NULL != *pData) {
                uiRemaining -=sizeof(WCHAR);
                pData += sizeof(WCHAR);
                uiSize += sizeof(WCHAR);
            }

            if(NULL == *pData && uiRemaining >= sizeof(WCHAR)) {
                *pString = pStart;
                pData += sizeof(WCHAR); //<--advance BEYOND the NULL
                uiSize += sizeof(WCHAR);
                uiRemaining -= sizeof(WCHAR);
                hr = S_OK;
            }
            if(SUCCEEDED(hr) && NULL != puiSize) {
                *puiSize = uiSize;
            }
            ASSERT(0 == (uiSize % 2));
            return hr;
        }

        HRESULT GetString(CHAR **pString, UINT *puiSize = NULL) {
            HRESULT hr = E_FAIL;
            CHAR *pStart = (CHAR *)pData;
            UINT uiSize = 0;
            while(uiRemaining && NULL != *pData) {
                uiRemaining --;
                pData ++;
                uiSize ++;
            }

            if(NULL == *pData && uiRemaining) {
                *pString = pStart;
                pData ++; //<--advance BEYOND the NULL
                uiSize ++;
                uiRemaining --;
                hr = S_OK;
            }
            if(SUCCEEDED(hr) && NULL != puiSize) {
                *puiSize = uiSize;
            }

            return hr;
       }

        HRESULT GetWORD(WORD *pWord) {
            if(uiRemaining < sizeof(WORD))
                return E_FAIL;
            else {
                //PERFPERF: this could be FASTER!!!
                memmove(pWord, pData, sizeof(WORD));
                pData += sizeof(WORD);
                uiRemaining -= sizeof(WORD);
                return S_OK;
            }
       }

       HRESULT GetDWORD(DWORD *pDWord) {
            if(uiRemaining < sizeof(DWORD))
                return E_FAIL;
            else {
                //PERFPERF: this could be FASTER!!!
                memmove(pDWord, pData, sizeof(DWORD));
                pData += sizeof(DWORD);
                uiRemaining -= sizeof(DWORD);
                return S_OK;
            }
       }

       HRESULT GetByte(BYTE *pByte) {
            if(uiRemaining < sizeof(BYTE))
                return E_FAIL;
            else {
                *pByte = *(BYTE *)pData;
                pData += sizeof(BYTE);
                uiRemaining -= sizeof(BYTE);
                return S_OK;
            }
       }

       UINT RemainingSize() {
            return uiRemaining;
       }

    private:
       BYTE *pData;
       UINT uiRemaining;
};


class UniqueID {
    public:
        UniqueID();
        ~UniqueID();
        HRESULT GetID(USHORT *ulID);
        HRESULT RemoveID(USHORT ulID);
        UINT NumIDSOutstanding() {return IDList.size();}
    private:
        ce::list<USHORT, UNIQUEID_ID_ALLOC> IDList;
        USHORT usNext;
};

class StringConverter {
    public:
        StringConverter();
        StringConverter(const CHAR *pString);
        StringConverter(const StringConverter&);
        ~StringConverter();
        HRESULT append(const CHAR *pString, UINT uiChars=0xFFFFFFFF);
        HRESULT append(const WCHAR *pString, UINT uiChars=0xFFFFFFFF);
        HRESULT Clear() ;
        const WCHAR *GetString();
        WCHAR *GetUnsafeString();
        UINT Size();
        UINT Length();
        BYTE *NewSTRING(UINT *uiSize, BOOL fUnicode);

    private:
        VOID Init();
        WCHAR *pMyString;
        UINT uiLength;
        UINT uiBufLength;
        WCHAR MyString[64];
};


class MemMappedBuffer {
    public:
        MemMappedBuffer();
        ~MemMappedBuffer();

        HRESULT Open(WCHAR *pFile, UINT uiMaxSize, UINT uiInc);
        HRESULT Close();
        VOID *Create(UINT *pBlockSize);
        VOID Return(VOID *);
        UINT BytesRemaining() {
            CCritSection csLock(&myLock);
            csLock.Lock();
            return m_uiMaxSize - m_uiUsed;
        }
        UINT TotalBufferSize() {
            CCritSection csLock(&myLock);
            csLock.Lock();
            return m_uiMaxSize;
        }

    private:
        HANDLE m_hFile;
        HANDLE m_hMapping;
        VOID *m_pNext;
        UINT m_uiInc;
        UINT m_uiUsed;
        UINT m_uiMaxSize;
        UINT m_uiBlocksOutstanding;
        BOOL m_fInited;
        CRITICAL_SECTION myLock;
};

class RING_NODE {
    public:
        void * operator new(size_t size) {
             return g_RingNodeAllocator.allocate(size);
        }
        void   operator delete(void *mem, size_t size) {
            g_RingNodeAllocator.deallocate(mem, size);
        }

        BYTE *m_pReadBuffer;
        UINT m_uiReadyToRead;

        UINT m_uiWriteRemaining;
        BYTE *m_pWriteBuffer;


        UINT m_uiBlockSize;
        VOID *m_pHead;
};

class RingBuffer {
   public:
        RingBuffer();
        ~RingBuffer();

        HRESULT Purge();
        HRESULT Read(BYTE *pDest, UINT uiRequested, UINT *puiReturned);
        HRESULT Write(const BYTE *pSrc, UINT uiSize, UINT *puiWritten);

        UINT BytesReadyToRead();
        UINT BytesRemaining() {
            CCritSection csLock(&m_myLock);
            csLock.Lock();
            return SMB_Globals::g_PrinterMemMapBuffers.BytesRemaining();
        }
        UINT TotalBufferSize() {
            CCritSection csLock(&m_myLock);
            csLock.Lock();
            return SMB_Globals::g_PrinterMemMapBuffers.TotalBufferSize();
        }
   private:

        //
        // Write to the beginning, read from the end
        //    (front() = newest from wire)
        //    (last()  = oldest data)
        ce::list<RING_NODE *, RING_LIST_NODE_ALLOC > m_BufferList;

        UINT m_uiReadyToRead;
        CRITICAL_SECTION m_myLock;
};


template <size_t InitCount, typename T>
class ClassPoolAllocator {
    public:
        ClassPoolAllocator() {
        }
        ~ClassPoolAllocator() {
        }
        T* Alloc() {
            T *pRet = (T*)(myAlloc.allocate(sizeof(T)));

            //
            // Construct in place
            new (pRet) T;

            return pRet;

        }
        VOID  Free(T *p) {
            p->~T();
            myAlloc.deallocate(p, sizeof(T));
        };
    private:
        ce::fixed_block_allocator<InitCount> myAlloc;
};


template <size_t InitCount, typename T>
class ThreadSafePool {
    public:
        ThreadSafePool() {
            InitializeCriticalSection(&cs);
        }
        ~ThreadSafePool() {
            DeleteCriticalSection(&cs);
        }
        T* Alloc() {
            CCritSection csLock(&cs);
            csLock.Lock();
            return myAlloc.Alloc();
        }
        VOID  Free(T *p) {
            CCritSection csLock(&cs);
            csLock.Lock();
            myAlloc.Free(p);
        };
    private:
        CRITICAL_SECTION cs;
        ClassPoolAllocator<InitCount, T> myAlloc;
};


//
// Return the # of seconds since Jan1 1970 at 0:00:00
unsigned int SecSinceJan1970_0_0_0 (void);
unsigned int SecSinceJan1970_0_0_0 (FILETIME *ft);
BOOL FileTimeFromSecSinceJan1970(UINT uiSec, FILETIME *ft);

BOOL VerifyPassword(ce::smart_ptr<ActiveConnection> pMyConnection, BYTE *pBlob, UINT uiBlobLen);
HRESULT ConvertWildCard(const WCHAR *pWild, StringConverter *pNew, BOOL fUnicodeRules);
HRESULT FileTimeToSMBTime(const FILETIME *pFT, SMB_TIME *pSMBTime, SMB_DATE *pSMBDate);
WORD Win32AttributeToDos(DWORD attributes);
BOOL MatchesWildcard(DWORD len, LPCWSTR lpWild, DWORD len2, LPCWSTR lpFile);
DWORD ConvertHRtoNT(HRESULT hr);
DWORD ConvertNTToSMB(DWORD dwNT);
DWORD ConvertGLEToError(DWORD dwGLE, SMB_PACKET *pSMB);
DWORD ConvertHRToError(HRESULT hr, SMB_PACKET *pSMB);


/*
#ifdef DEBUG

class ChkPerf {
    public:
        ChkPerf() {
            dwStart = GetTickCount();
        }
        ~ChkPerf() {
            DWORD dwEnd = GetTickCount();
            TRACEMSG(ZONE_STATS, (L"CHKPERF: %d", dwEnd - dwStart));
            if(dwEnd - dwStart >= 1000) {
            ASSERT(FALSE);
            }
        }

    private:
        DWORD dwStart;
};
#endif*/



typedef VOID (*WakeUpEventFN) (USHORT usID, VOID *pToken);

class WakeUpNode
{
    public:
        WakeUpNode() : m_h(NULL), m_usID(0xFFFF), m_cRef(1) {}
        virtual ~WakeUpNode() {}
        USHORT GetID() {return m_usID;}
        VOID SetID(USHORT usID) {m_usID = usID;}

        HANDLE GetHandle() {return m_h;}
        VOID SetHandle(HANDLE h) {m_h = h;}

        virtual VOID WakeUp() = 0;
        virtual VOID Terminate(bool nolock = false) = 0;

        ULONG AddRef()
        {
            return (ULONG) InterlockedIncrement((LONG*) &m_cRef);
        }
        ULONG Release()
        {
            ULONG cNewRef = (ULONG) InterlockedDecrement((LONG*) &m_cRef);

            if( 0 == cNewRef )
                delete this;

            return cNewRef;
        }

   protected:
        volatile LONG m_cRef;
        USHORT m_usID;
        HANDLE m_h;
};

class WakeUpOnEvent
{
    public:
        WakeUpOnEvent();
        ~WakeUpOnEvent();

        HRESULT AddEvent(HANDLE h,  WakeUpNode *pNode, USHORT *pID);
        HRESULT RemoveEvent(USHORT usID);


        VOID Freeze() {EnterCriticalSection(&m_csLock);}
        VOID UnFreeze(){LeaveCriticalSection(&m_csLock);}


    protected:
        HRESULT WakeUp();

    private:

        static DWORD SMB_WakeupThread(LPVOID pWakeUpOnEventObj);


        HANDLE m_hWakeUpEvent;
        HANDLE m_hWakeUpThread;
        BOOL m_fStop;
        UniqueID m_UniqueID;
        CRITICAL_SECTION m_csLock;

        ce::list<WakeUpNode *> m_HandList;
};

#endif

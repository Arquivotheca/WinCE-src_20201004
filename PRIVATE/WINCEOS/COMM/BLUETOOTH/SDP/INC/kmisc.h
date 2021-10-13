//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

   Kmisc.h

Abstract:

    This module contains misc kernel classes like lists, queues, spinlocks...



    Husni Roukbi

  Notes:


Environment:

   Kernel mode only


Revision History:

--*/

#ifndef __KMISC_H__
#define __KMISC_H__

///////////////////////////////
// Defines
///////////////////////////////

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#define IRP_LE_OFFSET  ((ULONG_PTR) ( & ((IRP* ) 0)->Tail.Overlay.ListEntry))    // IRP list entry filed offset is 88 bytes

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CBaseObj class will be the base class for objects queued in the CxList classes.
//
// Rules for using this base obeject:
// 1) derived object should use SELF_DESTRUCT.
// 2) Use AddRef() right after you reference an object and Release(0 when you are done with the object reference.
// 3) Never delete this object or its derived objects directly. RefCount is incremented to one in constructor. 
//     Hence, a call to Release() that has no match to AddRef() will trigger deletion 
//     of this object and its derived objects. 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CBaseObj 
{
public:

    virtual SELF_DESTRUCT   
    CBaseObj() {m_RefCount = 1; m_pEvent = NULL;}
    virtual LONG AddRef() ;
    virtual LONG Release();

private:

	LONG        m_RefCount;	   // reference count

protected:
    virtual ~CBaseObj();       // will be called only from selfdestruct().
    PKEVENT  m_pEvent;         // cache an event ptr  waiting for this object to be deleted.
};

inline CBaseObj::~CBaseObj()
{
    ASSERT (m_RefCount == 0);
//    _DbgPrintF(DBG_PNP_INFO,("Deleting CBaseObj"));
}

inline LONG CBaseObj::AddRef()
{							
	return  InterlockedIncrement(&m_RefCount);
}	
					
inline LONG CBaseObj::Release() 
{	
    LONG RefCount;

    if ((RefCount = InterlockedDecrement(&m_RefCount)) == 0) {
//        ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
        //
        // Cache the event pointer is case something is blocked on
        // object deletion.
        //
        PKEVENT pEvent = m_pEvent;

        //
        // Call destructor on the parent object.
        //
        SelfDestruct();
        //
        // Set the close event if there is one.  
        //
        if (pEvent) {
            KeSetEvent(pEvent,IO_NO_INCREMENT,FALSE);
        }
    }
    return RefCount;
}			

//
// Smart pointer class.  This class makes the assumptiont that the T* being
// assigned to it ALREADY has an AddRef called against it.  This means that this
// class will only call T::Release() on pT, never T::AddRef().  This is especially
// important to remember when doing an assinment or contructing this ptr;
//
template<class T> class SmartPtr {
public:
    SmartPtr() : pT(NULL) {}
    SmartPtr(T* pT) : pT(pT) {}
    
    ~SmartPtr()
    {
        if (pT != NULL) {
            pT->Release();
        }
    }

        operator T*()       { return pT;            }
    T*  operator ->() const { return pT;            }
    T&  operator *()        { return *pT;           }
    T** operator &()        { return &pT;           }

    bool operator!=(const T* pRightT) const { return pT != pRightT; }
    bool operator! (void)             const { return pT == NULL;    }
    bool operator==(const T* pRightT) const { return pT == pRightT; }
    bool operator< (const T* pRightT) const { return pT < pRightT;  }

    T*&  operator= (T* pNewT)
    {
        if (pT != NULL) {
            pT->Release();
        }
        pT = pNewT;

        return pT;
    }

    T* Detach() 
    {
        T* p = pT;
        pT = NULL;
        return p;
    }

protected:
    //
    // do not want these functions to be invoked
    //
    SmartPtr(const SmartPtr<T>& lp) { ASSERT(FALSE); }

    T*& operator=(const SmartPtr<T>& pNewT) 
    {
        ASSERT(FALSE);
        return pT; 
    }

    T* pT;
};
#endif // UNDER_CE

template<class T> class NoRefObject {
public:
    void AddRef(T* pT)  {}
    void Release(T* pT) {}
};

template<class T> class RefObject {
public:
    void AddRef(T* pT)  { pT->AddRef(); }
    void Release(T* pT) { pT->Release(); }
};


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CKSpinLock wraps around spinlocks and hides dispatch level vs. passive level issues..
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CKSpinLock
{
public:
    SELF_DESTRUCT

    CKSpinLock(void);
    ~CKSpinLock(void);

    void Lock()     { AcquireSpinLock();}
    void Unlock()   { ReleaseSpinLock();}

	KIRQL _AcquireSpinLock(void);
    void AcquireSpinLock(void);
    void ReleaseSpinLock(void);
    operator PKSPIN_LOCK () { return &m_SpinLock; }

private:
    KSPIN_LOCK m_SpinLock;
    KIRQL            m_OldIrql;
};

//
// inline functions for CKSpinLock
//

inline CKSpinLock::CKSpinLock(void)
{
	KeInitializeSpinLock(&m_SpinLock);
}

inline CKSpinLock::~CKSpinLock(void)
{
}

inline void CKSpinLock::AcquireSpinLock(void)
{
    m_OldIrql = _AcquireSpinLock();
}

inline KIRQL CKSpinLock::_AcquireSpinLock(void)
{
    if (KeGetCurrentIrql() < DISPATCH_LEVEL) {
        KIRQL  Irql;
	    KeAcquireSpinLock(&m_SpinLock, &Irql);
        return Irql;
	}
	else {
		KeAcquireSpinLockAtDpcLevel(&m_SpinLock);
        return DISPATCH_LEVEL;
	}
}

inline void CKSpinLock::ReleaseSpinLock(void)
{
    if (m_OldIrql < DISPATCH_LEVEL) {
        KeReleaseSpinLock(&m_SpinLock, m_OldIrql); 
    }
    else {
        KeReleaseSpinLockFromDpcLevel(&m_SpinLock);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
class CFastMutex
{
public:
    SELF_DESTRUCT

    CFastMutex() { ExInitializeFastMutex(&m_FastMutex); }
    ~CFastMutex() {}

    void Lock()     { AcquireMutex(); }
    void Unlock()   { ReleaseMutex(); }

    void AcquireMutex();
    void ReleaseMutex();

private:
    FAST_MUTEX  m_FastMutex;
};

inline void CFastMutex::AcquireMutex()
{
    KeEnterCriticalRegion();
    ExAcquireFastMutexUnsafe(&m_FastMutex);
}

inline void CFastMutex::ReleaseMutex()
{
    ExReleaseFastMutexUnsafe(&m_FastMutex);
    KeLeaveCriticalRegion();
}

////////////////////////////////////////////////////////////////////////////////////////////
class CSemaphore
{
public:
    SELF_DESTRUCT

    CSemaphore(LONG Count = 1, LONG Limit = MAXLONG) ;
    ~CSemaphore() {}
    BOOLEAN ReleaseSem(KPRIORITY Increment = 0,LONG Adjustment = 1, BOOLEAN bWait = FALSE);
    void AcquireSem();
    BOOLEAN ReadState();

    void Lock()     { AcquireSem(); }
    void Unlock()   { ReleaseSem(); }

private:
    KSEMAPHORE m_Semaphore;
};

inline CSemaphore::CSemaphore(LONG Count , LONG Limit) 
{
	KeInitializeSemaphore(&m_Semaphore, Count, Limit);
}

inline void CSemaphore::AcquireSem() 
{ 
    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
    KeWaitForSingleObject((PVOID) &m_Semaphore,Executive,KernelMode,FALSE,NULL);
}

inline BOOLEAN CSemaphore::ReleaseSem(KPRIORITY Increment, LONG Adjustment, BOOLEAN bWait)
{
    if ( bWait) {
        ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    }
    else {
        ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    }
 
	return (BOOLEAN) KeReleaseSemaphore(&m_Semaphore,Increment, Adjustment, bWait); 
}

inline BOOLEAN CSemaphore::ReadState()
{
	return (BOOLEAN) KeReadStateSemaphore( &m_Semaphore );
}
#endif // defined (UNDER_CE) || defined (WINCE_EMULATION)

class NoRealLock {
public:
    SELF_DESTRUCT

    void Lock();
    void Unlock();
};

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
class CSemaphore
{
public:
    SELF_DESTRUCT

    CSemaphore(LONG Count = 1, LONG Limit = MAXLONG) {;}
    ~CSemaphore() {}
    //BOOLEAN ReadState() { ; }
    void AcquireSem();
    BOOLEAN CSemaphore::ReleaseSem(KPRIORITY Increment = 0,LONG Adjustment = 1, BOOLEAN bWait = FALSE);

    void Lock()     { AcquireSem(); }
    void Unlock()   { ReleaseSem(); }
};

#endif // UNDER_CE

////////////////////////////////////////////////////////////////////////////////////////////
//
// CList : this template will take any structure that contain a LIST_ENTRY field 
// the default construtor assumes that LIST_ENTRY is the first field in a structure.
// if not, you have to call the init function with the field offset of the list entry before using the que. 
// to que an irp in this template, use init(IRP_LE_OFFSET) function before starting
// to use the que. If the objects to be inserted in the que are derived from CBaseObj, then
// set the UseBaseObj to TRUE when you declare these objects. If you do, the template class 
// will call the AddRef() on the object when it gets inserted in the que, and Release() when it is removed.
//  
template <class T, ULONG OffsetLE = 0, class L = NoRealLock, class R = NoRefObject<T> > 
class CList 
{
private:
    SELF_DESTRUCT
	
    LIST_ENTRY m_ListHead;
    CList (CList<T, OffsetLE, class L, class R> &);               // no copy constuctor allowed.
    void operator= (CList<T, OffsetLE, class L, class R>&);
	T* ObjectPtrFromListEntryPtr(PLIST_ENTRY pListEntry);
	PLIST_ENTRY ListEntryPtrFromObjectPtr(PVOID pObject);

    L m_Lock;

public:

    CList(void);
    ~CList(void);
	T*		GetHead(void);
	T* 		GetTail(void);
	T* 		GetNext(T* pTCurr);
	T*		GetPrev(T* pTCurr);
	void	AddHead(T* pT);
	void	AddTail(T* pT);
	T* 		RemoveHead(void);
	T* 		RemoveTail(void);
	T* 		RemoveAt(T* pT);
	BOOLEAN	IsEmpty(void);
	ULONG 	GetCount(void);

	void Lock(void)     { m_Lock.Lock();      }
	void Unlock(void)   { m_Lock.Unlock();    }
};

template <class T, ULONG OffsetLE, class L, class R> 
CList<T, OffsetLE, L, R>::CList(void) 
{ 
   InitializeListHead (&m_ListHead);
}

template <class T, ULONG OffsetLE, class L, class R>
CList<T, OffsetLE, L, R>::~CList(void) 
{ 
}

template <class T, ULONG OffsetLE, class L, class R>
PLIST_ENTRY CList<T, OffsetLE, L, R>::ListEntryPtrFromObjectPtr(PVOID pObject)
{
    if (pObject) {
	    return  (PLIST_ENTRY)( OffsetLE + (ULONG)pObject);
    }
    return NULL;
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::ObjectPtrFromListEntryPtr(PLIST_ENTRY pListEntry)
{
	if (pListEntry == &m_ListHead) {
		return NULL;
    }
    else {
        if ( pListEntry) {
		    return (T*)( (PUCHAR)pListEntry - OffsetLE );
        }
    }
    return NULL;
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::GetHead(void) 
{ 
    T * pT;
    if ( IsListEmpty(&m_ListHead) ) {
        pT = NULL;
    }
    else {
        pT =  ObjectPtrFromListEntryPtr( m_ListHead.Flink);
    }
    return  pT;
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::GetTail(void) 
{ 
    T * pT;
    if ( IsListEmpty(&m_ListHead)) {
        pT = NULL;
    }
    else {
        pT =  ObjectPtrFromListEntryPtr(m_ListHead.Blink);
    }
    return  pT;
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::GetNext(T* pTCurr)
{
	T *  pT;
    if (pTCurr == NULL) {
		return GetHead();  
    }
    else {
        if ( ( ListEntryPtrFromObjectPtr(pTCurr))->Flink != &m_ListHead) {
		    pT =  ObjectPtrFromListEntryPtr( (ListEntryPtrFromObjectPtr(pTCurr) )->Flink ) ;
        }
        else  {
            pT = NULL;
        }
    }
	return pT;
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::GetPrev(T* pTCurr)
{
	T *  pT;
    if (pTCurr == NULL) {
		return GetTail();
    }
    else {
        if ( (ListEntryPtrFromObjectPtr(pTCurr))->Blink != &m_ListHead) {
    		pT = ObjectPtrFromListEntryPtr( (ListEntryPtrFromObjectPtr(pTCurr) )->Blink) );
        }
        else {
            pT = NULL;
        }
    }
	return pT;
}

template <class T, ULONG OffsetLE, class L, class R>
void CList<T, OffsetLE, L, R>::AddHead(T* pT)
{
    R r;
    r.AddRef(pT);

	InsertHeadList(&m_ListHead, ListEntryPtrFromObjectPtr(pT) );
}

template <class T, ULONG OffsetLE, class L, class R>
void CList<T, OffsetLE, L, R>::AddTail(T* pT)
{
    R r;
    r.AddRef(pT);

	InsertTailList(&m_ListHead, ListEntryPtrFromObjectPtr(pT) );
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::RemoveHead(void) 
{ 
    T* pT = NULL;
    if ( IsListEmpty(&m_ListHead) == FALSE) {
        PLIST_ENTRY pListEntry = RemoveHeadList(&m_ListHead);
        ASSERT( pListEntry != NULL);
        pT = ObjectPtrFromListEntryPtr(pListEntry);
        R r;
        r.Release(pT);
    }
    return pT;    
}


template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::RemoveTail(void) 
{ 
    T* pT = NULL;
    if ( IsListEmpty(&m_ListHead) == FALSE) {
        PLIST_ENTRY pListEntry = RemoveTailList(&m_ListHead);
        ASSERT( pListEntry != NULL);
        pT = ObjectPtrFromListEntryPtr(pListEntry);
        R r;
        r.Release(pT);
    }
    return pT;    
}

template <class T, ULONG OffsetLE, class L, class R>
T* CList<T, OffsetLE, L, R>::RemoveAt(T* pTCurr)
{
    if (pTCurr){
        R r;
		RemoveEntryList( ListEntryPtrFromObjectPtr(pTCurr) );

        r.Release(pTCurr);
	}
	return pTCurr;
}

template <class T, ULONG OffsetLE, class L, class R>
BOOLEAN CList<T, OffsetLE, L, R>::IsEmpty(void) 
{ 
	BOOLEAN bTest = IsListEmpty(&m_ListHead);
	return bTest;
}

template <class T, ULONG OffsetLE, class L, class R>
ULONG CList<T, OffsetLE, L, R>::GetCount(void)
{
    ULONG ulCount=0;
    if (IsListEmpty(&m_ListHead)) {
        ulCount = 0;
    }
    else{
		PLIST_ENTRY pListEntry  = m_ListHead.Flink;
        ulCount = 0;
        do {
			ulCount++;
			pListEntry = pListEntry->Flink;
		} while  (pListEntry != &m_ListHead);
	}
	return ulCount;
}

////////////////////////////////////////////////////////////////////////////////
typedef 
void
(*PFNCMNDCOMPLETE)( PVOID CommandContext   );

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
class KCounter 
{
private:
	LONG m_Counter;
public:
    SELF_DESTRUCT

	KCounter(void) { m_Counter = 0; }
    void init (LONG value) { m_Counter = value;}
    BOOLEAN Off() { return ((m_Counter == 0) ? TRUE : FALSE); }
    BOOLEAN On() { return ((m_Counter == 0) ? FALSE : TRUE); }
    LONG operator ++(void);
	LONG operator --(void) { return InterlockedDecrement(&m_Counter); }
	operator LONG () { return m_Counter; }
	LONG Peek(void) { return InterlockedExchangeAdd(&m_Counter, 0); }
	LONG TestAndSet(LONG Value) { return InterlockedExchange(&m_Counter, Value);}
    LONG Add( LONG Value) { return InterlockedExchangeAdd(&m_Counter, Value);}
};

inline LONG KCounter::operator ++(void)
{
    LONG counter = InterlockedIncrement(&m_Counter);
    if (counter == 0 )
        ++counter;
    return counter;
}

/////////////////////////////////////////////////////////////////////////
extern ULONG PoolTag;
//
// this function sets the pool tag. 
//
inline void AssignPoolTag(ULONG Tag)
{
    PoolTag = Tag;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
inline PVOID operator new ( size_t iSize,POOL_TYPE poolType  )
{
    PVOID result = NULL;
    if ( iSize) {
        result = ExAllocatePoolWithTag(poolType,iSize,PoolTag);
    }
    if (result) {
        RtlZeroMemory(result,iSize);
    }

    return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
inline PVOID operator new ( size_t iSize,POOL_TYPE poolType ,  ULONG tag)
{
    PVOID result = NULL;
    if ( iSize) {
        result = ExAllocatePoolWithTag(poolType,iSize,tag);
    }
    if (result) {
        RtlZeroMemory(result,iSize);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// operator new for a previously allocated chunk of memory, usefull for open
// ended C++ structures 
inline PVOID operator new (size_t iSize, PVOID pVoid)
{
    if (pVoid) {
        RtlZeroMemory(pVoid, iSize);
    }

    return pVoid;
}

///////////////////////////////////////////////////////////////////////////
inline void __cdecl operator delete(PVOID pVoid)
{
    if ( pVoid)
        ExFreePool(pVoid);
}
///////////////////////////////////////////////////////////////////////////
inline void __cdecl operator delete [] (PVOID pVoid)
{
    if (pVoid)
        ExFreePool(pVoid);
}
#endif // UNDER_CE

#endif

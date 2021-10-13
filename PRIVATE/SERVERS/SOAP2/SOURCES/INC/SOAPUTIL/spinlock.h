//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:   spinlock.h
// 
// Contents:
//
//      spinlock header file
//    
//
//-----------------------------------------------------------------------------

#ifndef __SPINLOCK_H_INCLUDED__
#define __SPINLOCK_H_INCLUDED__

// Spinlock types.
enum ESpinLockType
{
    x_NullSpinlock = 0, // used to detect uninitialized spin locks.
    x_AssertFile,
    x_MultiLang,

    // STEP 1: Add new spinlock types *BEFORE* this line!
    //
    x_Last
};    

//--------------------------------------------------------------------
// provides spinlock functionality for soap
//
//
class CSpinlock
{
public:
    CSpinlock() : 
        m_dwLock(0),
        m_eType(x_NullSpinlock)
    {
        #ifdef DEBUG
            m_tid = 0xffffffff;
        #endif // DEBUG
    }

    CSpinlock(ESpinLockType eLockType) :
        m_dwLock(0),
        m_eType(eLockType)
    {
        ASSERT(eLockType > x_NullSpinlock && eLockType < x_Last);
        #ifdef DEBUG
            m_tid = 0xffffffff;
        #endif // DEBUG
    }

    // acquire the spin lock
    void Get();

    inline void Init(ESpinLockType eLockType)
    {
        m_dwLock = 0;
        m_eType = eLockType;
        ASSERT(eLockType > x_NullSpinlock && eLockType < x_Last);
    }


    // trys to acquire the spinlock without waiting
    BOOL GetNoWait();

    // releases the spinlock
    void Release();

    #ifdef DEBUG
    BOOL IsAcquired()
    {
        return m_dwLock > 0 && m_tid == GetCurrentThreadId();
    }
    #endif // DEBUG


protected:
    // function - where it actually spins and sleeps
    void SpinToAcquire();

    // the actual spinlock itself
    volatile DWORD    m_dwLock;

    // Spinlock eType for information
    ESpinLockType    m_eType;

    #ifdef DEBUG
        // @cmember thread owning spinlock
        DWORD             m_tid;
    #endif // DEBUG
};


inline void CSpinlock::Get()
{
    if (!GetNoWait())
        SpinToAcquire();

    #ifdef DEBUG
        m_tid = GetCurrentThreadId();
    #endif
}


#pragma warning (disable : 4035)
inline BOOL CSpinlock::GetNoWait ()
{
    #if defined (i386)
        {
        _asm
            {
            mov ecx, this
            xor eax, eax
            inc eax
            xchg [ecx], eax
            dec eax
            };
        }
    #else
        return InterlockedExchange ((long*)&m_dwLock, 1) == 0;
    #endif
}
#pragma warning (default : 4035)

inline void CSpinlock::Release()
{
    #if defined (i386)
        // This inline asm serves to force the compiler
        // to complete all preceding stores.  Without it,
        // the compiler may clear the spinlock *before*
        // it completes all of the work that was done
        // inside the spinlock.

        _asm { __asm _emit 0x90 }

        // This generates a smaller instruction to clear out
        // the byte containing the 1.

        *(char*)&m_dwLock = 0;

    #else
        // Everybody else uses interlocked exchange.
        InterlockedExchange ((long*)&m_dwLock, 0);
    #endif
}


//--------------------------------------------------------------------
// 'local' spinlock implementation. The idea is that an object
//        of this class is created on entry in a block, the lock
//        is automaticly freed on exit of the block.
//        
//        The only allowed constructor for this object takes a pointer
//        to a CSpinlock object.
//
// @hungarian lspl
//
class CLocalSpinlock
{
public:
    CLocalSpinlock(CSpinlock * pspl)
    {
        ASSERTTEXT(pspl, "CLocalSpinlock ctor requires legal parameter");
        m_pspl=pspl;
        pspl->Get();
    }

    // set the spinlock associated with a local spin lock
    void SetSpinlock(CSpinlock *pspl)
    {
        ASSERT(pspl != NULL);
        ASSERT(m_pspl == NULL);
        m_pspl = pspl;
        m_pspl->Get();
    }


    ~CLocalSpinlock()
    {
        if (m_pspl != NULL)
            m_pspl->Release();
    }
            
private:
    // function - constructor when no spinlock is supplied
    CLocalSpinlock()
    {
        m_pspl = NULL;
    };


    // Spinlock eType for keeping statistics
    CSpinlock *     m_pspl;
};

//--------------------------------------------------------------------
// enhanced 'local' spinlock implementation. The idea is that an object
//        of this class is created on entry in a block, the lock
//        is automaticly freed on exit of the block.
//        
//        The only allowed constructor for this object takes a pointer
//        to a CSpinlock object.
//
//
class CEnhancedLocalSpinlock
{
public:
    CEnhancedLocalSpinlock (
        CSpinlock * pspl,
        BOOL fAcquire)
    {
        ASSERTTEXT(pspl, "CLocalSpinlock ctor requires legal parameter");
        m_pspl=pspl;
        if (fAcquire)
        {
            pspl->Get();
            m_fAcquired = TRUE;
        }
        else
            m_fAcquired = FALSE;
    }

    CEnhancedLocalSpinlock()
    {
        m_pspl = NULL;
        m_fAcquired = FALSE;
    }


    ~CEnhancedLocalSpinlock()
    {
        if (m_fAcquired)
            m_pspl->Release();
    }

    // @cmember set the spinlock and acquire it
    void SetSpinlock(CSpinlock *pspl)
    {
        ASSERT(!m_fAcquired);
        m_pspl = pspl;
        Acquire();
    }

    // @cmember release spinlock
    void Release()
    {
        ASSERT(m_fAcquired);
        m_pspl->Release();
        m_fAcquired = FALSE;
    }

    // acquire spinlock
    void Acquire()
    {
        ASSERT(!m_fAcquired);
        m_pspl->Get();
        m_fAcquired = TRUE;
    }

    // determine if spinlock is acquired
    BOOL IsAcquired()
    {
        return m_fAcquired;
    }
            
private:

    // Spinlock eType for keeping statistics
    CSpinlock *     m_pspl;

    // is spinlock acquired
    BOOL m_fAcquired;
};

#endif  // __SPINLOCK_H_INCLUDED__

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
#pragma once

// CookieJar is a Template for an object to translate between objects and cookies.
// Objects can be anything, but must have a method void SetCookie(DWORD cookie);
// 
// Cookies may be freely given out without ref counting.
// All routines may be safely called with the invalid cookie, 0.
// All routines may be safely called with a cookie that has become invalidated.
// An object is locked when its pointer is acquired from a cookie.
// An object is unlocked when the cookie is released.
// 

#if 0
/**/    //  CookieJar can be used with any Class that has the following
/**/    //  public ctor(DWORD cookie)
/**/    //  public dtor()
/**/    //  Cookie jars are partly differentiated with the Author number (0-15).
/**/    
/**/    // 1A. Dynamic Cookie jars created and destroyed normally.
/**/    pCJ = new ce::CookieJar<obj,Author>;
/**/    delete pCJ;
/**/
/**/    // 1B. Global Cookie jars created and destroyed differently.  
/**/    ce::CookieJar<obj,Author>  g_CookieJarObj;
/**/    either: void    pCookieJar.GlobalCleanup(); Total cleanup before DLL detach
/**/    or:     void    CookieJar.GlobalShutdown(); Limited cleanup during DLL detach
/**/
/**/    // 2. Making/freeing items - allocate and free
/**/    HRESULT CookieJar.Alloc(Cookie *pC);
/**/    HRESULT CookieJar.Free(Cookie c);
/**/
/**/    // 3. Acquire and Release - Get pointer to and lock Item; Unlock item and clear pointer
/**/    HRESULT CookeJar.Acquire(Cookie c, &pObj);
/**/    HRESULT CookeJar.Release(Cookie c, &pObj);
/**/
/**/    // 4. Callback Acquire and Release - special access for limited use while locked.
/**/    HRESULT CookeJar.CallbackAcquire(Cookie c, &pObj);
/**/    HRESULT CookeJar.CallbackRelease(Cookie c, &pObj);
/**/
/**/    // 5. Enumeration.  
/**/    //      Provide input from 0 to NumberAlloc() or til E_FAIL.  Get Cookie if valid.
/**/    //      Cookies may be invalidated between Iterate and Acquire.
/**/    //      Cookies may be created at an index after checking that index.
/**/    DWORD CookeJar.NumberAlloc(void); // returns the number allocated objects
/**/    HRESULT CookeJar.Iterate(index, Cookie *pC);  // S_OK on valid cookie
/**/                                                  // S_FALSE on invalid cookie
/**/                                                  // E_FAIL on index out of range
/**/
/**/    // 6. other
/**/    DWORD CookeJar.NumberInUse(void); // returns the number active objects
/**/
#endif

// Special cookie value
static const DWORD CJ_NO_COOKIE = 0;

// CookieJar template class
template <typename T, DWORD author>
class CookieJar 
{
private:

    // Pointer types
    typedef T* PT;

    // Bitfield usage of a cookie
    typedef struct
    {
            DWORD index:12;   // index to table (which has the CJRecord)
            DWORD unique:16;  // prevents cookies from tripping over each other
            DWORD author:4;   // suggests different flavors of cookies
    } CookieBits;

    // Pointer to arrays in use 
    typedef struct
    {
        DWORD  Cookie;
        PT     Pointer;
        BYTE   NormalBusy;
        BYTE   CallbackBusy;
    } CJRecord, *PCJRecord;
    static const DWORD RecordAllocSize = 20;
    CJRecord   m_DfltRecords[RecordAllocSize];
    PCJRecord  m_Records;

    CRITICAL_SECTION m_cs;
    DWORD   m_nRecords;  // Number allocated records
    DWORD   m_nLimit;    // Total number of records allowed.
    DWORD   m_nInUse;    // Number records in use
    DWORD   m_nLastIndex;// Last allocated index
    DWORD   m_dwAuthor;  // Keeps different cookie jars looking different
    DWORD   m_dwUnique;  // Keeps different cookies in the jar looking different
    bool    m_bDestroyed;// Flags validity of this object

public:
    CookieJar()
    {
        InitializeCriticalSection(&m_cs);
        DWORD minusone = -1;
        m_dwAuthor = author;
        m_dwUnique = GetTickCount();
        m_nLastIndex = 0;
        m_nInUse = 0;
        m_nLimit = ((CookieBits*)&minusone)->index;
        m_Records = m_DfltRecords;
        InitArray();
        m_bDestroyed = false;
        assert(sizeof(DWORD) == sizeof(CookieBits));
    };

private:
    void InitArray()
    {
        // Free the table memory if it was allocated
        if (m_Records != m_DfltRecords)
        {
            delete [] m_Records;
            m_Records = m_DfltRecords;
        }

        memset(m_DfltRecords, 0, sizeof(m_DfltRecords));
        m_nRecords = _countof(m_DfltRecords);
    };
    void _Cleanup(bool bTotalCleanup)
    {
        // Must be alive to die
        if (m_bDestroyed) 
        { 
            return; 
        }
        m_bDestroyed = true;

        // Free the table memory
        EnterCriticalSection(&m_cs);
        if (bTotalCleanup)
        {
            for (DWORD i = 0; i < m_nRecords; i++)
            {
                delete m_Records[i].Pointer;
            }
        }
        InitArray();
        LeaveCriticalSection(&m_cs);
        DeleteCriticalSection(&m_cs);
    };


public:
    ~CookieJar()
    {
        _Cleanup(true);
    };
    void GlobalCleanup(void)
    {
        _Cleanup(true);
    };
    void GlobalShutdown(void)
    {
        _Cleanup(false);
    };

    HRESULT Alloc(DWORD *pCookie)
    {
        HRESULT hr = S_OK;
        PT pObj = NULL;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        // Check input
        ChkBool(pCookie != NULL, E_POINTER);

        // Increase table length when nearly full
        if (m_nInUse * 9 / 8 > m_nRecords)
        {
            Chk(Resize());
        }

        // Find slot in table - start from the last used entry
        for (DWORD iter = 0; iter < m_nRecords; iter++)
        {
            DWORD index = (m_nLastIndex + iter) % m_nRecords;

            // Check availability
            PCJRecord pCJRec = &m_Records[index];
            if (pCJRec->Pointer == NULL && pCJRec->Cookie == CJ_NO_COOKIE)
            {
                // Make new cookie
                CookieBits cookie = {0};
                DWORD dwCookie = 0;
                cookie.author = m_dwAuthor;
                cookie.index = index;
                do 
                {
                    cookie.unique = m_dwUnique++;
                } while (cookie.unique == 0);
                dwCookie = *(DWORD*)&cookie;

                // Allocate the object
                pObj = new T(dwCookie);
                ChkBool(pObj != NULL, E_OUTOFMEMORY);
        
                // Init the record
                pCJRec->Cookie = dwCookie;
                pCJRec->Pointer = pObj;
                pCJRec->CallbackBusy = 0;
                pCJRec->NormalBusy = 0;

                // Success
                *pCookie = *(DWORD*)&cookie;
                m_nLastIndex = index;
                m_nInUse++;
                hr = S_OK;
                goto Cleanup;
            }
        }
        hr = E_UNEXPECTED;

    Cleanup:
        if (FAILED(hr))
        {
            delete pObj;
        }
        LeaveCriticalSection(&m_cs);
        return hr;
    };

    HRESULT Free(DWORD cookie)
    {
        HRESULT hr = S_OK;
        CJRecord *pCJRec = NULL;
        PT pObj = NULL;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        // Take ownership of the record
        Chk(LockRecord(cookie, &pCJRec));
        pObj = pCJRec->Pointer;

        // Invalidate the entry
        memset(pCJRec, 0, sizeof(CJRecord));
        m_nInUse--;
        
    Cleanup:
        LeaveCriticalSection(&m_cs);

        // Destroy object - if found
        delete pObj;
        return hr;
    };



    HRESULT Acquire(DWORD cookie, PT &pObj)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        Chk(LockRecord(cookie, &pCJRec));
        pObj = pCJRec->Pointer;

    Cleanup:
        LeaveCriticalSection(&m_cs);
        return hr;
    };

    HRESULT Release(DWORD cookie, PT &pObj)
    {
        HRESULT hr = S_OK;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        Chk(UnlockRecord(cookie));
        pObj = NULL;

    Cleanup:
        LeaveCriticalSection(&m_cs);
        return hr;
    };



    HRESULT CallbackAcquire(DWORD cookie, PT &pObj)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        Chk(CallbackLockRecord(cookie, &pCJRec));
        pObj = pCJRec->Pointer;

    Cleanup:
        LeaveCriticalSection(&m_cs);
        return hr;
    };

    HRESULT CallbackRelease(DWORD cookie, PT &pObj)
    {
        HRESULT hr = S_OK;

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        Chk(CallbackUnlockRecord(cookie));
        pObj = NULL;

    Cleanup:
        LeaveCriticalSection(&m_cs);
        return hr;
    };



    HRESULT Iterate(DWORD index, PDWORD pCookie)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;
        ChkBool(pCookie != NULL, E_POINTER);

        ChkBool(!m_bDestroyed, E_UNEXPECTED);
        EnterCriticalSection(&m_cs);

        // locate the object at the specified index
        ChkBool(index < m_nRecords, E_FAIL);
        ChkBool(m_Records[index].Cookie != 0 && m_Records[index].Pointer != NULL, S_FALSE);

        // Got one
        *pCookie = m_Records[index].Cookie;
        hr = S_OK;

    Cleanup:
        LeaveCriticalSection(&m_cs);
        return hr;
    };

    DWORD NumberInUse(void) // returns the number active objects
    {
        return m_bDestoyed ? 0 : m_nInUse;
    }

    DWORD NumberAlloc(void) // returns the number spaces allocated
    {
        return m_bDestoyed ? 0 : m_nRecords;
    }



private:
    HRESULT Resize()
    {
        HRESULT hr = S_OK;
        DWORD nRecords = m_nRecords + RecordAllocSize;

        // Decide on the new amount
        ChkBool(nRecords <= m_nLimit, E_OUTOFMEMORY);

        // Allocate new space 
        PCJRecord pRecords = new CJRecord[nRecords];
        ChkBool(pRecords != NULL, E_OUTOFMEMORY);

        // copy the old and clear the new
        memcpy(pRecords, m_Records, m_nRecords * sizeof(CJRecord));
        memset(&pRecords[m_nRecords], 0, RecordAllocSize * sizeof(CJRecord));

        // Free the old
        if (m_Records != m_DfltRecords)
        {
            delete [] m_Records;
        }

        // Update with the new
        m_nRecords = nRecords;
        m_Records = pRecords;

    Cleanup:
        return hr;
    };



    HRESULT LockRecord(DWORD cookie, PCJRecord *ppCJRec)
    {
        HRESULT hr = S_OK;
        DWORD state = 0;
        DWORD start = GetTickCount();
        PCJRecord pCJRec = NULL;

        // Check input
        ChkBool(ppCJRec != NULL, E_POINTER);

        // Try to own, release CookieJar lock when waiting for a CookieLock
        while (1)
        {
            // recheck validity
            Chk(GetRecord(cookie, &pCJRec));
            ChkBool(GetTickCount() - start < 30000, E_FAIL);

            if (pCJRec->NormalBusy)
            {
                // Its busy.  Sleep and try again
                LeaveCriticalSection(&m_cs);
                Sleep(1);
                EnterCriticalSection(&m_cs);
            }
            else
            {
                pCJRec->NormalBusy = TRUE;
                *ppCJRec = pCJRec;
                break;
            }
        }

    Cleanup:
        return hr;
    }


    HRESULT UnlockRecord(DWORD cookie)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;

        // Make sure the entry is valid
        Chk(GetRecord(cookie, &pCJRec));

        // Release the record
        pCJRec->NormalBusy = FALSE;

    Cleanup:
        return hr;
    }


    HRESULT CallbackLockRecord(DWORD cookie, PCJRecord *ppCJRec)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;

        // Check input
        ChkBool(ppCJRec != NULL, E_POINTER);

        // Make sure the entry is valid
        Chk(GetRecord(cookie, &pCJRec));

        // Try to own callback lock.  Fail if busy
        ChkBool(pCJRec->CallbackBusy == FALSE, E_FAIL);
        pCJRec->CallbackBusy = TRUE;
        *ppCJRec = pCJRec;

    Cleanup:
        return hr;
    }


    HRESULT CallbackUnlockRecord(DWORD cookie)
    {
        HRESULT hr = S_OK;
        PCJRecord pCJRec = NULL;

        // Make sure the entry is valid
        Chk(GetRecord(cookie, &pCJRec));

        // Release the callback lock
        pCJRec->CallbackBusy = FALSE;

    Cleanup:
        return hr;
    }

    inline HRESULT GetRecord(DWORD cookie, PCJRecord *pOut)
    {
        // general checks
        DWORD index = ((CookieBits*)&cookie)->index;
        if (pOut == NULL || cookie == 0 || index >= m_nRecords)
        {
            return E_FAIL;
        }
        // explicit checks
        PCJRecord pCJRec = &m_Records[index];
        if (pCJRec->Pointer == NULL ||  pCJRec->Cookie != cookie)
        {
            return E_FAIL;
        }
        *pOut = pCJRec;
        return S_OK;
    }
};


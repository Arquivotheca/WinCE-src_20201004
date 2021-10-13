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
// Definitions for the WZCStructPtr_t template class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCStructPtr_t_
#define _DEFINED_WZCStructPtr_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a template class representing a reference-counted pointer to
// an object. Unlike SmartPtr or AutoPtr, this object is designed to work
// efficiently with objects containing sub-object pointers. In particular,
// these objects require both shallow and deep copy operations. The former
// copies the object bit-for-bit. Any sub-pointers still refer to the same
// objects. The latter, deep copy operation, copies not only the object
// itself, but all the sub-objects, too.
//
// This class implements shallow copy operations using the built-in
// C-style structure-copy operation. The deep-copy and, consequently,
// the object-deallocation function must be supplied by the template
// parameters:
//
//      T - the object being referenced
//          This is not a pointer. Instead, this is the aggregate object
//          containing sub-pointers.
//
//      CopySig/Fn - deep-copy function signature and pointer
//
//      FreeSig/Fn - object-deallocation function signature and pointer
//
template<class T, class CopySig, CopySig CopyFn,
                  class FreeSig, FreeSig FreeFn>
class WZCStructPtr_t
{
private:

    // Local copy:
    T m_Data;

    // Pointer to a count of references:
    long *m_pRefCount;

public:

    typedef WZCStructPtr_t<T, CopySig, CopyFn, FreeSig, FreeFn> _MyT;

    // Default constructor:
    WZCStructPtr_t(void)
        : m_pRefCount(NULL)
    {
        memset(&m_Data, 0, sizeof(T));
    }

    // Destructor:
   ~WZCStructPtr_t(void)
    {
        if (m_pRefCount)
        {
            if (0 == InterlockedDecrement(m_pRefCount))
            {
                FreeFn(&m_Data);
                LocalFree(m_pRefCount);
            }
            m_pRefCount = NULL;
        }
    }

    // Copy constructors:
    WZCStructPtr_t(const _MyT &Source)
        : m_Data(Source.m_Data),
          m_pRefCount(Source.m_pRefCount)
    {
        if (m_pRefCount)
        {
            InterlockedIncrement(m_pRefCount);
        }
    }
    explicit WZCStructPtr_t(const T &Source)
        : m_Data(Source),
          m_pRefCount(NULL)
    {
        // nothing to do
    }

    // Asignment operators:
    WZCStructPtr_t & operator = (const _MyT &Source)
    {
        if (&Source != this)
        {
            this->~_MyT();
            new (this) _MyT(Source);
        }
        return *this;
    }
    WZCStructPtr_t & operator = (const T &Source)
    {
        if (&Source != &m_Data)
        {
            this->~_MyT();
            new (this) _MyT(Source);
        }
        return *this;
    }

    // Uses the deep-copy operation to make sure this pointer refers to
    // a private copy of the object. Using this method to create a private
    // copy of the object before performing any modifications implements a
    // "copy on write" pattern.
    DWORD Privatize(void)
    {
        DWORD result;
        if (NULL != m_pRefCount && 1 == *m_pRefCount)
        {
            result = ERROR_SUCCESS;
        }
        else
        {
            void *pCount = LocalAlloc(LPTR, sizeof(long));
            if (NULL == pCount)
            {
                result = ERROR_OUTOFMEMORY;
            }
            else
            {
                T copiedData = m_Data;
                result = CopyFn(&copiedData, m_Data);
                if (ERROR_SUCCESS != result)
                {
                    LocalFree(pCount);
                }
                else
                {
                    this->~_MyT();
                    m_Data = copiedData;
                    m_pRefCount = reinterpret_cast<long *>(pCount);
                   *m_pRefCount = 1;
                }
            }
        }
        return result;
    }

    // Accessors:
          T &operator *(void)       { return m_Data; }
    const T &operator *(void) const { return m_Data; }
          T *operator->(void)       { return &m_Data; }
    const T *operator->(void) const { return &m_Data; }
    operator       T *(void)       { return &m_Data; }
    operator const T *(void) const { return &m_Data; }
};

};
};
#endif /* _DEFINED_WZCStructPtr_t_ */
// ----------------------------------------------------------------------------

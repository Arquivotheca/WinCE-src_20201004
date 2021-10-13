//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*
 * Copyright (c) 1999, Microsoft Corporation
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * History:
 *   04/06/1999   ericflee created
 *   05/21/1999   ericflee update const_map to return reference to default value
 */

#ifndef __CONSTMAP_H__
#define __CONSTMAP_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <map>

#ifdef __STL_BEGIN_NAMESPACE
// Version for SGI STL

__STL_BEGIN_NAMESPACE

// ===========================================================================
// class const_map<>
//      Same as map<> except that the operator[] does not insert new map 
//      entries, and it returns a const reference to the second entry of 
//      referenced pair (or a NULL equivelent if not found)
// ===========================================================================
template <class _Key, class _Tp, 
          class _Compare __STL_DEPENDENT_DEFAULT_TMPL(less<_Key>),
          class _Alloc = __STL_DEFAULT_ALLOCATOR(_Tp),
          _Tp _DefaultValue = NULL >
class const_map : public map<_Key, _Tp, _Compare, _Alloc>
{
public:
    typedef map<_Key, _Tp, _Compare, _Alloc> base_class;
    typedef const_map<_Key,_Tp, _Compare,_Alloc,_DefaultValue> this_type;

    const_map() : base_class(), m_DefaultValue(_DefaultValue) {}

    explicit const_map(const _Compare& __comp,
                       const allocator_type& __a = allocator_type())
                       : base_class(__comp, __a), m_DefaultValue(_DefaultValue) { }

#   ifdef __STL_MEMBER_TEMPLATES
    
    template <class _InputIterator>
    const_map(_InputIterator __first, _InputIterator __last)
    : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    template <class _InputIterator>
    const_map(_InputIterator __first, 
              _InputIterator __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
    : base_class(__first, __last, __comp, __a), m_DefaultValue(_DefaultValue) {}

#   else

    const_map(const value_type* __first, const value_type* __last)
        : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    const_map(const value_type* __first,
              const value_type* __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
              : base_class(__first, __last, __comp, __a),
                m_DefaultValue(_DefaultValue)
    {}

    const_map(const_iterator __first, const_iterator __last)
        : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    const_map(const_iterator __first, 
              const_iterator __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
              : base_class(__first, __last, __comp, __a),
                m_DefaultValue(_DefaultValue)
    {}

#   endif /* __STL_MEMBER_TEMPLATES */

    // copy constructor
    const_map(const this_type& __x) 
        : base_class(__x), m_DefaultValue(_DefaultValue) {}

    // assignment
    this_type& operator=(const this_type& __x)
    {
        base_class::operator=(__x);
        return *this; 
    }

    const data_type& operator[](const key_type& __k) const 
    {
        const_iterator __i = lower_bound(__k);
        
        // __i->first is greater than or equivalent to __k.
        if (__i == end() || key_comp()(__k, (*__i).first))
            return m_DefaultValue;

        return (*__i).second;
    }

private:
    data_type m_DefaultValue;
};

// ==========
// static_map
// ==========
template <class TKey, class Ty>
class static_map 
{
public:
    class _elem
    {
    public:
        TKey first;
        Ty second;

        _elem(TKey _first, Ty _second) 
            : first(_first), second(_second) 
        {}
    };

    typedef TKey key_type;
    typedef _elem value_type;
    typedef const value_type* const_iterator;
    typedef static_map<TKey,Ty> this_type;

    static_map(const_iterator _begin, const_iterator _end)
        : m_pBegin(_begin), m_pEnd(_end) { ASSERT(_end>=_begin); }

    static_map(const this_type& sm)
        : m_pBegin(sm.m_pBegin), m_pEnd(sm.m_pEnd) {}

    const_iterator begin() const { return m_pBegin; }
    const_iterator end() const { return m_pEnd; }

    const Ty operator[](key_type key) const
    {
        for(const_iterator itr=begin();
            itr!=end();
            itr++)
            if (itr->first==key)
                return itr->second;        
#       ifndef __STL_DEFAULT_CONSTRUCTOR_BUG
        return Ty();
#       else
        return (Ty) NULL;
#       endif
    }
protected:
    const_iterator m_pBegin, m_pEnd;
};

__STL_END_NAMESPACE

#else
// Version for MS STL

_STD_BEGIN

// ===========================================================================
// class const_map<>
//      Same as map<> except that the operator[] does not insert new map 
//      entries, and it returns a const reference to the second entry of 
//      referenced pair (or a NULL equivelent if not found)
// ===========================================================================
template <class _Key_, class _Tp, 
          class _Compare = less<_Key_>,
          class _Alloc = allocator<_Tp>,
          _Tp _DefaultValue = NULL >
class const_map : public map<_Key_, _Tp, _Compare, _Alloc>
{
public:
    typedef map<_Key_, _Tp, _Compare, _Alloc> base_class;
    typedef const_map<_Key_,_Tp, _Compare,_Alloc,_DefaultValue> this_type;

    const_map() : base_class(), m_DefaultValue(_DefaultValue) {}

    explicit const_map(const _Compare& __comp,
                       const allocator_type& __a = allocator_type())
                       : base_class(__comp, __a), m_DefaultValue(_DefaultValue) { }

#   ifdef __STL_MEMBER_TEMPLATES
    
    template <class _InputIterator>
    const_map(_InputIterator __first, _InputIterator __last)
    : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    template <class _InputIterator>
    const_map(_InputIterator __first, 
              _InputIterator __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
    : base_class(__first, __last, __comp, __a), m_DefaultValue(_DefaultValue) {}

#   else

    const_map(const value_type* __first, const value_type* __last)
        : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    const_map(const value_type* __first,
              const value_type* __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
              : base_class(__first, __last, __comp, __a),
                m_DefaultValue(_DefaultValue)
    {}

    const_map(const_iterator __first, const_iterator __last)
        : base_class(__first, __last), m_DefaultValue(_DefaultValue) {}

    const_map(const_iterator __first, 
              const_iterator __last, 
              const _Compare& __comp,
              const allocator_type& __a = allocator_type())
              : base_class(__first, __last, __comp, __a),
                m_DefaultValue(_DefaultValue)
    {}

#   endif /* __STL_MEMBER_TEMPLATES */

    // copy constructor
    const_map(const this_type& __x) 
        : base_class(__x), m_DefaultValue(_DefaultValue) {}

    // assignment
    this_type& operator=(const this_type& __x)
    {
        base_class::operator=(__x);
        return *this; 
    }

    const _Tp& operator[](const key_type& __k) const 
    {
        const_iterator __i = lower_bound(__k);
        
        // __i->first is greater than or equivalent to __k.
        if (__i == end() || key_comp()(__k, (*__i).first))
            return m_DefaultValue;

        return (*__i).second;
    }

private:
    referent_type m_DefaultValue;
};

// ==========
// static_map
// ==========
template <class TKey, class Ty>
class static_map 
{
public:
    class _elem
    {
    public:
        TKey first;
        Ty second;

        _elem(TKey _first, Ty _second) 
            : first(_first), second(_second) 
        {}
    };

    typedef TKey key_type;
    typedef _elem value_type;
    typedef const value_type* const_iterator;
    typedef static_map<TKey,Ty> this_type;

    static_map(const_iterator _begin, const_iterator _end)
        : m_pBegin(_begin), m_pEnd(_end) { ASSERT(_end>=_begin); }

    static_map(const this_type& sm)
        : m_pBegin(sm.m_pBegin), m_pEnd(sm.m_pEnd) {}

    const_iterator begin() const { return m_pBegin; }
    const_iterator end() const { return m_pEnd; }

    const Ty operator[](key_type key) const
    {
        for(const_iterator itr=begin();
            itr!=end();
            itr++)
            if (itr->first==key)
                return itr->second;        
#       ifndef __STL_DEFAULT_CONSTRUCTOR_BUG
        return Ty();
#       else
        return (Ty) NULL;
#       endif
    }
protected:
    const_iterator m_pBegin, m_pEnd;
};

_STD_END

#endif

#endif

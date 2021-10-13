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
//+----------------------------------------------------------------------------
//
//
// File:
//      TypedDoubleList.h
//
// Contents:
//
//      CTypedDouble list template class declaration
//
//-----------------------------------------------------------------------------


#ifndef __TYPEDDOUBLELIST_H_INCLUDED__
#define __TYPEDDOUBLELIST_H_INCLUDED__

template<class T> class CTypedDoubleList : private CDoubleList
{
public:
    CTypedDoubleList();
    ~CTypedDoubleList();

    bool IsEmpty() const;

    void InsertHead(T *pEntry);
    void InsertTail(T *pEntry);
    void InsertBefore(T *pPosition, T *pEntry);
    void InsertAfter(T *pPosition, T *pEntry);

    T *RemoveHead();
    T *RemoveTail();

    T *prev(T *pEntry) const;
    T *next(T *pEntry) const;

    T *getHead() const;
    T *getTail() const;


    void RemoveEntry(T *pEntry);
};


template<class T> inline CTypedDoubleList<T>::CTypedDoubleList()
{
}

template<class T> CTypedDoubleList<T>::~CTypedDoubleList()
{
}

template<class T> inline bool CTypedDoubleList<T>::IsEmpty() const
{
    return CDoubleList::IsEmpty();
}

template<class T> inline void CTypedDoubleList<T>::InsertHead(T *pEntry)
{
    CDoubleList::InsertHead(static_cast<CDoubleListEntry *>(pEntry));
}

template<class T> inline void CTypedDoubleList<T>::InsertTail(T *pEntry)
{
    CDoubleList::InsertTail(static_cast<CDoubleListEntry *>(pEntry));
}

template<class T> inline void CTypedDoubleList<T>::InsertBefore(T *pPosition, T *pEntry)
{
    CDoubleList::InsertBefore(static_cast<CDoubleListEntry *>(pPosition),
                              static_cast<CDoubleListEntry *>(pEntry));
}

template<class T> inline void CTypedDoubleList<T>::InsertAfter(T *pPosition, T *pEntry)
{
    CDoubleList::InsertAfter(static_cast<CDoubleListEntry *>(pPosition),
                             static_cast<CDoubleListEntry *>(pEntry));
}

template<class T> inline T *CTypedDoubleList<T>::RemoveHead()
{
    return static_cast<T *>(CDoubleList::RemoveHead());
}

template<class T> inline T *CTypedDoubleList<T>::RemoveTail()
{
    return static_cast<T *>(CDoubleList::RemoveTail());
}

template<class T> inline T *CTypedDoubleList<T>::prev(T *pEntry) const
{
    return static_cast<T *>(CDoubleList::prev(pEntry));
}

template<class T> inline T *CTypedDoubleList<T>::next(T *pEntry) const
{
    return static_cast<T *>(CDoubleList::next(pEntry));
}

template<class T> inline T *CTypedDoubleList<T>::getHead() const
{
    return static_cast<T *>(CDoubleList::getHead());
}

template<class T> inline T *CTypedDoubleList<T>::getTail() const
{
    return static_cast<T *>(CDoubleList::getTail());
}
template<class T> inline void CTypedDoubleList<T>::RemoveEntry(T *pEntry)
{
    CDoubleList::RemoveEntry(static_cast<CDoubleListEntry *>(pEntry));
}

#endif //__TYPEDDOUBLELIST_H_INCLUDED__

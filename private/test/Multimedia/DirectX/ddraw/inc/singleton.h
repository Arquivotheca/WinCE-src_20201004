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

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

// external dependencies
// ---------------------
#include <algorithm>
#include <set>
#include <com_utilities.h>

class singleton_base 
    : protected IUnknown // NOTE: IUnknown is for depedency control, NOT for lifetime management. 
                         // Hence the protected inheritence.
{
public:
    virtual ~singleton_base();

    void ReleaseObject();

    void RemoveDependant(IUnknown *piDependant);

protected:
    singleton_base() : m_fState(false), m_lRefCount(0), m_fDestroying(false) {};
    bool EnsureCreated();
    void AddDependant(IUnknown *piDependant);

    long m_lRefCount;
    bool m_fState;
    std::set<IUnknown*> m_setDependants;    

    virtual bool CreateObject() = 0;
    virtual void DestroyObject() = 0;

    void ReleaseDependants();

    virtual ULONG STDMETHODCALLTYPE Release();
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);

private:
    bool m_fDestroying;
};

template <class T>
class singleton : public singleton_base
{
public:
    typedef T object_type;
    typedef singleton<T> this_type;

    singleton() {} 

    const object_type& GetObject()
    {
        EnsureCreated();
        return m_object;
    }

    const object_type& AddDependant(IUnknown *piDependant = NULL) 
    { 
        singleton_base::AddDependant(piDependant);
        return m_object;
    }
    
protected:
    T m_object;
}; // class singleton<>


#endif






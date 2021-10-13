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
// ============================================================================
// Library com_utilities
//
//     Template Library for COM
//
// contains: 
//     class CComPointer<>
//     class CStabilize<>
//     macro POINTER_PRESET
//     macro SafeQI
//     macro SafeQI_
//
// History:
//
//     12/12/1998 ericflee  Created
//      1/24/2000 ericflee  Added op_release and op_addref functor operators
//
// ============================================================================

#pragma once

#ifndef __COM_UTILITIES_H__
#define __COM_UTILITIES_H__

#include <platform_config.h>

// A preset value for pointers (Courtesy Leonardo Blanco)
// Make sure that it is typed to a void*
#undef POINTER_PRESET
// BUGBUG: Return this to normal if the methods ever start clearing out pointers on failure
//#define POINTER_PRESET (reinterpret_cast<void*>(0xca510dad))
//If POINTER_PRESET is changed then in all the if conditions using this macro, a NULL check also should be added.
#define POINTER_PRESET (NULL)

// ——————
// UUIDOF
// ——————
template <class TInterface>
inline REFIID UUIDOF(TInterface **p)
{
    return __uuidof(**p);
}

#define DECLARE_STD_UUIDOF_SPECIALIZATION(INTERFACE) template <> inline REFIID UUIDOF(INTERFACE **p) { return IID_##INTERFACE; }

// ——————————————————————
// A Safer QueryInterface 
// ——————————————————————
#define SafeQI(EXPR) QueryInterface(UUIDOF(EXPR), reinterpret_cast<void**>(EXPR)) 
#define SafeQI_(INTERFACE,EXPR) QueryInterface(__uuidof(**(EXPR)), reinterpret_cast<void**>(static_cast<INTERFACE**>(EXPR)))

namespace com_utilities
{
    // ===================================================
    // class CComPointerBase
    //    base for smart pointer class for COM interfaces.
    //    Automatically handles AddRef and Release
    // ===================================================
    template <class TInterface>
    class CComPointerBase
    {
        friend class CStabilize;
    protected:
        TInterface *m_pi;

        // constructor is hidden to prevent 
        // direct instances of this class
        CComPointerBase() : m_pi(NULL) {}

    public:
        typedef TInterface interface_type;

        // Access Operator (hides AddRef and Release)
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        class _TInterface : public TInterface
        {
        private:
            virtual ULONG STDMETHODCALLTYPE AddRef() = 0;    
            virtual ULONG STDMETHODCALLTYPE Release() = 0;
        };
        
        _TInterface* operator -> () const 
        {
            return static_cast<_TInterface*>(m_pi);
        }

        // ReleaseInterface
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        void ReleaseInterface()
        {
            if (m_pi!=POINTER_PRESET)
                m_pi->Release();
            m_pi = NULL;
        }

        // Destructor
        // ¯¯¯¯¯¯¯¯¯¯
        virtual ~CComPointerBase() 
        { 
            if (m_pi!=POINTER_PRESET) 
                m_pi->Release(); 
        }

        // predicate members
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        class _tag {};

        operator _tag* () const 
        {
            return reinterpret_cast<_tag*>(m_pi);
        }

        operator !() const 
        {
            return NULL == m_pi;
        }

        bool IsAssigned() const
        { 
            // kept for backward compatability
            return m_pi!=POINTER_PRESET; 
        }

        // members for retrieving the interface pointer
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        TInterface* GetInterface(BOOL bAddRef) const
        {  
            if (bAddRef && m_pi!=NULL && m_pi!=POINTER_PRESET)
                m_pi->AddRef();  
            return m_pi;
        }

        TInterface* AsInParam() const
        {
            return GetInterface(FALSE);
        }

        TInterface* AsRightHandSide() const
        {
            return GetInterface(TRUE);
        }

        TInterface** AsOutParam () 
        {
            if (m_pi!=POINTER_PRESET)
            {
                m_pi->Release();
                m_pi=NULL;
            }
            return &m_pi;
        }

        TInterface** AsInOutParam () 
        {
            return &m_pi;
        }

        // Additional members for testing OUT parameters of various interface methods.  
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        TInterface** AsTestedOutParam()
        {
            if (m_pi!=POINTER_PRESET)
            {
                m_pi->Release();
                m_pi=reinterpret_cast<TInterface*>(POINTER_PRESET);
            }
            return &m_pi;
        }

        bool InvalidOutParam()
        {
            if (m_pi==POINTER_PRESET)
            {
                m_pi=NULL;
                return true;
            }
            return false;
        }

    };



    // ===========================================
    // class CComPointer
    //    smart pointer class for COM interfaces.
    //    Automatically handles AddRef and Release
    // ===========================================
    template <class TInterface>
    class CComPointer : public CComPointerBase<TInterface>
    {
        friend class CStabilize;

    public:
        typedef CComPointerBase<TInterface> base_class;
        typedef CComPointer<TInterface> this_type;
        typedef typename base_class::interface_type interface_type;

        // Constructors
        // ¯¯¯¯¯¯¯¯¯¯¯¯
        CComPointer() {}

        CComPointer(TInterface *pi)
        {
            m_pi=pi;
            if (pi!=POINTER_PRESET) m_pi->AddRef();
        }

        explicit CComPointer(IUnknown *punk)
        {
            if (punk!=NULL && punk!=POINTER_PRESET)
            {
                HRESULT hr = punk->SafeQI(&m_pi);
                ASSERT(SUCCEEDED(hr));
            }
            else
                m_pi=NULL;
        }

        explicit CComPointer(void *pv)
        {
            ASSERT(pv==NULL);
            m_pi = static_cast<TInterface*>(pv);
        }

#       if defined(__CFG_MEMBER_TEMPLATES)
            template <class TSomeInterface>
            explicit CComPointer(const CComPointer<TSomeInterface>& pi) 
            {
                if (!pi)
                {
                    HRESULT hr = pi->SafeQI(&m_pi);
                    ASSERT(SUCCEEDED(hr));
                }
                else
                    m_pi=NULL;
            }

#           if defined(__CFG_TEMPLATE_SPECIALIZATION_SYNTAX)
                // specialization for copy construction
//                template <>
                CComPointer(const this_type& pi) 
                {
                    m_pi=pi.m_pi;
                    if (m_pi!=NULL && m_pi!=POINTER_PRESET) m_pi->AddRef();
                }

#           endif
#       else
            CComPointer(const this_type& pi) 
            {
                m_pi=pi.m_pi;
                if (m_pi!=POINTER_PRESET) m_pi->AddRef();
            }
#       endif

        // Assignment Operators
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

        this_type& operator = (interface_type *pi)
        {
            interface_type *tmp = m_pi;
            m_pi = pi;
            if (m_pi!=POINTER_PRESET) m_pi->AddRef();
            if (tmp!=POINTER_PRESET) tmp->Release();
            return *this;
        }

#       if defined(__CFG_MEMBER_TEMPLATES)

            template <class TSomeInterface>
            this_type& Assign(const CComPointer<TSomeInterface>& pi)
            {
                TInterface tmp = m_pi;
                if (pi!=NULL && pi!=POINTER_PRESET)
                {
                    HRESULT hr = pi->SafeQI(&m_pi);
                    ASSERT(SUCCEEDED(hr));
                }
                else m_pi = NULL;
                if (tmp!=NULL && tmp!=POINTER_PRESET) tmp->Release();
                return *this;
            }

            template <class TSomeInterface>
            this_type& Assign(TSomeInterface *pi)
            {
                TInterface tmp = m_pi;
                if (pi!=NULL && pi!=POINTER_PRESET)
                {
                    HRESULT hr = pi->SafeQI(&m_pi);
                    ASSERT(SUCCEEDED(hr));
                }
                else m_pi = NULL;
                if (tmp!=NULL && tmp!=POINTER_PRESET) tmp->Release();
                return *this;
            }

#           if defined(__CFG_TEMPLATE_SPECIALIZATION_SYNTAX)

                template <>
                this_type& Assign(const this_type& pi)
                {
                    interface_type* tmp = m_pi;
                    m_pi = pi.m_pi;
                    if (m_pi!=NULL && m_pi!=POINTER_PRESET) m_pi->AddRef();
                    if (tmp!=NULL && tmp!=POINTER_PRESET) tmp->Release();
                    return *this;
                }

                template <>
                this_type& Assign(interface_type *pi)
                {
                    interface_type* tmp = m_pi;
                    m_pi = pi;
                    if (m_pi!=NULL && m_pi!=POINTER_PRESET) m_pi->AddRef();
                    if (tmp!=NULL && tmp!=POINTER_PRESET) tmp->Release();
                    return *this;
                }

#           endif 

            template <class TSomeInterface>
            inline this_type& operator = (const CComPointer<TSomeInterface>& pi) 
            {
                Debug(LOG_COMMENT, TEXT("%s\n"), FILELOC);

                return Assign(pi);
            }

#           if defined(__CFG_TEMPLATE_SPECIALIZATION_SYNTAX)

//                template <>
                inline this_type& operator = (const this_type& pi) 
                {
                    return Assign(pi);
                }

#           endif 

#       else
            // Work around for when member templates are not available
            // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
            this_type& operator = (const this_type& pi) 
            {
                interface_type *tmp = m_pi;
                m_pi = pi.m_pi;
                if (m_pi!=POINTER_PRESET) m_pi->AddRef();
                if (tmp!=POINTER_PRESET) tmp->Release();
                return *this;
            }
#       endif
       
    };


    // ===========================================
    // class CComPointer<IUnknown>
    //    smart pointer class for COM interfaces.
    //    Automatically handles AddRef and Release
    // ===========================================
    template <>
    class CComPointer<IUnknown> : public CComPointerBase<IUnknown>
    {
        friend class CStabilize;

    public:
        
        // —————————————————————————————————————
        // Constructors and Assignment Operators
        // —————————————————————————————————————
        CComPointer() { } 

        explicit CComPointer(IUnknown *pi)
        {
            m_pi = pi;
            if (m_pi!=POINTER_PRESET) m_pi->AddRef();
        }

        explicit CComPointer(void *pv)
        {
            ASSERT(pv==NULL);
            m_pi = static_cast<IUnknown*>(pv);
        }

        CComPointer<IUnknown>& operator = (IUnknown *pi)
        {
            IUnknown *punk = m_pi;
            m_pi = pi;
            if (pi!=POINTER_PRESET) m_pi->AddRef();
            if (punk!=POINTER_PRESET) punk->Release();

            return *this;
        }

#       if defined(__CFG_MEMBER_TEMPLATES)
            template <class TSomeInterface>
            explicit CComPointer(const CComPointer<TSomeInterface>& pi) 
            {
                m_pi = pi->m_pi;

                if (pi!=NULL && pi!=POINTER_PRESET)
                    m_pi->AddRef();
            }

            template <class TSomeInterface>
            CComPointer<IUnknown>& operator = (const CComPointer<TSomeInterface>& pi) 
            {
                IUnknown *tmp = m_pi;
                m_pi = pi->m_pi;
                if (pi!=NULL && pi!=POINTER_PRESET) m_pi->AddRef();
                if (tmp!=NULL && tmp!=POINTER_PRESET) tmp->Release();
                return *this;
            }
#       else
            explicit CComPointer(const CComPointer<IUnknown>& pi) 
            {
                m_pi = pi.m_pi;
                if (m_pi!=POINTER_PRESET) m_pi->AddRef();
            }

            CComPointer<IUnknown>& operator = (const CComPointer<IUnknown>& pi) 
            {
                IUnknown *tmp = m_pi;
                m_pi = pi.m_pi;
                if (m_pi!=POINTER_PRESET) m_pi->AddRef();
                if (tmp!=POINTER_PRESET) tmp->Release();
                return *this;
            }
#       endif

    
    };


    // ================
    // class CStabilize
    // ================
    class CStabilize
    {
    protected:
        IUnknown *m_punk;
    public:
        CStabilize(IUnknown *punk) 
        : m_punk(punk)   
        { 
            if (m_punk) 
                m_punk->AddRef(); 
        }
    
        ~CStabilize() 
        { 
            if (m_punk) 
                m_punk->Release(); 
        }

# if defined(__CFG_MEMBER_TEMPLATES)
        template <class TInterface>  
        CStabilize(CComPointer<TInterface>& si) 
        : m_punk(si.m_pi)   
        { 
            if (m_punk) 
                m_punk->AddRef();  
        }
# else
        CStabilize(CComPointer<IUnknown>& si) 
        : m_punk(si.m_pi)   
        { 
            if (m_punk) 
                m_punk->AddRef();  
        }
# endif
    };


    template <class TInterface>
    class IUnknownImpl : public TInterface
    {
    public:
        IUnknownImpl() : m_lRefCount(1) {}
        virtual ~IUnknownImpl() { }

        virtual ULONG STDMETHODCALLTYPE AddRef() 
        { 
            ASSERT(m_lRefCount>0);
            return InterlockedIncrement(&m_lRefCount); 
        }

        virtual ULONG STDMETHODCALLTYPE Release() 
        {
            ASSERT(m_lRefCount>0);
            ULONG result = InterlockedDecrement(&m_lRefCount);
            if (!result)
                delete this;
            return result;
        }

    protected:
        long m_lRefCount;
    };
    

    // ==========================================
    // Functor operators op_release and op_addref 
    //    useful for STL algorithms like for_each
    // ==========================================
    class op_release
    {
    public:   
        void operator() (IUnknown* pi) 
        { 
            if (pi!=POINTER_PRESET) pi->Release(); 
        }

#       if defined(__CFG_MEMBER_TEMPLATES)
        template <class TInterface>
        void operator() (CComPointer<TInterface>& pi) 
        { 
            pi.ReleaseInterface();
        }
#       endif
    };

    class op_addref
    {
    public:   
        void operator() (IUnknown* pi) 
        { 
            if (pi!=POINTER_PRESET) pi->AddRef(); 
        }

#       if defined(__CFG_MEMBER_TEMPLATES)
        template <class TInterface>
        void operator() (CComPointer<TInterface>& pi) 
        { 
            pi.GetInterface(true);
        }
#       endif
    };

} // com_utilities


// ——————————————
// Interface Name
// ——————————————
#define STD_INTERFACE_NAME(INTERFACE) \
    inline LPCTSTR InterfaceName(INTERFACE* p) { return TEXT(#INTERFACE) ; } \
    inline LPCTSTR InterfaceName(const com_utilities::CComPointer<INTERFACE>& pi) { return TEXT(#INTERFACE) ; }

#endif

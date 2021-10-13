//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <psl_marshaler.hxx>

#ifndef __PSL_MARSHALER_ALLOCATOR__
#define __PSL_MARSHALER_ALLOCATOR__

//
// Allocator that can be used with ce::psl_proxy and ce::psl_stub to allow server allocate memory
// that can be returned to the client
//

#if 0
/**/
/**/    ///////////////////////////////////////////////////////
/**/    // 1. 
/**/    
/**/    
#endif

namespace ce
{

struct psl_allocator_extra_info
{
    LPVOID  m_pCallbackContext;
    FARPROC m_pfnAllocateCallback;
    FARPROC m_pfnDeallocateCallback;
};

//
// psl_allocator
//
// psl_allocator can be used to allocate memory that can be used by both PSL client and server:
//  - memory allocated by the client can be passed to the server (via psl_proxy/psl_stub) 
//    which can use it and/or free it.
//  - memory allocated by the server can be returned to the client (via psl_proxy/psl_stub)
//    which can use it and/or free it.
//
template <typename _Al = ce::allocator>
class psl_allocator : public psl_allocator_extra_info,
                      protected _Al
{
public:    
    typedef psl_allocator<_Al>					_Myt;
    typedef psl_stub<psl_allocator_extra_info*> psl_stub;
    
    psl_allocator()
    {
        m_pCallbackContext = this;
        m_pfnAllocateCallback = reinterpret_cast<FARPROC>(alloc_callback);
        m_pfnDeallocateCallback = reinterpret_cast<FARPROC>(free_callback);
    }
    
    explicit psl_allocator(const _Al& _Alloc)
        : _Al(_Alloc)
    {
        m_pCallbackContext = this;
        m_pfnAllocateCallback = reinterpret_cast<FARPROC>(alloc_callback);
        m_pfnDeallocateCallback = reinterpret_cast<FARPROC>(free_callback);
    }
    
    //
    // Memory operation during a PSL call (when there is an instance of psl_stub on the current thread)
    // are delegated to the calling (client) process.
    // This way memory allocated by a PSL server is owned by the client process which can use/free it after 
    // PSL call returns.
    //
    void* allocate(size_t size) const
    {
        if(const psl_stub* pStub = psl_stub::get_thread_stub())
        {
            psl_allocator_extra_info* pInfo = NULL;
            
            if(pStub->extra_info(pInfo) &&
               pInfo && 
               pInfo->m_pfnDeallocateCallback &&
               pInfo->m_pCallbackContext)
            {
                CALLBACKINFO cbi;
            
                cbi.hProc   = GetCallerProcess();
                cbi.pfn     = pInfo->m_pfnAllocateCallback;
                cbi.pvArg0  = pInfo->m_pCallbackContext;
                
                void* p = NULL;
                
                __try
                {
                    PerformCallBack(&cbi, size, &p);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                }
                
                return MapPtrToProcWithSize(p, size, cbi.hProc);
            }
            else
                return NULL;
        }
        else
            return _Al::allocate(size);
    }
        
    void deallocate(void* ptr, size_t size) const
    {
        if(const psl_stub* pStub = psl_stub::get_thread_stub())
        {
            psl_allocator_extra_info* pInfo = NULL;
            
            if(pStub->extra_info(pInfo) && 
               pInfo && 
               pInfo->m_pfnDeallocateCallback &&
               pInfo->m_pCallbackContext)
            {
                CALLBACKINFO cbi;
            
                cbi.hProc   = GetCallerProcess();
                cbi.pfn     = pInfo->m_pfnDeallocateCallback;
                cbi.pvArg0  = pInfo->m_pCallbackContext;
                
                __try
                {
                    PerformCallBack(&cbi, ptr, size);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
        }
        else
            _Al::deallocate((void*)UnMapPtr(ptr), size);
    }

protected:
    static void alloc_callback(_Myt* _this, size_t size, void** pp)
    {
        assert(_this);
        __assume(_this);
        assert(pp);
        __assume(pp);
        
        *pp = _this->_Al::allocate(size);
    }
    
    static void free_callback(_Myt* _this, void* ptr, size_t size)
    {
        assert(_this);
        __assume(_this);
        
        _this->_Al::deallocate(ptr, size);
    }
};

}; // namespace ce

#endif // __PSL_MARSHALER_ALLOCATOR__

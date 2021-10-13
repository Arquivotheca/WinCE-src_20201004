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
// File:
//      Utility.h
//
// Contents:
//
//      Various utilities
//
//-----------------------------------------------------------------------------

#ifndef __UTILITY_H_INCLUDED__
#define __UTILITY_H_INCLUDED__

//////////////////////////////////////////////////
// Various constants

#define INITIAL_REFERENCE   (1)
 
#ifndef ONEK
#define ONEK 1024
#endif

#ifndef FOURK
#define FOURK 4096
#endif

//////////////////////////////////////////////////
// wsprintf isn't supported on Windows 9x, raise compile time error

#ifdef wsprintf
#undef wsprintf
#endif

#define wsprintf __WIN9X_DOESN_T_SUPPORT_WSPRINTF__

//////////////////////////////////////////////////
// Macros

#ifndef UNDER_CE
#define offsetof(c,m) ( reinterpret_cast<size_t>(&(reinterpret_cast<c*>(0))->m) )
#endif
#define countof(a) (sizeof(a)/sizeof(a[0]))
#define maxindex(a) (countof(a) - 1)
#define char_array_size(a) (sizeof(a) - sizeof(a[0]))

#define STRING(x) #x
#define REGKEY(x) "{" STRING(x) "}"

#define MIN(a, b)  ((a <= b) ? a : b )

//////////////////////////////////////////////////
// Object counter

#define OBJECT_CREATED() { ::AtomicIncrement(reinterpret_cast<LPLONG>(&g_cObjects)); TRACEL((10, "Creating Object (%i)\n", g_cObjects)); }
#define OBJECT_DELETED() { ::AtomicDecrement(reinterpret_cast<LPLONG>(&g_cObjects)); TRACEL((10, "Deleting Object (%i)\n", g_cObjects)); }

//////////////////////////////////////////////////
// Error handling

#define CHK(err) { hr = (err); if (FAILED(hr)) { goto Cleanup; } }
#define CHK_ARG(arg) { if (!(arg)) return E_INVALIDARG; }
#define CHK_MEM(ptr) CHK_BOOL((ptr) != NULL, E_OUTOFMEMORY)
#define CHK_BOOL(cond, err) { if (!(cond)) { hr = (err); goto Cleanup; } }
#define SETHR_BOOL(cond, err) { if (!(cond)) {hr = (err);} }

//////////////////////////////////////////////////
// Interface Riders

struct InterfaceRiderElement
{
    const IID *pIID;
    DWORD offs;
};

struct InterfaceRider
{
    LONG elc;
    const InterfaceRiderElement *elv;
};

#define NULL_POINTER 8
#define class_offset(class, base) \
    (reinterpret_cast<DWORD>(static_cast<base*>(reinterpret_cast<class*>(NULL_POINTER))) - NULL_POINTER)

#define DECLARE_INTERFACE_MAP \
protected: \
    static const InterfaceRiderElement s_InterfaceRiderElements[]; \
    static const InterfaceRider        s_InterfaceRider

#define BEGIN_INTERFACE_MAP(class) \
const InterfaceRiderElement class::s_InterfaceRiderElements[] = {

#define ADD_IUNKNOWN(class, interface) \
{ &__uuidof(IUnknown), class_offset(class, interface) },

#define ADD_INTERFACE(class, interface) \
{ & __uuidof(interface), class_offset(class, interface) },

#define END_INTERFACE_MAP(class) \
}; \
const InterfaceRider class::s_InterfaceRider = \
{ countof(class::s_InterfaceRiderElements), class::s_InterfaceRiderElements };

HRESULT QueryInterfaceRider(const InterfaceRider *pRider, void *pvThis, REFIID iid, void **ppvObject);

#define QUERYINTERFACE(iid, ppvObject) \
    ::QueryInterfaceRider(& s_InterfaceRider, static_cast<void*>(static_cast<T *>(this)), iid, ppvObject)

//////////////////////////////////////////////////
// Class creation support

struct FactoryProduct
{
    const CLSID *pCLSID;
    HRESULT (WINAPI *pCtorA)(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
    HRESULT (WINAPI *pCtorN)(IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
};

struct FactoryRider
{
    LONG elc;
    const FactoryProduct *elv;
};

#define DECLARE_FACTORY_MAP \
    static const FactoryProduct s_FactoryProducts[]; \
    static const FactoryRider   s_FactoryRider
#define BEGIN_FACTORY_MAP(class) \
const FactoryProduct class::s_FactoryProducts[] = {

#define ADD_FACTORY_PRODUCT(clsid, class) \
{ &clsid, CSoapAggObject< class >::CreateObject, CSoapObject< class >::CreateObject },

#define ADD_FACTORY_PRODUCT_FTM(clsid, class) \
{ &clsid, CSoapAggObjectFtm< class >::CreateObject, CSoapObjectFtm< class >::CreateObject },

#define END_FACTORY_MAP(class) \
};\
const FactoryRider class::s_FactoryRider = \
{ countof(class::s_FactoryProducts), class::s_FactoryProducts };

HRESULT FindFactoryProduct(REFCLSID rclsid, const FactoryRider *pFactoryRider, const FactoryProduct **ppFactProd);

#define FIND_FACTORY_PRODUCT(rclsid, ppFactProd) \
    ::FindFactoryProduct(rclsid, &s_FactoryRider, ppFactProd)

//////////////////////////////////////////////////
// string conversion macro

#define CONVERT_WIDECHAR_TO_MULTIBYTE_(cp, what, where) \
{ \
	int size = ::WideCharToMultiByte(cp, 0, what, ::SysStringLen(what), 0, 0, 0, 0); \
    if(! size) \
    { \
        hr = E_INVALIDARG; \
        goto Cleanup; \
    } \
    where = reinterpret_cast<LPSTR>(_alloca(size + 1)); \
    ASSERT(where); \
    ::WideCharToMultiByte(cp, 0, what, ::SysStringLen(what), where, size, 0, 0); \
    where[size] = 0; \
}

#define CONVERT_WIDECHAR_TO_MULTIBYTE(what, where) CONVERT_WIDECHAR_TO_MULTIBYTE_(CP_ACP, what, where)
#define CONVERT_WIDECHAR_TO_UTF8(what, where)      \
{ \
	UINT uiCodePage = CP_UTF8; \
	int size; \
	if(NULL == what) { \
	    hr = E_INVALIDARG; \
	    goto Cleanup; \
	} \
	if(0 != wcslen(what)) { \
    	size = ::WideCharToMultiByte(uiCodePage, 0, what, ::SysStringLen(what), 0, 0, 0, 0); \
    	if(!size) \
    	{ \
    		uiCodePage = CP_ACP; \
    		size = ::WideCharToMultiByte(uiCodePage, 0, what, ::SysStringLen(what), 0, 0, 0, 0); \
    	} \
    	if(!size) \
    	{ \
    	    hr = E_INVALIDARG; \
    	    goto Cleanup; \
    	} \
	} else { \
	    size = 0; \
    } \
	where = reinterpret_cast<LPSTR>(_alloca(size + 1)); \
    ASSERT(where); \
    ::WideCharToMultiByte(uiCodePage, 0, what, ::SysStringLen(what), where, size, 0, 0); \
    where[size] = 0; \
}
		
//////////////////////////////////////////////////
// generic helper function to take a source and save a copy of it in target

HRESULT allocateAndCopy(WCHAR ** target, const WCHAR * source);

//////////////////////////////////////////////////
// BSTR helper functions

HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc);
HRESULT FreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length);
HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc);
HRESULT CopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc, int length);
HRESULT CatBSTRs(BSTR &pbstrDst, const WCHAR *pStr1, int len1, const WCHAR *pStr2, int len2);
HRESULT FreeAndStoreBSTR(BSTR &bstrDst, const BSTR bstrSrc);
HRESULT AtomicFreeAndCopyBSTR(BSTR &pbstrDst, const WCHAR *pszSrc);

//////////////////////////////////////////////////
// formating helper class
class CAutoFormat
{
public:
    CAutoFormat()
    {
        m_pchBuffer = 0;
        m_ulSize = 0; 
    }
    ~CAutoFormat()
    {
        delete [] m_pchBuffer;
    }

    HRESULT sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne);
    HRESULT sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo);    
    HRESULT sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo, const TCHAR *pchPartThree);        
    HRESULT sprintf(const TCHAR *pchFormatString, const TCHAR *pchPartOne, const TCHAR *pchPartTwo, const TCHAR *pchPartThree, const TCHAR *pchPartFour);        
    HRESULT copy(const TCHAR *pchSource);

    inline TCHAR * CAutoFormat::operator &(void)
    {
        return m_pchBuffer;
    }

protected:
    HRESULT verifyBuffer(ULONG ulNewSize);
    
    TCHAR * m_pchBuffer;
    ULONG   m_ulSize; 
};


#if defined(_M_IX86) && !defined(UNDER_CE)
LONG __stdcall AtomicDecrement(LPLONG lpAddend);
#else
inline LONG AtomicIncrement(LPLONG lpAddend)
{
    return InterlockedIncrement(lpAddend);
}

inline LONG AtomicDecrement(LPLONG lpAddend)
{
    return InterlockedDecrement(lpAddend);
}
#endif

HRESULT InitSoaputil(void);

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T *AssignInterface(T **ppD, T *pS)
//
//  parameters:
//          
//  description:
//          Performs interface pointer assignment with necessary AddRef/Release
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T *AssignInterface(T **ppD, T *pS)
{
    if(pS) pS->AddRef();
    if(*ppD) (*ppD)->Release();
    return (*ppD) = pS;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline ULONG ReleaseInterface(const IUnknown *& pUnk)
//
//  parameters:
//          
//  description:
//          Safe release of an interface pointer
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> inline ULONG ReleaseInterface(const T *& ppObj)
{
    IUnknown *pUnknown = reinterpret_cast<IUnknown *>(InterlockedExchangePointer(&ppObj, 0));

    if(pUnknown)
    {
        return pUnknown->Release();
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T> void release(T ** ppUnk)
{
    T * pUnk = *ppUnk;
    if (pUnk)
    {
        *ppUnk = NULL;
        pUnk->Release();
    }
}

template <class T> void assign(T ** ppUnkDest, T *pUnkSrc)
{
    if (ppUnkDest && pUnkSrc)
    {
        *ppUnkDest = pUnkSrc;
        pUnkSrc->AddRef();
    }
}

inline void free_bstr(BSTR &p)
{
    if (NULL != p) 
    {
        SysFreeString(p); 
        p = NULL;
    }
}

#endif //__UTILITY_H_INCLUDED__


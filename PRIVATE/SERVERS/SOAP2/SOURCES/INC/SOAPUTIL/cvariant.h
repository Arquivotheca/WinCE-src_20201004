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
// File:    cvariant.h
// 
// Contents:
//
//  implementation file 
//
//		CVariant implementation  
//	
//
//-----------------------------------------------------------------------------
#ifndef _CVARIANT_H_
#define _CVARIANT_H_
        

#define LCID_TOUSE (MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT))

class CVariant : public VARIANT
{
public:
    // Constructors
    //
    CVariant();

    // Destructor
    //
    ~CVariant();

    // Assignment operations
    //
    HRESULT Assign(VARIANT* pSrc, bool fCopy = true);

    HRESULT Assign(short sSrc);				// Assign a VT_I2, or a VT_BOOL
    HRESULT Assign(long lSrc);				// Assign a VT_I4, a VT_ERROR or a VT_BOOL
    HRESULT Assign(float fltSrc);				// Assign a VT_R4
    HRESULT Assign(double dblSrc);			// Assign a VT_R8, or a VT_DATE
    HRESULT Assign(const CY& cySrc);			// Assign a VT_CY
    HRESULT Assign(const WCHAR * bstrSrc, bool bCopy = true);	// Assign a VT_BSTR
//  HRESULT Assign(const wchar_t* pSrc);		// Assign a VT_BSTR
    HRESULT Assign(const char* pSrc);			// Assign a VT_BSTR
    HRESULT Assign(IDispatch* pSrc, bool fAddRef = true);
    HRESULT Assign(IUnknown* pSrc, bool fAddRef = true);
    HRESULT Assign(bool bSrc);				// Assign a VT_BOOL
    HRESULT Assign(IUnknown* pSrc);			// Assign a VT_UNKNOWN
    HRESULT Assign(const DECIMAL& decSrc);	// Assign a VT_DECIMAL
    HRESULT Assign(BYTE bSrc);				// Assign a VT_UI1
    HRESULT Assign(LONGLONG llSrc);			// Assign a VT_I8
    HRESULT Assign(ULONGLONG ullSrc);			// Assign a VT_UI8
    
    // Comparison operations
    //
    //bool operator==(const VARIANT& varSrc) const;
    bool operator==(const VARIANT* pSrc) const;

    bool operator!=(const VARIANT& varSrc) const;
    bool operator!=(const VARIANT* pSrc) const;
    operator VARIANT() {return *this;};
    operator VARIANT*() {return this;};

    // Low-level operations
    //
    HRESULT Clear();
    
    HRESULT Attach(VARIANT& varSrc);
    VARIANT Detach();

    HRESULT ChangeType(VARTYPE vartype, const CVariant* pSrc = NULL);
    
private:
    HRESULT CopyVariant(const VARIANT* varSrc);
    
};

//////////////////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////////////////////
inline CVariant::CVariant()
{
    ::VariantInit(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Assignment operations
//
//////////////////////////////////////////////////////////////////////////////////////////

// Construct a CVARIANT from a VARIANT*.  If fCopy is FALSE, give control of
// data to the CVARIANT without doing a VariantCopy.
//
inline HRESULT CVariant::Assign(VARIANT* pSrc, bool fCopy)
{
    HRESULT hr = S_OK;
    
    CHK(Clear());
    if (fCopy) 
    {
        CHK (CopyVariant(pSrc));
    }
    else 
    {
        memcpy(this, pSrc, sizeof(VARIANT));
        V_VT(pSrc) = VT_EMPTY;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}


// Assign a short creating either VT_I2 VARIANT or a
// VT_BOOL VARIANT (VT_I2 is the default)
//
inline HRESULT CVariant::Assign(short sSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) == VT_I2) {
        V_I2(this) = sSrc;
    }
    else if (V_VT(this) == VT_BOOL) {
        V_BOOL(this) = (sSrc ? VARIANT_TRUE : VARIANT_FALSE);
    }
    else {
        // Clear the VARIANT and create a VT_I2
        //
        CHK (Clear());

        V_VT(this) = VT_I2;
        V_I2(this) = sSrc;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a long creating either VT_I4 VARIANT, a VT_ERROR VARIANT
// or a VT_BOOL VARIANT (VT_I4 is the default)
//
inline HRESULT CVariant::Assign(long lSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) == VT_I4) {
        V_I4(this) = lSrc;
    }
    else if (V_VT(this) == VT_ERROR) {
        V_ERROR(this) = lSrc;
    }
    else if (V_VT(this) == VT_BOOL) {
        V_BOOL(this) = (lSrc ? VARIANT_TRUE : VARIANT_FALSE);
    }
    else {
        // Clear the VARIANT and create a VT_I4
        //
        CHK (Clear());

        V_VT(this) = VT_I4;
        V_I4(this) = lSrc;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a float creating a VT_R4 VARIANT
//
inline HRESULT CVariant::Assign(float fltSrc) 
{
    HRESULT hr = S_OK;
  
    if (V_VT(this) != VT_R4) {
        // Clear the VARIANT and create a VT_R4
        //
        CHK (Clear());

        V_VT(this) = VT_R4;
    }

    V_R4(this) = fltSrc;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a double creating either a VT_R8 VARIANT, or a VT_DATE
// VARIANT (VT_R8 is the default)
//
inline HRESULT CVariant::Assign(double dblSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) == VT_R8) {
        V_R8(this) = dblSrc;
    }
    else if(V_VT(this) == VT_DATE) {
        V_DATE(this) = dblSrc;
    }
    else {
        // Clear the VARIANT and create a VT_R8
        //
        CHK(Clear());

        V_VT(this) = VT_R8;
        V_R8(this) = dblSrc;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a CY creating a VT_CY VARIANT
//
inline HRESULT CVariant::Assign(const CY& cySrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_CY) {
        // Clear the VARIANT and create a VT_CY
        //
        CHK(Clear());

        V_VT(this) = VT_CY;
    }

    V_CY(this) = cySrc;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a const _bstr_t& creating a VT_BSTR VARIANT
//
/*
inline HRESULT CVariant::Assign(const BSTR& bstrSrc, bool fCopy) 
{
    HRESULT hr = S_OK;
    
    // Clear the VARIANT (This will SysFreeString() any previous occupant)
    //
    CHK(Clear());

    if (!bstrSrc) 
    {
        V_VT(this) = VT_BSTR;
        V_BSTR(this) = NULL;
    }
    else
    {
        BSTR bstr = static_cast<wchar_t*>(bstrSrc);
        wchar_t*tmp;

        if (fCopy)          // check if we have to copy
            tmp = ::SysAllocString(pSrc);
        else
            tmp = bstrSrc;          // or want to claim ownership

        CHK_BOOL (tmp != NULL, E_OUTOFMEMORY);
        V_VT(this) = VT_BSTR;
  		V_BSTR(this) = tmp;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}
*/

// Assign a const wchar_t* creating a VT_BSTR VARIANT
//
inline HRESULT CVariant::Assign(const WCHAR* pSrc, bool bCopy) 
{
    HRESULT hr = S_OK;
    
    // Clear the VARIANT (This will SysFreeString() any previous occupant)
    //
    CHK(Clear());

    if (pSrc == NULL) {
        V_VT(this) = VT_BSTR;
        V_BSTR(this) = NULL;
    }
    else {
        BSTR temp = NULL;
        
        if (bCopy)
        {
            temp = ::SysAllocString(pSrc);
        	CHK_BOOL(temp != NULL, E_OUTOFMEMORY);
        }
        else
        {
            //we better are sure we got a BSTR passed it !
            temp = (BSTR) pSrc;
        }
        V_VT(this) = VT_BSTR;
        V_BSTR(this) = temp;
    }

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}


// Construct a VT_DISPATCH VARIANT from an IDispatch*
//
inline HRESULT CVariant::Assign(IDispatch* pSrc, bool fAddRef)
{
    HRESULT hr = S_OK;

    CHK (Clear());
    
    V_VT(this) = VT_DISPATCH;
    V_DISPATCH(this) = pSrc;

    // Need the AddRef() as VariantClear() calls Release(), unless fAddRef
    // false indicates we're taking ownership
    //
    if (fAddRef) {
        V_DISPATCH(this)->AddRef();
    }
Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Construct a VT_UNKNOWN VARIANT from an IUnknown*
//
inline HRESULT CVariant::Assign(IUnknown* pSrc, bool fAddRef)  
#ifndef UNDER_CE 
    throw()
#endif
{
    HRESULT hr = S_OK;

    CHK (Clear());
    
    V_VT(this) = VT_UNKNOWN;
    V_UNKNOWN(this) = pSrc;

    // Need the AddRef() as VariantClear() calls Release(), unless fAddRef
    // false indicates we're taking ownership
    //
    if (fAddRef) {
        V_UNKNOWN(this)->AddRef();
    }
Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}


// Assign a bool creating a VT_BOOL VARIANT
//
inline HRESULT CVariant::Assign(bool bSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_BOOL) {
        // Clear the VARIANT and create a VT_BOOL
        //
        CHK(Clear());

        V_VT(this) = VT_BOOL;
    }

    V_BOOL(this) = (bSrc ? VARIANT_TRUE : VARIANT_FALSE);

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign an IUnknown* creating a VT_UNKNOWN VARIANT
//
inline HRESULT CVariant::Assign(IUnknown* pSrc) 
{
    HRESULT hr = S_OK;
    
    // Clear VARIANT (This will Release() any previous occupant)
    //
    CHK(Clear());

    V_VT(this) = VT_UNKNOWN;
    V_UNKNOWN(this) = pSrc;

    // Need the AddRef() as VariantCHK(Clear()) calls Release()
    //
    V_UNKNOWN(this)->AddRef();

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a DECIMAL creating a VT_DECIMAL VARIANT
//
inline HRESULT CVariant::Assign(const DECIMAL& decSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_DECIMAL) {
        // Clear the VARIANT
        //
        CHK(Clear());
    }

    // Order is important here! Setting V_DECIMAL wipes out the entire VARIANT
    V_DECIMAL(this) = decSrc;
    V_VT(this) = VT_DECIMAL;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a BTYE (unsigned char) creating a VT_UI1 VARIANT
//
inline HRESULT CVariant::Assign(BYTE bSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_UI1) {
        // Clear the VARIANT and create a VT_UI1
        //
        CHK(Clear());

        V_VT(this) = VT_UI1;
    }

    V_UI1(this) = bSrc;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a LONGLONG creating a VT_I8 VARIANT
//
inline HRESULT CVariant::Assign(LONGLONG llSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_I8) {
        // Clear the VARIANT and create a VT_I8
        //
        CHK(Clear());

        V_VT(this) = VT_I8;
    }

    V_I8(this) = llSrc;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

// Assign a ULONGLONG creating a VT_UI8 VARIANT
//
inline HRESULT CVariant::Assign(ULONGLONG ullSrc) 
{
    HRESULT hr = S_OK;
    
    if (V_VT(this) != VT_UI8) {
        // Clear the VARIANT and create a VT_UI8
        //
        CHK(Clear());

        V_VT(this) = VT_UI8;
    }

    V_UI8(this) = ullSrc;

Cleanup:
    ASSERT(hr == S_OK);
    return hr;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Comparison operations
//
//////////////////////////////////////////////////////////////////////////////////////////
/*
// Compare a CVariant against a const VARIANT& for equality
//
inline bool CVariant::operator==(const VARIANT& varSrc) const
{
    return *this == &varSrc;
}
*/
// Compare a CVariant against a const VARIANT* for equality
//
inline bool CVariant::operator==(const VARIANT* pSrc) const
{
    if (this == pSrc) {
        return true;
    }

    //
    // Variants not equal if types don't match
    //
    if (V_VT(this) != V_VT(pSrc)) {
        return false;
    }

    //
    // Check type specific values
    //
    switch (V_VT(this)) {
        case VT_EMPTY:
        case VT_NULL:
        	return true;

        case VT_I2:
        	return V_I2(this) == V_I2(pSrc);

        case VT_I4:
        	return V_I4(this) == V_I4(pSrc);

        case VT_I8:
        	return V_I8(this) == V_I8(pSrc);

        case VT_R4:
        	return V_R4(this) == V_R4(pSrc);

        case VT_R8:
        	return V_R8(this) == V_R8(pSrc);

        case VT_CY:
        	return memcmp(&(V_CY(this)), &(V_CY(pSrc)), sizeof(CY)) == 0;

        case VT_DATE:
        	return V_DATE(this) == V_DATE(pSrc);

        case VT_BSTR:
        	return (::SysStringByteLen(V_BSTR(this)) == ::SysStringByteLen(V_BSTR(pSrc))) &&
        			(memcmp(V_BSTR(this), V_BSTR(pSrc), ::SysStringByteLen(V_BSTR(this))) == 0);

        case VT_DISPATCH:
        	return V_DISPATCH(this) == V_DISPATCH(pSrc);

        case VT_ERROR:
        	return V_ERROR(this) == V_ERROR(pSrc);

        case VT_BOOL:
        	return V_BOOL(this) == V_BOOL(pSrc);

        case VT_UNKNOWN:
        	return V_UNKNOWN(this) == V_UNKNOWN(pSrc);

        case VT_DECIMAL:
        	return memcmp(&(V_DECIMAL(this)), &(V_DECIMAL(pSrc)), sizeof(DECIMAL)) == 0;

        case VT_UI1:
        	return V_UI1(this) == V_UI1(pSrc);

        default:
            #pragma warning (push)
            #pragma warning (disable : 4127)
            ASSERT(0);
            #pragma warning (pop)
 			// fall through
    }

    return false;
}

// Compare a CVariant against a const VARIANT& for in-equality
//
inline bool CVariant::operator!=(const VARIANT& varSrc) const
{
    return !(*this == &varSrc);
}

// Compare a CVariant against a const VARIANT* for in-equality
//
inline bool CVariant::operator!=(const VARIANT* pSrc) const
{
    return !(*this == pSrc);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Low-level operations
//
//////////////////////////////////////////////////////////////////////////////////////////

// Clear the CVariant
//
inline HRESULT CVariant::Clear() 
{
    HRESULT hr = S_OK;

    hr = VariantClear(this);

    ASSERT(hr == S_OK);
    return hr;
}

inline HRESULT CVariant::Attach(VARIANT& varSrc) 
{
    HRESULT hr;
    //
    // Free up previous VARIANT
    //
    CHK(Clear());

    //
    // Give control of data to CVariant
    //
    memcpy(this, &varSrc, sizeof(varSrc));
    V_VT(&varSrc) = VT_EMPTY;
Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}

inline VARIANT CVariant::Detach() 
{
    VARIANT varResult = *this;
    V_VT(this) = VT_EMPTY;

    return varResult;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Destructor
//
//////////////////////////////////////////////////////////////////////////////////////////

inline CVariant::~CVariant() 
{
    Clear();
}


#endif




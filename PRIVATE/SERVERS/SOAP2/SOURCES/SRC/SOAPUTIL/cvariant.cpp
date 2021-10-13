//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      cvariant.cpp
//
// Contents:
//
//      CVariant's implementation
//
//----------------------------------------------------------------------------------

#include "Headers.h"

// Change the type and contents of this CVariant to the type vartype and
// contents of pSrc
//
HRESULT CVariant::ChangeType(VARTYPE vartype, const CVariant* pSrc) 
{
    HRESULT hr = S_OK;
    
    //
    // If pDest is NULL, convert type in place
    //
    if (pSrc == NULL) {
        pSrc = this;
    }

    if ((this != pSrc) || (vartype != V_VT(this))) 
    {
        CHK( ::VariantChangeTypeEx(static_cast<VARIANT*>(this),
                              const_cast<VARIANT*>(static_cast<const VARIANT*>(pSrc)),
                               LCID_TOUSE, VARIANT_ALPHABOOL, vartype));
    }
Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}


HRESULT CVariant::CopyVariant(const VARIANT* varSrc) 
{
    HRESULT hr;
    CHK(Clear());
    
    CHK (::VariantCopy(this, (VARIANTARG*)varSrc));
    
Cleanup:
    ASSERT (hr == S_OK);

    return hr;
}




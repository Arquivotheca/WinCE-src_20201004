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




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
#ifndef __CE_PTR_CPP_INCLUDED__
#define __CE_PTR_CPP_INCLUDED__

#include <CePtr.hpp>


    
 //Default CePtr constructor initializes pointer to 0.
CePtrBase_t::CePtrBase_t(
    void
    )
{
    m_Ptr = 0;
    m_hProc = NULL;
}

CePtrBase_t::CePtrBase_t(
    void*       p,
    HPROCESS    hProcess
    ) 
{
    m_Ptr = p;

    if( m_Ptr )
        {
        ASSERT_RETAIL(hProcess);
        m_hProc = hProcess;
        }
    else
        {
         // If the ptr is NULL, we dont need hProcess as its whole purpose is to tell which process does the ptr belongs to.        
        m_hProc = NULL;
        }
    return;    
}


#endif


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
/***
*typname.cpp - Implementation of type_info.name() for RTTI.
*
*
*Purpose:
*    This module provides an implementation of the class member function
*    type_info.name() for Run-Time Type Information (RTTI).
*
****/

#include <stdlib.h>
#include <typeinfo>
#include <mtdll.h>
#include <string.h>
#include <dbgint.h>
#include <undname.h>

// Locks not currently supported
#define _mlock(n)
#define _munlock(n)

_CRTIMP const char* type_info::name() const //17.3.4.2.5
{
    void *pTmpUndName;


        if (this->_m_data == NULL) {
#if _M_MRX000 >= 4000
            pTmpUndName = __unDName(NULL, !strncmp(this->_m_d_name,"_TD",3)? (this->_m_d_name)+4 : (this->_m_d_name)+1, 0, &malloc, &free, UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY);
#else
            pTmpUndName = __unDName(NULL, (this->_m_d_name)+1, 0, &malloc, &free, UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY);
#endif

            for (int l=strlen((char *)pTmpUndName)-1; ((char *)pTmpUndName)[l] == ' '; l--)
                ((char *)pTmpUndName)[l] = '\0';

            _mlock (_TYPEINFO_LOCK);
            size_t cch = strlen((char *)pTmpUndName) + 1;
            if ((((type_info *)this)->_m_data = malloc (cch)) != NULL)
                memcpy ((char *)((type_info *)this)->_m_data, (char *)pTmpUndName, cch);
            free (pTmpUndName);
            _munlock(_TYPEINFO_LOCK);

        }
        return (char *) this->_m_data;
}


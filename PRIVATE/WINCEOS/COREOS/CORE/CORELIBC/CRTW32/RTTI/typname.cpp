//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#pragma message("ISSUE: fix debug stuff in rtti\n")
#ifndef _DEBUGBLAH

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
            ((type_info *)this)->_m_data = malloc (strlen((char *)pTmpUndName) + 1);
            strcpy ((char *)((type_info *)this)->_m_data, (char *)pTmpUndName);
            free (pTmpUndName);
            _munlock(_TYPEINFO_LOCK);


        }
        return (char *) this->_m_data;
}

#else   // _DEBUG

_CRTIMP const char* type_info::name() const //17.3.4.2.5
{
    void *pTmpUndName;


        if (this->_m_data == NULL) {
#if _M_MRX000 >= 4000
            pTmpUndName = __unDName(NULL, !strncmp(this->_m_d_name,"_TD",3)? (this->_m_d_name)+4 : (this->_m_d_name)+1, 0, &_malloc_base, &_free_base, UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY);
#else
            pTmpUndName = __unDName(NULL, (this->_m_d_name)+1, 0, &_malloc_base, &_free_base, UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY);
#endif

            for (int l=strlen((char *)pTmpUndName)-1; ((char *)pTmpUndName)[l] == ' '; l--)
                ((char *)pTmpUndName)[l] = '\0';

            _mlock (_TYPEINFO_LOCK);
            ((type_info *)this)->_m_data = _malloc_crt (strlen((char *)pTmpUndName) + 1);
            strcpy ((char *)((type_info *)this)->_m_data, (char *)pTmpUndName);
            _free_base (pTmpUndName);
            _munlock(_TYPEINFO_LOCK);
        }
        return (char *) this->_m_data;
}

#endif // _DEBUG

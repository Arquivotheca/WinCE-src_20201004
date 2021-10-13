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
*typeinfo.cpp - Implementation of type_info for RTTI.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module provides an implementation of the class type_info
*       for Run-Time Type Information (RTTI).
****/

#define _USE_ANSI_CPP   /* Don't emit /lib:libcp directive */

#include <stdlib.h>
#ifndef _SYSCRT
#include <typeinfo>
#else
#include <typeinfo.h>
#endif
#include <mtdll.h>
#include <string.h>
#include <dbgint.h>
#include <undname.h>


#ifndef _SYSCRT
_CRTIMP const char* __thiscall type_info::name(__type_info_node* __ptype_info_node) const //17.3.4.2.5
{
        return _Name_base(this, __ptype_info_node);
}

__thiscall type_info::~type_info()
{
    type_info::_Type_info_dtor(this);
}

_CRTIMP const char* __thiscall type_info::_name_internal_method(__type_info_node* __ptype_info_node) const //17.3.4.2.5
{
        return _Name_base_internal(this, __ptype_info_node);
}

_CRTIMP void __thiscall type_info::_type_info_dtor_internal_method(void)
{
    type_info::_Type_info_dtor_internal(this);
}

#else
_CRTIMP type_info::~type_info()
{


    _mlock(_TYPEINFO_LOCK);
    __TRY
        if (_M_data != NULL) {
#ifdef _DEBUG /* CRT debug lib build */
                _free_base (_M_data);
#else
                free (_M_data);
#endif
        }
    __FINALLY
        _munlock(_TYPEINFO_LOCK);
    __END_TRY_FINALLY

}
#endif

#if defined(_SYSCRT)
_CRTIMP int __thiscall type_info::operator==(const type_info& rhs) const
#else
_CRTIMP bool __thiscall type_info::operator==(const type_info& rhs) const
#endif
{
        return (strcmp((rhs._M_d_name)+1, (_M_d_name)+1)?0:1);
}

#if defined(_SYSCRT)
_CRTIMP int __thiscall type_info::operator!=(const type_info& rhs) const
#else
_CRTIMP bool __thiscall type_info::operator!=(const type_info& rhs) const
#endif
{
        return (strcmp((rhs._M_d_name)+1, (_M_d_name)+1)?1:0);
}

#if defined(_SYSCRT)
_CRTIMP int __thiscall type_info::before(const type_info& rhs) const
#else
_CRTIMP bool __thiscall type_info::before(const type_info& rhs) const
#endif
{
        return (strcmp((rhs._M_d_name)+1,(_M_d_name)+1) > 0);
}

_CRTIMP const char* __thiscall type_info::raw_name() const
{
    return _M_d_name;
}

__thiscall type_info::type_info(const type_info& rhs)
{
//      *TBD*
//      "Since the copy constructor and assignment operator for
//      type_info are private to the class, objects of this type
//      cannot be copied." - 18.5.1
//
//  _M_data = NULL;
//  _M_d_name = new char[strlen(rhs._M_d_name) + 1];
//  if (_M_d_name != NULL)
//      strcpy( (char*)_M_d_name, rhs._M_d_name );
}


type_info& __thiscall type_info::operator=(const type_info& rhs)
{
//      *TBD*
//
//  if (this != &rhs) {
//      this->type_info::~type_info();
//      this->type_info::type_info(rhs);
//  }
    return *this;
}

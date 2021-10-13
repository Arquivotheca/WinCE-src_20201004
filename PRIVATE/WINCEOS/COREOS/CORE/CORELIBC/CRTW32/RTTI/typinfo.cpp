//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*typinfo.cpp - Implementation of type_info for RTTI.
*
*
*Purpose:
*       This module provides an implementation of the class type_info
*       for Run-Time Type Information (RTTI).
*
****/

#include <typeinfo>
#include <mtdll.h>
#include <undname.h>

// Locks not currently supported
#define _mlock(n)
#define _munlock(n)

_CRTIMP type_info::~type_info()
{
    _mlock(_TYPEINFO_LOCK);
    if (_m_data != NULL) {
        free (_m_data);
    }
    _munlock(_TYPEINFO_LOCK);

}

_CRTIMP int type_info::operator==(const type_info& rhs) const
{
        return (strcmp((rhs._m_d_name)+1, (_m_d_name)+1)?0:1);
}

_CRTIMP int type_info::operator!=(const type_info& rhs) const
{
        return (strcmp((rhs._m_d_name)+1, (_m_d_name)+1)?1:0);
}

_CRTIMP int type_info::before(const type_info& rhs) const
{
        return (strcmp((rhs._m_d_name)+1,(_m_d_name)+1) > 0);
}

_CRTIMP const char* type_info::raw_name() const
{
    return _m_d_name;
}

type_info::type_info(const type_info& rhs)
{
//      *TBD*
//      "Since the copy constructor and assignment operator for
//      type_info are private to the class, objects of this type
//      cannot be copied." - 18.5.1
//
//  _m_data = NULL;
//  _m_d_name = new char[strlen(rhs._m_d_name) + 1];
//  if (_m_d_name != NULL)
//      strcpy( (char*)_m_d_name, rhs._m_d_name );
}


type_info& type_info::operator=(const type_info& rhs)
{
//      *TBD*
//
//  if (this != &rhs) {
//      this->type_info::~type_info();
//      this->type_info::type_info(rhs);
//  }
    return *this;
}

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
*typeinfo.h - Defines the type_info structure and exceptions used for RTTI
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the type_info structure and exceptions used for
*       Runtime Type Identification.
*
*       [Public]
*
*Revision History:
*       09/16/94  SB    Created
*       10/04/94  SB    Implemented bad_cast() and bad_typeid()
*       10/05/94  JWM   Added __non_rtti_object(), made old modena names 
*                       #ifdef __RTTI_OLDNAMES
*       11/11/94  JWM   Made typeinfo class & exception classes _CRTIMP, 
*                       removed #include <windows.h>
*       11/15/94  JWM   Moved include of stdexcpt.h below the definition of 
*                       class type_info (workaround for compiler bug)
*       02-14-95  CFW   Clean up Mac merge.
*       02/15/95  JWM   Class type_info no longer _CRTIMP, member functions 
*                       are exported instead
*       02/27/95  JWM   Class type_info now defined in ti_core.h
*       03/03/95  CFW   Bring core stuff back in, use _TICORE.
*       07/02/95  JWM   Cleaned up for ANSI compatibility.
*       12-14-95  JWM   Add "#pragma once".
*       02-21-97  GJF   Cleaned out obsolete support for _NTSDK. Also, 
*                       detab-ed and reformatted the header a bit.
*       05-17-99  PML   Remove all Macintosh support.
*       06-01-99  PML   __exString disappeared as of 5/3/99 Plauger STL drop.
*       03-21-01  PML   Move bad_cast, bad_typeid, __non_rtti_object func
*                       bodies to stdexcpt.cpp.
*       10-09-02  PK    Changed return type for type_info::Operator== and 
*                       type_info::Operator!= from int to bool (Whidbey bug 2398)
*       03-26-03  SSM   Moved classes into std namespace (Whidbey:1832)
*       04-07-04  MSL   Double slash removal
*       09-14-04  MR    Add std:: for bad_cast and bad_typeid (VSWhidbey: 308723)
*       09-22-04  AGH   Guard header with #ifndef RC_INVOKED, so RC ignores it
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#include <crtdefs.h>

#ifndef _INC_TYPEINFO
#define _INC_TYPEINFO

#pragma pack(push,_CRT_PACKING)

#ifndef RC_INVOKED

#ifndef __cplusplus
#error This header requires a C++ compiler ...
#endif

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#ifndef _SYSCRT
#include <typeinfo>

#ifndef _TICORE

#ifdef  __RTTI_OLDNAMES
/* Some synonyms for folks using older standard */
using std::bad_cast;
using std::bad_typeid;

typedef type_info Type_info;
typedef bad_cast Bad_cast;
typedef bad_typeid Bad_typeid;
#endif  /* __RTTI_OLDNAMES */

#endif  /* _TICORE */

#else   /* _SYSCRT */

//#ifndef _SYSCRT
struct __type_info_node {
    void *_MemPtr;
    __type_info_node* _Next;
};

extern __type_info_node __type_info_root_node;
//#endif

class type_info {
public:
#ifndef _WIN32_WCE
    SECURITYCRITICAL_ATTRIBUTE
#endif
    _CRTIMP virtual __thiscall ~type_info();
    _CRTIMP int __thiscall operator==(_In_ const type_info& _Rhs) const;
    _CRTIMP int __thiscall operator!=(_In_ const type_info& _Rhs) const;  
#ifdef _SYSCRT
    _CRTIMP int __thiscall before(_In_ const type_info& _Rhs) const;
    _Check_return_ _CRTIMP const char* __thiscall name() const;
#else
    _CRTIMP bool __thiscall before(_In_ const type_info& _Rhs) const;
    _Check_return_ _CRTIMP const char* __thiscall name(_Inout_ __type_info_node* __ptype_info_node = &__type_info_root_node) const;
#endif
    _Check_return_ _CRTIMP const char* __thiscall raw_name() const;
private:
    void *_M_data;
    char _M_d_name[1];
    __thiscall type_info(_In_ const type_info& _Rhs);
    type_info& __thiscall operator=(_In_ const type_info& _Rhs);
};
#ifndef _TICORE

/* This include must occur below the definition of class type_info */
#include <stdexcpt.h>

class _CRTIMP bad_cast : public std::exception {
public:
    __thiscall bad_cast(_In_z_ const char * _Message = "bad cast");
    __thiscall bad_cast(_In_ const bad_cast & _Bad_cast);
    virtual __CLR_OR_THIS_CALL ~bad_cast();
#ifndef _INTERNAL_IFSTRIP_
#ifdef  CRTDLL
private:
    /* This is aliased to public:bad_cast(const char * const &) to provide */
    /* the old, non-conformant constructor. */
    bad_cast(_In_z_ const char * const * _Message);
#endif  /* CRTDLL */
#endif  /* _INTERNAL_IFSTRIP_ */
};

class _CRTIMP bad_typeid : public std::exception {
public:
    bad_typeid(_In_z_ const char * _Message = "bad typeid");
    bad_typeid(_In_ const bad_typeid &);
    virtual ~bad_typeid();
};

class _CRTIMP __non_rtti_object : public bad_typeid {
public:
    __non_rtti_object(_In_z_ const char * _Message);
    __non_rtti_object(_In_ const __non_rtti_object &);
    virtual ~__non_rtti_object();
};

#ifdef  __RTTI_OLDNAMES
/* Some synonyms for folks using older standard */
typedef type_info Type_info;
typedef bad_cast Bad_cast;
typedef bad_typeid Bad_typeid;
#endif  /* __RTTI_OLDNAMES */

#endif  /* _TICORE */

#endif

#endif /* RC_INVOKED */

#pragma pack(pop)

#endif  /* _INC_TYPEINFO */

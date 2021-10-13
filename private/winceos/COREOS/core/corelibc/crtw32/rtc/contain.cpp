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
*contain.cpp - RTC support
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*
*Revision History:
*       07-28-98  JWM   Module incorporated into CRTs (from KFrei)
*       11-03-98  KBF   added throw() to eliminate C++ EH code
*       05-11-99  KBF   Error if RTC support define not enabled
*       05-26-99  KBF   Added _RTC_ prefix, _RTC_ADVMEM stuff
*
****/

#ifndef _RTC
#error  RunTime Check support not enabled!
#endif

#include "rtcpriv.h"

#ifdef _RTC_ADVMEM

_RTC_Container *
_RTC_Container::AddChild(_RTC_HeapBlock *hb) throw()
{
    if (kids)
    {
        _RTC_Container *p = kids->get(hb);
        if (p)
            return p->AddChild(hb);
        kids->add(hb);

    } else
    {   
        kids = new _RTC_BinaryTree(new _RTC_Container(hb));
    }
    return this;
}


_RTC_Container *
_RTC_Container::DelChild(_RTC_HeapBlock* hb) throw()
{
    if (kids)
    {
        _RTC_Container *p = kids->get(hb);
        if (p)
        {
            if (p->inf == hb) {
                kids->del(hb)->kill();
                return this;
            } else
                return p->DelChild(hb);
        }
        kids->del(hb);
        return this;
    } else
        return 0;
}


_RTC_Container *
_RTC_Container::FindChild(_RTC_HeapBlock *i) throw()
{
    if (inf == i)
        return this;
    else if (kids)
    {
        _RTC_Container *res = kids->get(i);
        if (res)
            return res->FindChild(i);
    }
    return 0;
}


void 
_RTC_Container::kill() throw()
{
    if (kids) 
    {
        _RTC_BinaryTree::iter i;

        for (_RTC_Container *c = kids->FindFirst(&i); c; c = kids->FindNext(&i))
            c->kill();

        delete kids;
        kids = 0;
    } 
    
    if (inf)
    {
        delete inf;
        inf = 0;
    }
}

#endif // _RTC_ADVMEM

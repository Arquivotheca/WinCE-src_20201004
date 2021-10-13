//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include <list.hxx>
#include <auto_xxx.hxx>
#include <string.hxx>
#include <litexml.h>

#include "FnActionBase.h"

class FnActionList
{
private:
    //member variables
    ce::list<ce::smart_ptr<FnActionBase>> m_lstActions;
    BOOL _ExportActionsXml( litexml::XmlElement_t & pElement );
public:
    //constructors
    FnActionList( ) {};
    
    // import/export methods   
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );
 
    BOOL AddAction( ce::smart_ptr<FnActionBase> fnAction );


    //iterators
    //actions
    typedef ce::list<ce::smart_ptr<FnActionBase>>::iterator ActionIterator;
    ActionIterator beginActions()
        {return m_lstActions.begin(); }
    ActionIterator endActions()
        {return m_lstActions.end(); }

};
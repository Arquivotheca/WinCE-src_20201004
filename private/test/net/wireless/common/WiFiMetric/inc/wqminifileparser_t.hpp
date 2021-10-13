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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// File Name:	WQMIniFileParser_t.hpp
// Description:  This file provide class definition for WQMInitFileParser
//
//-----------------------------------------------------------------------------

#pragma once
#ifndef __WQMINIFILEPARSER_h__
#define __WQMINIFILEPARSER_h__


#include "WQMUtil.hpp"
#include "litexml.h"

#define WQM_FILE_ROOT_TAG   _X("TestRun")


namespace ce {
namespace qa {

// forward declaration
class  WQMTestCase_t;
class  WQMTestSuiteBase_t;

struct WQMTstSuiteNode
{
    WQMParamList*       ParamNode;
    WQMTestSuiteBase_t* TstSuite;
    WQMEleNode*         ConfigNode;
    WQMTstSuiteNode*    nextUnit;
};

#define MAX_TSTGRPNAME_LEN    64
struct WQMTstGroupNode
{
    TCHAR             TstGroupName[MAX_TSTGRPNAME_LEN];
    WQMParamList*     ParamNode;
    WQMTstSuiteNode*  TstSuiteList;
    WQMTstGroupNode*  nextGroup;
};


struct WQMTstCaseNode
{
    WQMTestCase_t*      TstCaseNode;
    WQMTstCaseNode*     nextUnit;
};

#define MAX_TSTSUITENAME_LEN   64
struct WQMTstSuiteHash
{
    TCHAR             TstSuiteName[MAX_TSTSUITENAME_LEN];
    WQMNameHash       TstHashTable;
};
   
class WQMIniFileParser_t
{
    public:

        WQMIniFileParser_t(void);
        ~WQMIniFileParser_t(void);

        BOOL               ParseFile(LPTSTR ptszIDNFile,
                                          WQMNameHash&  setupLkUpTable,
                                          WQMTstSuiteHash* tstSuiteLkUpTable,
                                          int  tstSuiteLkUpTableSize);

        int                GetTestUnits(WQMTstCaseNode** testList); 
        WQMParamList*      GetSetupUnit(void) {return m_pSetup;}
        WQMTstGroupNode*   GetTstGroups(void);


    private:
        litexml::XmlElement_t*    ParseRootElement(const litexml::XmlElement_t* rootNode,
                                                            WQMNameHash& hashTable,
                                                            WQMNameHash& SetupCmdTable,
                                                            WQMTstSuiteHash* tstSuiteLkUpTable,
                                                            int  tstSuiteLkUpTableSize);  
        
        litexml::XmlElement_t*    ExpandIncFile(LPCTSTR incFileName, 
                                                     WQMNameHash& hashTable,
                                                     const litexml::XmlBaseElement_t* incNode,
                                                     WQMNameHash& SetupCmdTable,
                                                     WQMTstSuiteHash* tstSuiteLkUpTable,
                                                     int  tstSuiteLkUpTableSize);        
        
        BOOL  HandleTstCase(const litexml::XmlBaseElement_t*  tstCase, int repeatIndex = 0);
        BOOL  HandleTstSuite(const litexml::XmlBaseElement_t*  tstSuite);
        BOOL  HandleTstSetup(const litexml::XmlBaseElement_t*  tstSetup);
        BOOL  HandleJumble(const litexml::XmlBaseElement_t*  jumble, BOOL isParallel);
        BOOL  HandleTstGroup(const litexml::XmlBaseElement_t*  tstGroup);
          
          
        WQMTstCaseNode*           m_pTestExecutionList;
        WQMParamList*             m_pSetup;
        WQMTstCaseNode*           m_pCurUnit;
        int                       m_TotalTestUnit;
        WQMTstGroupNode*          m_pTstGroupList;
        WQMTstGroupNode*          m_pCurTstGroup;
        WQMTstSuiteNode*          m_pCurTstSuite;
        litexml::XmlElement_t*    m_pRootElement;
      
};
};
};

#endif // __WQMINIFILEPARSER_h__

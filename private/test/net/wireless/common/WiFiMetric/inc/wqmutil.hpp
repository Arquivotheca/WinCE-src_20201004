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
// File Name: WQMUtil.hpp
//
// Description: This file provide helper function for WiFi Metrics (WQM)
//
// ----------------------------------------------------------------------------

#pragma once
#ifndef _DEFINED_WQMUtil_H_
#define _DEFINED_WQMUtil_H_

#include "Hash_map.hxx"
namespace litexml
{
    class XmlBaseElement_t;
    class XmlAttributeElement_t;
    typedef ce::wstring	XmlString_t;
    class XmlElement_t;
};


namespace ce {
namespace qa {

#define WQM_REPEAT_ATTR_ITER        TEXT("Iteration")
#define WIFI_METRIC_MAX_SUBBLK_NO      ((int) 16) 
#define WIFI_METRIC_INPUT_BUFFER_LEN   ((int) 300)


#define WQM_SUITE_CONFIG_TAG    _X("Config")

struct sNode
{
   LPTSTR   ptszKey;
   LPTSTR   ptszString;
   sNode*   right;
   sNode*   left;
};

typedef sNode* LPSNODE;

struct WQMParamList
{
   LPSNODE       pNode;
   WQMParamList *pParent;
};

typedef ce::hash_map<ce::wstring,ce::wstring> WQMNameHash;

struct WQMEleNode
{
    litexml::XmlBaseElement_t*  ConfigElement;
    WQMNameHash NameHash;
};

enum WQMNameReplace_Enum
{
    WQMNameReplace_None = 0,
    WQMNameReplace_Done,
    WQMNameReplace_Pending,
    WQMNameReplace_Fail,
};

class WQMTestSuiteBase_t;
class CmdArg_t;
class CmdArgList_t;

// Common helper functions and related defines
LPTSTR   FindString(const LPSNODE pNode, const LPCTSTR ptszKey);
LPTSTR   FindStringEx(const WQMParamList* pNode, const LPCTSTR ptszKey);
void     PrintNode(const LPSNODE pNode, int indentLevel);
void     PrintNodeEx(const WQMParamList* pNode,int indentLevel = 0, BOOL recursive = FALSE);
LPSNODE  StringMakeNewNode( LPTSTR ptszKey, LPTSTR ptszString );
VOID     StringAddNode(LPSNODE* pHead, LPSNODE pNewNode );
LPTSTR   StringAddString(LPSNODE* pHead, LPTSTR ptszKey);
BOOL     StringDestroyTree(LPSNODE pNode);
BOOL     DestroyParamTree(WQMParamList* pNode);
BOOL     GetBuildInfo (DWORD *pdwMajorOSVer, DWORD *pdwMinorOSVer, DWORD *pdwOSBuildNum, DWORD *pdwROMBuildNum);
BOOL     GetPlatformName (LPTSTR tszPlatname, DWORD dwMaxStrLen);
BOOL     GetBatteryLife (DWORD *expectedLife, DWORD *percentLeft, DWORD *timeLeft);
BOOL     FindFile(LPTSTR lptsFullPath, DWORD dwBuffLen, LPTSTR lptsFileName);
HANDLE   CreateLogFile(LPTSTR lptsFullPath, DWORD dwBuffLen, LPTSTR lptsFileName);
BOOL     GetBinaryPath(LPTSTR lptstrBinaryPath);
BOOL     GetAutoRunPath(LPTSTR lptstrBinaryPath);
BOOL     GetPersistentStorageRoot (LPTSTR  ptszOut, DWORD *dwOutSize);
BOOL     SetPersistentStorageRoot(LPTSTR ptszPath, DWORD dwPathSize);
BOOL     WCharToAsc(const WCHAR* buf, CHAR* outBuf, int outBufLen);
WQMNameReplace_Enum     
ReplaceNameInString(litexml::XmlString_t& eleString, WQMNameHash& NameHash, BOOL toErase = FALSE);
BOOL     RunConfig(WQMEleNode* configNode);

BOOL
ReplaceNameForElement(WQMNameHash& NameHash, 
                                 litexml::XmlAttributeElement_t* outputEle);

// Load a dll based on the name field included in the param list
HMODULE LoadDllModule(const WQMParamList *pTestSuiteParams);

// Help func to load dll and instantiate a TestSuite Object
WQMTestSuiteBase_t* 
InstantiateTstSuiteObj(const WQMParamList *pTestSuiteParams, HANDLE logHandle);

// Prototype of the Factory func
typedef WQMTestSuiteBase_t* (*FactoryFunc) (HMODULE,HANDLE);

// Func to destroy a TestSuite Object and unload dll
void 
DestroyTstSuite(WQMTestSuiteBase_t* pTestSuite);

DWORD 
CopyDesc(LPCTSTR* dest, const TCHAR* Description);

DWORD
GetValueFromConfigNode(const WQMParamList *pTestParams, CmdArg_t* pArg);

DWORD
ParseConfigurationNode(const WQMParamList *pTestParams, CmdArgList_t* pCmdList);

BOOL
UpdateParamNode(LPSNODE* rootNode, const litexml::XmlBaseElement_t*  element);

};
};

#endif // _DEFINED_WQMUtil_H_



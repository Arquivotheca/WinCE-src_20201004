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

///////////////////////////////////////////////////////////////
//
//  ApiTestUtils Library
//
//  Module: PerfTestUtils.h
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <auto_xxx.hxx>
#include <XRPtr.h>
#include <..\..\inc\XRCommon.h>

#define MAX_BUFFER_LENGTH           1024

#define XR_TESTNAME_FORMAT          L"XR\\%s"
#define XRPERF_API_PATH             L"XR\\tux\\API"

#define XRPERF_TEST_PATH_FORMAT     L"%s\\xamlruntime\\test\\perf\\%s"
#define XRPERF_RESOURCE_DIR         L"\\release\\xamlruntime\\test\\perf\\"

#define XR_WINDOWSXAMLRUNTIME_PATH                                      L"\\windows\\XamlRuntime"
#define XR_XAMLRUNTIMETEST_PATH XR_WINDOWSXAMLRUNTIME_PATH              L"\\test"
#define XR_XAMLRUNTIMETESTPERF_PATH XR_XAMLRUNTIMETEST_PATH             L"\\perf"
#define XR_XAMLRUNTIMETESTPERFCOPIES_PATH XR_XAMLRUNTIMETESTPERF_PATH   L"\\copies"

HRESULT GetTargetElement(IXRVisualHost *pHost,WCHAR *pszName, IXRDependencyObject **ppDO);
bool LoadDataFile(SPS_SHELL_INFO *spParam);
bool LoadDataFile(wchar_t* spParam);
bool SetTestCaseData(SPS_BEGIN_TEST *spParam);
void CleanTestCaseData();
HRESULT GetTargetElementName(WCHAR *pszName, size_t NameSize);
HRESULT Getint(WCHAR *pszAttrName, int *pValue);
HRESULT Getbool(WCHAR *pszAttrName, bool *pValue);
HRESULT GetWCHAR(WCHAR* pszAttrName, WCHAR* pValue, size_t maxLen = MAX_PATH);

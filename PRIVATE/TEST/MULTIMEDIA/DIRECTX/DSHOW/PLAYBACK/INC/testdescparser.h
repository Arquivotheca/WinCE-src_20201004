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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _TESTDESC_PARSER_H
#define _TESTDESC_PARSER_H

#include "xmlif.h"
#include "TestDesc.h"

typedef HRESULT (*ParseTestFunction)(HELEMENT hElement, TestDesc** pTestDesc);

extern ParseTestFunction GetParseFunction(TCHAR* szTestName);

HRESULT ParseStringListString(const char* szStringList, const char delimiter, char* szString, int len, int& pos);

extern HRESULT ParsePlaceholderTestDesc(HELEMENT hElement, TestDesc** ppTestDesc);
extern HRESULT ParseVRTestDesc(HELEMENT hElement, TestDesc** ppTestDesc);
extern HRESULT ParsePlaybackTestDesc(HELEMENT hElement, TestDesc** ppTestDesc);
extern HRESULT ParseStateChangeTestDesc(HELEMENT hElement, TestDesc** ppTestDesc);
extern HRESULT ParseSetPosRateTestDesc(HELEMENT hElement, TestDesc** pTestDesc);
extern HRESULT ParseBuildGraphTestDesc(HELEMENT hElement, TestDesc** ppTestDesc);
extern HRESULT ParseTestConfig(HELEMENT hTestConfig, TestConfig* pTestConfig);
extern HRESULT ParseGraphDesc(HELEMENT hGraphDesc, GraphDesc* pGraphDesc);
extern HRESULT ParseDWORD(HELEMENT hDWord, DWORD* pDWord);
extern HRESULT ParseMedia(HELEMENT hMedia, Media* pMedia);
extern HRESULT ParseTestDesc(HELEMENT hTestDesc, TestDesc** pTestDesc);
extern HRESULT ParseTestDescList(HELEMENT hTestDescList, TestDescList* pTestDescList);
extern HRESULT ParseConnectionDesc(HELEMENT hElement, ConnectionDesc* pConnectionDesc);
extern HRESULT ParseRendererMode(const char* szElement, VideoRendererMode* pVRMode);
extern HRESULT ParseSurfaceType(const char* szSurfaceType, DWORD *pSurfaceType);

extern TestConfig* ParseConfigFile(TCHAR* szConfigFile);
extern HRESULT ParseMediaListFile(TCHAR* szMediaListFile, MediaList* pMediaList);


extern HRESULT ToState(const char* szState, FILTER_STATE* pState);
extern HRESULT ToStateChangeSequence(const char* szSequence, StateChangeSequence* pSequence);
extern HRESULT ParseState(HELEMENT hState, FILTER_STATE* pState);
extern HRESULT ParseLONGLONG(HELEMENT hDWord, LONGLONG* pllInt);
extern HRESULT ParseLONGLONG(const char* szLLInt, LONGLONG* pllInt);
extern HRESULT ParseRect(HELEMENT hRect, RECT* pRect);
extern HRESULT ParseString(HELEMENT hString, TCHAR* szString, int length);
extern HRESULT ParseStringList(HELEMENT hStringList, StringList* pStringList, char delimiter);
extern HRESULT ParsePositionList(HELEMENT hElement, PositionList* pPositionList);
extern HRESULT ParseRateList(HELEMENT hElement, RateList* pRateList);
extern HRESULT ParsePosRateList(HELEMENT hElement, PosRateList* pPosRateList);
extern HRESULT ParseFilterId(HELEMENT hElement, FilterId* pFilterId);
extern HRESULT ParseFilterDesc(HELEMENT hElement, FilterDesc* pFilterDesc);
extern HRESULT ParseFilterDescList(HELEMENT hElement, FilterDescList* pFilterDescList);
extern HRESULT ParseVerifyDWORD(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifyRECT(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifyGraphDesc(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifyRendererMode(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifyRendererSurfaceType(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifySampleDelivered(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerifyPlaybackDuration(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList);
extern HRESULT ParseVerification(HELEMENT hVerification, VerificationList* pVerificationList);



class TestDescParser
{
public:
	static TestConfig* ParseConfigFile(TCHAR* szConfigFile);
	static MediaList* GetMediaList();
	static TestList* GetTestList();
	static FUNCTION_TABLE_ENTRY* GetFunctionTable();
};

#endif
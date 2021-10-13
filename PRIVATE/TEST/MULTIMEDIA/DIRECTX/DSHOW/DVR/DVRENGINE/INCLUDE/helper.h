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
/*	------------------------------------------------------------
	This is the big header file for Filter testing purpose.
	For general useage, all definition AND implementation
	are with the single Header file so user can easily expand
	it later. If compiles as Helper.cpp and Helper.h to a
	Lib file then it is hard to expand for futhre use
	------------------------------------------------------------*/

#ifndef _DVR_HELPER_H_
#define _DVR_HELPER_H_

#include <windows.h>
#include "DShow.h"
#include "Wxdebug.h"

#ifndef _WIN32_WCE 
#define MC_GUIDEAPP_REGPATH                 _T("Software\\Microsoft\\Windows\\CurrentVersion\\Media Center\\MCShell")
#else // !_WIN32_WCE 
#define MC_GUIDEAPP_REGPATH                 _T("Software\\Microsoft\\Windows\\CurrentVersion\\MediaCenter\\MCShell") 
#endif // !_WIN32_WCE 

#define DVRSPLT_DEFAUDIOTYPE                TEXT("CLSID\\{2E517BA9-5F2C-4E6E-86E2-533763858FEC}\\Pins\\Audio Output"),


const DWORD MAX_MY_PERMANENT_FILE_SIZE = 0x8000000;	// ~134 meg	for testing purposes
const DWORD MAX_MY_TEMPORARY_FILE_SIZE = 0x40000;	// ~256k for testing purposes
const DWORD MAX_PERMANENT_FILE_SIZE = 0xffffffff;	// ~134 meg	for testing purposes
const DWORD MAX_TEMPORARY_FILE_SIZE = 0x8000000;	// ~256k for testing purposes

const LPCTSTR MAX_PERMANENT_FILE_SIZE_KEY = TEXT("MaxPermFileSizeCb");
const LPCTSTR MAX_TEMPORARY_FILE_SIZE_KEY = TEXT("MaxTempFileSizeCb");


const GUID CLSID_Demux = 
{0xa50ba5e3, 0x4a09, 0x4d58, {0x9c, 0x34, 0x17, 0xbb, 0xa2, 0x77, 0xed, 0x1a}};
const GUID CLSID_Babylon_EM10 =		/* in fact ASFSource GUID */
{0xba46b749, 0x97d8, 0x4811, {0xbd, 0x99, 0x56, 0x9b, 0xcd, 0xc4, 0x7e, 0xd2}};
const CLSID CLSID_AsfSource = 
{0xba46b749, 0x97d8, 0x4811, {0xbd, 0x99, 0x56, 0x9b, 0xcd, 0xc4, 0x7e, 0xd2}};
const CLSID CLSID_DVRSinkFilter =
{0x8255F4E7, 0x1EC2, 0x4EE0, {0xB9, 0xD5, 0x5B, 0x57, 0x69, 0xF5, 0x7C, 0x30}};
const CLSID CLSID_DVRSourceFilter =
{0x860D1550, 0x2308, 0x4AAA, {0x89, 0x06, 0x26, 0xE5, 0x5F, 0xD8, 0xF9, 0x76}};
const CLSID CLSID_ASF_DEMUX_FILTER  =
{0XA50BA5E3, 0X4A09, 0X4D58, {0X9C, 0X34, 0X17, 0XBB, 0XA2, 0X77, 0XED, 0X1A}};
const CLSID CLSID_LSTEncoderTuner =
{0x774fb826, 0x574f, 0x4606, {0x96, 0x6b, 0x3a, 0x00, 0x7e, 0x55, 0x0a, 0x3e}};
const CLSID CLSID_AOvMixer =
{0x1b96e2d2, 0xc4e4, 0x4dff, {0x9f, 0x06, 0x30, 0x01, 0x4e, 0x26, 0x27, 0x39}};
const CLSID CLSID_AudioRenderer2 =
{0xcaa1adfb, 0x19b1, 0x4361, {0x9a, 0xbe, 0xb9, 0x28, 0x65, 0xad, 0xee, 0x1c}};

// Necessary test filters
const CLSID CLSID_QinNullRenderer =
{0x155BEAF8, 0x7FB3, 0x4db5, {0x88, 0x5D, 0x54, 0x0E, 0x39, 0x5E, 0xDD, 0xD3}};
const GUID CLSID_myGrabberSample = 
{0xC40ADD49, 0xD7DB, 0x461f, {0x8C, 0xB5, 0x0D, 0x8C, 0x2E, 0xAD, 0x42, 0x82}};
const CLSID CLSID_MpegSource =
{0x23AE9137, 0xDE5B, 0x41DB, {0x88, 0x2B, 0x9F, 0x2E, 0xD0, 0xA7, 0xFE, 0x62}};
const CLSID CLSID_MediaExcel =
{0xEDC37564, 0xF1F9, 0x4FB9, {0x8A, 0xC2, 0x48, 0x89, 0x58, 0x15, 0x6B, 0x94}};
const CLSID CLSID_DVRTestSink =
{0x70569fb3, 0xbc50, 0x4b4d, {0xa0, 0xeb, 0x14, 0x9d, 0xc0, 0xd4, 0x86, 0x8b}};
const CLSID CLSID_PullToPushFilter = 
{0x7d07ae87, 0x2e1a, 0x4e8a, {0x99, 0x6f, 0x8f, 0x1f, 0x80, 0x51, 0xfd, 0x1f}};
const CLSID CLSID_MorphFilter = 
{0xc9eeb20a, 0x353b, 0x4ee8, {0xa1, 0xa5, 0xce, 0x64, 0x44, 0xff, 0x1, 0xe}};
const CLSID CLSID_MPEG2Demultiplexer =
{0xafb6c280, 0x2c41, 0x11d3, {0x8a, 0x60, 0x00, 0x00, 0xf8, 0x1e, 0x0e, 0x4a}};
const CLSID CLSID_Tee =
{0x22b8142, 0x946, 0x11cf, {0xbc, 0xb1, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0}};
const CLSID CLSID_NullRenderer =
{0xC1F400A4, 0x3F08, 0x11D3, {0x9F, 0x0B, 0x00, 0x60, 0x8, 0x3, 0x9E, 0x37}};
const CLSID CLSID_MPegVerifier2File = 
{0xB30BA5D3, 0x5A09, 0x4d58, {0xAc, 0x34, 0x37, 0xbb, 0xa2, 0x87, 0xEA, 0x1d}};

typedef  HRESULT ( __stdcall *pFuncpfv)();

/////////////////////////////////////////////////////////////////////////////////////////
#ifndef _WIN32_WCE		// XP only
HRESULT Helper_SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath);
HRESULT Helper_LoadGraphFile(IGraphBuilder *pGraph, const WCHAR* wszName);
#endif
HRESULT Helper_RegisterDllUnderCE(LPCTSTR tzDllName);
HRESULT Helper_GetPinbyName(IBaseFilter *pFilter, LPCWSTR wsPinName, IPin **ppPin);
HRESULT Helper_DisplayPinInfo(IPin *pPin);
// HRESULT	Helper_ListCategory(CLSID myCategory);
HRESULT	Helper_EnumPinsofFilter(IBaseFilter *pFilter, LPCWSTR wsDescription);
HRESULT	Helper_DisplayFilter(IBaseFilter * pFilter);
HRESULT Helper_DislplayFilterOfGraph(IGraphBuilder * pGraph);
HRESULT Helper_GetFilterFromGraphByName(IGraphBuilder * pGraph, IBaseFilter **pFilter, LPCWSTR wsPinName);
//	This API will save a VIDEOINFOHEADER struct into a file
HRESULT Helper_SaveMediaTypeToFile(VIDEOINFOHEADER * pmt, LPCTSTR lpFileName);
HRESULT Helper_FindInterfaceFromGraph(IGraphBuilder * pIGraphBuilder, const IID & riid, void ** ppvObject);
DWORD Helper_DvrRegQueryValueGenericCb(LPCTSTR pcszKey, LPBYTE pbBuffer, DWORD cbBuffer, DWORD* pdwType);
DWORD Helper_DvrRegQueryValueDWORD(LPCTSTR pcszKey, LPDWORD pDWord);
DWORD Helper_DvrRegSetValueGenericCb(LPCTSTR pcszKey, LPBYTE pbBuffer, DWORD cbBuffer, DWORD* pdwType);
DWORD Helper_DvrRegSetValueDWORD(LPCTSTR pcszKey, LPDWORD pDWord);
DWORD Helper_DvrSetAudioType(GUID AudioType);
BOOL Helper_CleanupRecordPath(LPCTSTR tszLocation);
BOOL Helper_CleanupSingleRecordPath(LPCTSTR tszLocation);
void Helper_CreateRandomGuid(GUID* pguid);

#endif // _DVR_HELPER_H_


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

#include "logging.h"
#include "globals.h"
#include "TuxGraphTest.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "TestDescParser.h"
#include "Operation.h"
#include "xmlif.h"

const char* const STR_True = "true";
const char* const STR_False = "false";
const char* const STR_TestGroup = "TestGroup";
const char* const STR_TestId = "TestId";
const char* const STR_Tolerance = "Tolerance";
const char* const STR_GraphDesc = "GraphDesc";
const char* const STR_PreloadFilter = "FilterList";
const char* const STR_MediaList = "MediaList";
const char* const STR_TestList = "TestList";
const char* const STR_Media = "Media";
const char* const STR_Url = "Url";
const char* const STR_Name = "Name";
const char* const STR_TestDesc = "Test";
const char* const STR_Desc = "Desc";
const char* const STR_FileName = "FileName";
const char* const STR_Description = "Description";
const char* const STR_IsVideo = "IsVideo";
const char* const STR_BaseUrls = "BaseUrl";
const char* const STR_Verification = "Verify";
const char* const STR_DownloadLocation = "DownloadTo";
const char* const STR_PlaybackDuration = "PlaybackDuration";
const char* const STR_AudioInfo = "AudioInfo";
const char* const STR_VideoInfo = "VideoInfo";
const char* const STR_PositionList = "PositionList";
const char* const STR_PosRateList = "PosRateList";
const char* const STR_NumFrames = "Frames";
const char* const STR_GenerateId = "GenerateId";
const char* const STR_Index = "Index";
const char* const STR_RendererMode = "RendererMode";
const char* const STR_RendererMode2 = "RendererMode2";
const char* const STR_GDIMode = "GDI";
const char* const STR_DDrawMode = "DDRAW";
const char* const STR_OpList = "OpList";
const char* const STR_Op = "Op";
const char* const STR_WaitOp = "Wait";
const char* const STR_SwitchGDIOp = "SwitchGDI";
const char* const STR_SwitchDDrawOp = "SwitchDDraw";
const char* const STR_MaxBackBuffers = "MaxBackBuffers";
const char* const STR_VRImageSize = "ImageSize";
const char* const STR_MinImageSize = "MinImageSize";
const char* const STR_MaxImageSize = "MaxImageSize";
const char* const STR_FullScreen = "FullScreen";
const char* const STR_VRFlags = "VRFlags";
const char* const STR_AllowPrimaryFlipping = "AllowPrimaryFlipping";
const char* const STR_UseScanLine = "UseScanLine";
const char* const STR_UseOverlayStretch = "UseOverlayStretch";
const char* const STR_SrcRect = "SrcRect";
const char* const STR_DstRect = "DstRect";
const char* const STR_WndPosList = "WindowPositionList";
const char* const STR_WndScaleList = "WindowScaleList";
const char* const STR_WndRectList = "WindowRectList";
const char* const STR_State = "State";
const char* const STR_Stopped = "Stopped";
const char* const STR_Paused = "Paused";
const char* const STR_Running = "Running";
const char* const STR_Container = "Container";
const char* const STR_PlayPause = "PlayPause";
const char* const STR_PlayStop = "PlayStop";
const char* const STR_PauseStop = "PauseStop";
const char* const STR_PausePlayStop = "PausePlayStop";
const char* const STR_RandomSequence = "RandomSequence";
const char* const STR_StateChangeSequence = "StateChangeSequence";

const char* const STR_PS="PS";
const char* const STR_RGBOVR="RGBOVR";
const char* const STR_YUVOVR="YUVOVR";
const char* const STR_RGBOFF="RGBOFF";
const char* const STR_YUVOFF="YUVOFF";
const char* const STR_RGBFLP="RGBFLP";
const char* const STR_YUVFLP="YUVFLP";
const char* const STR_ALL="ALL";
const char* const STR_YUV="YUV";
const char* const STR_RGB="RGB";

// Basic media properties
const char* const STR_MEDIA_PROP = "MediaProperties";
const char* const STR_TIME_MEDIA_TIME = "TimeMediaTime";
const char* const STR_TIME_FIELD = "TimeField";
const char* const STR_TIME_SAMPLE = "TimeSample";
const char* const STR_TIME_BYTE = "TimeByte";
const char* const STR_TIME_FRAME = "TimeFrame";
const char* const STR_TIME_EARLIEST = "Earliest";
const char* const STR_TIME_LATEST = "Latest";
const char* const STR_TIME_DURATION = "Duration";
const char* const STR_TIME_PREROLL = "Preroll";
const char* const STR_SEEK_CAPS = "SeekCaps";

// Performance information for video
const char* const STR_VIDEO_AVG_FRAME_RATE = "AvgFrameRate";
const char* const STR_VIDEO_AVG_SYNC_OFFSET = "AvgSyncOffset";
const char* const STR_VIDEO_DEV_SYNC_OFFSET = "DevSyncOffset";
const char* const STR_VIDEO_FRAME_DRAWN = "FramesDrawn";
const char* const STR_VIDEO_FRAME_DROPPED = "FramesDropped";
const char* const STR_VIDEO_JITTER = "Jitter";

const char MEDIALIST_DELIMITER = ',';
const char POSITION_LIST_DELIMITER = ',';
const char POSRATE_LIST_DELIMITER = ',';
const char POSRATE_DELIMITER = ':';
const char SURFACE_MODE_STR_DELIMITER = ',';
const char RENDERER_MODE_STR_DELIMITER = ',';
const char RENDERER_SURFACE_MODE_STR_DELIMITER = ':';
const char STR_OP_DELIMITER = ':';
const char STR_RECT_DELIMITER = ',';
const char WNDPOS_LIST_DELIMITER = ',';
const char WNDPOS_DELIMITER = ':';
const char WNDRECT_LIST_DELIMITER = ',';
const char WNDRECT_DELIMITER = ':';
const char WNDSCALE_LIST_DELIMITER = ',';
const char WNDSCALE_DELIMITER = ':';
const char STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER = ',';
const char VR_FLAGS_DELIMITER = ',';
const char STR_STATE_CHANGE_SEQ_DELIMITER = ',';
const char STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER = ',';

HRESULT ToState(const char* szState, FILTER_STATE* pState)
{
	HRESULT hr = S_OK;
	// Convert from string to state
	if (!strcmp(szState, STR_Stopped))
		*pState = State_Stopped;
	else if (!strcmp(szState, STR_Paused))
		*pState = State_Paused;
	else if (!strcmp(szState, STR_Running))
		*pState = State_Running;
	else {
		hr = E_FAIL;
	}

	return hr;
}


HRESULT ToStateChangeSequence(const char* szSequence, StateChangeSequence* pSequence)
{
	HRESULT hr = S_OK;
	// Convert from string to state
	if (!strcmp(szSequence, STR_PlayPause))
		*pSequence = PlayPause;
	else if (!strcmp(szSequence, STR_PlayStop))
		*pSequence = PlayStop;
	else if (!strcmp(szSequence, STR_PauseStop))
		*pSequence = PauseStop;
	else if (!strcmp(szSequence, STR_PausePlayStop))
		*pSequence = PausePlayStop;
	else if (!strcmp(szSequence, STR_RandomSequence))
		*pSequence = RandomSequence;
	else {
		hr = E_FAIL;
	}
	return hr;
}

HRESULT ParseState(HELEMENT hState, FILTER_STATE* pState)
{
	HRESULT hr = S_OK;

	if (!pState)
		return E_POINTER;

	const char* szState = NULL;
	szState = XMLGetElementValue(hState);
	if (!szState)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse state."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_FAIL;
	}
	else {
		hr = ToState(szState, pState);
	}

	return hr;
}

HRESULT ParseDWORD(HELEMENT hDWord, DWORD* pDWord)
{
	HRESULT hr = S_OK;

	if (!pDWord)
		return E_POINTER;

	const char* szDWord = NULL;
	szDWord = XMLGetElementValue(hDWord);
	if (!szDWord)
	{
		LOG(TEXT("ParseDWord: did not find parseable DWord."));
		hr = E_FAIL;
	}
	else {
		// Convert from string to DWord
		TCHAR tszDWord[64];
		CharToTChar(szDWord, tszDWord, countof(tszDWord));
		*pDWord = (DWORD)_ttoi(tszDWord);
		hr = S_OK;
	}

	return hr;
}

HRESULT ParseDWORD(const char* szDWord, DWORD* pDWord)
{
	HRESULT hr = S_OK;

	if (!szDWord)
		return E_POINTER;

	// Convert from string to DWord
	TCHAR tszDWord[64];
	CharToTChar(szDWord, tszDWord, countof(tszDWord));
	*pDWord = (DWORD)_ttoi(tszDWord);
	hr = S_OK;

	return hr;
}

HRESULT ParseLONGLONG(HELEMENT hDWord, LONGLONG* pllInt)
{
	HRESULT hr = S_OK;

	if (!pllInt)
		return E_POINTER;

	const char* szDWord = NULL;
	szDWord = XMLGetElementValue(hDWord);
	if (!szDWord)
	{
		LOG(TEXT("ParseDWord: did not find parseable 64 bit integer."));
		hr = E_FAIL;
	}
	else {
		// Convert from string to DWord
		TCHAR tszDWord[64];
		CharToTChar(szDWord, tszDWord, countof(tszDWord));
		*pllInt = (LONGLONG)_ttoi(tszDWord);
		hr = S_OK;
	}

	return hr;
}

HRESULT ParseLONGLONG(const char* szLLInt, LONGLONG* pllInt)
{
	HRESULT hr = S_OK;

	if (!szLLInt)
		return E_POINTER;

	// Convert from string to longlong
	TCHAR tszDWord[64];
	CharToTChar(szLLInt, tszDWord, countof(tszDWord));
	*pllInt = (LONGLONG)_ttoi64(tszDWord);
	hr = S_OK;

	return hr;
}

HRESULT ParseRect(HELEMENT hRect, RECT* pRect)
{
	HRESULT hr = S_OK;

	if (!pRect)
		return E_POINTER;

	const char* szRect = XMLGetElementValue(hRect);
	if (!szRect)
		return E_FAIL;

	char szValue[8];
	int pos = 0;

	// Parse the left of the rect
	ParseStringListString(szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos);
	if (pos == -1)
		return E_FAIL;
	pRect->left = atol(szValue);

	// Parse the top of the rect
	ParseStringListString(szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos);
	if (pos == -1)
		return E_FAIL;
	pRect->top = atol(szValue);

	// Parse the width of the rect
	ParseStringListString(szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos);
	if (pos == -1)
		return E_FAIL;
	pRect->right = atol(szValue);

	// Parse the height of the rect
	ParseStringListString(szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos);
	pRect->bottom = atol(szValue);

	return hr;
}

HRESULT ParseString(HELEMENT hString, TCHAR* szString, int length)
{
	const char* szCharString = XMLGetElementValue(hString);
	return (CharToTChar(szCharString, szString, length)) ? S_OK : NULL;
}

HRESULT ParseStringListString(const char* szStringList, const char delimiter, char* szString, int len, int& pos)
{
	// Temp stirng buffer
	char szTempString[MAX_PATH];
	int length = 0;

	if (len <= 0)
		return E_INVALIDARG;

	char formatstr[32];
	// "%[^:]s"
	sprintf(formatstr, "%%[^%c]s", delimiter);

	// BUGBUG: Parse the string at the specified position (delimited by the delimiter
	if (!sscanf(szStringList + pos, formatstr, szTempString))
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to parse string list."), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}
	
	// Length of string read in
	length = (int)strlen(szTempString);
	
	// Move beyond the string read in
	pos = pos + length;
	
	// If there was a comma - then there might be one more string - so move position to the next string
	// If not or if there was no string read in, then return end of string as -1
	if (*(szStringList + pos) == delimiter)
		pos++;
	else if (!length || (*(szStringList + pos) == '\0'))
		pos = -1;

	// Copy the parsed string to the pointer passed in
	strncpy(szString, szTempString, len);

	return S_OK;
}

HRESULT ParseStringList(HELEMENT hStringList, StringList* pStringList, char delimiter)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szStringList = XMLGetElementValue(hStringList);
	if (!szStringList)
		return S_OK;
	char szString[MAX_PATH];

	int pos = 0;
	while(pos != -1)
	{
		hr = ParseStringListString(szStringList, delimiter, szString, countof(szString), pos);
		if (FAILED(hr))
			break;

		int len = (int)strlen(szString);
		TCHAR* tszString = new TCHAR[len + 1];
		if (!tszString)
		{
			LOG(TEXT("%S: ERROR %d@%s - failed to allocate temporary string when parsing string list."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
			break;
		}

		CharToTChar(szString, tszString, len);
		tszString[len] = 0;

		//LOG(TEXT("Parsed string %S -> %s"), szString, tszString);
		pStringList->push_back(tszString);
	}
	
	return hr;
}


HRESULT ParseGraphDesc(HELEMENT hElement, GraphDesc* pGraphDesc)
{
	HRESULT hr = S_OK;

	// Parse the string list - reading in the filters, 
	// BUGBUG: don't handle branched graphs
	hr = ParseStringList(hElement, &pGraphDesc->filterList, ':');

	return hr;
}

HRESULT ParseTestDesc(HELEMENT hTestDesc, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;

	// We don't have a test desc structure yet to store the name and desc - need temp storage
	TCHAR szTestName[TEST_NAME_LENGTH] = TEXT("");
	TCHAR szTestDesc[TEST_DESC_LENGTH] = TEXT("");

	HATTRIBUTE hAttrib = XMLGetFirstAttribute(hTestDesc);
	if (hAttrib) {
		// Get name of the test node
		const char *szName = NULL;
		if (!strcmp(STR_Name, XMLGetAttributeName(hAttrib)))
		{
			szName = XMLGetAttributeValue(hAttrib);

			// Convert name to TCHAR
			CharToTChar(szName, szTestName, countof(szTestName));

			// Move to the next attribute
			hAttrib = XMLGetNextAttribute(hAttrib);
		}
		else {
			LOG(TEXT("%S: ERROR %d@%S - failed to find test name attribute."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_FAIL;
		}
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - test node has no test name attribute."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr) && hAttrib)
	{
		const char* szDesc = NULL;
		if (!strcmp(STR_Desc, XMLGetAttributeName(hAttrib)))
		{
			szDesc = XMLGetAttributeValue(hAttrib);

			// Convert name to TCHAR
			CharToTChar(szDesc, szTestDesc, countof(szTestDesc));

			// Move to the next attribute
			hAttrib = XMLGetNextAttribute(hAttrib);
		}
	}

	// From test name, determine function to be called
	ParseTestFunction parsefn = GetParseFunction(szTestName);
	if (!parsefn)
	{
		LOG(TEXT("%S: ERROR %d@%S - unknown test name %s - failed to determine test function."), __FUNCTION__, __LINE__, __FILE__, szTestName);
		hr = E_FAIL;
	}
	else {
		hr = parsefn(hTestDesc, ppTestDesc);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse test %s."), __FUNCTION__, __LINE__, __FILE__, szTestName);
		}
		else {
			// Set the test name
			_tcsncpy((*ppTestDesc)->szTestName, szTestName, countof((*ppTestDesc)->szTestName));

			// Set the test desc
			_tcsncpy((*ppTestDesc)->szTestDesc, szTestDesc, countof((*ppTestDesc)->szTestDesc));
		}
	}

	return hr;
}

HRESULT ParseTestDescList(HELEMENT hElement, TestDescList* pTestDescList, TestGroupList* pTestGroupList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;
	HELEMENT hXmlGroup = 0;
	HELEMENT hAttrib = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	int nTests = 0;

	TestGroup* pTestGroup = NULL;
	while (hXmlChild)
	{
		// Check if this is a group
		if (!strcmp(STR_TestGroup, XMLGetElementName(hXmlChild)))
		{
			// Get the test group's name
			const char* szGroupName = NULL;
			DWORD testIdBase = -1;

			// Get the test group XML element
			hXmlGroup = hXmlChild;

			// Get the name attribute
			hAttrib = XMLGetFirstAttribute(hXmlGroup);
			if (hAttrib && !strcmp(STR_Name, XMLGetAttributeName(hAttrib)))
			{
				// Get the test group's name
				szGroupName = XMLGetAttributeValue(hAttrib);
			}
			else {
				LOG(TEXT("%S: ERROR %d@%S - failed to find TestGroup name."), __FUNCTION__, __LINE__, __FILE__);
				hr = E_FAIL;
				break;
			}

			// Get the TestIdBase attribute if present
			hAttrib = XMLGetNextAttribute(hAttrib);
			if (hAttrib)
			{
				const char* szTestIdBase = XMLGetAttributeValue(hAttrib);
				testIdBase = atol(szTestIdBase);
			}

			// Allocate a test group object
			pTestGroup = new TestGroup();
			if (!pTestGroup)
			{
				hr = E_OUTOFMEMORY;
				LOG(TEXT("%S: ERROR %d@%S - failed to allocate TestGroup object."), __FUNCTION__, __LINE__, __FILE__);
				break;
			}

			// Copy the group name
			CharToTChar(szGroupName, pTestGroup->szGroupName, countof(pTestGroup->szGroupName));
			pTestGroup->testIdBase = testIdBase;

			// Get the xml test objects to add to this group
			hXmlChild = XMLGetFirstChild(hXmlGroup);

			// Add the test group to the test group list
			pTestGroupList->push_back(pTestGroup);
		}

		// If there is a test in this group or list, then parse it
		if (hXmlChild && !strcmp(STR_TestDesc, XMLGetElementName(hXmlChild)))
		{
			TestDesc* pTestDesc = NULL;
			hr = ParseTestDesc(hXmlChild, &pTestDesc);
			if (pTestDesc)
			{
				// Add it to the test list
				pTestDescList->push_back(pTestDesc);

				// Add it to the test group
				if (pTestGroup)
					pTestGroup->testDescList.push_back(pTestDesc);
			}

			if ( FAILED(hr) )
			{
				LOG(TEXT("%S: ERROR %d@%S - failed to parse Test."), __FUNCTION__, __LINE__, __FILE__);
				break;
			}
			else nTests++;
		}

		// If there is a test in this group or list, then try going to the next one
		if (hXmlChild)
			hXmlChild = XMLGetNextSibling(hXmlChild);

		// If we went to the end of the list and there is a group, pop back to the next group
		if (!hXmlChild && hXmlGroup)
		{
			hXmlChild = XMLGetNextSibling(hXmlGroup);
			hXmlGroup = 0;
		}
	}

	// Return an error if we found no tests
	if (SUCCEEDED(hr) && !nTests)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to find Test."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_FAIL;
	}

	return hr;
}

HRESULT ParseWaitOp(HELEMENT hElement, Operation** pOp)
{
	HRESULT hr = S_OK;

	WaitOp *pWaitOp = new WaitOp();
	if (!pWaitOp)
		return E_OUTOFMEMORY;

	// Parse the wait time
	DWORD time = 0;
	hr = ParseDWORD(hElement, &time);
	if (SUCCEEDED(hr))
	{		
		pWaitOp->time = time;
		if (pOp)
			*pOp = pWaitOp;
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse WaitOp time."), __FUNCTION__, __LINE__, __FILE__);	
	}

	if (FAILED(hr) || !pOp)
		delete pWaitOp;
	
	return hr;
}

HRESULT ParseSwitchGDIOp(HELEMENT hElement, Operation** pOp)
{
	HRESULT hr = S_OK;

	SwitchGDIOp *pSwitchGDIOp = new SwitchGDIOp();
	if (!pSwitchGDIOp)
		return E_OUTOFMEMORY;

	if (pOp)
		*pOp = pSwitchGDIOp;

	if (FAILED(hr) || !pOp)
		delete pSwitchGDIOp;
	
	return hr;
}

HRESULT ParseSwitchDDrawOp(HELEMENT hElement, Operation** pOp)
{
	HRESULT hr = S_OK;

	SwitchDDrawOp *pSwitchDDrawOp = new SwitchDDrawOp();
	if (!pSwitchDDrawOp)
		return E_OUTOFMEMORY;

	if (pOp)
		*pOp = pSwitchDDrawOp;

	if (FAILED(hr) || !pOp)
		delete pSwitchDDrawOp;
	
	return hr;
}

HRESULT ParseOperation(HELEMENT hElement, Operation** pOp)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	char szOpName[MAX_OP_NAME_LEN];
	const char* szOp = XMLGetElementValue(hElement);
	
	// Parse the op name
	int pos = 0;
	ParseStringListString(szOp, STR_OP_DELIMITER, szOpName, countof(szOpName), pos);

	if (!strcmp(STR_WaitOp, szOpName))
	{
		hr = ParseWaitOp(hXmlChild, pOp);
	}
	else if (!strcmp(STR_SwitchGDIOp, szOpName))
	{
		hr = ParseSwitchGDIOp(hXmlChild, pOp);
	}
	else if (!strcmp(STR_SwitchDDrawOp, szOpName))
	{
		hr = ParseSwitchDDrawOp(hXmlChild, pOp);
	}
	else {
		hr = E_FAIL;
	}

	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse %s: %s."), __FUNCTION__, __LINE__, __FILE__, XMLGetElementName(hXmlChild), XMLGetElementValue(hXmlChild));	
	}

	return hr;
}

HRESULT ParseOpList(HELEMENT hElement, OpList* pOpList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	while (true)
	{
		if (!strcmp(STR_Op, XMLGetElementName(hXmlChild)))
		{
			Operation* pOp = NULL;
			hr = ParseOperation(hXmlChild, &pOp);
			pOpList->push_back(pOp);
			if (FAILED(hr))
				break;
		}
		else {
			hr = E_FAIL;
			break;
		}

		hXmlChild = XMLGetNextSibling(hXmlChild);
		if (hXmlChild == 0)
			return S_OK;
	}
	if (FAILED(hr))
		LOG(TEXT("%S: ERROR %d@%S - failed to parse oplist."), __FUNCTION__, __LINE__, __FILE__);	

	return hr;
}

HRESULT ParseMediaList(HELEMENT hElement, MediaList* pMediaList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	// Check the attributes of the media list element for a container file for the media list
	HATTRIBUTE hAttrib = XMLGetFirstAttribute(hElement);
	while (hAttrib) {
		// We are only looking for the Container attribute
		if (!strcmp(STR_Container, XMLGetAttributeName(hAttrib)))
		{
			const char* szContainer = XMLGetAttributeValue(hAttrib);
			// If the file name is invalid, return an error
			if (!szContainer || !strcmp("", szContainer))
				return E_FAIL;
			else {
				// Parse the media list file
				TCHAR tszContainer[MAX_PATH];
				CharToTChar(szContainer, tszContainer, countof(tszContainer));
				return ParseMediaListFile(tszContainer, pMediaList);
			}
		}
		// Move to the next attribute
		hAttrib = XMLGetNextAttribute(hAttrib);
	}

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	while ( true )
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			Media* pMedia = new Media();
			if (!pMedia)
			{	
				LOG(TEXT("%S: ERROR %d@%S - failed to allocate media object when parsing."), __FUNCTION__, __LINE__, __FILE__);
				hr = E_OUTOFMEMORY;
				break;
			}

			hr = ParseMedia(hXmlChild, pMedia);
			pMediaList->push_back(pMedia);
			if (FAILED(hr))
				break;

		}
		else {
			hr = E_FAIL;
			break;
		}

		hXmlChild = XMLGetNextSibling(hXmlChild);
		if (hXmlChild == 0)
			return S_OK;
	}
	return hr;
}

HRESULT ParseFilterId(HELEMENT hElement, FilterId* pFilterId)
{
	return S_OK;
}

HRESULT ParseFilterDesc(HELEMENT hElement, FilterDesc* pFilterDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hElement == 0)
		return E_FAIL;


	hr = ParseFilterId(hXmlChild, &pFilterDesc->m_id);
	if ( FAILED(hr) )
		return hr;

	return hr;
}

HRESULT ParseFilterDescList(HELEMENT hElement, FilterDescList* pFilterDescList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hElement == 0)
		return S_OK;

	int i = 0;
	while (true)
	{
		hXmlChild = XMLGetNextSibling(hXmlChild);
		if (hElement == 0)
			return S_OK;

		FilterDesc* pFilterDesc = new FilterDesc();
		if (!pFilterDesc)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate FilterDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
			break;
		}

		hr = ParseFilterDesc(hXmlChild, pFilterDesc);
		if ( FAILED(hr) )
			return hr;

		pFilterDescList->push_back(pFilterDesc);
	}
	return hr;
}

HRESULT ParseBaseUrl(HELEMENT hElement, BaseUrl *pBaseUrl)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szProtocol = XMLGetElementName(hElement);
	const char* szBaseUrl = XMLGetElementValue(hElement);

	if (!CharToTChar(szProtocol, pBaseUrl->szProtocolName, countof(pBaseUrl->szProtocolName)))
		hr = E_FAIL;
	else {
		if (!CharToTChar(szBaseUrl, pBaseUrl->szBaseUrl, countof(pBaseUrl->szBaseUrl)))
			hr = E_FAIL;
	}
	return hr;
}

HRESULT ParseBaseUrls(HELEMENT hElement, BaseUrl *pBaseUrl)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return E_FAIL;

	int i = 0;
	while(hXmlChild)
	{
		hr = ParseBaseUrl(hXmlChild, &pBaseUrl[i]);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse base url %s: %s."), __FUNCTION__, __LINE__, __FILE__, XMLGetElementName(hXmlChild), XMLGetElementValue(hXmlChild));
			break;
		}
		else {
			i++;
			hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}
	return hr;
}

HRESULT ParseTimeFormatPara(HELEMENT hElement, CMediaProp* pMediaProp, TimeFormat enFormat )
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return E_FAIL;

	if ( hXmlChild && !strcmp(STR_TIME_EARLIEST, XMLGetElementName(hXmlChild)))
	{
		switch( enFormat )
		{
		case TIME_MEDIA_TIME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llMediaTimeEarliest);
			break;
		case TIME_FIELD:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFieldEarliest);
			break;
		case TIME_SAMPLE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llSampleEarliest);
			break;
		case TIME_BYTE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llByteEarliest);
			break;
		case TIME_FRAME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFrameEarliest);
			break;
		}
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the Earliest tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if ( hXmlChild && !strcmp(STR_TIME_LATEST, XMLGetElementName(hXmlChild)))
	{
		switch( enFormat )
		{
		case TIME_MEDIA_TIME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llMediaTimeLatest);
			break;
		case TIME_FIELD:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFieldLatest);
			break;
		case TIME_SAMPLE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llSampleLatest);
			break;
		case TIME_BYTE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llByteLatest);
			break;
		case TIME_FRAME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFrameLatest);
			break;
		}
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the Latest tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if ( hXmlChild && !strcmp(STR_TIME_DURATION, XMLGetElementName(hXmlChild)))
	{
		switch( enFormat )
		{
		case TIME_MEDIA_TIME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llMediaTimeDuration);
			break;
		case TIME_FIELD:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFieldDuration);
			break;
		case TIME_SAMPLE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llSampleDuration);
			break;
		case TIME_BYTE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llByteDuration);
			break;
		case TIME_FRAME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFrameDuration);
			break;
		}
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the Duration tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if ( hXmlChild && !strcmp(STR_TIME_PREROLL, XMLGetElementName(hXmlChild)))
	{
		switch( enFormat )
		{
		case TIME_MEDIA_TIME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llMediaTimePreroll);
			pMediaProp->m_bMediaTimePreroll = true;
			break;
		case TIME_FIELD:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFieldPreroll);
			pMediaProp->m_bFieldPreroll = true;
			break;
		case TIME_SAMPLE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llSamplePreroll);
			pMediaProp->m_bSamplePreroll = true;
			break;
		case TIME_BYTE:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llBytePreroll);
			pMediaProp->m_bBytePreroll = true;
			break;
		case TIME_FRAME:
			hr = ParseLONGLONG(hXmlChild, &pMediaProp->m_llFramePreroll);
			pMediaProp->m_bFramePreroll = true;
			break;
		}
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the Preroll tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
	}

	return hr;
}


HRESULT ParseMediaProps(HELEMENT hElement, CMediaProp* pMediaProp)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return E_FAIL;

	if (!strcmp(STR_SEEK_CAPS, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pMediaProp->m_dwSeekCaps);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the SeekCaps tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	
	if (hXmlChild && !strcmp(STR_TIME_MEDIA_TIME, XMLGetElementName(hXmlChild)))
	{
		hr = ParseTimeFormatPara(hXmlChild, pMediaProp, TIME_MEDIA_TIME);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the TimeMediaTime tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (hXmlChild && !strcmp(STR_TIME_FIELD, XMLGetElementName(hXmlChild)))
	{
		hr = ParseTimeFormatPara(hXmlChild, pMediaProp, TIME_FIELD);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the TimeField tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if (hXmlChild && !strcmp(STR_TIME_SAMPLE, XMLGetElementName(hXmlChild)))
	{
		hr = ParseTimeFormatPara(hXmlChild, pMediaProp, TIME_SAMPLE);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the TimeSample tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if (hXmlChild && !strcmp(STR_TIME_BYTE, XMLGetElementName(hXmlChild)))
	{
		hr = ParseTimeFormatPara(hXmlChild, pMediaProp, TIME_BYTE);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the TimeByte tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if (hXmlChild && !strcmp(STR_TIME_FRAME, XMLGetElementName(hXmlChild)))
	{
		hr = ParseTimeFormatPara(hXmlChild, pMediaProp, TIME_FRAME);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the TimeFrame tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if (SUCCEEDED(hr) && hXmlChild)
	{
	}

	return hr;
}


HRESULT ParseVideoInfo(HELEMENT hElement, VideoInfo* pVideoInfo)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return E_FAIL;

	if (!strcmp(STR_NumFrames, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pVideoInfo->nFrames);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the dropped frames tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	
	if (hXmlChild && !strcmp(STR_Index, XMLGetElementName(hXmlChild)))
	{
		hr = ParseString(hXmlChild, pVideoInfo->szIndexFile, countof(pVideoInfo->szIndexFile));
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the index file tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if ( hXmlChild && !strcmp(STR_VIDEO_AVG_FRAME_RATE, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropAvgFrameRate);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the AvgFrameRate tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if ( hXmlChild && !strcmp(STR_VIDEO_AVG_SYNC_OFFSET, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropAvgSyncOffset);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the AvgSyncOffset tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if ( hXmlChild && !strcmp(STR_VIDEO_DEV_SYNC_OFFSET, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropDevSyncOffset);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the DevSyncOffset tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if ( hXmlChild && !strcmp(STR_VIDEO_FRAME_DRAWN, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropFramesDrawn);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the FrameDrawn tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	if ( hXmlChild && !strcmp(STR_VIDEO_FRAME_DROPPED, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropFramesDroppedInRenderer);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the FramesDropped tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}	
	if ( hXmlChild && !strcmp(STR_VIDEO_JITTER, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, (DWORD *)&pVideoInfo->m_nQualPropJitter);
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the Jitter tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
	}

	return hr;
}

HRESULT ParseAudioInfo(HELEMENT hElement, AudioInfo* pAudioInfo)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);

	if (hXmlChild && !strcmp(STR_Index, XMLGetElementName(hXmlChild)))
	{
		hr = ParseString(hXmlChild, pAudioInfo->szIndexFile, countof(pAudioInfo->szIndexFile));
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse the index file tag."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	return hr;
}


HRESULT ParseMedia(HELEMENT hElement, Media* pMedia)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return E_FAIL;

	if (!strcmp(STR_Name, XMLGetElementName(hXmlChild)))
	{
		hr = ParseString(hXmlChild, pMedia->szName, countof(pMedia->szName));
		if (FAILED(hr))
			LOG(TEXT("%S: ERROR %d@%S - failed to parse media name."), __FUNCTION__, __LINE__, __FILE__);
		else hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_FileName, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pMedia->szFileName, countof(pMedia->szFileName));
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse media file name."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Description, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pMedia->szDescription, countof(pMedia->szDescription));
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse media description."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_IsVideo, XMLGetElementName(hXmlChild)))
		{
			if ( !strcmp( "true", XMLGetElementValue( hXmlChild) ) )
				pMedia->m_bIsVideo = true;
			else
				pMedia->m_bIsVideo = false;
			
			hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_BaseUrls, XMLGetElementName(hXmlChild)))
		{
			hr = ParseBaseUrls(hXmlChild, pMedia->baseUrls);
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse base urls."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	// basic media properties.
	CMediaProp tempProp;	// save to a temporary place.
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_MEDIA_PROP, XMLGetElementName(hXmlChild)))
		{
			hr = ParseMediaProps(hXmlChild, &tempProp);
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse base media properties."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_PlaybackDuration, XMLGetElementName(hXmlChild)))
		{
			DWORD duration;
			hr = ParseDWORD(hXmlChild, &duration);
			// Convert to 100 ns units
			pMedia->duration = duration*10000;
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse duration tag."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_VideoInfo, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVideoInfo(hXmlChild, &pMedia->videoInfo);
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse video info."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);

			// get the basic media prop
			*( (CMediaProp *)&pMedia->videoInfo ) = tempProp;
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_AudioInfo, XMLGetElementName(hXmlChild)))
		{
			hr = ParseAudioInfo(hXmlChild, &pMedia->audioInfo);
			if ( FAILED(hr) )
				LOG(TEXT("%S: ERROR %d@%S - failed to parse audio info."), __FUNCTION__, __LINE__, __FILE__);
			else hXmlChild = XMLGetNextSibling(hXmlChild);
			// get the basic media prop
			*( (CMediaProp *)&pMedia->audioInfo ) = tempProp;
		}
	}

	return hr;
}

HRESULT ParseFilterType(const char* szFilterMajorClass, const char* szFilterSubClass, DWORD* pFilterType)
{
	// BUGBUG:
	*pFilterType = ToSubClass(VideoDecoder);
	return S_OK;}

HRESULT ParsePinDirection(HELEMENT hElement, PIN_DIRECTION* pPinDir)
{
	// BUGBUG:
	*pPinDir = PINDIR_INPUT;
	return S_OK;
}

HRESULT ParseMediaTypeMajor(HELEMENT hElement, GUID* pMajorType)
{
	// BUGBUG:
	*pMajorType = MEDIATYPE_Video;
	return S_OK;
}

HRESULT ParseGraphEvent(HELEMENT hElement, GraphEvent* pGraphEvent)
{
	// BUGBUG:
	*pGraphEvent = END_FLUSH;
	return S_OK;
}

HRESULT AddVerification(VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;
	pVerificationList->insert(VerificationPair(type, NULL));
	return hr;
}

HRESULT ParseVerifyDWORD(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	DWORD dwValue= 0;
	hr = ParseDWORD(hVerification, &dwValue);
	if (SUCCEEDED(hr))
	{
		DWORD* pDWord = new DWORD;
		if (!pDWord)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate tempory DWORD when parsing verification."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
		}
		else 
		{
			*pDWord = dwValue;
			pVerificationList->insert(VerificationPair(type, pDWord));
		}
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse verification DWORD value."), __FUNCTION__, __LINE__, __FILE__);
	}
	return hr;
}

HRESULT ParseVerifyRECT(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	RECT rect = {};
	hr = ParseRect(hVerification, &rect);
	if (SUCCEEDED(hr))
	{
		RECT* pRect = new RECT;
		if (!pRect)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate tempory RECT when parsing verification."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
		}
		else 
		{
			*pRect = rect;
			pVerificationList->insert(VerificationPair(type, pRect));
		}
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse verification RECT value."), __FUNCTION__, __LINE__, __FILE__);
	}
	return hr;
}

HRESULT ParseVerifyGraphDesc(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	GraphDesc* pGraphDesc = new GraphDesc();
	if (!pGraphDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate TestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		hr = ParseGraphDesc(hVerification, pGraphDesc);
		if (SUCCEEDED(hr))
		{
			pVerificationList->insert(VerificationPair(type, (void*)pGraphDesc));
		}
		else {
			delete pGraphDesc;
			LOG(TEXT("%S: ERROR %d@%S - failed to parse verification graph description."), __FUNCTION__, __LINE__, __FILE__);
		}
	}

	return hr;
}

HRESULT ParseVerifyRendererMode(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	VideoRendererMode mode = 0;
	hr = ParseRendererMode(XMLGetElementValue(hVerification), &mode);
	if (SUCCEEDED(hr))
	{
		VideoRendererMode* pMode = new VideoRendererMode;
		if (!pMode)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate space for VideoRendererMode when parsing verification."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
		}
		else 
		{
			*pMode = mode;
			pVerificationList->insert(VerificationPair(type, pMode));
		}
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse video renderer mode verification %S."), __FUNCTION__, __LINE__, __FILE__, XMLGetElementValue(hVerification));
	}
	return hr;
}

HRESULT ParseVerifyRendererSurfaceType(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	DWORD surfaceType = 0;
	hr = ParseSurfaceType(XMLGetElementValue(hVerification), &surfaceType);
	if (SUCCEEDED(hr))
	{
		DWORD* pSurfaceType = new DWORD;
		if (!pSurfaceType)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate space for surface type when parsing verification."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
		}
		else 
		{
			*pSurfaceType = surfaceType;
			pVerificationList->insert(VerificationPair(type, pSurfaceType));
		}
	}
	else {
		LOG(TEXT("%S: ERROR %d@%S - failed to parse video renderer surface type verification %S."), __FUNCTION__, __LINE__, __FILE__, XMLGetElementValue(hVerification));
	}
	return hr;
}

HRESULT ParseVerifySampleDelivered(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	// Temp to store parsed data
	SampleDeliveryVerifierData data;
	
	// Extract the string
	const char* szVerifySampleDeliveredData = XMLGetElementValue(hVerification);
	if (!szVerifySampleDeliveredData)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to extract VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Now parse the string list into the component elements
	int pos = 0;
	char szString[128] = "";
	// Parse the filter sub type
	hr = ParseStringListString(szVerifySampleDeliveredData, STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	hr = ParseFilterType(NULL, szString, &data.filterType);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered filter type data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the pin direction
	hr = ParseStringListString(szVerifySampleDeliveredData, STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	hr = ParsePinDirection(szString, &data.pindir);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered pindir data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the media type
	hr = ParseStringListString(szVerifySampleDeliveredData, STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	hr = ParseMediaTypeMajor(szString, &data.mediaMajorType);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered major type data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the tolerance
	hr = ParseStringListString(szVerifySampleDeliveredData, STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	hr = ParseDWORD(szString, &data.tolerance);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered tolerance data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the event
	hr = ParseStringListString(szVerifySampleDeliveredData, STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	hr = ParseGraphEvent(szString, &data.event);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered event data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Add the verification data to the verifier list
	SampleDeliveryVerifierData* pData = new SampleDeliveryVerifierData;
	if (!pData)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate space for SampleDeliveryVerifierData."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
	}
	else 
	{
		*pData = data;
		pVerificationList->insert(VerificationPair(type, pData));
	}

	return hr;
}

HRESULT ParseVerifyPlaybackDuration(HELEMENT hVerification, VerificationType type, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;

	// Temp to store parsed data
	PlaybackDurationTolerance data;
	
	// Extract the string
	const char* szVerifyPlaybackDurationData = XMLGetElementValue(hVerification);
	if (!szVerifyPlaybackDurationData)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to extract VerifyPlaybackDuration data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Now parse the string list into the component elements
	int pos = 0;
	char szString[128] = "";
	
	// Parse the pct duration tolerance
	hr = ParseStringListString(szVerifyPlaybackDurationData, STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifyPlaybackDuration data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	hr = ParseDWORD(szString, &data.pctDurationTolerance);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse pct duration tolerance."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the abs duration tolerance
	hr = ParseStringListString(szVerifyPlaybackDurationData, STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse VerifyPlaybackDuration data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	hr = ParseDWORD(szString, &data.absDurationTolerance);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse abs duration tolerance."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Add the verification data to the verifier list
	PlaybackDurationTolerance* pData = new PlaybackDurationTolerance;
	if (!pData)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate space for PlaybackDurationTolerance."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
	}
	else 
	{
		*pData = data;
		pVerificationList->insert(VerificationPair(type, pData));
	}

	return hr;
}

extern VerifierMgr g_verifierMgr;

HRESULT ParseVerification(HELEMENT hVerification, VerificationList* pVerificationList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hVerification);
	// It's ok if we don't have a child. No verification is specified.
	if (!hXmlChild)
		return S_OK;

	int nVerificationParserTableEntries = g_verifierMgr.GetNumVerifiers();

	// For each verification type specified, parse the type and value
	while(hXmlChild && SUCCEEDED_F(hr))
	{
		const char* szVerificationType = XMLGetElementName(hXmlChild);
		int i = 0;
		VerifierObj* vTemp = NULL;
		for(; i < nVerificationParserTableEntries; i++)
		{
			vTemp = g_verifierMgr.GetVerifier(i);
			// Check the name for a match
			if (!strcmp(vTemp->szVerificationType, szVerificationType))
			{
				// If there is a parser method specified, call it
				if (vTemp->pVerificationParserFunction)
				{
					hr = vTemp->pVerificationParserFunction(hXmlChild, vTemp->verificationType, pVerificationList);
					if (SUCCEEDED(hr))
						hXmlChild = XMLGetNextSibling(hXmlChild);
				}
				else {
					// Otherwise just add the verification type to the list
					hr = AddVerification(vTemp->verificationType, pVerificationList);
					if (SUCCEEDED(hr))
						hXmlChild = XMLGetNextSibling(hXmlChild);
				}

				// break out if we found the table entry
				break;
			}
		}

		// If we failed to parse the verification, skip the remaining verifications
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse verification type %S."), __FUNCTION__, __LINE__, __FILE__, szVerificationType);
			break;
		}

		// If we reached the end of the parser table, we didn't find a match
		if (i == nVerificationParserTableEntries)
		{
			LOG(TEXT("%S: ERROR %d@%S - unknown verification type %S."), __FUNCTION__, __LINE__, __FILE__, szVerificationType);
			hr = E_FAIL;
			break;
		}
	}

	return hr;
}

HRESULT ParseTestConfig(HELEMENT hElement, TestConfig* pTestConfig)
{
	HRESULT hr = S_OK;
	
	// Parse the attributes
	HATTRIBUTE hAttrib = XMLGetFirstAttribute(hElement);
	while (hAttrib) {
		// We are only looking for the GenerateId attribute
		if (!strcmp(STR_GenerateId, XMLGetAttributeName(hAttrib)))
		{
			const char* szGenerateId = XMLGetAttributeValue(hAttrib);
			if (!strcmp(STR_True, szGenerateId))
				pTestConfig->bGenerateId = true;
			break;
		}
		// Move to the next attribute
		hAttrib = XMLGetNextAttribute(hAttrib);

	}

	// Parse the children
	HELEMENT hXmlChild = XMLGetFirstChild(hElement);
	if (hXmlChild == 0)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to find MediaList."), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}

	if (!strcmp(STR_MediaList, XMLGetElementName(hXmlChild)))
	{
		hr = ParseMediaList(hXmlChild, &pTestConfig->mediaList);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr))
	{
		if (hXmlChild)
		{
			if (!strcmp(STR_TestList, XMLGetElementName(hXmlChild)))
			{
				// Get the test and group list pointers from the test config structure
				hr = ParseTestDescList(hXmlChild, &pTestConfig->testDescList, &pTestConfig->testGroupList);
				if (FAILED(hr))
					LOG(TEXT("%S: ERROR %d@%S - failed to parse TestList."), __FUNCTION__, __LINE__, __FILE__);
			}
		}
		else {
			LOG(TEXT("%S: ERROR %d@%S - failed to find TestList."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_FAIL;
		}
	}

	return hr;
}

HRESULT ParseMediaListFile(TCHAR* szMediaListFile, MediaList* pMediaList)
{
	HRESULT hr = S_OK;
	
	// If the caller has not specified a config file or pTestDesc is NULL
	if (!szMediaListFile)
		return E_INVALIDARG;
	
	// Open the config file
	HANDLE hFile = CreateFile(szMediaListFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to open file %s."), __FUNCTION__, __LINE__, __FILE__, szMediaListFile);
		return E_INVALIDARG;
	}

	// Read the size of the file
	DWORD size = 0;
	size = GetFileSize(hFile, 0);
	if (size == -1)
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to read the file size."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		return E_FAIL;
	}

	// Allocate space for the whole file and read it in completely
	char* xmlMediaList = new char[size + 1];
	if (!xmlMediaList)
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to allocate memory for the media list file."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		return E_OUTOFMEMORY;
	}

	DWORD nBytesRead = 0;
	BOOL bRead = ReadFile(hFile, xmlMediaList, size, &nBytesRead, NULL);
	if (!bRead || (nBytesRead != size))
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to read the complete file."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		delete [] xmlMediaList;
		return E_FAIL;
	}
	
	// Insert a string terminator after reading in the file - which is now an XML string
	xmlMediaList[size] = 0;

	// Parse the XML string
	HELEMENT hXmlRoot = NULL;
    DWORD rc;

    rc = XMLParse(xmlMediaList, &hXmlRoot);
	if (rc || !hXmlRoot)
	{
		LOG(TEXT("%S ERROR %d@%S: XMLParse failed to parse the config file."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_FAIL;
	}
	else {
		// Parse the media list from the xml string
		hr = ParseMediaList(hXmlRoot, pMediaList);
		if (FAILED(hr))
			LOG(TEXT("%S: ParseMediaList failed."), __FUNCTION__);
	}

	// Free resources and return
	if (hXmlRoot)
		XMLFree(hXmlRoot);

	if (xmlMediaList)
		delete [] xmlMediaList;

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);

	return hr;
}

TestConfig* ParseConfigFile(TCHAR* szXmlConfig)
{
	TestConfig* pTestConfig = NULL;
	HRESULT hr = NOERROR;
	
	// If the caller has not specified a config file or pTestDesc is NULL
	if (!szXmlConfig)
		return NULL;
	
	// Open the config file
	HANDLE hFile = CreateFile(szXmlConfig, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		//if we didn't actually get the complete path of the file, let's try if we can find the file in the current folder
		if(szXmlConfig[0] != '\\') {
		
			LOG(TEXT("%S: WARNING %d@%S - failed to open file %s"), __FUNCTION__, __LINE__, __FILE__, szXmlConfig);

			// Try to see if maybe we can find the file in our current directory.
			TCHAR szDir[MAX_PATH];
			DWORD dwActual, i =0;
			dwActual= GetModuleFileName(NULL, szDir, MAX_PATH);
			if(dwActual) {
				i = dwActual -1;
				while( i >=0 && szDir[i]!='\\')
					i--;
				i++;
			}		
			szDir[i] = '\0';

			LOG(TEXT("%S: Trying to see if we can find the file in the current directory: %s"), __FUNCTION__, szDir);		

			//make sure we never try to write more data than we have buffer
			if((wcsnlen(szXmlConfig, MAX_PATH) + wcsnlen(szDir, MAX_PATH) + 1) <= MAX_PATH) {
				
				wcsncat(szDir, szXmlConfig, wcsnlen(szXmlConfig, MAX_PATH)); 
				
				hFile = CreateFile(szDir, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
				if (hFile == INVALID_HANDLE_VALUE)
				{
					LOG(TEXT("%S: ERROR %d@%S - failed to open file %s."), __FUNCTION__, __LINE__, __FILE__, szDir);
					return NULL;
				}
			}
			else {
				LOG(TEXT("%S: ERROR %d@%S - File name (%s) or directory name(%s) is too big %s."), __FUNCTION__, __LINE__, __FILE__, szXmlConfig, szDir);
				return NULL;
			}
		}
		else {
			LOG(TEXT("%S: ERROR %d@%S - failed to open file %s."), __FUNCTION__, __LINE__, __FILE__, szXmlConfig);			
			return NULL;
		}
	}

	// Read the size of the file
	DWORD size = 0;
	size = GetFileSize(hFile, 0);
	if (size == -1)
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to read the file size."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		return NULL;
	}

	// Allocate space for the whole file and read it in completely
	char* xmlConfig = new char[size + 1];
	if (!xmlConfig)
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to allocate memory for the config file."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		return NULL;
	}

	DWORD nBytesRead = 0;
	BOOL bRead = ReadFile(hFile, xmlConfig, size, &nBytesRead, NULL);
	if (!bRead || (nBytesRead != size))
	{
		LOG(TEXT("%S: ERROR %d@%s - failed to read the complete file."), __FUNCTION__, __LINE__, __FILE__);
		CloseHandle(hFile);
		delete [] xmlConfig;
		return NULL;
	}
	
	// Insert a string terminator after reading in the file - which is now an XML string
	xmlConfig[size] = 0;

	// Parse the XML string
	HELEMENT hXmlRoot = NULL;
    DWORD rc;

    rc = XMLParse(xmlConfig, &hXmlRoot);
	if (rc || !hXmlRoot)
	{
		LOG(TEXT("%S: XMLParse failed to parse the config file."), __FUNCTION__);
		hr = E_FAIL;
	}
	else {
		// Parse the DMO configurations from the XML string
		pTestConfig = new TestConfig();
		if (pTestConfig)
		{
			hr = ParseTestConfig(hXmlRoot, pTestConfig);
			if (FAILED(hr))
				LOG(TEXT("%S: ParseTestDesc failed."), __FUNCTION__);
		}
		else {
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate TestConfig object when parsing."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
		}
	}

	// Free resources and return
	if (hXmlRoot)
		XMLFree(hXmlRoot);

	if (xmlConfig)
		delete [] xmlConfig;

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);

	if (FAILED(hr))
	{
		if (pTestConfig)
			delete pTestConfig;
		pTestConfig = NULL;
	}

	return pTestConfig;
}

HRESULT ParseStartStopPosition(HELEMENT hXmlChild, PlaybackTestDesc* pPlaybackTestDesc)
{
	HRESULT hr = S_OK;
	const char* szStartStopPosition = XMLGetElementValue(hXmlChild);
	if (!szStartStopPosition)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to extract Start /Stop position data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Now parse the string list into the component elements
	int pos = 0;
	DWORD iPosition = 0;
	char szString[128] = "";
	
	// Parse the start position
	hr = ParseStringListString(szStartStopPosition, POSITION_LIST_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse Start /Stop position data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	hr = ParseDWORD(szString, &iPosition);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse start position."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	pPlaybackTestDesc->start = iPosition;

	// Parse the stop position
	hr = ParseStringListString(szStartStopPosition, POSITION_LIST_DELIMITER, szString, countof(szString), pos);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse Start /Stop position data."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	hr = ParseDWORD(szString, &iPosition);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse stop position."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	pPlaybackTestDesc->stop = iPosition;

	return hr;
	
}


HRESULT ParsePlaybackTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	PlaybackTestDesc* pPlaybackTestDesc = new PlaybackTestDesc();
	if (!pPlaybackTestDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate PlaybackTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}

	*ppTestDesc = pPlaybackTestDesc;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hXmlChild == 0)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to find test id."), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}

	if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pPlaybackTestDesc->testId);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStringList(hXmlChild, &pPlaybackTestDesc->mediaList, MEDIALIST_DELIMITER);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DownloadLocation, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pPlaybackTestDesc->szDownloadLocation, countof(pPlaybackTestDesc->szDownloadLocation));
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_PositionList, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStartStopPosition(hXmlChild, pPlaybackTestDesc);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Verification, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVerification(hXmlChild, &pPlaybackTestDesc->verificationList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	return hr;
}

HRESULT ParseStateChangeSequenceString(const char* szStateChangeString, StateChangeTestDesc* pStateChangeTestDesc)
{
	HRESULT hr = S_OK;

	char szString[128];
	int pos = 0;

	// Parse the initial state
	ParseStringListString(szStateChangeString, STR_STATE_CHANGE_SEQ_DELIMITER, szString, countof(szString), pos);
	if (pos == -1)
		return E_FAIL;
	hr = ToState(szString, &pStateChangeTestDesc->state);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse state %s."), __FUNCTION__, __LINE__, __FILE__, szString);
		return hr;
	}

	// Parse the sequence
	ParseStringListString(szStateChangeString, STR_STATE_CHANGE_SEQ_DELIMITER, szString, countof(szString), pos);
	if (pos == -1)
		return E_FAIL;
	hr = ToStateChangeSequence(szString, &pStateChangeTestDesc->sequence);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse state change sequence %S."), __FUNCTION__, __LINE__, __FILE__, szString);
		return hr;
	}

	// Parse the number of state changes
	ParseStringListString(szStateChangeString, STR_STATE_CHANGE_SEQ_DELIMITER, szString, countof(szString), pos);
	if (pos == -1)
		return E_FAIL;
	hr = ParseDWORD(szString, &pStateChangeTestDesc->nStateChanges);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse nStateChanges."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	// Parse the time between states
	ParseStringListString(szStateChangeString, STR_STATE_CHANGE_SEQ_DELIMITER, szString, countof(szString), pos);
	// This should be the last part of the string
	if (pos != -1)
		return E_FAIL;
	hr = ParseDWORD(szString, &pStateChangeTestDesc->dwTimeBetweenStates);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse dwTimeBetweenStates."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}

	return hr;
}

HRESULT ParseStateChangeTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	StateChangeTestDesc* pStateChangeTestDesc = new StateChangeTestDesc();
	if (!pStateChangeTestDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate StateChangeTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}

	*ppTestDesc = pStateChangeTestDesc;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hXmlChild == 0)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to find test id."), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}

	if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pStateChangeTestDesc->testId);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStringList(hXmlChild, &pStateChangeTestDesc->mediaList, MEDIALIST_DELIMITER);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DownloadLocation, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pStateChangeTestDesc->szDownloadLocation, countof(pStateChangeTestDesc->szDownloadLocation));
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_StateChangeSequence, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStateChangeSequenceString(XMLGetElementValue(hXmlChild), pStateChangeTestDesc);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Verification, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVerification(hXmlChild, &pStateChangeTestDesc->verificationList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	return hr;
}

HRESULT ParsePlaceholderTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	PlaceholderTestDesc* pPlaceholderTestDesc = new PlaceholderTestDesc();
	if (!pPlaceholderTestDesc)
		return E_OUTOFMEMORY;

	hXmlChild = XMLGetFirstChild(hElement);

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
		{
			hr = ParseDWORD(hXmlChild, &pPlaceholderTestDesc->testId);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr))
		*ppTestDesc = pPlaceholderTestDesc;
	else
		delete pPlaceholderTestDesc;

	return hr;
}


HRESULT ParseBuildGraphTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	BuildGraphTestDesc* pBuildGraphTestDesc = new BuildGraphTestDesc();
	if (!pBuildGraphTestDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate TestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}

	*ppTestDesc = pBuildGraphTestDesc;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pBuildGraphTestDesc->testId);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	
	if (SUCCEEDED(hr)  && hXmlChild)
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStringList(hXmlChild, &pBuildGraphTestDesc->mediaList, MEDIALIST_DELIMITER);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DownloadLocation, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pBuildGraphTestDesc->szDownloadLocation, countof(pBuildGraphTestDesc->szDownloadLocation));
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if ( SUCCEEDED(hr) && hXmlChild )
	{
		if ( !strcmp( STR_PreloadFilter, XMLGetElementName(hXmlChild) ) )
		{
			hr = ParseStringList( hXmlChild, &pBuildGraphTestDesc->filterList, ':');
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}


	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Verification, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVerification(hXmlChild, &pBuildGraphTestDesc->verificationList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (FAILED(hr))
	{
		delete pBuildGraphTestDesc;
		*ppTestDesc = NULL;
	}

	return hr;
}

HRESULT ParseWndScale(const char* szWndScale, WndScale* pWndScale)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;
	int listmarker = 0;

	char szX[32];
	char szY[32];

	// Read the X coord
	hr = ParseStringListString(szWndScale, WNDSCALE_DELIMITER, szX, countof(szX), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read X coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndScale->x = atol(szX);
	}

	// Read the Y coord
	hr = ParseStringListString(szWndScale, WNDSCALE_DELIMITER, szY, countof(szY), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read Y coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndScale->y = atol(szY);
	}

	return hr;
}

HRESULT ParseWndPos(const char* szWndPos, WndPos* pWndPos)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;
	int listmarker = 0;

	char szX[32];
	char szY[32];

	// Read the X coord
	hr = ParseStringListString(szWndPos, WNDPOS_DELIMITER, szX, countof(szX), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read X coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndPos->x = atol(szX);
	}

	// Read the Y coord
	hr = ParseStringListString(szWndPos, WNDPOS_DELIMITER, szY, countof(szY), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read Y coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndPos->y = atol(szY);
	}

	return hr;
}

HRESULT ParseWndScaleList(HELEMENT hElement, WndScaleList* pWndScaleList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szWndScaleList = XMLGetElementValue(hElement);
	if (!szWndScaleList)
		return S_OK;
	char szWndScale[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window scaleition
		hr = ParseStringListString(szWndScaleList, WNDSCALE_LIST_DELIMITER, szWndScale, countof(szWndScale), listmarker);
		if (FAILED(hr))
			break;

		WndScale wndScale;
		hr = ParseWndScale(szWndScale, &wndScale);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse window scaleition %S."), __FUNCTION__, __LINE__, __FILE__, szWndScale);
		}
		pWndScaleList->push_back(wndScale);
	}
	
	return hr;
}

HRESULT ParseWndPosList(HELEMENT hElement, WndPosList* pWndPosList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szWndPosList = XMLGetElementValue(hElement);
	if (!szWndPosList)
		return S_OK;
	char szWndPos[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window position
		hr = ParseStringListString(szWndPosList, WNDPOS_LIST_DELIMITER, szWndPos, countof(szWndPos), listmarker);
		if (FAILED(hr))
			break;

		WndPos wndPos;
		hr = ParseWndPos(szWndPos, &wndPos);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse window position %S."), __FUNCTION__, __LINE__, __FILE__, szWndPos);
		}
		pWndPosList->push_back(wndPos);
	}
	
	return hr;
}

HRESULT ParseWndRect(const char* szWndRect, WndRect* pWndRect)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;
	int listmarker = 0;

	char szTop[32];
	char szLeft[32];
	char szBottom[32];
	char szRight[32];

	// Read the left coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szLeft, countof(szLeft), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read left coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->left = atol(szLeft);
	}

	// Read the top coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szTop, countof(szTop), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read top coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->top = atol(szTop);
	}

	// Read the right coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szRight, countof(szRight), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read right coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->right = atol(szRight);
	}


	// Read the bottom coord
	hr = ParseStringListString(szWndRect, WNDRECT_DELIMITER, szBottom, countof(szBottom), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read bottom coord."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pWndRect->bottom = atol(szBottom);
	}

	return hr;
}

HRESULT ParseWndRectList(HELEMENT hElement, WndRectList* pWndRectList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szWndRectList = XMLGetElementValue(hElement);
	if (!szWndRectList)
		return S_OK;
	char szWndRect[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next window position
		hr = ParseStringListString(szWndRectList, WNDRECT_LIST_DELIMITER, szWndRect, countof(szWndRect), listmarker);
		if (FAILED(hr))
			break;

		WndRect wndRect;
		hr = ParseWndRect(szWndRect, &wndRect);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse window position %S."), __FUNCTION__, __LINE__, __FILE__, szWndRect);
		}
		pWndRectList->push_back(wndRect);
	}
	
	return hr;
}

HRESULT ParsePosRate(const char* szPosRate, PosRate* pPosRate)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;
	int listmarker = 0;

	char szStart[32];
	char szStop[32];
	char szRate[32];

	if (!szPosRate)
		return E_POINTER;

	// Read the start position 
	hr = ParseStringListString(szPosRate, POSRATE_DELIMITER, szStart, countof(szStart), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read start position."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pPosRate->start = (LONGLONG)_atoi64(szStart);
	}

	// Read the stop position
	hr = ParseStringListString(szPosRate, POSRATE_DELIMITER, szStop, countof(szStop), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to read stop position."), __FUNCTION__, __LINE__, __FILE__);
		return hr;
	}
	else {
		pPosRate->stop = (LONGLONG)_atoi64(szStop);
	}

	// Read the rate
	hr = ParseStringListString(szPosRate, POSRATE_DELIMITER, szRate, countof(szRate), listmarker);
	if (FAILED(hr))
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to parse rate."), __FUNCTION__, __LINE__, __FILE__);
	}
	else {
		pPosRate->rate = atof(szRate);
	}

	return hr;
}


HRESULT ParsePosRateList(HELEMENT hElement, PosRateList* pPosRateList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szPosRateList = XMLGetElementValue(hElement);
	if (!szPosRateList)
		return S_OK;
	char szPosRate[32];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next position and rate triple
		hr = ParseStringListString(szPosRateList, POSRATE_LIST_DELIMITER, szPosRate, countof(szPosRate), listmarker);
		if (FAILED(hr))
			break;

		PosRate posrate;
		hr = ParsePosRate(szPosRate, &posrate);
		if (FAILED(hr))
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to parse positions and rate %S."), __FUNCTION__, __LINE__, __FILE__, szPosRate);
		}
		pPosRateList->push_back(posrate);
	}
	
	return hr;
}

HRESULT ParsePositionList(HELEMENT hElement, PositionList* pPositionList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szPositionList = XMLGetElementValue(hElement);
	if (!szPositionList)
		return S_OK;
	char szPosition[8];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next position
		hr = ParseStringListString(szPositionList, POSITION_LIST_DELIMITER, szPosition, countof(szPosition), listmarker);
		if (FAILED(hr))
			break;

		LONGLONG pos = (LONGLONG)_atoi64(szPosition);
		pPositionList->push_back(pos);
	}
	
	return hr;
}

HRESULT ParseRateList(HELEMENT hElement, RateList* pRateList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	const char* szRateList = XMLGetElementValue(hElement);
	if (!szRateList)
		return S_OK;
	char szRate[8];

	int listmarker = 0;
	while(listmarker != -1)
	{
		// Read the next rate
		hr = ParseStringListString( szRateList, POSITION_LIST_DELIMITER, szRate, countof(szRate), listmarker );
		if (FAILED(hr))
			break;

		double rate = atof(szRate);
		pRateList->push_back(rate);
	}
	
	return hr;
}

HRESULT ParseSetPosRateTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	SetPosRateTestDesc* pSetPosRateTestDesc = new SetPosRateTestDesc();
	if (!pSetPosRateTestDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate SetPosRateTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}


	*ppTestDesc = pSetPosRateTestDesc;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hXmlChild == 0)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to find test id."), __FUNCTION__, __LINE__, __FILE__);
		return E_FAIL;
	}

	if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pSetPosRateTestDesc->testId);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStringList(hXmlChild, &pSetPosRateTestDesc->mediaList, MEDIALIST_DELIMITER);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DownloadLocation, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pSetPosRateTestDesc->szDownloadLocation, countof(pSetPosRateTestDesc->szDownloadLocation));
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_PositionList, XMLGetElementName(hXmlChild)))
		{
			hr = ParsePositionList(hXmlChild, &pSetPosRateTestDesc->positionList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_PosRateList, XMLGetElementName(hXmlChild)))
		{
			hr = ParsePosRateList(hXmlChild, &pSetPosRateTestDesc->posrateList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Verification, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVerification(hXmlChild, &pSetPosRateTestDesc->verificationList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}


	return hr;
}

HRESULT ParseRendererMode(const char* szRendererMode, VideoRendererMode *pVRMode)
{
	HRESULT hr = S_OK;

	char szMode[32] = "";
	
	*pVRMode = 0;
	
	int pos = 0;
	while(pos != -1)
	{
		hr = ParseStringListString(szRendererMode, RENDERER_MODE_STR_DELIMITER, szMode, countof(szMode), pos);
		if (FAILED(hr))
			return hr;
        if (!strcmp(szMode, STR_GDIMode))
			*pVRMode |= VIDEO_RENDERER_MODE_GDI;
		else if (!strcmp(szMode, STR_DDrawMode))
			*pVRMode |= VIDEO_RENDERER_MODE_DDRAW;
		else {
			return E_FAIL;
		}
	}

	return hr;
}

HRESULT ParseSurfaceType(const char* szSurfaceType, DWORD *pSurfaceType)
{
	HRESULT hr = S_OK;

	char szType[32] = "";
	
	*pSurfaceType = 0;

	int pos = 0;
	while(pos != -1)
	{
		hr = ParseStringListString(szSurfaceType, SURFACE_MODE_STR_DELIMITER, szType, countof(szType), pos);
		if (FAILED(hr))
			return hr;
        if (!strcmp(szType, STR_PS))
			*pSurfaceType |= AMDDS_PS;
		else if (!strcmp(szType, STR_RGBOVR))
			*pSurfaceType |= AMDDS_RGBOVR;
		else if (!strcmp(szType, STR_YUVOVR))
			*pSurfaceType |= AMDDS_YUVOVR;
		else if (!strcmp(szType, STR_RGBOFF))
			*pSurfaceType |= AMDDS_RGBOFF;
		else if (!strcmp(szType, STR_YUVOFF))
			*pSurfaceType |= AMDDS_YUVOFF;
		else if (!strcmp(szType, STR_RGBFLP))
			*pSurfaceType |= AMDDS_RGBFLP;
		else if (!strcmp(szType, STR_YUVFLP))
			*pSurfaceType |= AMDDS_YUVFLP;
		else if (!strcmp(szType, STR_ALL))
			*pSurfaceType |= AMDDS_ALL;
		else if (!strcmp(szType, STR_YUV))
			*pSurfaceType |= AMDDS_YUV;
		else if (!strcmp(szType, STR_RGB))
			*pSurfaceType |= AMDDS_RGB;
		else {
			LOG(TEXT("%S: ERROR %d@%S - unknown surface type %S."), __FUNCTION__, __LINE__, __FILE__, szType);
			return E_FAIL;
		}
	}
	
	return hr;
}

HRESULT ParseRendererSurfaceType(const char* szRendererSurfaceType, VideoRendererMode *pVRMode, DWORD* pSurfaceType)
{
	HRESULT hr = S_OK;

	int pos = 0;
	char szRendererMode[64] = "";
	char szSurfaceType[512] = "";
	hr = ParseStringListString(szRendererSurfaceType, RENDERER_SURFACE_MODE_STR_DELIMITER, szRendererMode, countof(szRendererMode), pos);
	if (FAILED(hr))
		return hr;
	
	if (pos != -1)
	{
		hr = ParseStringListString(szRendererSurfaceType, RENDERER_SURFACE_MODE_STR_DELIMITER, szSurfaceType, countof(szSurfaceType), pos);
		if (FAILED(hr))
			return hr;
	}

	if (strcmp(szRendererMode, ""))
		hr = ParseRendererMode(szRendererMode, pVRMode);

	if (FAILED(hr))
		return hr;

	if (strcmp(szSurfaceType, ""))
		hr = ParseSurfaceType(szSurfaceType, pSurfaceType);

	return hr;
}

HRESULT ParseImageSize(HELEMENT hElement, VRImageSize* pImageSize)
{
	HRESULT hr = S_OK;

	const char* szImageSize = XMLGetElementValue(hElement);
	if (!szImageSize)
		return E_FAIL;

	if (!strcmp(szImageSize, STR_MinImageSize))
		*pImageSize = MIN_IMAGE_SIZE;
	else if (!strcmp(szImageSize, STR_MaxImageSize))
		*pImageSize = MAX_IMAGE_SIZE;
	else if (!strcmp(szImageSize, STR_FullScreen))
		*pImageSize = FULL_SCREEN;
	else {
		LOG(TEXT("%S: ERROR %d@%S - unknown image size %S."), __FUNCTION__, __LINE__, __FILE__, szImageSize);
		return E_FAIL;
	}
	
	return hr;
}

HRESULT ParseVRFlags(HELEMENT hElement, DWORD* pVRFlags)
{
	HRESULT hr = S_OK;

	*pVRFlags = 0;

	// Get the flag strings
	const char* szVRFlags = XMLGetElementValue(hElement);
	if (!szVRFlags)
		return S_OK;

	// Extract the individual flags
	char szVRFlag[256] = "";	
	int pos = 0;
	while(pos != -1)
	{
		hr = ParseStringListString(szVRFlags, VR_FLAGS_DELIMITER, szVRFlag, countof(szVRFlag), pos);
		if (FAILED(hr))
			return hr;
        if (!strcmp(szVRFlag, STR_AllowPrimaryFlipping))
			*pVRFlags |= ALLOW_PRIMARY_FLIPPING;
		else if (!strcmp(szVRFlag, STR_UseScanLine))
			*pVRFlags |= USE_SCAN_LINE;
		else if (!strcmp(szVRFlag, STR_UseOverlayStretch))
			*pVRFlags |= USE_OVERLAY_STRETCH;
		else {
			LOG(TEXT("%S: ERROR %d@%S - unknown VR flag %S."), __FUNCTION__, __LINE__, __FILE__, szVRFlag);
			return E_FAIL;
		}
	}
	
	return hr;
}

HRESULT ParseVRTestDesc(HELEMENT hElement, TestDesc** ppTestDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	VRTestDesc* pVRTestDesc = new VRTestDesc();
	if (!pVRTestDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate VRTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}

	*ppTestDesc = pVRTestDesc;

	hXmlChild = XMLGetFirstChild(hElement);
	if (!hXmlChild)
		return S_OK;

	if (!strcmp(STR_TestId, XMLGetElementName(hXmlChild)))
	{
		hr = ParseDWORD(hXmlChild, &pVRTestDesc->testId);
		if (SUCCEEDED(hr))
			hXmlChild = XMLGetNextSibling(hXmlChild);
	}
	
	if (SUCCEEDED(hr)  && hXmlChild)
	{
		if (!strcmp(STR_Media, XMLGetElementName(hXmlChild)))
		{
			hr = ParseStringList(hXmlChild, &pVRTestDesc->mediaList, MEDIALIST_DELIMITER);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DownloadLocation, XMLGetElementName(hXmlChild)))
		{
			hr = ParseString(hXmlChild, pVRTestDesc->szDownloadLocation, countof(pVRTestDesc->szDownloadLocation));
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_State, XMLGetElementName(hXmlChild)))
		{
			hr = ParseState(hXmlChild, &pVRTestDesc->state);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
				pVRTestDesc->bStatePresent = true;
			}
		}
	}

	// Parse the renderer mode
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_RendererMode, XMLGetElementName(hXmlChild)))
		{
			const char* szRendererSurfaceType = XMLGetElementValue(hXmlChild);
			hr = ParseRendererSurfaceType(szRendererSurfaceType, (VideoRendererMode*)&pVRTestDesc->vrMode, (DWORD*)&pVRTestDesc->surfaceType);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	// Parse the renderer mode 2
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_RendererMode2, XMLGetElementName(hXmlChild)))
		{
			const char* szRendererSurfaceType = XMLGetElementValue(hXmlChild);
			hr = ParseRendererSurfaceType(szRendererSurfaceType, (VideoRendererMode*)&pVRTestDesc->vrMode2, (DWORD*)&pVRTestDesc->surfaceType2);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	// Parse the max back buffers
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_MaxBackBuffers, XMLGetElementName(hXmlChild)))
		{
			hr = ParseDWORD(hXmlChild, &pVRTestDesc->dwMaxBackBuffers);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the VR flags
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_VRFlags, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVRFlags(hXmlChild, &pVRTestDesc->dwVRFlags);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse image size recommendation
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_VRImageSize, XMLGetElementName(hXmlChild)))
		{
			hr = ParseImageSize(hXmlChild, &pVRTestDesc->imageSize);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the source rect
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_SrcRect, XMLGetElementName(hXmlChild)))
		{
			hr = ParseRect(hXmlChild, &pVRTestDesc->srcRect);
			if (SUCCEEDED(hr))
			{
				pVRTestDesc->bSrcRect = true;
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the destination rect
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_DstRect, XMLGetElementName(hXmlChild)))
		{
			hr = ParseRect(hXmlChild, &pVRTestDesc->dstRect);
			if (SUCCEEDED(hr))
			{
				pVRTestDesc->bDstRect = true;
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the window position list
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_WndPosList, XMLGetElementName(hXmlChild)))
		{
			hr = ParseWndPosList(hXmlChild, &pVRTestDesc->wndPosList);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the window scale list
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_WndScaleList, XMLGetElementName(hXmlChild)))
		{
			// WndScaleList has the same format as WndPosList so use the same parse method
			hr = ParseWndScaleList(hXmlChild, &pVRTestDesc->wndScaleList);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the window rectangle list
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_WndRectList, XMLGetElementName(hXmlChild)))
		{
			// WndScaleList has the same format as WndPosList so use the same parse method
			hr = ParseWndRectList(hXmlChild, &pVRTestDesc->wndRectList);
			if (SUCCEEDED(hr))
			{
				hXmlChild = XMLGetNextSibling(hXmlChild);
			}
		}
	}

	// Parse the op list
	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_OpList, XMLGetElementName(hXmlChild)))
		{
			hr = ParseOpList(hXmlChild, &pVRTestDesc->opList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (SUCCEEDED(hr) && hXmlChild)
	{
		if (!strcmp(STR_Verification, XMLGetElementName(hXmlChild)))
		{
			hr = ParseVerification(hXmlChild, &pVRTestDesc->verificationList);
			if (SUCCEEDED(hr))
				hXmlChild = XMLGetNextSibling(hXmlChild);
		}
	}

	if (FAILED(hr))
	{
		delete pVRTestDesc;
		*ppTestDesc = NULL;
	}

	return hr;
}


ParseTestFunction GetParseFunction(TCHAR* szTestName)
{
	// Run through table and determine funciton or NULL
	for(int i = 0; i < g_nLocalTestFunctions; i++)
	{
		if (!_tcscmp(g_lpLocalFunctionTable[i].szTestName, szTestName)) {
			return g_lpLocalFunctionTable[i].parsefn;
			break;
		}
	}

	return NULL;
}

// static function definitions of TestDescParser
TestConfig* 
TestDescParser::ParseConfigFile(TCHAR* szConfigFile)
{
	return ::ParseConfigFile(szConfigFile);
}

MediaList* 
TestDescParser::GetMediaList()
{
	return NULL;
}

TestList* 
TestDescParser::GetTestList()
{
	return NULL;
}

FUNCTION_TABLE_ENTRY* 
TestDescParser::GetFunctionTable()
{
	return NULL;
}


#if 0
HRESULT ParseConnectionDesc(HELEMENT hElement, ConnectionDesc* pConnectionDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hElement == 0)
		return E_FAIL;

	hr = ParseFilterDesc(hXmlChild, &pConnectionDesc->fromFilter);
	if ( FAILED(hr) )
		return hr;

	hXmlChild = XMLGetNextSibling(hXmlChild);
	if (hElement == 0)
		return E_FAIL;


	hr = ParseFilterDesc(hXmlChild, &pConnectionDesc->toFilter);
	if ( FAILED(hr) )
		return hr;

	return hr;
}

HRESULT ParseGraphDesc(HELEMENT hElement, GraphDesc* pGraphDesc)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hElement == 0)
		return E_FAIL;


	hr = ParseConnectionDescList(hXmlChild, &pGraphDesc->connectionDescList);
	if ( FAILED(hr) )
		return hr;

	hXmlChild = XMLGetNextSibling(hXmlChild);
	if (hElement == 0)
		return E_FAIL;


	hr = ParseFilterDescList(hXmlChild, &pGraphDesc->filterDescList);
	if ( FAILED(hr) )
		return hr;

	return hr;
}

HRESULT ParseConnectionDescList(HELEMENT hElement, ConnectionDescList* pConnectionDescList)
{
	HRESULT hr = S_OK;
	HELEMENT hXmlChild = 0;

	hXmlChild = XMLGetFirstChild(hElement);
	if (hXmlChild == 0)
		return S_OK;

	ConnectionDesc* pConnectionDesc = new ConnectionDesc();
	if (!pConnectionDesc)
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to allocate ConnectionDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
		hr = E_OUTOFMEMORY;
		return hr;
	}


	hr = ParseConnectionDesc(hXmlChild, pConnectionDesc);
	if ( FAILED(hr) )
	{
		delete pConnectionDesc;
		return hr;
	}

	pConnectionDescList->push_back(pConnectionDesc);

	while ( true )
	{
		hXmlChild = XMLGetNextSibling(hXmlChild);
		if (hXmlChild == 0)
			return S_OK;

		pConnectionDesc = new ConnectionDesc();
		if (!pConnectionDesc)
		{
			LOG(TEXT("%S: ERROR %d@%S - failed to allocate SetPosRateTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
			hr = E_OUTOFMEMORY;
			break;
		}


		hr = ParseConnectionDesc(hXmlChild, pConnectionDesc);
		if ( FAILED(hr) )
		{
			delete pConnectionDesc;
			return hr;
		}

		pConnectionDescList->push_back(pConnectionDesc);
	}

	return hr;
}
#endif

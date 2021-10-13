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
////////////////////////////////////////////////////////////////////////////////

#ifndef _TESTDESC_PARSER_H
#define _TESTDESC_PARSER_H

#include "DxXML.h"
#include "TestDesc.h"

// Constant strings common to every test description
const char* const STR_True = "true";
const char* const STR_False = "false";
const char* const STR_TestGroup = "TestGroup";
const char* const STR_TestId = "TestId";
const char* const STR_TestList = "TestList";
const char* const STR_Media = "Media";
const char* const STR_DownloadLocation = "DownloadTo";
const char* const STR_GenerateId = "GenerateId";
const char* const STR_Verification = "Verify";
const char* const STR_State = "State";
const char* const STR_StateList = "StateList";

const char* const STR_StateChangeSequence = "StateChangeSequence";
const char* const STR_Stopped = "Stopped";
const char* const STR_Paused = "Paused";
const char* const STR_Running = "Running";
const char* const STR_StopPause = "StopPause";
const char* const STR_StopPlay = "StopPlay";
const char* const STR_PauseStop = "PauseStop";
const char* const STR_PausePlay = "PausePlay";
const char* const STR_PlayPause = "PlayPause";
const char* const STR_PlayStop = "PlayStop";
const char* const STR_PausePlayStop = "PausePlayStop";
const char* const STR_RandomSequence = "RandomSequence";

const char* const STR_TIME_FORMAT = "TimeFormat";
const char* const STR_TIME_MEDIA_TIME = "MediaTime";
const char* const STR_TIME_FIELD = "Field";
const char* const STR_TIME_SAMPLE = "Sample";
const char* const STR_TIME_BYTE = "Byte";
const char* const STR_TIME_FRAME = "Frame";

const char MEDIALIST_DELIMITER = ',';
const char TIMEFORMAT_LIST_DELIMITER = ',';
const char STR_STATE_CHANGE_SEQ_DELIMITER = ',';
const char STR_STATE_DELIMITER = ',';


extern HRESULT ToState(const char* szState, FILTER_STATE* pState);
extern HRESULT ToStateChangeSequence(const char* szSequence, StateChangeSequence* pSequence);
extern HRESULT ParseStateChangeSequenceString( const char* szStateChangeString, StateChangeData* pStateChange );
extern HRESULT ParseGraphDesc( CXMLElement *hGraphDesc, GraphDesc* pGraphDesc );
extern HRESULT ParseTimeFormat( CXMLElement *hTimeFormat, TimeFormatList *pTimeFormatList );
extern HRESULT ParseConnectionDesc( CXMLElement *hElement, ConnectionDesc* pConnectionDesc );
extern HRESULT ParseRect( CXMLElement *hRect, RECT* pRect );
extern HRESULT ParseRendererMode( const char* szElement, VideoRendererMode* pVRMode );
extern HRESULT ParseSurfaceType( const char* szSurfaceType, DWORD *pSurfaceType );
extern HRESULT ParseStartStopPosition( CXMLElement *hXmlChild, LONGLONG &start, LONGLONG &stop );
extern HRESULT ParseState( CXMLElement *hState, FILTER_STATE* pState );
extern HRESULT ParseStateList( CXMLElement *hStateList, StateList* pStateList );
extern HRESULT ParseFilterId( CXMLElement *hElement, FilterId* pFilterId );
extern HRESULT ParseFilterDesc( CXMLElement *hElement, FilterDesc* pFilterDesc );
extern HRESULT ParseFilterDescList( CXMLElement *hElement, FilterDescList* pFilterDescList );
extern HRESULT ParseCommonTestInfo( CXMLElement *hElement, TestDesc* pTestDesc ); 

// Verification parser
extern HRESULT ParseVerifyDWORD( CXMLElement *hVerification, 
								 VerificationType type, 
								 VerificationList* pVerificationList );
extern HRESULT ParseVerifyVideoRECT( CXMLElement *hVerification, 
									 VerificationType type, 
									 VerificationList* pVerificationList );
extern HRESULT ParseVerifyRECT( CXMLElement *hVerification, 
							    VerificationType type, 
								VerificationList* pVerificationList );
extern HRESULT ParseVerifyRendererMode( CXMLElement *hVerification, 
									    VerificationType type, 
										VerificationList* pVerificationList );
extern HRESULT ParseVerifyRendererSurfaceType( CXMLElement *hVerification, 
											   VerificationType type, 
											   VerificationList* pVerificationList );
extern HRESULT ParseVerifySampleDelivered( CXMLElement *hVerification, 
										   VerificationType type, 
										   VerificationList* pVerificationList );
extern HRESULT ParseVerifyGraphDesc( CXMLElement *hVerification, 
									 VerificationType type, 
									 VerificationList* pVerificationList );
extern HRESULT ParseVerifyPlaybackDuration( CXMLElement *hVerification, 
										    VerificationType type, 
											VerificationList* pVerificationList );
extern HRESULT ParseVerification( CXMLElement *hVerification, 
								  VerificationList* pVerificationList );


typedef HRESULT (*ParseTestFunction)( CXMLElement *hElement, TestDesc** pTestDesc );
extern ParseTestFunction GetParseFunction( TCHAR* szTestName );

extern HRESULT ParseTestConfig( CXMLElement *hTestConfig, TestConfig* pTestConfig );
extern HRESULT ParseTestDesc( CXMLElement *hTestDesc, TestDesc** pTestDesc );
extern HRESULT ParseTestDescList( CXMLElement *hTestDescList, TestDescList* pTestDescList, TestGroupList* pTestGroupList );
extern HRESULT ParseVerification( CXMLElement *hVerification, 
								  VerificationList* pVerificationList );

extern TestConfig* ParseConfigFile( TCHAR* szConfigFile );

struct GraphFunctionTableEntry {
	TCHAR szTestDesc[TEST_DESC_LENGTH];
	TCHAR szTestName[TEST_NAME_LENGTH];
	TESTPROC lpTestProc; 
	ParseTestFunction parsefn;
};

#endif

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
#ifndef __DEBUG__H
#define __DEBUG__H

#define ZONEID_ERROR			0
	// for all of the out-of-memory and other catastrophic-to-a/v issues
	// (e.g., sample consumer stopping playback because 1x playback is
	// impossible and pause buffer is exhausted). Normally turned on.
	// Also is DirectShow ERROR zone

#define ZONEID_WARNING			1
	// for very suspicious things (almost certain bugs) and for innocuous things
	// that test keeps finding alarming (e.g., Test sees chatter about all C++ exceptions
	// thrown, including ones that are perfectly normal and safe).  Normally turned on.
	// Also is DirectShow WARNING zone

#define ZONEID_USER_REQUEST		2
	// to record requests coming in from middleware, e.g., to set the rate, start a
	// recording, change positions, turn encryption on/off, ... 

#define ZONEID_COPY_PROTECTION	3
	// for encryption /decryption / copy protection from both source & sink
	// Also is DirectShow LOG_TRACE zone -- and since most chatter comes from
	// the EncDec object, the copy protection zone is chosen for overlap

#define ZONEID_EVENT_DETECTED	4
	// to record detection of a/v events of unusual significance, e.g.,
	// detecting tunes, detecting changes in copy protection,
	// when the encoder/receiver detects a reason to change its translation factor
	// for PTS -> DShow times
	// In full debug builds, you will also see initialization chatter from
	// DirectShow.

#define ZONEID_FILE_IO			5
	// for the detailed chatter about seeks, reads, and writes
	// Doubles as DirectShow's zone for enter/exit chatter.

#define ZONEID_SOURCE_DISPATCH	6
	// for detailed chatter about sample dispatch in the DVR source filter
	// Doubles as the DirectShow zone for chattering about timing, which
	// is largely related to sample dispatch so overlap is OK.

#define ZONEID_DSHOW_REFS_AND_MEMORY	7
	// Doubles are DirectShow's MEMORY and ref-count zone. Way too noisy
	// to be overloaded

#define ZONEID_FILE_OTHER		8
	//  to record high level file events - file creation, deletion, open, closes
	// Doubles as DirectShow's LOCKING zone.

#define ZONEID_ENGINE_OTHER		9
	// for all other chatter from the DVR sink or source filter.
	// Also the DirectShow zone for chattering about MediaTypes -- that seems
	// like useful overlap

#define ZONEID_SOURCE_STATE		10
	// for DVR source filter chatter about major internal mode changes
	// (e.g., reader being told to stream vs not, reader seeks, reader
	// every-frame-forward/every-Nth-frame-forward/...  changes;  similar
	// major changes in the decoder driver and/or sample consumer)
	// All flush chatter goes here -- even if from the plumbing where
	// it might actually be sink chatter.

#define ZONEID_SINK_DISPATCH	11
	// for detailed chatter about sample dispatch in the DVR sink filter.
	// Doubles as DirectShow's REFCOUNT zone

#define ZONEID_CLOCK		   	12
	// for details of clock slaving
	// Doubles as DirectShow's PERF zone

#define ZONEID_PAUSE_BUFFER		13
	// to record all events related to changing or reporting what goes into the pause buffer

#define ZONEID_SAMPLE_CONSUMER_AND_DECODER_DRIVER	14
	// for details into sample consumer state and state changes
	// for details of decoder driver state and state changes

#define ZONEID_TEST				15
	// reserved for whatever our short-term need is for special chatter
	// (turned on when needed to track down an urgent bug)

#define ZONEMASK_ERROR				(1 << ZONEID_ERROR)
#define ZONEMASK_WARNING			(1 << ZONEID_WARNING)
#define ZONEMASK_USER_REQUEST		(1 << ZONEID_USER_REQUEST)
#define ZONEMASK_EVENT_DETECTED		(1 << ZONEID_EVENT_DETECTED)
#define ZONEMASK_PAUSE_BUFFER		(1 << ZONEID_PAUSE_BUFFER)
#define ZONEMASK_FILE_IO			(1 << ZONEID_FILE_IO)
#define ZONEMASK_FILE_OTHER			(1 << ZONEID_FILE_OTHER)
#define ZONEMASK_SINK_DISPATCH		(1 << ZONEID_SINK_DISPATCH)
#define ZONEMASK_SOURCE_DISPATCH	(1 << ZONEID_SOURCE_DISPATCH)
#define ZONEMASK_ENGINE_OTHER		(1 << ZONEID_ENGINE_OTHER)
#define ZONEMASK_SOURCE_STATE		(1 << ZONEID_SOURCE_STATE)
#define ZONEMASK_SAMPLE_CONSUMER	(1 << ZONEID_SAMPLE_CONSUMER_AND_DECODER_DRIVER)
#define ZONEMASK_CLOCK				(1 << ZONEID_CLOCK)
#define ZONEMASK_COPY_PROTECTION	(1 << ZONEID_COPY_PROTECTION)
#define ZONEMASK_DECODER_DRIVER		(1 << ZONEID_SAMPLE_CONSUMER_AND_DECODER_DRIVER)
#define ZONEMASK_TEST				(1 << ZONEID_TEST)

#define DVRENGINE_NAMES            \
		TEXT("Errors"),				\
		TEXT("Warnings"),			\
		TEXT("User Request"),		\
		TEXT("Copy Protection"),	\
		TEXT("Event Detection"),	\
		TEXT("File IO"),		    \
		TEXT("Playback Dispatch"), 	\
		TEXT("Ref Count"),			\
		TEXT("Misc File"),			\
		TEXT("Miscellaneous"),		\
		TEXT("Playback State"),		\
		TEXT("Capture Dispatch"), 	\
		TEXT("Clock"),				\
		TEXT("Pause Buffer"),		\
		TEXT("Decoder n Consumer"),	\
		TEXT("Test")

// #ifdef DEBUG
#ifndef SHIP_BUILD

#ifndef ZONE_ERROR
#define ZONE_ERROR				DEBUGZONE(ZONEID_ERROR)
#endif // !ZONE_ERROR

#ifndef ZONE_WARNING
#define ZONE_WARNING			DEBUGZONE(ZONEID_WARNING)
#endif // !ZONE_WARNING

#define ZONE_USER_REQUEST		DEBUGZONE(ZONEID_USER_REQUEST)
#define ZONE_EVENT_DETECTED		DEBUGZONE(ZONEID_EVENT_DETECTED)
#define ZONE_PAUSE_BUFFER		DEBUGZONE(ZONEID_PAUSE_BUFFER)
#define ZONE_FILE_IO			DEBUGZONE(ZONEID_FILE_IO)
#define ZONE_FILE_OTHER			DEBUGZONE(ZONEID_FILE_OTHER)
#define ZONE_SINK_DISPATCH		DEBUGZONE(ZONEID_SINK_DISPATCH)
#define ZONE_SOURCE_DISPATCH	DEBUGZONE(ZONEID_SOURCE_DISPATCH)
#define ZONE_ENGINE_OTHER		DEBUGZONE(ZONEID_ENGINE_OTHER)
#define ZONE_SOURCE_STATE		DEBUGZONE(ZONEID_SOURCE_STATE)
#define ZONE_SAMPLE_CONSUMER	DEBUGZONE(ZONEID_SAMPLE_CONSUMER_AND_DECODER_DRIVER)
#define ZONE_CLOCK				DEBUGZONE(ZONEID_CLOCK)
#define ZONE_COPY_PROTECTION	DEBUGZONE(ZONEID_COPY_PROTECTION)
#define ZONE_DECODER_DRIVER		DEBUGZONE(ZONEID_SAMPLE_CONSUMER_AND_DECODER_DRIVER)
#define ZONE_TEST				DEBUGZONE(ZONEID_TEST)

//
//  For now, we need retail logging support for this module; delete
//  this section once this is no longer required  -- RB
//

#undef DbgLog
#define DbgLog(_x_) RetailMsgConvert _x_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
	
void RetailMsgConvert(BOOL fCond, DWORD /*Level*/, const TCHAR *pFormat, ...);

#ifdef __cplusplus
};
#endif // __cplusplus

//  End of retail logging configuration

#else // DEBUG

#ifndef ZONE_ERROR
#define ZONE_ERROR				0
#endif // !ZONE_ERROR

#ifndef ZONE_WARNING
#define ZONE_WARNING			0
#endif // !ZONE_WARNING

#define ZONE_USER_REQUEST		0
#define ZONE_EVENT_DETECTED		0
#define ZONE_PAUSE_BUFFER		0
#define ZONE_FILE_IO			0
#define ZONE_FILE_OTHER			0
#define ZONE_SINK_DISPATCH		0
#define ZONE_SOURCE_DISPATCH	0
#define ZONE_ENGINE_OTHER			0
#define ZONE_SOURCE_STATE		0
#define ZONE_SAMPLE_CONSUMER	0
#define ZONE_CLOCK				0
#define ZONE_COPY_PROTECTION	0
#define ZONE_DECODER_DRIVER		0
#define ZONE_TEST				0


#endif // DEBUG

#ifndef LOG_ERROR
#define LOG_ERROR			ZONE_ERROR
#endif // !LOG_ERROR

#ifndef LOG_WARNING
#define LOG_WARNING			ZONE_WARNING			
#endif // !LOG_WARNING

#define LOG_USER_REQUEST	ZONE_USER_REQUEST			
#define LOG_EVENT_DETECTED	ZONE_EVENT_DETECTED			
#define LOG_PAUSE_BUFFER	ZONE_PAUSE_BUFFER			
#define LOG_FILE_OTHER		ZONE_FILE_OTHER			
#define LOG_FILE_IO			ZONE_FILE_IO			
#define LOG_SINK_DISPATCH	ZONE_SINK_DISPATCH			
#define LOG_SOURCE_DISPATCH	ZONE_SOURCE_DISPATCH			
#define LOG_ENGINE_OTHER	ZONE_ENGINE_OTHER			
#define LOG_SOURCE_STATE	ZONE_SOURCE_STATE			
#define LOG_SAMPLE_CONSUMER	ZONE_SAMPLE_CONSUMER			
#define LOG_CLOCK			ZONE_CLOCK			
#define LOG_COPY_PROTECTION	ZONE_COPY_PROTECTION			
#define LOG_DECODER_DRIVER	ZONE_DECODER_DRIVER			
#define LOG_TEST			ZONE_TEST			

#endif



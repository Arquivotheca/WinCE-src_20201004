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
/////////////////////////////////////////////////////////////////////////////////////////////////
//  logwrap.h
// 
//  A wrapper for logging messages through NetLog.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _LOG_WRAP_H_
#define _LOG_WRAP_H_

#include "netlog.h"


#ifdef __cplusplus
extern "C" {
#endif

//
//  Initialize the logging wrapper functions.  
//    test_name        -  Unique name of the test.  Used to determine the output file names.
//                        The final file name will be "%test_name%_%processor%_%build#%_%config%_%pass/fail%.log"
//    log_level        -  The level of logging desired, see netlog.h for details.  LOG_PASS is typical for retail builds.
//    output_flags     -  The desired output streams, see netlog.h for details.
//    use_multi_thread -  Use a different output log file for each thread.
//
BOOL NetLogInitWrapper(LPCTSTR test_name, DWORD log_level, DWORD output_flags, BOOL use_multi_thread);

//
//  Clean up after logging.  
//  Deletes all necessary objects.
//  All logging functions will no longer work.
//
BOOL NetLogCleanupWrapper();

//
//  Return TRUE if the system is initialized and FALSE if it is not.
//
BOOL NetLogSetInitialize(HANDLE h);

//
//  Return a handle if the log wrapper is initialized, FALSE if it is not.
//
HANDLE NetLogIsInitialized();

//
//  Gets the verbosity level required to log a message.
//
DWORD NetLogGetLogLevel(void);

//
//  Sets the verbosity level required to log a message.
//
BOOL NetLogSetLogLevel(DWORD NewLevel);

//
//  Gets the current WATT-output flag. If this flag is on the system will output
//  the 6 @ signs and error count to tell the automation server the test is done.
//
BOOL NetLogGetWATTOutput(void);

//
//  Sets the system into a state where it does or doesn't output the 6 @ signs to tell the automation server that 
//  the test is done.  This is used when we want to batch tests together.
//
void NetLogSetWATTOutput(BOOL wattOutput);


/////////////////////////////////////////////////////////////////////////////////////////////////
//  Output Routines
/////////////////////////////////////////////////////////////////////////////////////////////////

// 
//  Logs a message at the specified verbosity level and increments the
//  corresponding message count.
//  Concatenates the user message to the specified header.
//
void NetLogV(DWORD Verbosity, LPCTSTR pHeader, LPCTSTR pFormat, va_list pArgs);

//
//  Output an error message and increment the error count. At the NetLog level of LOG_FAIL. 
//  Concatenate the user message to the string "Error: ".
//
void NetLogError(LPCTSTR format, ...);

//
//  Output a warning message and increment the warning count.  At the NetLog level of LOG_WARNING.
//  Concatenate the user message to the string "Warning: ".
//
void NetLogWarning(LPCTSTR format, ...);

//
//  Output a message.  At the NetLog level of LOG_PASS.
//  Concatenate the user message to the string "Message: ".
//
void NetLogMessage(LPCTSTR format, ...);

//
//  Output a debug message.  Set at the NetLog level of LOG_DETAIL, which is higher than default and is not normally outputed.
//  Concatenate the user message to the string "Debug: ".
//
void NetLogDebug(LPCTSTR format, ...);

//
//  Output a verbose message.  Set at the NetLog level of LOG_COMMENT, which is higher than default and is not normally outputted.
//  Concatenate the user message to the string "Verbose: ".
//
void NetLogVerbose(LPCTSTR format, ...);

/////////////////////////////////////////////////////////////////////////////////////////////////
//  Statistic Routines
/////////////////////////////////////////////////////////////////////////////////////////////////

//
//  Return the total number of errors currently counted by the logging tool.
// 
DWORD NetLogGetErrorCount(void);

//
//  Return the total number of warnings currently counted by the logging tool.
// 
DWORD NetLogGetWarningCount(void);

// 
//  Retrieves the message counts into the specified array.
//
BOOL NetLogGetMessageCounts(__out_ecount(NumberCounts) LONG *pMsgCounts, DWORD  NumberCounts);

//
//  Reset the message counts.
// 
BOOL NetLogResetMessageCounts(void);

//
//  Return the number of errors counted on this thread. If there is only one thread, or if multi-threaded logging is disabled,
//  that number will be the same amount as NetLogGetErrorCount returns.
//
DWORD NetLogGetThreadErrorCount(void);

//
//  Return the number of warnings counted on this thread. If there is only one thread, or if multi-threaded logging is disabled,
//  that number will be the same amount as NetLogGetWarningCount returns.
//
DWORD NetLogGetThreadWarningCount(void);

// 
//  Retrieves the counts of messages on this thread. If there is only one thread, or if multi-threaded logging is disabled,
//  that number will be the same amount as NetLogGetMessageCounts returns.
//
BOOL NetLogGetThreadMessageCounts(__out_ecount(NumberCounts) LONG *pMsgCounts, DWORD  NumberCounts);

//
//  Reset the count of messages counted on this thread. If there is only one thread,
//  this does nothing.
// 
BOOL NetLogResetThreadMessageCounts(void);

/////////////////////////////////////////////////////////////////////////////////////////////////
//  #define's for the old names
/////////////////////////////////////////////////////////////////////////////////////////////////
#define InitQALogWrapper		NetLogInitWrapper
#define CleanupQALogWrapper		NetLogCleanupWrapper
#define QASetInitialize			NetLogSetInitialize
#define QAIsInitialized			NetLogIsInitialized
#define QASetWATTOutput			NetLogSetWATTOutput
#define QAError					NetLogError
#define QAMessage				NetLogMessage
#define QAWarning				NetLogWarning
#define QAVerbose				NetLogVerbose
#define QADebug					NetLogDebug
#define QAGetErrorCount			NetLogGetErrorCount
#define QAGetThreadErrorCount	NetLogGetThreadErrorCount
#define QAGetWarningCount		NetLogGetWarningCount
#define QAGetThreadWarningCount NetLogGetThreadWarningCount


#ifdef __cplusplus
}
#endif

#endif
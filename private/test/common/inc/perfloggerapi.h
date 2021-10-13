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
#include <Windows.H>

#ifndef __PERFLOGGERAPI_H__
#define __PERFLOGGERAPI_H__

//
// @doc PerfLogger
//
// @module PerfLogger |
//
// PerfLog is a logging utility for use with performance tests. This module is 
// designed to give developers a consistent interface for writing performance
// tests. 
//
// This library will help make benchmarking results consistent across all tests 
// and will speed up the development process.  The library provides an interface 
// for test applications to register themselves, create event markers, mark timestamps 
// mark CPU and Memory usage, and to output user definable data.
//
// @comm
//
// If DEBUG=1 when built, the logger will create a log file in the release directory
// containing all performance data. The name of the file is based on the platform name, 
// os version, and build as follows: 
//      
// PERF_[platform]_[cpu]_VER[major version]_[minor version]_[build].LOG
//

// @type MARKER_ID | Performance Marker ID Number
typedef DWORD MARKER_ID;

#define MAX_MARKER_ID           100
#define MAX_APPNAME_LEN         32
#define MAX_MARKERNAME_LEN      512

// thread limits
#define MAX_THREADS             128

// log file name
#define MAX_FILENAMELEN         128

#define MARK_CALIBRATE          (MAX_MARKER_ID + 1)
#define MARK_AUTOMEM            (MAX_MARKER_ID + 2)
#define MARK_AUTOCPU            (MAX_MARKER_ID + 3)

#define TOTAL_MARKS             (MAX_MARKER_ID + 4)

// system monitor flags
#define SYS_MON_CPU             1
#define SYS_MON_MEM             2
#define SYS_MON_LOG             4

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
//  @func Initialize perf library
BOOL Perf_LibInit();
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API must be called before using any of the other perf 
//  library functions.  It will be called automatically with the dll 
//  version.  This function is reference counted.
// --------------------------------------------------------------------


// --------------------------------------------------------------------
//  @func Un-initialize perf library
BOOL Perf_LibDeinit();
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API must be called when finished with the perf library.
//  It will be called automatically with the dll version.  This 
//  function is reference counted.
// --------------------------------------------------------------------


// --------------------------------------------------------------------
//  @func Register a named marker with the performance logger. Any time
//  MarkBegin or MarkEnd is called with this value, this string will
//  be associated with the event.
BOOL Perf_RegisterMark(
    MARKER_ID markerId, // @parm ID number to register, max value of <c MAX_MARKER_ID>
    LPCTSTR szMarkName, // @parm formatted string to associate with markerId
    ... // @parmvar additional arguments for the formatted string
    ); 
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API is used to register a marker with the performance logger.  
//  The marker is associated with a formatted string, making it easy to create 
//  dynamic marker descriptions based on test parameters.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Set the name of the calling test
BOOL Perf_SetTestName(
    LPCTSTR szTestName); // @parm name of the calling test
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This name will be associated with all marks during this test run. 
//  The name of the log file will also be based on the test name. Make
//  sure this is the first Perf_* function to be called, or it will fail.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Set the title of the calling test
BOOL Perf_SetTestTitle(
    LPCTSTR szTestTitle); // @parm name of the calling test
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This name will be associated with all marks during this test run. 
//  The name of the log file will also be based on the test name. Make
//  sure this is the first Perf_* function to be called, or it will fail.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log a starting performance marker (timestamp) with the associated 
//  MARKER_ID.
BOOL Perf_MarkBegin(
    MARKER_ID markerId // @parm ID number of a previously registerd marker
    ); 
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm Calls to this API indicate the beginning of an operation whose 
//  duration is to be recorded.  The marker is associated with the string 
//  with which it was registered.  Note that the API does not fail if the 
//  marker was not registered; it outputs an empty marker string in the log 
//  instead.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log an intermediate performance marker (timestamp) with the associated 
//  MARKER_ID.
BOOL Perf_MarkAccumulator(
    MARKER_ID markerId // @parm ID number of a previously registerd marker
    ); 
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm Calls to this API indicate an intermediate step of an operation whose 
//  duration is to be recorded.  The elapsed time since the corresponding
// <f Perf_MarkBegin> is added to the accumulator. The value of the accumulator
// will be logged during the next call to <f Perf_MarkEnd>.
// The marker is associated with the string 
//  with which it was registered.  Note that the API does not fail if the 
//  marker was not registered; it outputs an empty marker string in the log 
//  instead.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log an ending performance marker (timestamp) with the associated
//  MARKER_ID.
BOOL Perf_MarkEnd(
    MARKER_ID markerId // @parm ID number of a previously registered marker
    ); 
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API logs the end of an operation.  The time that is recorded 
//  is with respect to the corresponding <f Perf_MarkBegin>, and is 
//  associated with the string with which it was registered.  Note that the 
//  API does not fail if the marker was not registered; it outputs an empty marker 
//  string in the log instead. If <f Perf_MarkBegin> was not previously called
//  on this marker, then the behavior of this function is undefined.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log an ending performance marker (timestamp) with the associated
//  MARKER_ID.
BOOL Perf_MarkEndAbsolute(
    MARKER_ID markerId, // @parm ID number of a previously registered marker
    DWORD dwMicroseconds // @parm Elapsed time to record
    ); 
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API logs the end of an operation.  The time that is recorded 
//  (dwMicroSeconds) is associated with the string with which it was registered.
//  Note that the API does not fail if the marker was not registered;
//  it outputs an empty marker string in the log instead. If <f Perf_MarkBegin>
//  was not previously called on this marker, then the behavior of this function is undefined.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log an error to be associated with the corresponding MARKER_ID.
BOOL Perf_MarkError(
    MARKER_ID markerId // @parm ID number of a previously registered marker
    ); 
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm This API logs an error associated with the registered marker.
//  This call should be used to signal that an error condition was
//  reached during the test. Calling this API also invalidates any 
//  previous call to <f Perf_MarkBegin>.  If <f Perf_MarkBegin> was 
//  not previously called on this marker, then the behavior of this
//  function is undefined.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Start a thread to monitor and log CPU and memory usage at a given 
//  polling interval. Must be called before MarkMem() or MarkCPU() can 
//  be called.
VOID Perf_StartSysMonitor(
    LPCTSTR szTestName, // @parm stringname to associate with CPU data
    DWORD dwFlags, // @parm conrols the system monitor behavior
                   // @flag SYS_MON_CPU | monitor CPU usage
                   // @flag SYS_MON_MEM | monitor global memory usage
                   // @flag SYS_MON_LOG | automatically log CPU/Memory status
    DWORD dwRefreshMS, // @parm polling/logging inverval (in milliseconds)
    DWORD dwCalibMS // @parm calibration duration for CPU monitor
    ); 
//
//  @comm The <c SYS_MON_LOG> flag will cause the monitor to auto log all
//  CPU and Memory use data. If this flag is not specified, make <f Perf_MarkCPU> 
//  and <f Perf_MarkMem> calls to log CPU and Memory use or nothing will be logged.
//  The <f Perf_MarkCPU> function has undefined behavior if called prior
//  to <f Perf_StartSysMonitor> with the <c SYS_MON_CPU> flag specified.
//  <f Perf_MarkMem> can be used without calling <f Perf_StartSysMonitor>.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Stop the system monitor thread
VOID Perf_StopSysMonitor(VOID);
//
//  @comm Stops the system monitor thread previously started when
//  <f Perf_StartSysMonitor> was called.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log current memory usage with the associated MARKER_ID.
BOOL Perf_MarkMem(
    MARKER_ID markerId // @parm ID number of a previously registerd marker
    );
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm <f Perf_StartSysMonitor> does not need to be called prior 
//  using this API. Use this API when you want to handle logging memory
//  usage at test-specified times rather than at regular intervals.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log current CPU utilization with the associated MARKER_ID.
BOOL Perf_MarkCPU(
    MARKER_ID markerId // @parm ID number of a previously registered marker
    );
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm <f StartSysMonitor> needs to be called prior using the <c SYS_MON_CPU>
//  flag, or data logged by this API will be invalid.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func Log a namd/value pair associated with a registered MARKER_ID
BOOL Perf_MarkAttribute(
    MARKER_ID markerId, // @parm parm ID number of a previously registered marker
    LPCTSTR szAttribName, // @parm name string of the name/value pair
    DWORD dwUserValue // @parm value of the name/value pair
    );
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm Attribute markers are for special case performance tests where values 
//  other than those tracked inherently by the logger need to be recorded.
// --------------------------------------------------------------------


// --------------------------------------------------------------------
//  @func Log a namd/value pair associated with a registered MARKER_ID
BOOL Perf_MarkAttributeDecimal(
    MARKER_ID markerId, // @parm parm ID number of a previously registered marker
    LPCTSTR szAttribName, // @parm name string of the name/value pair
    double dUserValue  // @parm value of the name/value pair
    );


// --------------------------------------------------------------------
//  @func Retrieves the duration of the current marker in Milliseconds
DWORD Perf_MarkGetDuration(
    MARKER_ID markerId // @parm parm ID number of a previously registered marker
    );


// --------------------------------------------------------------------
//  @comm Use to pause the timer for events that should not be measured
// --------------------------------------------------------------------

#ifdef UNDER_CE
// --------------------------------------------------------------------
//  @func pause a timer with a registered MARKER_ID
BOOL Perf_Pause(
    MARKER_ID markerId, // @parm parm ID number of a previously registered marker
    BOOL  fOn           // @parm TRUE to pause the timer, FALSE to restart it
    );
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm Attribute markers are for special case performance tests where values 
//  other than those tracked inherently by the logger need to be recorded.
// --------------------------------------------------------------------

// --------------------------------------------------------------------
//  @func pause all registered timers
BOOL Perf_PauseAll(
    BOOL  fOn           // @parm TRUE to pause the timer, FALSE to restart it
    );
//
//  @rdesc returns the elapsed time since the call to Perf_MarkBegin for the given marker,
//         0 on failure
//
//  @rdesc returns TRUE on success, FALSE on failure
//
//  @comm Perf_MarkGetDuration, used in conjunction with Perf_MarkEndAbsolute,
//        can be used if there is a need to convert the event duration to a more
//        test specific number (MBPS, PPS, etc.) or to simply track the current
//        perf times (stress, etc.).
//
//  @comm Use to pause the timer for events that should not be measured
// --------------------------------------------------------------------
#endif // UNDER_CE

//events for Perf Logger
typedef enum {
    PLE_MARKEND=1
};

typedef struct tag_PerInfo {
    MARKER_ID markerId;
    LPCTSTR szMarkName;
    float ulMilliseconds;
    DWORD dwEvent;
}PERFINFO, * LPPERFINFO;

typedef HRESULT (*PERFCALLBACK) (LPPERFINFO pPerfInfo, LPVOID pUserData);

//  @func register call back function for clients
HRESULT Perf_RegisterCallBack(
    PERFCALLBACK pOnEvent,           // @parm pointer to call back function
                                    // set to NULL to de-register callbacks.
    LPVOID pUserData       //user defined value for call back.
    );


#ifdef __cplusplus
} // end extern "C"
#endif

#endif // __PERFLOGGERAPI_H__

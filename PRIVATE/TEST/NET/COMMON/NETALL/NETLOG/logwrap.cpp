//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
      @doc LIBRARY


Module Name:

  logwrap.cpp

 @module LogWrap - Implements a C wrapper for NetLog class.| 
 
    Description

    Abstracts the logging mechanism provided by NetLog in C style functions.  This allows the 
    program to call more intuitive functions like 'NetLogError' and 'NetLogWarning' to output errors and warnings.  
    Also includes a suite of other functions that allow the user to output retail, debug, and verbose message 
    and query information from the tool


   <nl>Link:    netlog.lib
   <nl>Include: LogWrap.h

     @xref    <f NetLogInitWrapper>
     @xref    <f NetLogCleanupWrapper>
     @xref    <f NetLogIsInitialized>
     @xref    <f NetLogSetInitialize>
	 @xref    <f NetLogSetWATTOutput>
     @xref    <f NetLogError>
     @xref    <f NetLogWarning>
     @xref    <f NetLogMessage>
     @xref    <f NetLogDebug>
     @xref    <f NetLogVerbose>
     @xref    <f NetLogGetErrorCount>
     @xref    <f NetLogGetWarningCount>
     @xref    <f NetLogGetThreadErrorCount>
     @xref    <f NetLogGetThreadWarningCount>
  
*/

#include "logwrap.h"
#include "whichcfg.h"

#ifdef UNDER_CE
 #include "ceassert.h"
 #include "bldver.h"
#else
 #include <assert.h>
 #define _ASSERT assert
#endif

#ifndef _TGTCPU
 #define _TGTCPU "i486"
#endif

//
//  Arbitrary max values, these can be changed if necessary.
//
#define BUFFER_SIZE 4096
#define MAX_THREADS 40

#define MILLISEC_PER_HOUR   (1000*60*60)
#define MILLISEC_PER_MINUTE (1000*60)


//
//  Abstract base class for storing logging classes.
//
class LogTable 
{
public:
    struct Logger {
        DWORD      thread;
        LONG       errors;
        LONG       warnings;
        HANDLE     handle;
        CNetLog *logger;
    };
public:
    LogTable(): m_init(FALSE) {  }
    virtual ~LogTable() {}
    virtual Logger* Find() = 0;
    virtual void    SetLogLevel(DWORD dwLogLevel) = 0;
    virtual void    CleanupDeadThreads(void)     = 0;
    BOOL    IsInit(void) const { return m_init; }
protected:
    Logger CreateNewLog(DWORD thrd_id);
    
    BOOL m_init;
};

//
//  Contains a single instance of the logging class.
//
class SingleLogTable: public LogTable
{
public:
    SingleLogTable() { m_logger = CreateNewLog(0); }
    ~SingleLogTable() { delete m_logger.logger; }
    virtual Logger* Find() { _ASSERT(this); return &m_logger; }
    virtual void    SetLogLevel(DWORD dwLogLevel) { _ASSERT(this); m_logger.logger->SetMaxVerbosity(dwLogLevel); }
    virtual void    CleanupDeadThreads(void)     { _ASSERT(this); return; };
private:
    Logger m_logger;
};

//
//  Contains an instance of the logging class for each thread.
//
class MultiLogTable: public LogTable
{
public:
    MultiLogTable();
    ~MultiLogTable();
    virtual Logger* Find();
    virtual void    SetLogLevel(DWORD dwLogLevel);
    virtual void    CleanupDeadThreads(void);
private:
    Logger m_logs[MAX_THREADS];
    UINT   m_size;
};

struct NetLogWrapParams 
{
    LogTable    *pLogTable;
    BOOL        dwWATTOutput;
    DWORD       dwErrorCount;
    DWORD       dwWarningCount;
    DWORD       dwOutputFlags;
    DWORD       dwLogLevel;
    DWORD       dwStartTick;
    DWORD       dwNameLen;
    TCHAR       szTestName[_MAX_PATH];
    TCHAR       szBuffer[BUFFER_SIZE];
    TCHAR       szNewBuffer[BUFFER_SIZE];
    CRITICAL_SECTION cs;
};

//
//  Globals
//
static struct NetLogWrapParams gParams = {
    NULL,       // Log Table
    TRUE,       // WATT
    0,          // Error count
    0,          // Warning count
    0,          // Output flags
    0,          // Log level
    0,          // Start tick
    0,          // Name length
    _T(""),     // Test name
    _T(""),     // buffer
    _T("")      // new buffer
};

static BOOL  gbInit = FALSE;

static BOOL OutputMessageToScreen(DWORD verbosity, TCHAR *head, LPCTSTR str, va_list pArgs);

//
//  @func BOOL | NetLogInitWrapper |  
//  Initialize the logging wrapper functions.  
//
//  @rdesc TRUE if the wrapper was initialized and FALSE if there was an error.
//
//  @parm  IN LPCTSTR                | pszTestName |  Unique name of the test.  Used to determine the output 
//                                                    file names.  The final file name will be 
//                                                    "%test_name%_%processor%_%build#%_%config%_%pass/fail%.log"
//  @parm  IN DWORD                  | dwLogLevel     |  The level of logging desired, see netlog.h for details.  
//                                                       LOG_PASS is typical for retail builds.
//  @parm  IN DWORD                  | dwOutputFlags  |  The desired output streams, see netlog.h for details.
//  @parm  IN BOOL                   | bUseMultiThread|  Use a different output log file for each thread.
//
//  @comm:(LIBRARY)
//  This function needs to be called before any other log wrapper functions are called.  If it is not 
//  the other functions are likely to ASSERT and/or crash.  After this function succeeds the user can call 
//  the logging functions (<f NetLogError>, <f NetLogMessage>, <f NetLogDebug> or <f NetLogVerbose>).  When a program
//  has completed all logging it should call <f NetLogCleanupWrapper> to make sure the library has cleaned
//  up all necessary resources.
//
//  When the Boolean value 'bUseMultiThread' parameter is set to TRUE, it causes each logging function
//  additional overhead since it must find the appropriate logging object for the thread it is on.
//  This also means that the first time the function is called on a particular thread it will go through
//  additional logic to create and initialize the logging object.  In critically timed cases the user
//  should be aware of this and may have to output a preliminary message such that all the initialization is
//  completed when the critically timed messages are output.
// 
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  This function is not thread safe and should only be called once for each program.  If it is called 
//  multible times on separate threads without calling NetLogCleanupWrapper in between it could cause the 
//  library to get into an unstable state and cause crashes.
//
extern "C" BOOL 
NetLogInitWrapper(LPCTSTR szTestName, DWORD dwLogLevel, DWORD output_flags, BOOL use_multi_thread)
{
    _ASSERT(!gbInit);
    if(gbInit)
        return FALSE;

    //
    //  Record the current tick count so we can figure out how long the test took.
    //
    gParams.dwStartTick = GetTickCount();

    //
    //  Initialize the globals.
    //
    gParams.dwWATTOutput    = TRUE;
    gParams.dwOutputFlags   = output_flags;
    gParams.dwLogLevel      = dwLogLevel;
    _tcsncpy(gParams.szTestName, szTestName, _MAX_PATH);
	gParams.szTestName[_MAX_PATH - 1] = _T('\0');
    
    //
    //  Store the name of the test in the buffer so that each message indicates which test generated the message.
    //
    _tcscpy(gParams.szNewBuffer, gParams.szTestName);
    _tcscat(gParams.szNewBuffer, _T(" "));
    gParams.dwNameLen = _tcslen(gParams.szNewBuffer);
    
    if(use_multi_thread)
    {
        gParams.pLogTable = new MultiLogTable;
    }
    else
    {
        gParams.pLogTable = new SingleLogTable;
    }

    if(gParams.pLogTable)
    {
        if(!gParams.pLogTable->IsInit())
        {   
            delete gParams.pLogTable;
            gParams.pLogTable = 0;

            return FALSE;
        }

        //
        //  Finally indicate that we are initialized.
        //
        gbInit      = TRUE;
        InitializeCriticalSection(&gParams.cs);
        
        return TRUE;
    }
    return FALSE;
}


//
//  @func BOOL | NetLogCleanupWrapper |  
//  Clean up after logging.  
//
//  @rdesc TRUE if the function was successful and FALSE if there was an error.
//
//  @comm:(LIBRARY)
//  Deletes all necessary objects, changes file names to reflect pass or fail (if any errors are logged 
//  it's asumed to be a failure). All logging functions will no longer work after this is called.  This function
//  will fail and ASSERT if the function <f NetLogInitWrapper> was not called first.
//
//  Although the primary purpose of this function is to cleanup after logging it also performs some 
//  alternate functions such as dumping out error information to the user and returning the amount
//  of time has passed since <f NetLogInitWrapper> was called.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//  This function is not thread safe and should only be called once for each program.
//  If this function is called at the same time as one of the logging functions is called it may 
//  causes kernel asserts, as this function will delete the critical section that the other thread is 
//  trying to use.  In this case, the functions attempt to handle the problem gracefully but no guarantees 
//  are made that the system is still in a stable state.  It is up to the program to insure that this situation 
//  will not occur since there is no easy way for the library to detect it.
//
extern "C" BOOL 
NetLogCleanupWrapper()
{
    _ASSERT(gbInit);
    if(!gbInit)
        return FALSE;

    //
    //  Clean up the the variables.  Needs to be atomic just in case there are other threads trying
    //  to output messages at the same time, or if this function is called by more than one thread.
    //
    EnterCriticalSection(&gParams.cs);
    {
        //
        //  Try to gracefully handle two threads calling this function at the same time.
        //
        _ASSERT(gbInit);
        if(!gbInit)
        {
	        LeaveCriticalSection(&gParams.cs);        
            return FALSE;
        }

        //
        //  Determing how long the test took and pretty print it to the user.  Include the actual millisecond count
        //  for a sanity check.  There are more efficient ways of doing this but I wanted it to be readable.
        //
        int TickDifference = GetTickCount() - gParams.dwStartTick;
        int hours   = TickDifference / MILLISEC_PER_HOUR;
        int minutes = (TickDifference - (hours*MILLISEC_PER_HOUR))/MILLISEC_PER_MINUTE;
        int seconds = (TickDifference - ((hours*MILLISEC_PER_HOUR) + (minutes*MILLISEC_PER_MINUTE)))/1000;

        NetLogMessage(_T("Time to complete test: %02d:%02d:%02d total milliseconds: %u"), hours, minutes, seconds, TickDifference);
        NetLogMessage(_T("The test %s with %d errors and %d warnings"), NetLogGetErrorCount() ? _T("FAILED") : _T("PASSED"), 
                                    NetLogGetErrorCount(), NetLogGetWarningCount());

        if(gParams.dwWATTOutput)
        {
            //
            //  Output six "@" signs and the number of errors in order to let the automation parser
			//  know the results of the test.
            //
			if(gParams.pLogTable && gParams.pLogTable->Find()->logger)
				gParams.pLogTable->Find()->logger->Log(LOG_PASS, _T("@@@@@@%d\r\n"), NetLogGetErrorCount());
			else
			{
				_stprintf(gParams.szBuffer, _T("@@@@@@%d\r\n"), NetLogGetErrorCount());
				OutputDebugString(gParams.szBuffer);
			}
        }

        //
        //  Get rid of the log table and make sure all the variables are set to default values.
        //
        delete gParams.pLogTable;
        gbInit = FALSE;
        gParams.pLogTable = 0;
    }
    LeaveCriticalSection(&gParams.cs);        
    DeleteCriticalSection(&gParams.cs);
    
    return TRUE;
}

//
//  @func HANDLE | NetLogSetInitialize | Set the init handle from the logging tool
//
//  @rdesc Returns FALSE if the log could not be set and TRUE if it could be.
//
//  @parm  IN  HANDLE | hInitHandle | The handle aquired from <f NetLogGetLogInitHandle>
// 
//  @comm:(LIBRARY)
//  This is used primarily to copy the current logging state to a loaded DLL.  When it is used
//  there is still only one copy of the logging mechanism, so the two can get out of sync.
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" BOOL
NetLogSetInitialize(IN HANDLE hInitHandle)
{
    _ASSERT(!gbInit);
    _ASSERT(hInitHandle);
    if(gbInit || !hInitHandle)
        return FALSE;

    gParams = *((NetLogWrapParams*)hInitHandle);
    gbInit  = TRUE;
    
    return TRUE;
}

//
//  @func HANDLE | NetLogIsInitialized  | 
//  Determine if the log is initialized.
//
//  @rdesc  Return a HANDLE to the logging if the log wrapper is initialized, HANDLE if it is not.
//
//  @comm:(LIBRARY)
//  This function is not thread safe and could return an incorrect result if another thread at the 
//  same time is calling either the function NetLogCleanupWrapper or NetLogInitWrapper.
//
//  This is used primarily to copy the current logging state to a loaded DLL.  When it is used
//  their is still only one copy of the logging mechanism, so the two can get out of sync.
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" HANDLE 
NetLogIsInitialized()
{
    if(!gbInit)
        return NULL;

    return (HANDLE)&gParams;
}

//
//  @func void | NetLogSetWATTOutput  | 
//  Set state of the system with respect to WATT output.
//
//  @parm   BOOL | wattOutput | When TRUE (system default) the system outputs the 6 @ signs, when
//                              FALSE does not output the 6 @ signs.
//  @comm:(LIBRARY)
//  Changes the system state. It does not output the 6 @ signs to tell the automation server that 
//  the test is done.  This is used when we want to batch tests together.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void
NetLogSetWATTOutput(BOOL wattOutput)
{
    gParams.dwWATTOutput = wattOutput;
}


/////////////////////////////////////////////////////////////////////////////////////////////////
//  Output Routines
/////////////////////////////////////////////////////////////////////////////////////////////////

//
//  @func void | NetLogError |  Output an error message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the error.
//  @parmvar           The parameters that are needed by the format string.
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized using the function 
//  <f NetLogInitWrapper> and not after the function <f NetLogCleanupWrapper> has been called.
//
//  Output an error message and increment the error count. At the NetLog level of LOG_FAIL. Concatenate 
//  the user message to the string "ERROR: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void 
NetLogError(LPCTSTR format, ...)
{
    _ASSERT(gbInit);
    va_list pArgs;
    va_start(pArgs, format);

    if(OutputMessageToScreen(LOG_FAIL, _T("ERROR: "), format, pArgs))
    {
        InterlockedIncrement((LONG*)&gParams.dwErrorCount);
        InterlockedIncrement(&(gParams.pLogTable->Find()->errors));
    }

    va_end(pArgs);
}

//
//  @func void | NetLogWarning |  Output an warning message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the warning.
//  @parmvar           The parameters that are needed by the format string.
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized using the function 
//  <f NetLogInitWrapper> and not after the function <f NetLogCleanupWrapper> has been called.
//
//  Output a warning message and increment the warning count.  At the NetLog level of LOG_WARNING.
//  Concatenate the user message to the string "Wrn: ".
//
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void 
NetLogWarning(LPCTSTR format, ...)
{
    _ASSERT(gbInit);
    va_list pArgs;
    va_start(pArgs, format);

    if(OutputMessageToScreen(LOG_WARNING, _T("Wrn: "), format, pArgs))
    {
	    InterlockedIncrement((LONG*)&gParams.dwWarningCount);
        InterlockedIncrement(&(gParams.pLogTable->Find()->warnings));
    }

    va_end(pArgs);
}

//
//  @func void | NetLogMessage |  Output a message to the user.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message.
//  @parmvar           The parameters that are needed by the format string.
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized using the function 
//  <f NetLogInitWrapper> and not after the function <f NetLogCleanupWrapper> has been called.
//
//  Output a message.  At the NetLog level of LOG_PASS.
//  Concatenate the user message to the string "Msg: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void 
NetLogMessage(LPCTSTR format, ...)
{
    _ASSERT(gbInit);
    va_list pArgs;
    va_start(pArgs, format);

    OutputMessageToScreen(LOG_PASS, _T("Msg: "), format, pArgs);

    va_end(pArgs);
}

//
//  @func void | NetLogDebug |  Output a debug message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message.
//  @parmvar           The parameters that are needed by the format string.
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized using the function 
//  <f NetLogInitWrapper> and not after the function <f NetLogCleanupWrapper> has been called.
//
//  Output a debug message.  Set at the NetLog level of LOG_DETAIL, which is higher than default and is not normally outputed.
//  Concatenate the user message to the string "Dbg: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void 
NetLogDebug(LPCTSTR format, ...)
{
    _ASSERT(gbInit);
    va_list pArgs;
    va_start(pArgs, format);

    OutputMessageToScreen(LOG_DETAIL, _T("Dbg: "), format, pArgs);

    va_end(pArgs);
}

//
//  @func void | NetLogVerbose |  Output a error message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message.
//  @parmvar           The parameters that are needed by the format string.
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized using the function 
//  <f NetLogInitWrapper> and not after the function <f NetLogCleanupWrapper> has been called.
//
//  Output a verbose message.  Set at the NetLog level of LOG_COMMENT, which is higher than default and 
//  is not normally output.
//  Concatenate the user message to the string "Vbs: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" void 
NetLogVerbose(LPCTSTR format, ...)
{
    _ASSERT(gbInit);
    va_list pArgs;
    va_start(pArgs, format);

    OutputMessageToScreen(LOG_COMMENT, _T("Vbs: "), format, pArgs);

    va_end(pArgs);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//  Statistic Routines
/////////////////////////////////////////////////////////////////////////////////////////////////

//
//  @func DWORD | NetLogGetErrorCount |  Get the current number of errors.
//
//  @rdesc Returns the total number of errors currently counted by the logging tool.
//
//  @comm:(LIBRARY)
//
//  This function can be called at any time and is not dependent on the logging tool being initialized.  It will return the 
//  value associated with the most recent run of the logging wrapper.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" DWORD 
NetLogGetErrorCount(void)
{
    return gParams.dwErrorCount;
}

//
//  @func DWORD | NetLogGetWarningCount |  Get the current number of warnings.
//
//  @rdesc Returns the total number of warnings currently counted by the logging tool.
//
//  @comm:(LIBRARY)
//
//  This function can be called at any time and is not dependent on the logging tool being initialized.  It will return the 
//  value associated with the most recent run of the logging wrapper.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" DWORD 
NetLogGetWarningCount(void)
{
    return gParams.dwWarningCount;
}

//
//  @func DWORD | NetLogGetThreadErrorCount |  Get the current number of errors on this thread.
//
//  @rdesc Return the number of errors counted on this thread.
//
//  @comm:(LIBRARY)
//
// This function requires that the logging engine has already been initalized using the function <f NetLogInitWrapper> 
// and not after the function <f NetLogCleanupWrapper> has been called.
//
// If there is only one thread, or if multi-threaded logging is disabled, that number will be the same amount 
// as <f NetLogGetErrorCount> returns.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" DWORD 
NetLogGetThreadErrorCount(void)
{
    _ASSERT(gbInit);
    return gParams.pLogTable->Find()->errors;
}

//
//  @func DWORD | NetLogGetThreadWarningCount |  Get the current number of warnings on this thread.
//
//  @rdesc Return the number of warnings counted on this thread.
//
//  @comm:(LIBRARY)
//
// This function requires that the logging engine has already been initalized using the function <f NetLogInitWrapper> 
// and not after the function <f NetLogCleanupWrapper> has been called.
//
// If there is only one thread, or if multi-threaded logging is disabled, that number will be the same amount 
// as <f NetLogGetWarningCount> returns.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
extern "C" DWORD 
NetLogGetThreadWarningCount(void)
{
    _ASSERT(gbInit);
    return gParams.pLogTable->Find()->warnings;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Private routines to help implement the functions.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Low level function to output to the file or the screen.  Concatenate the user string to the message head and pass
//  the arguments to the sprintf function to be formatted onto the buffer.  And force a linefeed on the end.
//  The NetLog object will decide whether or not to output the message depending on verbosity.
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL 
OutputMessageToScreen(DWORD verbosity, TCHAR* head, LPCTSTR str, va_list pArgs)
{
    _ASSERT(gbInit);
    if(!gbInit)
        return FALSE;

    EnterCriticalSection(&gParams.cs);
    {
        //
        //  Rerun the test inside the critical section to try to gracefully handle the case when this function is called
        //  the same time NetLogCleanupWrapper is called.
        //
        _ASSERT(gbInit);
        if(!gbInit)
        {
	        LeaveCriticalSection(&gParams.cs);        
            return FALSE;
        }

        //
        //  Copy head and string body to the buffer.  Then format the message and concatenate an end of line.
        //
        _tcscpy(gParams.szNewBuffer + gParams.dwNameLen, head);
        
        size_t nNewBufferLength = _tcslen(gParams.szNewBuffer);
        _tcsncat(gParams.szNewBuffer, str, BUFFER_SIZE - (nNewBufferLength+3));
        gParams.szNewBuffer [BUFFER_SIZE-3] = _T('\0');

        //
        //  We have to analyze the arguments here because the only interface into NetLog is the variable parameters list,
        //  not the va_list.  Make sure that we do not try to overrun the buffer, and leave room for the end of line.
        //
        int num_chars = _vsntprintf(gParams.szBuffer, BUFFER_SIZE - 3, gParams.szNewBuffer, pArgs);
        _ASSERT(num_chars != -1);
        _tcscat(gParams.szBuffer, _T("\r\n"));

        //
        //  Pass the buffer to the NetLog logging class.
        //
        _ASSERT(gParams.pLogTable->Find()->logger);
		
		// 4/17/03: Apparently CNetLog::Log takes a format + args so we need to make sure the string doesn't
		// get formatted twice
        //gParams.pLogTable->Find()->logger->Log(verbosity, gParams.szBuffer);
		gParams.pLogTable->Find()->logger->Log(verbosity, _T("%s"), gParams.szBuffer);

    }
    LeaveCriticalSection(&gParams.cs);

    return TRUE;
}

MultiLogTable::MultiLogTable(): m_size(1) 
{ 
    memset(m_logs, 0, sizeof(Logger)*MAX_THREADS); 
    m_logs[0] = CreateNewLog(GetCurrentThreadId());
}

MultiLogTable::~MultiLogTable() 
{ 
    for(UINT i=0; i<m_size; i++) { 
        delete m_logs[i].logger; 
        CloseHandle(m_logs[i].handle);
    }
}

void    
MultiLogTable::SetLogLevel(DWORD dwLogLevel) 
{ 
    _ASSERT(this); 
    for(UINT i=0; i<m_size; i++) 
        m_logs[i].logger->SetMaxVerbosity(dwLogLevel); 
}

LogTable::Logger* 
MultiLogTable::Find() 
{
    _ASSERT(gbInit);
    _ASSERT(this);

    //
    //  Look for log file according to thread.  Use a simple linear search.
    //
    DWORD t = GetCurrentThreadId();
    for(UINT i=0; i<m_size; i++) {
        if(m_logs[i].thread == t) {
            return &(m_logs[i]);
        }
    }
    
    //
    //  Can't go over the maximum amount of threads.
    //
    _ASSERT(i<MAX_THREADS);
    if(i>=MAX_THREADS)
        return NULL;

    //
    //  Log not found, create a new one.  
    //
    m_logs[i] = CreateNewLog(t);
    m_size++;

    //
    //  Get rid of any stale threads to try and make room for more.  Although it is more
    //  intuitive to call this before adding on this current thread to the table, it needs
    //  to be called afterward because this function can output information about closed threads.
    //
    CleanupDeadThreads();

    return &m_logs[i];
}

//
//  CleanupDeadThreads
// 
//  Worker function that implements cleaning up resources for threads that do not exist anymore.  
//

void
MultiLogTable::CleanupDeadThreads(void)
{
    _ASSERT(this);

    UINT count = 0;
    
    //
    //  Create a duplicate of the handles in the format needed by WaitForMultiplyObjects.
    //
    HANDLE handle[MAX_THREADS];

    for(UINT i=0;i<m_size;i++)
        handle[i] = m_logs[i].handle;

    //
    //  Find all of the threads that are not in use anymore.  Remove them from the list.
    //
    DWORD ret = 0;
    do {
        ret = WaitForMultipleObjects(m_size, handle, FALSE, 0); 
        if(m_size > 0 && ret >= WAIT_OBJECT_0 && ret <= (WAIT_OBJECT_0 + m_size - 1))
        {
            NetLogMessage(_T("Thread %8x completed with %d errors and %d warnings"), 
                            m_logs[ret].handle, m_logs[ret].errors, m_logs[ret].warnings);

            // Clear out the object that is not being used.
            delete m_logs[ret].logger;

#ifndef UNDER_CE
            CloseHandle(m_logs[ret].handle);
#endif

            //
            //  Take the current last item on the list and put it where we are now.  Then decrement the
            //  current count.
            //
            m_size--;
            m_logs[ret] = m_logs[m_size];
            handle[ret] = handle[m_size];
        }
		else
			break;  // Either we're out of threads or WaitForMultipleObjects timed out/failed
    } while(1);
}

LogTable::Logger 
LogTable::CreateNewLog(DWORD thrd_id)
{
    _ASSERT(this);

    //
    //  Set up CNetLog to output to a file on the NT machine.  If it is already on 
    //  an NT machine than it just needs to output to a normal file, CE needs to output on PPSH.  
    //  Also have both dump the results to the debug output as well.
    //
    TCHAR config_name[_MAX_PATH] = _T("Unknown");
#ifdef UNDER_CE
    if(!WhichConfig(config_name))
        wcscpy(config_name, L"Unknown");
#endif

    //
    //  Try to get the build number of the OS. 
    //
    DWORD build = WhichBuild();

#ifdef UNDER_CE
    //
    //  Some CE builds don't include the build number.  If this is one of those, try to get it ourselves.
    //  This will not work on local builds because the macro has to be set at build time.  When this is
    //  is true the build number on the file name will be a 0.
    //
    if(build == 0)
        build = CE_BUILD_VER;
#endif

    TCHAR version[_MAX_PATH];
    if(!WhichVersion(version))
        _tcscpy(version, _T(""));
    
    //
    //  Combine everything into a single unique filename.  Skip putting the zero at the end if thrd_id is zero.
    //
    TCHAR  name[_MAX_PATH];    
    if(!thrd_id)
    {
        thrd_id = GetCurrentThreadId();
    }
    _stprintf(name, _T("%s_%hs_%s_%s_%d_%x"), gParams.szTestName, _TGTCPU, config_name, version, build, thrd_id);

    HANDLE handle = 0;
    Logger log;
    log.thread   = thrd_id;
    
#ifndef UNDER_CE
    if(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, NULL, NULL, DUPLICATE_SAME_ACCESS))
        log.handle   = handle;
    else
        log.handle   = 0;
#else
    log.handle   = (HANDLE)::GetCurrentThreadId();
#endif

    log.warnings = 0;
    log.errors   = 0;
    log.logger   = new CNetLog(name, gParams.dwOutputFlags);
    
    _ASSERT(log.logger);
    if(log.logger)
    {
        if(!log.logger->GetLastError())
            m_init = TRUE;

		log.logger->SetMaxVerbosity(gParams.dwLogLevel);
    }

    return log;
}



//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>


///////////////////////////////////////////////////////////////////////////////
//
// @doc SAMPLESTRESSDLL
//
//
// @topic Dll Modules for CE Stress |
//
//	The simplest way to implement a CE Stress module is by creating a DLL
//  that can be managed by the stress harness.  Each stress DLL is loaded and 
//	run by a unique instance of the container: stressmod.exe.  The container
//  manages the duration of your module, collects and reports results to the
//  harness, and manages multiple test threads.
//
//	Your moudle is required to export an initialization function (<f InitializeStressModule>)
//	and a termination function (<f TerminateStressModule>).  Between these calls 
//  your module's main test function (<f DoStressIteration>) will be called in a 
//  loop for the duration of the module's run.  The harness will aggragate and report
//  the results of your module's run based on this function's return values.
//
//	You may wish to run several concurrent threads in your module.  <f DoStressIteration>
//	will be called in a loop on each thread.  In addition, you may implement per-thread
//  initialization and cleanup functions (<f InitializeTestThread> and <f CleanupTestThread>).
//	  
//	<nl>
//	Required functions:
//
//    <f InitializeStressModule> <nl>
//    <f DoStressIteration> <nl>
//    <f TerminateStressModule>
//
//  Optional functions:
//
//    <f InitializeTestThread> <nl>
//    <f CleanupTestThread>
//

//
// @topic Stress utilities |
//
//	Documentation for the utility functions in StressUtils.Dll can be
//  found on:
//
//     \\\\cestress\\docs\\stresss <nl>
//     %_WINCEROOT%\\private\\test\\stress\\stress\\docs
//
//
// @topic Sample code |
//
//	Sample code can be found at: 
//       <nl>\t%_WINCEROOT%\\private\\test\\stress\\stress\\samples <nl>
//
//	Actual module examples can be found at: 
//       <nl>%_WINCEROOT%\\private\\test\\stress\\stress\\modules
//

HANDLE g_hInst = NULL;



///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) BOOL | InitializeStressModule |
//			Called by the harness after your DLL is loaded.
// 
// @rdesc	Return TRUE if successful.  If you return FALSE, CEStress will 
//			terminate your module.
// 
// @parm	[in] <t MODULE_PARAMS>* | pmp | Pointer to the module params info 
//			passed in by the stress harness.  Most of these params are handled 
//			by the harness, but you can retrieve the module's user command
//          line from this structure.
// 
// @parm	[out] UINT* | pnThreads | Set the value pointed to by this param 
//			to the number of test threads you want your module to run.  The 
//			module container that loads this DLL will manage the life-time of 
//			these threads.  Each thread will call your <f DoStressIteration>
//			function in a loop.
// 		
// @comm    You must call InitializeStressUtils( ) (see \\\\cestress\\docs\\stress\\stressutils.hlp) and 
//			pass it the <t MODULE_PARAMS>* that was passed in to this function by
//			the harness.  You may also do any test-specific initialization here.
//
//
//

BOOL InitializeStressModule (
							/*[in]*/ MODULE_PARAMS* pmp, 
							/*[out]*/ UINT* pnThreads
							)
{
	*pnThreads = 3;
	
	// !! You must call this before using any stress utils !!


	InitializeStressUtils (
							_T("SAMPLE3"),									// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GDI, SLOG_FONT | SLOG_RGN),	// Logging zones used by default
							pmp												// Forward the Module params passed on the cmd line
							);

	// Note on Logging Zones: 
	//
	// Logging is filtered at two different levels: by "logging space" and
	// by "logging zones".  The 16 logging spaces roughly corresponds to the major
	// feature areas in the system (including apps).  Each space has 16 sub-zones
	// for a finer filtering granularity.
	//
	// Use the LOGZONE(space, zones) macro to indicate the logging space
	// that your module will log under (only one allowed) and the zones
	// in that space that you will log under when the space is enabled
	// (may be multiple OR'd values).
	//
	// See \test\stress\stress\inc\logging.h for a list of available
	// logging spaces and the zones that are defined under each space.



	// (Optional) 
	// Get the number of modules of this type (i.e. same exe or dll)
	// that are currently running.  This allows you to bail (return FALSE)
	// if your module can not be run with more than a certain number of
	// instances.

	LONG count = GetRunningModuleCount(g_hInst);

	LogVerbose(_T("Running Modules = %d"), count);


	// TODO:  Any module-specific initialization here


	
	
	return TRUE;
}




///////////////////////////////////////////////////////////////////////////////
//
// @func	(Optional) UINT | InitializeTestThread | 
//			Called once at the start of each test thread.  
// 
// @rdesc	Return <t CESTRESS_PASS> if successful.  If your return <t CESTRESS_ABORT>
//			your module will be terminated (although <f TerminateStressModule> 
//			will be called first).
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] int | index | Zero-based index of this thread.  You can use this
//			for indexing arrays of per-thread data.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			initialization only.
//
//
//

UINT InitializeTestThread (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ int index)
{
	LogVerbose(_T("InitializeTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
					hThread, 
					dwThreadId, 
					index 
					);

	return CESTRESS_PASS;
}





///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) UINT | DoStressIteration | 
//			Called once at the start of each test thread.  
// 
// @rdesc	Return a <t CESTRESS return value>.  
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] LPVOID | pv | This can be cast to a pointer to an <t ITERATION_INFO>
//			structure.
// 		
// @comm    This is the main worker function for your test.  A test iteration should 
//			begin and end in this function (though you will likely call other test 
//			functions along the way).  The return value represents the result for a  
//			single iteration, or test case.  
//
//
//

UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ LPVOID pv)
{
	ITERATION_INFO* pIter = (ITERATION_INFO*) pv;

	LogVerbose(_T("Thread %i, iteration %i"), pIter->index, pIter->iteration);


	// TODO:  Do your actual testing here.
	//
	//        You can do whatever you want here, including calling other functions.
	//        Just keep in mind that this is supposed to represent one iteration
	//        of a stress test (with a single result), so try to do one discrete 
	//        operation each time this function is called. 




	// You must return one of these values:

	return CESTRESS_PASS;
	//return CESTRESS_FAIL;
	//return CESTRESS_WARN1;
	//return CESTRESS_WARN2;
	//return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
}



///////////////////////////////////////////////////////////////////////////////
//
// @func	(OPTIONAL) UINT | CleanupTestThread | 
//			Called once before each test thread exits.  
// 
// @rdesc	Return a <f CESTRESS return value>.  If you return anything other than
//			<t CESTRESS_PASS> this will count toward total test results (i.e. as
//			a final iteration of your test thread).
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] int | index | Zero-based index of this thread.  You can use this
//			for indexing arrays of per-thread data.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			clean-up only.
//
//

UINT CleanupTestThread (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ int index)
{
	LogComment(_T("CleanupTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
					hThread, 
					dwThreadId, 
					index 
					);

	return CESTRESS_WARN2;
}



///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) DWORD | TerminateStressModule | 
//			Called once before your module exits.  
// 
// @rdesc	Unused.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			clean-up only.
//

DWORD TerminateStressModule (void)
{
	// TODO:  Do test-specific clean-up here.


	// This will be used as the process exit code.
	// It is not currently used by the harness.

	return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
	g_hInst = hInstance;
    return TRUE;
}

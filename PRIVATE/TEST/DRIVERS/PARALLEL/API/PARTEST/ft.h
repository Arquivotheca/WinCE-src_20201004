#if 0
-------------------------------------------------------------------------------

Copyright (c) 1997, Microsoft Corporation

Module Name:

	ft.h

Description:
ing the symbol __DEFINE_FTE__, we force the file ft.h
	Declares the TUX function table and test function prototypes, except when
	included by globals.cpp; in that case, it defines the function table.

-------------------------------------------------------------------------------
#endif

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))

#ifndef __FT_H__
#define __FT_H__
#endif


// ----------------------------------------------------------------------------
// Local Macros
// ----------------------------------------------------------------------------

#ifdef __DEFINE_FTE__

	#undef BEGIN_FTE
	#undef FTE
	#undef FTH
	#undef END_FTE
	#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
	#define FTH(a, b) { TEXT(a), b, 0, 0, NULL },
	#define FTE(a, b, c, d, e) { TEXT(a), b, c, d, e },
	#define END_FTE { NULL, 0, 0, 0, NULL } };

#else // __DEFINE_FTE__

	#ifdef __GLOBALS_CPP__

		#define BEGIN_FTE 

	#else // __GLOBALS_CPP__

		#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];

	#endif // __GLOBALS_CPP__

	#define FTH(a, b)
	#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
	#define END_FTE

#endif // __DEFINE_FTE__


/*
-------------------------------------------------------------------------------
To create the function table and function prototypes, two macros are available:

	FTH(description, level)
		(Function Table Header) Used for entries that don't have functions,
		entered only as headers (or comments) into the function table.

	FTE(description, level, param, code, function)
		(Function Table Entry) Used for all functions. DON'T use this macro
		if the "function" field is NULL. In that case, use the FTH macro.

You must not use the TEXT or _T macros here. This is done by the FTH and FTE
macros.

In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

The global function table will only be defined once (when it is included by
globals.cpp). The function table will be declared as 'external' when this
file is included by any other source file.

-------------------------------------------------------------------------------
*/


// ----------------------------------------------------------------------------
// TUX Function Table
// ----------------------------------------------------------------------------

#define BASE 1000

BEGIN_FTE

	FTH("API Test", 							0)
	FTE(  "Open/Close Port",			1, 1,		BASE + 0,   TestOpenClosePort )
    FTE(  "Read/Set Timeouts (IOCTL)",	1, FALSE, 	BASE + 1, 	TestReadAndSetTimeouts )
    FTE(  "Read/Set Timeouts (Win32)",	1, TRUE, 	BASE + 2, 	TestReadAndSetTimeouts )    
    FTE(  "Get Device Status",			1, 1, 		BASE + 3, 	TestGetDeviceStatus )
    FTE(  "Write Bytes (IOCTL)", 		1, FALSE, 	BASE + 4, 	WriteBytes )
    FTE(  "Write Bytes (Win32)",		1, TRUE,	BASE + 5,	WriteBytes )
    
// 
// Bi-directional communication is not supported by the Windows CE API
// so the reading test is omitted.
//
#if 0    
     FTE(  "Read Bytes", 				1, 1, 		BASE + 6, 	ReadBytes )
#endif

	FTE(  "Get Device ID", 				1, 1, 		BASE + 6, 	TestGetDeviceID )    

//
// Test may not be needed; see 
//
#if 0
    FTE(  "Get ECP Channel 32", 		1, 1, 		BASE + 8, 	TestGetECPChannel32 )	
#endif

END_FTE

#endif

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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
//	DllEntry()
//
//	Routine Description:
//
//		Implement dllentry() function of enroll.
//
//	Arguments:
//	
//		hinstDLL	::	Instance pointer.
//		Op			::	Reason routine is called.
//		lpvReserved	::	System parameter.
//	
//	Return Value:
//	
//		TRUE / FALSE.
//

#include <windows.h>

extern "C" BOOL
DllEntry (HANDLE hinstDLL, DWORD Op, PVOID lpvReserved)
{
	BOOL Status = TRUE;

	switch (Op) 
	{
		case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls ((HMODULE)hinstDLL);
			break;

		case DLL_PROCESS_DETACH:			
			break;

		case DLL_THREAD_DETACH :
			break;

		case DLL_THREAD_ATTACH :
			break;
			
		default :
			break;
	}
	return Status;

}	// DllEntry()


// Declaring DoEnroll() implemented in Enroll.lib
HRESULT DoEnroll(TCHAR* pCmdLine, HANDLE logHandle);

// Entry point that can be called by other modules to use Enrollment Functionality
extern "C" HRESULT EnrollUsingCmdLine(
    TCHAR* szCmdLine,
    HANDLE logHandle = NULL)
{
    return DoEnroll(szCmdLine, logHandle);
}

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

#include <windows.h>
#include <tchar.h>
#include "basebvt.h"
#include "modtest.h"
#include "basedll.h"

/*****************************************************************************
 *
 *    Description: 
 *
 *       BaseModuleTest tests LoadLibrary() and invokes a Library Function. 
 *
 *    Need to have the BaseDll.dll built in the release dir for this test.
 *
 *****************************************************************************/

TESTPROCAPI 
BaseModuleTest(
               UINT uMsg, 
               TPPARAM /*tpParam*/, 
               LPFUNCTION_TABLE_ENTRY /*lpFTE*/
               ) 
{
    HANDLE hTest=NULL;
    HINSTANCE hModule=NULL,h2Module=NULL;


    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }
    hTest = StartTest( TEXT("Base Module Test") );
    
    do{

        LogDetail(TEXT("Check if a dll can be loaded using LoadLibrary API"));
        CHECKTRUE( BaseLoadLibrary(DLL_NAME,&hModule));

        LogDetail( TEXT("check if a predefined entry point in this DLL can be invoked"));
        CHECKTRUE( BaseInvokeLibraryFunction(hModule,DLL_FUNCTION_NAME));

        LogDetail(TEXT("Check if a dll can be re-loaded using LoadLibrary API again, to get another module handle"));
        CHECKTRUE( BaseLoadLibrary(DLL_NAME,&h2Module));

        LogDetail(TEXT("check if a predefined entry point in this reloaded DLL can be invoked"));
        CHECKTRUE( BaseInvokeLibraryFunction(h2Module,DLL_FUNCTION_NAME));

        LogDetail(TEXT("call Free module using the 2nd handle"));
        CHECKTRUE( BaseFreeLibrary(h2Module));

        LogDetail(TEXT("call FreeModule using the 1st handle"));
        CHECKTRUE( BaseFreeLibrary(hModule));

        goto DONE;

    }while(FALSE);

    FailTest(hTest);
    BaseFreeLibrary(hModule);

DONE : 
    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


/*****************************************************************************
*
*   Name    : BaseLoadLibrary
*
*   Purpose : This function calls the API LoadLibrary which loads a
*             DLL into the memory
*
*    Input  : variation number, dll pathname, address where handle to the
*             loaded DLL should be stored.
*
*
*   Exit    : none
*
*   Calls   : LoadLibrary
*
*
*
*****************************************************************************/


BOOL BaseLoadLibrary( LPTSTR lpDllName, HINSTANCE *phModule)
{
    *phModule  = YLoadLibrary(lpDllName);

    if (phModule == NULL)
    {
        LogDetail ( TEXT("Make sure BaseDll.dll is present in the release dir"));
    }
    return( phModule != NULL);
    
}



/*****************************************************************************
*
*   Name    : BaseInvokeLibraryFunction
*
*   Purpose : Verify that GetProcAddress give the function pointer of the
*             entry point, and this entry point can be invoked with success.
*
*
*    Input  : variation number, handle of the dll which has the entry point,
*             name of the entry point in this dll which is invoked.
*
*
*
*   Exit    : none
*
*   Calls   : GetProcAddress, FunctionPointer(entry point inside the dll)
*
*
*
*****************************************************************************/


BOOL BaseInvokeLibraryFunction(HINSTANCE hModule, LPTSTR lpProcName)

{

    FARPROC FunctionPointer = NULL;
    DWORD   dwStatus = 0;

    FunctionPointer = YGetProcAddress(hModule,lpProcName);

    if (!FunctionPointer)  {
        return(FALSE);
        
    }

    dwStatus = FunctionPointer();

    return( dwStatus == DLL_FUNCTION_RETURN);
    
}



/*****************************************************************************
*
*   Name    : BaseFreeLibrary
*
*   Purpose : invoke FreeLibrary API to free the loaded dll.
*
*
*
*    Input  : variation number, handle of the loaded DLL
*
*
*
*
*   Exit    : none
*
*   Calls   : FreeLibrary
*
*
*
*****************************************************************************/


BOOL BaseFreeLibrary(HINSTANCE hModule)
{

    BOOL bRc = FALSE;


    bRc = FreeLibrary( hModule);

    return(bRc);
    
}

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
//
//  TUXTEST TUX DLL
//
//  Module: WMFunctional_Tests_No_Media.cpp
//          Contains Camera Driver Property tests
//          CSPROPSETID_Pin
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"
#define DEFINE_CAMERA_TEST_PROPERTIES
#include "CameraSpecs.h"
#include <camera.h>

////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CTYPE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CTYPE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_CTYPES.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CTYPE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CTYPE : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulMaxNumOfPinTypes    = 3 ;
    ULONG        ulMinNumOfPinTypes    = 2 ;
    ULONG       ulNumOfPinTypes = 0;
    DWORD        dwBytesReturned = 0 ;
    
    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes > ulMaxNumOfPinTypes || ulNumOfPinTypes < ulMinNumOfPinTypes )
    {
        FAIL( TEXT( "Test_PIN_CTYPE : PIN_CTYPES do not match expected value" ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_PIN_CTYPE : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CTYPE2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CTYPE2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_PIN_CTYPES. Verify that a client can not set the number of Pin types supported by
//                    the driver.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CTYPE2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CTYPE2 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 0
    LONG        lNumOfPinTypes = 0;
    DWORD        dwBytesReturned = 0 ;
    
    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &lNumOfPinTypes, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE2 : Should not have been able to set CSPROPERTY_PIN_CTYPES " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_PIN_CTYPE2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CTYPE3
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CTYPE3
//
//  Assertion:        
//
//  Description:    3: Test CSPROPERTY_PIN_CTYPES. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CTYPE3( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwBytesReturned = 0 ;
    ULONG        ulTestBuffer;

    csProp.Set    = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    // causes a handled exception in the driver, but fails properly
#if 0
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            sizeof( ULONG64 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of NULL output buffer  " ) ) ;
    }
#endif

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ulTestBuffer, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of 0 size output buffer  " ) ) ;
    }

    bool bTestVar = false ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( bool ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of BOOL output buffer  " ) ) ;
    }

    INT16 wTestVar = 0 ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( INT16 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of WORD output buffer  " ) ) ;
    }

    ULONG64 ul64TestVar = 0 ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ul64TestVar, 
                                                sizeof( ULONG64 ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CTYPE3 :  Querying for CSPROPERTY_PIN_CTYPES should have not have failed incase of ULONG64 output buffer  " ) ) ;
    }

done:    
    Log( TEXT( "Test_PIN_CTYPE3 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CINSTANCES
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CINSTANCES
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_CINSTANCESS. Verify that DeviceIOControl 
//                           succeeds for READ operation and max number of streams this pin creates do not exceed 1
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CINSTANCES( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;

    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;
    
    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;

    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_CINSTANCES ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_GET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        CSPIN_CINSTANCES    csPinCInstances ;

        dwBytesReturned        = 0 ;
        csPin.PinId            = iCount ;
        if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                    &csPin, 
                                                    sizeof( CSP_PIN ), 
                                                    &csPinCInstances , 
                                                    sizeof( CSPIN_CINSTANCES ), 
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            FAIL( TEXT( "Test_PIN_CINSTANCES : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CINSTANCES " ) ) ;
            goto done ;
        }

        if ( 1 < csPinCInstances.PossibleCount )
        {
            FAIL( TEXT( "Test_PIN_CINSTANCES : Maximum number of streams per pin can not exceed 1 " ) ) ;
            goto done ;
        }

        if ( 0 != csPinCInstances.CurrentCount )
        {
            FAIL( TEXT( "Test_PIN_CINSTANCES : Current number of streams for a given pin must be 0 before creating the stream" ) ) ;
            goto done ;
        }

    }
    
done:    
    Log( TEXT( "Test_PIN_CINSTANCES : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CINSTANCES2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CINSTANCES2
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_CINSTANCESS. Verify that DeviceIOControl 
//                           fails for WRITE operation
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CINSTANCES2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES2 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;
    
    csProp.Set    = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES2 : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_CINSTANCES2 : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;
    
    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_CINSTANCES ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_SET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        dwBytesReturned = 0 ;
        CSPIN_CINSTANCES csPinCInstances ;
        csPinCInstances.PossibleCount    = 5 ;
        csPinCInstances.CurrentCount    = 3 ;

        csPin.PinId            = iCount ;
        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                    &csPin, 
                                                    sizeof( CSP_PIN ), 
                                                    &csPinCInstances , 
                                                    sizeof( CSPIN_CINSTANCES ), 
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            FAIL( TEXT( "Test_PIN_CINSTANCES2 : CSPROPERTY_PIN_CINSTANCES should be a ReadOnly property" ) ) ;
            goto done ;
        }

    }
    
done:    
    Log( TEXT( "Test_PIN_CINSTANCES2 : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_NAME
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_NAME
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_NAME. Verify that DeviceIOControl 
//                           succeeds for READ operation you get the name of the Pin
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_NAME( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_NAME : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_NAME : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_NAME : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_NAME : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;

    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_NAME ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_GET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_NAME : Testing Pin %d" ), iCount ) ;

        TCHAR *szPinName = NULL ;
        csPin.PinId            = iCount ;
        dwBytesReturned = 0 ;
        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                NULL , 
                                                0,
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_NAME : TestDeviceIOControl should have failed." ) ) ;
            goto done ;
        }

        szPinName = new TCHAR[ dwBytesReturned ] ;
        if ( NULL == szPinName )
        {
            FAIL( TEXT( "Test_PIN_NAME : OOM " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                szPinName, 
                                                dwBytesReturned,
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_NAME : TestDeviceIOControl should not have failed." ) ) ;
            SAFEARRAYDELETE( szPinName ) ;
            goto done ;
        }

        if ( NULL == szPinName )
        {
            FAIL( TEXT( "Test_PIN_NAME : This pin should have a name associated to it" ) ) ;
            SAFEARRAYDELETE( szPinName ) ;
            goto done ;
        }

        SAFEARRAYDELETE( szPinName ) ;

    }
    
done:    
    Log( TEXT( "Test_PIN_NAME : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_NAME2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_NAME2
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_NAME. Verify that DeviceIOControl 
//                           fails for WRITE operation.
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_NAME2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_NAME2 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_NAME2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_NAME2 : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_NAME2 : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;
    
    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_NAME ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_SET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_NAME2 : Testing Pin %d" ), iCount ) ;

        TCHAR szPinName[] = TEXT( "DUMMYNAME" ) ;
        
        csPin.PinId            = iCount ;
        dwBytesReturned = 0 ;

        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                szPinName, 
                                                dim( szPinName ),
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_NAME : TestDeviceIOControl should have failed." ) ) ;
            goto done ;
        }

    }
    
done:    
    Log( TEXT( "Test_PIN_NAME : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CATEGORY
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CATEGORY
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_CATEGORY. Verify that DeviceIOControl 
//                           succeeds for READ operation you get the name of the Pin
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CATEGORY( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;
    
    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_CATEGORY ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_GET ; 
    csPin.Reserved             = 0 ;

    DWORD matchCount[MAX_STREAMS] = {0};
    static GUID guidExpected[MAX_STREAMS] = {
        PINNAME_VIDEO_PREVIEW,
        PINNAME_VIDEO_CAPTURE,
        PINNAME_VIDEO_STILL
    };

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_CATEGORY : Testing Pin %d" ), iCount ) ;
        GUID guidCategory ;

        csPin.PinId            = iCount ;
        dwBytesReturned        = 0 ;

        if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                    &csPin, 
                                                    sizeof( CSP_PIN ), 
                                                    &guidCategory, 
                                                    sizeof ( GUID ),
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            FAIL( TEXT( "Test_PIN_CATEGORY : TestDeviceIOControl should not have failed." ) ) ;
            goto done ;
        }



        for ( INT iIndex = 0 ; iIndex < MAX_STREAMS ; ++iIndex )
        {
            if ( guidCategory == guidExpected[iIndex] )
            {
                ++matchCount[iIndex];
                break;
            }
        }

        if ( iIndex == MAX_STREAMS )
        {
            FAIL( TEXT( "Test_PIN_CATEGORY : Incorrect guid returned for the pin." ) ) ;
            goto done ;
        }
    }

    if ( matchCount[STREAM_PREVIEW] > 1 )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : CAPTURE Pin number should be 0 or 1." ) ) ;
        goto done ;
    }

    if ( matchCount[STREAM_CAPTURE] != 1 )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : CAPTURE Pin number should be 1." ) ) ;
        goto done ;
    }

    if ( matchCount[STREAM_STILL] != 1 )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY : CAPTURE Pin number should be 1." ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_PIN_CATEGORY : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_CATEGORY2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_CATEGORY2
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_CATEGORY. Verify that DeviceIOControl 
//                           fails for WRITE operation.
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_CATEGORY2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_CATEGORY2 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY2 : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_CATEGORY2 : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;
    
    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_CATEGORY ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_SET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_CATEGORY2 : Testing Pin %d" ), iCount ) ;
        const GUID guidTestVar =     {     0x22D6F312L, 
                                    0xB0F6, 
                                    0x11D0,
                                    {     0x94, 0xAB, 0x00, 0x80,
                                        0xC7, 0x4C, 0x7E, 0x95 
                                    } 
                                };

        csPin.PinId            = iCount ;
        dwBytesReturned        = 0 ;

        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                (LPVOID)&guidTestVar, 
                                                sizeof( GUID ),
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_CATEGORY : TestDeviceIOControl should have failed, succeeded in setting the pin GUID." ) ) ;
            goto done ;
        }

    }
    
done:    
    Log( TEXT( "Test_PIN_CATEGORY : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_DATARANGES
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_DATARANGES
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_DATARANGESS.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_DATARANGES( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_DATARANGES : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;
    
    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ulNumOfPinTypes, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES : PIN_CTYPES should be greater than 0" ) ) ;
        goto done ;
    }

    // iCount is passed into GetDataRanges, which takes the test's pin identifier, not the real pin id.
    // so, test all three pins and if there's no available pin instance for one of the ones being tested, skip it.
    for ( INT iCount = 0 ; iCount < 3 ; iCount++ )
    {
    //    For each pin, determine the dataranges
        Log( TEXT( "Test_PIN_DATARANGES : Testing Pin %d" ), iCount ) ;

        if(!camTest.AvailablePinInstance(iCount))
        {
            Log(TEXT("Pin %d not available, continuing test."), iCount);
            continue;
        }
        
        PCSMULTIPLE_ITEM pCSMultipleItem = NULL ;
        
        dwBytesReturned = 0 ;

        if ( FALSE == camTest.GetDataRanges(     iCount /*PIN ID*/,
                                                &pCSMultipleItem /*MultipleItem buffer*/ ) )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES  : GetDataRanges returned FALSE." ) ) ;
            goto done ;
        }

        if ( NULL == pCSMultipleItem )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES  : pCSMultipleItem is NULL." ) ) ;
            goto done ;
        }

        if ( 0 == pCSMultipleItem->Count )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES  : pCSMultipleItem->Count should be greater than 0. No dataranges provided by the driver." ) ) ;
            goto done ;            
        }

        if ( sizeof ( CSMULTIPLE_ITEM ) >= pCSMultipleItem->Size )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES  : pCSMultipleItem->Size should be greater than sizeof(CSMULTIPLE_ITEM). Size member does not include size for datarange structures to follow." ) ) ;
            goto done ;            
        }

        PCS_DATARANGE_VIDEO pCSDataRangeVid = NULL ;
        pCSDataRangeVid = reinterpret_cast< PCS_DATARANGE_VIDEO > ( pCSMultipleItem + 1 ) ;

        if ( NULL == pCSDataRangeVid )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES  : pCSDataRange can not be NULL." ) ) ;
            goto done ;                        
        }

        INT iIndex = 0 ;
        do
        {
            Log( TEXT( "Test_PIN_DATARANGES : Testing Pin %d, DataRange %d" ), iCount, iIndex ) ;

            iIndex++ ;
            if (     CSDATAFORMAT_TYPE_WILDCARD         == pCSDataRangeVid->DataRange.MajorFormat ||
                    CSDATAFORMAT_SUBTYPE_WILDCARD     == pCSDataRangeVid->DataRange.SubFormat ||
                    CSDATAFORMAT_SPECIFIER_WILDCARD     == pCSDataRangeVid->DataRange.Specifier )
            {
                if ( !(     CSDATAFORMAT_TYPE_WILDCARD         == pCSDataRangeVid->DataRange.MajorFormat &&
                            CSDATAFORMAT_SUBTYPE_WILDCARD     == pCSDataRangeVid->DataRange.SubFormat &&
                            CSDATAFORMAT_SPECIFIER_WILDCARD     == pCSDataRangeVid->DataRange.Specifier )
                )
                {
                    FAIL( TEXT( "Test_PIN_DATARANGES  : If specifying wildcards, MajorFormat, Subformat and Specifier should all have wildcards." ) ) ;
                    break ;
                }
            }
            pCSDataRangeVid = reinterpret_cast< PCS_DATARANGE_VIDEO > ( pCSDataRangeVid +  1 ) ;

        } while ( iIndex != pCSMultipleItem->Count ) ;
    }

done:    

    Log( TEXT( "Test_PIN_DATARANGES : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_DATARANGES1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_DATARANGES1
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_DATARANGES1S.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_DATARANGES1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_DATARANGES1 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES1 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES1 : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_DATARANGES1 : PIN_CTYPES should be greater than 0" ) ) ;
        goto done ;
    }


    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount++ )
    {
    //    For each pin, determine the dataranges
        PCSMULTIPLE_ITEM pCSMultipleItem  = NULL ;
        CSP_PIN csPin ;

        Log( TEXT( "Test_PIN_DATARANGES1 : Testing Pin %d" ), iCount ) ;

        dwBytesReturned = 0 ;

        pCSMultipleItem = reinterpret_cast< PCSMULTIPLE_ITEM > ( new BYTE[ sizeof( CSMULTIPLE_ITEM ) + sizeof( CSDATARANGE ) ] ) ;
        
        if ( NULL == pCSMultipleItem )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES1: OOM." ) ) ;
            goto done ;
        }

        pCSMultipleItem->Count  = 1 ;
        pCSMultipleItem->Size    = sizeof( CSMULTIPLE_ITEM ) + sizeof( CSDATARANGE ) ;

        PCSDATARANGE                pCSDataRange = NULL ;
        
        pCSDataRange                = reinterpret_cast< PCSDATARANGE > ( pCSMultipleItem + 1 ) ;
        pCSDataRange->FormatSize     = sizeof( CSDATARANGE ) ;
        pCSDataRange->MajorFormat     = CSDATAFORMAT_TYPE_WILDCARD ;
        pCSDataRange->SubFormat        = CSDATAFORMAT_SUBTYPE_WILDCARD ;
        pCSDataRange->Specifier        = CSDATAFORMAT_SPECIFIER_WILDCARD ;

        csPin.Property.Set            = CSPROPSETID_Pin ;
        csPin.Property.Id            = CSPROPERTY_PIN_DATARANGES ;
        csPin.Property.Flags        = CSPROPERTY_TYPE_SET ; 
        csPin.Reserved                 = 0 ;
        csPin.PinId                    = iCount ;

        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                    &csPin, 
                                                    sizeof( CSP_PIN ), 
                                                    pCSMultipleItem, 
                                                    sizeof( *pCSMultipleItem ),
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            FAIL( TEXT( "Test_PIN_DATARANGES1: TestDeviceIOControl should have failed." ) ) ;
            delete[] pCSMultipleItem;
            pCSMultipleItem = NULL;
            goto done ;
        }

        delete[] pCSMultipleItem;
        pCSMultipleItem = NULL;
    }

done:    

    Log( TEXT( "Test_PIN_DATARANGES1 : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_DEVICENAME
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_DEVICENAME
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_DEVICENAME. Verify that DeviceIOControl 
//                           succeeds for READ operation you get the name of the Pin
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_DEVICENAME( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;

    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_DEVICENAME ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_GET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_DEVICENAME : Testing Pin %d" ), iCount ) ;

        TCHAR *szPinName = NULL ;
        csPin.PinId            = iCount ;
        dwBytesReturned = 0 ;
        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                NULL , 
                                                0,
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_DEVICENAME : TestDeviceIOControl should have failed." ) ) ;
            goto done ;
        }

        szPinName = new TCHAR[ dwBytesReturned ] ;
        if ( NULL == szPinName )
        {
            FAIL( TEXT( "Test_PIN_DEVICENAME : OOM " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                szPinName, 
                                                dwBytesReturned,
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_DEVICENAME : TestDeviceIOControl should not have failed." ) ) ;
            SAFEARRAYDELETE( szPinName ) ;
            goto done ;
        }

        if ( NULL == szPinName )
        {
            FAIL( TEXT( "Test_PIN_DEVICENAME : This pin should have a name associated to it" ) ) ;
            SAFEARRAYDELETE( szPinName ) ;
            goto done ;
        }

        SAFEARRAYDELETE( szPinName ) ;

    }
    
done:    
    Log( TEXT( "Test_PIN_DEVICENAME : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_PIN_DEVICENAME2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_PIN_DEVICENAME2
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_PIN_DEVICENAME. Verify that DeviceIOControl 
//                           fails for WRITE operation.
//
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_PIN_DEVICENAME2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME2 : Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    ULONG        ulNumOfPinTypes = 0 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set        = CSPROPSETID_Pin ;
    csProp.Id        = CSPROPERTY_PIN_CTYPES ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                &csProp, 
                                                sizeof( CSPROPERTY ), 
                                                &ulNumOfPinTypes, 
                                                sizeof( ULONG ), 
                                                &dwBytesReturned,
                                                NULL ) )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME2 : TestDeviceIOControl failed. Unable to get CSPROPERTY_PIN_CTYPES  " ) ) ;
        goto done ;
    }

    if ( ulNumOfPinTypes == 0 )
    {
        FAIL( TEXT( "Test_PIN_DEVICENAME2 : CSPROPERTY_PIN_CTYPES should be > 0" ) ) ;
        goto done ;
    }

    CSP_PIN csPin ;
    
    csPin.Property.Set        = CSPROPSETID_Pin ;
    csPin.Property.Id        = CSPROPERTY_PIN_DEVICENAME ;
    csPin.Property.Flags    = CSPROPERTY_TYPE_SET ; 
    csPin.Reserved             = 0 ;

    for ( INT iCount = 0 ; iCount < (INT)ulNumOfPinTypes ; iCount ++ )
    {
        Log( TEXT( "Test_PIN_DEVICENAME2 : Testing Pin %d" ), iCount ) ;

        TCHAR szPinName[] = TEXT( "DUMMYNAME" ) ;
        
        csPin.PinId            = iCount ;
        dwBytesReturned = 0 ;

        if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                                &csPin, 
                                                sizeof( CSP_PIN ), 
                                                szPinName, 
                                                dim( szPinName ),
                                                &dwBytesReturned,
                                                NULL ) )
        {
            FAIL( TEXT( "Test_PIN_DEVICENAME : TestDeviceIOControl should have failed." ) ) ;
            goto done ;
        }

    }
    
done:    
    Log( TEXT( "Test_PIN_DEVICENAME : Test Completed. " ) ) ;
    return GetTestResult();
}


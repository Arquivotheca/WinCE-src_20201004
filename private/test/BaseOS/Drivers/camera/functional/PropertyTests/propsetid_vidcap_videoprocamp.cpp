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
//          CSPROPSETID_VIDCAP_VIDEOPROCAMP
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"
#include "CameraSpecs.h"

BOOL
RetrieveVidProcAmpParams(GUID guidPropertySet, ULONG ulProperty, LONG *lUpperBound, LONG *lLowerBound, LONG *lDefaultValue, LONG *lDelta)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    PCSPROPERTY_STEPPING_LONG pCSPropSteppingLong = NULL;
    PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL ;
    PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL ;
    PCSPROPERTY_DESCRIPTION pCSPropDescription = NULL ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Camera: Failed to determine camera availability." ) ) ;
        return FALSE;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Camera: InitializeDriver failed " ) ) ;
        return FALSE;
    }

    camTest.PrepareTestEnvironment(     guidPropertySet,  
                                    ulProperty,
                                    VT_I4 ) ;

    if ( FALSE == camTest.IsPropertySetSupported( ) )
    {
        SKIP ( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOPROCAMP is not a supported property set" ) ) ;
        return FALSE;
    }

    if ( FALSE == camTest.FetchAccessFlags ( ) )
    {
        SKIP( TEXT( "Camera: FetchAccessFlags returned FALSE, property not supported. " ) ) ;
        return FALSE;
    }

    if ( camTest.IsReadAllowed( ) )
    {
        // first, fetch the ranges
        if ( FALSE == camTest.FetchBasicSupport( ) )
        {
            FAIL( TEXT( "Camera: FetchBasicSupport() returned FALSE. " ) ) ;
            return FALSE;
        }

        if ( FALSE == camTest.GetValueBuffer( &pCSPropDescription ) ) 
        {
            FAIL( TEXT( "Camera: GetValueBuffer() returned FALSE. " ) ) ;
            return FALSE;
        }

        pCSPropMemberList = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> ( pCSPropDescription + 1 ) ;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >( pCSPropMemberList ) ;
        pCSPropSteppingLong = reinterpret_cast< PCSPROPERTY_STEPPING_LONG >( pCSPropMemberHeader + 1 ) ;

        if(lLowerBound)
            *lLowerBound = pCSPropSteppingLong->Bounds.UnsignedMinimum;

        if(lUpperBound)
            *lUpperBound = pCSPropSteppingLong->Bounds.UnsignedMaximum;

        if(lDelta)
            *lDelta = pCSPropSteppingLong->SteppingDelta;

        // now fetch the default value
        if ( FALSE == camTest.FetchDefaultValues( ) )
        {
            FAIL( TEXT( "Camera: FetchDefaultValues() returned FALSE. " ) ) ;
            return FALSE;
        }

        if ( FALSE == camTest.GetValueBuffer( &pCSPropDescription ) ) 
        {
            FAIL( TEXT( "Camera: GetValueBuffer() returned FALSE. " ) ) ;
            return FALSE;
        }

        pCSPropMemberList = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> ( pCSPropDescription + 1 ) ;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >( pCSPropMemberList ) ;
        pCSPropSteppingLong = reinterpret_cast< PCSPROPERTY_STEPPING_LONG >( pCSPropMemberHeader + 1 ) ;

        LPVOID lpMembers = *reinterpret_cast<LPVOID *>( pCSPropMemberHeader + 1 ) ;
        LONG lDetectedDefaultValue = (LONG) lpMembers ;

        if(lDefaultValue)
            *lDefaultValue = lDetectedDefaultValue;
    }

    return TRUE;
}

void
Test_CSProperty(GUID guidPropertySet, ULONG ulProperty, LONG lUpperBound, LONG lLowerBound, LONG lMaxCount, LONG lDefaultValue)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    RECT rc;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    Log( TEXT( "Camera :  Default value is %d, upper bound %d, lower bound %d" ), lDefaultValue, lUpperBound, lLowerBound ) ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Camera: Failed to determine camera availability." ) ) ;
        return ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Camera: InitializeDriver failed " ) ) ;
        return ;
    }

    camTest.PrepareTestEnvironment(     guidPropertySet,  
                                    ulProperty,
                                    VT_I4 ) ;

    // Let's first see if the driver supports PROPSETID_VIDCAP_VIDEOPROCAMP or not

    if ( FALSE == camTest.IsPropertySetSupported( ) )
    {
        FAIL ( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOPROCAMP is not a supported property set" ) ) ;
        return;
    }

    // We are here so PROPSETID_VIDCAP_VIDEOPROCAMP is a supported property.
    // Now we'll query for the accessflags of Brightness, i.e., Read-only, R/W etc

    if ( FALSE == camTest.FetchAccessFlags ( ) )
    {
        FAIL( TEXT( "Camera: FetchAccessFlags returned FALSE. " ) ) ;
        return;
    }

    if(FALSE == camTest.CreateStream(STREAM_CAPTURE, NULL, rc))
    {
        FAIL(TEXT("Camera : Creating the stream failed "));
        return;
    }

    if(0 == camTest.SelectVideoFormat(STREAM_CAPTURE, 0))
    {
        FAIL(TEXT("Test_IO_Preview : No supported formats on the pin "));
        return;
    }

    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
        return;
    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
    {
        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
        return;
    }

    /*    Now AccessFlags should contain a combination of
        CSPROPERTY_TYPE_GET
        CSPROPERTY_TYPE_SET
        CSPROPERTY_TYPE_DEFAULT
        This will later be used to decide what kind of data to expect and test to perform
    */

    if ( camTest.IsReadAllowed( ) )
    {
        // This property allows us to read it's current or set of default values. 

        // Let's read the current value first
        CSPROPERTY_VIDEOPROCAMP_S csPropertyVidProcAmp ;
        if ( FALSE == camTest.GetCurrentPropertyValue(    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ), 
                                                    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ) ) )
        {
            FAIL( TEXT( "Camera: GET query for the property failed. " ) ) ;
            return;
        }

        Log( TEXT( "Camera :  Current value is %d" ), csPropertyVidProcAmp.Value ) ;

        if(csPropertyVidProcAmp.Value > lUpperBound || 
            csPropertyVidProcAmp.Value < lLowerBound)
        {
            FAIL( TEXT( "Camera: current value outside of valid range. " ) ) ;
        }

        // Now Let's try to retrieve a list of default values if applicable. For this we'll call DeviceIOControl twice.
        // First DeviceIOControl call  will the size of datastructure returned. Based on this size, we'll then allocate 
        // memory. Then pass on this buffer in the second DeviceIOControl call to get the buffer.

        if ( FALSE == camTest.HasDefaultValue( ) )
        {
            FAIL( TEXT( "Camera: Default value does not exist. " ) ) ;
        }
        
        if ( FALSE == camTest.FetchDefaultValues( ) )
        {
            FAIL( TEXT( "Camera: FetchDefaultValues() returned FALSE. " ) ) ;
        }

        PCSPROPERTY_DESCRIPTION pCSPropDescription = NULL ;
        if ( FALSE == camTest.GetValueBuffer( &pCSPropDescription ) ) 
        {
            FAIL( TEXT( "Camera: GetValueBuffer() returned FALSE. " ) ) ;
        }

        // Now we are all set to traverse the property data. The data is packed in the form of
        // CSPROPERTY_DESCRIPTION 
        // CSPROPERTY_MEMBERSLIST[pCSPropDescritpion->MembersListCount]

        if ( NULL == pCSPropDescription )
        {
            FAIL( TEXT( "Camera: pCSPropDescription structure is NULL. " ) ) ;
            return;
        }

        LONG lCounter = pCSPropDescription->MembersListCount ;

        if ( lMaxCount != lCounter )
        {
            FAIL( TEXT( "Camera: incorrect number of members in the data structure. " ) ) ;
        }
        
        // Get to the first CSPROPERTY_MEMBERSLIST element, 
        // which is right after CSPROPERTY_DESCRIPTION structure;

        PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL ;

        pCSPropMemberList = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> ( pCSPropDescription + 1 ) ;

        if ( NULL == pCSPropMemberList )
        {
            FAIL( TEXT( "Camera : MembersList structure is NULL." ) ) ;
            return;
        }

        // Now we will be traversing this array of CSPROPERTY_MEMBERSLIST

        /*
        Format of CSPROPERYT_MEMBERLIST

        typedef struct {
            CSPROPERTY_MEMBERSHEADER    MembersHeader;
            const VOID*                 Members;
        } CSPROPERTY_MEMBERSLIST, *PCSPROPERTY_MEMBERSLIST;

        */

        while(lCounter > 0)
        {
            PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL ;
            pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >( pCSPropMemberList ) ;

            // For non-Bolean Properties like Brightness, MemberSize can not be equal to 0
            if ( 0 == pCSPropMemberHeader->MembersSize )
            {
                FAIL( TEXT( "Camera : MembersSize should not be 0 for non-Bolean values." ) ) ;            
            }

            if ( CSPROPERTY_MEMBER_FLAG_DEFAULT != pCSPropMemberHeader->Flags )
            {
                FAIL( TEXT( "Camera : Query should only have returned Default values." ) ) ;
            }

            if ( lMaxCount != pCSPropMemberHeader->MembersCount )
            {
                Log ( TEXT ( "Camera : MemberHeader->MembersCount  Expected %d, Actual %ld." ), lMaxCount, pCSPropMemberHeader->MembersCount ) ;
                FAIL( TEXT( "Camera: Incorrect number of members. " ) ) ;
            }
            // pCSPropMemberHeader+1 gives us the address of the pointer, dereference to get the pointer.
            LPVOID lpMembers = *reinterpret_cast<LPVOID *>( pCSPropMemberHeader + 1 ) ;
            
            switch ( pCSPropMemberHeader->MembersFlags )
            {
                case CSPROPERTY_MEMBER_VALUES :
                {
                    LONG lDetectedDefaultValue = (LONG) lpMembers ;

                    if(lDetectedDefaultValue > lUpperBound || 
                        lDetectedDefaultValue < lLowerBound )
                    {
                        FAIL( TEXT( "Camera: default value outside of valid range. " ) ) ;
                    }

                    if ( lDetectedDefaultValue != lDefaultValue )
                    {
                        FAIL( TEXT( "Camera : The default value is incorrect." ) ) ;
                    }
                    break ;
                }
                default :
                    FAIL( TEXT( "Camera : Invalid MemberHeader->Flags value, expected CSPROPERTY_MEMBER_VALUES" ) ) ;
                    break ;
            }

            // Jump to the next CSPROPERTY_MEMBERSLIST Item
            pCSPropMemberList++;
            lCounter-- ;
        };
    }
    else
    {
        FAIL( TEXT( "Camera : Driver does not support READ operation on CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS " ) ) ; 
    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
    {
        FAIL(TEXT("Test_IO_Preview : SetState stop failed"));
        return;
    }

    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
        return;
    }

    return;
}

void
Test_CSProperty1(GUID guidPropertySet, ULONG ulProperty, LONG lUpperBound, LONG lLowerBound, LONG lDelta, LONG lMaxCount)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    CSPROPERTY_VIDEOPROCAMP_S csPropertyVidProcAmp ;
    RECT rc;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    Log( TEXT( "Camera :  Upper bound %d, lower bound %d, Delta %d" ), lUpperBound, lLowerBound, lDelta ) ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Camera : Failed to determine camera availability." ) ) ;
        return;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Camera : InitializeDriver failed " ) ) ;
        return;
    }

    camTest.PrepareTestEnvironment(     guidPropertySet,  
                                    ulProperty,
                                    VT_I4 ) ;

    // Let's first see if the driver supports PROPSETID_VIDCAP_VIDEOPROCAMP or not

    if ( FALSE == camTest.IsPropertySetSupported( ) )
    {
        FAIL ( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOPROCAMP is not a supported property set" ) ) ;
        return;
    }

    // We are here so PROPSETID_VIDCAP_VIDEOPROCAMP is a supported property.
    // Now we'll query for the accessflags of Brightness, i.e., Read-only, R/W etc

    if ( FALSE == camTest.FetchAccessFlags ( ) )
    {
        ERRFAIL( TEXT( "Camera: FetchAccessFlags returned FALSE. " ) ) ;
        return;
    }

    if(FALSE == camTest.CreateStream(STREAM_CAPTURE, NULL, rc))
    {
        FAIL(TEXT("Camera : Creating the stream failed "));
        return;
    }

    if(0 == camTest.SelectVideoFormat(STREAM_CAPTURE, 0))
    {
        FAIL(TEXT("Test_IO_Preview : No supported formats on the pin "));
        return;
    }

    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
        return;
    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
    {
        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
        return;
    }

    /*    Now AccessFlags should contain a combination of
        CSPROPERTY_TYPE_GET
        CSPROPERTY_TYPE_SET
        CSPROPERTY_TYPE_DEFAULT
        This will later be used to decide what kind of data to expect and test to perform
    */

    if ( camTest.IsReadAllowed( ) )
    {
        // This property allows us to read it's current or set of default values. 

        // Let's read the current value first

        if ( FALSE == camTest.GetCurrentPropertyValue(    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ), 
                                                    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ) ) )
        {
            FAIL( TEXT( "Camera: GET query for CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS Failed. " ) ) ;
        }

        if (     ! ( CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL & csPropertyVidProcAmp.Flags ) &&
            ! ( CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO     & csPropertyVidProcAmp.Flags ) )
        {
            FAIL( TEXT( "Camera : Flags is not set to one of the allowed values " ) ) ;
        }
    }

    // We are here, meaning we are free to set the value of this property.
    // We'll start off by looking at the range of values supported
    
    if ( FALSE == camTest.FetchBasicSupport( ) )
    {
        ERRFAIL( TEXT( "Camera: FetchBasicSupport() returned FALSE. " ) ) ;
    }

    PCSPROPERTY_DESCRIPTION pCSPropDescription = NULL ;
    if ( FALSE == camTest.GetValueBuffer( &pCSPropDescription ) ) 
    {
        FAIL( TEXT( "Camera: GetValueBuffer() returned FALSE. " ) ) ;
    }

    // Now we are all set to traverse the property data. The data is packed in the form of
    // CSPROPERTY_DESCRIPTION 
    // CSPROPERTY_MEMBERSLIST[pCSPropDescritpion->MembersListCount]

    if ( NULL == pCSPropDescription )
    {
        FAIL( TEXT( "Camera: pCSPropDescription structure is NULL. " ) ) ;
        return;
    }

    LONG lCounter = pCSPropDescription->MembersListCount ;

    if ( lMaxCount != lCounter )
    {
        FAIL( TEXT( "Camera: incorrect member count. " ) ) ;
    }
    
    // Get to the first CSPROPERTY_MEMBERSLIST element, 
    // which is right after CSPROPERTY_DESCRIPTION structure;

    PCSPROPERTY_MEMBERSLIST pCSPropMemberList = NULL ;

    pCSPropMemberList = reinterpret_cast <PCSPROPERTY_MEMBERSLIST> ( pCSPropDescription + 1 ) ;

    if ( NULL == pCSPropMemberList )
    {
        FAIL( TEXT( "Camera : MembersList structure is NULL." ) ) ;
        return;
    }

    // Now we will be traversing this array of CSPROPERTY_MEMBERSLIST

    /*
    Format of CSPROPERYT_MEMBERLIST

    typedef struct {
        CSPROPERTY_MEMBERSHEADER    MembersHeader;
        const VOID*                 Members;
    } CSPROPERTY_MEMBERSLIST, *PCSPROPERTY_MEMBERSLIST;

    */
    
    while(lCounter > 0)
    {
        PCSPROPERTY_MEMBERSHEADER pCSPropMemberHeader = NULL ;
        pCSPropMemberHeader = reinterpret_cast< PCSPROPERTY_MEMBERSHEADER >( pCSPropMemberList ) ;

        // For non-Bolean Properties like Brightness, MemberSize can not be equal to 0
        if ( 0 == pCSPropMemberHeader->MembersSize )
        {
            FAIL( TEXT( "Camera : MembersSize should not be 0 for non-Bolean values." ) ) ;            
        }


        if ( lMaxCount != pCSPropMemberHeader->MembersCount )
        {
            Log ( TEXT ( "Camera : MemberHeader->MembersCount  Expected %d, Actual %ld." ), lMaxCount, pCSPropMemberHeader->MembersCount ) ;
            FAIL( TEXT( "Camera: incorrect member count. " ) ) ;
        }
            
        switch ( pCSPropMemberHeader->MembersFlags )
        {
            case CSPROPERTY_MEMBER_RANGES :
                {
                    PCSPROPERTY_STEPPING_LONG pCSPropSteppingLong ;
                    pCSPropSteppingLong = reinterpret_cast< PCSPROPERTY_STEPPING_LONG >( pCSPropMemberHeader + 1 ) ;

                    if ( lLowerBound != pCSPropSteppingLong->Bounds.UnsignedMinimum )
                    {
                        Log ( TEXT ( "Camera : pCSPropBoundsLong->Minimum Expected %ld, Actual %ld." ), lLowerBound, pCSPropSteppingLong->Bounds.UnsignedMinimum  ) ;
                        FAIL( TEXT( "Camera: LowerBound does not match expected value. " ) ) ;
                    }

                    if ( lUpperBound != pCSPropSteppingLong->Bounds.UnsignedMaximum )
                    {
                        Log ( TEXT ( "Camera : pCSPropBoundsLong->Maximum Expected %ld, Actual %ld." ), lUpperBound, pCSPropSteppingLong->Bounds.UnsignedMaximum  ) ;
                        FAIL( TEXT( "Camera: UpperBound does not match expected value. " ) ) ;
                    }

                    if ( lDelta != pCSPropSteppingLong->SteppingDelta )
                    {
                        Log ( TEXT ( "Camera : pCSPropBoundsLong->SteppingDelta Expected %ld, Actual %ld." ), lDelta, pCSPropSteppingLong->SteppingDelta  ) ;
                        FAIL( TEXT( "Camera: Delta does not match expected value. " ) ) ;
                    }   
                }
                break ;

            default :
                FAIL( TEXT( "Camera : Invalid MemberHeader->Flags value, should be CSPROPERTY_MEMBER_RANGES" ) ) ;
                break ;
        }

        // Jump to the next CSPROPERTY_MEMBERSLIST Item
        pCSPropMemberList++;
        lCounter-- ;
    }

    // Now that we have successfully tested the range. Let's do some boundary value testing
    LONG lValidSet[] = { 
        lLowerBound, 
        lLowerBound + lDelta, 
        lUpperBound - lDelta, 
        lUpperBound } ;
    LONG lInvalidSet[] = { 
        lLowerBound - 1, 
        lUpperBound + 1, 
        lLowerBound - lDelta,
        lUpperBound + lDelta,
        lDelta > 1 ? lLowerBound + lDelta - 1 : lLowerBound - 2, 
        lDelta > 1 ? lUpperBound - lDelta + 1 : lUpperBound + 2,
        lLowerBound > 0 ? 0 : lLowerBound - 1,
        lUpperBound < 0x7fffffff ? 0x7fffffff : lUpperBound + 2} ;

    // In some instances, the valid set is only one entry (which means lower + delta will be > upper).
    // Make sure this doesn't happen for any valid set.
    for ( int i = 0; i < dim( lValidSet ) ; i++ )
    {
        if ( lValidSet[ i ] < lLowerBound )
        {
            lValidSet[ i ] = lLowerBound;
        }
        if ( lValidSet[ i ] > lUpperBound )
        {
            lValidSet[ i] = lUpperBound;
        }
    }

    if(csPropertyVidProcAmp.Flags == CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL)
    {
        for ( int iCount = 0 ; iCount < dim( lInvalidSet ) ; iCount++ )
        {
            Log( TEXT( "Camera : value being set = %ld" ), lInvalidSet[ iCount ] ) ;
            csPropertyVidProcAmp.Value    = lInvalidSet[ iCount ] ;
            if ( TRUE == camTest.SetPropertyValue(    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ), 
                                                &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ) ) )
            {
                FAIL( TEXT( "Camera : The driver should not have set this illegal value " ) ) ;
            }

            // small delay for the property to take effect on the stream
            Sleep(200);
        }
    }
    else if(csPropertyVidProcAmp.Flags == CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO)
    {
        for ( int iCount = 0 ; iCount < dim( lInvalidSet ) ; iCount++ )
        {
            Log( TEXT( "Camera : value being set = %ld" ), lInvalidSet[ iCount ] ) ;
            csPropertyVidProcAmp.Value    = lInvalidSet[ iCount ] ;
            if ( FALSE == camTest.SetPropertyValue(    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ), 
                                                &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ) ) )
            {
                FAIL( TEXT( "Camera : The driver should not have set this illegal value " ) ) ;
            }
            // small delay for the property to take effect on the stream
            Sleep(200);
        }
    }

    for ( int iCount = 0 ; iCount < dim( lValidSet ) ; iCount++ )
    {
        Log( TEXT( "Camera : value being set = %ld" ), lValidSet[ iCount ] ) ;

        csPropertyVidProcAmp.Value    = lValidSet[ iCount ] ;
        if ( FALSE == camTest.SetPropertyValue(    &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ), 
                                            &csPropertyVidProcAmp, sizeof( csPropertyVidProcAmp ) ) )
        {
            ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
        }
            // small delay for the property to take effect on the stream
            Sleep(200);
    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
    {
        FAIL(TEXT("Test_IO_Preview : SetState stop failed"));
        return;
    }

    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
        return;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_Brightness
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_Brightness
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_Brightness( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -10000 || lUpperBound != 10000 || lDefaultValue != 750)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_BRIGHTNESS_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_Brightness1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_Brightness1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_Brightness1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -10000 || lUpperBound != 10000 || lDefaultValue != 750)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_BRIGHTNESS_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}



////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_Contrast
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_Contrast
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_CONTRAST. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_Contrast( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_CONTRAST, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != 0 || lUpperBound != 10000 || lDefaultValue != 100)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_CONTRAST, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_CONTRAST_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_Contrast1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_Contrast1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_Contrast. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_Contrast1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_CONTRAST, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != 0 || lUpperBound != 10000 || lDefaultValue != 100)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_CONTRAST, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_CONTRAST_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}



////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_GAMMA
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_GAMMA
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_GAMMA. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_GAMMA( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_GAMMA, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != 1 || lUpperBound != 500 || !(lDefaultValue == 100 || lDefaultValue == 220))
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_GAMMA, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_GAMMA_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_GAMMA1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_GAMMA1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_GAMMA. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_GAMMA1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_GAMMA, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 1 || lUpperBound != 500 || !(lDefaultValue == 100 || lDefaultValue == 220))
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_GAMMA, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_GAMMA_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_SATURATION
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_SATURATION
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_SATURATION. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_SATURATION( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_SATURATION, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 10000 || lDefaultValue != 100)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_SATURATION, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_SATURATION_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_SATURATION1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_SATURATION1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_SATURATION. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_SATURATION1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_SATURATION, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 10000 || lDefaultValue != 100)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_SATURATION, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_SATURATION_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_SHARPNESS
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_SHARPNESS
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_SHARPNESS. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_SHARPNESS( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_SHARPNESS, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 100 || lDefaultValue != 50)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_SHARPNESS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_SHARPNESS_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_SHARPNESS1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_SHARPNESS1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_SHARPNESS. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_SHARPNESS1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_SHARPNESS, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 100 || lDefaultValue != 50)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_SHARPNESS, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_SHARPNESS_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_WHITEBALANCE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_WHITEBALANCE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_WHITEBALANCE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_WHITEBALANCE_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_WHITEBALANCE1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_WHITEBALANCE1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_WHITEBALANCE1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_WHITEBALANCE_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_COLORENABLE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_COLORENABLE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_COLORENABLE. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_COLORENABLE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_COLORENABLE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 1 || lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_COLORENABLE, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_COLORENABLE_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_COLORENABLE1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_COLORENABLE1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_COLORENABLE. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_COLORENABLE1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_COLORENABLE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 1 || lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_COLORENABLE, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_COLORENABLE_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 1 || lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_BACKLIGHT_COMPENSATION_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != 0 || lUpperBound != 1 || lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_BACKLIGHT_COMPENSATION_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}



////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_GAIN
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_GAIN
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_GAIN( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_GAIN, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_GAIN, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_GAIN_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_GAIN1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_GAIN1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_GAIN. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_GAIN1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_GAIN, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lDefaultValue != 1)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_GAIN, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_GAIN_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_HUE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_HUE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_HUE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_HUE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != -18000 || lUpperBound != 18000 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_HUE, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_HUE_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VideoProcAmp_HUE1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VideoProcAmp_HUE1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_VIDEOPROCAMP_HUE. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VideoProcAmp_HUE1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveVidProcAmpParams(PROPSETID_VIDCAP_VIDEOPROCAMP, 
                                                        CSPROPERTY_VIDEOPROCAMP_HUE, 
                                                        &lUpperBound, 
                                                        &lLowerBound, 
                                                        &lDefaultValue, 
                                                        &lDelta))
    {

        if(lLowerBound != -18000 || lUpperBound != 18000 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_CSProperty1(PROPSETID_VIDCAP_VIDEOPROCAMP,
                                    CSPROPERTY_VIDEOPROCAMP_HUE, 
                                    lUpperBound, 
                                    lLowerBound,
                                    lDelta,
                                    MAX_RANGE_HUE_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


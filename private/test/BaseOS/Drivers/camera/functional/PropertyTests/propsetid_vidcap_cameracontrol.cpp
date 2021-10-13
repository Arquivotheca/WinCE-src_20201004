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
//          PROPSETID_VIDCAP_CAMERACONTROL
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
RetrieveParams(GUID guidPropertySet, ULONG ulProperty, LONG *lUpperBound, LONG *lLowerBound, LONG *lDefaultValue, LONG *lDelta)
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
        SKIP( TEXT( "Camera: FetchAccessFlags returned FALSE, property unsupported. " ) ) ;
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
Test_VidCap(GUID guidPropertySet, ULONG ulProperty, LONG lUpperBound, LONG lLowerBound, LONG lMaxCount, LONG lDefaultValue)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    RECT rc;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    Log( TEXT( "Camera :  Default value is %d, upper bound %d, lower bound %d" ), lDefaultValue, lUpperBound, lLowerBound ) ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Camera: Failed to determine camera availability." ) ) ;
        return;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Camera: InitializeDriver failed " ) ) ;
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
        CSPROPERTY_CAMERACONTROL_S csPropertyCameraControl ;
        if ( FALSE == camTest.GetCurrentPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
        {
            FAIL( TEXT( "Camera: GET query for the property failed. " ) ) ;
            return;
        }

        Log( TEXT( "Camera :  Current Flags is %#x" ), csPropertyCameraControl.Flags ) ;
        Log( TEXT( "Camera :  Current Capabilities is %#x" ), csPropertyCameraControl.Capabilities ) ;
        Log( TEXT( "Camera :  Current value is %u" ), csPropertyCameraControl.Value ) ;

        if(!csPropertyCameraControl.Flags)
        {
            FAIL( TEXT( "Camera :  Current Flag is not set; must be valid flag from Capabilities" ) ) ;
        }
        if(csPropertyCameraControl.Flags & (csPropertyCameraControl.Flags - 1))
        {
            FAIL( TEXT( "Camera :  Current Flag has more than one flag set; only one is allowed" ) ) ;
        }
        if(!(csPropertyCameraControl.Flags & csPropertyCameraControl.Capabilities))
        {
            FAIL( TEXT( "Camera :  Current Flag is not included in Capabilities; must be valid flag from Capabilities" ) ) ;
        }
        if(csPropertyCameraControl.Value > lUpperBound || 
            csPropertyCameraControl.Value < lLowerBound)
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
Test_VidCap1(GUID guidPropertySet, ULONG ulProperty, LONG lUpperBound, LONG lLowerBound, LONG lDelta, LONG lMaxCount)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    CSPROPERTY_CAMERACONTROL_S csPropertyCameraControl ;
    ULONG ulCamControlCaps = 0;
    RECT rc;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    Log( TEXT( "Camera :  Upper bound %d, lower bound %d, Delta %d" ), lUpperBound, lLowerBound, lDelta ) ;

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

        if ( FALSE == camTest.GetCurrentPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
        {
            FAIL( TEXT( "Camera: GET query for CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS Failed. " ) ) ;
        }

        ulCamControlCaps = csPropertyCameraControl.Capabilities;

        if (! ( CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL            & csPropertyCameraControl.Capabilities ) &&
            ! ( CSPROPERTY_CAMERACONTROL_FLAGS_AUTO              & csPropertyCameraControl.Capabilities ) &&
            ! ( CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO & csPropertyCameraControl.Capabilities ) )
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

    if(ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
    {
        csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
        for ( int iCount = 0 ; iCount < dim( lInvalidSet ) ; iCount++ )
        {
            Log( TEXT( "Camera : value being set = %ld" ), lInvalidSet[ iCount ] ) ;
            csPropertyCameraControl.Value    = lInvalidSet[ iCount ] ;
            if ( TRUE == camTest.SetPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                                &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
            {
                FAIL( TEXT( "Camera : The driver should not have set this illegal value " ) ) ;
            }
            Sleep(200);
        }
    }
    else if(ulCamControlCaps == CSPROPERTY_CAMERACONTROL_FLAGS_AUTO)
    {
        csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
        for ( int iCount = 0 ; iCount < dim( lInvalidSet ) ; iCount++ )
        {
            Log( TEXT( "Camera : value being set = %ld" ), lInvalidSet[ iCount ] ) ;
            csPropertyCameraControl.Value    = lInvalidSet[ iCount ] ;
            if ( FALSE == camTest.SetPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                                &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
            {
                FAIL( TEXT( "Camera : The driver should not have set this illegal value " ) ) ;
            }
            Sleep(200);
        }
    }
    
    for ( int iCount = 0 ; iCount < dim( lValidSet ) ; iCount++ )
    {
        Log( TEXT( "Camera : value being set = %ld" ), lValidSet[ iCount ] ) ;

        csPropertyCameraControl.Value    = lValidSet[ iCount ] ;
        if ( FALSE == camTest.SetPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                            &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
        {
            ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
        }
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

BOOL
WaitForNotification(CCameraDriverTest & camTest, ULONG ulNotificationFlag, ULONG ulExpectedData)
{
    LONG lTimeoutWait = 0;
    // This wait timeout matches the DShow requirements for a hard failure for async focus.
    const int NOTIF_TIMEOUT = 30000;
    const int NOTIF_INCREMENT = 500;
    BOOL bEventReceived = FALSE;
    NOTIFICATIONDATA ndNotifData;
    Log( TEXT( "Camera : Waiting for Async property Notification from Adapter" ) );
    while ( lTimeoutWait < NOTIF_TIMEOUT && !bEventReceived )
    {
        Sleep ( NOTIF_INCREMENT );
        lTimeoutWait += NOTIF_INCREMENT;
        bEventReceived = camTest.GetNextReceivedNotification (
            STREAM_ADAPTER,
            ulNotificationFlag, 
            &ndNotifData );
    }
    
    // verify the async stopped command is returned
    if (!bEventReceived)
    {
        FAIL( TEXT( "Camera : Did not receive any notification after cancelling async property" ) );
        Log( TEXT( "Camera : notification was not received within %d milliseconds" ), lTimeoutWait );
        return FALSE;
    }
    else if (ndNotifData.Context.Data != ulExpectedData)
    {
        if (ulExpectedData == 1)
        {
            FAIL( TEXT( "Camera : Received async completion notification instead of cancelled notification after cancelling async property" ) );
        }
        else
        {
            FAIL( TEXT( "Camera : Received async cancelled notification instead of completed notification after successfully setting property" ) );
        }
    }
    else
    {
        Log( TEXT( "Camera : Successfully received async property cancellation notification" ) );
    }
    return TRUE;
}
void
Test_VidCapAsync(
    GUID guidPropertySet, 
    ULONG ulProperty, 
    ULONG ulNotificationFlag, 
    ULONG ulStartCommand,
    ULONG ulStopCommand,
    LONG lUpperBound, 
    LONG lLowerBound, 
    LONG lDelta, 
    LONG lMaxCount)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    CSPROPERTY_CAMERACONTROL_S csPropertyCameraControl ;
    NOTIFICATIONDATA ndNotifData ;
    ULONG ulCamControlCaps = 0 ;
    RECT rc;

    memset( &csPropertyCameraControl, 0x00, sizeof(csPropertyCameraControl) );

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    Log( TEXT( "Camera : Testing ASync functionality if supported" ) ) ;

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
        FAIL(TEXT("Camera : No supported formats on the pin "));
        return;
    }

    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Camera : Creating the stream failed "));
        return;
    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
    {
        FAIL(TEXT("Camera : SetState run failed"));
        return;
    }

    if(FALSE == camTest.CreateStream(STREAM_STILL, NULL, rc))
    {
        FAIL(TEXT("Camera : Creating the stream failed "));
        return;
    }

    if(0 == camTest.SelectVideoFormat(STREAM_STILL, 0))
    {
        FAIL(TEXT("Camera : No supported formats on the pin "));
        return;
    }

    if(FALSE == camTest.SetupStream(STREAM_STILL))
    {
        FAIL(TEXT("Camera : Creating the stream failed "));
        return;
    }

    /*    Now AccessFlags should contain a combination of
        CSPROPERTY_TYPE_GET
        CSPROPERTY_TYPE_SET
        CSPROPERTY_TYPE_DEFAULT
        This will later be used to decide what kind of data to expect and test to perform
    */

    // Retreive the capabilities of the driver.
    if ( camTest.IsReadAllowed( ) )
    {
        // This property allows us to read it's current or set of default values. 

        // Let's read the current value first

        if ( FALSE == camTest.GetCurrentPropertyValue(    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                                                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
        {
            FAIL( TEXT( "Camera: GET query for CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS Failed. " ) ) ;
        }

        ulCamControlCaps = csPropertyCameraControl.Capabilities;

        if (! ( CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL            & csPropertyCameraControl.Capabilities ) &&
            ! ( CSPROPERTY_CAMERACONTROL_FLAGS_AUTO              & csPropertyCameraControl.Capabilities ) &&
            ! ( CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO & csPropertyCameraControl.Capabilities ) )
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
    
    // If Async_auto is not supported for this property, skip
    if (! ( CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO & ulCamControlCaps) )
    {
        SKIP( TEXT( "Camera: Asynchronous Auto is not supported for this property." ) );

        // We need to also make sure the stream is stopped, since otherwise the
        // test will have a false failure
        if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
        {
            FAIL(TEXT("Camera : SetState stop on capture stream failed"));
        }

        if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
        {
            FAIL(TEXT("Camera : Cleaning up the capture stream failed "));
        }

        if(FALSE == camTest.CleanupStream(STREAM_STILL))
        {
            FAIL(TEXT("Camera : Cleaning up the still stream failed"));
        }
        return;
    }

    // These are our starting points for the async command
    LONG lValidSet[] = { 
        lLowerBound, 
        lUpperBound } ;

    CSSTATE csStates[] = {
        CSSTATE_STOP,
        CSSTATE_PAUSE,
        CSSTATE_RUN } ;

    // Verify that the camera driver behaves consistently while the pins are stopped, paused, and run.
    // For conistency across properties, the driver should succeed the call and return the event regardless
    // of pin state.
    for (int i = 0; i < _countof(csStates); ++i)
    {

        // Put pins into state under test
        if(FALSE == camTest.SetState(STREAM_CAPTURE, csStates[i]))
        {
            FAIL(TEXT("Camera : SetState failed"));
            return;
        }
        Sleep(100);

        // verify that cancellation while not performing a focus fails
        csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
        csPropertyCameraControl.Value = ulStopCommand;
        Log( TEXT( "Camera : Setting async property cancel without first setting property; this should fail and not return a notification" ) );
        if ( TRUE == camTest.SetPropertyValue(    
                &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
        {
            FAIL( TEXT( "Camera : The driver succeeded with a cancellation when property wasn't set" ) ) ;
            WaitForNotification(camTest, ulNotificationFlag, 1);
        }

        // Verify that setting the async property in normal circumstances succeeds and returns
        // a proper notification.
        Sleep(100);
        for (int i = 0; i < _countof(lValidSet); ++i)
        {
            // for each starting point
            if (ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
            {
                Log( TEXT( "Camera : Using Manual to start at property value %d"), lValidSet[i] ) ;
                csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
                csPropertyCameraControl.Value = lValidSet[i];

                if ( FALSE == camTest.SetPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
                }
                Sleep(200);
            }
            // call async start command
            csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
            csPropertyCameraControl.Value = ulStartCommand;
            Log( TEXT( "Camera : Setting async property; this should succeed and return a notification" ) );
            // Should succeed
            if ( FALSE == camTest.SetPropertyValue(    
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
            {
                ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
            }
            else
            {
                // wait for complete event to be returned
                WaitForNotification(camTest, ulNotificationFlag, 0);
            }

            // verify that the focus property has changed
            if ( camTest.IsReadAllowed( ) )
            {
                if ( FALSE == camTest.GetCurrentPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    FAIL( TEXT( "Camera: GET query for property Failed. " ) ) ;
                }

                if ( csPropertyCameraControl.Value == lValidSet[i] )
                {
                    WARN( TEXT( "Camera : After setting async property, value of property has not changed" ) );
                }
            }
        }

        // Verify that setting the async property and then cancelling it behaves correctly
        Sleep(100);
        for (int i = 0; i < _countof(lValidSet); ++i)
        {
            // for each starting point
            if (ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
            {
                Log( TEXT( "Camera : Using Manual to start at property value %d"), lValidSet[i] ) ;
                csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
                csPropertyCameraControl.Value = lValidSet[i];

                if ( FALSE == camTest.SetPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
                }
                Sleep(200);
            }
            // call async start command
            csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
            csPropertyCameraControl.Value = ulStartCommand;
            Log( TEXT( "Camera : Setting async property; this should succeed and return a notification" ) );
            // Should succeed
            if ( FALSE == camTest.SetPropertyValue(    
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
            {
                ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
            }
            else
            {
                csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
                csPropertyCameraControl.Value = ulStopCommand;
                // immediately call async stop command: as long as the async set doesn't succeed immediately this should succeed.
                Log( TEXT( "Camera : Cancelling async property; this should succeed; If not, success notification must have been sent" ) );
                if ( FALSE == camTest.SetPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    // verify that the async complete command is returned
                    if (!camTest.GetNextReceivedNotification (
                            STREAM_ADAPTER,
                            ulNotificationFlag, 
                            &ndNotifData ))
                    {
                        FAIL( TEXT( "Camera : Cancelling the async property failed, but no success notification was received" ) );
                    }
                    else if ( ndNotifData.Context.Data != 0 )
                    {
                        FAIL( TEXT( "Camera : Canceling the async property failed, but a cancellation notification was received" ) );
                    }
                }
                else
                {
                    // verify the async complete command is not returned
                    WaitForNotification(camTest, ulNotificationFlag, 1);
                }
            }
        }

        // Verify that setting the async property and then immediately setting the
        // property to a manual value causes the async cancel to be returned.
        if (ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
        {
            Sleep(100);
            for (int i = 0; i < _countof(lValidSet); ++i)
            {
                // for each starting point
                if (ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
                {
                    Log( TEXT( "Camera : Using Manual to start at property value %d"), lValidSet[i] ) ;
                    csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
                    csPropertyCameraControl.Value = lValidSet[i];

                    if ( FALSE == camTest.SetPropertyValue(    
                            &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                            &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                    {
                        ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
                    }
                    Sleep(200);
                }
                // call async start command
                csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
                csPropertyCameraControl.Value = ulStartCommand;
                Log( TEXT( "Camera : Setting async property; this should succeed and return a notification" ) );
                // Should succeed
                if ( FALSE == camTest.SetPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
                }
                else
                {
                    csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
                    csPropertyCameraControl.Value = lValidSet[(i + 1) % _countof(lValidSet)];
                    // immediately set manual focus
                    Log( TEXT( "Camera : Setting manual property immediately after setting async auto property: should succeed and a cancelled notification should be received." ) );
                    if ( FALSE == camTest.SetPropertyValue(    
                            &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                            &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                    {
                        ERRFAIL( TEXT( "Camera : Setting manual property failed" ) );
                    }
                    else
                    {
                        // verify the async complete command is not returned
                        WaitForNotification(camTest, ulNotificationFlag, 1);
                    }
                }
            }
        }

        // Verify that taking a still capture while the async property is being set causes
        // the property to cancel.
        Sleep(100);
        for (int i = 0; i < _countof(lValidSet); ++i)
        {
            // for each starting point
            if (ulCamControlCaps & CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL)
            {
                Log( TEXT( "Camera : Using Manual to start at property value %d"), lValidSet[i] ) ;
                csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
                csPropertyCameraControl.Value = lValidSet[i];

                if ( FALSE == camTest.SetPropertyValue(    
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                        &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
                {
                    ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
                }
                Sleep(200);
            }
            // call async start command
            csPropertyCameraControl.Flags = CSPROPERTY_CAMERACONTROL_FLAGS_ASYNCHRONOUS_AUTO;
            csPropertyCameraControl.Value = ulStartCommand;
            Log( TEXT( "Camera : Setting async property; this should succeed and return a notification" ) );
            // Should succeed
            if ( FALSE == camTest.SetPropertyValue(    
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ), 
                    &csPropertyCameraControl, sizeof( csPropertyCameraControl ) ) )
            {
                ERRFAIL( TEXT( "Camera : The driver should have set this legal value " ) ) ;
            }
            else
            {
                Log( TEXT( "Camera : Triggering still capture while driver is running; Should either immediately return cancel or should return completion when property finishes" ) );
                camTest.TriggerCaptureEvent(STREAM_STILL);
                if (camTest.GetNextReceivedNotification(STREAM_ADAPTER, ulNotificationFlag, &ndNotifData))
                {
                    Log(TEXT( "Camera : Received %s notification after triggering still image while changing async property" ),
                        ndNotifData.Context.Data ? TEXT("complete") : TEXT("cancel"));
                }
                else
                {
                    Log(TEXT( "Camera : No notification immediately received, waiting for success notification" ) );
                    WaitForNotification(camTest, ulNotificationFlag, 0);
                }
            }
        }

    }

    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
    {
        FAIL(TEXT("Camera : SetState stop on capture stream failed"));
    }

    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Camera : Cleaning up the capture stream failed "));
    }

    if(FALSE == camTest.CleanupStream(STREAM_STILL))
    {
        FAIL(TEXT("Camera : Cleaning up the still stream failed"));
    }

}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_EXPOSURE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_EXPOSURE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_EXPOSURE. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_EXPOSURE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_EXPOSURE, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_EXPOSURE, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_EXPOSURE_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_EXPOSURE1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_EXPOSURE1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_EXPOSURE. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_EXPOSURE1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_EXPOSURE, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_EXPOSURE, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_EXPOSURE_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}



////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_FOCUS
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_FOCUS
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_FOCUS. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_FOCUS( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_FOCUS, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_FOCUS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_FOCUS_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_FOCUS1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_FOCUS1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_FOCUS. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_FOCUS1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_FOCUS, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_FOCUS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_FOCUS_COUNT); 
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_IRIS
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_IRIS
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_IRIS. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_IRIS( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_IRIS, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_IRIS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_IRIS_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_IRIS1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_IRIS1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_IRIS. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_IRIS1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_IRIS, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_IRIS, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_IRIS_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_ZOOM
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_ZOOM
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_ZOOM. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_ZOOM( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_ZOOM, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != 10 || lUpperBound != 600)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_ZOOM, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_ZOOM_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_ZOOM1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_ZOOM1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_ZOOM. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_ZOOM1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_ZOOM, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != 10 || lUpperBound != 600)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_ZOOM, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_ZOOM_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_PAN
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_PAN
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_PAN. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_PAN( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_PAN, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_PAN, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_PAN_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_PAN1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_PAN1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_PAN. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_PAN1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_PAN, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_PAN, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_PAN_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_TILT
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_TILT
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_TILT. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_TILT( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_TILT, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_TILT, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_TILT_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_TILT1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_TILT1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_TILT. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_TILT1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_TILT, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_TILT, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_TILT_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_ROLL
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_ROLL
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_ROLL. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_ROLL( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_ROLL, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_ROLL, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_ROLL_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_ROLL1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_ROLL1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_ROLL. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_ROLL1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_ROLL, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        //Fail if the properties are not valid
        if(lLowerBound != -180 || lUpperBound != 180 || lDefaultValue != 0)
        {
            FAIL( TEXT( "Camera : Properties are invalid." ) ) ;
            return TPR_FAIL;
        }

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_ROLL, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_ROLL_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_FLASH
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_FLASH
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_CAMERACONTROL_FLASH. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_FLASH( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_FLASH, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_FLASH, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    MAX_DEFAULT_ROLL_COUNT,
                                    lDefaultValue);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_FLASH1
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_FLASH1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_FLASH. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_FLASH1( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_FLASH, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {

        Test_VidCap1(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_FLASH, 
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_ROLL_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VidCap_CameraControl_FOCUS_ASYNC
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VidCap_CameraControl_FOCUS1
//
//  Assertion:        
//
//  Description:    2: Test CSPROPERTY_CAMERACONTROL_FOCUS. Try to get the range and set to various values
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VidCap_CameraControl_FOCUS_ASYNC( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    LONG lUpperBound, lLowerBound, lDefaultValue, lDelta;

    if(RetrieveParams(PROPSETID_VIDCAP_CAMERACONTROL, 
                                CSPROPERTY_CAMERACONTROL_FOCUS, 
                                &lUpperBound, 
                                &lLowerBound, 
                                &lDefaultValue, 
                                &lDelta))
    {
        Test_VidCapAsync(PROPSETID_VIDCAP_CAMERACONTROL,
                                    CSPROPERTY_CAMERACONTROL_FOCUS,
                                    FLAG_MSGQ_ASYNCHRONOUS_FOCUS,
                                    CS_CAMCONTROL_AUTO_FOCUS_START,
                                    CS_CAMCONTROL_AUTO_FOCUS_STOP,
                                    lUpperBound, 
                                    lLowerBound, 
                                    lDelta,
                                    MAX_RANGE_ROLL_COUNT);
    }

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

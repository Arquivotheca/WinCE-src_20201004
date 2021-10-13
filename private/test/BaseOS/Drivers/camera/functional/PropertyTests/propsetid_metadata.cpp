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

TESTPROCAPI Test_CSProperty_MetadataAll( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest;
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    DWORD dwSizeIn = 0;
    ULONG ulSizeOut = 0;
    PCSMETADATA_S pBuffer = NULL;
    ULONG Id = 0;
    ULONG Flags = 0;
    DWORD dwExpectedSizeInBytes = 0;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL(TEXT("Camera: Failed to determine camera availability."));
        retval = TPR_FAIL;
        goto Cleanup;
    }

    if(FALSE == camTest.InitializeDriver())
    {
        FAIL(TEXT( "Camera : InitializeDriver failed "));
        retval = TPR_FAIL;
        goto Cleanup;
    }

    dwSizeIn = 0;
    // first, standard bad parameter tests. It's going to succeed if both pointers are null
    // because the device manager succeeds the call, the driver has no control of it.
    if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, NULL, NULL)))
    {
        Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata with a NULL data pointer and NULL size pointer failed. 0x%08x"), hr);
        retval = TPR_FAIL;
    }

    // if the retrieve size is 0, then the pointer param is irrelevent also, but give it some space just in case it writes something there.
    if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, (PCSMETADATA_S) &dwExpectedSizeInBytes)))
    {
        Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata with a 0 size buffer succeeded. 0x%08x"), hr);
        retval = TPR_FAIL;
    }
    // ulSizeOut will be filled in with the size of the buffer even though the call fails.


    // now to the real tests, retrieve the size of the buffer needed.
    if(FAILED(hr =  camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, NULL)) || 0 == ulSizeOut)
    {
        Log(TEXT("***Test_CSProperty_MetadataAll: retrieved the expected metadata size failed, feature not supported on the capture driver?"));
    }
    else
    {
        dwExpectedSizeInBytes = ulSizeOut;

        // allocate a full buffer, plus a bit. Put some check bits before the pointer passed to dshow, 
        // after, and in the middle.
        pBuffer = (PCSMETADATA_S) new BYTE[dwExpectedSizeInBytes + 2*sizeof(DWORD)];
        memset(pBuffer, 0, dwExpectedSizeInBytes + 2*sizeof(DWORD));
        pBuffer = (PCSMETADATA_S) (((DWORD *) pBuffer) + 1);
        *(((DWORD *) pBuffer) -1) = 0xDDDDDDDD;
        // 0 to dwExpectedSizeInBytes - 1, so the sentinal is at dwExpectedSizeInBytes exactly.
        *((DWORD *)(((BYTE *) pBuffer) +dwExpectedSizeInBytes)) = 0xDDDDDDDD;



        // verify that setting the metadata fails, only Get should be supported.
        dwSizeIn = dwExpectedSizeInBytes;
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_SET)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: setting the metadata succeeded. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, CSPROPERTY_METADATA_ALL, 3)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: an invalid CSPROPERTY_TYPE succeeded, tried with a flag of 3 when only CSPROPERTY_TYPE_GET should be supported. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_SETSUPPORT)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: CSPROPERTY_TYPE_SETSUPPORT failed, metadata should be reported as supported. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        ULONG ulOutBuf;
        dwSizeIn = sizeof(ULONG);
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, NULL, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_BASICSUPPORT)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: BASICSUPPORT succeeded when given a NULL buffer pointer. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        // this will succeed because the device manager doesn't pass the output buffer size pointer down to the driver, so the driver
        // has no way to distinguish this call from a valid call.
        if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, NULL, (PCSMETADATA_S) &ulOutBuf, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_BASICSUPPORT)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: BASICSUPPORT failed when given a NULL output size pointer, should succeed but do nothing. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) != (hr = camTest.GetDriverMetadata(0, &ulSizeOut, (PCSMETADATA_S) &ulOutBuf, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_BASICSUPPORT)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: BASICSUPPORT succeeded when told to fill 0 bytes. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, (PCSMETADATA_S) &ulOutBuf, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_BASICSUPPORT)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: BASICSUPPORT query failed when expected to succeed. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(ulOutBuf != CSPROPERTY_TYPE_GET)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: support reported was not CSPROPERTY_TYPE_GET. 0x%08x"), ulOutBuf);
            retval = TPR_FAIL;
        }

        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, CSPROPERTY_METADATA_ALL, CSPROPERTY_TYPE_DEFAULTVALUES)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: DEFAULTVALUE request succeeded. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        // check that it validates the ID as CSPROPERTY_METADATA_ALL, any other value should fail. CSPROPERTY_METADATA_ALL is defined to be 0.
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, 1)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata with an invalid CSPROPERTY operation ID succeeded, tried with an ID of 1, but only CSPROPERTY_METADATA_ALL (0) should be supported. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer, -1)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata with an invalid CSPROPERTY operation ID succeeded, tried with an ID of -1, but only CSPROPERTY_METADATA_ALL (0) should be supported. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        //////////////////////////////////////////
        // check that it fails retrieving 1 byte
        dwSizeIn = 1;
        *(((DWORD *) pBuffer) + dwSizeIn) = 0xDDDDDDDD;
        
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving 1 byte of the metadata succeeded. 0x%08x"), hr);
            retval = TPR_FAIL;
        }
        
        // check the check bits.
        if(*(((DWORD *) pBuffer) + dwSizeIn) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving 1 byte of data modified the memory buffer when it shouldn't have"));
            retval = TPR_FAIL;
        }

        if(*(((DWORD *) pBuffer) -1) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving 1 byte of data caused a buffer underrun"));
            retval = TPR_FAIL;
        }

        if(*((DWORD *)(((BYTE *) pBuffer) + dwExpectedSizeInBytes)) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving 1 byte of data caused a buffer overrun"));
            retval = TPR_FAIL;
        }


        //////////////////////////////////////////////////////////////
        // check bits in the middle for the retrieve half way test
        dwSizeIn = ( dwExpectedSizeInBytes * sizeof( BYTE ) / sizeof( DWORD ) ) / 2;
        *(((DWORD *) pBuffer) + dwSizeIn) = 0xDDDDDDDD;
        
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving half of the metadata succeeded. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        // check the check bits.
        if(*(((DWORD *) pBuffer) + dwSizeIn) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving half of the metadata modified the memory buffer"));
            retval = TPR_FAIL;
        }

        if(*(((DWORD *) pBuffer) -1) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving half of the metadata caused a buffer underrun"));
            retval = TPR_FAIL;
        }

        if(*((DWORD *)(((BYTE *) pBuffer) + dwExpectedSizeInBytes)) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving half of the metadata caused a buffer overrun"));
            retval = TPR_FAIL;
        }

        /////////////////////////////////////////////////////////////////////
        // check that it fails if retrieving 1 less than the expected size
        dwSizeIn = ( dwExpectedSizeInBytes * sizeof ( BYTE ) / sizeof( DWORD ) ) - 1;
        *(((DWORD *) pBuffer) + dwSizeIn) = 0xDDDDDDDD;
        
        if(HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER) != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving all but 1 byte of metadata succeeded. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        // check the check bits.
        if(*(((DWORD *) pBuffer) + dwSizeIn) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving all but 1 byte of metadata modified the memory buffer"));
            retval = TPR_FAIL;
        }

        if(*(((DWORD *) pBuffer) -1) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving all but 1 byte of metadata caused a buffer underrun"));
            retval = TPR_FAIL;
        }

        if(*((DWORD *)(((BYTE *) pBuffer) + dwExpectedSizeInBytes)) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving all but 1 byte of metadata caused a buffer overrun"));
            retval = TPR_FAIL;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // Now retrieve everything, make sure it doesn't overrun or underrun the buffer
        dwSizeIn = dwExpectedSizeInBytes;
        memset(pBuffer, 0, dwExpectedSizeInBytes);
        pBuffer->ulCount = ULONG_MAX;
        if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata failed when expected to succeed. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(ulSizeOut != dwExpectedSizeInBytes)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: size of returned property items (%d) doesn't match original reported size (%d)"), ulSizeOut, dwExpectedSizeInBytes);
            retval = TPR_FAIL;
        }

        if(*(((DWORD *) pBuffer) -1) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata caused a buffer underrun"));
            retval = TPR_FAIL;
        }

        if(*((DWORD *)(((BYTE *) pBuffer) + dwExpectedSizeInBytes)) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata caused a buffer overrun"));
            retval = TPR_FAIL;
        }

        if(ulSizeOut < pBuffer->ulCount * sizeof(CS_PROPERTYITEM) + sizeof(ULONG))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieved item count invalid or size incorrect, buffer size %d is smaller than count * property item size + sizeof 1 dword (%d)"), ulSizeOut, pBuffer->ulCount * sizeof(CS_PROPERTYITEM) + sizeof(ULONG));
            retval = TPR_FAIL;
        }

        //////////////////////////////////////////////////////////////////////////////////////
        // tell dshow the buffer is bigger than it needs (it really isn't though), confirm
        // that it doesn't overrun or underrun the buffer.
        dwSizeIn = dwExpectedSizeInBytes + 10;
        memset(pBuffer, 0, dwExpectedSizeInBytes);
        pBuffer->ulCount = MAXDWORD;
        if(S_OK != (hr = camTest.GetDriverMetadata(dwSizeIn, &ulSizeOut, pBuffer)))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata failed when expected to succeed. 0x%08x"), hr);
            retval = TPR_FAIL;
        }

        if(ulSizeOut != dwExpectedSizeInBytes)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: size of returned property items (%d) doesn't match original reported size (%d)"), ulSizeOut, dwExpectedSizeInBytes);
            retval = TPR_FAIL;
        }

        // check the leading and trailing check bits.
        if(*(((DWORD *) pBuffer) -1) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata caused a buffer underrun"));
            retval = TPR_FAIL;
        }

        if(*((DWORD *)(((BYTE *) pBuffer) + dwExpectedSizeInBytes)) != 0xDDDDDDDD)
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieving the metadata caused a buffer overrun"));
            retval = TPR_FAIL;
        }

        if(ulSizeOut < pBuffer->ulCount * sizeof(CS_PROPERTYITEM) + sizeof(ULONG))
        {
            Log(TEXT("***Test_CSProperty_MetadataAll: retrieved item count invalid or size incorrect, buffer size %d is smaller than count * property item size + sizeof 1 dword (%d)"), ulSizeOut, pBuffer->ulCount * sizeof(CS_PROPERTYITEM) + sizeof(ULONG));
            retval = TPR_FAIL;
        }

        pBuffer = (PCSMETADATA_S) (((DWORD *) pBuffer) -1);
        delete[] pBuffer;
    }

Cleanup:
    return retval;
}




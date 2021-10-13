//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#define INITGUID

#include <winsock2.h>
#include <ws2bth.h>
#include <bt_sdp.h>
#include <bt_api.h>
#include <coguid.h>
#include <tux.h>
#include "SDPUtil.h"
#include "util.h"

/*--------------------------------------------------------------------------------------------------------------------*/
//
//TestId        : 1.1.2
//Title         : Enumerate Device Test
//Description   : Master try to enumerate in range devices
//Expected		:	
//					Success: at least the peer device is discovered
//					Fail:	the peer device is not discovered
//
TESTPROCAPI SDP_f1_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	BTH_DEVICE_INFO	btDevices[10];
	DWORD		dwCount = 10;
	DWORD		i;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	if (g_bIsServer)
	{
		retcode = (int) SDPUtilInquery(btDevices, &dwCount, LUP_RETURN_NAME | LUP_RETURN_ADDR);
		if (retcode == 0)
		{
			retcode = -1;
			// check if peer is enumerated
			for (i = 0; i < dwCount; i ++)
			{
				if (memcmp(&(btDevices[i].address), &(g_RemoteRadioAddr), sizeof(g_RemoteRadioAddr)) == 0)
				{
					retcode = 0;
					break;
				}
			}
		}

		// do inquery again, this time use LUP_RETURN_BLOB
		retcode = (int) SDPUtilInquery(btDevices, &dwCount, LUP_RETURN_NAME | LUP_RETURN_ADDR | LUP_RETURN_BLOB);
		if (retcode == 0)
		{
			retcode = -1;
			// check if peer is enumerated
			for (i = 0; i < dwCount; i ++)
			{
				if (memcmp(&(btDevices[i].address), &(g_RemoteRadioAddr), sizeof(g_RemoteRadioAddr)) == 0)
				{
					retcode = 0;
					break;
				}
			}
		}
	}
	else
	{
		Sleep(10 * 1000);
	}

	if (retcode == 0)
	{
		return TPR_PASS;
	} 
	else
	{
		return TPR_FAIL;
	}
}

//
//TestId        : 1.1.3
//Title         : Enumerate services on local device
//Description   : Both Master and Slave enumerate services locally
//Expected:
//					Success:	At least one service is found
//					Fail:		No services are found 
//
TESTPROCAPI SDP_f1_1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;

	TEST_ENTRY;

	serviceSearchPattern.Append(L2CAP_PROTOCOL_UUID16);
	retcode = SDPUtilServiceSearch(0, &serviceSearchPattern, ulServiceHandles, &dwCount);

	if ((retcode == 0) && (dwCount > 0))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 1.1.4
//Title         : Enumerate services on Remote device
//Description   : MASTER enumerate services on SLAVE device
//Expected:
//					Success:	At least one service is found
//					Fail:		No services are found 
//
TESTPROCAPI SDP_f1_1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode =	0;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	


	if (!g_bIsServer)
	{
		Sleep(5000);
	}
	else
	{
		serviceSearchPattern.Append(L2CAP_PROTOCOL_UUID16);
		retcode = SDPUtilServiceSearch(*((BT_ADDR*) &g_RemoteRadioAddr), &serviceSearchPattern, ulServiceHandles, &dwCount);
	}

	if ((retcode == 0) && (dwCount > 0))
	{
		return TPR_PASS;
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 1.1.5
//Title         : Register Services and Lookup Local
//Description   : Both Master and Slave Try to register a new service in the local SDP database
//Expected:
//					Success:	The new service can be enumerated on the local database
//					Fail:		The service is not enumerable in the local database
//
TESTPROCAPI SDP_f1_1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode =	0;
	SDPUtilRecord sdpUtilRecord;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);

	if(retcode == 0)
	{
		// query the device for the new service
		serviceSearchPattern.Append(SOCKTEST_PROTOCOL_UUID16);
		retcode = SDPUtilServiceSearch(0, &serviceSearchPattern, ulServiceHandles, &dwCount);
	}

	if ((retcode == 0) && (dwCount > 0))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 1.1.6
//Title         : Modify existing SDP record
//Description   : Both Master and Slave 
//					update the name of the service and save it
//					check if the changes have been made
//Expected		:
//					Success:	the name of the service is changed
//					Fail:		the name of the service is not changed
//
TESTPROCAPI SDP_f1_1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int						retcode = 0;
	SDPUtilRecord			sdpUtilRecord;
	CUUIDList				serviceSearchPattern;
	CAttributeIDRangeList	attributeIDRangeList;
	AttributeIDRange		attributeIDRange;
	ULONG					ulServiceHandles[10];
	DWORD					dwCount = 10;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_UNIQUE_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);

	// change service name
	_tcscpy(sdpUtilRecord.ServiceName, TEXT("Changed SDP Test Service Record\n"));
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
	_tcscpy(sdpUtilRecord.ServiceName, TEXT("\n"));

	//	check if the service name is changed
	if (retcode == 0)
	{
		serviceSearchPattern.Append(SOCKTEST_UNIQUE_UUID16);
		retcode = SDPUtilServiceSearch(0, &serviceSearchPattern, ulServiceHandles, &dwCount);
		if (retcode)
		{
			OutStr(TEXT("Service Search failed, error = %i\n"), retcode);
		}
		else
		{
			if (dwCount == 0)
			{
				OutStr(TEXT("No service record found\n"));
				retcode = -1;
			}
			else
			{
				attributeIDRange.minAttribute = 0x100;
				attributeIDRange.maxAttribute = 0x100;
				attributeIDRangeList.Append(&attributeIDRange);
				retcode = SDPUtilAttributeSearch(0, ulServiceHandles[0], &attributeIDRangeList, &sdpUtilRecord);
			}
		}
	}

	// delete service record
	SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);

	// exit code
	if ((retcode == 0) && (_tcscmp(sdpUtilRecord.ServiceName, TEXT("Changed SDP Test Service Record\n")) == 0))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 1.1.7
//Title         : Delete a service
//Description   : Delete the SOCKTEST service from the local database
//Expected		:
//					Success:	the service is no more enumerable 
//					Fail:		the service is still enumerable 
//	
TESTPROCAPI SDP_f1_1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	SDPUtilRecord sdpUtilRecord;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;
	DWORD		dwBeginCount = 10;

	TEST_ENTRY;

	// find out how many record contains SOCKTEST_PROTOCOL_UUID16
	serviceSearchPattern.Append(SOCKTEST_PROTOCOL_UUID16);
	retcode = SDPUtilServiceSearch(0, &serviceSearchPattern, ulServiceHandles, &dwBeginCount);
	if (retcode)
	{
		OutStr(TEXT("SDPUtilServiceSearch fails, error = %u\n"), retcode);
		goto FailExit;
	}

	// add the record to the local SDP database	
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);

	// delete record
	if (retcode == 0)
	{
		retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);

		// query the device for the new service
		// expected 0 devices found
		if (retcode == 0)
		{
			serviceSearchPattern.Append(SOCKTEST_PROTOCOL_UUID16);
			retcode = SDPUtilServiceSearch(0, &serviceSearchPattern, ulServiceHandles, &dwCount);
		}
	}

FailExit:
	if ((retcode == 0) && (dwCount == dwBeginCount))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}


//
//TestId        : 1.1.8
//Title         : Register a service and lookup remote
//Description   : MASTER registers a new service in the SDP database the SLAVE try to look it up
//Expected:
//					Success:	The new service can be enumerated on the local database
//					Fail:		The service is not enumerable in the local database
//
TESTPROCAPI SDP_f1_1_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode =	0;
	SDPUtilRecord sdpUtilRecord;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;
	CAttributeIDRangeList	attributeIDRangeList;
	AttributeIDRange		attributeIDRange;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	if (g_bIsServer)
	{
		// add the record to the local SDP database		
		SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_UNIQUE_UUID16);				//	port number

		// change service name
		_tcscpy(sdpUtilRecord.ServiceName, TEXT("SDP_f1_1_8\n"));
		retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
		Sleep(5000);
		retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);
	}
	else
	{
		// SLAVE
		Sleep(500);
		// query the device for the new service
		serviceSearchPattern.Append(SOCKTEST_UNIQUE_UUID16);
		retcode = SDPUtilServiceSearch(*((BT_ADDR*) &g_RemoteRadioAddr), &serviceSearchPattern, ulServiceHandles, &dwCount);

		//	check if the service name is changed
		if ((retcode == 0) && (dwCount > 0))
		{
			attributeIDRange.minAttribute = 0x000;
			attributeIDRange.maxAttribute = 0x102;
			attributeIDRangeList.Append(&attributeIDRange);
			SDPUtilInitRecord(&sdpUtilRecord);
			retcode = SDPUtilAttributeSearch(*((BT_ADDR*) &g_RemoteRadioAddr), ulServiceHandles[0], &attributeIDRangeList, &sdpUtilRecord);
			if (retcode == 0)
			{
				if (_tcscmp(sdpUtilRecord.ServiceName, TEXT("SDP_f1_1_8\n")))
					retcode = -1;
			}
			else
			{
				OutStr(TEXT("SDPUtilAttributeSearch fails, error = 0x%08x\n"), retcode);
			}
		}
	}

	if ((retcode == 0) && (dwCount > 0))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 1.1.9
//Title         : Service attribute search
//Description   : SLAVE registers a new service in the SDP database the MASTER try to look it up
//Expected:
//					Success:	The new service can be enumerated on the local database
//					Fail:		The service is not enumerable in the local database
//
TESTPROCAPI SDP_f1_1_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode =	0;
	DWORD		dwCount = 5;
	SDPUtilRecord sdpUtilRecord;
	SDPUtilRecord sdpUtilRecords[5];
	CUUIDList	serviceSearchPattern;
	CAttributeIDRangeList	attributeIDRangeList;
	AttributeIDRange		attributeIDRange;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	if (!g_bIsServer)
	{
		// add the record to the local SDP database		
		SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number

		// change service name
		_tcscpy(sdpUtilRecord.ServiceName, TEXT("SDP_f1_1_9\n"));
		retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
		Sleep(5000);
		retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);
	}
	else
	{
		// MASTER
		Sleep(500);
		// query the device for the new service
		serviceSearchPattern.Append(SOCKTEST_PROTOCOL_UUID16);

		attributeIDRange.minAttribute = 0x000;
		attributeIDRange.maxAttribute = 0x102;
		attributeIDRangeList.Append(&attributeIDRange);
		SDPUtilInitRecord(&sdpUtilRecord);
		retcode = SDPUtilServiceAttributeSearch(*((BT_ADDR*) &g_RemoteRadioAddr), &serviceSearchPattern, &attributeIDRangeList, sdpUtilRecords, &dwCount);
	}

	if ((retcode == 0) && (dwCount > 0))
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.1
//Title         : SERVICE_MULTIPLE flag fail
//Description   : SetService with SERVICE_MULTIPLE should fail for Bluetooth
//				
//Expected		:	
//					Success:	the WSASetService with SERVICE_MULTIPLE flags set fails
//					Fail:		the WSASetService with SERVICE_MULTIPLE flags set passes
//

TESTPROCAPI SDP_f2_1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	SDPUtilRecord sdpUtilRecord;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, SERVICE_MULTIPLE);

	if (retcode != 0)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.2
//Title         : Register Invalid SD Record
//Description   : Test return code for invalid SD record
//				
//Expected		:	
//					Success:	WSASetService fails
//					Fail:		WSASetService passes
//

TESTPROCAPI SDP_f2_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	SDPUtilRecord sdpUtilRecord;

	TEST_ENTRY;

	// add the record to the local SDP database	
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number

	// delete ServiceClassIDList to make it invalid
	sdpUtilRecord.SerivceClassIDList.Clear();

	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);

	if (retcode != 0)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.3
//Title         : Update with Invalid SD Record
//Description   : Test return code for updating a valid SD record with an invalid one
//				
//Expected		:	
//					Success:	updating fails
//					Fail:		updating passes
//

TESTPROCAPI SDP_f2_1_3(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	BOOL bTestFail = FALSE;
	SDPUtilRecord sdpUtilRecord;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
	if (retcode != 0)
	{
		OutStr(TEXT("Failed to register new SDP record, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// delete ServiceClassIDList to make it invalid
	sdpUtilRecord.SerivceClassIDList.Clear();

	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
	if (retcode == 0)
	{
		OutStr(TEXT("Succeeded to update valid SDP record with an invalid one\n"));
		bTestFail = TRUE;
		goto FailExit;
	}

FailExit:
	if (!bTestFail)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.4
//Title         : Delete non-existing record
//Description   : Test return code for invalid SD record
//				
//Expected		:	
//					Success:	Delete fails
//					Fail:		Delete passes
//

TESTPROCAPI SDP_f2_1_4(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	SDPUtilRecord sdpUtilRecord;

	TEST_ENTRY;

	retcode = (int) SDPUtilDeleteRecord(0xFFFFFFFE);

	if (retcode != 0)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.5
//Title         : Delete record twice
//Description   : Test return code for deleting SD record twice
//				
//Expected		:	
//					Success:	Second delete fails
//					Fail:		Second delete passes
//

TESTPROCAPI SDP_f2_1_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	BOOL bTestFail = FALSE;
	SDPUtilRecord sdpUtilRecord;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
	if (retcode != 0)
	{
		OutStr(TEXT("Failed to register new SDP record, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// delete it
	retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);
	if (retcode != 0)
	{
		OutStr(TEXT("Fails to SDP record, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// try delete it again, this time should fail
	retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);
	if (retcode == 0)
	{
		OutStr(TEXT("Succeeded to delete an already delete record while this operation is expected to fail\n"));
		bTestFail = TRUE;
		goto FailExit;
	}

FailExit:
	if (!bTestFail)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.6
//Title         : Lookup service-class ID
//Description   : Lookup service record using lpServiceClassId parameter instead of lpBlob
//				
//Expected		:	
//					Success:	Delete fails
//					Fail:		Delete passes
//

TESTPROCAPI SDP_f2_1_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	BOOL bTestFail = FALSE;
	SDPUtilRecord sdpUtilRecord;
	WSAQUERYSET wsaQuery;
	UCHAR buf[MAX_SDPRECSIZE];
	DWORD dwSize = sizeof(buf);
	LPWSAQUERYSET pResults = (LPWSAQUERYSET) buf;
	HANDLE hLookup;
	GUID uuid;

	TEST_ENTRY;

	// add the record to the local SDP database		
	SDPUtilBuildRecord(&sdpUtilRecord, BTHPROTO_RFCOMM,		//	protocl 1 = RFCOMM
							24, SOCKTEST_PROTOCOL_UUID16);				//	port number
	retcode = (int) SDPUtilUpdateRecord(&sdpUtilRecord, 0);
	if (retcode != 0)
	{
		OutStr(TEXT("Failed to register new SDP record, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// lookup service
	memset(&wsaQuery, 0, sizeof(WSAQUERYSET));
	wsaQuery.dwSize      = sizeof(WSAQUERYSET);
	wsaQuery.dwNameSpace = NS_BTH;
	memcpy(&uuid, &Bluetooth_Base_UUID, sizeof(GUID));
	uuid.Data1 += SOCKTEST_PROTOCOL_UUID16;
	wsaQuery.lpServiceClassId = &uuid;
	wsaQuery.lpBlob      = NULL;
	
	retcode = BthNsLookupServiceBegin(&wsaQuery, LUP_RES_SERVICE, &hLookup);
	if (retcode)
	{
		OutStr(TEXT("BthNsLookupServiceBegin fails, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// get results
	retcode = BthNsLookupServiceNext(hLookup, 0, &dwSize, pResults);
	if (ERROR_SUCCESS == retcode) 
	{
		OutStr(TEXT("BthNsLookupServiceNext returned %u handles\n"), (pResults->lpBlob->cbSize) / sizeof(ULONG));
	}
	else
	{
		OutStr(TEXT("BthNsLookupServiceNext fails, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// end lookup
	retcode = BthNsLookupServiceEnd(hLookup);
	if (retcode)
	{
		OutStr(TEXT("BthNsLookupServiceEnd fails, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

	// fail is none record is found
	if ((pResults->lpBlob->cbSize) / sizeof(ULONG) == 0)
	{
		OutStr(TEXT("No service record found\n"));
		bTestFail = TRUE;
	}

	// delete it
	retcode = (int) SDPUtilDeleteRecord(sdpUtilRecord.ServiceRecordHandle);
	if (retcode != 0)
	{
		OutStr(TEXT("Fails to SDP record, error = %u\n"), retcode);
		bTestFail = TRUE;
		goto FailExit;
	}

FailExit:
	if (!bTestFail)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}

//
//TestId        : 2.1.7
//Title         : Lookup service-invaid address
//Description   : Lookup service on a non-existing device  
//					
//Expected		:	
//					Success:	lookup fails
//					Fail:		lookup succeeds
//
TESTPROCAPI SDP_f2_1_7(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode = 0;
	ULONG		ulServiceHandles[10];
	DWORD		dwCount = 10;
	CUUIDList	serviceSearchPattern;

	TEST_ENTRY;

	serviceSearchPattern.Append(L2CAP_PROTOCOL_UUID16);
	retcode = SDPUtilServiceSearch(0xffff, &serviceSearchPattern, ulServiceHandles, &dwCount);

	if (retcode != 0)
	{
		return TPR_PASS; //Test passed
	} 
	else
	{
		return TPR_FAIL; //Test failed
	}
}
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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "perf_dbEx.h"
#include "dbhlpr.h"

BOOL g_Flag_Sleep=FALSE;
WCHAR g_szDbVolume[MAX_PATH]=VOL_NAME;
WCHAR g_szDb[CEDB_MAXDBASENAMELEN]=DB_NAME;
WCHAR g_szRootPath[MAX_PATH]=DEFAULT_ROOT_PATH;
DWORD g_dwPoolIntervalMs;
DWORD g_dwCPUCalibrationMs;


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_WriteDB
//  
// Params:    
//	pszVolName		:	cannot be NULL.
//	pszDBName		:	cannot be NULL
//	dwNumRecords	:	if 0, then DEFAULT_NUM_RECORDS
//	dwNumProps		:	should be = SORT_PROPS_AVAILABLE
//	dwFlags			:	things like SORT_DESCENDING, UNCOMPRESSED etc. 
//	Primary_Propid	:	Could be 0, if you want no sort. 
//	cbBlob			:	Blob size to use for varying size of record. If 0, then default is used. 
//	ccString			:	(Not used) Should be 0. Changing the string size is disabled. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL    L_WriteDB(LPTSTR pszVolName, 
					LPTSTR pszDBName, 
					DWORD dwNumRecords, 
					DWORD dwNumProps,
					DWORD dwFlags,
					CEPROPID Primary_propid, 
					DWORD cbBlob,
					DWORD ccString)
					
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	PBYTE pbBlob=NULL;
	FILETIME myFileTime;
	CEBLOB myBlob;
	DWORD dwDbaseType=0;
	WORD wNumSortOrder=0;
	SORTORDERSPEC rgSortSpecs[1]={0};
	HANDLE hDB = INVALID_HANDLE_VALUE;
	CEDBASEINFO DBInfo;
	UINT i=0;
	UINT j=0, k=0;
	CEPROPVAL *rgPropVal = NULL;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};   //  This has to be WCHAR because CEVALUNION takes LPWSTR
	DWORD dwPropID_Table_Index=0;
	CEOID RecordOid=0;
	BOOL fRet=FALSE;

	CREATE_INVALIDGUID(&VolGUID);

	ASSERT(dwNumRecords);
	ASSERT(SORT_PROPS_AVAILABLE == dwNumProps);
	
	if (0==dwNumProps)
	{
		//  dwNumProps = DEFAULT_NUMPROPS;
		//  Debug(TEXT("Num of Props not specified, making %d props per record\n"), dwNumProps);
		Debug(TEXT(">>>> ERROR : Num of Props not specified\r\n"));
		goto ErrorReturn;
	}

	if (0==Primary_propid)
		Debug(TEXT(">>> No primary sort property specified\r\n"));

	//  Make sure the ccString is 0, because it's unused. 
	//  We don't want any caller assuming that ccString will be used. 
	ASSERT(0==ccString);

	//  Specifying the sort order for the database
	dwDbaseType = 0xBADDB;
	wNumSortOrder=1;
	memset(rgSortSpecs, 0, sizeof(rgSortSpecs));

	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	//  First delete the volume file. 
	DeleteFile(pszVolName);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	//Perf_MarkBegin(MARK_VOL_MOUNT);
	if (!CeMountDBVol( &VolGUID, pszVolName, CREATE_ALWAYS))
		GENERIC_FAIL(CeMountDBVol);
	//Perf_MarkEnd(MARK_VOL_MOUNT);
    
	//  Fill in the DBInfo structure
	memset(&DBInfo, 0, sizeof(CEDBASEINFO));
	//_tcscpy(DBInfo.szDbaseName, pszDBName);

	StringCchCopy(DBInfo.szDbaseName, CEDB_MAXDBASENAMELEN, pszDBName);
	
	DBInfo.dwDbaseType = dwDbaseType;
	DBInfo.wNumSortOrder=wNumSortOrder;
	ASSERT(1==wNumSortOrder);
	DBInfo.rgSortSpecs[0].propid = Primary_propid;     
	if (dwFlags & PERF_BIT_SORT_DESCENDING)
	{
		Debug(TEXT("Setting Descending Sort order\r\n"));
		DBInfo.rgSortSpecs[0].dwFlags = CEDB_SORT_DESCENDING;
	}
	   
	DBInfo.dwFlags |= CEDB_VALIDDBFLAGS | CEDB_VALIDNAME;

	if (dwFlags & PERF_BIT_DB_UNCOMPRESSED)
	{
		Debug(TEXT("Creating an UNCOMPRESSED database\r\n"));
		DBInfo.dwFlags |= CEDB_NOCOMPRESS;
	}

	if (0 == Primary_propid)
	{
		Debug(TEXT("No Sort order specified. Using none\r\n"));
		wNumSortOrder=0;
		DBInfo.wNumSortOrder=wNumSortOrder;
	}
		
	//  Create the database
	Debug(TEXT("Creating DBEx %s\n"), pszDBName);
	
	if (NULL == (DBOid = CeCreateDatabaseEx(&VolGUID, &DBInfo)))
		GENERIC_FAIL(CeCreateDatabaseEx);
	
	//  Successfully created database. 
	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);

	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	//  ==================================
	//  At this point we have a valid database handle.
	//  ==================================
	if (0==cbBlob)
		cbBlob=100;

	pbBlob = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbBlob);
	CHECK_ALLOC(pbBlob);

	rgPropVal = (CEPROPVAL*)LocalAlloc(LMEM_ZEROINIT, dwNumProps*sizeof(CEPROPVAL));
	memset(rgPropVal, 0, dwNumProps*sizeof(CEPROPVAL));
	CHECK_ALLOC(rgPropVal);
  
	//  =============================================
	//  Create records each with multiple property
	//  =============================================
	Debug(TEXT("Writing records...\n"));
	for (i=0; i<dwNumRecords; i++)
	{
		memset(rgPropVal, 0, dwNumProps*sizeof(CEPROPVAL));

		Hlp_FillBuffer(pbBlob, cbBlob, HLP_FILL_RANDOM);
		
		myBlob.lpb=pbBlob;
		myBlob.dwCount=cbBlob;

		//  =================================
		//  This loop generates all the data
		//  0th property is the Primary SortIndex.
		//  For every other property create some random data and fill in the database.
		//  =================================
		for (j=0; j<dwNumProps; j++)
		{
			rgPropVal[j].propid = j<<16 | rgPropId_Table[j];

			if (LOWORD(Primary_propid) == LOWORD(rgPropVal[j].propid))
				rgPropVal[j].propid = Primary_propid;

			switch(GET_PROPID(rgPropVal[j].propid))
			{
				//  The Primary sort prop gets predictable data 
				//  All other props get random data.
				case CEVT_I2 :          //  Short
						rgPropVal[j].val.iVal = (short)Random();
					break;

				case CEVT_UI2 :         //  USHORT
						rgPropVal[j].val.uiVal = (USHORT)Random();
					break;

				case CEVT_I4 :          //  LONG
						rgPropVal[j].val.lVal = (long)Random();
					break;

				case CEVT_UI4 :         //  ULONG
						rgPropVal[j].val.ulVal = (ULONG)Random();
					break;

				case CEVT_FILETIME :    //  FILETIME
					Hlp_GenRandomFileTime(&myFileTime);
					rgPropVal[j].val.filetime = myFileTime;
					break;

				case CEVT_LPWSTR :      //  LPWSTR
					Hlp_GenStringData(pwszStringData, PERF_MAX_STRING, TST_FLAG_ALPHA_NUM);
					rgPropVal[j].val.lpwstr=pwszStringData;
					break;

				case CEVT_BLOB :        //  BLOB
					rgPropVal[j].val.blob=myBlob;            
					break;

				case CEVT_BOOL :        //  BOOLVAL
					rgPropVal[j].val.boolVal =  TRUE*(j%2);
					break;

				case CEVT_R8 :          //  DOUBLE
						rgPropVal[j].val.dblVal=(double)Random();
					break;

				default :
					Debug(TEXT("BUGBUG: Logic error\n"));
					ASSERT(FALSE);
					GENERIC_FAIL(L_WriteDB);
			}
		}

		if (PERF_BIT_NO_MEASURE & dwFlags)
		{
			RecordOid = CeWriteRecordProps(hDB, 0, (WORD)dwNumProps, rgPropVal);
		}
		else
		{
			Perf_MarkBegin(MARK_RECWRITE);
			RecordOid = CeWriteRecordProps(hDB, 0, (WORD)dwNumProps, rgPropVal);
			Perf_MarkEnd(MARK_RECWRITE);
		}

		if (!RecordOid)
		{
			Hlp_DisplayMemoryInfo();
			
			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));

			Sleep(500);

			if (PERF_BIT_NO_MEASURE & dwFlags)
			{
				RecordOid = CeWriteRecordProps(hDB, 0, (WORD)dwNumProps, rgPropVal);
			}
			else
			{
				Perf_MarkBegin(MARK_RECWRITE);
				RecordOid = CeWriteRecordProps(hDB, 0, (WORD)dwNumProps, rgPropVal);
				Perf_MarkEnd(MARK_RECWRITE);
			}

			
			if (!RecordOid)
				GENERIC_FAIL(CeWriteRecordProps);
		}

		//  Log every 75 records
		if (0==i%75)
			Debug(TEXT("\nWrote record # %d "), i);

		if (g_Flag_Sleep && (0==i%2000))
			Sleep(1000);
	}
	Debug(TEXT("Database write completed. Wrote %d records\n"), i);

	//  Close the database
	
	CLOSE_HANDLE(hDB);
	
	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);

	fRetVal=TRUE;

ErrorReturn :

	CLOSE_HANDLE(hDB);
	
	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
	
	FREE(pbBlob);
	FREE(rgPropVal);
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_ModifyAllRecords
//	This function modifies all the properties in the record, including
//	the sort property. 
//	This results in re-insertion in index. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_ModifyAllRecords(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	WORD wPropsRead=0;
	PCEPROPVAL rgPropVal;
	UINT i=0;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};
	DWORD dwNumRecords=0;
	CEOID* pRecOids=NULL;
	DWORD dwOidIndex=0;
	DWORD cRecOids=0;
	CEOID RecordOid_Seek=0;
	BOOL fTemp=FALSE;

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	//  Collect all the Record Oids. 
	if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
		GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
	ASSERT(dwNumRecords);

	pRecOids = (CEOID*)LocalAlloc(LMEM_ZEROINIT, dwNumRecords*sizeof(CEOID));
	CHECK_ALLOC(pRecOids);

	dwOidIndex=0;
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		ASSERT(SORT_PROPS_AVAILABLE==wPropsRead );
		pRecOids[dwOidIndex++] = RecordOid;
	}
	cRecOids = dwOidIndex;

	ASSERT(dwOidIndex == dwNumRecords);
	//  Then delete the props per record oid. 

	if (!CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, NULL))
		GENERIC_FAIL(CeSeekDatabase);
	
	//  Read all records. 
	dwOidIndex=0;
	dwNumRecords=0;
	while (dwOidIndex < cRecOids)
	{	
		RecordOid_Seek=0;
		if (!(RecordOid_Seek = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, pRecOids[dwOidIndex++], NULL)))
			GENERIC_FAIL(CeSeekDatabase);

		RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL);
		ASSERT(RecordOid_Seek == RecordOid);
		if (RecordOid_Seek != RecordOid)
		{
			Debug(TEXT(">>>>ERROR : Expecting record with oid=0x%x, got 0x%x instead\r\n"), RecordOid_Seek, RecordOid);
			goto ErrorReturn;
		}
		
		rgPropVal = (PCEPROPVAL)pbRecord;
		for (i=0; i<wPropsRead; i++)
		{
			//  Modify the records. 
			switch(LOWORD(rgPropVal[i].propid))
			{
				//  The Primary sort prop gets predictable data 
				//  All other props get random data.
				case CEVT_I2 :          //  Short
						rgPropVal[i].val.iVal = (short)Random();
					break;

				case CEVT_UI2 :         //  USHORT
						rgPropVal[i].val.uiVal = (USHORT)Random();
					break;

				case CEVT_I4 :          //  LONG
						rgPropVal[i].val.lVal = (long)Random();
					break;

				case CEVT_UI4 :         //  ULONG
						rgPropVal[i].val.ulVal = (ULONG)Random();
					break;

				case CEVT_FILETIME :    //  FILETIME
					Hlp_GenRandomFileTime(&(rgPropVal[i].val.filetime));
					break;

				case CEVT_LPWSTR :      //  LPWSTR
					Hlp_GenStringData(rgPropVal[i].val.lpwstr, _tcslen(rgPropVal[i].val.lpwstr)+1, TST_FLAG_ALPHA_NUM);
					break;

				case CEVT_BLOB :        //  BLOB
					Hlp_FillBuffer(rgPropVal[i].val.blob.lpb, rgPropVal[i].val.blob.dwCount, HLP_FILL_RANDOM);
					break;

				case CEVT_BOOL :        //  BOOLVAL
					rgPropVal[i].val.boolVal =  TRUE*(i%2);
					break;

				case CEVT_R8 :          //  DOUBLE
						rgPropVal[i].val.dblVal=(double)Random();
					break;

				default :
					Debug(TEXT("BUGBUG: Logic error\n"));
					ASSERT(FALSE);
					GENERIC_FAIL(L_ModifyAllRecords);
			}
		}


		Perf_MarkBegin(MARK_RECWRITE);
		fTemp=CeWriteRecordProps(hDB, RecordOid, wPropsRead, rgPropVal);
		Perf_MarkEnd(MARK_RECWRITE);

		if (!fTemp)
		{
			Hlp_DisplayMemoryInfo();
			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));

			Sleep(500);

			Perf_MarkBegin(MARK_RECWRITE);
			fTemp=CeWriteRecordProps(hDB, RecordOid, wPropsRead, rgPropVal);
			Perf_MarkEnd(MARK_RECWRITE);

			if (!fTemp) 
				GENERIC_FAIL(CeWriteRecordProps);
		}


		dwNumRecords++;

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nWrote record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);

	}
	ASSERT((MANY_RECORDS==dwNumRecords) || (FEW_RECORDS==dwNumRecords));

	//  Cleanup:
	CLOSE_HANDLE(hDB);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);
	
	fRetVal=TRUE;

ErrorReturn :
	FREE(pRecOids);
	FREE(pbRecord);
	
	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
		
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_DelPropsAllRecords
//	Delete properties from all records
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_DelPropsAllRecords(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	WORD wPropsRead=0;
	//PCEPROPVAL rgPropVal;
	UINT i=0, k=0;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};
	CEPROPVAL rgPropVal_Del[6] = {0};
	DWORD dwNumRecords=0;
	BOOL fTemp=FALSE;

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	//  Read all records. 
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		dwNumRecords++;
		memset(rgPropVal_Del, 0, sizeof(rgPropVal_Del));
		const CEPROPVAL* rgPropVal = (const CEPROPVAL*)pbRecord;
		
		//  Delete all properties except ULONG, String and filetime:
		ASSERT(SORT_PROPS_AVAILABLE == wPropsRead);
		if (wPropsRead != SORT_PROPS_AVAILABLE)
		{
			Debug(TEXT(">>> ERROR: Should have read %d properties. Instead read only %d\r\n"), SORT_PROPS_AVAILABLE, wPropsRead);
			DUMP_LOCN;
			goto ErrorReturn;
		}

		i=0;
		k=0;
		while ((i<wPropsRead))
		{
			ASSERT(k<6);
			if (k >= 6)
				break;
			
			if ((CEVT_UI4 != LOWORD(rgPropVal[i].propid)) &&
				(CEVT_LPWSTR != LOWORD(rgPropVal[i].propid)) &&
				(CEVT_FILETIME != LOWORD(rgPropVal[i].propid)))
			{
				rgPropVal_Del[k].propid=rgPropVal[i].propid;
				rgPropVal_Del[k].wFlags=CEDB_PROPDELETE;
				k++;
			}
			i++;
		}

		Perf_MarkBegin(MARK_RECWRITE);
		fTemp = CeWriteRecordProps(hDB, RecordOid, 6, rgPropVal_Del);
		Perf_MarkEnd(MARK_RECWRITE);

		if (!fTemp)
		{
			Hlp_DisplayMemoryInfo();

			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));
			Sleep(500);
			
			Perf_MarkBegin(MARK_RECWRITE);
			fTemp = CeWriteRecordProps(hDB, RecordOid, 6, rgPropVal_Del);
			Perf_MarkEnd(MARK_RECWRITE);

			if (!fTemp)    
				GENERIC_FAIL(CeWriteRecordProps);
		}

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nWrote record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);
		
	}
	
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	//  Cleanup:
	CLOSE_HANDLE(hDB);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);
	
	fRetVal=TRUE;

ErrorReturn :

	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
		FREE(pbRecord);
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_AddPropsToDB
//  
// Params:    
//	pszVolName		:	cannot be NULL.
//	pszDBName		:	cannot be NULL
//	dwNumProps		:	should be 6
//	Primary_Propid	:	Could be 0, if you want no sort. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL    L_AddPropsToDB(LPTSTR pszVolName, 
							LPTSTR pszDBName, 
							DWORD dwNumProps,
							CEPROPID Primary_propid)
					
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	PBYTE pbBlob=NULL;
	static DWORD cbBlob=100;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	FILETIME myFileTime;
	CEBLOB myBlob;
	HANDLE hDB = INVALID_HANDLE_VALUE;
	UINT j=0, k=0;
	CEPROPVAL rgPropVal_Add[6]={0};
	WCHAR pwszStringData[PERF_MAX_STRING]={0};   //  This has to be WCHAR because CEVALUNION takes LPWSTR
	CEOID RecordOid=0;
	static WORD wNumNewProps=6;
	WORD wPropsRead=0;
	DWORD dwNumRecords=0;
	BOOL fTemp=FALSE;

	ASSERT(6==dwNumProps);

	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
		GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	dwNumRecords=0;

	//  ==================================
	//  At this point we have a valid database handle.
	//  ==================================
	pbBlob = (PBYTE)LocalAlloc(LMEM_ZEROINIT, cbBlob);
	CHECK_ALLOC(pbBlob);

	//  =============================================
	//  Create records each with multiple property
	//  =============================================
	Debug(TEXT("Writing records...\n"));

	//  Read all records. 
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		dwNumRecords++;
		memset(rgPropVal_Add, 0, sizeof(rgPropVal_Add));
		ASSERT(3 == wPropsRead);

		Hlp_FillBuffer(pbBlob, cbBlob, HLP_FILL_RANDOM);
		
		myBlob.lpb=pbBlob;
		myBlob.dwCount=cbBlob;

		//  =================================
		//  This loop generates all the data
		//  0th property is the Primary SortIndex.
		//  For every other property create some random data and fill in the database.
		//  =================================
		
		for (j=0, k=0; j<wNumNewProps; j++, k++)
		{
			
			while ((0==rgPropVal_Add[j].propid) && (k<SORT_PROPS_AVAILABLE))
			{
				if ((CEVT_UI4 == LOWORD(rgPropId_Table[k])) ||
					(CEVT_LPWSTR == LOWORD(rgPropId_Table[k])) ||
					(CEVT_FILETIME == LOWORD(rgPropId_Table[k])))
				{
					k++;
					continue;
				}
				rgPropVal_Add[j].propid = j<<16 | rgPropId_Table[k];
			
				switch(GET_PROPID(rgPropVal_Add[j].propid))
				{
					//  The Primary sort prop gets predictable data 
					//  All other props get random data.
					case CEVT_I2 :          //  Short
							rgPropVal_Add[j].val.iVal = (short)Random();
						break;

					case CEVT_UI2 :         //  USHORT
							rgPropVal_Add[j].val.uiVal = (USHORT)Random();
						break;

					case CEVT_I4 :          //  LONG
							rgPropVal_Add[j].val.lVal = (long)Random();
						break;

					case CEVT_UI4 :         //  ULONG
							ASSERT(0); //  shouldn't reach here. 
							rgPropVal_Add[j].val.ulVal = (ULONG)Random();
						break;

					case CEVT_FILETIME :    //  FILETIME
						ASSERT(0); //  shouldn't reach here. 
						Hlp_GenRandomFileTime(&myFileTime);
						rgPropVal_Add[j].val.filetime = myFileTime;
						break;

					case CEVT_LPWSTR :      //  LPWSTR
						ASSERT(0); //  shouldn't reach here. 
						Hlp_GenStringData(pwszStringData, PERF_MAX_STRING, TST_FLAG_ALPHA_NUM);
						rgPropVal_Add[j].val.lpwstr=pwszStringData;
						break;

					case CEVT_BLOB :        //  BLOB
						rgPropVal_Add[j].val.blob=myBlob;            
						break;

					case CEVT_BOOL :        //  BOOLVAL
						rgPropVal_Add[j].val.boolVal =  TRUE*(j%2);
						break;

					case CEVT_R8 :          //  DOUBLE
						rgPropVal_Add[j].val.dblVal=(double)Random();
						break;

					default :
						Debug(TEXT("BUGBUG: Logic error\n"));
						ASSERT(FALSE);
						GENERIC_FAIL(L_AddPropsToDB);
				}
			}
		}

		ASSERT(RecordOid);
		Perf_MarkBegin(MARK_RECWRITE);
		fTemp = CeWriteRecordProps(hDB, RecordOid, wNumNewProps, rgPropVal_Add);
		Perf_MarkEnd(MARK_RECWRITE);

		if (!fTemp)
		{
			Hlp_DisplayMemoryInfo();

			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));

			Sleep(500);

			Perf_MarkBegin(MARK_RECWRITE);
			fTemp = CeWriteRecordProps(hDB, RecordOid, wNumNewProps, rgPropVal_Add);
			Perf_MarkEnd(MARK_RECWRITE);

			if (!fTemp)
				GENERIC_FAIL(CeWriteRecordProps);
		}

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nWrote record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);
	}
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	//  Close the database
		CLOSE_HANDLE(hDB);
	
	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);

	fRetVal=TRUE;

ErrorReturn :
	CLOSE_HANDLE(hDB);
	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
	
	FREE(pbBlob);
	FREE(pbRecord);
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_Del_Sort_PropAllRecords
//  
///////////////////////////////////////////////////////////////////////////////
BOOL L_Del_Sort_PropAllRecords(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	WORD wPropsRead=0;
	UINT i=0, k=0;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};
	CEPROPVAL PropVal_Del;
	DWORD dwNumRecords=0;
	CEOID* pRecOids=NULL;
	DWORD dwOidIndex=0;
	DWORD cRecOids=0;
	BOOL fTemp=FALSE;

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	if (0 == Primary_propid)
		return TRUE;

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);


	//  Collect all the Record Oids. 
	if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
		GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
	ASSERT(dwNumRecords);

	pRecOids = (CEOID*)LocalAlloc(LMEM_ZEROINIT, dwNumRecords*sizeof(CEOID));
	CHECK_ALLOC(pRecOids);

	dwOidIndex=0;
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		ASSERT(SORT_PROPS_AVAILABLE==wPropsRead );
		pRecOids[dwOidIndex++] = RecordOid;
	}
	cRecOids = dwOidIndex;

	ASSERT(dwOidIndex == dwNumRecords);
	//  Then delete the props per record oid. 

	if (!CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, NULL))
		GENERIC_FAIL(CeSeekDatabase);

	//  Read all records. 
	dwOidIndex=0;
	dwNumRecords=0;
	while (dwOidIndex < cRecOids)
	{
		RecordOid = pRecOids[dwOidIndex++];
		memset(&PropVal_Del, 0, sizeof(PropVal_Del));
		
		PropVal_Del.propid=Primary_propid;
		PropVal_Del.wFlags=CEDB_PROPDELETE;

		Perf_MarkBegin(MARK_RECWRITE);
		fTemp = CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_Del);
		Perf_MarkEnd(MARK_RECWRITE);

		if (!fTemp)
		{
			Hlp_DisplayMemoryInfo();

			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));
			
			Sleep(500);

			Perf_MarkBegin(MARK_RECWRITE);
			fTemp = CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_Del);
			Perf_MarkEnd(MARK_RECWRITE);

			if (!fTemp)    
				GENERIC_FAIL(CeWriteRecordProps);
		}

		dwNumRecords++;

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nWrote record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);
	}
	
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	//  Cleanup:
	CLOSE_HANDLE(hDB);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);
	
	fRetVal=TRUE;

ErrorReturn :
	FREE(pRecOids);
	FREE(pbRecord);
	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function:	L_AddPropsToDB
//  
//  Params:    
//  pszVolName:		cannot be NULL.
//  pszDBName:		cannot be NULL
//  dwNumProps:		ignored
//  Primary_Propid:	Could be 0, if you want no sort. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_Add_Sort_PropsToDB(LPTSTR pszVolName, 
								LPTSTR pszDBName, 
								DWORD dwNumProps,
								CEPROPID Primary_propid)
					
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB = INVALID_HANDLE_VALUE;
	UINT j=0, k=0;
	CEPROPVAL PropVal_Add={0};
	CEOID RecordOid=0;
	WORD wPropsRead=0;
	DWORD dwNumRecords=0;
	FILETIME myFileTime;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};
	CEOID* pRecOids=NULL;
	DWORD dwOidIndex=0;
	DWORD cRecOids=0;
	BOOL fTemp=FALSE;

	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	if (0 == Primary_propid)
		return TRUE;


	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);


	if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
		GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	pRecOids = (CEOID*)LocalAlloc(LMEM_ZEROINIT, dwNumRecords*sizeof(CEOID));
	CHECK_ALLOC(pRecOids);

	dwOidIndex=0;
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		ASSERT(SORT_PROPS_AVAILABLE==wPropsRead+1 );
		pRecOids[dwOidIndex++] = RecordOid;
	}
	cRecOids = dwOidIndex;

	ASSERT(dwOidIndex == dwNumRecords);

	if (!CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, NULL))
		GENERIC_FAIL(CeSeekDatabase);



	dwOidIndex=0;
	dwNumRecords=0;
	while (dwOidIndex < cRecOids)
	{
		RecordOid = pRecOids[dwOidIndex++];
		memset(&PropVal_Add, 0, sizeof(PropVal_Add));

		//  =================================
		//  Create data for the primary prop id.
		//  =================================
		PropVal_Add.propid = Primary_propid;
		
		switch(GET_PROPID(PropVal_Add.propid))
		{
			case CEVT_UI4 :         //  ULONG
					PropVal_Add.val.ulVal = (ULONG)Random();
				break;

			case CEVT_FILETIME :    //  FILETIME
				Hlp_GenRandomFileTime(&myFileTime);
				PropVal_Add.val.filetime = myFileTime;
				break;

			case CEVT_LPWSTR :      //  LPWSTR
				Hlp_GenStringData(pwszStringData, PERF_MAX_STRING, TST_FLAG_ALPHA_NUM);
				PropVal_Add.val.lpwstr=pwszStringData;
				break;

			default :
				//  The only property missing should be the SORT property. 
				//  And that should bethe only property added in this code. 
				Debug(TEXT("BUGBUG: Logic error\n"));
				ASSERT(FALSE);
				GENERIC_FAIL(L_AddPropsToDB);
		}


		ASSERT(RecordOid);
		Perf_MarkBegin(MARK_RECWRITE);
		fTemp=CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_Add);
		Perf_MarkEnd(MARK_RECWRITE);

		if (!fTemp)
		{
			Hlp_DisplayMemoryInfo();

			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));

			Sleep(500);

			Perf_MarkBegin(MARK_RECWRITE);
			fTemp=CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_Add);
			Perf_MarkEnd(MARK_RECWRITE);

			if (!fTemp)    
				GENERIC_FAIL(CeWriteRecordProps);
		}

		dwNumRecords++;

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nWrote record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);
	}
	
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));


	//  Close the database
	
	CLOSE_HANDLE(hDB);
	
	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);

	fRetVal=TRUE;

ErrorReturn :
	FREE(pRecOids);
	CLOSE_HANDLE(hDB);
	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
			
	FREE(pbRecord);
	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
//  Function: L_PrepareDBForSeek
//
//  Modifies the database so that we have predictable data for the sort prop. 
//  The data will be increasing. 
//  Unique values are guarenteed for all except FILETIME. 
//  that will be verified later. 
//  If there is no sort property, then the function returns TRUE. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_PrepareDBForSeek(LPTSTR        pszVolName, 
						LPTSTR      pszDBName, 
						CEPROPID    Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	WORD wPropsRead=0;
	CEPROPVAL PropVal_New;
	UINT i=0;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};
	DWORD dwNumRecords=0;
	FILETIME myFileTime;
	CEOID* pRecOids=NULL;
	DWORD dwOidIndex=0;
	DWORD cRecOids=0;
	CEOID RecordOid_new=0;
	
	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	//  If there is no sort property on the database, there is nothing
	//  to be done in form of preparing the database. 
	if (0==Primary_propid)
		return TRUE;


	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	if (CEVT_FILETIME == LOWORD(Primary_propid))
	{
		myFileTime.dwHighDateTime = PERF_START_HIGH_DATE;
		myFileTime.dwLowDateTime = PERF_START_LOW_DATE;
	}
	
	if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
		GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	pRecOids = (CEOID*)LocalAlloc(LMEM_ZEROINIT, dwNumRecords*sizeof(CEOID));
	CHECK_ALLOC(pRecOids);

	dwOidIndex=0;
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		ASSERT(SORT_PROPS_AVAILABLE==wPropsRead );
		pRecOids[dwOidIndex++] = RecordOid;
	}
	cRecOids = dwOidIndex;

	ASSERT(dwOidIndex == dwNumRecords);

	if (!CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, NULL))
		GENERIC_FAIL(CeSeekDatabase);



	dwOidIndex=0;
	dwNumRecords=0;
	while (dwOidIndex < cRecOids)
	{
		RecordOid = pRecOids[dwOidIndex++];
		memset(&PropVal_New, 0, sizeof(PropVal_New));
		PropVal_New.propid = Primary_propid;


		//  Modify the records. 
		switch(LOWORD(PropVal_New.propid))
		{
			//  Modify the Primary sort prop to get predictable data 
			case CEVT_UI4 :         //  ULONG
					PropVal_New.val.ulVal = (ULONG)dwNumRecords;
				break;

			case CEVT_FILETIME :    //  FILETIME
				//Hlp_GetSysTimeAsFileTime(&(PropVal_New.val.filetime));
				PropVal_New.val.filetime.dwHighDateTime = myFileTime.dwHighDateTime;
				PropVal_New.val.filetime.dwLowDateTime = myFileTime.dwLowDateTime++;
				break;

			case CEVT_LPWSTR :      //  LPWSTR
				_stprintf(pwszStringData, _T("Record_%d"), dwNumRecords);
				PropVal_New.val.lpwstr = pwszStringData;
				break;

			default :
				Debug(TEXT("BUGBUG: Logic error\n"));
				ASSERT(FALSE);
				GENERIC_FAIL(L_ModifyAllRecords);
		}


		if (!(RecordOid_new = CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_New)))
		{
			Hlp_DisplayMemoryInfo();

			Debug(TEXT(">>>>CeWriteRecordProps failed line %d file %s. Error=0x%x\r\n"), 
						__LINE__, _T(__FILE__), GetLastError());
			Debug(TEXT("Sleeping and trying again...... \r\n"));

			Sleep(500);

			if (!(RecordOid_new = CeWriteRecordProps(hDB, RecordOid, 1, &PropVal_New)))    
				GENERIC_FAIL(CeWriteRecordProps);
		}
		ASSERT(RecordOid_new == RecordOid);
		dwNumRecords++;

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nModified record # %d "), dwNumRecords);
		if (g_Flag_Sleep && (0==dwNumRecords%2000))
			Sleep(1000);
	}
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));


	//  Now that all the records are properly modified, we can do seek. 

	fRetVal=TRUE;

ErrorReturn:
	FREE(pbRecord);
	FREE(pRecOids);
	CLOSE_HANDLE(hDB);
	
	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		

	return fRetVal;
}



///////////////////////////////////////////////////////////////////////////////
//
// Function: L_Measure_DBSeek
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_Measure_DBSeek(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid, DWORD dwSeekType, DWORD dwFlags)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid_Cur=0;
	CEOID RecordOid_Prev=0;
	CEOID RecordOid_Next=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	DWORD dwNumRecords=0;
	WORD wPropsRead=0;
	DWORD dwIndex=0, *pIndex=NULL;
	BOOL fSeekDone=FALSE;
	CEOID* pRecOids=NULL;
	DWORD cRecOids=0;
	DWORD dwOidIndex=0;
	CEPROPVAL Seek_PropVal;
	FILETIME myFileTime;
	WCHAR pwszStringData[PERF_MAX_STRING]={0};

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	// Test is skipped for seeking by value when there is no sort property
	if ((0==Primary_propid) && (dwSeekType == PERF_SEEK_VALUE))
		return TRUE;

	if (PERF_BIT_NO_INDEX & dwFlags)
		pIndex = NULL;
	else
		pIndex = &dwIndex;
	
	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							0, // CEDB_AUTOINCREMENT
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	//  ------------
	//  Do the initial setup depending on the seek type
	//  ------------
	switch(dwSeekType) 
	{
		case PERF_SEEK_NEXT : 
			//  Just rewind to the start of DB
			CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, pIndex);
			break;
			
		case PERF_SEEK_PREV : 
			//  Goto the end of DB
			CeSeekDatabase(hDB, CEDB_SEEK_END, 0, pIndex);
			break;

		case PERF_SEEK_OID :
			//  Get all the record oids, so we can sequentially seek for them
			if (!Hlp_GetRecordCount_FromDBOidEx(VolGUID, DBOid, &dwNumRecords))
				GENERIC_FAIL(Hlp_GetRecordCount_FromDBOidEx);
			ASSERT(dwNumRecords);
			
			pRecOids = (CEOID*)LocalAlloc(LMEM_ZEROINIT, dwNumRecords*sizeof(CEOID));
			CHECK_ALLOC(pRecOids);

			//  reopen the database with CEDB_AUTOINCREMENT
			CLOSE_HANDLE(hDB);
			hDB = CeOpenDatabaseEx(&VolGUID, 
									&DBOid, 
									pszDBName, 
									Primary_propid, 
									CEDB_AUTOINCREMENT,
									NULL);
			if (INVALID_HANDLE_VALUE == hDB)
				GENERIC_FAIL(CEOpenDataBaseEx);
			
			dwOidIndex=0;
			while (RecordOid_Cur = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
				pRecOids[dwOidIndex++] = RecordOid_Cur;
			cRecOids = dwOidIndex-1;

			ASSERT(dwOidIndex == dwNumRecords);

			//  Reopen the database without CEDB_AUTOINCREMENT
			CLOSE_HANDLE(hDB);
			hDB = CeOpenDatabaseEx(&VolGUID, 
									&DBOid, 
									pszDBName, 
									Primary_propid, 
									0, // CEDB_AUTOINCREMENT
									NULL);
			if (INVALID_HANDLE_VALUE == hDB)
				GENERIC_FAIL(CEOpenDataBaseEx);

			dwNumRecords=0;
			dwOidIndex=1;  //  to correctly seek the second record after the first one is read. 

			break;

		case PERF_SEEK_VALUE :
			//  Set to the beginning of DB. 
			//  Also, set the "seed" FileTime. 
			CeSeekDatabase(hDB, CEDB_SEEK_BEGINNING, 0, pIndex);

			//  set the starting point for time.
			myFileTime.dwHighDateTime = PERF_START_HIGH_DATE;
			myFileTime.dwLowDateTime = PERF_START_LOW_DATE+1;  //  This corresponds to the first retrieved record.
			break;
			
		default : 
			Debug(TEXT("Logic Error in file %s at line %d\r\n"), _T(__FILE__), __LINE__);
			ASSERT(0);
			goto ErrorReturn;
	}
	
	//  Start measuring SEEK. 
	fSeekDone=FALSE;

	while ((RecordOid_Cur = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL)) &&
			!fSeekDone)
	{
		if (RecordOid_Prev == RecordOid_Cur)
		{
			Debug(TEXT(">>>> ERROR : Previous oid = 0x%x, currently read oid=0x%x, dwNumRecords=%d\r\n"), 
				RecordOid_Prev, RecordOid_Cur, dwNumRecords);
			goto ErrorReturn;
		}
		if (RecordOid_Next && (RecordOid_Cur != RecordOid_Next))
		{
			Debug(TEXT(">>>> ERROR : Expecting next oid = 0x%x, but read oid=0x%x, dwNumRecords=%d\r\n"), 
				RecordOid_Next, RecordOid_Cur, dwNumRecords);
			goto ErrorReturn;
		}

		RecordOid_Prev = RecordOid_Cur;

		//  Measure the Seek
		switch (dwSeekType)
		{
			//  SEEK_NEXT
			case PERF_SEEK_NEXT :
				Perf_MarkBegin(MARK_REC_SEEK);
				RecordOid_Next = CeSeekDatabase(hDB, CEDB_SEEK_CURRENT, 1, pIndex);
				Perf_MarkEnd(MARK_REC_SEEK);
				
				if (!RecordOid_Next)
				{
					Debug(TEXT("Seek Database ended with dwNumRecords=%d\r\n"), dwNumRecords);
					fSeekDone=TRUE;
				}

				break;

			//  SEEK PERVIOUS
			case PERF_SEEK_PREV :
				Perf_MarkBegin(MARK_REC_SEEK);
				RecordOid_Next = CeSeekDatabase(hDB, CEDB_SEEK_CURRENT, -1, pIndex);
				Perf_MarkEnd(MARK_REC_SEEK);

				if (!RecordOid_Next)
				{
					Debug(TEXT("Seek Database ended with dwNumRecords=%d\r\n"), dwNumRecords);
					fSeekDone=TRUE;
				}

				break;

			//  SEEK BY OID
			case PERF_SEEK_OID : 
				if (dwOidIndex > cRecOids)
					fSeekDone=TRUE;
				else
				{
					Perf_MarkBegin(MARK_REC_SEEK);
					RecordOid_Next = CeSeekDatabase(hDB, CEDB_SEEK_CEOID, pRecOids[dwOidIndex++], pIndex);
					Perf_MarkEnd(MARK_REC_SEEK);
					
					if (!RecordOid_Next)
						Debug(TEXT("Seek Database ended with dwNumRecords=%d\r\n"), dwNumRecords);
					
				}
				break;

			//  SEEK BY VALUE
			case PERF_SEEK_VALUE :
				//  Generate the value to seek for. 
				memset(&Seek_PropVal, 0, sizeof(CEPROPVAL));
				
				Seek_PropVal.propid = Primary_propid;
				switch (LOWORD(Primary_propid))
				{
					case CEVT_UI4 : 
						Seek_PropVal.val.ulVal = (ULONG)dwNumRecords+1;
					break;

					case CEVT_FILETIME :    //  FILETIME
						Seek_PropVal.val.filetime.dwHighDateTime = myFileTime.dwHighDateTime;
						Seek_PropVal.val.filetime.dwLowDateTime = myFileTime.dwLowDateTime++;
						fRetVal=TRUE;
						goto ErrorReturn;
					break;

					case CEVT_LPWSTR :      //  LPWSTR
						_stprintf(pwszStringData, _T("Record_%d"), dwNumRecords+1);
						Seek_PropVal.val.lpwstr = pwszStringData;
						break;
				}
		
				//  Then do the seek. 
				Perf_MarkBegin(MARK_REC_SEEK);
				RecordOid_Next = CeSeekDatabase(hDB, CEDB_SEEK_VALUEFIRSTEQUAL, (DWORD)&Seek_PropVal, pIndex);
				Perf_MarkEnd(MARK_REC_SEEK);
				
				if (!RecordOid_Next)
				{
					Debug(TEXT("Seek Database ended with dwNumRecords=%d\r\n"), dwNumRecords);
					fSeekDone=TRUE;
				}
				
				break;
		}
		dwNumRecords++;
	}
	
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));
	if ((dwNumRecords!=MANY_RECORDS) && (FEW_RECORDS!=dwNumRecords))
	{
		Debug(TEXT(">>>>>> ERROR : Seek tests ended too soon. Found only %d records. File %s line %d\r\n"), dwNumRecords, _T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	fRetVal=TRUE;

ErrorReturn:
	FREE(pbRecord);
	FREE(pRecOids);
	CLOSE_HANDLE(hDB);
	
	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_DeleteAllRecords
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_DeleteAllRecords(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	WORD wPropsRead=0;
	DWORD dwNumRecords=0;
	BOOL fTemp=FALSE;

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	//  Read all records. 
	while (RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL))
	{
		dwNumRecords++;
	   
		Perf_MarkBegin(MARK_RECDELETE);
		fTemp = CeDeleteRecord(hDB, RecordOid);
		Perf_MarkEnd(MARK_RECDELETE);

		if (!fTemp)
			GENERIC_FAIL(CeDeleteRecord);

		//  Log every 75 records
		if (0==dwNumRecords%75)
			Debug(TEXT("\nDeleted record # %d "), dwNumRecords);
	}
	
	ASSERT((dwNumRecords==MANY_RECORDS) || (FEW_RECORDS==dwNumRecords));

	//  Cleanup:
	CLOSE_HANDLE(hDB);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);
	
	fRetVal=TRUE;
ErrorReturn :

	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
		FREE(pbRecord);
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_SetDbInfo
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_SetDbInfo(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid, DWORD dwFlags)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	BOOL fTemp=FALSE;
	CEOIDINFO OidInfo;
	CEOID RecordOid=0;
	PBYTE pbRecord=NULL;
	DWORD cbRecord=0;
	WORD wPropsRead=0;
	DWORD i=0;
	PCEPROPVAL rgPropVals=NULL;
	
	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	if (!(RecordOid = CeReadRecordPropsEx(hDB, CEDB_ALLOWREALLOC, &wPropsRead, NULL, &pbRecord, &cbRecord, NULL)))
		GENERIC_FAIL(CeReadRecordPropsEx);
	rgPropVals=(PCEPROPVAL)pbRecord;

	CLOSE_HANDLE(hDB);

	//  Get the DB Info. 
	if (!CeOidGetInfoEx(&VolGUID, DBOid, &OidInfo))
		GENERIC_FAIL(CeOidGetInfoEx);

	if (Primary_propid && (1 != OidInfo.infDatabase.wNumSortOrder))
		Debug(TEXT(">>> ERRROR : Incorrect number of sort orders. Expecting 1\r\n"));

	ASSERT(OBJTYPE_DATABASE == OidInfo.wObjType);
	//ASSERT(1 == OidInfo.infDatabase.wNumSortOrder);
	//ASSERT(Primary_propid == OidInfo.infDatabase.rgSortSpecs[0].propid);
	
	i=0;
	OidInfo.infDatabase.rgSortSpecs[0].propid=0;
	switch(Primary_propid)
	{
		case GLOBAL_SORT_KEY_ULONG :         //  ULONG
			while ((i<wPropsRead) && (LOWORD(GLOBAL_SORT_KEY_LPWSTR) != LOWORD(rgPropVals[i].propid)))
				i++;
			OidInfo.infDatabase.rgSortSpecs[0].propid = rgPropVals[i].propid;
			//OidInfo.infDatabase.rgSortSpecs[0].propid = GLOBAL_SORT_KEY_LPWSTR;
			break;

		case GLOBAL_SORT_KEY_LPWSTR :    //  FILETIME
			while ((i<wPropsRead) && (LOWORD(GLOBAL_SORT_KEY_FILETIME) != LOWORD(rgPropVals[i].propid)))
				i++;
			OidInfo.infDatabase.rgSortSpecs[0].propid = rgPropVals[i].propid;
			//OidInfo.infDatabase.rgSortSpecs[0].propid = GLOBAL_SORT_KEY_FILETIME;
			break;

		case GLOBAL_SORT_KEY_FILETIME :      //  LPWSTR
		default : 
			while ((i<wPropsRead) && (LOWORD(GLOBAL_SORT_KEY_ULONG) != LOWORD(rgPropVals[i].propid)))
				i++;
			OidInfo.infDatabase.rgSortSpecs[0].propid = rgPropVals[i].propid;
			//OidInfo.infDatabase.rgSortSpecs[0].propid = GLOBAL_SORT_KEY_ULONG;
			break;    
	}

	OidInfo.infDatabase.dwFlags = CEDB_VALIDSORTSPEC;
	if (0==Primary_propid)
		OidInfo.infDatabase.wNumSortOrder =1;

	Perf_MarkBegin(MARK_DBINFO_SET);
	fTemp = CeSetDatabaseInfoEx(&VolGUID, DBOid, &(OidInfo.infDatabase));
	Perf_MarkEnd(MARK_DBINFO_SET);
	if (!fTemp)
		GENERIC_FAIL(CeSetDatabaseInfoEx);

	//  Reset the db Info. 
	OidInfo.infDatabase.rgSortSpecs[0].propid = Primary_propid;
	if (0==Primary_propid)
		OidInfo.infDatabase.wNumSortOrder =0;
	fTemp = CeSetDatabaseInfoEx(&VolGUID, DBOid, &(OidInfo.infDatabase));
	if (!fTemp)
		GENERIC_FAIL(CeSetDatabaseInfoEx);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);
	
	fRetVal=TRUE;

ErrorReturn :
	FREE(pbRecord);
	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_DeleteDatabase
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_DeleteDatabase(LPTSTR pszVolName, LPTSTR pszDBName, CEPROPID Primary_propid)
{
	BOOL fRetVal=FALSE;
	CEGUID VolGUID;
	CEOID DBOid=0;
	HANDLE hDB=INVALID_HANDLE_VALUE;
	BOOL fTemp=FALSE;

	//  Check params
	if ((!pszVolName) || (!_tcslen(pszVolName)) || (!pszDBName) || (!_tcslen(pszDBName)))
	{
		Debug(TEXT(">>> ERROR : Volume (=%s) or DBname (=%s) is missing. File=%s line=%d\r\n"), pszVolName, pszDBName, 
						_T(__FILE__), __LINE__);
		goto ErrorReturn;
	}

	CREATE_INVALIDGUID(&VolGUID);
	Debug(TEXT("Mounting Volume %s\n"), pszVolName);

	//  Mount the Database.
	if (!CeMountDBVol( &VolGUID, pszVolName, OPEN_EXISTING))
		GENERIC_FAIL(CeMountDBVol);

	//  Open the database
	hDB = CeOpenDatabaseEx(&VolGUID, 
							&DBOid, 
							pszDBName, 
							Primary_propid, 
							CEDB_AUTOINCREMENT,
							NULL);
	if (INVALID_HANDLE_VALUE == hDB)
		GENERIC_FAIL(CEOpenDataBaseEx);

	CLOSE_HANDLE(hDB);

	Perf_MarkBegin(MARK_DBDELETE);
	fTemp = CeDeleteDatabaseEx(&VolGUID, DBOid);
	Perf_MarkEnd(MARK_DBDELETE);

	if (!fTemp)
		GENERIC_FAIL(CeDeleteDatabaseEx);

	if (!CeUnmountDBVol(&VolGUID))
		GENERIC_FAIL(CeUnmountDBVol);
	CREATE_INVALIDGUID(&VolGUID);

	fRetVal=TRUE;

ErrorReturn :

	CLOSE_HANDLE(hDB);

	//  UnMount Volume
	if (FALSE == CHECK_INVALIDGUID(&VolGUID))
		CeUnmountDBVol(&VolGUID);
		
	return fRetVal;

}


///////////////////////////////////////////////////////////////////////////////
//
// Function: L_RunPerfScenario
//	Helper function to actually run the perf test
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_RunPerfScenario(LPTSTR pszVolName, 
							LPTSTR pszDBName, 
							DWORD dwNumRecords, 
							DWORD dwNumProps, 
							DWORD dwFlags, 
							CEPROPID SortPropId,
							LPCTSTR pszScenDesc, 
							DWORD cbBlob,
							DWORD ccString)
{
	BOOL    fRetVal=FALSE;
	TCHAR   pszDesc[1500]={0};  

	//  
	//  Register markers and measure WRITING
	//  
	_stprintf(pszDesc, _T("Write %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);
	
	// Just register what needs to be registered
	if (!Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_WriteDB(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags, SortPropId, cbBlob, ccString))
		goto ErrorReturn;

	//  
	//  Register markers and measure MODIFYING
	//  
	_stprintf(pszDesc, _T("Mdfy_Recs %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered
	if (!Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_ModifyAllRecords(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

#ifdef SUPER_PERF
	//  
	//  Register markers and measure DEL_PROPS
	//  
	_stprintf(pszDesc, _T("Del_Props %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered
	if (!Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_DelPropsAllRecords(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

	//  
	//  Register markers and measure ADD_PROPS
	//  
	_stprintf(pszDesc, _T("Add_Props %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered
	if (!Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_AddPropsToDB(g_szDbVolume, g_szDb, 6, SortPropId))
		goto ErrorReturn;

#endif

	//  
	//  Register markers and measure ADD_PROPS
	//  
	_stprintf(pszDesc, _T("Del_Sort_Prop %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered. If there's no SortPropId, no need to register a marker	
	if (SortPropId && !Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Del_Sort_PropAllRecords(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

	//  
	//  Register markers and measure ADD_PROPS
	//  
	_stprintf(pszDesc, _T("Add_Sort_Prop %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered. If there's no SortPropId, no need to register a marker	
	if (SortPropId && !Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Add_Sort_PropsToDB(g_szDbVolume, g_szDb, 1, SortPropId))
		goto ErrorReturn;
	
	//  Now, modify the database such that the sort prop has predictable data. 
	//  We won't know what to seek for otherwise. 
	if (!L_PrepareDBForSeek(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_next (with index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_NEXT, 0))
		goto ErrorReturn;

#ifdef SUPER_PERF
	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_Prev (witn index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (SortPropId && !Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_PREV, 0))
		goto ErrorReturn;

#endif

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_OID (with index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_OID, 0))
		goto ErrorReturn;

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_Value (with Index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (SortPropId && !(SortPropId & GLOBAL_SORT_KEY_FILETIME) && !Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_VALUE, 0))
		goto ErrorReturn;

#ifdef SUPER_PERF

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_next (w/o index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_NEXT, PERF_BIT_NO_INDEX))
		goto ErrorReturn;

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_Prev (w/o index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_PREV, PERF_BIT_NO_INDEX))
		goto ErrorReturn;

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_OID (w/o index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_OID, PERF_BIT_NO_INDEX))
		goto ErrorReturn;

	//  
	//  Register markers and measure SEEK
	//  
	_stprintf(pszDesc, _T("Seek_Value (w/o Index)%s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (SortPropId && !(SortPropId & GLOBAL_SORT_KEY_FILETIME) && !Perf_RegisterMark(MARK_REC_SEEK, _T("Test=CeSeekDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_Measure_DBSeek(g_szDbVolume, g_szDb, SortPropId, PERF_SEEK_VALUE, PERF_BIT_NO_INDEX))
		goto ErrorReturn;

#endif

	//  
	//  Register markers and measure Delete
	//  
	_stprintf(pszDesc, _T("Del_Recs %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_RECDELETE, _T("Test=CeDeleteRecord - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_DeleteAllRecords(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

	_stprintf(pszDesc, _T("Write %s"), pszScenDesc);
	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	// Just register what needs to be registered. If there's no SortPropId, no need to register a marker
	// If we specificy no measurement, don't bother to register the marker either.
	//if (SortPropId && !(dwFlags & PERF_BIT_NO_MEASURE) && !Perf_RegisterMark(MARK_RECWRITE, _T("Test=CeWriteRecord - %s"), pszDesc))
	//	GENERIC_FAIL(Perf_RegisterMark);
	
	if (!L_WriteDB(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags | PERF_BIT_NO_MEASURE, SortPropId, cbBlob, ccString))
		goto ErrorReturn;

	//  
	//  Register markers and measure WRITING
	//  
	_stprintf(pszDesc, _T("SetDBINfo %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_DBINFO_SET, _T("Test=CeSetDatabaseInfo - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_SetDbInfo(g_szDbVolume, g_szDb, SortPropId, 0))
		goto ErrorReturn;
	
	//  
	//  Register markers and measure DeleteDatabase
	//  
	_stprintf(pszDesc, _T("DeleteDatabase %s"), pszScenDesc);

	// Add the AVG macro for parsing
	_stprintf(pszDesc, _T("%s,time=%%AVG%%"), pszDesc);

	if (!Perf_RegisterMark(MARK_DBDELETE, _T("Test=CeDeleteDatabase - %s"), pszDesc))
		GENERIC_FAIL(Perf_RegisterMark);

	if (!L_DeleteDatabase(g_szDbVolume, g_szDb, SortPropId))
		goto ErrorReturn;

	fRetVal=TRUE;

ErrorReturn :
	return fRetVal;
}
						

						
///////////////////////////////////////////////////////////////////////////////
//
// Function: L_Measure_DBPerf
//	Performs the test run for one sort type.
//	This includes measuring CeWriteRecord and CeSeekDatabase. 
//	There are 4 scenarios under here. 
//
///////////////////////////////////////////////////////////////////////////////
BOOL L_Measure_DBPerf(LPCTSTR pszTestDesc, CEPROPID SortPropId)
{
	BOOL fRetVal=FALSE;
	DWORD dwNumRecords=0;
	DWORD dwNumProps=0;
	DWORD dwFlags=0;
	TCHAR pszDesc[512]={0};
	TCHAR rgszScenario[1024]={0};

	// Start monitoring CPU and memory
	Perf_StartSysMonitor(PERF_APP_NAME, SYS_MON_CPU | SYS_MON_MEM | SYS_MON_LOG, 
							g_dwPoolIntervalMs, g_dwCPUCalibrationMs);


	//  ================================================================
	//  SCENARIO 1 : 
	//  Compressed database.
	//  Many small records.
	//  ================================================================
	dwNumRecords = MANY_RECORDS;
	dwNumProps=SORT_PROPS_AVAILABLE;
	dwFlags=0;
	_stprintf(rgszScenario, _T("%s (%d recs)"), pszTestDesc, dwNumRecords);

	if (!L_RunPerfScenario(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags, SortPropId, rgszScenario, 0, 0))
		goto ErrorReturn;


	//  ================================================================
	//  SCENARIO 2 : 
	//  Un-Compressed database.
	//  Many small records
	//  ================================================================
	dwNumRecords = MANY_RECORDS;
	dwNumProps=SORT_PROPS_AVAILABLE;
	dwFlags=PERF_BIT_DB_UNCOMPRESSED;    
	_stprintf(rgszScenario, _T("%s Uncompr (%d recs)"), pszTestDesc, dwNumRecords);

	if (!L_RunPerfScenario(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags, SortPropId, rgszScenario, 0, 0))
		goto ErrorReturn;

	
	//  ================================================================
	//  SCENARIO 3 : 
	//  Compressed database.
	//  Few BIG records.
	//  ================================================================
	dwNumRecords = FEW_RECORDS; 
	dwNumProps=SORT_PROPS_AVAILABLE;
	dwFlags=0;
	_stprintf(rgszScenario, _T("%s (%d BIG recs)"), pszTestDesc, dwNumRecords);

	if (!L_RunPerfScenario(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags, SortPropId, rgszScenario, 16*1024, 0))
		goto ErrorReturn;


	//  ================================================================
	//  SCENARIO 4 : 
	//  Un-Compressed database.
	//  few BIG records
	//  ================================================================
	dwNumRecords = FEW_RECORDS; 
	dwNumProps=SORT_PROPS_AVAILABLE;
	dwFlags=PERF_BIT_DB_UNCOMPRESSED;
	_stprintf(rgszScenario, _T("%s Uncompr (%d BIG recs)"), pszTestDesc, dwNumRecords);

	if (!L_RunPerfScenario(g_szDbVolume, g_szDb, dwNumRecords, dwNumProps, dwFlags, SortPropId, rgszScenario, 16*1024, 0))
		goto ErrorReturn;
	

	fRetVal=TRUE;
	
ErrorReturn:
	Perf_StopSysMonitor();
	return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test Case: Tst_SortOnNothing
//	Measure DB performance on default sorting mechanism 
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SortOnNothing(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD dwRetVal=TPR_FAIL;
	DWORD dwThreadCount=0;

	COMMON_TUX_HEADER;

	if (!L_Measure_DBPerf(lpFTE->lpDescription, 0))    
		goto ErrorReturn;

	dwRetVal = TPR_PASS;

ErrorReturn :
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test Case: Tst_SortOnDWORD
//	Measure DB performance on DWORD sorting mechanism 
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SortOnDWORD(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD   dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	if (!L_Measure_DBPerf(lpFTE->lpDescription, GLOBAL_SORT_KEY_ULONG))
		goto ErrorReturn;

	dwRetVal = TPR_PASS;
	
ErrorReturn:
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test Case: Tst_SortOnString
//	Measure DB performance on String sorting mechanism 
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SortOnString(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD   dwRetVal=TPR_FAIL;

	COMMON_TUX_HEADER;

	if (!L_Measure_DBPerf(lpFTE->lpDescription, GLOBAL_SORT_KEY_LPWSTR))
		goto ErrorReturn;

	dwRetVal = TPR_PASS;

ErrorReturn:
	return dwRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
// Test Case: Tst_SortOnFiletime
//	Measure DB performance on Time sorting mechanism 
//
///////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SortOnFiletime(UINT uMsg, TPPARAM tpParam, const LPFUNCTION_TABLE_ENTRY lpFTE )
{
	UNREFERENCED_PARAMETER(lpFTE);
	DWORD   dwRetVal=TPR_FAIL;
	DWORD   dwThreadCount=0;

	COMMON_TUX_HEADER;

	if (!L_Measure_DBPerf(lpFTE->lpDescription, GLOBAL_SORT_KEY_FILETIME))
		goto ErrorReturn;

	dwRetVal = TPR_PASS;

ErrorReturn:
	return dwRetVal;
}








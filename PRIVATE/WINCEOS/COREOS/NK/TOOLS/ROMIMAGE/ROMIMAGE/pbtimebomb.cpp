//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifdef PBTIMEBOMB
//////////////////////////////////////////////////////////////////////////////////////////////
//  PB timebomb.  This code compares the MPC in the resource of the current module with		//
//  the known PB Pro MPC in the PB devshl.dll resource.										//
//  If PB Pro, then IsRomImageEnabled returns												//
//  TRUE. Any MPC that doesn't match the PB Pro is considered a Trial MPC.  If it is a		//
//  Trial, the number of days remaining in the Trial must be greater than 0.  If equal to or//
//  less than 0, the IsRomImageEnabled function will return FALSE, otherwise it will return//
//  TRUE (For the sixty dayes).																//
//////////////////////////////////////////////////////////////////////////////////////////////



#include "pbtimebomb.h"
#include <stdio.h>

#define IDS_ERR_NO_PBCH					61255


// This function is for calculating the number of days left in PB Trial
int GetDaysLeft(void)
{
    int nRetVal = 0;
    TCHAR tszInstallDir[255]={0};
    DWORD dwSize = sizeof(tszInstallDir);
    WIN32_FIND_DATA wfd = {0};
    HANDLE hFile = INVALID_HANDLE_VALUE;

    //  get the install DIR from the registry
    HKEY hKey = NULL;
    LONG bRetVal = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                   _T("SOFTWARE\\Microsoft\\Platform Builder\\" PBVERSION "\\Directories"),
                                   0,
                                   KEY_READ,
                                   &hKey
                                   );
    if ((bRetVal != ERROR_SUCCESS) || (hKey == NULL))
        goto done;


    bRetVal = RegQueryValueEx( hKey,
                               _T("IDE Install Dir"),
                               NULL,
                               NULL,
                               (BYTE*)tszInstallDir,
                               &dwSize
                               );
    if (bRetVal != ERROR_SUCCESS)
        goto done;

    //  we have the install dir, get the "etkremov.inf" timestamp
    _tcscat(tszInstallDir, _T("etkremov.inf"));

    hFile = FindFirstFile( tszInstallDir, &wfd );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        //  we didn't find the file
        goto done;
    }

    {
        FILETIME ftTimeStamp;

        FileTimeToLocalFileTime(&(wfd.ftCreationTime), &ftTimeStamp);

        //  we should have the timestamp now.
        //  Compare with localtime
        SYSTEMTIME stLocalTime;
        FILETIME ftLocalTime;

        GetLocalTime(&stLocalTime);

        SystemTimeToFileTime( &stLocalTime, &ftLocalTime);

        //  This is the goofiest thing I have ever done in my life.
        ULARGE_INTEGER ulLocalTime, ulTimeStamp, ulDiff, ulTrialTime;
        ulLocalTime.LowPart = ftLocalTime.dwLowDateTime;
        ulLocalTime.HighPart = ftLocalTime.dwHighDateTime;

        ulTimeStamp.LowPart = ftTimeStamp.dwLowDateTime;
        ulTimeStamp.HighPart = ftTimeStamp.dwHighDateTime;

		if (ulLocalTime.QuadPart <= ulTimeStamp.QuadPart)
		{
			ulDiff.QuadPart = 0;
		}
		else
		{
	        ulDiff.QuadPart = ulLocalTime.QuadPart - ulTimeStamp.QuadPart;
		}

		// Figure out the number of 100-nanoseconds in the trial day period
		ulTrialTime.QuadPart = TRIALDAYS * 24; // hours
		ulTrialTime.QuadPart = ulTrialTime.QuadPart * 60; // minutes
		ulTrialTime.QuadPart = ulTrialTime.QuadPart * 60; // seconds
		ulTrialTime.QuadPart = ulTrialTime.QuadPart * 1000 * 1000 * 10; // 100-nanoseconds

//		ulTrialTime.QuadPart = ulLocalTime.QuadPart + ulTrialTime.QuadPart;

        if (ulDiff.QuadPart > ulTrialTime.QuadPart)
		{
				nRetVal = -1;
		}
		else
		{
			ulDiff.QuadPart = ulTrialTime.QuadPart - ulDiff.QuadPart;
			// This is nanoseconds, calculate days
			ulDiff.QuadPart = ulDiff.QuadPart / 1000;
			ulDiff.QuadPart = ulDiff.QuadPart / 1000;
			ulDiff.QuadPart = ulDiff.QuadPart / 10; // seconds
			ulDiff.QuadPart = ulDiff.QuadPart / 60; // minutes
			ulDiff.QuadPart = ulDiff.QuadPart / 60; // hours
			ulDiff.QuadPart = ulDiff.QuadPart / 24; // days
			nRetVal = (int) ulDiff.QuadPart;
		}

    }

done:
    if (hKey != NULL)
    {
        RegCloseKey(hKey);
        hKey = NULL;
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        FindClose(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    return nRetVal;

}


BOOL GetMPCFromPB(LPTSTR szMPC)
{

    TCHAR tszInstallDir[255]={0};
    DWORD dwSize = sizeof(tszInstallDir);
    WIN32_FIND_DATA wfd = {0};
    HANDLE hFile = INVALID_HANDLE_VALUE;

    //  get the install DIR from the registry
    HKEY hKey = NULL;
    LONG bRetVal = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                   _T("SOFTWARE\\Microsoft\\Platform Builder\\" PBVERSION "\\Directories"),
                                   0,
                                   KEY_READ,
                                   &hKey
                                   );
    if ((bRetVal != ERROR_SUCCESS) || (hKey == NULL))
        return FALSE;


    bRetVal = RegQueryValueEx( hKey,
                               _T("IDE Install Dir"),
                               NULL,
                               NULL,
                               (BYTE*)tszInstallDir,
                               &dwSize
                               );
    if (bRetVal != ERROR_SUCCESS)
        return FALSE;

    //  we have the install dir, get the "etkremov.inf" timestamp
    _tcscat(tszInstallDir, _T("\\cepb\\bin\\devshl.dll"));

    if (hKey != NULL)
    {
        RegCloseKey(hKey);
        hKey = NULL;
    }

    hFile = FindFirstFile( tszInstallDir, &wfd );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        //  we didn't find the file
        return FALSE;
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        FindClose(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

	HINSTANCE hinst;


	//Test
    //_tcscpy(tszInstallDir, _T("d:\\pb\\dev\\bind\\devshld.dll"));


	hinst = LoadLibraryEx(tszInstallDir, NULL, 
		DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);


    TCHAR tszMPCMKD[10]={0};

	if(LoadString(hinst,IDS_ERR_NO_PBCH,tszMPCMKD,sizeof(tszMPCMKD)) <=0)
		return FALSE;

	if(hinst!=NULL) 
		FreeLibrary(hinst);



    _tcscpy(szMPC, tszMPCMKD);


	DWORD dwProMPC = strtoul( szMPC, NULL , 16 );
	_stprintf(szMPC,"%d",dwProMPC);
	dwProMPC = strtoul( szMPC, NULL , 8 );
	_stprintf(szMPC,"%5.5d",dwProMPC);


	
	return TRUE;
}




BOOL IsProMPC(){

	//KAS 3/28/00  Check the resource to see if this PB is Pro or EVAL
	HRSRC hRsrc;
	HGLOBAL hGbl;
	char szRpcOld[RPC_SIZE+1]={0};
	char szMPC[10]={0};

	//Check for Magic Reg Key.
    //  get the install DIR from the registry
    HKEY hKey = NULL;
    LONG bRetVal = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                   _T("SOFTWARE\\Microsoft\\MagicProPB"),
                                   0,
                                   KEY_READ,
                                   &hKey
                                   );
	//If the magic key was found, this will be pro.
    if (hKey != NULL){
        RegCloseKey(hKey);
        hKey = NULL;
        return TRUE;
	}

	//Get the Pro MPC from devshl.dll
	if (GetMPCFromPB(szMPC) == FALSE)
		return FALSE;



	HINSTANCE hinst = NULL;
	
	//Test
	//hinst = LoadLibraryEx(_T("d:\\pb\\dev\\bind\\ided\\devcppd.pkg"), NULL, 
	//	DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);



	//Find the resource that has the PID in it.
	hRsrc = FindResource( hinst, MAKEINTRESOURCE(PID_RES_NUM), MAKEINTRESOURCE(PID_RES_TYPE) );

	//Make sure the resource is that right size before reading
	if( PID_BUFFER_SIZE <= SizeofResource(hinst, hRsrc) )
	{
		/** FILE READ **/
		hGbl = LoadResource(hinst,hRsrc);
		if( NULL != hGbl )
		{
			BYTE* rgchBuf = (BYTE*)LockResource( hGbl );
			if( NULL != rgchBuf )
			{

				//Grap the MPC portion of the PID.
				_tcsncpy(szRpcOld, (char *) (rgchBuf + cbCDEncrypt), RPC_SIZE );
				szRpcOld[RPC_SIZE] = (TCHAR)0;

				//Compare the MPC portion of the PID with the PB PRO MPC
				if( 0 == _tcscmp(szMPC,szRpcOld)  )
				{
					//The MPC stamped in the exe is for PB PRO
					return TRUE;
				}
			}
		}
	}

	return FALSE;

}



BOOL IsRomImageEnabled(){


	if(IsProMPC() )
		return TRUE;

	if(GetDaysLeft() <= 0){
		return FALSE;
	}

	return TRUE;

}

#endif // PBTIMEBOMB



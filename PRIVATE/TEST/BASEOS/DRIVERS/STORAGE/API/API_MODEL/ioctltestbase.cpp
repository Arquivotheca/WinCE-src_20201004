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
#include "main.h"
#include <auto_xxx.hxx>
#include <ceddk.h>
#include <cebus.h>

#define NUM_IN_OUT_BUFFER_TYPES 4
#define NUM_BYTES_RETURNED_VALUES 2
#define NUM_OVERLAPPED_VALUES 1
#define NUM_ALIGNMENT_TYPES 1
#define NUM_BUFFER_TYPES 1
TCHAR* g_InOutBufferTypes [NUM_IN_OUT_BUFFER_TYPES] = {_T("ALPHA"),_T("NULL"),_T("ZERO_BUFFER"),_T("ONE_BUFFER")};


TCHAR* g_BytesReturned [NUM_BYTES_RETURNED_VALUES] = {_T("size_of_buffer"),_T("NULL")};

//TCHAR* g_AlignmentValues[NUM_ALIGNMENT_TYPES] = {_T("Page"),_T("DWORD"),_T("WORD"),_T("BYTE")};
//TCHAR* g_BufferTypes [NUM_BUFFER_TYPES] = {_T("stack"),_T("global"),_T("heap"),_T("r_virtual"),_T("re_virtual"),_T("rw_virtual"),_T("rwx_virtual")};
//TODO: change alignment types and buffer types, and make sure GetBuffer function uses them properly
TCHAR* g_AlignmentValues[NUM_ALIGNMENT_TYPES] = {_T("Page")};
TCHAR* g_BufferTypes [NUM_BUFFER_TYPES] = {_T("stack")};


BOOL UnloadDriver(LPTSTR sc_pszSearchParam,DEVMGR_DEVICE_INFORMATION *di,DeviceSearchType searchType);
BOOL LoadDriver(DEVMGR_DEVICE_INFORMATION *di);
BOOL GetParentDevice(LPTSTR sc_pszSearchParam,DEVMGR_DEVICE_INFORMATION *parent_di);

/*This function loads the basic parameters needed to run a DeviceIOControl.
Specifically, it enumerates all testing combinations for inBuf,outBuf,inBufSize,
outBufSize,lpBytesReturned,the alignment of the in and out buffers, and where the
memory for the in and out buffers is allocated from.  Users should NOT change this function,
but should override LoadCustomParameters in their own derived class.  In their derived function,
they can remove parameters from this list if needed (probably not necessary), or add new ones.
*/
BOOL DRIVER_TEST::LoadParameters(void)
{
	BOOL bRet = TRUE;

	//add possible lpInBuffer and lpOutBuffer values
	PGROUP tempGroup = new GROUP(PARAM_lpInBuffer);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, _T("FAILED go allocate new GROUP for lpInBuffer\n"));
		return FALSE;
	}
	PGROUP tempGroup1 = new  GROUP(PARAM_lpOutBuffer);
	if(!tempGroup1)
	{
		m_pKato->Log(LOG_DETAIL, _T("FAILED go allocate new GROUP for lpOutBuffer\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_IN_OUT_BUFFER_TYPES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_lpInBuffer,g_InOutBufferTypes[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
		tempParam = new PARAMETER(PARAM_lpOutBuffer,g_InOutBufferTypes[i]);
		tempGroup1->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	paramGroupList.AddTail(tempGroup1);
	//add possible lpBytesReturned values
	tempGroup = new GROUP(PARAM_IpBytesReturned);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, _T("FAILED go allocate new GROUP for lpBytesReturned\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_BYTES_RETURNED_VALUES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_IpBytesReturned,g_BytesReturned[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	//add possible BufferType values
	tempGroup = new GROUP(PARAM_BufferType);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, _T("FAILED go allocate new GROUP for BufferType\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_BUFFER_TYPES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_BufferType,g_BufferTypes[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);

	//add possible Alignment values
	tempGroup = new GROUP(PARAM_Alignment);
	if(!tempGroup)
	{
		m_pKato->Log(LOG_DETAIL, _T("FAILED go allocate new GROUP for Alignment\n"));
		return FALSE;
	}
	for(int i = 0; i < NUM_ALIGNMENT_TYPES; i++)
	{
		PPARAMETER tempParam = new PARAMETER(PARAM_Alignment,g_AlignmentValues[i]);
		tempGroup->m_ParameterList.AddTail(tempParam);
	}
	paramGroupList.AddTail(tempGroup);
	
	return LoadCustomParameters();
	
}

BOOL DRIVER_TEST::Start(void)
{
	/*DEVMGR_DEVICE_INFORMATION di; //information about Host Controller devic
	//init the structure
	memset(&di, 0, sizeof(di));
	di.dwSize = sizeof(&di);

	

	LPTSTR searchString;
	BOOL lErr;
	DEVMGR_DEVICE_INFORMATION parent_di = {0};
	if(_tcscmp(g_szProfile,_T("HDProfile")) == 0 || _tcscmp(g_szProfile,_T("PCMCIA")) == 0)
	{
		searchString = _T("IDE*");
		GetParentDevice(searchString, &parent_di);
		searchString = _tcschr(parent_di.szBusName,'\\') + 1;
		lErr = UnloadDriver(searchString,&di,DeviceSearchByBusName);
	}
		
	else
	{
		searchString = _T("UNKNOWN");
	}

	
	//BOOL lErr= UnloadDriver(searchString,&di);
	//SetRegistryInfo();
	LoadDriver(&di);
	Sleep(3000);			*/	
	PGROUP cur = paramGroupList.head;
	m_dwTotalTests = 1;
	while(cur != NULL)
	{
		m_dwTotalTests *= cur->m_ParameterList.Items;
		cur = cur->next;
	}
	BOOL	bRet = LoopFunction(paramGroupList.head);
	return bRet;
}
BOOL DRIVER_TEST::Fill(void)
{
	PPARAMETER tParam = NULL;
	PPARAMETER BufferType = NULL;
	tParam = loopList.FindParameterByName(PARAM_lpInBuffer);
	BufferType = loopList.FindParameterByName(PARAM_BufferType);
	inData.lpInBuffer = GetBuffer(tParam->value,BufferType->value,inData.nActualInBufferSize);
	tParam = loopList.FindParameterByName(PARAM_lpOutBuffer);
	inData.lpOutBuffer= GetBuffer(tParam->value,BufferType->value,inData.nActualOutBufferSize);
	tParam = loopList.FindParameterByName(PARAM_IpBytesReturned); 	
	inData.lpBytesReturned = (LPDWORD)GetBuffer(tParam->value,_T("stack"), inData.nActualOutBufferSize);
	inData.lpOverLapped = NULL;
	tParam = loopList.FindParameterByName(PARAM_nInBufferSize);
	inData.nInBufferSize = GetDWORD(tParam->value,inData.nActualInBufferSize);
	tParam = loopList.FindParameterByName(PARAM_nOutBufferSize);
	inData.nOutBufferSize = GetDWORD(tParam->value,inData.nActualOutBufferSize);
	return TRUE;
}
BOOL DRIVER_TEST::LoopFunction(PGROUP curGroup)
{
	BOOL bRet = TRUE;
	if(curGroup == NULL)
	{
		loopCounter++;

		CustomFill();
		cpyData = inData;
		m_pKato->Log(LOG_DETAIL, _T("Running IOCTL: %d of %d\n"),loopCounter,m_dwTotalTests);
		if(g_dwBreakAtTestCase && g_dwBreakAtTestCase == loopCounter)
			DebugBreak();
		//loopList.Print();
		bRet &= RunIOCTL();	
		bRet &= Validate();
		Cleanup();
		if(!bRet)
		{
		  if(g_dwBreakOnFailure)
		  {
		    DebugBreak();    
		  }
			m_pKato->Log(LOG_DETAIL, _T("FAILED IOCTL: %d\n"),loopCounter);
			loopList.Print();
		}
		else
		{
			m_pKato->Log(LOG_DETAIL, _T("PASSED IOCTL: %d\n"),loopCounter);
		}
		return bRet;
	}
	for(int i = 0; i < curGroup->m_ParameterList.Items; i++)
	{	
		PPARAMETER tParam = curGroup->m_ParameterList.GetItem(i);
		PPARAMETER tempParam = new PARAMETER(tParam->paramtype,tParam->value);
		loopList.AddTail(tempParam);
		bRet &= LoopFunction(curGroup->next);
		PPARAMETER temp = loopList.RemoveLast();
		if(temp)
		{
			delete temp;
			temp = NULL;
		}
	}
	return bRet;
}

void DRIVER_TEST::Cleanup(void)
{
	if(inData.lpInBuffer)
	{
		delete[] inData.lpInBuffer;
		inData.lpInBuffer = NULL;
	}
	if(inData.lpOutBuffer)
	{
		delete[] inData.lpOutBuffer;
		inData.lpOutBuffer = NULL;
	}
	if(cpyData.lpInBuffer)
	{
		delete[] cpyData.lpInBuffer;
		cpyData.lpInBuffer = NULL;
	}
	if(cpyData.lpOutBuffer)
	{
		delete[] cpyData.lpOutBuffer;
		cpyData.lpOutBuffer = NULL;
	}
	if(inData.lpBytesReturned)
	{
		delete inData.lpBytesReturned;
		inData.lpBytesReturned = NULL;
	}
	if(cpyData.lpBytesReturned)
	{
		delete cpyData.lpBytesReturned;
		cpyData.lpBytesReturned = NULL;
	}
	CustomCleanup();
}
BOOL DRIVER_TEST::Init(void)
{
	BOOL bRet = GetDeviceInfo();
	return bRet;
}

BOOL DRIVER_TEST::GetDeviceInfo(void)
{
	m_DeviceInfo.OSVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetSystemInfo(&m_DeviceInfo.SystemInfo);
	if(!GetVersionEx(&m_DeviceInfo.OSVersion))
	{
		m_pKato->Log(LOG_DETAIL,_T("Failed to get version info with GetVersionEX\n"));
		return FALSE;
	}
	return TRUE;
}

_IOCTL_DATA::_IOCTL_DATA(void)
{
	dwIoControlCode = 0;
	lpInBuffer=NULL;
	nActualInBufferSize = 0;
	nInBufferSize = 0;
	lpOutBuffer = NULL;
	nActualOutBufferSize = 0;
	nOutBufferSize = 0;
	lpBytesReturned = NULL;
	lpOverLapped = NULL;
	LastError = rand();
	RetVal = (BOOL)(rand() % 1);
}

_IOCTL_DATA::~_IOCTL_DATA()
{
	if(lpInBuffer)
	{
		delete[] lpInBuffer;
		lpInBuffer = NULL;
	}
	if(lpOutBuffer)
	{
		delete[] lpOutBuffer;
		lpOutBuffer = NULL;
	}
	if(lpBytesReturned)
		delete lpBytesReturned;
}


_IOCTL_DATA& _IOCTL_DATA::operator=(const _IOCTL_DATA &copy)
{
	//cleanup
	if(lpInBuffer)
	{
		delete lpInBuffer;
		lpInBuffer = NULL;
	}
	if(lpOutBuffer)
	{
		delete lpOutBuffer;
		lpOutBuffer = NULL;
	}
	if(lpBytesReturned)
	{
		delete lpBytesReturned;
		lpBytesReturned = NULL;
	}

	dwIoControlCode = copy.dwIoControlCode;

	nActualInBufferSize = copy.nActualInBufferSize;
	
	//copy lpInBuffer
	if(copy.lpInBuffer)
	{
		
		lpInBuffer = new TCHAR[copy.nActualInBufferSize];
		if(lpInBuffer)
			memcpy(lpInBuffer,copy.lpInBuffer,copy.nActualInBufferSize);
	}
	else
		lpInBuffer = NULL;

	nInBufferSize = copy.nInBufferSize;
	
	nActualOutBufferSize = copy.nActualOutBufferSize;
	//copy lpOutBuffer
	if(copy.lpInBuffer)
	{
		lpOutBuffer = new TCHAR[copy.nActualOutBufferSize];
		if(lpOutBuffer)
			memcpy(lpOutBuffer,copy.lpOutBuffer,copy.nActualOutBufferSize);
	}
	else
		lpInBuffer = NULL;

	nOutBufferSize = copy.nOutBufferSize;
	if(copy.lpBytesReturned)
	{
		lpBytesReturned = new DWORD;
		if(lpBytesReturned)
			memcpy(lpBytesReturned,copy.lpBytesReturned,sizeof(DWORD));
	}
	else
		lpBytesReturned = NULL;
	//lpBytesReturned = copy.lpBytesReturned;
	lpOverLapped = copy.lpOverLapped;
	RetVal = copy.RetVal;
	LastError = copy.LastError;
	return *this;
}

_IOCTL_DATA::_IOCTL_DATA(const _IOCTL_DATA &copy)
{
	//cleanup
	if(lpInBuffer)
	{
		delete lpInBuffer;
		lpInBuffer = NULL;
	}
	if(lpOutBuffer)
	{
		delete lpOutBuffer;
		lpOutBuffer = NULL;
	}
	if(lpBytesReturned)
	{
		delete lpBytesReturned;
		lpBytesReturned = NULL;
	}

	dwIoControlCode = copy.dwIoControlCode;

	nActualInBufferSize = copy.nActualInBufferSize;

	//copy lpInBuffer
	lpInBuffer = new TCHAR[copy.nActualInBufferSize];
	if(lpInBuffer)
		memcpy(lpInBuffer,copy.lpInBuffer,copy.nActualInBufferSize);
	nInBufferSize = copy.nInBufferSize;
	
	nActualOutBufferSize = copy.nActualOutBufferSize;
	//copy lpOutBuffer
	lpOutBuffer = new TCHAR[copy.nActualOutBufferSize];
	if(lpOutBuffer)
		memcpy(lpOutBuffer,copy.lpOutBuffer,copy.nActualOutBufferSize);
	
	nOutBufferSize = copy.nOutBufferSize;
	if(copy.lpBytesReturned)
	{
		lpBytesReturned = new DWORD;
		if(lpBytesReturned)
			memcpy(lpBytesReturned,copy.lpBytesReturned,sizeof(DWORD));
	}
	else
		lpBytesReturned = NULL;
	//lpBytesReturned = copy.lpBytesReturned;
	lpOverLapped = copy.lpOverLapped;
	RetVal = copy.RetVal;
	LastError = copy.LastError;
	
}

BOOL UnloadDriver(LPTSTR sc_pszSearchParam,DEVMGR_DEVICE_INFORMATION *di,DeviceSearchType searchType)
{
	HANDLE hSearchDevice = NULL;	//handle to device if found
	BOOL rVal = false; //return value

	 memset(di, 0, sizeof(*di));
	 di->dwSize = sizeof(*di);
	hSearchDevice = FindFirstDevice(searchType, sc_pszSearchParam,  di);
	if(hSearchDevice == NULL)
	{
		rVal = true;
	}
	else
	{
	  HANDLE hParentDevice = di->hParentDevice;
	  DEVMGR_DEVICE_INFORMATION parent_di;
	  memset(&parent_di, 0, sizeof(parent_di));
	  parent_di.dwSize = sizeof(parent_di);
	  if(!GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di))
	   {
	       	rVal = true;
	   }
	   else
	   {
	       //open a handle to the parent device
	      LPCTSTR pszParentBusName = parent_di.szBusName;
    
	      ce::auto_hfile hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (!hfParent.valid()) {
            NKDbgPrintfW(_T("CreateFile('%s') failed %d\r\n"), pszParentBusName, GetLastError());
            rVal = true;
        } 
        else
        {
              bool fUnload = true;

              LPCTSTR pszBusName = di->szBusName;
              BOOL fSuccess = DeviceIoControl(hfParent, fUnload ? IOCTL_BUS_DEACTIVATE_CHILD : IOCTL_BUS_ACTIVATE_CHILD,
                  (PVOID) pszBusName, (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), NULL, 0, NULL, NULL);
              if(!fSuccess) {
                  DEBUGMSG(TRUE, (_T("%s on '%s' failed %d\r\n"),
                      fUnload ? L"IOCTL_BUS_DEACTIVATE_CHILD" : L"IOCTL_BUS_ACTIVATE_CHILD",
                      pszBusName, GetLastError()));
				  //found a device that matches the string above and deactivated it
                  rVal = true;
              }
        }
	   }
	}
	FindClose(hSearchDevice);
	return rVal;
}

BOOL LoadDriver(DEVMGR_DEVICE_INFORMATION *di)
{
/*	
	HANDLE ActiveDevice = ActivateDevice(di->szDeviceKey,0);
		if(ActiveDevice == NULL)
			rVal = false;
		else
			rVal = true;


	return rVal;*/
	BOOL rVal = false; //return value
	
      HANDLE hParentDevice = di->hParentDevice;
	  DEVMGR_DEVICE_INFORMATION parent_di;
	  memset(&parent_di, 0, sizeof(parent_di));
	  parent_di.dwSize = sizeof(parent_di);
	  if(!GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di))
	  {
	       rVal = true;
	  }
	  else
	  {
			//open a handle to the parent device
			LPCTSTR pszParentBusName = parent_di.szBusName;
    
			ce::auto_hfile hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (!hfParent.valid()) {
				NKDbgPrintfW(_T("CreateFile('%s') failed %d\r\n"), pszParentBusName, GetLastError());
				rVal = true;
			} 
			else
			{
				bool fUnload = false;

				LPCTSTR pszBusName = di->szBusName;
				BOOL fSuccess = DeviceIoControl(hfParent, fUnload ? IOCTL_BUS_DEACTIVATE_CHILD : IOCTL_BUS_ACTIVATE_CHILD,
					  (PVOID) pszBusName, (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), NULL, 0, NULL, NULL);
				if(!fSuccess) {
					DEBUGMSG(TRUE, (_T("%s on '%s' failed %d\r\n"),
                    fUnload ? L"IOCTL_BUS_DEACTIVATE_CHILD" : L"IOCTL_BUS_ACTIVATE_CHILD",
                    pszBusName, GetLastError()));
					//found a device that matches the string above and deactivated it
                 rVal = true;
				}
			}
	  }
	  return rVal;
}

BOOL GetParentDevice(LPTSTR sc_pszSearchParam,DEVMGR_DEVICE_INFORMATION *parent_di)
{
	HANDLE hSearchDevice = NULL;	//handle to device if found
	BOOL rVal = TRUE; //return value
	memset(parent_di, 0, sizeof(*parent_di));
	 parent_di->dwSize = sizeof(*parent_di);
	
	DEVMGR_DEVICE_INFORMATION di = {0};
	 di.dwSize = sizeof(di);
	 
	 hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, sc_pszSearchParam,  &di);
	if(hSearchDevice == NULL)
	{
		rVal = FALSE;
	}
	else
	{
	  HANDLE hParentDevice = di.hParentDevice;
	   if(!GetDeviceInformationByDeviceHandle(hParentDevice,parent_di))
	   {
	       	rVal = FALSE;
	   }
	   else
	   {
	       rVal = TRUE;
	   }
	}
	return rVal;
}


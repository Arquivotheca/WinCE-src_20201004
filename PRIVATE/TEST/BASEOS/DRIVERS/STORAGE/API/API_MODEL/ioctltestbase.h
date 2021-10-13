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

#ifndef _IOCTL_TEST_BASE_H
#define _IOCTL_TEST_BASE_H

#include "main.h"
#include "parameterfuncs.h"
#include "Group.h"

#include <Winbase.h>

typedef struct _IOCTL_DATA
{
	_IOCTL_DATA(const _IOCTL_DATA &copy);
	_IOCTL_DATA(void);
	~_IOCTL_DATA();
	_IOCTL_DATA& operator=(const _IOCTL_DATA &copy);
	DWORD        dwIoControlCode;	//IOCTL
	LPVOID       lpInBuffer;
	DWORD        nActualInBufferSize;
	DWORD        nInBufferSize;
	LPVOID       lpOutBuffer;
	DWORD        nActualOutBufferSize;
	DWORD        nOutBufferSize;
	LPVOID        lpBytesReturned;
	LPOVERLAPPED lpOverLapped;
	BOOL	     RetVal;	//return value of IOCTL
	DWORD	     LastError; //value of GetLastError();
}IOCTL_DATA,*PIOCTL_DATA;

typedef struct _IOCTL_INPUT
{
	TCHAR* lpInBuffer;
	TCHAR* lpInBufferSize;
	TCHAR* lpOutBuffer;
	TCHAR* lpOutBufferSize;
	TCHAR* lpBytesReturned;
	TCHAR* lpOverLapped;
	TCHAR* lpBufferType;
	TCHAR* lpAlignmentType;
}IOCTL_INPUT,*PIOCTL_INPUT;

//contains data about the device we're running on
typedef struct _DEVICE_INFO
{
	SYSTEM_INFO	  SystemInfo;
	OSVERSIONINFO OSVersion; //which build
}DEVICE_INFO,*PDEVICE_INFO;


typedef class _DRIVER_TEST
{
protected:
	HANDLE       m_hDevice;	//handle to driver
	GROUP_LIST paramGroupList;
	IOCTL_DATA   inData;	//data sent to IOCTL
	IOCTL_DATA   cpyData;	//data received from IOCTL
	CKato		 *m_pKato;	//logger
	DEVICE_INFO	m_DeviceInfo;
	IOCTL_INPUT m_IoctlInput;
	PARAMETER_LIST loopList;
	DWORD	loopCounter;
	DWORD m_dwTotalTests;
	DWORD dwDeviceType;

public:
	_DRIVER_TEST(CKato *pKato){m_pKato = pKato;loopCounter=0;m_dwTotalTests = 0;}
	_DRIVER_TEST(void){m_pKato = NULL;loopCounter=0;m_dwTotalTests = 0;}
	//virtual BOOL FillInData(void) = 0;  //might not need this
	BOOL Init();	  //function to call before Start()
	BOOL Start();    //starts the test
	BOOL LoadParameters(void);
	virtual BOOL LoadCustomParameters(void) = 0;
	BOOL GetDeviceInfo(void);
	void Cleanup(void);
	virtual BOOL RunIOCTL() = 0;	  //runs the IOCTL
	virtual BOOL Validate() = 0;  //function to validate that it ran properly
	BOOL LoopFunction(PGROUP curGroup);
	BOOL Fill(void);
	virtual BOOL CustomFill(void)=0;
	virtual BOOL CustomCleanup(void) = 0;
	DWORD GetDWORD(const TCHAR* value,DWORD dwBufferSize);
	TCHAR* GetBuffer(const TCHAR* value,const TCHAR* BufferType,DWORD & dwBufferSize);


}DRIVER_TEST,*PDRIVER_TEST;

#endif





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

#ifndef _IOCTL_DISK_READ_H
#define _IOCTL_DISK_READ_H
#include "main.h"
#include "parameterfuncs.h"
#include "IoctlTestBase.h"

/*typedef struct _IOCTL_DATA
{
	DWORD        dwIoControlCode;	//IOCTL
	LPVOID       lpInBuffer;
	DWORD        nInBufferSize;
	LPVOID       lpOutBuffer;
	DWORD        nOutBufferSize;
	DWORD        lpBytesReturned;
	LPOVERLAPPED lpOverLapped;
	BOOL	     RetVal;	//return value of IOCTL
	DWORD	     LastError; //value of GetLastError();
}IOCTL_DATA,*PIOCTL_DATA;*/


typedef class _IOCTL_DISK_READ_TEST : public  DRIVER_TEST
{
	//HANDLE       hDevice;	//handle to driver
	//IOCTL_DATA   inData;	//data sent to IOCTL
	//IOCTL_DATA   outData;	//data received from IOCTL
	//BOOL Init();	  //function to call before run()
	//BOOL Start();    //starts the test
	virtual BOOL CustomFill(void);
	virtual BOOL LoadCustomParameters(void);
	DWORD m_dwNumSGBufs;
	DWORD m_dwNumSectors;
	DWORD m_dwStartSector;
	//DWORD m_dwSgBufferSize;
	//TCHAR* m_sgREQ;
	TCHAR* m_dataBuffer;
	DWORD m_dwDataBufferSize;
	//IOCTL_DATA writeData;
public:
	_IOCTL_DISK_READ_TEST(void){m_pKato = NULL;m_hDevice = NULL;dwDeviceType = 0;inData.dwIoControlCode = IOCTL_DISK_READ;m_dataBuffer = NULL;}
	_IOCTL_DISK_READ_TEST(HANDLE Handle,CKato *pKato,DWORD deviceType){m_pKato = pKato;m_hDevice = Handle;dwDeviceType = deviceType;inData.dwIoControlCode = IOCTL_DISK_READ;m_dataBuffer = NULL;}
	~_IOCTL_DISK_READ_TEST(){if(m_dataBuffer)delete m_dataBuffer;m_dataBuffer = NULL;};
private:
	virtual BOOL RunIOCTL();	  //runs the IOCTL
	virtual BOOL Validate();  //function to validate that it ran properly
	virtual BOOL CustomCleanup();
	BOOL ATAPIValidate(void);
}IOCTL_DISK_READ_TEST,*PIOCTL_DISK_READ_TEST;
#endif

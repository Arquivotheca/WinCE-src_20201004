//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//		CUIOAdapter.h
//
//  Class defination for CUIOAdapter.
//  This header file has definations of methods to 
//  + Open file handle to NDISUIO
//  + Associate with any given miniport adapter
//  + Send data to it
//  + Recv data from it.
//  + Query OID to it.
//  + Set OID to it.
//  + Query binding to it.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __UIO_ADAPTER__
#define __UIO_ADAPTER__

#include <windows.h>
#include <tchar.h>
#include <ntddndis.h>
#include <nuiouser.h>
#include <intsafe.h>

#ifndef NUIO_MAC_ADDR_LEN
#define NUIO_MAC_ADDR_LEN (6)
#endif

// LENGTHs are in bytes.
#define UIO_BINDING_DEV_NAME_BUFF_LEN		(256)
#define UIO_BINDING_DEV_DESC_BUFF_LEN		(256)

//typedef enum { READ_ACCESS=0, WRITE_ACCESS } tEAccess;

typedef struct _tsUIODeviceDesc
{
	TCHAR tszDeviceName[UIO_BINDING_DEV_NAME_BUFF_LEN/sizeof(TCHAR)];
	TCHAR tszDeviceDesc[UIO_BINDING_DEV_DESC_BUFF_LEN/sizeof(TCHAR)];
}tsUIODeviceDesc, *PtsUIODeviceDesc;

// The default protocol used by this Test library.
typedef UNALIGNED struct 
{
   UCHAR    ucDSAP;              // always 0xAA
   UCHAR    ucSSAP;              // always 0xAA
   UCHAR    ucControl;           // always 0x03
   UCHAR    ucPID0;              // always 0x00
   UCHAR    ucPID1;              // always 0x00
   UCHAR    ucPID2;              // always 0x00
   USHORT   usDIX;               // always 0x3781
   ULONG    ulSignature;         // always 'NDIS'
   USHORT   usTargetPortId;      // target port id
   USHORT   usSourcePortId;      // local port id
   ULONG    ulSequenceNumber;    // packet sequence number for a command
   UCHAR    ucResponseMode;      // response required
   UCHAR    ucFirstByte;         // value of first byte in the packet body
   USHORT   usReplyId;           // ID used for reply identification
   UINT     uiSize;              // Header & body size
   ULONG    ulCheckSum;          // packet header check sum
} PROTOCOL_HEADER;

#define NUIO_LIB_RECV_SIGN		(ULONG)'URCV'

typedef struct _sRecvDetails_t
{
   DWORD    dwHndThrd;           // Handle to Recv thread. <Application shd not touch this>
   DWORD	dwIdThrd;			 // Thread Id of Recv Thread. <Application shd not touch this>
   DWORD	dwSignature;		 // Signature used to verify authenticity of this struct.
   DWORD	dwoUio;				 // Pointer to CUIOAdapter struct.
   DWORD	dwStatus;			 // Status of Recv operation.=0 ==> Fail, =1 ==> Pass.
   DWORD	dwPcktsRecved;		 // Total number of pkt received.
   DWORD	dwBytesRecved;		 // Total number of bytes received.
   DWORD	dwRecvTime;			 // Time taken to receive all packets.
   DWORD	dwEthType;			 // EthType;
} sRecvDetails_t, *PsRecvDetails_t;

//******************************************************************************************
//++++++
typedef UNALIGNED struct _tUIOLIB_SendRecv_Reserved
{
	BYTE DstAddr[NUIO_MAC_ADDR_LEN];
	BYTE SrcAddr[NUIO_MAC_ADDR_LEN];
	union 
	{
		USHORT usLength;
		USHORT usEthType;
		BYTE pbLength[2];
	}uLen;

	PROTOCOL_HEADER sProtoHd;
}tUIOLIB_SendRecv_Reserved, *PtUIOLIB_SendRecv_Reserved;

//this is divided as [Ethernet Header]+[Protocol Header]+[User app data]
typedef struct _UIOLIB_SendBuffer
{
	tUIOLIB_SendRecv_Reserved sSendRecvReserved; //"Don't care" for user app
	BYTE pbDataBuffer[1]; //Data should start here.
} tUIOLIB_SendRecvBuffer, *PtUIOLIB_SendRecvBuffer;
//------

//******************************************************************************************

class CUIODevice
{
public:
	TCHAR m_tszDeviceName[UIO_BINDING_DEV_NAME_BUFF_LEN/sizeof(TCHAR)];
	TCHAR m_tszDeviceDesc[UIO_BINDING_DEV_DESC_BUFF_LEN/sizeof(TCHAR)];
	NDIS_MEDIUM m_medium;
	BYTE m_MacAddr[NUIO_MAC_ADDR_LEN];

	void ClearDevice();
	CUIODevice() { ClearDevice(); };
	BOOL SetDeviceName(PTCHAR tszDeviceName);
	BOOL SetDeviceDesc(PTCHAR tszDeviceDesc);
	~CUIODevice() {};
};
	
class CUIOAdapter
{
	friend DWORD NuioBlockRecvThrdProc(LPVOID pv);

private:
	// file handle to NDISUIO which later gets associated with miniport adapter
	HANDLE m_hAdapter;

	//Reqd. type of access to miniport (GENERIC_READ, GENERIC_WRITE etc.)
	DWORD m_dwAccess;

	DWORD NuioRecvThrd(LPVOID pv);

public:
	// miniport adapter properties object.
	CUIODevice m_oUIODevice;

	CUIOAdapter::CUIOAdapter()
	{
		m_dwAccess = 0; //Query access
		m_hAdapter = INVALID_HANDLE_VALUE;
	}

	CUIOAdapter(PTCHAR tszDeviceName, DWORD dwAccess=(GENERIC_READ|GENERIC_WRITE))
	{
		Open(tszDeviceName, dwAccess);
	}

//*******************************************************************************************************************************************************
// Opens file handle to NDISUIO & associates to miniport interface defined by tszDeviceName.
//*******************************************************************************************************************************************************
	BOOL Open(PTCHAR tszDeviceName=NULL, DWORD dwAccess=(GENERIC_READ|GENERIC_WRITE), DWORD dwIoControlCode=0, DWORD dwNotiFicationTypes=0);

//*******************************************************************************************************************************************************
// Closes the file handle to NDISUIO
//*******************************************************************************************************************************************************
	BOOL Close();

//*******************************************************************************************************************************************************
// Associates 'file handle to NDISUIO' to that of miniport interface defined by tszDeviceName.
// dwNotiFicationTypes is used only when dwIoControlCode=IOCTL_NDISUIO_OPEN_DEVICE_EX
//*******************************************************************************************************************************************************
	BOOL Associate(PTCHAR tszDeviceName, DWORD dwIoControlCode=0, DWORD dwNotiFicationTypes=0);

//*******************************************************************************************************************************************************
// Sends the data to associated miniport adapter
// Requirement:
// One should have called Open(), before calling this function.
// Input:
// lpBuffer : Buffer that has data to be sent
// nNumberOfBytesToWrite : Number of bytes to send
// Output:
// dwNumberOfBytesWritten : retruns Number of bytes actually sent
//*******************************************************************************************************************************************************
	BOOL Send(LPCVOID lpBuffer,DWORD nNumberOfBytesToWrite,DWORD& dwNumberOfBytesWritten);

//*******************************************************************************************************************************************************
// Receives the data from associated miniport adapter
// Requirement:
// One should have called Open(), before calling this function.
// Input:
// lpBuffer : Buffer that will have data in it
// nNumberOfBytesToRead : Number of bytes to read
// Output:
// dwNumberOfBytesRead : return Number of bytes actually read
//*******************************************************************************************************************************************************
	BOOL Recv(LPVOID lpBuffer,DWORD nNumberOfBytesToRead,DWORD& dwNumberOfBytesRead);

//*******************************************************************************************************************************************************
// Stop receiving data & unblock the thread which is blocked on above Recv() function.
// Requirement:
// One should have called Open(), before calling this function.
// Input:
// lpBuffer : None.
// nNumberOfBytesToRead : None.
// Output:
// dwNumberOfBytesRead : None
//*******************************************************************************************************************************************************
	BOOL CancelRecv();

//*******************************************************************************************************************************************************
// Query's the OID of a miniport adapter
// Requirement:
// 1. One should have called Open(), before calling this function.
// 2. lpBuffer should be big enoguh to accomodate struct NDISUIO_QUERY_OID + data to be received from adapter.
//    sizeof(NDISUIO_QUERY_OID) := 4 * sizeof(ULONG);
//    Caller can call with dwBuffSize = 0 to know the required size that will be returned in dwBuffSize.
// Input:
// Oid : Oid to be queried
// lpBuffer : Buffer that will have the output data.
// dwBuffSize : size of the lpBuffer. In case if this functions returns FALSE & caller app gets 
//              GetLastError = "ERROR_INSUFFICIENT_BUFFER" then dwBuffSize indicates the size required for buffer.
// dwNumberOfBytesReturned : return Number of bytes actually written into lpBuffer by NDISUIO.
// tszDeviceName: The caller may specify adapter name here different than the one with which this object is associated.
//*******************************************************************************************************************************************************
	BOOL QueryOID(NDIS_OID Oid, LPVOID lpBuffer, DWORD& dwBuffSize, DWORD& dwNumberOfBytesReturned, PTCHAR tszDeviceName=NULL);

//*******************************************************************************************************************************************************
// Sets the OID of a miniport adapter
// Requirement:
// 1. One should have called Open(), before calling this function.
// 2. lpBuffer should be big enoguh to accomodate struct NDISUIO_SET_OID + data to be SET for adapter.
//    sizeof(NDISUIO_SET_OID) := 4 * sizeof(ULONG);
//    Caller can call with dwBuffSize = 0 to know the required size that will be returned in dwBuffSize.
// Input:
// Oid : Oid to be queried
// lpBuffer : Buffer that has the data to be SET
// dwBuffSize : size of the lpBuffer. In case if this functions returns FALSE & caller app gets 
//              GetLastError = "ERROR_INSUFFICIENT_BUFFER" then dwBuffSize indicates the size required for buffer.
// dwNumberOfBytesToWrite : the actual length of data in lpBuffer in bytes to be SET.
// tszDeviceName: The caller may specify adapter name here different than the one with which this object is associated.
//*******************************************************************************************************************************************************
	BOOL SetOID(NDIS_OID Oid, LPCVOID lpBuffer, DWORD& dwBuffSize,DWORD dwNumberOfBytesToWrite,PTCHAR tszDeviceName=NULL);

//*******************************************************************************************************************************************************
// Querys the binding of a NDISUIO protocol for BindingIndex = 0;
// Requirement:
// 1. One should have called Open(), before calling this function.
// 2. lpBuffer should be big enoguh to accomodate struct NDISUIO_QUERY_BINDING + binding data to be read from NDISUIO.
//    sizeof(NDISUIO_QUERY_OID) := 5 * sizeof(ULONG);
//    Caller can call with dwBuffSize = 0 to know the required size that will be returned in dwBuffSize.
// Input:
// lpBuffer : This is the Buffer that will have the binding data on SUCCESS of this function call. 
//            On SUCCESS just typecast lpBuffer to PtsUIODeviceDesc. Please see the above requirements for lpBuffer.
// BindingIndex : This is the index number for adapter database that NDISUIO has. If this binding index is wrong then
//                NDISUIO returns NDIS_STATUS_ADAPTER_NOT_FOUND & this function returns FALSE.
// dwBuffSize : size of the lpBuffer. In case if this functions returns FALSE & caller app gets 
//              GetLastError = "ERROR_INSUFFICIENT_BUFFER" then dwBuffSize indicates the size required for buffer.
// dwBytesReturned : Number of bytes actually written into lpBuffer by NDISUIO.
//*******************************************************************************************************************************************************
	BOOL QueryBinding(LPVOID lpBuffer, ULONG BindingIndex, DWORD& dwBuffSize, DWORD& dwBytesReturned);


//*******************************************************************************************************************************************************
// Set the ether type value over associated NIC.
// Requirement:
// 1. One should have called Open(), before calling this function.
// Input:
// usEthType  : Eth type is two-octet field used to indicate type of Ethernet Frame that needs to be received.
//*******************************************************************************************************************************************************
	BOOL SetEthType(PUSHORT pusEthType, DWORD dwNoOfEthTypes);

//*******************************************************************************************************************************************************
// Frames & Sends the data to associated miniport adapter
// Requirement:
// One should have called Open(), before calling this function.
// Input:
// PtUIOLIB_SendRecvBuffer : Buffer that has data to be sent at pbDataBuffer member.
// nNumberOfBytesToWrite : Number of bytes to send
// dwBuffSize : Size of the buffer. It should be such that it can accomodate Ethernet
//              Header + Protocol Header + Data that is to be sent.
//              Caller can call with dwBuffSize = 0 to know the required size that will 
//              be returned in dwBuffSize along with GetLastError = "ERROR_INSUFFICIENT_BUFFER"
// Output:
// dwTotalNumberOfBytesWritten : retruns Number of bytes actually sent
// dwBuffSize : In case if this functions returns FALSE & caller app gets 
//              GetLastError = "ERROR_INSUFFICIENT_BUFFER" then dwBuffSize indicates the size required for buffer.
//*******************************************************************************************************************************************************
	BOOL FrameAndSendPacket(PtUIOLIB_SendRecvBuffer psSendBuff, DWORD& dwBuffSize, DWORD nNumberOfBytesToWriteFromBuff,
								DWORD& dwTotalNumberOfBytesWritten, DWORD dwNosOfPacketsToSend, DWORD& dwNosOfPacketsSent,  
								PBYTE pbDestAddr, PBYTE pbSrcAddr = NULL, PUSHORT pusEthType=NULL 
								);

	//QueryNICStatistics();
	~CUIOAdapter()
	{
		Close();
	}

//*******************************************************************************************************************************************************
// Gets the NIC statistics like bytes sent/Recived/Resets dones etc.;
// Requirement:
// 1. One should have called Open(), before calling this function.
// 2. lpBuffer should be big enoguh to accomodate struct NIC_STATISTICS.
//    Caller can call with dwBuffSize = 0 to know the required size that will be returned in dwBuffSize.
// Input:
// lpBuffer : This is the Buffer that will have the NIC statistics data on SUCCESS of this function call. 
//            On SUCCESS just typecast lpBuffer to NIC_STATISTICS. Please see the above requirements for lpBuffer.
// dwBuffSize : size of the lpBuffer. In case if this functions returns FALSE & caller app gets 
//              GetLastError = "ERROR_INSUFFICIENT_BUFFER" then dwBuffSize indicates the size required for buffer.
// dwBytesReturned : Number of bytes actually written into lpBuffer by NDISUIO.
//*******************************************************************************************************************************************************
BOOL GetNICStats(LPVOID lpBuffer, DWORD& dwBuffSize, DWORD& dwBytesReturned);

//*******************************************************************************************************************************************************
// Starts the thread to read the data from associated miniport adapter Asynchronously.
// Requirement:
// One should have called Open(), before calling this function.
// Input: psRecvD pointer to sRecvDetails_t struct.
// Output:
//*******************************************************************************************************************************************************
BOOL StartRecvAsync(PsRecvDetails_t psRecvD);

//*******************************************************************************************************************************************************
// Stops the Async read immediately & return the results.
// Max time to wait is specified in millisec in dwTimeOutms.
// Requirement:
// One should have called StartRecvAsync(), with the same psRecvD as it passed to this function.
// Input: psRecvD pointer to sRecvDetails_t struct.
// Output:
//*******************************************************************************************************************************************************
BOOL StopRecvAsync(PsRecvDetails_t psRecvD, DWORD dwTimeOutms=5000);


};

#endif //__UIO_ADAPTER__
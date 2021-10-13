//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// SDPUtil.cpp

#include <stdlib.h>
#include <winsock2.h>
#include <bthapi.h>
#include <bt_api.h>
#include <bt_sdp.h>
#include <bt_buffer.h>
#include <svsutil.hxx>
#include <bt_ddi.h>
#include "SDPUtil.h"
#include "SDPUtilWalk.h"
#include "util.h"

void SDPUtilSetUUID16(NodeData *pNode, unsigned short uuid16);
void SDPUtilSetUINT8(NodeData *pNode, unsigned char uuint8);
void SDPUtilSetUINT16(NodeData *pNode, unsigned short uuint16);
void SDPUtilSetUINT32(NodeData *pNode, unsigned int uuint32);
void SDPUtilSetSTRING(NodeData *pNode, TCHAR * tString);
void SDPUtilSetURL(NodeData *pNode, TCHAR *turl);

// Global 

LPCRITICAL_SECTION gpCriticalSection = NULL;

//
// Intilize a SDP record
//
// pSDPUtilRecord is the record to be initialized
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilInitRecord(
	IN OUT		SDPUtilRecord *		pSDPUtilRecord)
{
	pSDPUtilRecord->ServiceRecordHandle = 0;
	pSDPUtilRecord->SerivceClassIDList.Clear();
	pSDPUtilRecord->ServiceRecordState = 0;
	pSDPUtilRecord->ServiceID = 0;
	pSDPUtilRecord->ProtocolDescriptorList.Clear();
	pSDPUtilRecord->BrowseGroupList.Clear();
	pSDPUtilRecord->LanguageBaseAttributeIDList.Clear();
	pSDPUtilRecord->ServiceInfoTimeToLive = 0;
	pSDPUtilRecord->BluetoothProfileDescriptorList.Clear();
	_tcscpy(pSDPUtilRecord->DocumentationURL, TEXT(""));
	_tcscpy(pSDPUtilRecord->ClientExecutableURL, TEXT(""));
	_tcscpy(pSDPUtilRecord->IconURL, TEXT(""));
	_tcscpy(pSDPUtilRecord->ServiceName, TEXT(""));
	_tcscpy(pSDPUtilRecord->ServiceDescription, TEXT(""));
	_tcscpy(pSDPUtilRecord->ProviderName, TEXT(""));

	return 0;
}

//
// Build a default SDP record
//
// pSDPUtilRecord is the record to be built
// dwProtocol is the protocol
// dwPort is port number
// usServiceID is the new service uuid
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilBuildRecord(
	IN OUT		SDPUtilRecord *		pSDPUtilRecord,
	IN			USHORT				usProtocol,
	IN			USHORT				usPort,
	IN			USHORT				usServiceID)
{
	CProtocolDescriptorStack	protocolDescriptorStack;
	ProtocolDescriptor			protocolDescriptor;

	SDPUtilInitRecord(pSDPUtilRecord);

	// set SerivceClassIDList
	pSDPUtilRecord->SerivceClassIDList.Append(OBEXObjectPushServiceClassID_UUID16);
	pSDPUtilRecord->SerivceClassIDList.Append(usServiceID);

	// set ServiceID
	pSDPUtilRecord->ServiceID = usServiceID;

	// set ProtocolDescriptorList
	protocolDescriptor.uuid = L2CAP_PROTOCOL_UUID16;
	protocolDescriptor.usVersion = 1;
	protocolDescriptor.usPort = 3;
	protocolDescriptorStack.Append(&protocolDescriptor);

	protocolDescriptor.uuid = usProtocol;
	protocolDescriptor.usVersion = 1;
	protocolDescriptor.usPort = usPort;
	protocolDescriptorStack.Append(&protocolDescriptor);

	pSDPUtilRecord->ProtocolDescriptorList.Append(&protocolDescriptorStack);

	// set BrowseGroupList
	pSDPUtilRecord->BrowseGroupList.Append(PublicBrowseRoot_UUID16);

	// set Servicename
	_tcscpy(pSDPUtilRecord->ServiceName, TEXT("SDP Test Service Record\n"));

	return 0;
}

//
// Print a SDP record
//
// pSDPUtilRecord is the record to be printed
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD SDPUtilPrintRecord(
	IN			SDPUtilRecord *		pSDPUtilRecord)
{
	if (gpCriticalSection)
	{
		EnterCriticalSection(gpCriticalSection);
		OutStr(TEXT("In thread 0x%08x\n"), GetCurrentThreadId());
	}

	OutStr(TEXT("\n"));
	OutStr(TEXT("----------------------------------------------\n"));
	OutStr(TEXT("ServiceRecordHandle   : 0x%08X\n"), pSDPUtilRecord->ServiceRecordHandle);

	OutStr(TEXT("ServiceClassIDList\n"));
	pSDPUtilRecord->SerivceClassIDList.Print();
	
	OutStr(TEXT("ServiceRecordState    : %u\n"), pSDPUtilRecord->ServiceRecordState);
	
	OutStr(TEXT("ServiceID             : 0x%04X\n"), pSDPUtilRecord->ServiceID);
	
	OutStr(TEXT("ProtocolDescriptorList\n"));
	pSDPUtilRecord->ProtocolDescriptorList.Print();

	OutStr(TEXT("BrowseGroupList\n"));
	pSDPUtilRecord->BrowseGroupList.Print();

	OutStr(TEXT("LanguageBaseAttributeIDList\n"));
	pSDPUtilRecord->LanguageBaseAttributeIDList.Print();

	OutStr(TEXT("ServiceInfoTimeToLive : %u\n"), pSDPUtilRecord->ServiceInfoTimeToLive);

	OutStr(TEXT("ServiceAvailability   : %u\n"), pSDPUtilRecord->ServiceAvailability);

	OutStr(TEXT("BluetoothProfileDescriptorList\n"));
	pSDPUtilRecord->BluetoothProfileDescriptorList.Print();

	OutStr(TEXT("DocumentationURL      : %s\n"), pSDPUtilRecord->DocumentationURL);

	OutStr(TEXT("ClientExecutableURL   : %s\n"), pSDPUtilRecord->ClientExecutableURL);

	OutStr(TEXT("IconURL               : %s\n"), pSDPUtilRecord->IconURL);

	OutStr(TEXT("ServiceName           : %s\n"), pSDPUtilRecord->ServiceName);

	OutStr(TEXT("ServiceDescription    : %s\n"), pSDPUtilRecord->ServiceDescription);

	OutStr(TEXT("ProviderName          : %s\n"), pSDPUtilRecord->ProviderName);
	OutStr(TEXT("----------------------------------------------\n"));
	OutStr(TEXT("\n"));

	if (gpCriticalSection)
		LeaveCriticalSection(gpCriticalSection);

	return 0;
}

//
// Print a SDP stream
//
// pSDPStream is the stream to be printed
// ulStreamSize is the size of the stream
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilPrintStream(
	IN			UCHAR *		pSDPStream,
	IN			DWORD		dwStreamSize)
{
	TCHAR tempstr[20];
	TCHAR str1[255];
	TCHAR str2[255];
	DWORD i;
	CSDPUtilWalk sdpUtilWalk;
	ISdpStream *pIStream;

	if (gpCriticalSection)
	{
		EnterCriticalSection(gpCriticalSection);
		OutStr(TEXT("In thread 0x%08x\n"), GetCurrentThreadId());
	}

	OutStr(TEXT("In SDPUtilPrintStream\n"));
	//
	// Initialize COM
	//
    CoInitializeEx(NULL,COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);

	for (i = 0; i < dwStreamSize; i ++)
	{
		if ((i % 8) == 0)
		{
			_tcscpy(str1, TEXT(""));
			_tcscpy(str2, TEXT(""));
			_stprintf(tempstr, TEXT("%04x "), (USHORT) i);
			_tcscat(str1, tempstr);
			_stprintf(tempstr, TEXT("  |   "));
			_tcscat(str2, tempstr);
		}

		_stprintf(tempstr, TEXT("%02x "), pSDPStream[i]);
		_tcscat(str1, tempstr);

		if ((pSDPStream[i] > 31) && (pSDPStream[i] < 127))
			_stprintf(tempstr, TEXT("%c "), pSDPStream[i]);
		else
			_stprintf(tempstr, TEXT(". "));

		_tcscat(str2, tempstr);

		if ((i % 8) == 7)
		{
			OutStr(TEXT("%s %s\n"), str1, str2);
		}
	}

	if (i % 8 != 7)
	{
		i = i % 8;
		for (; i < 8; i ++)
			_tcscat(str1, TEXT("   "));
		OutStr(TEXT("%s %s\n"), str1, str2);
	}

	//
	// walk stream
	//
	OutStr(TEXT("Walking pSDPStream\n"));
	hr = pIStream->Walk(pSDPStream, dwStreamSize, &sdpUtilWalk);

	if (gpCriticalSection)
	{
		LeaveCriticalSection(gpCriticalSection);
	}

	pIStream->Release();
	pIStream = NULL;

	if (pIStream)
		pIStream->Release();

	return 0;
}

//
// Convert SDPUtilRecord to a byte stream
//
// pStream is the stream to conver to
// pSDPUtilRecord is the record to convert
// pStreamSize is the size of the stream
//		After conversion, it will be the number of bytes converted
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilRecordToStream(
	OUT			UCHAR *			pStream,
	IN			SDPUtilRecord *	pSDPUtilRecord,
	IN OUT		DWORD *			pStreamSize)
{
	IClassFactory *pCFRecord    = NULL;
	IClassFactory *pCFContainer = NULL;
    ISdpRecord *pRecord         = NULL;
	NodeData nodeData;
	NodeData nodeDataBak;
	ISdpStream *pIStream = NULL;
	UCHAR *pCoStream = NULL;
	ULONG ulCoStreamSize;

	ISdpNodeContainer *pSerivceClassIDListContainer = NULL;
	ISdpNodeContainer *pProtocolDescriptorListContainer  = NULL;
	ISdpNodeContainer *pProtocolDescriptorStackContainer = NULL;
	ISdpNodeContainer *pProtocolDescriptorContainer      = NULL;
	ISdpNodeContainer *pBrowseGroupListContainer = NULL;
	ISdpNodeContainer *pLanguageBaseAttributeIDListContainer = NULL;
	ISdpNodeContainer *pLanguageBaseAttributeIDContainer = NULL;
	ISdpNodeContainer *pBluetoothProfileDescriptorListContainer = NULL;
	ISdpNodeContainer *pBluetoothProfileDescriptorContainer = NULL;

	BluetoothProfileDescriptor *pBluetoothProfileDescriptor = NULL;
	LanguageBaseAttributeID *pLanguageBaseAttributeID = NULL;;
	CProtocolDescriptorStack *pProtocolDescriptorStack = NULL;
	ProtocolDescriptor *pProtocolDescriptor = NULL;

    ULONG ulErrorByte;
	DWORD dwResult = 0;
	DWORD i, j;

	CSDPUtilWalk sdpUtilWalk;

	OutStr(TEXT("In SDPUtilRecordToStream\n"));
	//
	// Initialize COM
	//
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    HRESULT hr = CoGetClassObject(__uuidof(SdpRecord),
                     CLSCTX_INPROC_SERVER,
                     NULL,
                     IID_IClassFactory,
                     (PVOID*)&pCFRecord); 

    if (pCFRecord == NULL) {
        OutStr(TEXT("CoGetClassObject(uuidof(SdpRecord) fails, error=0x%08x\n"), hr);
        goto Finished;
    }

    hr = CoGetClassObject(__uuidof(SdpNodeContainer),
                     CLSCTX_INPROC_SERVER,
                     NULL,
                     IID_IClassFactory,
                     (PVOID*)&pCFContainer); 

    if (pCFContainer == NULL)  {
		OutStr(TEXT("CoGetClassObject(uuidof(SdpNodeContainer) fails, error=0x%08x\n"), hr);
        goto Finished;
	}

	//
	// Build record
	//
    hr = pCFRecord->CreateInstance(NULL, 
                              __uuidof(ISdpRecord),
                              (PVOID*) &pRecord);
    if (pRecord == NULL) {
		OutStr(TEXT("CreateInstance on ISdpRecord fails, error = 0x%08x\n"), hr);
        goto Finished;
    }

	//
	// 0x0000 ServiceRecordHandle
	//
	// Nothing needs to be done for here ServiceRecordHandle. WSASetService will set this.
	if (pSDPUtilRecord->ServiceRecordHandle != 0)
	{
		SDPUtilSetUINT32(&nodeData, pSDPUtilRecord->ServiceRecordHandle);
		hr = pRecord->SetAttribute(SDP_ATTRIB_RECORD_HANDLE, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_RECORD_HANDLE fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0001 SerivceClassIDList
	//
	// We do some additional sanity check here for ISdpNodeContainer functions 
	// Because SerivceClassIDList and ServiceRecordHandle are the only two attributes
	// required in a service record
	//
	if (pSDPUtilRecord->SerivceClassIDList.Count() != 0)
	{
		UCHAR pContainerStream[510];
		ULONG ulNumBytes = 510;

		// prepare the container
		hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pSerivceClassIDListContainer);
		if (pSerivceClassIDListContainer == NULL) 
		{
			OutStr(TEXT("CreateInstance on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		hr = pSerivceClassIDListContainer->SetType(NodeContainerTypeSequence);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetType on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// additional check
		// after the container is locked, SetNode and AppendNode should fail.
		// SetType should pass, however.
		hr = pSerivceClassIDListContainer->LockContainer(TRUE);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("LockContainer on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// check if AppendNode fails
		SDPUtilSetUUID16(&nodeData, pSDPUtilRecord->SerivceClassIDList.Get(0));
		hr = pSerivceClassIDListContainer->AppendNode(&nodeData);
		if (!FAILED(hr))
		{
			OutStr(TEXT("AppendNode on pSerivceClassIDListContainer success when container is locked, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// unlock container
		hr = pSerivceClassIDListContainer->LockContainer(FALSE);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("LockContainer on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// append nodes to container
		for (i = 0; i < pSDPUtilRecord->SerivceClassIDList.Count(); i ++)
		{
			SDPUtilSetUUID16(&nodeData, pSDPUtilRecord->SerivceClassIDList.Get(i));
			hr = pSerivceClassIDListContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}
		}

		// additional check for SetNode
		hr = pSerivceClassIDListContainer->LockContainer(TRUE);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("LockContainer on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// this SetNode should fail
		hr = pSerivceClassIDListContainer->SetNode(0, &nodeData);
		if (!FAILED(hr))
		{
			OutStr(TEXT("SetNode on pSerivceClassIDListContainer success when container is locked, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// unlock container
		hr = pSerivceClassIDListContainer->LockContainer(FALSE);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("LockContainer on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// backup node 0
		hr = pSerivceClassIDListContainer->GetNode(0, &nodeDataBak);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("GetNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// set node 0 in the container to some random value
		nodeData.type = SDP_TYPE_UINT;
		nodeData.specificType = SDP_ST_UINT32;
		nodeData.u.uint32 = 0xFFFFFFFF;

		hr = pSerivceClassIDListContainer->SetNode(0, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// check if it is changed
		memset(&nodeData, 0, sizeof(nodeData));
		hr = pSerivceClassIDListContainer->GetNode(0, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("GetNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		if ( (nodeData.type != SDP_TYPE_UINT) ||
			 (nodeData.specificType != SDP_ST_UINT32) ||
			 (nodeData.u.uint32 != 0xFFFFFFFF) )
		{
			OutStr(TEXT("Value retrieved from GetNode does not match value set by SetNode\n"));
			dwResult = 1;
			goto Finished;
		}

		// change it back
		hr = pSerivceClassIDListContainer->SetNode(0, &nodeDataBak);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}		

		// additional check for WriteStream and CreateFromStream
		// write the container to a stream
		// change the container
		// and restore the original container from stream
		hr = pSerivceClassIDListContainer->WriteStream(pContainerStream, &ulNumBytes);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("WriteStream on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}		
		
		// set node 0 in the container to some random value
		// also do a quick check for GetNodeStringData
		SDPUtilSetSTRING(&nodeData, TEXT("Random String"));
		hr = pSerivceClassIDListContainer->SetNode(0, &nodeData);
		CoTaskMemFree(nodeData.u.str.val);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetNode on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}		

		memset(&nodeData, 0, sizeof(nodeData));
		hr = pSerivceClassIDListContainer->GetNodeStringData(0, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("GetNodeStringData on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}	
		if (strncmp(nodeData.u.str.val, "Random String", nodeData.u.str.length))
		{
			OutStr(TEXT("String value retrieved from GetNodeStringData does not match string value set\n"));
			CoTaskMemFree(nodeData.u.str.val);
			dwResult = 1;
			goto Finished;
		}
		CoTaskMemFree(nodeData.u.str.val);
		
		// restore original container
		hr = pSerivceClassIDListContainer->CreateFromStream(pContainerStream, ulNumBytes);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("WriteStream on pSerivceClassIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}			

		// add container to record
		nodeData.type = SDP_TYPE_CONTAINER;
		nodeData.specificType = SDP_ST_NONE;
		nodeData.u.container = pSerivceClassIDListContainer;

		hr = pRecord->SetAttribute(SDP_ATTRIB_CLASS_ID_LIST, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_CLASS_ID_LIST fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		
		pSerivceClassIDListContainer->Release();
		pSerivceClassIDListContainer = NULL;
    }

	//
	// 0x0002 ServiceRecordState
	//
	if (pSDPUtilRecord->ServiceRecordState != 0)
	{
		SDPUtilSetUINT32(&nodeData, pSDPUtilRecord->ServiceRecordState);
		hr = pRecord->SetAttribute(SDP_ATTRIB_RECORD_STATE, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_RECORD_STATE fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0003 ServiceID
	//
	if (pSDPUtilRecord->ServiceID != 0)
	{
		SDPUtilSetUUID16(&nodeData, pSDPUtilRecord->ServiceID);
		hr = pRecord->SetAttribute(SDP_ATTRIB_SERVICE_ID, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_SERVICE_ID fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0004 ProtocolDescriptorList
	//
	if (pSDPUtilRecord->ProtocolDescriptorList.Count() != 0)
	{
		// prepare ProtocolDescriptorListContainer
		hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pProtocolDescriptorListContainer);
		if (pProtocolDescriptorListContainer == NULL) 
		{
			OutStr(TEXT("CreateInstance on pProtocolDescriptorListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		hr = pProtocolDescriptorListContainer->SetType(NodeContainerTypeAlternative);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetType on pProtocolDescriptorListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// append nodes to ProtocolDescriptorListContainer
		for (i = 0; i < pSDPUtilRecord->ProtocolDescriptorList.Count(); i ++)
		{
			pProtocolDescriptorStack = pSDPUtilRecord->ProtocolDescriptorList.Get(i);
			
			// prepare pProtocolDescriptorStackContainer
			hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pProtocolDescriptorStackContainer);
			if (pProtocolDescriptorStackContainer == NULL) 
			{
				OutStr(TEXT("CreateInstance on pProtocolDescriptorStackContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}
			hr = pProtocolDescriptorStackContainer->SetType(NodeContainerTypeSequence);
			if (FAILED(hr)) 
			{
				OutStr(TEXT("SetType on pProtocolDescriptorStackContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}

			// append nodes to ProtocolDescriptorStackContainer
			for (j = 0; j < pProtocolDescriptorStack->Count(); j++)
			{
				pProtocolDescriptor = pProtocolDescriptorStack->Get(j);

				// prepare pProtocolDescriptorContainer
				hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
				                (PVOID*) &pProtocolDescriptorContainer);
				if (pProtocolDescriptorContainer == NULL) 
				{
					OutStr(TEXT("CreateInstance on pProtocolDescriptorContainer fails, error = 0x%08x\n"), hr);
					dwResult = 1;
					goto Finished;
				}
				hr = pProtocolDescriptorContainer->SetType(NodeContainerTypeSequence);
				if (FAILED(hr)) 
				{
					OutStr(TEXT("SetType on pProtocolDescriptorContainer fails, error = 0x%08x\n"), hr);
					dwResult = 1;
					goto Finished;
				}

				// uuid
				SDPUtilSetUUID16(&nodeData, pProtocolDescriptor->uuid);
				hr = pProtocolDescriptorContainer->AppendNode(&nodeData);
				if (FAILED(hr))
				{
					OutStr(TEXT("AppendNode on pProtocolDescriptorStackContainer fails, error = 0x%08x\n"), hr);
					dwResult = 1;
					goto Finished;
				}

				// port - has to be the next parameter after uuid
				// for RFCOMM this data is of type UINT8
				// for L2CAP this data has type UINT16
				if (nodeData.u.uuid16 == BTHPROTO_RFCOMM)
					SDPUtilSetUINT8(&nodeData, (UCHAR) pProtocolDescriptor->usPort);
				else
					SDPUtilSetUINT16(&nodeData, pProtocolDescriptor->usPort);

				hr = pProtocolDescriptorContainer->AppendNode(&nodeData);
				if (FAILED(hr))
				{
					OutStr(TEXT("AppendNode on pProtocolDescriptorStackContainer fails, error = 0x%08x\n"), hr);
					dwResult = 1;
					goto Finished;
				}	
				
				// version
				SDPUtilSetUINT16(&nodeData, pProtocolDescriptor->usVersion);
				hr = pProtocolDescriptorContainer->AppendNode(&nodeData);
				if (FAILED(hr))
				{
					OutStr(TEXT("AppendNode on pProtocolDescriptorStackContainer fails, error = 0x%08x\n"), hr);
					dwResult = 1;
					goto Finished;
				}

				
				// append pProtocolDescriptorContainer to pProtocolDescriptorStackContainer
				nodeData.type=SDP_TYPE_CONTAINER;
				nodeData.specificType=SDP_ST_NONE;
				nodeData.u.container = pProtocolDescriptorContainer;
				pProtocolDescriptorStackContainer->AppendNode(&nodeData);

				pProtocolDescriptorContainer->Release();
				pProtocolDescriptorContainer = NULL;
			}

			// append nodes to pProtocolDescriptorListContainer
			nodeData.type=SDP_TYPE_CONTAINER;
			nodeData.specificType=SDP_ST_NONE;
			nodeData.u.container = pProtocolDescriptorStackContainer;
			pProtocolDescriptorListContainer->AppendNode(&nodeData);

			if (pSDPUtilRecord->ProtocolDescriptorList.Count() > 1)
			{
				pProtocolDescriptorStackContainer->Release();
				pProtocolDescriptorStackContainer = NULL;
			}
		}

		// add pProtocolDescriptorListContainer to record
		nodeData.type = SDP_TYPE_CONTAINER;
		nodeData.specificType = SDP_ST_NONE;
		if (pSDPUtilRecord->ProtocolDescriptorList.Count() > 1)
		{
			nodeData.u.container = pProtocolDescriptorListContainer;
			if (gpCriticalSection)
			{
				EnterCriticalSection(gpCriticalSection);
			}

			OutStr(TEXT("Walking pProtocolDescriptorListContainer\n"));
			pProtocolDescriptorListContainer->Walk(&sdpUtilWalk);

			if (gpCriticalSection)
			{
				LeaveCriticalSection(gpCriticalSection);
			}
		}
		else
		{
			nodeData.u.container = pProtocolDescriptorStackContainer;
			if (gpCriticalSection)
			{
				EnterCriticalSection(gpCriticalSection);
			}

			OutStr(TEXT("Walking pProtocolDescriptorStackContainer\n"));
			pProtocolDescriptorStackContainer->Walk(&sdpUtilWalk);

			if (gpCriticalSection)
			{
				LeaveCriticalSection(gpCriticalSection);
			}
		}

		hr = pRecord->SetAttribute(SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		
		if (pSDPUtilRecord->ProtocolDescriptorList.Count() == 1)
		{
			pProtocolDescriptorStackContainer->Release();
			pProtocolDescriptorStackContainer = NULL;
		}

		pProtocolDescriptorListContainer->Release();
		pProtocolDescriptorListContainer = NULL;
	}

	//
	// 0x0005 BrowseGroupList
	//
	if (pSDPUtilRecord->BrowseGroupList.Count() != 0)
	{
		// prepare the container
		hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pBrowseGroupListContainer);
		if (pBrowseGroupListContainer == NULL) 
		{
			OutStr(TEXT("CreateInstance on pBrowseGroupListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		hr = pBrowseGroupListContainer->SetType(NodeContainerTypeSequence);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetType on pBrowseGroupListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// append nodes to container
		for (i = 0; i < pSDPUtilRecord->BrowseGroupList.Count(); i ++)
		{
			SDPUtilSetUUID16(&nodeData, pSDPUtilRecord->BrowseGroupList.Get(i));
			hr = pBrowseGroupListContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pBrowseGroupListContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}
		}

		// add container to record
		nodeData.type = SDP_TYPE_CONTAINER;
		nodeData.specificType = SDP_ST_NONE;
		nodeData.u.container = pBrowseGroupListContainer;

		hr = pRecord->SetAttribute(SDP_ATTRIB_BROWSE_GROUP_LIST, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_BROWSE_GROUP_LIST fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		
		pBrowseGroupListContainer->Release();
		pBrowseGroupListContainer = NULL;
    }

	//
	// 0x0006 LanguageBaseAttributeIDList
	//
	if (pSDPUtilRecord->LanguageBaseAttributeIDList.Count() != 0)
	{
		// prepare pLanguageBaseAttributeIDListContainer
		hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pLanguageBaseAttributeIDListContainer);
		if (pLanguageBaseAttributeIDListContainer == NULL) 
		{
			OutStr(TEXT("CreateInstance on pLanguageBaseAttributeIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		hr = pLanguageBaseAttributeIDListContainer->SetType(NodeContainerTypeSequence);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetType on pLanguageBaseAttributeIDListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// append nodes to pLanguageBaseAttributeIDListContainer
		for (i = 0; i < pSDPUtilRecord->BrowseGroupList.Count(); i ++)
		{
			pLanguageBaseAttributeID = pSDPUtilRecord->LanguageBaseAttributeIDList.Get(i);

			// prepare pLanguageBaseAttributeIDContainer
			hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
				                (PVOID*) &pLanguageBaseAttributeIDContainer);
			if (pLanguageBaseAttributeIDContainer == NULL) 
			{
				OutStr(TEXT("CreateInstance on pLanguageBaseAttributeIDContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}
			hr = pLanguageBaseAttributeIDContainer->SetType(NodeContainerTypeSequence);
			if (FAILED(hr)) 
			{
				OutStr(TEXT("SetType on pLanguageBaseAttributeIDContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}

			// language
			SDPUtilSetUINT16(&nodeData, pLanguageBaseAttributeID->usLanguage);
			hr = pLanguageBaseAttributeIDContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pLanguageBaseAttributeIDContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}	

			// encoding
			SDPUtilSetUINT16(&nodeData, pLanguageBaseAttributeID->usEncoding);
			hr = pLanguageBaseAttributeIDContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pLanguageBaseAttributeIDContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}	

			// base attribute ID
			SDPUtilSetUINT16(&nodeData, pLanguageBaseAttributeID->usBaseAttributeID);
			hr = pLanguageBaseAttributeIDContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pLanguageBaseAttributeIDContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}	

			// append pLanguageBaseAttributeIDContainer to pLanguageBaseAttributeIDListContainer
			nodeData.type = SDP_TYPE_CONTAINER;
			nodeData.specificType = SDP_ST_NONE;
			nodeData.u.container = pLanguageBaseAttributeIDContainer;

			pLanguageBaseAttributeIDContainer->Release();
			pLanguageBaseAttributeIDContainer = NULL;
		}

		// add pLanguageBaseAttributeIDListContainer to record
		nodeData.type = SDP_TYPE_CONTAINER;
		nodeData.specificType = SDP_ST_NONE;
		nodeData.u.container = pLanguageBaseAttributeIDListContainer;

		hr = pRecord->SetAttribute(SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		
		pLanguageBaseAttributeIDListContainer->Release();
		pLanguageBaseAttributeIDListContainer = NULL;
    }

	//
	// 0x0007 ServiceInfoTimeToLive
	//
	if (pSDPUtilRecord->ServiceInfoTimeToLive != 0)
	{
		SDPUtilSetUINT32(&nodeData, pSDPUtilRecord->ServiceInfoTimeToLive);
		hr = pRecord->SetAttribute(SDP_ATTRIB_INFO_TIME_TO_LIVE, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_INFO_TIME_TO_LIVE fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0008 ServiceAvailability
	//
	if (pSDPUtilRecord->ServiceAvailability != 0)
	{
		SDPUtilSetUINT8(&nodeData, pSDPUtilRecord->ServiceAvailability);
		hr = pRecord->SetAttribute(SDP_ATTRIB_AVAILABILITY, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_AVAILABILITY fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0009 BluetoothProfileDescriptorList
	//
	if (pSDPUtilRecord->BluetoothProfileDescriptorList.Count() != 0)
	{
		// prepare pBluetoothProfileDescriptorListContainer
		hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
                              (PVOID*) &pBluetoothProfileDescriptorListContainer);
		if (pBluetoothProfileDescriptorListContainer == NULL) 
		{
			OutStr(TEXT("CreateInstance on pBluetoothProfileDescriptorListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		hr = pBluetoothProfileDescriptorListContainer->SetType(NodeContainerTypeSequence);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetType on pBluetoothProfileDescriptorListContainer fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		// append nodes to pBluetoothProfileDescriptorListContainer
		for (i = 0; i < pSDPUtilRecord->BluetoothProfileDescriptorList.Count(); i ++)
		{
			pBluetoothProfileDescriptor = pSDPUtilRecord->BluetoothProfileDescriptorList.Get(i);

			// prepare pBluetoothProfileDescriptorContainer
			hr = pCFContainer->CreateInstance(NULL, 
                              __uuidof(ISdpNodeContainer),
				                (PVOID*) &pBluetoothProfileDescriptorContainer);
			if (pBluetoothProfileDescriptorContainer == NULL) 
			{
				OutStr(TEXT("CreateInstance on pBluetoothProfileDescriptorContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}
			hr = pBluetoothProfileDescriptorContainer->SetType(NodeContainerTypeSequence);
			if (FAILED(hr)) 
			{
				OutStr(TEXT("SetType on pBluetoothProfileDescriptorContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}

			// uuid
			SDPUtilSetUUID16(&nodeData, pBluetoothProfileDescriptor->uuid);
			hr = pBluetoothProfileDescriptorContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pBluetoothProfileDescriptorContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}	

			// version
			SDPUtilSetUINT16(&nodeData, pBluetoothProfileDescriptor->usVersion);
			hr = pBluetoothProfileDescriptorContainer->AppendNode(&nodeData);
			if (FAILED(hr))
			{
				OutStr(TEXT("AppendNode on pBluetoothProfileDescriptorContainer fails, error = 0x%08x\n"), hr);
				dwResult = 1;
				goto Finished;
			}	

			// append pBluetoothProfileDescriptorContainer to pBluetoothProfileDescriptorListContainer
			nodeData.type = SDP_TYPE_CONTAINER;
			nodeData.specificType = SDP_ST_NONE;
			nodeData.u.container = pBluetoothProfileDescriptorContainer;

			pBluetoothProfileDescriptorContainer->Release();
			pBluetoothProfileDescriptorContainer = NULL;
		}

		// add pBluetoothProfileDescriptorListContainer to record
		nodeData.type = SDP_TYPE_CONTAINER;
		nodeData.specificType = SDP_ST_NONE;
		nodeData.u.container = pBluetoothProfileDescriptorListContainer;

		hr = pRecord->SetAttribute(SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
		
		pBluetoothProfileDescriptorListContainer->Release();
		pBluetoothProfileDescriptorListContainer = NULL;
    }

	//
	// 0x000A DocumentationURL
	//
	if (_tcscmp(pSDPUtilRecord->DocumentationURL, TEXT("\n")))
	{
		SDPUtilSetURL(&nodeData, pSDPUtilRecord->DocumentationURL);
		hr = pRecord->SetAttribute(SDP_ATTRIB_DOCUMENTATION_URL, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_DOCUMENTATION_URL fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x000B ClientExecutableURL
	//
	if (_tcscmp(pSDPUtilRecord->ClientExecutableURL, TEXT("\n")))
	{
		SDPUtilSetURL(&nodeData, pSDPUtilRecord->ClientExecutableURL);
		hr = pRecord->SetAttribute(SDP_ATTRIB_CLIENT_EXECUTABLE_URL, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_CLIENT_EXECUTABLE_URL fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x000C IconURL
	//
	if (_tcscmp(pSDPUtilRecord->IconURL, TEXT("\n")))
	{
		SDPUtilSetURL(&nodeData, pSDPUtilRecord->IconURL);
		hr = pRecord->SetAttribute(SDP_ATTRIB_ICON_URL, &nodeData);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on SDP_ATTRIB_ICON_URL fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0000 + LanguageBaseAttributeID ServiceName
	//
	if (_tcscmp(pSDPUtilRecord->ServiceName, TEXT("\n")))
	{
		SDPUtilSetSTRING(&nodeData, pSDPUtilRecord->ServiceName);

		// according to spec : the base attribuate ID value for the primary language must be 0x0100
		hr = pRecord->SetAttribute(0x0100, &nodeData);
		CoTaskMemFree(nodeData.u.url.val);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on 0x0100 fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0001 + LanguageBaseAttributeID ServiceDescription
	//
	if (_tcscmp(pSDPUtilRecord->ServiceDescription, TEXT("\n")))
	{
		SDPUtilSetSTRING(&nodeData, pSDPUtilRecord->ServiceDescription);

		// according to spec : the base attribuate ID value for the primary language must be 0x0100
		hr = pRecord->SetAttribute(0x0101, &nodeData);
		CoTaskMemFree(nodeData.u.url.val);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on 0x0101 fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	//
	// 0x0002 + LanguageBaseAttributeID ProviderName
	//
	if (_tcscmp(pSDPUtilRecord->ProviderName, TEXT("\n")))
	{
		SDPUtilSetSTRING(&nodeData, pSDPUtilRecord->ProviderName);

		// according to spec : the base attribuate ID value for the primary language must be 0x0100
		hr = pRecord->SetAttribute(0x0101, &nodeData);
		CoTaskMemFree(nodeData.u.url.val);
		if (FAILED(hr)) 
		{
			OutStr(TEXT("SetAttribute on 0x0102 fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}
	}

	// convert record to stream
	hr = pRecord->WriteToStream(&pCoStream, &ulCoStreamSize, 0, 0);
	if (FAILED (hr))
	{
		OutStr( TEXT ("WriteToStream failed\n") );
		goto Finished;
	}


    hr = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);

	// do some additional check for Validate function
	hr = pIStream->Validate(pCoStream, 1, &ulErrorByte);
	if (!FAILED (hr))
	{
		OutStr(TEXT("Validate pCoStream successes with stream length 1 when expected to fail\n"));
		dwResult = 1;
		goto Finished;
	}
	
	hr = pIStream->Validate(pCoStream, ulCoStreamSize - 1 , &ulErrorByte);
	if (!FAILED (hr))
	{
		OutStr(TEXT("Validate pCoStream successes with stream length ulCoStreamSize - 1 when expected to fail\n"));
		dwResult = 1;
		goto Finished;
	}

	hr = pIStream->Validate(pCoStream, ulCoStreamSize, &ulErrorByte);
    if (FAILED (hr))
    {
		OutStr(TEXT("Validate pCoStream fails at byte %u, error = 0x%08x\n"), ulErrorByte, hr);
		dwResult = 1;
		goto Finished;
    }

	if (ulCoStreamSize <= *pStreamSize)
	{
		memcpy(pStream, pCoStream, ulCoStreamSize);
		*pStreamSize = ulCoStreamSize;
	}
	else
	{
		OutStr(TEXT("pStream not big enough. pStreamSize = %u, size needed = %lu\n"), *pStreamSize, ulCoStreamSize);
		dwResult = 1;
		goto Finished;
	}

	//
	// walk record
	//
	if (gpCriticalSection)
	{
		EnterCriticalSection(gpCriticalSection);
	}

	OutStr(TEXT("Walking pRecord\n"));
	hr = pRecord->Walk(&sdpUtilWalk);

	if (gpCriticalSection)
	{
		LeaveCriticalSection(gpCriticalSection);
	}

	pIStream->Release();
	pIStream = NULL;

	CoTaskMemFree(pCoStream);
	pCoStream = NULL;

	// cleanup
	pRecord->Release();
	pRecord = NULL;

	pCFContainer->Release();
	pCFContainer = NULL;

	pCFRecord->Release();
	pCFRecord = NULL;

	OutStr(TEXT("Converted SDP record :\n"));
	SDPUtilPrintRecord(pSDPUtilRecord);
	OutStr(TEXT("To stream :\n"));
	SDPUtilPrintStream(pStream, *pStreamSize);

Finished:

	if (pIStream)
		pIStream->Release();

	if (pBluetoothProfileDescriptorListContainer)
		pBluetoothProfileDescriptorListContainer->Release();

	if (pBluetoothProfileDescriptorContainer)
		pBluetoothProfileDescriptorContainer->Release();

	if (pLanguageBaseAttributeIDListContainer)
		pLanguageBaseAttributeIDListContainer->Release();

	if (pLanguageBaseAttributeIDContainer)
		pLanguageBaseAttributeIDContainer->Release();

	if (pBrowseGroupListContainer)
		pBrowseGroupListContainer->Release();

	if (pProtocolDescriptorContainer)
		pProtocolDescriptorContainer->Release();

	if (pProtocolDescriptorStackContainer)
		pProtocolDescriptorStackContainer->Release();

	if (pProtocolDescriptorListContainer)
		pProtocolDescriptorListContainer->Release();

	if (pSerivceClassIDListContainer)
		pSerivceClassIDListContainer->Release();

	if (pRecord)
		pRecord->Release();

	if (pCFContainer)
		pCFContainer->Release();

	if (pCFRecord)
		pCFRecord->Release();

	CoUninitialize();

	return dwResult;
}

//
// Add or update existing SDP service record
//
// pSDPUtilRecord->ServiceRecordHandle is updated on success
// dwControlFlags is the flag to use for WSASetService
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD										
SDPUtilUpdateRecord (
	IN OUT		SDPUtilRecord	*pSDPUtilRecord,
	IN			DWORD			dwControlFlags)		
{
	WSAQUERYSET Service;
	UCHAR buf[MAX_SDPRECSIZE];
	BLOB blob;
	PBTHNS_SETBLOB updateBlob = (PBTHNS_SETBLOB) buf;
	DWORD dwResult = 0;
	ULONG ulSdpVersion = BTH_SDP_VERSION;

	blob.pBlobData = (PBYTE) updateBlob;

	OutStr(TEXT("In SDPUtilUpdateRecord\n"));
	updateBlob->pRecordHandle  = &(pSDPUtilRecord->ServiceRecordHandle);
    updateBlob->pSdpVersion	   = &ulSdpVersion;
	updateBlob->fSecurity      = 0;
	updateBlob->fOptions       = 0;

	updateBlob->ulRecordLength = MAX_SDPRECSIZE - sizeof(BTHNS_SETBLOB);
	if (dwResult = SDPUtilRecordToStream(updateBlob->pRecord, pSDPUtilRecord, &(updateBlob->ulRecordLength)))
	{
		OutStr(TEXT("SDPUtilRecordToStream fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	blob.cbSize    = sizeof(BTHNS_SETBLOB) + updateBlob->ulRecordLength - 1;

	memset(&Service,0,sizeof(Service));
	Service.dwSize = sizeof(Service);
	Service.lpBlob = &blob;
	Service.dwNameSpace = NS_BTH;

	dwResult = BthNsSetService(&Service, RNRSERVICE_REGISTER, dwControlFlags);
	if (dwResult)
	{
		OutStr(TEXT("BthNsSetService fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	OutStr(TEXT("SDP record updated, handle = 0x%08x\n"), pSDPUtilRecord->ServiceRecordHandle);
	OutStr(TEXT("New service record :\n"));
	SDPUtilPrintRecord(pSDPUtilRecord);

	return dwResult;
}


//
// Delete existing SDP service record
//
// hRecordHandle is the handle of the SDP record to be deleted
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilDeleteRecord(
	IN			ULONG			hRecordHandle)				// SDP record handle
{
	BLOB blob;
	BTHNS_SETBLOB delBlob;
	WSAQUERYSET Service;
	DWORD dwResult = 0;
	ULONG ulSdpVersion = BTH_SDP_VERSION;
	
	OutStr(TEXT("In SDPUtilDeleteRecord\n"));
	blob.cbSize    = sizeof(BTHNS_SETBLOB);
	blob.pBlobData = (PBYTE) &delBlob;

	memset(&delBlob,0,sizeof(delBlob));
	delBlob.pRecordHandle = &hRecordHandle;
    delBlob.pSdpVersion   = &ulSdpVersion;
	
	memset(&Service,0,sizeof(Service));
	Service.dwSize = sizeof(Service);
	Service.lpBlob = &blob;
	Service.dwNameSpace = NS_BTH;

	dwResult = BthNsSetService(&Service,RNRSERVICE_DELETE,0);
	if (dwResult)
	{
		OutStr(TEXT("BthNsSetService fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	OutStr(TEXT("SDP record deleted, handle = 0x%08x\n"), hRecordHandle);

	return dwResult;
}

//
// Inquery all bluetooth devices in proximity
//
// btDevices is the array of devices discovered by this inquery operation
// dwCount indicates the maximum nubmer of devices can be held in btDevices.
//         After the inquery it indicates the number of devices discovered.
// dwInqueryFlags specifies the flags for inquery operation
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD SDPUtilInquery(
	OUT			BTH_DEVICE_INFO	*btDevices, 
	IN OUT		DWORD			*pdwCount,
	IN			DWORD			dwInqueryFlags)
{
	WSAQUERYSET wsaQuery;
	DWORD dwFlags = LUP_CONTAINERS;
	HANDLE hLookup;
	UCHAR buf[MAX_SDPRECSIZE];
	LPWSAQUERYSET pResults = (LPWSAQUERYSET) buf;
	DWORD dwLen = sizeof(buf);
	DWORD i = 0; 
	DWORD dwResult = 0;
	BTH_DEVICE_INFO *pDeviceInfo = NULL;

	TCHAR *szName = NULL;
	//TCHAR szStr[255];
	PSOCKADDR_BTH pSockAddr;
	BT_ADDR *pba;

	BTH_DEVICE_INFO bthDeviceInfo;

	OutStr(TEXT("In SDPUtilInquery\n"));
	memset(&wsaQuery, 0, sizeof(WSAQUERYSET));
	wsaQuery.dwSize      = sizeof(WSAQUERYSET);
	wsaQuery.dwNameSpace = NS_BTH;

	dwResult = BthNsLookupServiceBegin(&wsaQuery,dwFlags,&hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceBegin fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	dwFlags = dwInqueryFlags;

	while (ERROR_SUCCESS == (dwResult = BthNsLookupServiceNext(hLookup, dwFlags, &dwLen, pResults))) 
	{

		szName = pResults->lpszServiceInstanceName;
		pSockAddr = (PSOCKADDR_BTH) pResults->lpcsaBuffer->RemoteAddr.lpSockaddr;
		pba = (BT_ADDR*)&pSockAddr->btAddr;
		
		memset(&bthDeviceInfo, 0, sizeof(bthDeviceInfo));

		if (dwInqueryFlags & LUP_RETURN_NAME)
		{
			if (pResults->lpszServiceInstanceName)
			{
				_tcscpy(bthDeviceInfo.name, pResults->lpszServiceInstanceName);
			}
			else
			{
				_tcscpy(bthDeviceInfo.name, TEXT("( null )\n"));
			}
		}

		if (dwInqueryFlags & LUP_RETURN_ADDR)
		{
			memcpy(&(bthDeviceInfo.address), pba, sizeof(BT_ADDR));
		}
		
		if (dwInqueryFlags & LUP_RETURN_BLOB)
		{
			memcpy(&(bthDeviceInfo.address), &(((BthInquiryResult *)pResults->lpBlob->pBlobData)->ba), sizeof(BT_ADDR));
		}

		OutStr(TEXT("BthNsLookupServiceNext (i=%d, retVal=0x%08x) name = %s, address=0x%04x%08x\n"), i, dwResult, bthDeviceInfo.name, GET_NAP(bthDeviceInfo.address), GET_SAP(bthDeviceInfo.address));

		dwLen = sizeof(buf);

		// copy results to btDevices
		if (i < *pdwCount)
		{
			memcpy(&btDevices[i], &bthDeviceInfo, sizeof(bthDeviceInfo));
		}
		i++;
	}
	OutStr(TEXT("Final BthNsLookupServiceNext returns 0x%08x,GetLastError=0x%08x\n"), dwResult, GetLastError());

	dwResult = BthNsLookupServiceEnd(hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceEnd fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	if (i < *pdwCount)
		*pdwCount = i;

	return dwResult;
}

//
// Do a SDP service search on a specific device
//
// btAddr is the bluetooth address of the device to search
// pServiceSearchPattern is a list of UUID16 to form the service search pattern
// hRecordHandles is the array of services handles returned back by the search
// dwCount indicates the maximum nubmer of service records can be held in hRecordHandles.
//         After the inquery it indicates the number of services discovered.
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilServiceSearch(				 
	IN			BT_ADDR			btAddr,
	IN			CUUIDList		*pServiceSearchPattern, 
	OUT			ULONG			*hRecordHandles,
	IN OUT		DWORD			*pdwCount)
{
	DWORD dwResult = 0;
	SdpQueryUuid   UUIDs[MAX_UUIDS_IN_QUERY];
	unsigned short cUUIDs;
	unsigned short cMaxHandles = MAX_HANDLES;
	WSAQUERYSET         wsaQuery;
	BTHNS_RESTRICTIONBLOB rBlob;
	BLOB blob;
	DWORD          dwFlags = 0;
	HANDLE hLookup;
	UCHAR buf[MAX_SDPRECSIZE];
	LPWSAQUERYSET pResults = (LPWSAQUERYSET) buf;
	DWORD dwSize = sizeof(buf);
	unsigned long  *pHandles;
	unsigned long  ulReturnedHandles = 0;
	DWORD i;

	OutStr(TEXT("In SDPUtilServiceSearch\n"));
	// set LUP_RES_SERVICE flag if btAddr is zero indicating local machine 
	if (0 == btAddr)
		dwFlags = LUP_RES_SERVICE;

	// copy uuids from pServiceSearchPattern
	for (i = 0; i < pServiceSearchPattern->Count(); i ++)
	{
		UUIDs[i].u.uuid16 = pServiceSearchPattern->Get(i);
		UUIDs[i].uuidType = SDP_ST_UUID16;
	}
	cUUIDs = (unsigned short) i;

	// setup query
	memset(&rBlob,0,sizeof(rBlob));
	rBlob.type      = SDP_SERVICE_SEARCH_REQUEST;
	memcpy(rBlob.uuids, UUIDs, cUUIDs*sizeof(SdpQueryUuid));

	blob.cbSize = sizeof(BTHNS_RESTRICTIONBLOB);
	blob.pBlobData = (BYTE*)&rBlob;

	CSADDR_INFO csAddr;
	SOCKADDR_BTH sockBT;
	
	memset(&wsaQuery, 0, sizeof(WSAQUERYSET));
	wsaQuery.dwSize      = sizeof(WSAQUERYSET);
	wsaQuery.dwNameSpace = NS_BTH;
	wsaQuery.lpBlob      = &blob;

	wsaQuery.lpcsaBuffer = &csAddr;
	memset(&csAddr, 0, sizeof(CSADDR_INFO));
	memset(&sockBT, 0, sizeof(SOCKADDR_BTH));
	memcpy(&(sockBT.btAddr), &btAddr, sizeof(BT_ADDR));
	csAddr.RemoteAddr.lpSockaddr = (LPSOCKADDR) &sockBT;
	csAddr.RemoteAddr.iSockaddrLength  = sizeof(SOCKADDR_BTH);

	// begin lookup
	dwResult = BthNsLookupServiceBegin(&wsaQuery, dwFlags, &hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceBegin fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// Get Results
	dwResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pResults);
	if (ERROR_SUCCESS == dwResult) 
	{
		ulReturnedHandles = (pResults->lpBlob->cbSize) / sizeof(ULONG);
		pHandles          = (unsigned long *) pResults->lpBlob->pBlobData;

		OutStr(TEXT("BthNsLookupServiceNext returned %u handles\n"), ulReturnedHandles);
		for (i = 0; i < ulReturnedHandles; i++)  {
			OutStr(TEXT("Handle[%u] = 0x%08x\n"), i, pHandles[i]);
		}
	}
	else
	{
		OutStr(TEXT("BthNsLookupServiceNext fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// end lookup
	dwResult = BthNsLookupServiceEnd(hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceEnd fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// copy results
	if (ulReturnedHandles < *pdwCount)
	{
		*pdwCount = ulReturnedHandles;
	}

	memcpy(hRecordHandles, pHandles, *pdwCount * sizeof(ULONG));

	return dwResult;
}

//
// Set a specific attribute in SDPUtilRecord
//
// pSDPUtilRecord is the record to set
// usAttribute is the attribute to set
// pNodeData is the attribute value to set to
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilSetAttribute(
	OUT			SDPUtilRecord		*pSDPUtilRecord, 
	IN			USHORT				usAttribute, 
	IN			NodeData			*pNodeData)
{
	DWORD dwResult = 0;
	DWORD i, j;

	OutStr(TEXT("In SDPUtilSetAttribute\n"));
	OutStr(TEXT("Set attribute 0x%04x\n"), usAttribute);

	switch (usAttribute)
	{
	//
	// 0x0000 ServiceRecordHandle
	//
	case SDP_ATTRIB_RECORD_HANDLE:
		if ((pNodeData->type == SDP_TYPE_UINT) && (pNodeData->specificType == SDP_ST_UINT32))
		{
			pSDPUtilRecord->ServiceRecordHandle = pNodeData->u.uint32;
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_UINT, SDP_ST_UINT32,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0001 SerivceClassIDList
	//
	case SDP_ATTRIB_CLASS_ID_LIST:
		if ((pNodeData->type == SDP_TYPE_CONTAINER) && (pNodeData->specificType == SDP_ST_NONE))
		{
			ULONG ulNodeCount;
			ISdpNodeContainer *pContainer;
			NodeData node;
			NodeContainerType nodeType;
			HRESULT hr;
			
			pContainer = pNodeData->u.container;

			// check countainer type
			hr = pContainer->GetType(&nodeType);
			if (FAILED(hr))
			{
				OutStr(TEXT("GetNodeCount failed on SDP_ATTRIB_CLASS_ID_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			if (nodeType != NodeContainerTypeSequence)
			{
				OutStr(TEXT("Container type mismatch for SDP_ATTRIB_CLASS_ID_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
				dwResult = 1;
				break;
			}

			// get node count
			hr = pContainer->GetNodeCount(&ulNodeCount);
			if (FAILED(hr))
			{
				OutStr(TEXT("GetNodeCount failed on SDP_ATTRIB_CLASS_ID_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}

			pSDPUtilRecord->SerivceClassIDList.Clear();
			
			// go through all nodes and get value
			for (i = 0; i < ulNodeCount; i ++)
			{
				hr = pContainer->GetNode(i, &node);
				if (FAILED(hr))
				{
					OutStr(TEXT("GetNode failed on SDP_ATTRIB_CLASS_ID_LIST, error = 0x%08x\n"), hr);
					dwResult = 1;
					break;
				}

				// check type
				if (node.type == SDP_TYPE_UUID)
				{
					switch (node.specificType)
					{
					case SDP_ST_UUID16:
						pSDPUtilRecord->SerivceClassIDList.Append(node.u.uuid16);
						break;
					case SDP_ST_UUID32:
						pSDPUtilRecord->SerivceClassIDList.Append((USHORT) node.u.uuid32);
						break;
					case SDP_ST_UUID128:
						pSDPUtilRecord->SerivceClassIDList.Append((USHORT) (node.u.uuid128.Data1));
						break;
					default:
						OutStr(TEXT("Unknown specific type : 0x%04x for type UUID\n"), node.specificType);
						break;
					}
				}
				else
				{
					OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x. Got 0x%04x\n"), 
						usAttribute,
						SDP_TYPE_UUID,
						pNodeData->type);
					dwResult = 1;
				}
			}
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_CONTAINER, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}

		break;

	//
	// 0x0002 ServiceRecordState
	//
	case SDP_ATTRIB_RECORD_STATE:
		if ((pNodeData->type == SDP_TYPE_UINT) && (pNodeData->specificType == SDP_ST_UINT32))
		{
			pSDPUtilRecord->ServiceRecordState = pNodeData->u.uint32;
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_UINT, SDP_ST_UINT32,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0003 ServiceID
	//
	case SDP_ATTRIB_SERVICE_ID: // need some work here to convert to UUID16
		if (pNodeData->type == SDP_TYPE_UUID)
		{
			switch (pNodeData->specificType)
			{
			case SDP_ST_UUID16:
				pSDPUtilRecord->ServiceID = pNodeData->u.uuid16;
				break;
			case SDP_ST_UUID32:
				pSDPUtilRecord->ServiceID = (USHORT) pNodeData->u.uuid32;
				break;
			case SDP_ST_UUID128:
				pSDPUtilRecord->ServiceID = (USHORT) (pNodeData->u.uuid128.Data1);
				break;
			default:
				OutStr(TEXT("Unknown specific type : 0x%04x for type UUID\n"), pNodeData->specificType);
				break;
			}
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x. Got 0x%04x\n"), 
				usAttribute,
				SDP_TYPE_UUID,
				pNodeData->type);
			dwResult = 1;
		}

		break;

	//
	// 0x0004 ProtocolDescriptorList
	//
	case SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST:
		if ((pNodeData->type == SDP_TYPE_CONTAINER) && (pNodeData->specificType == SDP_ST_NONE))
		{
			ULONG ulListNodeCount;
			ULONG ulStackNodeCount;
			ULONG ulNodeCount;
			ISdpNodeContainer *pListContainer;
			ISdpNodeContainer *pStackContainer;
			ISdpNodeContainer *pContainer;
			CProtocolDescriptorStack protocolDescriptorStack;
			ProtocolDescriptor protocolDescriptor;
			NodeData listNode;
			NodeData stackNode;
			NodeData node;
			NodeContainerType nodeType;
			BOOL	bSequence = FALSE;  // weather it is a data element sequence or data element alternative
			HRESULT hr;
			
			pListContainer = pNodeData->u.container;

			// check ListCountainer type
			hr = pListContainer->GetType(&nodeType);
			if (FAILED(hr))
			{
				OutStr(TEXT("pListContainer->GetType failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			if (nodeType == NodeContainerTypeAlternative)
			{
				// get list node count
				hr = pListContainer->GetNodeCount(&ulListNodeCount);
				if (FAILED(hr))
				{
					OutStr(TEXT("pListContainer->GetNodeCount failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
					dwResult = 1;
					break;
				}
			}
			else
			{
				ulListNodeCount = 1;
				bSequence = TRUE;
			}

			pSDPUtilRecord->ProtocolDescriptorList.Clear();
			
			// go through all list nodes and get value
			for (i = 0; i < ulListNodeCount; i ++)
			{
				if (!bSequence)
				{
					hr = pListContainer->GetNode(i, &listNode);
					if (FAILED(hr))
					{
						OutStr(TEXT("GetNode failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}
				}
				else
				{
					listNode.type = SDP_TYPE_CONTAINER;
					listNode.specificType = SDP_ST_NONE;
					listNode.u.container = pNodeData->u.container;
				}
					
				if ((listNode.type == SDP_TYPE_CONTAINER) && (listNode.specificType == SDP_ST_NONE))
				{
					pStackContainer = listNode.u.container;

					// check pStackContainer type
					hr = pStackContainer->GetType(&nodeType);
					if (FAILED(hr))
					{
						OutStr(TEXT("pStackContainer->GetType failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}
					if (nodeType != NodeContainerTypeSequence)
					{
						OutStr(TEXT("pStackContainer type mismatch for SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
						dwResult = 1;
						break;
					}

					// get pStackContainer count
					hr = pStackContainer->GetNodeCount(&ulStackNodeCount);
					if (FAILED(hr))
					{
						OutStr(TEXT("pStackContainer->GetNodeCount failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}

					protocolDescriptorStack.Clear();

					// go through all pStackContainer nodes and get value
					for (j = 0; j < ulStackNodeCount; j ++)
					{
						hr = pStackContainer->GetNode(j, &stackNode);
						if (FAILED(hr))
						{
							OutStr(TEXT("pStackContainer->GetNode failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
							dwResult = 1;
							break;
						}

						if ((stackNode.type == SDP_TYPE_CONTAINER) && (listNode.specificType == SDP_ST_NONE))
						{
							pContainer = stackNode.u.container;

							// check pContainer type
							hr = pContainer->GetType(&nodeType);
							if (FAILED(hr))
							{
								OutStr(TEXT("pContainer->GetType failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
								dwResult = 1;
								break;
							}
							if (nodeType != NodeContainerTypeSequence)
							{
								OutStr(TEXT("pContainer type mismatch for SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
								dwResult = 1;
								break;
							}

							// get pContainer count
							hr = pContainer->GetNodeCount(&ulNodeCount);
							if (FAILED(hr))
							{
								OutStr(TEXT("pContainer->GetNodeCount failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
								dwResult = 1;
								break;
							}

							// ProtocolDescriptor has possibly three elements. 
							// The second one is port and the third one is version
							// protocol
							pContainer->GetNode(0, &node);

							if (node.type == SDP_TYPE_UUID)
							{
								switch (node.specificType)
								{
								case SDP_ST_UUID16:
									protocolDescriptor.uuid = node.u.uuid16;
									break;
								case SDP_ST_UUID32:
									protocolDescriptor.uuid = (USHORT) node.u.uuid32;
									break;
								case SDP_ST_UUID128:
									protocolDescriptor.uuid = (USHORT) (node.u.uuid128.Data1);
									break;
								default:
									OutStr(TEXT("Unknown specific type : 0x%04x for type UUID\n"), node.specificType);
									break;
								}
							}
							else
							{
								OutStr(TEXT("Type mismatch for attribute 0x%04x when parsing protocol value. Expected 0x%04x. Got 0x%04x.\n"), 
									usAttribute,
									SDP_TYPE_UUID,
									node.type);
								dwResult = 1;
								break;
							}

							// get node count
							hr = pContainer->GetNodeCount(&ulNodeCount);
							if (FAILED(hr))
							{
								OutStr(TEXT("pContainer->GetNodeCount failed on SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
								dwResult = 1;
								break;
							}

							// Assume the second node is port number
							if (ulNodeCount > 1)
							{
								pContainer->GetNode(1, &node);
								if (node.type == SDP_TYPE_UINT)
								{
									protocolDescriptor.usPort = node.u.uint16;
								}
								else
								{
									OutStr(TEXT("Type mismatch for attribute 0x%04x when parsing port number. Expected 0x%04x. Got 0x%04x.\n"), 
										usAttribute,
										SDP_TYPE_UINT,
										node.type);
									dwResult = 1;
									break;
								}
							}
							else
							{
								protocolDescriptor.usPort = 0;
							}

							// Assume the third node is version number
							if (ulNodeCount > 2)
							{
								pContainer->GetNode(2, &node);
								if ((node.type == SDP_TYPE_UINT) && (node.specificType == SDP_ST_UINT16))
								{
									protocolDescriptor.usVersion = node.u.uint16;
								}
								else
								{
									OutStr(TEXT("Type mismatch for attribute 0x%04x when parsing version number. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
										usAttribute,
										SDP_TYPE_UINT, SDP_ST_UINT16,
										node.type, node.specificType);
									dwResult = 1;
									break;
								}
							}
							else
							{
								protocolDescriptor.usVersion = 0;
							}
				
							protocolDescriptorStack.Append(&protocolDescriptor);
						}
						else
						{
							OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
								usAttribute,
								SDP_TYPE_CONTAINER, SDP_ST_NONE,
								node.type, node.specificType);
							dwResult = 1;
							break;
						}

					}
					pSDPUtilRecord->ProtocolDescriptorList.Append(&protocolDescriptorStack);
				}
				else
				{
					OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
						usAttribute,
						SDP_TYPE_CONTAINER, SDP_ST_NONE,
						pNodeData->type, pNodeData->specificType);
					dwResult = 1;
				}
			}
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_CONTAINER, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}

		break;

	//
	// 0x0005 BrowseGroupList
	//
	case SDP_ATTRIB_BROWSE_GROUP_LIST:
		if ((pNodeData->type == SDP_TYPE_CONTAINER) && (pNodeData->specificType == SDP_ST_NONE))
		{
			ULONG ulNodeCount;
			ISdpNodeContainer *pContainer;
			NodeData node;
			NodeContainerType nodeType;
			HRESULT hr;
			
			pContainer = pNodeData->u.container;

			// check countainer type
			hr = pContainer->GetType(&nodeType);
			if (FAILED(hr))
			{
				OutStr(TEXT("GetNodeCount failed on SDP_ATTRIB_BROWSE_GROUP_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			if (nodeType != NodeContainerTypeSequence)
			{
				OutStr(TEXT("Container type mismatch for SDP_ATTRIB_BROWSE_GROUP_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
				dwResult = 1;
				break;
			}

			// get node count
			hr = pContainer->GetNodeCount(&ulNodeCount);
			if (FAILED(hr))
			{
				OutStr(TEXT("GetNodeCount failed on SDP_ATTRIB_BROWSE_GROUP_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			
			// go through all nodes and get value
			pSDPUtilRecord->BrowseGroupList.Clear();

			for (i = 0; i < ulNodeCount; i ++)
			{
				hr = pContainer->GetNode(i, &node);
				if (FAILED(hr))
				{
					OutStr(TEXT("GetNode failed on SDP_ATTRIB_BROWSE_GROUP_LIST, error = 0x%08x\n"), hr);
					dwResult = 1;
					break;
				}

				// check type
				if (node.type == SDP_TYPE_UUID)
				{
					switch (node.specificType)
					{
					case SDP_ST_UUID16:
						pSDPUtilRecord->BrowseGroupList.Append(node.u.uuid16);
						break;
					case SDP_ST_UUID32:
						pSDPUtilRecord->BrowseGroupList.Append((USHORT) node.u.uuid32);
						break;
					case SDP_ST_UUID128:
						pSDPUtilRecord->BrowseGroupList.Append((USHORT) (node.u.uuid128.Data1));
						break;
					default:
						OutStr(TEXT("Unknown specific type : 0x%04x for type UUID\n"), node.specificType);
						break;
					}
				}
				else
				{
					OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x. Got 0x%04x\n"), 
						usAttribute,
						SDP_TYPE_UUID,
						pNodeData->type);
					dwResult = 1;
				}
			}
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_CONTAINER, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}

		break;

	//
	// 0x0006 LanguageBaseAttributeIDList
	//
	case SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST:
		if ((pNodeData->type == SDP_TYPE_CONTAINER) && (pNodeData->specificType == SDP_ST_NONE))
		{
			ULONG ulListNodeCount;
			ULONG ulNodeCount;
			ISdpNodeContainer *pListContainer;
			ISdpNodeContainer *pContainer;
			LanguageBaseAttributeID languateBaseAttributeID;
			NodeData listNode;
			NodeData node;
			NodeContainerType nodeType;

			HRESULT hr;
			
			pListContainer = pNodeData->u.container;

			// check ListCountainer type
			hr = pListContainer->GetType(&nodeType);
			if (FAILED(hr))
			{
				OutStr(TEXT("pListContainer->GetType failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			if (nodeType != NodeContainerTypeSequence)
			{
				OutStr(TEXT("pListContainer type mismatch for SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
				dwResult = 1;
				break;
			}

			// get list node count
			hr = pListContainer->GetNodeCount(&ulListNodeCount);
			if (FAILED(hr))
			{
				OutStr(TEXT("pListContainer->GetNodeCount failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			
			// go through all list nodes and get value
			pSDPUtilRecord->LanguageBaseAttributeIDList.Clear();

			for (i = 0; i < ulListNodeCount; i ++)
			{
				hr = pListContainer->GetNode(i, &listNode);
				if (FAILED(hr))
				{
					OutStr(TEXT("GetNode failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
					dwResult = 1;
					break;
				}

				if ((listNode.type == SDP_TYPE_CONTAINER) && (listNode.specificType == SDP_ST_NONE))
				{
					pContainer = listNode.u.container;

					// check countainer type
					hr = pContainer->GetType(&nodeType);
					if (FAILED(hr))
					{
						OutStr(TEXT("pContainer->GetType failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}
					if (nodeType != NodeContainerTypeSequence)
					{
						OutStr(TEXT("pContainer type mismatch for SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
						dwResult = 1;
						break;
					}

					// get node count
					hr = pContainer->GetNodeCount(&ulNodeCount);
					if (FAILED(hr))
					{
						OutStr(TEXT("pContainer->GetNodeCount failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}

					// LanguageBaseAttributeID are triplets
					if (ulNodeCount != 3)
					{
						OutStr(TEXT("Number of nodes in container mismatch in SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST. Expected 3. Got %u.\n"), ulNodeCount);
						dwResult = 1;
						break;
					}

					pContainer->GetNode(0, &node);

					// language
					if ((node.type == SDP_TYPE_UINT) && (node.specificType == SDP_ST_UINT16))
					{
						languateBaseAttributeID.usLanguage = node.u.uint16;
					}
					else
					{
						OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
							usAttribute,
							SDP_TYPE_UINT, SDP_ST_UINT16,
							node.type, node.specificType);
						dwResult = 1;
						break;
					}


					// encoding
					pContainer->GetNode(1, &node);

					if ((node.type == SDP_TYPE_UINT) && (node.specificType == SDP_ST_UINT16))
					{
						languateBaseAttributeID.usEncoding = node.u.uint16;
					}
					else
					{
						OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
							usAttribute,
							SDP_TYPE_UINT, SDP_ST_UINT16,
							node.type, node.specificType);
						dwResult = 1;
						break;
					}

					// base attribute ID
					pContainer->GetNode(2, &node);

					if ((node.type == SDP_TYPE_UINT) && (node.specificType == SDP_ST_UINT16))
					{
						languateBaseAttributeID.usBaseAttributeID = node.u.uint16;
					}
					else
					{
						OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
							usAttribute,
							SDP_TYPE_UINT, SDP_ST_UINT16,
							node.type, node.specificType);
						dwResult = 1;
						break;
					}

					pSDPUtilRecord->LanguageBaseAttributeIDList.Append(&languateBaseAttributeID);
				}
				else
				{
					OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
						usAttribute,
						SDP_TYPE_CONTAINER, SDP_ST_NONE,
						node.type, node.specificType);
					dwResult = 1;
					break;
				}

			}	

		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_CONTAINER, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}

		break;

	//
	// 0x0007 ServiceInfoTimeToLive
	//
	case SDP_ATTRIB_INFO_TIME_TO_LIVE:
		if ((pNodeData->type == SDP_TYPE_UINT) && (pNodeData->specificType == SDP_ST_UUID32))
		{
			pSDPUtilRecord->ServiceInfoTimeToLive = pNodeData->u.uint32;
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_UINT, SDP_ST_UUID32,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0008 ServiceAvailability
	//
	case SDP_ATTRIB_AVAILABILITY:
		if ((pNodeData->type == SDP_TYPE_UINT) && (pNodeData->specificType == SDP_ST_UINT8))
		{
			pSDPUtilRecord->ServiceAvailability = pNodeData->u.uint8;
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_UINT, SDP_ST_UINT8,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0009 BluetoothProfileDescriptorList
	//
	case SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST:
		if ((pNodeData->type == SDP_TYPE_CONTAINER) && (pNodeData->specificType == SDP_ST_NONE))
		{
			ULONG ulListNodeCount;
			ULONG ulNodeCount;
			ISdpNodeContainer *pListContainer;
			ISdpNodeContainer *pContainer;
			BluetoothProfileDescriptor bluetoothProfileDescriptor;
			NodeData listNode;
			NodeData node;
			NodeContainerType nodeType;

			HRESULT hr;
			
			pListContainer = pNodeData->u.container;

			// check ListCountainer type
			hr = pListContainer->GetType(&nodeType);
			if (FAILED(hr))
			{
				OutStr(TEXT("pListContainer->GetType failed on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			if (nodeType != NodeContainerTypeSequence)
			{
				OutStr(TEXT("pListContainer type mismatch for SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
				dwResult = 1;
				break;
			}

			// get list node count
			hr = pListContainer->GetNodeCount(&ulListNodeCount);
			if (FAILED(hr))
			{
				OutStr(TEXT("pListContainer->GetNodeCount failed on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
				dwResult = 1;
				break;
			}
			
			// go through all list nodes and get value
			pSDPUtilRecord->BluetoothProfileDescriptorList.Clear();

			for (i = 0; i < ulListNodeCount; i ++)
			{
				hr = pListContainer->GetNode(i, &listNode);
				if (FAILED(hr))
				{
					OutStr(TEXT("GetNode failed on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
					dwResult = 1;
					break;
				}

				if ((listNode.type == SDP_TYPE_CONTAINER) && (listNode.specificType == SDP_ST_NONE))
				{
					pContainer = listNode.u.container;

					// check countainer type
					hr = pContainer->GetType(&nodeType);
					if (FAILED(hr))
					{
						OutStr(TEXT("pContainer->GetType failed on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}
					if (nodeType != NodeContainerTypeSequence)
					{
						OutStr(TEXT("pContainer type mismatch for SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST. Expected NodeContainerTypeSequence. Got 0x%04x.\n"), nodeType);
						dwResult = 1;
						break;
					}

					// get node count
					hr = pContainer->GetNodeCount(&ulNodeCount);
					if (FAILED(hr))
					{
						OutStr(TEXT("pContainer->GetNodeCount failed on SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}

					// BluetoothProfileDescriptorList has possibly two elements. Assume the second one is version
					// profile
					pContainer->GetNode(0, &node);

					if (node.type == SDP_TYPE_UUID)
					{
						switch (node.specificType)
						{
						case SDP_ST_UUID16:
							bluetoothProfileDescriptor.uuid = node.u.uuid16;
							break;
						case SDP_ST_UUID32:
							bluetoothProfileDescriptor.uuid = (USHORT) node.u.uuid32;
							break;
						case SDP_ST_UUID128:
							bluetoothProfileDescriptor.uuid = (USHORT) (node.u.uuid128.Data1);
							break;
						default:
							OutStr(TEXT("Unknown specific type : 0x%04x for type UUID\n"), node.specificType);
							break;
						}
					}
					else
					{
						OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x. Got 0x%04x.\n"), 
							usAttribute,
							SDP_TYPE_UUID,
							node.type);
						dwResult = 1;
						break;
					}

					// version
					hr = pContainer->GetNodeCount(&ulNodeCount);
					if (FAILED(hr))
					{
						OutStr(TEXT("pContainer->GetNodeCount failed on SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, error = 0x%08x\n"), hr);
						dwResult = 1;
						break;
					}

					// assume the second node is version number
					if (ulNodeCount > 1)
					{
						pContainer->GetNode(1, &node);
						if ((node.type == SDP_TYPE_UINT) && (node.specificType == SDP_ST_UINT16))
						{
							bluetoothProfileDescriptor.usVersion = node.u.uint16;
						}
						else
						{
							OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
								usAttribute,
								SDP_TYPE_UINT, SDP_ST_UINT16,
								node.type, node.specificType);
							dwResult = 1;
							break;
						}
					}
					else
					{
						bluetoothProfileDescriptor.usVersion = 0;
					}

					pSDPUtilRecord->BluetoothProfileDescriptorList.Append(&bluetoothProfileDescriptor);
				}
				else
				{
					OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
						usAttribute,
						SDP_TYPE_CONTAINER, SDP_ST_NONE,
						node.type, node.specificType);
					dwResult = 1;
					break;
				}

			}	

		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_CONTAINER, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}

		break;

	//
	// 0x000A DocumentationURL
	//
	case SDP_ATTRIB_DOCUMENTATION_URL:
		if ((pNodeData->type == SDP_TYPE_URL) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->DocumentationURL, pNodeData->u.url.val, pNodeData->u.url.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_URL, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x000B ClientExecutableURL
	//
	case SDP_ATTRIB_CLIENT_EXECUTABLE_URL:
		if ((pNodeData->type == SDP_TYPE_URL) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->ClientExecutableURL, pNodeData->u.url.val, pNodeData->u.url.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_URL, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x000C IconURL
	//
	case SDP_ATTRIB_ICON_URL:
		if ((pNodeData->type == SDP_TYPE_URL) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->IconURL, pNodeData->u.url.val, pNodeData->u.url.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_URL, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0000 + LanguageBaseAttributeID ServiceName
	//
	case 0x0100:
		if ((pNodeData->type == SDP_TYPE_STRING) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->ServiceName, pNodeData->u.str.val, pNodeData->u.str.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_STRING, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0001 + LanguageBaseAttributeID ServiceDescription
	//
	case 0x0101:
		if ((pNodeData->type == SDP_TYPE_STRING) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->ServiceDescription, pNodeData->u.str.val, pNodeData->u.str.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_STRING, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	//
	// 0x0002 + LanguageBaseAttributeID ProviderName
	//
	case 0x0102:
		if ((pNodeData->type == SDP_TYPE_STRING) && (pNodeData->specificType == SDP_ST_NONE))
		{
			mbstowcs(pSDPUtilRecord->ProviderName, pNodeData->u.str.val, pNodeData->u.str.length);
		}
		else
		{
			OutStr(TEXT("Type mismatch for attribute 0x%04x. Expected 0x%04x 0x%04x. Got 0x%04x 0x%04x.\n"), 
				usAttribute,
				SDP_TYPE_STRING, SDP_ST_NONE,
				pNodeData->type, pNodeData->specificType);
			dwResult = 1;
		}
		break;

	default:
		OutStr(TEXT("Unknow attribute : 0x%04x\n"), usAttribute);
		break;
	}

	return dwResult;
}

//
// Skip type and size field of a data element sequence
// 
// pCurrent pointer to data element sequence
//
// RETURN	pointer to content of data element sequence
//			NULL on error	
//
UCHAR *
SkipElementTypeAndSize(
	IN			UCHAR				*pCurrent)
{
	USHORT usBytesToSkip = 0;

	if (((*pCurrent) & 0xf8)>>3 != 6)
	{
		OutStr(TEXT("Not a data element sequence!\n"));
		return NULL;
	}

	switch((*pCurrent) & 0x07)
	{
	case 5:
		usBytesToSkip = 1;
		break;
	case 6:
		usBytesToSkip = 2;
		break;
	case 7:
		usBytesToSkip = 4;
		break;
	default:
		OutStr(TEXT("Unknow size desciptor!\n"));
		return NULL;
	}

	return &(pCurrent[usBytesToSkip + 1]);
}


//
// Get the size of a data element sequence including type and size field
// 
// pCurrent pointer to data element sequence
//
// RETURN	szie of data element sequence
//			0 on error	
//
DWORD
GetElementSize(
	IN			UCHAR				*pCurrent)
{
	DWORD dwSize = 0;

	if (((*pCurrent) & 0xf8)>>3 != 6)
	{
		OutStr(TEXT("Not a data element sequence!\n"));
		return NULL;
	}

	switch((*pCurrent) & 0x07)
	{
	case 5:
		dwSize = *((UCHAR *) &pCurrent[1]) + 2;
		break;
	case 6:
		dwSize = *((USHORT *) &pCurrent[1]) + 3;
		break;
	case 7:
		dwSize = *((DWORD *) pCurrent[1]) + 5;
		break;
	default:
		OutStr(TEXT("Unknow size desciptor!\n"));
		return NULL;
	}

	return dwSize;
}

//
// Convert a byte stream to SDPUtilRecord
//
// pSDPUtilRecord is the record to convert
// pStream is the stream to conver to
// dwStreamSize is the size of the stream
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilStreamToRecord(
	OUT			SDPUtilRecord		*pSDPUtilRecord,
	IN			UCHAR				*pStream,
	IN			DWORD				dwStreamSize)
{
	IClassFactory *pCFRecord    = NULL;
	IClassFactory *pCFContainer = NULL;
    ISdpRecord *pRecord         = NULL;
	ISdpStream *pIStream		= NULL;
	NodeData nodeData;
	USHORT *pList = NULL;
	ULONG ulListSize = 0;

	DWORD dwResult = 0;
	DWORD i;
	CSDPUtilWalk sdpUtilWalk;

	OutStr(TEXT("In SDPUtilStreamToRecord\n"));
	//
	// Initialize COM
	//
    CoInitializeEx(NULL,COINIT_MULTITHREADED);
    HRESULT hr = CoGetClassObject(__uuidof(SdpRecord),
                     CLSCTX_INPROC_SERVER,
                     NULL,
                     IID_IClassFactory,
                     (PVOID*)&pCFRecord); 

    if (pCFRecord == NULL) {
        OutStr(TEXT("CoGetClassObject(uuidof(SdpRecord) fails, error=0x%08x\n"), hr);
		dwResult = 1;
        goto Finished;
    }

	//
	// Validate stream
	//
    hr = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);

    ULONG ulErrorByte;

	hr = pIStream->Validate(pStream, dwStreamSize, &ulErrorByte);
    if (FAILED (hr))
    {
		OutStr(TEXT("Validate pStream fails at byte %u, error = 0x%08x\n"), ulErrorByte, hr);
		dwResult = 1;
		goto Finished;
    }

	//
	// Build record
	//
    hr = pCFRecord->CreateInstance(NULL, 
                              __uuidof(ISdpRecord),
                              (PVOID*) &pRecord);
    if (pRecord == NULL) {
		OutStr(TEXT("CreateInstance on ISdpRecord fails, error = 0x%08x\n"), hr);
		dwResult = 1;
        goto Finished;
    }

	hr = pRecord->CreateFromStream(pStream, dwStreamSize);
	if (FAILED(hr))
	{
		OutStr(TEXT("CreateFromStream fails, error = 0x%08x\n"), hr);
		dwResult = 1;
		goto Finished;
	}

	//
	// Walk this record
	//
	if (gpCriticalSection)
	{
		EnterCriticalSection(gpCriticalSection);
	}
	
	OutStr(TEXT("Walking pRecord\n"));
	hr = pRecord->Walk(&sdpUtilWalk);

	if (gpCriticalSection)
	{
		LeaveCriticalSection(gpCriticalSection);
	}

	//
	// Get attribute list
	//
	hr = pRecord->GetAttributeList(&pList, &ulListSize);
	if (FAILED(hr))
	{
		OutStr(TEXT("GetAttributeList fails, error = 0x%08x\n"), hr);
		dwResult = 1;
		goto Finished;
	}

	//
	// Additional check
	//
	if (!FAILED(pRecord->GetAttribute(SDP_ATTRIB_RECORD_HANDLE, &nodeData)))
	{
		UCHAR *pAttibStream = NULL;
		ULONG ulSize = 0;
		ULONG ulUint32 = 0;

		pRecord->GetAttributeAsStream(SDP_ATTRIB_RECORD_HANDLE, &pAttibStream, &ulSize);
		pIStream->RetrieveUint32(pAttibStream + 1, &ulUint32);	// skip type and size
		pIStream->ByteSwapUint32(ulUint32, &ulUint32);			// convert order

		if (nodeData.u.uint32 != ulUint32)
		{
			OutStr(TEXT("RetrieveUint32 fails. UINT32 = 0x%08x, expected 0x%08x\n"), ulUint32, nodeData.u.uint32);
			dwResult = 1;
			goto Finished;
		}

		pRecord->SetAttributeFromStream(SDP_ATTRIB_RECORD_HANDLE, pAttibStream, ulSize);

		CoTaskMemFree(pAttibStream);
	}
	//
	// Go though the attribute list and add each attribute to SDPUtilRecord
	//
	OutStr(TEXT("Total attributes %u\n"), ulListSize);

	for (i = 0; i < ulListSize; i ++)
	{
		hr = pRecord->GetAttribute(pList[i], &nodeData);
		if (FAILED(hr))
		{
			OutStr(TEXT("GetAttribute fails, error = 0x%08x\n"), hr);
			dwResult = 1;
			goto Finished;
		}

		dwResult = SDPUtilSetAttribute(pSDPUtilRecord, pList[i], &nodeData);
	}

	//
	// Clean up
	//
	pIStream->Release();
	pIStream = NULL;

	pRecord->Release();
	pRecord = NULL;

	pCFRecord->Release();
	pCFRecord = NULL;

	CoTaskMemFree(pList);
	pList = NULL;

	OutStr(TEXT("Stream : \n"));
	SDPUtilPrintStream(pStream, dwStreamSize);
	OutStr(TEXT("Converted to service record :\n"));
	SDPUtilPrintRecord(pSDPUtilRecord);

Finished:
	if (pRecord)
		pRecord->Release();

	if (pIStream)
		pIStream->Release();

	if (pCFRecord)
		pCFRecord->Release();

	if (pList)
		CoTaskMemFree(pList);

	CoUninitialize();

	return dwResult;
}

//
// Do a SDP attribute search on a specific record of a specific device
//
// btAddr is the bluetooth address of the device to search
// recordHandle is the handle of record to search
// attributeIDRangeList is a list of AttributeIDRange
// pSDPUtilRecord is the SDP records returned by the search
// dwCount indicates the maximum nubmer of service records can be held in SDPUtilRecords.
//         After the inquery it indicates the number of services records discovered.
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilAttributeSearch(
	IN			BT_ADDR					btAddr, 
	IN			ULONG					recordHandle, 
	IN			CAttributeIDRangeList	*attributeIDRangeList, 
	OUT			SDPUtilRecord			*pSDPUtilRecord)
{
	DWORD dwResult = 0;
	SdpAttributeRange  attribRange[MAX_ATTRIBUTES];
	unsigned long      cAttributes;
	WSAQUERYSET        wsaQuery;
	BLOB blob;
	UCHAR buf[MAX_SDPRECSIZE];
	LPWSAQUERYSET pResults = (LPWSAQUERYSET) buf;
	DWORD dwSize = sizeof(buf);
	UCHAR buf2[MAX_SDPRECSIZE];
	PBTHNS_RESTRICTIONBLOB prBlob =  (PBTHNS_RESTRICTIONBLOB) buf2;
	DWORD              dwFlags = 0;
	CSADDR_INFO csAddr;
	SOCKADDR_BTH sockBT;
	HANDLE hLookup;
	DWORD i;

	OutStr(TEXT("In SDPUtilAttributeSearch\n"));

	// set LUP_RES_SERVICE flag if btAddr is zero indicating local machine 
	if (0 == btAddr)
		dwFlags = LUP_RES_SERVICE;

	// setup attribRange
	for (i = 0; i < attributeIDRangeList->Count(); i ++)
	{
		attribRange[i].minAttribute = attributeIDRangeList->Get(i)->minAttribute;
		attribRange[i].maxAttribute = attributeIDRangeList->Get(i)->maxAttribute;
	}
	cAttributes = i;

	// setup query
	unsigned long ulBlobLen = sizeof(BTHNS_RESTRICTIONBLOB) + (cAttributes-1)* sizeof(SdpAttributeRange);
	prBlob->type          = SDP_SERVICE_ATTRIBUTE_REQUEST;
	prBlob->serviceHandle = recordHandle;
	prBlob->numRange      = cAttributes;
	memcpy(prBlob->pRange, attribRange, cAttributes * sizeof(SdpAttributeRange));

	blob.cbSize    = sizeof(BTHNS_RESTRICTIONBLOB) + (cAttributes-1)* sizeof(SdpAttributeRange);;
	blob.pBlobData = (BYTE*)prBlob;

	memset(&wsaQuery, 0, sizeof(WSAQUERYSET));
	wsaQuery.dwSize      = sizeof(WSAQUERYSET);
	wsaQuery.dwNameSpace = NS_BTH;
	wsaQuery.lpBlob      = &blob;

	wsaQuery.lpcsaBuffer = &csAddr;
	memset(&csAddr, 0, sizeof(CSADDR_INFO));
	memset(&sockBT, 0, sizeof(SOCKADDR_BTH));
	memcpy(&(sockBT.btAddr), &btAddr, sizeof(BT_ADDR));
	csAddr.RemoteAddr.lpSockaddr = (LPSOCKADDR) &sockBT;
	csAddr.RemoteAddr.iSockaddrLength  = sizeof(SOCKADDR_BTH);

	// begin lookup
	dwResult = BthNsLookupServiceBegin(&wsaQuery,dwFlags,&hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceBegin fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// get results
	dwResult = BthNsLookupServiceNext(hLookup,0,&dwSize,pResults);
	if (ERROR_SUCCESS == dwResult)  {
		OutStr(TEXT("BthNsLookupServiceNext return buffer size = 0x%d bytes\n"), pResults->lpBlob->cbSize);
	}
	else
	{
		OutStr(TEXT("BthNsLookupServiceNext fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// end lookup
	dwResult = BthNsLookupServiceEnd(hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceEnd fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	OutStr(TEXT("AttributeSearch returns stream :\n"));
	SDPUtilPrintStream(pResults->lpBlob->pBlobData, pResults->lpBlob->cbSize);

	// convert stream to SDUPUtilRecord
	dwResult = SDPUtilStreamToRecord(pSDPUtilRecord, pResults->lpBlob->pBlobData, pResults->lpBlob->cbSize);

	return dwResult;
}

//
// Convert a byte stream to multiple SDPUtilRecords
//
// pSDPUtilRecord is the record to convert
// pdwCount is the max number of record to covert. After conversion it is how many records converted
// pStream is the stream to conver
// dwStreamSize is the size of the stream
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilStreamToRecords(
	OUT			SDPUtilRecord		*pSDPUtilRecords,
	IN OUT		DWORD				*pdwCount,
	IN			UCHAR				*pStream,
	IN			DWORD				dwStreamSize)
{
	ISdpStream *pIStream = NULL;
	DWORD dwResult = 0;
	DWORD i;
	UCHAR * pCurrent;
	DWORD dwDataElementSize;

	OutStr(TEXT("In SDPUtilStreamToRecords\n"));
	//
	// Initialize COM
	//
    CoInitializeEx(NULL,COINIT_MULTITHREADED);

	//
	// Validate stream
	//
    HRESULT hr = CoCreateInstance(__uuidof(SdpStream),NULL,CLSCTX_INPROC_SERVER,
                            __uuidof(ISdpStream),(LPVOID *) &pIStream);

    ULONG ulErrorByte;

	hr = pIStream->Validate(pStream, dwStreamSize, &ulErrorByte);
    if (FAILED (hr))
    {
		OutStr(TEXT("Validate pStream fails at byte %u, error = 0x%08x\n"), ulErrorByte, hr);
		dwResult = 1;
		goto Finished;
    }

	pIStream->Release();
	pIStream = NULL;

	//
	// Build record
	//
	pCurrent = pStream;
	pCurrent = SkipElementTypeAndSize(pCurrent);
	if (pCurrent == NULL)
	{
		OutStr(TEXT("SkipElementTypeAndSize fails\n"));
		dwResult = 1;
		goto Finished;
	}
	for (i = 0; i < *pdwCount; i ++)
	{
		if (pCurrent >= (pStream + dwStreamSize))
			break;

		OutStr(TEXT("Record #%u\n"), i);
		dwDataElementSize = GetElementSize(pCurrent);
		if (dwDataElementSize == 0)
		{
			OutStr(TEXT("GetElementSize fails\n"));
			dwResult = 1;
			goto Finished;
		}
		SDPUtilStreamToRecord(&pSDPUtilRecords[i], pCurrent, dwDataElementSize);
		pCurrent += dwDataElementSize;
	}

	*pdwCount = i;

	//
	// Clean up
	//
Finished:
	if (pIStream)
		pIStream->Release();

	CoUninitialize();

	return dwResult;
}

//
// Do a SDP service attribute search on a specific device
//
// btAddr is the bluetooth address of the device to search
// pServiceSearchPattern is a list of UUID16 to form the service search pattern
// pAttributeIDRangeList is a list of UUID16 to form the attribute ID list
// pSDPUtilRecords is the array of SDP records returned by the search
// dwCount indicates the maximum nubmer of service records can be held in SDPUtilRecords.
//         After the inquery it indicates the number of services records discovered.
//
// RETURN 0 if success
//		  otherwise error code
//
DWORD 
SDPUtilServiceAttributeSearch(
	IN			BT_ADDR					btAddr, 
	IN			CUUIDList				*pServiceSearchPattern, 
	IN			CAttributeIDRangeList	*pAttributeIDRangeList, 
	OUT			SDPUtilRecord			*pSDPUtilRecords, 
	IN OUT		DWORD					*pdwCount)
{
	DWORD				dwResult = 0;
	SdpAttributeRange   attribRange[MAX_ATTRIBUTES];
	unsigned long       cAttributes = pAttributeIDRangeList->Count();	
	SdpQueryUuid        UUIDs[MAX_UUIDS_IN_QUERY];
	unsigned short      cUUIDs;
	DWORD               dwFlags = 0;
	DWORD				i;
	WSAQUERYSET         wsaQuery;
	PBTHNS_RESTRICTIONBLOB prBlob;
	BLOB blob;
	unsigned long ulBlobLen = sizeof(BTHNS_RESTRICTIONBLOB) + (cAttributes-1)* sizeof(SdpAttributeRange);
	UCHAR buf2[MAX_SDPRECSIZE];
	prBlob = (PBTHNS_RESTRICTIONBLOB) buf2;
	UCHAR buf[MAX_SDPRECSIZE];
	LPWSAQUERYSET pResults = (LPWSAQUERYSET) buf;
	DWORD dwSize  = sizeof(buf);
	HANDLE hLookup;

	OutStr(TEXT("In SDPUtilServiceAttributeSearch\n"));

	// set LUP_RES_SERVICE flag if btAddr is zero indicating local machine 
	if (0 == btAddr)
		dwFlags = LUP_RES_SERVICE;

	// copy uuids from pServiceSearchPattern
	for (i = 0; i < pServiceSearchPattern->Count(); i ++)
	{
		UUIDs[i].u.uuid16 = pServiceSearchPattern->Get(i);
		UUIDs[i].uuidType = SDP_ST_UUID16;
	}
	cUUIDs = (unsigned short) i;

	// setup attribRange
	for (i = 0; i < pAttributeIDRangeList->Count(); i ++)
	{
		attribRange[i].minAttribute = pAttributeIDRangeList->Get(i)->minAttribute;
		attribRange[i].maxAttribute = pAttributeIDRangeList->Get(i)->maxAttribute;
	}
	cAttributes = i;

	// Setup Query
	memset(buf2, 0, sizeof(buf2));

	prBlob->type          = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	prBlob->numRange      = cAttributes;
	memcpy(prBlob->pRange, attribRange, cAttributes*sizeof(SdpAttributeRange));
	memcpy(prBlob->uuids, UUIDs, cUUIDs*sizeof(SdpQueryUuid));

	blob.cbSize    = ulBlobLen;
	blob.pBlobData = (BYTE*)prBlob;

	CSADDR_INFO csAddr;
	SOCKADDR_BTH sockBT;

	memset(&wsaQuery, 0, sizeof(WSAQUERYSET));
	wsaQuery.dwSize      = sizeof(WSAQUERYSET);
	wsaQuery.dwNameSpace = NS_BTH;
	wsaQuery.lpBlob      = &blob;

	wsaQuery.lpcsaBuffer = &csAddr;
	memset(&csAddr, 0, sizeof(CSADDR_INFO));
	memset(&sockBT, 0, sizeof(SOCKADDR_BTH));
	memcpy(&(sockBT.btAddr), &btAddr, sizeof(BT_ADDR));
	csAddr.RemoteAddr.lpSockaddr = (LPSOCKADDR) &sockBT;
	csAddr.RemoteAddr.iSockaddrLength  = sizeof(SOCKADDR_BTH);
	
	// begin lookup
	dwResult = BthNsLookupServiceBegin(&wsaQuery,dwFlags,&hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceBegin fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// get results
	dwResult = BthNsLookupServiceNext(hLookup, 0, &dwSize, pResults);
	if (ERROR_SUCCESS == dwResult)  {
		OutStr(TEXT("BthNsLookupServiceNext return buffer size = 0x%d bytes\n"), pResults->lpBlob->cbSize);
	}
	else
	{
		OutStr(TEXT("BthNsLookupServiceNext fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	// end lookup
	dwResult = BthNsLookupServiceEnd(hLookup);
	if (dwResult)
	{
		OutStr(TEXT("BthNsLookupServiceEnd fails, error = 0x%08x\n"), dwResult);
		return dwResult;
	}

	OutStr(TEXT("ServiceAttributeSearch returns stream :\n"));
	SDPUtilPrintStream(pResults->lpBlob->pBlobData, pResults->lpBlob->cbSize);

	// convert stream to SDUPUtilRecords
	dwResult = SDPUtilStreamToRecords(pSDPUtilRecords, pdwCount, pResults->lpBlob->pBlobData, pResults->lpBlob->cbSize);

	return dwResult;
}


void SDPUtilSetUUID16(NodeData *pNode, unsigned short uuid16) {
	pNode->type=SDP_TYPE_UUID;
	pNode->specificType = SDP_ST_UUID16;
	pNode->u.uuid16 = uuid16;
}

void SDPUtilSetUINT8(NodeData *pNode, unsigned char uuint8) {
	pNode->type = SDP_TYPE_UINT;
	pNode->specificType = SDP_ST_UINT8;
	pNode->u.uint8 = uuint8;
}

void SDPUtilSetUINT16(NodeData *pNode, unsigned short uuint16) {
	pNode->type= SDP_TYPE_UINT;
	pNode->specificType = SDP_ST_UINT16;
	pNode->u.uint16 = uuint16;
}

void SDPUtilSetUINT32(NodeData *pNode, unsigned int uuint32) {
	pNode->type = SDP_TYPE_UINT;
	pNode->specificType = SDP_ST_UINT32;
	pNode->u.uint32 = uuint32;
}

void SDPUtilSetSTRING(NodeData *pNode, TCHAR * tString) {
	char str[255];
	int len;

	pNode->type = SDP_TYPE_STRING;
	pNode->specificType = SDP_ST_NONE;

#ifdef UNDER_CE
	len = wcstombs( str, tString, 255 );
#else
	len = _tcslen(tString);
	_tcscpy(str, tString);
#endif

	pNode->u.url.length = len;
	//pNode->u.url.val = str;

	pNode->u.url.val = (char *) CoTaskMemAlloc(len + 1);
	strcpy(pNode->u.url.val, str);
}

void SDPUtilSetURL(NodeData *pNode, TCHAR *turl) {
	char url[255];
	int len;

	pNode->type = SDP_TYPE_URL;
	pNode->specificType = SDP_ST_NONE;

#ifdef UNDER_CE
	len = wcstombs( url, turl, 255 );
#else
	len = _tcslen(turl);
	_tcscpy(url, turl);
#endif

	pNode->u.url.length = len;
	pNode->u.url.val = (char *) CoTaskMemAlloc(len + 1);
	strcpy(pNode->u.url.val, url);
}

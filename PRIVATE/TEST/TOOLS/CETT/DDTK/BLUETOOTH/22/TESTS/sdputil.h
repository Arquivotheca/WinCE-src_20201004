//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// SDPUtil.h

#ifndef __SDPUTIL_H__
#define __SDPUTIL_H__

#include "util.h"

#define SOCKTEST_PROTOCOL_UUID16 0x7777
#define SOCKTEST_UNIQUE_UUID16   0xFFFF

#ifndef OBEXObjectPushServiceClassID_UUID16
#define OBEXObjectPushServiceClassID_UUID16 0x1105
#endif

#ifndef PublicBrowseRoot_UUID16
#define PublicBrowseRoot_UUID16 0x1002
#endif

#define MAX_SDPRECSIZE		4096
#define MAX_HANDLES			50
#define MAX_ATTRIBUTES		50
#define MAX_STRING_LENGTH	255

#define S_NO_RECURSE ERROR_NOT_A_REPARSE_POINT 

// ---------------------------------- CUUIDList -----------------------------------------------------
class CUUIDList
{
private:
	USHORT uuids[36];
	DWORD  dwCount;
public:
	CUUIDList() : dwCount (0) {}
	
	DWORD Count()											// how many uuids are there in the list
	{
		return dwCount;
	}

	DWORD Append(USHORT uuid)								// add a uuid at the end of the list
	{
		if (dwCount > 36)
			return 0;

		uuids[dwCount] = uuid;

		return ++dwCount;
	}

	USHORT Get(DWORD dwIndex)								// return the uuid with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return 0;

		return uuids[dwIndex];
	}

	DWORD Clear()											// return 0 if success
	{
		dwCount = 0;

		return dwCount;
	}

	DWORD Print()											// print uuids
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("\tUUID %u : 0x%04X\n"), i, uuids[i]);
		}

		return 0;
	}
};

// ---------------------------------- CProtocolDescriptorList -----------------------------------------

typedef struct _ProtocolDescritptor
{
	USHORT uuid;											// UUID16
	USHORT usVersion;										// protocol version number
	USHORT usPort;											// connection-port number
} ProtocolDescriptor, *PProtocolDescriptor;

class CProtocolDescriptorStack
{
private:
	ProtocolDescriptor protocolDescriptors[36];
	DWORD dwCount;
public:
	CProtocolDescriptorStack() : dwCount (0) {}
	
	DWORD Count()											// how many protocol descriptor are there in the stack
	{
		return dwCount;
	}

	DWORD Append(ProtocolDescriptor *pProtocolDescriptor)		
															// add a protocol descriptor at the end of the stack
	{
		if (dwCount >= 36)
			return 0;

		memcpy(&protocolDescriptors[dwCount], pProtocolDescriptor, sizeof (ProtocolDescriptor));

		return ++dwCount;
	}

	ProtocolDescriptor *Get(DWORD dwIndex)					// return the protocol descriptor with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return NULL;

		return &protocolDescriptors[dwIndex];
	}

	DWORD Clear()											// return 0 if success
	{
		dwCount = 0;

		return dwCount;
	}

	DWORD Print()											// print protocol descriptors
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("\t\tprotocol descriptor %u : uuid = 0x%04X   version = %u   port = %u\n"),
																i,
																protocolDescriptors[i].uuid, 
																protocolDescriptors[i].usVersion,
																protocolDescriptors[i].usPort);
		}

		return 0;
	}

	CProtocolDescriptorStack & operator=(const CProtocolDescriptorStack &rhs)
	{
		if (rhs.dwCount > 36)
			return *this;

		memcpy(&(this->protocolDescriptors[0]), &rhs.protocolDescriptors[0], rhs.dwCount * sizeof (ProtocolDescriptor));
		this->dwCount = rhs.dwCount;

		return *this;
	}

};

class CProtocolDescriptorList
{
private:
	CProtocolDescriptorStack protocolDescriptorStacks[8];
	DWORD dwCount;
public:
	CProtocolDescriptorList() : dwCount (0) {}
	
	DWORD Count()											// how many protocol descriptor stacks are there in the list
	{
		return dwCount;
	}

	DWORD Append(CProtocolDescriptorStack *pProtocolDescriptorStack)		
															// add a protocol descriptor stacks at the end of the list
	{
		if (dwCount > 8)
			return 0;

		protocolDescriptorStacks[dwCount] = *pProtocolDescriptorStack;

		return ++dwCount;
	}

	CProtocolDescriptorStack *Get(DWORD dwIndex)			// return the protocol descriptor stack with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return NULL;

		return &protocolDescriptorStacks[dwIndex];
	}

	DWORD Clear()											// return 0 if success
	{
		dwCount = 0;

		return dwCount;
	}

	DWORD Print()											// print protocol descriptor stacks
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("\tProtocol descritptor stack %u\n"), i);
			protocolDescriptorStacks[i].Print();
		}

		return 0;
	}
};

// ---------------------------------- CLanguageBaseAttributeList -------------------------------------

typedef struct _LanguageBaseAttributeID
{
	USHORT usLanguage;										// UINT16
	USHORT usEncoding;										// UINT16
	USHORT usBaseAttributeID;								// UNIT16
} LanguageBaseAttributeID, *PLanguageBaseAttributeID;

class CLanguageBaseAttributeIDList
{
private:
	LanguageBaseAttributeID languageBaseAttributeIDs[36];
	DWORD dwCount;
public:
	CLanguageBaseAttributeIDList() : dwCount (0) {}
	
	DWORD Count()											// how many protocol descriptor stacks are there in the list
	{
		return dwCount;
	}

	DWORD Append(LanguageBaseAttributeID *pLanguageBaseAttributeID)		
															// add a protocol descriptor stacks at the end of the list
	{
		if (dwCount > 36)
			return 0;

		memcpy(&languageBaseAttributeIDs[dwCount], pLanguageBaseAttributeID, sizeof (LanguageBaseAttributeID));

		return ++dwCount;
	}

	LanguageBaseAttributeID *Get(DWORD dwIndex)				// return the protocol descriptor stack with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return NULL;

		return &languageBaseAttributeIDs[dwIndex];
	}

	DWORD Clear()											// return 0 if success
	{
		dwCount = 0;

		return dwCount;
	}

	DWORD Print()											// print language base attributes
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("\tlanguage base attrubute ID %u : language = %u   encoding = %u   baseAttributeID = %u\n"),
																i,
																languageBaseAttributeIDs[i].usLanguage, 
																languageBaseAttributeIDs[i].usEncoding,
																languageBaseAttributeIDs[i].usBaseAttributeID);
		}

		return 0;
	}
};

// ------------------------------ CBluetoothProfileDescriptorList -------------------------------------

typedef struct _BluetoothProfileDescriptor
{
	USHORT uuid;											// UUID16
	USHORT usVersion;										// profile version number
} BluetoothProfileDescriptor, *PBluetoothProfileDescriptor;

class CBluetoothProfileDescriptorList
{
private:
	BluetoothProfileDescriptor bluetoothProfileDescriptors[36];
	DWORD dwCount;
public:
	CBluetoothProfileDescriptorList() : dwCount (0) {}
	
	DWORD Count()											// how many protocol descriptor stacks are there in the list
	{
		return dwCount;
	}

	DWORD Append(BluetoothProfileDescriptor *pBluetoothProfileDescriptor)		
															// add a protocol descriptor stacks at the end of the list
	{
		if (dwCount > 36)
			return 0;

		memcpy(&bluetoothProfileDescriptors[dwCount], pBluetoothProfileDescriptor, sizeof(BluetoothProfileDescriptor));

		return ++dwCount;
	}

	BluetoothProfileDescriptor *Get(DWORD dwIndex)			// return the protocol descriptor stack with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return NULL;

		return &bluetoothProfileDescriptors[dwIndex];
	}

	DWORD Clear()											// return 0 if success
	{
		dwCount = 0;

		return dwCount;
	}

	DWORD Print()											// print bluetooth profile descriptors
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("\tbluetooth profile descriptor %u : uuid = %u   version = %u\n"),	
																i,
																bluetoothProfileDescriptors[i].uuid, 
																bluetoothProfileDescriptors[i].usVersion);
		}

		return 0;
	}
};

// ----------------------------------------- CAttributeIDRange ------------------------------------------
typedef struct _AttributeIDRange
{
	USHORT minAttribute;											
	USHORT maxAttribute;										
} AttributeIDRange, *PAttributeIDRange;

class CAttributeIDRangeList
{
private:
	AttributeIDRange attributeIDRanges[36];
	DWORD dwCount;
public:
	CAttributeIDRangeList() : dwCount (0) {}
	
	DWORD Count()											// how many attribute ID ranges are there in the list
	{
		return dwCount;
	}

	DWORD Append(AttributeIDRange *pAttributeIDRange)		
															// add an attribute ID range at the end of the list
	{
		if (dwCount > 36)
			return 0;

		memcpy(&attributeIDRanges[dwCount], pAttributeIDRange, sizeof(AttributeIDRange));

		return ++dwCount;
	}

	AttributeIDRange *Get(DWORD dwIndex)					// return the attribute ID range with index value dwIndex
	{
		if (dwIndex >= dwCount)
			return NULL;

		return &attributeIDRanges[dwIndex];
	}

	DWORD Print()											// print attribute ID ranges
	{
		for (DWORD i = 0; i < dwCount; i ++)
		{
			OutStr(TEXT("attribute ID range %u : minAttribute = %u   maxAttribute = %u\n"),	
																i,
																attributeIDRanges[i].minAttribute, 
																attributeIDRanges[i].maxAttribute);
		}

		return 0;
	}
};

// ------------------------------------------- SDPUtilRecord --------------------------------------------

typedef struct _SDPUtilRecord
{
	ULONG							ServiceRecordHandle;						// UINT32
	CUUIDList						SerivceClassIDList;							// List of UUID16
	ULONG							ServiceRecordState;							// UINT32
	USHORT							ServiceID;									// UUID16
	CProtocolDescriptorList			ProtocolDescriptorList; 
	CUUIDList						BrowseGroupList;
	CLanguageBaseAttributeIDList	LanguageBaseAttributeIDList;
	ULONG							ServiceInfoTimeToLive;						// UINT32
	UCHAR							ServiceAvailability;						// UINT8
	CBluetoothProfileDescriptorList BluetoothProfileDescriptorList;
	TCHAR							DocumentationURL[MAX_STRING_LENGTH];		// URL
	TCHAR							ClientExecutableURL[MAX_STRING_LENGTH];		// URL
	TCHAR							IconURL[MAX_STRING_LENGTH];					// URL
	TCHAR							ServiceName[MAX_STRING_LENGTH];				// STRING
	TCHAR							ServiceDescription[MAX_STRING_LENGTH];		// STRING
	TCHAR							ProviderName[MAX_STRING_LENGTH];			// STRING
} SDPUtilRecord, *PSDPUtilRecord;

typedef struct BTH_DEVICE_INFO {
	bt_addr address;
	bt_cod  classOfDevice;
	TCHAR	name[BTH_MAX_NAME_SIZE];
} BTH_DEVICE_INFO, *PBTH_DEVICE_INFO;

// ---------------------------------------- functions ----------------------------------------------

// Intilize a SDP record
DWORD SDPUtilInitRecord(SDPUtilRecord *);

// Build a default SDP record
DWORD SDPUtilBuildRecord(SDPUtilRecord *, USHORT, USHORT, USHORT);

// Print a SDP record
DWORD SDPUtilPrintRecord(SDPUtilRecord *);

// Print a SDP stream
DWORD SDPUtilPrintStream(UCHAR *, DWORD);

// Convert SDPUtilRecord to a byte stream
DWORD SDPUtilRecordToStream(UCHAR *, SDPUtilRecord *, DWORD *);

// Convert a byte stream to SDPUtilRecord
DWORD SDPUtilStreamToRecord(SDPUtilRecord *, UCHAR *, DWORD);

// Convert a byte stream to multiple SDPUtilRecords
DWORD SDPUtilStreamToRecords(SDPUtilRecord *, DWORD *, UCHAR *, DWORD);

// Add or update existing SDP service record
DWORD SDPUtilUpdateRecord(SDPUtilRecord *, DWORD);

// Delete existing SDP service record
DWORD SDPUtilDeleteRecord(ULONG);

// Inquery all bluetooth devices in proximity
DWORD SDPUtilInquery(BTH_DEVICE_INFO *, DWORD *, DWORD);

// Do a SDP service search on a specific device
DWORD SDPUtilServiceSearch(BT_ADDR, CUUIDList *, ULONG *, DWORD *);

// Do a SDP attribute search on a specific device
DWORD SDPUtilAttributeSearch(BT_ADDR, ULONG, CAttributeIDRangeList *, SDPUtilRecord *);

//Do a SDP service attribute search on a specific device
DWORD SDPUtilServiceAttributeSearch(BT_ADDR, CUUIDList *, CAttributeIDRangeList *, SDPUtilRecord *, DWORD *);

#endif //__SDPUTIL_H__
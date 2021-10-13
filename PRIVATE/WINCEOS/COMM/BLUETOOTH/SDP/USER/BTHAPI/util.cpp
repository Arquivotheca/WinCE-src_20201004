//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "stdafx.h"
#include "BthAPI.h"
#include "sdplib.h"
#include "util.h"

ListLock::ListLock(CRITICAL_SECTION *CS) : cs(CS)
{
    EnterCriticalSection(cs); 
}

ListLock::~ListLock()
{ 
    LeaveCriticalSection(cs); 
}

HRESULT CreateSdpNodeFromNodeData(NodeData *pData, PSDP_NODE *pNode)
{
//  BSTR string;
//  ULONG length;

    *pNode = NULL;

    switch (pData->type) {
    case SDP_TYPE_NIL:
        *pNode = SdpCreateNodeNil();
         break;

    case SDP_TYPE_BOOLEAN:
        *pNode = SdpCreateNodeBoolean(pData->u.booleanVal);
        break;

    case SDP_TYPE_UINT:
        switch (pData->specificType) {
        case SDP_ST_UINT8:
            *pNode = SdpCreateNodeUInt8(pData->u.uint8);
            break;

        case SDP_ST_UINT16:
            *pNode = SdpCreateNodeUInt16(pData->u.uint16);
            break;

        case SDP_ST_UINT32:
            *pNode = SdpCreateNodeUInt32(pData->u.uint32);
            break;

        case SDP_ST_UINT64:
            *pNode = SdpCreateNodeUInt64(pData->u.uint64);
            break;

        case SDP_ST_UINT128:
            *pNode = SdpCreateNodeUInt128(&pData->u.uint128);
            break;

        default:
            return E_INVALIDARG;
        }
		break;

    case SDP_TYPE_INT:
        switch (pData->specificType) {
        case SDP_ST_INT8:
            *pNode = SdpCreateNodeInt8(pData->u.int8);
            break;

        case SDP_ST_INT16:
            *pNode = SdpCreateNodeInt16(pData->u.int16);
            break;

        case SDP_ST_INT32:
            *pNode = SdpCreateNodeInt32(pData->u.int32);
            break;

        case SDP_ST_INT64:
            *pNode = SdpCreateNodeInt64(pData->u.int64);
            break;

        case SDP_ST_INT128:
            *pNode = SdpCreateNodeInt128(&pData->u.int128);
            break;

        default:
            return E_INVALIDARG;
        }
        break;

    case SDP_TYPE_UUID:
        switch (pData->specificType) {
        case SDP_ST_UUID128:
            *pNode = SdpCreateNodeUUID128(&pData->u.uuid128);
            break;

        case SDP_ST_UUID32:
            *pNode = SdpCreateNodeUUID32(pData->u.uuid32);
            break;

        case SDP_ST_UUID16:
            *pNode = SdpCreateNodeUUID16(pData->u.uuid16);
            break;

        default:
            return E_INVALIDARG;
        }
        break;

    case SDP_TYPE_STRING:
        *pNode = SdpCreateNodeString(pData->u.str.val, pData->u.str.length);
        break;

    case SDP_TYPE_URL:
        *pNode = SdpCreateNodeUrl(pData->u.url.val, pData->u.url.length);
        break;

    case SDP_TYPE_CONTAINER:
        if (pData->u.container == NULL) {
            return E_INVALIDARG;
        }

        *pNode = SdpCreateNode();
        if (*pNode != NULL) {
            (*pNode)->hdr.Type = SDP_TYPE_CONTAINER;
            (*pNode)->hdr.SpecificType = SDP_ST_NONE;
            (*pNode)->u.container = pData->u.container;
            pData->u.container->AddRef();
        }
        break;

    default:
        return E_INVALIDARG;
    }

    if (*pNode == NULL) {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

void CreateNodeDataFromSdpNode(PSDP_NODE pNode, NodeData *pData)
{
    RtlZeroMemory(pData, sizeof(NodeData));

    pData->type = pNode->hdr.Type;
    pData->specificType = pNode->hdr.SpecificType;

    switch (pData->type) {
    case SDP_TYPE_NIL:
        break;

    case SDP_TYPE_UINT:
        switch (pData->specificType) {
        case SDP_ST_UINT8:
            pData->u.uint8 = pNode->u.uint8;    
            break;

        case SDP_ST_UINT16:
            pData->u.uint16 = pNode->u.uint16; 
            break;

        case SDP_ST_UINT32:
            pData->u.uint32 = pNode->u.uint32; 
            break;

        case SDP_ST_UINT64:
            pData->u.uint64 = pNode->u.uint64; 
            break;

        case SDP_ST_UINT128:
            memcpy(&pData->u.uint128, &pNode->u.uint128, sizeof(pData->u.uint128));
            break;
        }
        break;

    case SDP_TYPE_INT:
        switch (pData->specificType) {
        case SDP_ST_INT8:
            pData->u.int8 = pNode->u.int8;    
            break;

        case SDP_ST_INT16:
            pData->u.int16 = pNode->u.int16; 
            break;

        case SDP_ST_INT32:
            pData->u.int32 = pNode->u.int32; 
            break;

        case SDP_ST_INT64:
            pData->u.int64 = pNode->u.int64; 
            break;

        case SDP_ST_INT128:
            memcpy(&pData->u.int128, &pNode->u.int128, sizeof(pData->u.int128));
            break;
        }
        break;

    case SDP_TYPE_UUID:
        switch (pData->specificType) {
        case SDP_ST_UUID16:
            pData->u.uuid16 = pNode->u.uuid16;
            break;

        case SDP_ST_UUID32:
            pData->u.uuid32 = pNode->u.uuid32;
            break;

        case SDP_ST_UUID128:
            memcpy(&pData->u.uuid128, &pNode->u.uuid128, sizeof(pData->u.uuid128));
            break;
        }
        break;

    case SDP_TYPE_STRING:
        pData->u.str.length = pNode->DataSize;
        pData->u.str.val = NULL;
        break;

    case SDP_TYPE_URL:
        pData->u.url.length = pNode->DataSize;
        pData->u.url.val = NULL;
        break;

    case SDP_TYPE_BOOLEAN:
        pData->u.booleanVal = pNode->u.boolean;
        break;

    case SDP_TYPE_CONTAINER:
        pData->specificType = SDP_ST_NONE;
        pData->u.container = pNode->u.container;
        break;
    }
}

HRESULT CopyStringDataToNodeData(CHAR *str, ULONG strLength, SdpString *pString)
{
    ULONG length;
    HRESULT err = S_OK;

    if (pString->val) {
        if (pString->length >= strLength) {
            length = strLength;
        }
        else {
            length = pString->length;
            err = S_FALSE;
        }

    }
    else {
        pString->val = (CHAR*) CoTaskMemAlloc(strLength);
        if (pString->val == NULL) {
            return E_OUTOFMEMORY;
        }
        else {
            ZeroMemory(pString->val, strLength);
            length = pString->length = strLength;
        }
    }

    memcpy(pString->val, str, length);
    return err;
}

void SdpReleaseContainer(ISdpNodeContainer *container)
{
    container->Release();
}

HRESULT MapNtStatusToHresult(NTSTATUS Status)
{
    if (NT_SUCCESS(Status)) {
        return S_OK;
    }

    switch (Status) {
    case STATUS_INVALID_PARAMETER:  return E_INVALIDARG;
    case STATUS_INSUFFICIENT_RESOURCES:  return E_OUTOFMEMORY;
    case STATUS_NOT_FOUND: return E_FAIL; 
    default: return E_FAIL;
    }
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#define INDEX_DEFAULT IDI_DEFAULT_MAJOR_CLASS 

MinorDeviceInfo ComputerMinorInfo[] =
{
    { COD_COMPUTER_MINOR_UNCLASSIFIED, IDI_PC, IDS_COD_COMPUTER /* IDS_COD_UNCLASSIFIED */ },
    { COD_COMPUTER_MINOR_DESKTOP,      IDI_PC, IDS_COD_COMPUTER_DESKTOP},
    { COD_COMPUTER_MINOR_SERVER,       IDI_PC, IDS_COD_COMPUTER_SERVER},
    { COD_COMPUTER_MINOR_LAPTOP,       INDEX_DEFAULT, IDS_COD_COMPUTER_LAPTOP},
    { COD_COMPUTER_MINOR_HANDHELD,     INDEX_DEFAULT, IDS_COD_COMPUTER_HANDHELD},
    { COD_COMPUTER_MINOR_PALM,         INDEX_DEFAULT, IDS_COD_COMPUTER_PALM},
};

MinorDeviceInfo PhoneMinorInfo[] =
{
    { COD_PHONE_MINOR_UNCLASSIFIED, INDEX_DEFAULT, IDS_COD_PHONE /* IDS_COD_UNCLASSIFIED */ },
    { COD_PHONE_MINOR_CELLULAR,     INDEX_DEFAULT, IDS_COD_PHONE_CELLULAR }, 
    { COD_PHONE_MINOR_CORDLESS,     INDEX_DEFAULT, IDS_COD_PHONE_CORDLESS }, 
    { COD_PHONE_MINOR_SMART,        INDEX_DEFAULT, IDS_COD_PHONE_SMART }, 
    { COD_PHONE_MINOR_WIRED_MODEM,  INDEX_DEFAULT, IDS_COD_PHONE_WIRED_MODEM }, 
};

MinorDeviceInfo LanMinorInfo[] =
{
    { COD_LAN_ACCESS_0_USED,  INDEX_DEFAULT, IDS_COD_LAN_ACCESS_0_USED },
    { COD_LAN_ACCESS_17_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_17_USED },
    { COD_LAN_ACCESS_33_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_33_USED },
    { COD_LAN_ACCESS_50_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_50_USED },
    { COD_LAN_ACCESS_67_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_67_USED },
    { COD_LAN_ACCESS_83_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_83_USED },
    { COD_LAN_ACCESS_99_USED, INDEX_DEFAULT, IDS_COD_LAN_ACCESS_99_USED },
    { COD_LAN_ACCESS_FULL,    INDEX_DEFAULT, IDS_COD_LAN_ACCESS_FULL },
};

MinorDeviceInfo AudioMinorInfo[] =
{
    { COD_AUDIO_MINOR_UNCLASSIFIED, IDI_SPEAKER, IDS_COD_AUDIO /* IDS_COD_UNCLASSIFIED */ },
    { COD_AUDIO_MINOR_HEADSET,      INDEX_DEFAULT, IDS_COD_AUDIO_HEADSET },
};

//
// defaults for minor mask (MM) and minor shift (MS)
//
#define MM_DEFAULT      (COD_MINOR_MASK) 
#define MS_DEFAULT      (COD_MINOR_BIT_OFFSET)

MajorDeviceInfo ClassOfDeviceInfo[] =
{
    { COD_MAJOR_MISCELLANEOUS, MM_DEFAULT,         MS_DEFAULT,                INDEX_DEFAULT, IDS_COD_MISCELLANEOUS, NULL, 0 },
    { COD_MAJOR_COMPUTER,      MM_DEFAULT,         MS_DEFAULT,                IDI_PC,        IDS_COD_COMPUTER,      ComputerMinorInfo, ARRAY_SIZE(ComputerMinorInfo) }, 
    { COD_MAJOR_PHONE,         MM_DEFAULT,         MS_DEFAULT,                INDEX_DEFAULT, IDS_COD_PHONE,         PhoneMinorInfo,    ARRAY_SIZE(PhoneMinorInfo) }, 
    { COD_MAJOR_LAN_ACCESS,    COD_LAN_ACCESS_MASK, COD_LAN_ACCESS_BIT_OFFSET, IDI_NET,      IDS_COD_LAN_ACCESS,    LanMinorInfo,      ARRAY_SIZE(LanMinorInfo) }, 
    { COD_MAJOR_AUDIO,         MM_DEFAULT,         MS_DEFAULT,                IDI_SPEAKER,   IDS_COD_AUDIO,         AudioMinorInfo,    ARRAY_SIZE(AudioMinorInfo) }, 
    { COD_MAJOR_PERIPHERAL,    MM_DEFAULT,         MS_DEFAULT,                INDEX_DEFAULT, IDS_COD_PERIPHERAL,    NULL, 0 }, 
    { COD_MAJOR_UNCLASSIFIED,  MM_DEFAULT,         MS_DEFAULT,                INDEX_DEFAULT, IDS_COD_UNCLASSIFIED,  NULL, 0 },
};

#define NUM_CLASS_OF_DEVICE_INFO (ARRAY_SIZE(ClassOfDeviceInfo))

struct ServiceDeviceInfo {
    UINT stringID;
    USHORT serviceClass;
};

ServiceDeviceInfo ServiceInfo[] =
{
    { IDS_COD_SERVICE_LIMITED,      COD_SERVICE_LIMITED     },
    { IDS_COD_SERVICE_NETWORKING,   COD_SERVICE_NETWORKING  },
    { IDS_COD_SERVICE_RENDERING,    COD_SERVICE_RENDERING   },
    { IDS_COD_SERVICE_CAPTURING,    COD_SERVICE_CAPTURING   },
    { IDS_COD_SERVICE_OBJECT_XFER,  COD_SERVICE_OBJECT_XFER },
    { IDS_COD_SERVICE_AUDIO,        COD_SERVICE_AUDIO       },
    { IDS_COD_SERVICE_TELEPHONY,    COD_SERVICE_TELEPHONY   },
    { IDS_COD_SERVICE_INFORMATION,  COD_SERVICE_INFORMATION },
};

#define NUM_SERVICE_INFO (ARRAY_SIZE(ServiceInfo))

MajorDeviceInfo* GetMajorClassInfo(bt_cod cod)
{
    //
    // Format is encoded in the bottom 2 bits of the first octet and we only 
    // understand format 00
    //
    if (GET_COD_FORMAT(cod) != COD_VERSION) {
        return NULL;
    }

    DWORD majorClass = GET_COD_MAJOR(cod); 

    for (int i = 0; i < NUM_CLASS_OF_DEVICE_INFO; i++) {
        if (ClassOfDeviceInfo[i].majorClass == majorClass) {
            return ClassOfDeviceInfo +  i;
        }
    }

    return NULL;
}

MinorDeviceInfo* GetMinorClassInfo(bt_cod cod, MajorDeviceInfo* MajorInfo)
{
    UINT i;
    ULONG minorClass;

    minorClass = (cod & MajorInfo->minorMask) >> MajorInfo->minorShift;

    for (i = 0; i < MajorInfo->numMinorDeviceInfo; i++) {
        if (minorClass == MajorInfo->minorDeviceInfo[i].minorClass) {
            return MajorInfo->minorDeviceInfo + i;
        }
    }

    return NULL;
}
#endif // UNDER_CE

HRESULT pNormalizeUuid(NodeData *pDataUuid, GUID* pNormalizedUuid)
{
    if (pDataUuid == NULL || pDataUuid->type != SDP_TYPE_UUID) {
        return E_INVALIDARG;
    }

//  GUID guid;

    switch (pDataUuid->specificType) {
    case SDP_ST_UUID16:
        *pNormalizedUuid = Bluetooth_Base_UUID;
        pNormalizedUuid->Data1 += pDataUuid->u.uuid16;
        return S_OK;

    case SDP_ST_UUID32:
        *pNormalizedUuid = Bluetooth_Base_UUID;
        pNormalizedUuid->Data1 += pDataUuid->u.uuid32;
        return S_OK;

    case SDP_ST_UUID128:
        *pNormalizedUuid =  pDataUuid->u.uuid128;
        return S_OK;

    default:
        return E_INVALIDARG;
    }
}

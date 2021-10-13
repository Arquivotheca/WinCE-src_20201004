//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __BTHSDPDEF_H__
#define __BTHSDPDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

struct SDP_LARGE_INTEGER_16 {
    ULONGLONG LowPart;
    LONGLONG HighPart;
}; 
 

struct SDP_ULARGE_INTEGER_16 {
    ULONGLONG LowPart;
    ULONGLONG HighPart;
};

typedef struct SDP_ULARGE_INTEGER_16 SDP_ULARGE_INTEGER_16, *PSDP_ULARGE_INTEGER_16, *LPSDP_ULARGE_INTEGER_16;
typedef struct SDP_LARGE_INTEGER_16  SDP_LARGE_INTEGER_16,  *PSDP_LARGE_INTEGER_16,  *LPSDP_LARGE_INTEGER_16;

enum NodeContainerType {
	NodeContainerTypeSequence,
    NodeContainerTypeAlternative
};

typedef enum NodeContainerType NodeContainerType;

typedef USHORT SDP_ERROR, *PSDP_ERROR;

enum SDP_TYPE {
    SDP_TYPE_NIL =  0x00,
    SDP_TYPE_UINT = 0x01,
    SDP_TYPE_INT = 0x02,
    SDP_TYPE_UUID = 0x03,
    SDP_TYPE_STRING = 0x04,
    SDP_TYPE_BOOLEAN = 0x05,
    SDP_TYPE_SEQUENCE = 0x06,
    SDP_TYPE_ALTERNATIVE = 0x07,
    SDP_TYPE_URL = 0x08,
    SDP_TYPE_CONTAINER = 0x20
};
//  9 - 31 are reserved
typedef enum SDP_TYPE SDP_TYPE;

// allow for a little easier type checking / sizing for integers and UUIDs
// ((SDP_ST_XXX & 0xF0) >> 4) == SDP_TYPE_XXX
// size of the data (in bytes) is encoded as ((SDP_ST_XXX & 0xF0) >> 8)
enum SDP_SPECIFICTYPE {
    SDP_ST_NONE = 0x0000,

    SDP_ST_UINT8 = 0x0010,
    SDP_ST_UINT16 = 0x0110,
    SDP_ST_UINT32 = 0x0210,
    SDP_ST_UINT64 = 0x0310,
    SDP_ST_UINT128 = 0x0410,
    
    SDP_ST_INT8 = 0x0020,
    SDP_ST_INT16 = 0x0120,
    SDP_ST_INT32 = 0x0220,
    SDP_ST_INT64 = 0x0320,
    SDP_ST_INT128 = 0x0420,
    
    SDP_ST_UUID16 = 0x0130,
    SDP_ST_UUID32 = 0x0230,
    SDP_ST_UUID128 = 0x0430
};
typedef enum SDP_SPECIFICTYPE SDP_SPECIFICTYPE;

typedef struct _SdpAttributeRange {
    USHORT minAttribute;
    USHORT maxAttribute;
} SdpAttributeRange;


typedef
#ifdef USE_MIDL_SYNTAX 
      [switch_type(unsigned short)]
#endif
                                    union SdpQueryUuidUnion {
#ifdef USE_MIDL_SYNTAX 
    [case(SDP_ST_UUID128)]
#endif
       GUID uuid128;

#ifdef USE_MIDL_SYNTAX 
    [case(SDP_ST_UUID32)] 
#endif _NTDDK_
       ULONG uuid32;

#ifdef USE_MIDL_SYNTAX 
    [case(SDP_ST_UUID16)]
#endif _NTDDK_
        USHORT uuid16;
} SdpQueryUuidUnion;

typedef struct _SdpQueryUuid {
#ifdef USE_MIDL_SYNTAX 
    [switch_is(uuidType)]
#endif
       SdpQueryUuidUnion u;

    USHORT uuidType;
} SdpQueryUuid;

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))

#ifdef USE_MIDL_SYNTAX 
cpp_quote("#define BTH_SDP_VERSION 1")
#endif
#define BTH_SDP_VERSION         1

typedef struct _BTHNS_SETBLOB {
    ULONG *pSdpVersion;   // version number; set to BTH_SDP_VERSION by client
	ULONG *pRecordHandle; // out for AddRecord, in for update record
    ULONG Reserved[4];
	ULONG fSecurity;	  // security requirements
	ULONG fOptions;	      // various other attributes
	ULONG ulRecordLength; // in
	UCHAR pRecord[1];     // in
} BTHNS_SETBLOB, *PBTHNS_SETBLOB;

#ifdef USE_MIDL_SYNTAX 
cpp_quote("#define MAX_UUIDS_IN_QUERY 12")

cpp_quote("#define SDP_SERVICE_SEARCH_REQUEST           1")
cpp_quote("#define SDP_SERVICE_ATTRIBUTE_REQUEST        2")
cpp_quote("#define SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST 3")
cpp_quote(" ")
cpp_quote("//")
cpp_quote("// The following may be passed as parameters to BthNsLookupServiceNext as extended ")
cpp_quote("// dwFlags options for device inquiry.")
cpp_quote("//")
cpp_quote(" ")
cpp_quote("// Causes traversal through list to be reset to first element.")
cpp_quote("#define BTHNS_LUP_RESET_ITERATOR 0x00010000")
cpp_quote("// Does not increment list, causes next query to be performed on current item as well.")
cpp_quote("#define BTHNS_LUP_NO_ADVANCE     0x00020000")
cpp_quote("// Causes LookupServiceEnd to abort current inquiry.")
cpp_quote("#define BTHNS_ABORT_CURRENT_INQUIRY 0xfffffffd")
cpp_quote(" ")
#endif // USE_MIDL_SYNTAX

#define MAX_UUIDS_IN_QUERY                   12

#define SDP_SERVICE_SEARCH_REQUEST           1
#define SDP_SERVICE_ATTRIBUTE_REQUEST        2
#define SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST 3

//
// The following may be passed as parameters to BthNsLookupServiceNext as extended 
// dwFlags options for device inquiry.
//

// Causes traversal through list to be reset to first element.
#define BTHNS_LUP_RESET_ITERATOR 0x00010000
// Does not increment list, causes next query to be performed on current item as well.
#define BTHNS_LUP_NO_ADVANCE     0x00020000
// Causes LookupServiceEnd to abort current inquiry.
#define BTHNS_ABORT_CURRENT_INQUIRY 0xfffffffd

// Paramaters for performing device inquiry.  Maps to parameters passed to BthPerformInquiry().
typedef struct _BTHNS_INQUIRYBLOB {
	ULONG         LAP;
	unsigned char length;
	unsigned char num_responses;
} BTHNS_INQUIRYBLOB, *PBTHNS_INQUIRYBLOB;

// Restrictions on searching for a particular service
typedef struct _BTHNS_RESTRICTIONBLOB {
	ULONG type;
	ULONG serviceHandle;
	SdpQueryUuid uuids[MAX_UUIDS_IN_QUERY];
	ULONG numRange;
	SdpAttributeRange pRange[1];
} BTHNS_RESTRICTIONBLOB, *PBTHNS_RESTRICTIONBLOB;



#endif // UNDER_CE

#ifdef __cplusplus
};
#endif

#endif // __BTHSDPDEF_H__

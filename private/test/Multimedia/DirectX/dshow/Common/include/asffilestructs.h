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
typedef unsigned __int64 QWORD;

#pragma pack( push )
#pragma pack( 1 )


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
} BasicObject_t;


typedef enum
{
    HEADER_OBJECT,
    ASF_DATA_OBJECT,
    INDEX_OBJECT,

    // Header objects
    FILE_PROPERTIES_OBJECT,
    STREAM_PROPERTIES_OBJECT,
    HEADER_EXTENSION_OBJECT,
    CODEC_LIST_OBJECT,
    SCRIPT_COMMAND_OBJECT,
    MARKER_OBJECT,
    BITRATE_MUTUAL_EXCLUSION_OBJECT,
    ERROR_CORRECTION_OBJECT,
    CONTENT_DESCRIPTION_OBJECT,
    EXTENDED_CONTENT_DESCRIPTION_OBJECT,
    STREAM_BITRATE_PROPERTIES_OBJECT,
    CONTENT_BRANDING_OBJECT,
    CONTENT_ENCRYPTION_OBJECT,
    EXTENDED_CONTENT_ENCRYPTION_OBJECT,
    DIGITAL_SIGNATURE_OBJECT,
    PADDING_OBJECT,

    // HeaderExtension objects
    EXTENDED_STREAM_PROPERTIES_OBJECT,
    ADVANCED_MUTUAL_EXCLUSION_OBJECT,
    GROUP_MUTUAL_EXCLUSION_OBJECT,
    STREAM_PRIORITIZATION_OBJECT,
    BANDWIDTH_SHARING_OBJECT,
    LANGUAGE_LIST_OBJECT,
    METADATA_OBJECT,
    METADATA_LIBRARY_OBJECT,
    INDEX_PARAMETERS_OBJECT,
    MEDIA_OBJECT_INDEX_PARAMETERS_OBJECT,
    TIMECODE_INDEX_PARAMETERS_OBJECT,
    COMPATIBILITY_OBJECT,
    ADVANCED_CONTENT_ENCRYPTION_OBJECT,

    OBJECT_TYPES_SIZEOF
} ObjectTypes;


/**** Top-level objects ****/

typedef struct      // Everything is DWORD aligned in this structure
{
    GUID    objectID;
    QWORD   qwObjectSize;
    DWORD   dwNumberOfObjects;
    BYTE    bReserved1;
    BYTE    bReserved2;
} HeaderObject_t;

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    fileID;
    QWORD   qwTotalDataPackets;
    WORD    wReserved;
} ASFDataObject_t;




/**** HeaderObject objects ****/

#define FLAG_BROADCAST  0x1
#define FLAG_SEEKABLE   0x2

typedef struct 		// Everything is DWORD aligned in this structure
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    fileID;
    QWORD   qwFileSize;
    QWORD   qwCreationDate;
    QWORD   qwDataPacketCount;
    QWORD   qwPlayDuration;
    QWORD   qwSendDuration;
    QWORD   qwPreroll;
    DWORD   dwFlags;
    DWORD   dwMinPacketSize;
    DWORD   dwMaxPacketSize;
    DWORD   dwMaxBitrate;
} FilePropertiesObject_t;

typedef struct 
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    streamType;
    GUID    errorCorrectionType;
    QWORD   qwTimeOffset;
    DWORD   dwTypeSpecificDataLength;
    DWORD   dwErrorCorrectionDataLength;
    WORD    wFlags;
    DWORD   dwReserved;
} StreamPropertiesObject_t;

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    ReservedField1;
    WORD    wReservedField2;
    UNALIGNED DWORD   dwHeaderExtensionDataSize;
} HeaderExtensionObject_t;


typedef struct
{
    WORD  wFlags;
    DWORD dwAverageBitrate;
} BitrateRecord_t;

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    WORD    wBitrateRecordsCount;
} StreamBitratePropertiesObject_t;

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    WORD    wTitleLength;
    WORD    wAuthorLength;
    WORD    wCopyrightLength;
    WORD    wDescriptionLength;
    WORD    wRatingLength;
} ContentDescriptionObject_t;


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    Reserved;
    DWORD   dwCodecEntriesCount;
} CodecListObject_t;


typedef struct
{
    DWORD dwPresentationTime;
    WORD  wTypeIndex;
    WORD  wCommandNameLength;
} Command_t;


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    Reserved;
    WORD    wCommandsCount;
    WORD    wCommandTypesCount;
} ScriptCommandObject_t;


typedef struct
{
    QWORD  qwOffset;
    QWORD  qwPresentationTime;
    WORD   wEntryLength;
    DWORD  dwSendTime;
    DWORD  dwFlags;
    DWORD  dwMarkerDescriptionLength;
} Marker_t;


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    Reserved;
    DWORD   dwMarkersCount;
    WORD    wReserved;
    WORD    wNameLength;
} MarkerObject_t;


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    exclusionType;
    WORD    wStreamNumbersCount;
} BitrateMutualExclusionObject_t;


typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    errorCorrectionType;
    DWORD   dwErrorCorrectionDataLength;
} ErrorCorrectionObject_t;


//
// HeaderExtensionObject objects
//
typedef struct
{
    WORD    wLanguageID;
    WORD    wStreamNameLength;
} StreamName_t;


typedef struct
{
    GUID    extensionSystemID;
    WORD    wExtensionDataSize;
    DWORD   dwExtensionSystemInfoLength;
} PayloadExtensionSystem_t;

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    QWORD   qwStartTime;
    QWORD   qwEndTime;
    DWORD   dwDataBitrate;
    DWORD   dwBufferSize;
    DWORD   dwInitialBufferFullness;
    DWORD   dwAlternateDataBitrate;
    DWORD   dwAlternateBufferSize;
    DWORD   dwAlternateInitialBuferFullness;
    DWORD   dwMaximumObjectSize;
    DWORD   dwFlags;
    WORD    wStreamNumber;
    WORD    wStreamLanguageIDIndex;
    QWORD   qwAvgTimePerFrame;
    WORD    wStreamNameCount;
    WORD    wPayloadExtensionSystemCount;
} ExtendedStreamPropertiesObject_t;



/**** ASFDataObject objects ****/

#define FLAGS_LENGTH_TYPE_MULTIPLE_PAYLOADS(dwFlags) ((dwFlags) & 0x1)
#define FLAGS_LENGTH_TYPE_SEQUENCE(dwFlags)          (((dwFlags) >> 1) & 0x3)
#define FLAGS_LENGTH_TYPE_PADDING(dwFlags)           (((dwFlags) >> 3) & 0x3)
#define FLAGS_LENGTH_TYPE_PACKET(dwFlags)            (((dwFlags) >> 5) & 0x3)
#define FLAGS_LENGTH_TYPE_ERROR_CORRECTION(dwFlags)  (((dwFlags) >> 7) & 0x1)

#define FLAGS_PROPERTY_REPLICATED_DATA_LENGTH(dwFlags) ((dwFlags) & 0x3)
#define FLAGS_PROPERTY_OFFSET_INTO_MEDIA_TYPE(dwFlags) (((dwFlags) >> 2) & 0x3)
#define FLAGS_PROPERTY_MEDIA_OBJECT_NUMBER(dwFlags)    (((dwFlags) >> 4) & 0x3)
#define FLAGS_PROPERTY_STREAM_NUMBER(dwFlags)          (((dwFlags) >> 6) & 0x3)

#define FLAGS_MULTIPLE_NUMBER_OF_PAYLOADS(dwFlags)((dwFlags) & 0x3F)
#define FLAGS_MULTIPLE_PAYLOAD_LENGTH_TYPE(dwFlags) (((dwFlags) >> 6) & 0x3)


typedef struct
{
    BYTE     bLengthTypeFlags;
    BYTE     bPropertyFlags;
    WORD     dwPaddingLength;
    DWORD    dwSendTime;
    WORD     dwDuration;
} PayloadParsingInfo_t;

typedef struct
{
    BYTE            bStreamNumber;
    DWORD           dwMediaObjectNumber;
    DWORD           dwPresentationTime;
    DWORD           dwReplicatedDataLength;
    BYTE            bPresentationTimeDelta;     // only for subpayloads
    DWORD           dwPayloadDataLength;        // only for multiple payloads
	DWORD			dwSubPayloadDataLength;		// only for subpayloads
	BYTE			bReplicatedData;			// only for subpayloads
	DWORD           dwOverhead;                 // only for subpayloads
} Payload_t;

/**** Simple Index Objects ****/

typedef struct
{
    GUID    objectID;
    QWORD   qwObjectSize;
    GUID    fileID;
    QWORD   qwIndexEntryTimeInterval;
    DWORD   dwMaximumPacketCount;
    DWORD   dwIndexEntriesCount;
} SimpleIndex_t;

typedef struct
{
    DWORD   dwPacketNumber;
    WORD    wPacketCount;
} SimpleIndexEntry_t;

#pragma pack( pop )


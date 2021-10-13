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
#ifndef _ASFFILE_H_
#define _ASFFILE_H_

#include "ASFFileStructs.h"

#define MAX_DWORD  4294967295; // 2^32 -1

/**** Forward Declarations ****/
class CASFFile;
class CASFPayload;
class CASFMediaObject;
class CASFStreamProperties;

// Encapsulates a simple index object for a particular video stream
class CASFSimpleIndex
{
public:
    CASFSimpleIndex();
    CASFSimpleIndex(CASFSimpleIndex*);
    virtual ~CASFSimpleIndex();

    // Take a buffer supposedly containing a Simple Index Object and parse it
    virtual HRESULT Load(BYTE* pRawData);

    // Return the object to its unloaded state
    virtual void Unload();

    // Retrieve the index entry corresponding to the specified presentation time
    virtual HRESULT Seek(QWORD presentationTime, SimpleIndexEntry_t** ppEntry);

    // Fetch the file ID of the ASF file
    virtual GUID GetFileID();

    // Fetch the time interval between each index entry
    virtual QWORD GetIndexEntryTimeInterval();

    // Fetch the maximum packet count value of all the index entries
    virtual DWORD GetMaximumPacketCount();

    // Fetch the total number of entries in the index
    virtual DWORD GetIndexEntriesCount();

    // Fetch the raw data from the ASF file that comprises this index
    virtual BYTE* GetRawData();

private:
    BOOL m_bLoaded;                 // state variable - signifies index data has been successfully parsed
    BYTE* m_RawData;                // raw packet data

    // Simple Index Object members 
    GUID m_FileID;                  // Unique ASF File ID
    QWORD m_IndexEntryTimeInterval;      // interval in 100ns increments for entries in the index
    DWORD m_MaximumPacketCount;     // maximum packet count value of all of the entries
    DWORD m_IndexEntriesCount;      // number of entries in the index
    SimpleIndexEntry_t* m_Entries;  // index entries; treated as an array (points into m_RawData)


};


// Encapsulates a packet within an ASF stream
class CASFPacket
{
    friend class CASFFile;
public:
    CASFPacket(DWORD dwLength);
    virtual ~CASFPacket();

    // Take a buffer supposedly containing an ASF packet and parse it
    virtual HRESULT Load(BYTE* pRawData);

    // Return the packet object to the unloaded state
    virtual void Unload();

    // Return the packet to its initially loaded state
    virtual HRESULT Reset();

    // Fetch the next payload within an ASF Packet
    virtual CASFPayload* GetNextPayload(BOOL bExtractSubPayloads=false);

    // Checks to see whether the packet contains payload data or opaque data
    virtual BOOL IsPayloadData();

private:
    // Fetch the header
    virtual HRESULT ParsePayload(BYTE** pCurrentPosition, Payload_t* pHeader, BYTE**pReplicatedData);

    BYTE* m_RawData;            // raw data of the entire packet
    BYTE* m_FirstPayload;       // points to the first byte of the first payload 
    BYTE* m_NextPayload;        // points to the first byte of the next payload/subpayload to be read.
    BOOL m_bLoaded;             // state variable - signifies raw packet data has been parsed successfully
    BOOL m_bHasPayloads;        // state variable - signifies the packet contains payloads instead of opaque data
    
    // Error Correction Data members
    BYTE* m_ErrorCorrectionData; // Error correction Data
    BYTE m_ErrorCorrectionFlags; // Describe the error correction data

    // Payload Parsing Data members
    BYTE m_LengthTypeFlags;     // defines the size of fields in the parsing data
    BYTE m_PropertyFlags;       // defines the size of fields in the payload data
    DWORD m_PacketLength;       // Length of the data packet in bytes
    DWORD m_Sequence;           // Reserved
    DWORD m_PaddingLength;      // Lenght of padding at the end of the packet
    DWORD m_SendTime;           // Send time of the packet in milliseconds
    WORD   m_Duration;          // Duration of the packet in milliseconds

    // In the case of multiple payloads
    BYTE m_PayloadFlags;        // defines the number of and size of length field in payloads
    DWORD m_PayloadNumber;      // state variable - number of the current payload

    // In the case of compressed payloads
    DWORD m_SubPayloadNumber; // state variable - number of the current sub payload 
    BYTE* m_SubPayloadStart;     // state variable - points to first subpayload of the subpayload array   
    Payload_t* m_SubPayloadHeader; // describes every compressed subpayload within this payload
       
};


// Encapsulates a payload within an ASF packet
class CASFPayload
{
public:
    CASFPayload();
    virtual ~CASFPayload();

    // Take a buffer that is payload data and parse it
    virtual HRESULT Load(BYTE* pPayloadData, BYTE* pReplicatedData, Payload_t header, DWORD SubPayloadNumber);

    // Return the payload object to an unloaded state.
    virtual void Unload();

    // Fetches the number of the stream the payload belongs to
    virtual BYTE GetStreamNumber();

    // Checks to see whether the payload belongs to a media object that is a key frame
    virtual BOOL IsKeyFrame();

    // Checks to see whether this payload was a compressed payload
    virtual BOOL IsCompressed();

    // Get the number of the media object that the payload belongs to
    virtual DWORD GetMediaObjectNumber();

    // Get the offset into the media object that the payload belongs to
    virtual DWORD GetOffsetIntoMediaObject();

    // Get the length in bytes of the replicated data in the payload
    virtual DWORD GetReplicatedDataLength();

    // Get the length in bytes of the payload data
    virtual DWORD GetPayloadDataLength();

    // Fetch the replicated data into the buffer specified
    virtual BYTE* GetReplicatedData();

    // Fetch the payload data into the buffer specified
    virtual BYTE* GetPayloadData();

    // Fetch the presentation time of the payload
    virtual DWORD GetPresentationTime();

    // Fetch the number of bytes in the payload header
    virtual DWORD GetOverhead();

private:
    BYTE    m_StreamNumber;             // ID of the stream
    DWORD   m_Overhead;                 // Number of bytes in the payload header
    DWORD   m_MediaObjectNumber;        // Number of the media object the payload belongs to
    DWORD   m_OffsetIntoMediaObject;    // Byte offset into the media object that the payload belongs to
    DWORD   m_PresentationTime;         // Time used to sync when the payload will be processed by filters in the filter graph
    DWORD   m_ReplicatedDataLength;     // Size, in bytes, of the replicated data
    DWORD   m_PayloadDataLength;        // Length in bytes of the payload data
    BYTE*   m_ReplicatedData;           // Data for Payload Extension Systems 
    BYTE*   m_PayloadData;              // The actual payload data

    BOOL    m_bCompressed;              // state variable - signifies that the payload was a compressed subpayload
    BOOL    m_bLoaded;                  // state variable - signifies that a payload has been successfully loaded and parsed
};

// Media Object indices will use largest media object number from the last packet as their upper bound
// when they are allocated.  If creation fails, additional attempts will be made using increasing multiples
// of the media object number.  This value defines the highest multiple that will be used.
#define MAX_INDEX_SCALE 10

// Encapsulates an ASF file
class CASFFile
{
public:
    CASFFile();
    virtual ~CASFFile();

    /**** Public Configuration Functions ****/

    // Take the name of a file that is supposedly an ASF file and attempt to parse it.
    virtual HRESULT Load(TCHAR* szFileName);

    // Reset the object to an unloaded state
    virtual void Unload();

    // Set the next packet number to retrieve
    virtual void SetNextPacket(DWORD dwPacketNumber);


    /**** Data Retrieval Functions ****/

    // Fetch the data packet corresponding to the specified packet number
    virtual CASFPacket* GetPacket(DWORD dwPacketNumber);

    // Fetch the next data packet (relative to the last packet fetched) within the data section of the file
    virtual CASFPacket* GetNextPacket();

    // Fetch the data contained within the media object of the stream and object number specified
    virtual BYTE* GetMediaObjectData(BYTE StreamNumber, DWORD dwMediaObjectNumber);

    // Fetch the media object of the stream nearest (but later than) the presentation time specified.
    virtual CASFMediaObject* GetMediaObject(BYTE StreamNumber, LONGLONG llPresentationTime);

	// Fetch the a media object for a given stream
	virtual CASFMediaObject* GetMediaObject(BYTE StreamNumber, DWORD dwMediaObjectNumber);

    // Find the numbers of the next complete media objects after the specified packet for all streams
    // returns data in a pre-allocated 2D array with dimensions [m_streamCount] x[2] .
    // Each sub array contains 2 entries.  The first is the stream number and the second is the media object number for that stream.
    // A -1 value will be used for any stream for which a completed media object is not found after the last packet is processed.
    // If a stream number is specified in dwKeyFrameStream, searching will start from the next occurring key frame of that stream
    virtual HRESULT GetNextMediaObjectNumbers(DWORD dwPacketNumber, int** pMediaObjectNumbers, DWORD dwKeyFrameStream);

    // Fetch the number of the simple index that corresponds to the stream number if it exists.
    // Returns -1 if not found.
    DWORD FindSimpleIndex(DWORD dwStreamNumber);

    // Get the nth simple index within the ASF file
    CASFSimpleIndex* GetSimpleIndex(DWORD dwIndex);

    // Find the index into the array of stream properties objects that corresponds with the specified stream number
    virtual DWORD FindStreamPropertiesIndex(BYTE streamNumber);

    // Fetch the stream properties object for a particular stream
    virtual CASFStreamProperties* GetStreamProperties(DWORD dwPropertiesIndex);


    /**** File Properties Object Accessor Functions ****/

    // Checks to see whether the file is seekable
    virtual BOOL IsSeekable();

    // Checks to see whether the file is in the process of being created (Broadcast)
    virtual BOOL IsBroadcast();

    // Fetch the maximum instantaneous bit rate in bits per second for the entire file
    virtual DWORD GetMaxBitRate();

    // Fetch the maximum size of packets within the data object
    virtual DWORD GetMaximumDataPacketSize();

    // Fetch the amout of time to buffer data before starting to play the file (in milliseconds)
    virtual QWORD GetPreroll();
     
    // Fetch the number of streams within the file
    virtual DWORD GetStreamCount();

    // Fetch the number of video streams within the file
    virtual DWORD GetVideoStreamCount();

    // Fetch the total time needed to play the file in 100ns units.
    virtual QWORD GetPlayDuration();

	// Fetch the total number of packets within the stream.
	virtual QWORD GetDataPacketsCount();

private:
    /**** Parsing helper functions ****/
    
    // Read dwSize bytes from the file into a buffer.
    virtual HRESULT ReadData(void* pBuffer, QWORD dwSize);

    // Move the file pointer back qwBytes bytes
    virtual HRESULT Rewind(QWORD qwBytes);

    // Move the file pointer forward qwBytes bytes
    virtual HRESULT FastForward(QWORD qwBytes);

    // Move the file pointer to the desired offset from the beginning of the file
    virtual HRESULT Seek(QWORD qwBytes);

    // Get the current position of the file pointer
    virtual HRESULT GetCurrentPosition(QWORD* pPosition);

    // Read the File Properties Object from the ASF file
    virtual HRESULT ReadFilePropertiesObject();

    // Read a Stream Properties Object from an ASF file
    virtual HRESULT ReadStreamPropertiesObject(QWORD qwSize);

    // Read a Simple Index Object from an ASF file
    virtual HRESULT ReadSimpleIndexObject(QWORD qwSize);

    // Move the file pointer past the next ASF object
    virtual HRESULT ReadUnknownObject();
    
    /**** Internal Index Helper functions ****/

    // Create a set of Media Object indices within a pre-allocated 2-Dimensional array of CASFMediaObject pointers
    // The dimensions of the array are [m_streamCount] x [dwIndexLength]
    virtual HRESULT BuildMediaObjectIndices(CASFMediaObject*** pppIndices, DWORD dwIndexLength);

    // Search for the next complete media object occurring after packet dwPacketNumber for Media Object Index dwIndexNumber
    virtual int FindNextCompleteMediaObject(DWORD dwPacketNumber, DWORD dwIndexNumber);

    // Search for the media object with the closest presentatin time to the one specified (but still after)
    virtual int FindMediaObjectByTime(LONGLONG llPresentationTime, DWORD dwIndexNumber);

	// Find the internal index of a stream based on its stream number
	virtual int GetStreamIndex(BYTE StreamNumber);
    
    HANDLE m_hFile;                 // Handle to the ASF file
    QWORD m_DataPacketsStart;    // points to the first byte of the first data packet within the ASF File
    BOOL m_bLoaded;                 // state variable - signifies that an ASF file is currently loaded and parsed
    BOOL m_bFixedPacketSize;        // state variable - signifies whether packet size is fixed; can't fetch indices or seek if it's not
    DWORD m_NextPacketNumber;       // state variable - number of the next packet to retrieve

    DWORD m_StreamCount;            // Count of streams within the ASF File
    CASFStreamProperties** m_ppStreamProperties;   // Array of stream properties objects
    DWORD m_numPropertiesObjects;
    DWORD m_MediaObjectIndexSize;               // count of elements within each Media Object Index
    CASFMediaObject*** m_pppMediaObjectIndices; // 2D array of media object pointers
    DWORD m_VideoStreamCount;       // Count of the video streams within the ASF file
    CASFSimpleIndex**   m_ppSimpleIndices;      // Array of Simple Indices of video streams
    
    // File Properties Object values
    GUID m_FileID;                  // Unique Identifier for the file
    QWORD m_FileSize;               // size, in bytes, of the ASF file
    QWORD m_CreationDate;           // Date and time of initial file creation in 100ns units since Jan 1, 1601
    QWORD m_DataPacketsCount;       // The number of data packet entries that exist in the Data Object
    QWORD m_PlayDuration;           // Time needed to play the file in 100ns units
    QWORD m_SendDuration;           // Time needed to send the fiel in 100ns units
    QWORD m_Preroll;                // Time spent buffering data before starting to play the file
    BOOL m_Broadcast;              // broadcast bit status
    BOOL m_Seekable;               // seekable bit status
    DWORD m_MinimumDataPacketSize;      // Minimum data packet size in bytes
    DWORD m_MaximumDataPacketSize;      // Maximum data packet size in bytes
    DWORD m_MaximumBitrate;         // Maximum instantaneous bit rate in bits per second for the entire file
    
};

// Encapsulates an ASF Stream Properties Object
class CASFStreamProperties
{
public:
    CASFStreamProperties();
    CASFStreamProperties(CASFStreamProperties* pProperties);
    
    virtual ~CASFStreamProperties();

    // Take a buffer supposedly containing a Stream Properties Object and parse it
    virtual HRESULT Load(BYTE* pRawData);

    // Return the Stream Properties object to an unloaded state
    virtual void Unload();

    // Fetch the number of the stream this object describes
    virtual BYTE GetStreamNumber();

    // Fetch the stream type of this stream
    virtual GUID GetStreamType();

    // Fetch the error correction type used by this stream
    virtual GUID GetErrorCorrectionType();

    // Fetch the offset that will be added to the presentation time of each sample within this stream
    virtual QWORD GetTimeOffset();

    // Fetch the length of type-specific data used by this stream
    virtual DWORD GetTypeSpecificDataLength();

    // Fetch the length of the error correction data used by this stream
    virtual DWORD GetErrorCorrectionDataLength();

    // Fetch the property flags value for this stream
    virtual WORD GetPropertyFlags();

    // checks to see whether the data for this stream is encrypted
    virtual BOOL IsEncrypted();

    // Return a copy of the raw data read from the asf file that comprises this object
    virtual BYTE* GetRawData();
    

private:
    BYTE* m_RawData;                // raw data of the stream properties object
    BOOL m_bLoaded;                 // state variable - signifies the object has parsed and loaded stream properties data

    // Stream Propeties Object values 
    GUID m_StreamType;              // Type of the stream
    GUID m_ErrorCorrectionType;     // Type of error correction used within the stream
    QWORD m_TimeOffset;             // Presentation time offset of the stream in 100ns units
    DWORD m_TypeSpecificDataLength; // length (in bytes) of m_TypeSpecificData
    DWORD m_ErrorCorrectionDataLength; // length (in bytes) of m_ErrorCorrectionData
    WORD m_PropertyFlags;           // defines the stream number and whether the stream is encrypted
    BYTE* m_TypeSpecificData;       // Type-specific format data (points within m_RawData)
    BYTE* m_ErrorCorrectionData;    // Data specific to the error correction type (points within m_RawData)
};

#define ASF_S_OBJECTCOMPLETE 0x27770001      // signifies a call to AddPayload just completed a media object

// Encapsulates an ASF Media Object
class CASFMediaObject
{
    friend class CASFFile;
public:
    CASFMediaObject();
	CASFMediaObject(CASFMediaObject* obj);
    virtual ~CASFMediaObject();

    // Restore this object to an unbound state
    virtual void Unload();

    // Attempt to add indices for the specified payload to this object;  If no objects have been added
    // previously, then this object is bound to the media object data of the payload.
    virtual HRESULT AddPayload(CASFPayload* pPayload, DWORD dwPacketNumber, DWORD dwPayloadIndex);

    // Fetch the ID of the object
    virtual DWORD GetMediaObjectNumber();

    // Fetch the number of payloads that are currently indexed as part of the Media Object
    virtual int GetPayloadsIndexed();
    
    // Fetch the packet number of the nth payload in the media object (indicated by dwObjectIndex)
    // Note: specifying a -1 for objectIndex will return the LAST entry listed for COMPLETE objects
    virtual DWORD GetPacketIndex(int objectIndex);

    // Fetch the position of the payload within its native packet of the nth payload in the media object (indicated by dwObjectIndex)
    // Note: specifying a -1 for objectIndex will return the LAST entry listed for COMPLETE objects
    virtual DWORD GetPayloadIndex(int objectIndex);

    // Fetch the stream number of the media object
    virtual BYTE GetStreamNumber();

    // Fetch the total number of bytes of the payloads indexed
    virtual DWORD GetTotalBytes();

	virtual DWORD GetBytesRead();

	virtual DWORD GetArraySize();

    // Checks to see whether all of the indices of the payloads that comprise the object are known
    virtual BOOL IsComplete();

    // Checks to see whether the media object is a key frame
    virtual BOOL IsKeyFrame();

    // Fetch the presentation time of the media object
    virtual LONGLONG GetPresentationTime();

private:
    DWORD m_MediaObjectNumber;      // Number of the object (ID)
    BYTE m_StreamNumber;           // Stream the object belongs to.
    DWORD* m_PacketNumbers;         // Numbers of packets containing payloads belonging to this object
    DWORD* m_PayloadIndices;        // Indices into the packets of the payloads
    int m_PayloadsIndexed;        // number of payloads currently indexed by this object
    DWORD m_ArraySize;              // Length of the arrays
    DWORD m_BytesRead;              // sum of the indexed payloads
    DWORD m_TotalBytes;             // total number of bytes of all of the indexed payloads
    BOOL m_bKeyFrame;               // Media Object is a key frame
    LONGLONG m_PresentationTime;       // Presentation time of the media object


    BOOL m_bComplete;                // status variable - indicates all of the payloads for the object have been indexed.
    
};

#endif

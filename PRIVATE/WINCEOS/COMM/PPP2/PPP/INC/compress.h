//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//************************************************************************
//            Microsoft Corporation
//
//
//  This file uses 4 space tabs
//************************************************************************

#ifdef COMP_12K

#define HISTORY_SIZE        	16000

#else

// Maximum back-pointer value

#define HISTORY_SIZE        	(8192U) 

#endif

// Maximum back-pointer value

#define HISTORY_MAX     		(HISTORY_SIZE -1) 
#define HASH_TABLE_SIZE     	4096
#define MAX_BACK_PTR        	8511
#define MAX_COMPRESSFRAME_SIZE 	1600


// Defines

#define COMPRESSION_PADDING 4
#define PACKET_FLUSHED      0x80
#define PACKET_AT_FRONT     0x40
#define PACKET_COMPRESSED   0x20
#define PACKET_ENCRYPTED    0x10



struct SendContext 
{
    UCHAR   History[ HISTORY_SIZE+1 ];
    int     CurrentIndex;           // how far into the history buffer we are
    PUCHAR  ValidHistory;           // how much of history is valid
    UCHAR   CompressBuffer[ MAX_COMPRESSFRAME_SIZE ];
    USHORT  HashTable[ HASH_TABLE_SIZE ];
};

typedef struct SendContext SendContext;


struct RecvContext 
{
    UCHAR     History[ HISTORY_SIZE+1 ];

#if DBG
#define DEBUG_FENCE_VALUE   0xABABABAB
    ULONG       DebugFence;
#endif

    UCHAR     *CurrentPtr ;     // how far into the history buffer we are
};

typedef struct RecvContext RecvContext;


// Function Prototypes

USHORT
compress( UCHAR         *CurrentBuffer,
          UCHAR         *CompOutBuffer,
          ULONG         *CurrentLength,
          SendContext   *context );


int
decompress ( UCHAR 	 	 *inbuf,
             int    	 inlen,
             int    	 start,
             UCHAR  	 **output,
             int    	 *outlen,
             RecvContext *context) ;

void getcontextsizes (long *, long *) ;
void initsendcontext (SendContext *) ;
void initrecvcontext (RecvContext *) ;



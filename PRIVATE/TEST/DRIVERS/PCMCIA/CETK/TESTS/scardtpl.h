//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
// SCARDTPL.H

#ifndef SCARDTPL_H
#define SCARDTPL_H

// Card types.
//#define CARD_MEMORY 1
//#define CARD_IO     2

class SCardTuple{

    	friend class SCard  ;

    	UINT8  uTupleCode   ;  // One of the CISTPL_ items.
    	UINT8  uTupleLink   ;  // Length of the tuple, excluding the header.

    	UINT16 fFlags       ;  // fFlags & TUPLE_FLAG_COMMON ids the space.
    	UINT32 uCISOffset   ;  // Bytes into current space.
    	UINT32 uLinkOffset  ;  // For link tuples, the link target offset.

    	PUCHAR pTupleData   ;  // The tuple data.

    	PVOID  parsedBuf    ;  // Parsed tuple output.
    	UINT32 parsedSize   ;  // Parsed buffer size.
    	UINT   nParsedItems ;  // nItems

    	SCardTuple *link    ;  // For the linked list.

    	//-----------------

    	SCardTuple (UINT8 cisTplCode, UINT8 length, PCARD_DATA_PARMS pParms) ;
    	~SCardTuple () ;

    	int idMatches (UINT16 flags, UINT32 offset) ;
    	int idMatches (PCARD_TUPLE_PARMS) ;
    	int idMatches (PCARD_DATA_PARMS) ;

    	int idPreceeds (UINT16 flags, UINT32 offset) ;
    	int idPreceeds (PCARD_TUPLE_PARMS p) ;
    	int idPreceeds (PCARD_DATA_PARMS p) ;

    	VOID dump () ;
  } ;

class HCard ;

class SCard{

    	SCardTuple *head ;
    	SCardTuple *current ;
    	UINT16     fFlags ;      // These are the link flags for the cursor.
    	UINT32     uLinkOffset ; // Bytes into target space when a link is followed.

    	int isFound     (PCARD_TUPLE_PARMS) ;

    	int followALink (PCARD_TUPLE_PARMS) ;
    	int followTheChain () ;

    	int scanOneStep (PCARD_TUPLE_PARMS) ;
   	STATUS scanTo   (PCARD_TUPLE_PARMS) ;

    	int firstStopRejectLinks (UINT8) ;
    	int firstStopAcceptLinks (UINT8) ;
    	int firstStop (UINT8, UINT16) ;

    
    	// Check p and p->hSocket.
    	STATUS socketCheck (PCARD_TUPLE_PARMS) ;
    	int isValid () ;

  public:
    	SCard (HCard *, UINT8 cSoc, UINT8 cFunc) ;
    	~SCard () ;

    	VOID addTuple (PCARD_TUPLE_PARMS, HCard *) ;
    	VOID addTuple (UINT8 cisTplCode, UINT8 length, PCARD_DATA_PARMS pParms) ;

    	STATUS CardGetFirstTuple  (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetNextTuple   (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetTupleData   (PCARD_DATA_PARMS) ;
    	STATUS CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple,
                               PVOID pBuf, PUINT pnItems) ;
    	STATUS CardGetThisTuple   (PCARD_TUPLE_PARMS) ;

    	int goToId (UINT16 flags, UINT32 offset) ;
    	int goToId (PCARD_TUPLE_PARMS) ;
    	int goToId (PCARD_DATA_PARMS) ;
    	int goToLinkTarget () ;

    	int idMatches (UINT16 flags, UINT32 offset, SCardTuple *p) ;
    	int idMatches (PCARD_DATA_PARMS, SCardTuple *) ;
    	int idMatches (PCARD_TUPLE_PARMS, SCardTuple *) ;

    	int isPresent (PCARD_TUPLE_PARMS) ;

    	UINT8 getTupleType (PCARD_TUPLE_PARMS) ;

    	VOID dump () ;
} ;

class HCard{
  public:
    	HCard () ;
    	~HCard () ;

    	STATUS CardGetFirstTuple  (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetNextTuple   (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetTupleData   (PCARD_DATA_PARMS) ;
    	STATUS CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple,
                               PVOID pBuf, PUINT pnItems) ;
} ;

class MatchedCard{

    	SCard *pSCard ;
    	HCard *pHCard ;

    	STATUS sStatus,         hStatus ;
    	UINT8  usTupleCode,     uhTupleCode ;
    	UINT8  usTupleLink,     uhTupleLink ;
    	UINT16 usDataLen,       uhDataLen ;
    	int    fDataMatches ;
    	UINT32 unExtraData ;

 public:
    	// Open on a given slot and function.
    	MatchedCard (UINT8 cSlot, UINT8 cFunc) ;
    	~MatchedCard () ;

    	VOID getMismatchedParams (UINT32 *which, UINT32 *softValue, UINT32 *hardValue) ;

    	STATUS CardGetFirstTuple  (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetNextTuple   (PCARD_TUPLE_PARMS) ;
    	STATUS CardGetTupleData   (PCARD_DATA_PARMS, UINT32) ;
    	STATUS CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple,
                               PVOID pBuf, PUINT pnItems,  UINT32 bufSize) ;
    	UINT8 getTupleType (PCARD_TUPLE_PARMS) ;
    	UINT8 getTupleType (PCARD_DATA_PARMS) ;

    	UINT getMaxParsedItems (CARD_SOCKET_HANDLE, UINT8) ;

    	int syncronizeSoftCard (PCARD_TUPLE_PARMS) ;
    	int syncronizeSoftCard (PCARD_DATA_PARMS) ;

    	int  isPresent (PCARD_TUPLE_PARMS) ;
    	VOID guaranteePresence (PCARD_TUPLE_PARMS) ;

    	VOID dump () ;
} ;

#endif // SCARDTPL_H



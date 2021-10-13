//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
// SCARDTPL.CPP

// PCMCIA tests of:
// CardGetFirstTuple
// CardGetNextTuple
// CardGetTupleData
// CardGetParsedTuple

#include "TestMain.h"
#include "common.h"
#include "scardtpl.h"
#include "tpltest.h"

//------------------------------ Utility functions --------------------------

// This test for modem vs. memory card is derived

static PCARD_TUPLE_PARMS newCopy (PCARD_TUPLE_PARMS p){
    	PCARD_TUPLE_PARMS q = new CARD_TUPLE_PARMS ;
    	if (!q) 
    		return 0 ;
    	memcpy (q, p, sizeof (CARD_TUPLE_PARMS)) ;
    	return q ;
  }

// Warning, this requires sizeOfP >= sizeof(CARD_DATA_PARMS)
static PCARD_DATA_PARMS newCopy (PCARD_DATA_PARMS p, UINT32 sizeOfP){
    	PUCHAR q = new UCHAR [sizeOfP] ;
    	if (!q) 
    		return 0 ;
    	memset (q, 0, sizeOfP) ;
    	memcpy (q, p, sizeof(CARD_DATA_PARMS)) ;
    	return (PCARD_DATA_PARMS) q ;
}

// Warning, this is dangerous because p->uTupleLink may not be initialized.
static PCARD_DATA_PARMS newDataParmsFromTupleParms (PCARD_TUPLE_PARMS p){
	
    	UINT16 size = p->uTupleLink + sizeof (CARD_DATA_PARMS) ;
    	if (!size) 
    		return 0 ;

    	PCARD_DATA_PARMS q = (PCARD_DATA_PARMS) new UCHAR [size] ;
    	if (!q) 
    		return 0 ;
    	memset (q, 0, size) ;
    	memcpy (q, p, sizeof (CARD_TUPLE_PARMS)) ;

    	q->uBufLen = p->uTupleLink ;
    	q->uDataLen = 0 ;
    	q->uTupleOffset = 0 ; // This was a BIG gotcha.

    	return q ;
}

//------------------------------ SCardTuple ---------------------------------

#define TUPLE_LINK_ZONE         0x001F    // All of the following bits
#define TUPLE_FLAG_COMMON       0x0001    // Tuples are in common memory
#define TUPLE_FLAG_LINK_TO_A    0x0002    // Link to attribute memory
#define TUPLE_FLAG_LINK_TO_C    0x0004    // Link to common memory
#define TUPLE_FLAG_NO_LINK      0x0008    // Do not take a link at CISTPL_END
#define TUPLE_FLAG_IMPLIED_LINK 0x0010    // If attribute first byte is 0xFF.

SCardTuple::SCardTuple (UINT8 cisTplCode, UINT8 length, PCARD_DATA_PARMS pParms){

    	uTupleCode   = cisTplCode ;
    	uTupleLink   = length ;
    	fFlags       = pParms->fFlags ;
    	uCISOffset   = pParms->uCISOffset ;
    	uLinkOffset  = 0 ;
    	pTupleData   = NULL ;
    	parsedBuf    = 0 ;
    	parsedSize   = 0 ;
    	nParsedItems = 0 ;
    	link         = 0 ;

    	// Try to verify that we have a whole tuple, and set the tuple data.
    	if (!pParms->uDataLen || (pParms->uDataLen != uTupleLink)){
        	pTupleData = NULL ;
        	uTupleLink = 0 ;
      	}
    	else{
        	pTupleData = new UCHAR [uTupleLink] ;
        	if (pTupleData)
          	memcpy (pTupleData, (PUCHAR)pParms + sizeof (CARD_DATA_PARMS), uTupleLink) ;
      	}

    	CARD_SOCKET_HANDLE hSocket = pParms->hSocket ;
    	switch (uTupleCode){
        	case CISTPL_LONGLINK_A:
        	case CISTPL_LONGLINK_C:
          		if (pTupleData && (uTupleLink >= 4)) 
          			uLinkOffset = * (UINT32*) pTupleData ;
          		else uLinkOffset = 0 ; // This never happens.
          		break ;

        	case CISTPL_CONFIG:
          		nParsedItems = 0 ; // GLITCH
          		CardGetParsedTuple(hSocket, uTupleCode, 0, &nParsedItems);
          		parsedSize = nParsedItems * sizeof (PARSED_CONFIG) ;

          		if (parsedSize){
              		parsedBuf = new UCHAR [parsedSize] ;
              		if (parsedBuf)
                			CardGetParsedTuple(hSocket, uTupleCode, parsedBuf, &nParsedItems);
              		else parsedSize = 0 ;
            		}
          		else 
          			nParsedItems = 0 ; // GLITCH
          		break ;

        	case CISTPL_CFTABLE_ENTRY:
          		nParsedItems = 0 ; // GLITCH
          		CardGetParsedTuple(hSocket, uTupleCode, 0, &nParsedItems);
          		parsedSize = nParsedItems * sizeof (PARSED_CFTABLE) ;

          		if (parsedSize){
              		parsedBuf = new UCHAR [parsedSize] ;
              		if (parsedBuf)
                			CardGetParsedTuple(hSocket, uTupleCode, parsedBuf, &nParsedItems);
              		else 
              			parsedSize = 0 ;
            		}
          		else 
          			nParsedItems = 0 ;
          		break ;
      	}
}

SCardTuple::~SCardTuple () {delete pTupleData ; delete parsedBuf ;}

int SCardTuple::idMatches (UINT16 flags, UINT32 offset){
    	if (offset != uCISOffset) 
    		return 0 ;
    	if ((TUPLE_FLAG_COMMON & fFlags) != (TUPLE_FLAG_COMMON & flags)) 
    		return 0 ;
    	return 1 ;
}

int SCardTuple::idMatches (PCARD_TUPLE_PARMS p) {return idMatches (p->fFlags, p->uCISOffset) ;}
int SCardTuple::idMatches (PCARD_DATA_PARMS p)  {return idMatches (p->fFlags, p->uCISOffset) ;}

int SCardTuple::idPreceeds (UINT16 flags, UINT32 offset){
    	// Yes if I am in Attr and it is in Common.
    	if (!(TUPLE_FLAG_COMMON & fFlags) &&  (TUPLE_FLAG_COMMON & flags)) 
    		return 1 ;

    	// No if I am in Common and it is in Attr.
    	if ( (TUPLE_FLAG_COMMON & fFlags) && !(TUPLE_FLAG_COMMON & flags)) 
    		return 0 ;

    	// We are in the same space, so check the offsets.
    	return uCISOffset < offset ;
  }

int SCardTuple::idPreceeds (PCARD_TUPLE_PARMS p) {return idPreceeds (p->fFlags, p->uCISOffset) ;}
int SCardTuple::idPreceeds (PCARD_DATA_PARMS p)  {return idPreceeds (p->fFlags, p->uCISOffset) ;}

VOID SCardTuple::dump () {REPORT_TUPLE (uTupleCode, uTupleLink, fFlags, uCISOffset, nParsedItems) ;}

//------------------------------ SCard --------------------------------------

SCard::SCard (HCard *pHCard, UINT8 cSoc, UINT8 cFunc)
  : head(0), current(0), uLinkOffset(0), fFlags(0)
{
    	PCARD_TUPLE_PARMS pTuple = new CARD_TUPLE_PARMS ;

    	// Set up to get the primary chain of tuples.
    	(pTuple->hSocket).uSocket   = cSoc ;
    	(pTuple->hSocket).uFunction = cFunc ;
    	pTuple->fAttributes = 0xFF ;
    	pTuple->uDesiredTuple = 0xFF ;

    	// Now get started
    	STATUS status = pHCard->CardGetFirstTuple (pTuple) ;

    	// Add the primary tuple chain, others are added as found.
    	while (status == CERR_SUCCESS){
        	addTuple (pTuple, pHCard) ;
        	status = pHCard->CardGetNextTuple (pTuple) ;
      }

    	delete pTuple ;
  }

SCard::~SCard (){
    	for (current = head ; current ; current = head){
    		head = current->link ; 
    		delete current ;
    	}
}

// For the goofy rules.
int isLink (UINT8 code){
    	switch (code){
        	case CISTPL_LONGLINK_CB:   
        	case CISTPL_LONG_LINK_MFC:
        	case CISTPL_LONGLINK_A:   
        	case CISTPL_LONGLINK_C:    
        	case CISTPL_LINKTARGET:   
        	case CISTPL_NO_LINK:       
        	case CISTPL_END:           
        	case CISTPL_NULL:          
        		return 1 ;
        	default:                   
        		return 0 ;
      }
}

int SCard::isFound (PCARD_TUPLE_PARMS p){
    	if (!current) 
    		return 0 ;

    	// If link tuples are Ok, we can settle this quickly.
    	if (p->fAttributes & 1){
        	if (p->uDesiredTuple == current->uTupleCode) 
        		return 1 ; // Really found.
        	if (p->uDesiredTuple == CISTPL_END)          
        		return 1 ; // Anything will do.
        	if (isLink(current->uTupleCode))             
        		return 1 ; // Nutty guess.
        	return 0 ;
      }

    	// So, we must not accept link tuples (unless p->uDesiredTuple is a link?).
    	// This reject list is from the Developer's Guide.
    	// Note: we take p->uDesiredTuple to override p->fAttributes
    	// if they contradict each other.  There is no documentation
    	// on this point.
    	switch (current->uTupleCode){
       	case CISTPL_LONGLINK_CB:   
       		return p->uDesiredTuple == CISTPL_LONGLINK_CB ;
        	case CISTPL_LONG_LINK_MFC: 
        		return p->uDesiredTuple == CISTPL_LONG_LINK_MFC ;
        	case CISTPL_LONGLINK_A:    
        		return p->uDesiredTuple == CISTPL_LONGLINK_A ;
        	case CISTPL_LONGLINK_C:    
        		return p->uDesiredTuple == CISTPL_LONGLINK_C ;
        	case CISTPL_LINKTARGET:    
        		return p->uDesiredTuple == CISTPL_LINKTARGET ;
        	case CISTPL_NO_LINK:       
        		return p->uDesiredTuple == CISTPL_NO_LINK ;
        	default:                   
        		break ;
      }

    	// We ran out of special rules, and current is not a link tuple.
    	if (p->uDesiredTuple == current->uTupleCode) 
    		return 1 ;
    	if (p->uDesiredTuple == CISTPL_END)          
    		return 1 ;

    	return 0 ;

}

int SCard::followALink (PCARD_TUPLE_PARMS p){
    	SCardTuple *pTpl = current ;
    	UINT32 uLinkOffset = p->uLinkOffset ;
    	UINT16 fFlags      = p->fFlags ;

    	// Set attr/common bit in fFlags for the link target.
    	if (fFlags & TUPLE_FLAG_LINK_TO_A)      
    		fFlags = 0 ;
    	else if (fFlags & TUPLE_FLAG_LINK_TO_C) 
    		fFlags = TUPLE_FLAG_COMMON ;

    	if (!goToId(fFlags,uLinkOffset)){
    		current = pTpl ; 
    		return 0 ;
    	}

    	if (current->uTupleCode != CISTPL_LINKTARGET){
    		current = pTpl ; 
    		return 0 ;
    	}

    	return followTheChain () ;
  }

int SCard::followTheChain (){
    	if (current && current->link){
        	current = current->link ;
        	return 1 ;
      }

    	return 0 ;
}

static int shouldFollowLink (UINT16 flags){
    	if (flags & TUPLE_FLAG_NO_LINK) 
    		return 0 ;
    	if (flags & TUPLE_FLAG_LINK_TO_A) 
    		return 1 ;
    	if (flags & TUPLE_FLAG_LINK_TO_C) 
    		return 1 ;
    	if (flags & TUPLE_FLAG_IMPLIED_LINK) 
    		return 1 ;
    	return 0 ;
 }

int SCard::scanOneStep (PCARD_TUPLE_PARMS p){
    	if (!current) 
    		return 0 ;

    	SCardTuple *pStart = current ;

    	if (p->uTupleCode == CISTPL_END){
        	if (shouldFollowLink(p->fFlags)) 
        		followALink (p) ;
        	else 
        		followTheChain () ;
      }
    	else 
    		followTheChain () ;

    	if (pStart == current) 
    		return 0 ;

    	// Wa are now parked on a new tuple.
    	p->uTupleCode = current->uTupleCode ;
    	p->uTupleLink = current->uTupleLink ;
    	p->uCISOffset = current->uCISOffset ;

    	// Now we must set fFlags and uLinkOffset
    	switch (current->uTupleCode){
        	case CISTPL_LONGLINK_CB:
          		p->fFlags = TUPLE_FLAG_LINK_TO_C ;
          		p->uLinkOffset = current->uLinkOffset ;
         		 break ;

        	case CISTPL_LONG_LINK_MFC:
          		p->fFlags = TUPLE_FLAG_LINK_TO_C ;
          		p->uLinkOffset = current->uLinkOffset ;
          		break ;

        	case CISTPL_LONGLINK_A:
          		p->fFlags = TUPLE_FLAG_LINK_TO_A ;
          		p->uLinkOffset = current->uLinkOffset ;
          		break ;

        	case CISTPL_LONGLINK_C:
          		p->fFlags = TUPLE_FLAG_LINK_TO_C ;
          		p->uLinkOffset = current->uLinkOffset ;
          		break ;

        	case CISTPL_LINKTARGET:
          		p->fFlags = 0 ;
          		p->uLinkOffset = 0 ;
          		break ;

        	case CISTPL_NO_LINK:
          		p->fFlags = TUPLE_FLAG_NO_LINK ;
          		p->uLinkOffset = 0 ;
          		break ;

        	default:
          		// Retain fFlags and uLinkOffset.
          		break ;
      	}

    	return 1 ;
}

STATUS SCard::scanTo (PCARD_TUPLE_PARMS p){
    	// This happens if there are no tuples.
    	if (!current) 
    		return CERR_BAD_VERSION ;

    	UINT16 fAcceptLinks  = p->fAttributes & 1 ;

    	while (scanOneStep (p)){
        	if (fAcceptLinks){
            		if (current->uTupleCode == p->uDesiredTuple) 
            			return CERR_SUCCESS ;
            		if (isLink(current->uTupleCode))             
            			return CERR_SUCCESS ;
            		if ((p->uDesiredTuple != CISTPL_END) && (current->uTupleCode == CISTPL_END))
              		return CERR_NO_MORE_ITEMS ;
          	}
        	else{
            		if (firstStopRejectLinks(p->uDesiredTuple))
            			return (current->uTupleCode == p->uDesiredTuple) ? CERR_SUCCESS : CERR_NO_MORE_ITEMS ;
            		if ((p->uDesiredTuple != CISTPL_END) && (current->uTupleCode == CISTPL_END))
              		return CERR_NO_MORE_ITEMS ;
          	}
      	}

    	return CERR_NO_MORE_ITEMS ;
}

// Check p and p->hSocket.
STATUS SCard::socketCheck (PCARD_TUPLE_PARMS p){
    	if (!p) 
    		return CERR_BAD_ARGS ;
//  With variable socket handle support it is no longer viable to check socket numbers
//  if ((p->hSocket).uSocket || (p->hSocket).uFunction) return CERR_BAD_SOCKET ;
    	return CERR_SUCCESS ;
}

int SCard::isValid () {return head ? 1 : 0 ;}

VOID SCard::addTuple (PCARD_TUPLE_PARMS pParms, HCard *p){
    	PCARD_DATA_PARMS pData = newDataParmsFromTupleParms (pParms) ;

    	if (CERR_SUCCESS == p->CardGetTupleData(pData))
      		addTuple (pParms->uTupleCode, pParms->uTupleLink, pData) ;
    	delete pData ;
}

VOID SCard::addTuple (UINT8 cisTplCode, UINT8 length, PCARD_DATA_PARMS pParms){
    	// This might be the first tuple.
    	if (!head) {
    		current = head = new SCardTuple (cisTplCode, length, pParms) ; 
    		return ;
    	}

    	// Straddle the target location.
    	SCardTuple *pLead = head, *pTrail = 0 ;
   	while (pLead && pLead->idPreceeds(pParms)) 
   	 	pLead = (pTrail=pLead)->link ;

    	// Maybe it is already there.
    	if (pLead && pLead->idMatches(pParms)) 
    		return ;


    	// Make a new tuple.
    	SCardTuple *pSCardTuple = new SCardTuple (cisTplCode, length, pParms) ;

    	// Now add it.
    	pSCardTuple->link = pLead ;
    	if (pTrail) 
    		pTrail->link = pSCardTuple ; // pTrail < pSCardTuple < pLead
    	else 
    		head = pSCardTuple ; // pLead == head, add at the start.
}

int SCard::firstStopRejectLinks (UINT8 uDesiredTuple){
    	switch (current->uTupleCode){
        	case CISTPL_LONGLINK_CB:   
        	case CISTPL_LONG_LINK_MFC: 
        	case CISTPL_LONGLINK_A:    
        	case CISTPL_LONGLINK_C:    
		case CISTPL_LINKTARGET: 
        	case CISTPL_NO_LINK:       
        		return 0 ;
      }

    	if (!current) 
    		return 1 ;

    	// Maybe this is it.
    	if (uDesiredTuple == current->uTupleCode) 
    		return 1 ;

    	// Maybe the tuples are exhausted.
    	if (!(current->link)) 
    		return 1 ;

    	return 0 ;
}

int SCard::firstStopAcceptLinks (UINT8 uDesiredTuple){
    	if (!current) 
    		return 1 ;

    	// Maybe this is it.
    	if (uDesiredTuple == current->uTupleCode) 
    		return 1 ;

    	// Maybe the tuples are exhausted.
    	if (!(current->link)) 
    		return 1 ;

    	// Now we can accept link tuples.
    	switch (current->uTupleCode){
        	case CISTPL_LONGLINK_CB: 
        	case CISTPL_LONG_LINK_MFC:
        	case CISTPL_LONGLINK_A:   
        	case CISTPL_LONGLINK_C:  
        	case CISTPL_LINKTARGET:   
        	case CISTPL_END:          
        	case CISTPL_NULL:      
        		return 1;
        	default:                   
        		return 0 ;
      }
}

int SCard::firstStop (UINT8 uDesiredTuple, UINT16 fAttributes){

    	int rtn = (fAttributes & 1) ? firstStopAcceptLinks (uDesiredTuple) :firstStopRejectLinks (uDesiredTuple) ;
    	return rtn ;
}

STATUS SCard::CardGetFirstTuple (PCARD_TUPLE_PARMS p){
    	STATUS rtnVal = socketCheck (p) ;
    	if (rtnVal != CERR_SUCCESS) 
    		return rtnVal ;

    	// If we are looking for a link and specified skip links
    	if (!(p->fAttributes&1) && isLink(p->uDesiredTuple)) 
    		return CERR_NO_MORE_ITEMS ;

    	current = head ;
    	while (current && (p->uDesiredTuple != current->uTupleCode)) 
    		current = current->link ;

    	// Possible quick answer.
    	if (!current) 
    		return CERR_NO_MORE_ITEMS ;

    	// Possible unacceptable link tuple
    	if (!(p->fAttributes&1) && isLink(current->uTupleCode)) 
    		return CERR_NO_MORE_ITEMS ;

    	// Mark the located tuple.
    	p->uTupleCode = current->uTupleCode ;
    	p->uTupleLink = current->uTupleLink ;

    	// Did we find what we were looking for ?
    	if (p->uDesiredTuple == current->uTupleCode){
        	return CERR_SUCCESS ;
      }
	//
	if (p->uDesiredTuple == CISTPL_NULL)
		return CERR_SUCCESS;
    	else
		return CERR_NO_MORE_ITEMS ;
}

STATUS SCard::CardGetNextTuple (PCARD_TUPLE_PARMS p){
    	STATUS rtnVal = socketCheck (p) ;

    	if (rtnVal != CERR_SUCCESS) 
    		return rtnVal ; // CISTPL_BAD_ARGS or CISTPL_BAD_SOCKET

    	if (current) 
    		rtnVal = scanTo (p) ;
    	else 
    		rtnVal = CERR_NO_MORE_ITEMS ;

    	if (rtnVal == CERR_NO_MORE_ITEMS) 
    		current = head ;

    	if (current){
        	p->uTupleCode = current->uTupleCode ;
        	p->uTupleLink = current->uTupleLink ;
      }

    	return rtnVal ;
 }

STATUS SCard::CardGetTupleData (PCARD_DATA_PARMS p)
  {
    STATUS rtnVal = socketCheck ((PCARD_TUPLE_PARMS)p) ;
    if (rtnVal != CERR_SUCCESS) return rtnVal ;
    if (!current) return CERR_READ_FAILURE ;
    if (!current->uTupleLink) {p->uDataLen = 0 ; return CERR_SUCCESS ;}

    // Maybe we cannot store all the requested tuple data.
    // uTupleOffset is the amount of the tuple to skip at the start.
    // According to Mr. Kanz, we always get all data from uTupleOffset to the end.

	if(current->uTupleCode == CISTPL_END){
		NKDbgPrintfW(_T("tplend"));
		return CERR_SUCCESS;
	}
    // Verify that we asked for at least 0 bytes.
    if (p->uTupleOffset > current->uTupleLink) {
		NKDbgPrintfW(_T("udatalen=0"));
		return CERR_BAD_ARG_LENGTH ;}

    // Verify that we will have p->uDataLen <= p->uBufLen.
    if ((current->uTupleLink - p->uTupleOffset) > p->uBufLen) {

		NKDbgPrintfW(_T("udatalen>ubuflen"));
		return CERR_BAD_ARG_LENGTH ;}

    // We will now have p->uDataLen <= p->uBufLen.
    p->uDataLen = current->uTupleLink - p->uTupleOffset ;

    // Now check to see if some data is actually requested.
    if (!p->uDataLen) return CERR_SUCCESS ;

    PUCHAR q = (PUCHAR)p + sizeof (CARD_DATA_PARMS) ;
    memcpy (q, current->pTupleData + p->uTupleOffset, p->uDataLen) ;
		NKDbgPrintfW(_T("Normal"));

    return CERR_SUCCESS ;
  }

STATUS SCard::CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket,
                                  UINT8 uDesiredTuple,
                                  PVOID pBuf,
                                  PUINT pnItems)
{
    	// Find the first uDesiredTuple, in pointer p.
    	for (SCardTuple *p = head ; p && (p->uTupleCode != uDesiredTuple) ; p = p->link) ;

    	// In case of no such tuple
    	if (!p) {
    		*pnItems = 0 ; 
    		return CERR_NO_MORE_ITEMS ;
    	}

    	// In case of no data
    	if (!p->parsedSize) {
    		*pnItems = 0 ; 
    		return CERR_NO_MORE_ITEMS ;
    	}

    	// Maybe we didn't even ask for any data
    	if (*pnItems == 0) 
    		return CERR_BAD_ARG_LENGTH ;

    	UINT nCount = (*pnItems > p->nParsedItems) ? p->nParsedItems : *pnItems ;

    	// I do not have the size of pBuf to check it.
    	switch (uDesiredTuple){
        	case CISTPL_CFTABLE_ENTRY:
          		memcpy (pBuf, p->parsedBuf, nCount * sizeof(PARSED_CFTABLE)) ;
          		*pnItems = nCount ;
          		return CERR_SUCCESS ;

        	case CISTPL_CONFIG:
          		memcpy (pBuf, p->parsedBuf, nCount * sizeof(PARSED_CONFIG)) ;
          		*pnItems = nCount ;
          		return CERR_SUCCESS ;

        	default: 
        		return CERR_READ_FAILURE ;
      	}
}

STATUS SCard::CardGetThisTuple (PCARD_TUPLE_PARMS p)
  {
    if (!p) return CERR_BAD_ARGS ;
    if (!goToId(p)) return CERR_NO_MORE_ITEMS ;
    p->uTupleCode = current->uTupleCode ;
    p->uTupleLink = current->uTupleLink ;
    return CERR_SUCCESS ;
  }

int SCard::goToId (UINT16 flags, UINT32 offset){
    	for (SCardTuple *p = head ; p && !p->idMatches(flags,offset) ; p = p->link) ;
    	if (p) {
    		current = p ; 
    		return 1 ;
    	}
    	return 0 ;
}

int SCard::goToId (PCARD_TUPLE_PARMS p) {return goToId (p->fFlags, p->uCISOffset) ;}
int SCard::goToId (PCARD_DATA_PARMS p)  {return goToId (p->fFlags, p->uCISOffset) ;}

int SCard::idMatches (UINT16 flags, UINT32 offset, SCardTuple *p)
  {return p->idMatches (flags, offset) ;}

int SCard::idMatches (PCARD_DATA_PARMS pData, SCardTuple *p)
  {return idMatches ((PCARD_TUPLE_PARMS)pData, p) ;}

int SCard::idMatches (PCARD_TUPLE_PARMS pTuple, SCardTuple *p)
  {return p->idMatches (pTuple) ;}

int SCard::isPresent (PCARD_TUPLE_PARMS p){
    	for (SCardTuple *q = head ; q ; q = q->link)
      		if (q->idMatches(p)) 
      			return 1 ;
    	return 0 ;
}

UINT8 SCard::getTupleType (PCARD_TUPLE_PARMS p){
    	for (SCardTuple *q = head ; q ; q = q->link)
      		if (q->idMatches(p)) 
      			return q->uTupleCode ;
    	return 0xFF ; // Unknown
}

VOID SCard::dump () {for (SCardTuple *q = head ; q ; q = q->link) q->dump () ;}



HCard::HCard () {;}

HCard::~HCard () {;}

STATUS HCard::CardGetFirstTuple  (PCARD_TUPLE_PARMS p){
    	STATUS status = CallCardServices(CardGetFirstTuple)(p);
    	return status ;
}

STATUS HCard::CardGetNextTuple   (PCARD_TUPLE_PARMS p){
    	STATUS status = CallCardServices(CardGetNextTuple)(p);
    	return status ;
}

STATUS HCard::CardGetTupleData   (PCARD_DATA_PARMS p){
    	STATUS status = CallCardServices(CardGetTupleData)(p);
    	return status ;
}

STATUS HCard::CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket,
                                  UINT8 uDesiredTuple,
                                  PVOID pBuf,
                                  PUINT pnItems)
{
    	STATUS status = CallCardServices(CardGetParsedTuple)(hSocket, uDesiredTuple, pBuf, pnItems);
    	return status ;
}

//------------------------------ MatchedCard --------------------------------

MatchedCard::MatchedCard (UINT8 cSlot, UINT8 cFunc){
    	pHCard = new HCard ;
   	pSCard = new SCard (pHCard, cSlot, cFunc) ;

    	sStatus      = hStatus     = 0 ;
    	usTupleCode  = uhTupleCode = 0 ;
   	usTupleLink  = uhTupleLink = 0 ;
    	usDataLen    = uhDataLen   = 0 ;
    	fDataMatches = 1 ;
    	unExtraData = 0 ;
}

MatchedCard::~MatchedCard () {delete pSCard ; delete pHCard ;}

VOID MatchedCard::getMismatchedParams (UINT32 *which, UINT32 *softValue, UINT32 *hardValue){

	if (sStatus != hStatus)   {
		if ((sStatus == CERR_NO_MORE_ITEMS)&&(hStatus == CERR_SUCCESS)){
			*which = 0xFFFFFFFF ; 
			*softValue = 0 ; 
			*hardValue = 0 ; 
			return ;
		}
	     	else{
	     		*which = 0 ; 
	     		*softValue = sStatus ;     
	     		*hardValue = hStatus ;     
	     		return ;
	     	}
	}

    	if (usTupleCode != uhTupleCode) {
    		*which = 1 ; 
    		*softValue = usTupleCode ; 
    		*hardValue = uhTupleCode ; 
    		return ;
    	}
    	if (usTupleLink != uhTupleLink) {
    		*which = 2 ; 
    		*softValue = usTupleLink ; 
    		*hardValue = uhTupleLink ; 
    		return ;
    	}
    	if (usDataLen != uhDataLen) {
    		*which = 3 ; 
    		*softValue = usDataLen ;   
    		*hardValue = uhDataLen ;  
    		return ;
    	}
    	if (!fDataMatches) {
    		*which = 4 ; 
    		*softValue = usDataLen ;   
    		*hardValue = uhDataLen ;   
    		return ;
    	}
    	if (unExtraData){
    		*which = 5 ; 
    		*softValue = 0 ;           
    		*hardValue = unExtraData ; 
    		return ;
    	}

    	// The values to return if all matches.
    	*which = 0xFFFFFFFF ; 
    	*softValue = 0 ; 
    	*hardValue = 0 ; 
    	return ;
}

STATUS MatchedCard::CardGetFirstTuple (PCARD_TUPLE_PARMS p){
    	// Get ready to write on p by keeping a copy.
    	PCARD_TUPLE_PARMS s = newCopy (p) ;
    	if (!s) 
    		return CERR_OUT_OF_RESOURCE ;

    	// Get HardCard result.
    	hStatus = pHCard->CardGetFirstTuple (p) ;
    	uhTupleCode = p->uTupleCode ;
    	uhTupleLink = p->uTupleLink ;

    	if (hStatus == CERR_SUCCESS){
        	// CISTPL_END has a second meaning: any tuple
        	if (p->uDesiredTuple == CISTPL_END){
	            sStatus      = hStatus ;
	            usTupleCode  = uhTupleCode ;
	            usTupleLink  = uhTupleLink ;
	            usDataLen    = uhDataLen ;
	            fDataMatches = 1 ;
	            unExtraData  = 0 ;
	            delete s ;
	            return hStatus ;
          	}

        	// Cover the possibility that SCard is not completely initialized.
        	// If hStatus != CERR_SUCCESS then guaranteePresence makes no sense.
        	guaranteePresence (p) ;
      }

    	// Get SCard result
    	sStatus = pSCard->CardGetFirstTuple (s) ;
    	usTupleCode = s->uTupleCode ;
    	usTupleLink = s->uTupleLink ;
    	delete s ;

    	if ((hStatus == CERR_NO_MORE_ITEMS) && (sStatus == CERR_NO_MORE_ITEMS)){
        	sStatus      = hStatus ;
        	usTupleCode  = uhTupleCode ;
        	usTupleLink  = uhTupleLink ;
        	usDataLen    = uhDataLen ;
        	fDataMatches = 1 ;
        	unExtraData  = 0 ;
        	return hStatus ;
      }

    	// CISTPL_NULL anomaly
    	if ((usTupleCode == CISTPL_END) && (uhTupleCode == CISTPL_NULL))
        	usTupleCode = CISTPL_NULL ;

    	// What we didn't try to get must be OK.
    	usDataLen = uhDataLen = 0 ;
    	fDataMatches = 1 ;
    	unExtraData = 0 ;

    	return hStatus ;
}

STATUS MatchedCard::CardGetNextTuple (PCARD_TUPLE_PARMS p){
    	hStatus = pHCard->CardGetNextTuple (p) ;
    	uhTupleCode = p->uTupleCode ;
    	uhTupleLink = p->uTupleLink ;

    	if (hStatus == CERR_SUCCESS){
        	// Cover the possibility that SCard is not completely initialized.
        	guaranteePresence (p) ;

        	// Maybe we were not supposed to find links.
        	if (!(p->fAttributes & 1) && isLink(p->uTupleCode)) 
        		sStatus = CERR_NO_MORE_ITEMS ;
        	else 
        		sStatus = isPresent(p) ? hStatus : CERR_READ_FAILURE ;

        	if (sStatus == hStatus){
            		PCARD_TUPLE_PARMS s = newCopy (p) ;

            		if (pSCard) 
            			pSCard->CardGetThisTuple (s) ;
            		usTupleCode = s->uTupleCode ;
            		usTupleLink = s->uTupleLink ;
            		delete s ;
            		// Possible pass
          	}
        	else{ // The error is a status error.
            		usTupleCode  = uhTupleCode ;
            		usTupleLink  = uhTupleLink ;
            		// Certain failure
          	}
      	}
    	else{
        	// It was not found by HCard.
        	// Assume the tuple walking algorithm is correct.
        	// This algorithm is really arcane, given the
        	// contradictions in the PCMCIA books, and the
        	// inconsistencies in the real code.
       	sStatus      = hStatus ;
        	usTupleCode  = uhTupleCode ;
        	usTupleLink  = uhTupleLink ;
        	// Possible pass
      	}

    	// This stuff does not exist.
    	usDataLen    = uhDataLen = 0 ;
    	fDataMatches = 1 ;
    	unExtraData  = 0 ;

    	return hStatus ;
}

STATUS MatchedCard::CardGetTupleData (PCARD_DATA_PARMS p, UINT32 size){
    	// Get ready to detect overwrites.
   	memset ((PUCHAR)p + sizeof(CARD_DATA_PARMS), 0, size - sizeof(CARD_DATA_PARMS)) ;
    	PCARD_DATA_PARMS s = newCopy (p, size) ;

    	// This succeeds because guaranteePresence () was cleverly called using a CARD_TUPLE_PARMS version of p.
    	syncronizeSoftCard (p) ;

    	// Get SCard result: assume p->uBufLen and p->uTupleOffset are correctly set.
    	sStatus = pSCard->CardGetTupleData(s) ;

    	// Get HardCard result.
    	hStatus = pHCard->CardGetTupleData (p) ;
	NKDbgPrintfW(_T("sStatus = %d; hStatus=%d"), sStatus, hStatus);

    	// In case of CERR_BAD_ARG_LENGTH in both cases, we pass
    	if ((sStatus == CERR_BAD_ARG_LENGTH) && (hStatus == CERR_BAD_ARG_LENGTH)){
        	usTupleCode = uhTupleCode = 0 ;
        	usTupleLink = uhTupleLink = 0 ;
        	usDataLen   = uhDataLen   = 0 ;
        	fDataMatches = 1 ; unExtraData = 0 ;
        	delete s ;
        	return hStatus ;
      }

    	usDataLen = s->uDataLen ;
    	uhDataLen = p->uDataLen ;

	NKDbgPrintfW(_T("sDatalen=%d, hDataLen=%d"), usDataLen, uhDataLen);
    	// See if data matches.
    	if (uhDataLen == usDataLen){
        	UINT start = sizeof (CARD_DATA_PARMS) ;
        	PUCHAR x = (PUCHAR)s + start, y = (PUCHAR)p + start ;
        	fDataMatches = !memcmp (x, y, uhDataLen) ;
      }
    	else 
    		fDataMatches = 0 ; // Not used if the data lengths are different,

    	// s is longer needed.
    	delete s ;

    	// Look for extra data.
    	if (fDataMatches){
        	unExtraData = 0 ;
        	UINT16 uStartFrom = sizeof(CARD_DATA_PARMS) + p->uDataLen ;
        	PUCHAR pBuf = (PUCHAR) p ;
        	for (UINT16 i = uStartFrom ; i < size ; i++) 
        		if (pBuf[i]) 
        			unExtraData++ ;
      }
    	else 
    		unExtraData = 0 ; // A useless but assigned value.

    	// These are not available.
    	usTupleCode = uhTupleCode = 0 ;
    	usTupleLink = uhTupleLink = 0 ;

    	return hStatus ;
}

STATUS MatchedCard::CardGetParsedTuple (CARD_SOCKET_HANDLE hSocket,
                                        UINT8 uDesiredTuple,
                                        PVOID pBuf,
                                        PUINT pnItems,
                                        UINT32 actualBufSize)
{
   	// Get ready to detect overwrites - pBuf != 0 of course.
    	memset (pBuf, 0, actualBufSize) ;

    	// Set a soft card buffer.
    	PVOID s = new UCHAR [actualBufSize] ;
    	if (!s) return CERR_OUT_OF_RESOURCE ;
    	memset (s, 0, actualBufSize) ;

     	// Get SoftCard result.
    	UINT nsCount = *pnItems ;
    	sStatus = pSCard->CardGetParsedTuple (hSocket, uDesiredTuple, s, &nsCount) ;
    	usDataLen = (sStatus == CERR_SUCCESS) ? (UINT16)nsCount : 0 ;

    	// Get HardCard result.
    	UINT nhCount = *pnItems ;
    	hStatus = pHCard->CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, &nhCount) ;
    	uhDataLen = (hStatus == CERR_SUCCESS) ? (UINT16)nhCount : 0 ;

    	UINT nBufSize ;
    	if (uhDataLen == usDataLen){
        	switch (uDesiredTuple){
            		case CISTPL_CONFIG:
              	nBufSize = usDataLen * sizeof (PARSED_CONFIG) ;
              	break ;
	            case CISTPL_CFTABLE_ENTRY:
	              	nBufSize = usDataLen * sizeof (PARSED_CFTABLE) ;
	              	break ;
	            default: // This of course never happens.
	              	nBufSize = 0 ;
	              	break ;
          	}
        	if (nBufSize) 
        		fDataMatches = !memcmp (s, pBuf, nBufSize) ;
        	else 
        		fDataMatches = 1 ;
      }
    	else {
    		nBufSize = 0  ; 
    		fDataMatches = 0 ;
    	} // Not used if the data lengths are different.

    	// s is no longer needed.
    	delete[] s ;

    	// Look for extra data.
    	if (fDataMatches){
        	unExtraData = 0 ;       // Look for extraneous extra bytes in pBuf.
        	PUCHAR pRtn = (PUCHAR) pBuf ;
        	for (UINT i = nBufSize ; i < actualBufSize ; i++) 
        		if (pRtn[i]) 
        			unExtraData++ ;
      	}
    	else 
    		unExtraData = 0 ; // A useless value since data does not match.

    	// These are not available, so they are not in error.
    	usTupleCode = uhTupleCode = 0 ;
    	usTupleLink = uhTupleLink = 0 ;

    	return hStatus ;
}

UINT8 MatchedCard::getTupleType (PCARD_TUPLE_PARMS p)
  {return pSCard->getTupleType (p) ;}

UINT8 MatchedCard::getTupleType (PCARD_DATA_PARMS p)
  {return getTupleType ((PCARD_TUPLE_PARMS) p) ;}

UINT MatchedCard::getMaxParsedItems (CARD_SOCKET_HANDLE hSocket, UINT8 uDesiredTuple){
    	UINT nItems = 6000 ; // GLITCH
    	pHCard->CardGetParsedTuple (hSocket, uDesiredTuple, 0, &nItems) ;
    	return nItems ;
}

int MatchedCard::syncronizeSoftCard (PCARD_TUPLE_PARMS p)
  {return syncronizeSoftCard ((PCARD_DATA_PARMS)p) ;}

int MatchedCard::syncronizeSoftCard (PCARD_DATA_PARMS p)
  {return pSCard->goToId (p) ;}

int MatchedCard::isPresent  (PCARD_TUPLE_PARMS p)
  {return pSCard->isPresent(p) ;}

VOID MatchedCard::guaranteePresence (PCARD_TUPLE_PARMS p)
  {if (!isPresent(p)) pSCard->addTuple (p, pHCard) ;}

VOID MatchedCard::dump () {if (pSCard) pSCard->dump () ;}

// SCARDTPL.CPP



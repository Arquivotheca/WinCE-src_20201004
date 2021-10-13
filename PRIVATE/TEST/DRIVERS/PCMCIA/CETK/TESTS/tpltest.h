//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _TPLTEST_H_
#define _TPLTEST_H_

#include "scardtpl.h"
#include "minithread.h"

//config test parameters
typedef struct {
	int	nSeed;
	int 	nTrials;
}  TPLTESTPARAMS, *LPTPLTESTPARAMS;

#define MAX_TPLCASES 3

#define MAX_PCARD_DATA_PARMS ((UINT16)1024)
#define MAX_PARSE_BUFFER ((UINT16)1024)

// Number of possible API calls in switch (below).
#define NUMBER_OF_CASES 4

#define MAX_TRY		5

#define LAST_FIRST  0
#define LAST_NEXT   1
#define LAST_DATA   2
#define LAST_PARSED 3
#define LAST_NONE   4

/*
#ifdef DEBUG
#define REPORT_GET_FIRST(x,y,z)     reportGetFirstTuple(x,y,z)
#define REPORT_GET_NEXT(x,y,z)      reportGetNextTuple(x,y,z)
#define REPORT_GET_DATA(x,y,z)      reportGetTupleData(x,y,z)
#define REPORT_GET_PARSED(x,y,z,w)  reportGetParsedTuple(x,y,z,w)
#else // !defined 
#define REPORT_GET_FIRST(x,y,z)
#define REPORT_GET_NEXT(x,y,z)
#define REPORT_GET_DATA(x,y,z)
#define REPORT_GET_PARSED(x,y,z,w)
#define REPORT_TUPLE(x,y,z,w,v)

#endif //
*/

#define REPORT_TUPLE(x,y,z,w,v)     dumpTuple(x,y,z,w,v)

#ifndef _STDEF_
#define _STDEF_	

static TPLTESTPARAMS tplarr[MAX_TPLCASES] =
{
//    nSeed    trials   
  {    0,        240},
  {    100,     200},
  {    256,     256},
};
#endif // _STDEF_

typedef STATUS (*CardCBack) (CARD_EVENT,CARD_SOCKET_HANDLE,PCARD_EVENT_PARMS) ;


class TplTest : public MiniThread {
public:
	TplTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
			MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
			dwCaseID(dwTestCase),
			dwThreadID(dwThreadNo),
			uLocalSock(uSock),
			uLocalFunc(uFunc){
		NKDMSG(TEXT("ConfigTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					dwCaseID, dwThreadID, uLocalSock, uLocalFunc);
		SetResult(TRUE);
	};

	BOOL Init();
	VOID Dump () {pMatchedCard ->dump();};
	~TplTest(){;};

private:
	virtual DWORD ThreadRun();
    	VOID ResetCardTupleParms () ;

    	VOID MakeACall () ;
    	VOID DumpCurrentErrors () ;

    	VOID CardGetFirstTuple  () ;
    	VOID CardGetNextTuple   () ;
    	VOID CardGetTupleData   () ;
    	VOID CardGetParsedTuple () ;
	BOOL AddiTst_GetFirstTuple();
	BOOL AddiTst_GetNextTuple();
	BOOL AddiTst_GetTupleData();
	
	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	USHORT	uLocalSock, uLocalFunc;
	int		nSeed, nTrials;
	CARD_CLIENT_HANDLE hClient ;

    	// This is how we make a union.
    	// Allocate and delete only pData.
    	PCARD_TUPLE_PARMS pTuple;
    	MatchedCard *pMatchedCard;
    	PCARD_DATA_PARMS  pData ;

    	int lastCalled;

    	// Useage data
    	STATUS  fstat ;            // Most recent return state
    	UINT16  fMask ;           // Current event mask
    	CardCBack	callback;	
    	UINT16  fLocalExpMask ;   // Expected with calls to getEventMask
    	UINT16  fGlobalExpMask ;  // Expected with calls to getEventMask
    	UINT16  fAttr ;     // 0 = global, 0xFFFF = local
};

TCHAR *findTupleNameStr (UINT32) ;
TCHAR *findTupleNameStrEx (UINT32) ;
int requiredSupport (CISStrings *, unsigned int) ;
int numberOfTupleCodes () ;
int isPossibleReturn (long n) ;
TCHAR *findMatchedStringAt (int n) ;

VOID reportGetFirstTuple (TCHAR *, PCARD_TUPLE_PARMS, STATUS) ;
VOID reportGetNextTuple  (TCHAR *, PCARD_TUPLE_PARMS, STATUS) ;
VOID reportGetTupleData  (TCHAR *, PCARD_DATA_PARMS, STATUS) ;
VOID reportGetParsedTuple (TCHAR *, UINT8, UINT32, STATUS) ;
VOID dumpTuple (UINT8 uTupleCode, UINT8 uTupleLink, UINT16 flags, UINT32 cisOffset, UINT32 nItems);

#endif


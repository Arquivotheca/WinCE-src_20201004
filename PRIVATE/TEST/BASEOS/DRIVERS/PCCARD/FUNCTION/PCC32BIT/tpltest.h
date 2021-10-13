//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	Tpltest.h

Abstract:

    Definition of tuple-related test class and other needed functions/data structures

--*/

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
			m_dwCaseID(dwTestCase),
			m_dwThreadID(dwThreadNo),
			m_uLocalSock(uSock),
			m_uLocalFunc(uFunc){
		NKDMSG(TEXT("ConfigTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					m_dwCaseID, m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(TRUE);
	};

	BOOL Init();
	VOID Dump () {m_pMatchedCard ->dump();};
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

	BOOL ReadAllTuples(PTPLCONTENT pTpls, INT *pOrder);
	BOOL GetOneTypeTuples(PTPLCONTENT pTpls, INT loc);
	VOID CleanupTupleContents(PTPLCONTENT pTplContents);
	BOOL AreTwoTupleContentsIdentical(PTPLCONTENT pTpl1, PTPLCONTENT pTpl2);
	BOOL Test_ReadAllTuples();
	BOOL Test_MT_ReadAllTuples();
	static DWORD WINAPI ReadTuplesSubThread(LPVOID dParam);
	DWORD RunReadTuplesSubThreadProc(UINT8 uSubID);
	BOOL GenRandomOrder(INT* pArr, UINT uLen);
	
	DWORD	m_dwCaseID;
	DWORD 	m_dwThreadID;
	USHORT	m_uLocalSock, m_uLocalFunc;
	int		m_nSeed, m_nTrials;

    	// This is how we make a union.
    	// Allocate and delete only pData.
    	PCARD_TUPLE_PARMS m_pTuple;
    	MatchedCard *m_pMatchedCard;
    	PCARD_DATA_PARMS  m_pData ;
	PTPLCONTENT		m_pTplContents;

    	int m_lastCalled;

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


typedef struct _THREAD_PARMS{
	TplTest*	 pTplTest;
	UINT8	uSubID;
}THREAD_PARMS, *PTHREAD_PARMS;


#endif


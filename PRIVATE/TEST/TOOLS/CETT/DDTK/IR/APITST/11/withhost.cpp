//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock.h>
#include <oscfg.h>
#include <katoex.h>
#include <tux.h>
#undef AF_IRDA
#include <af_irda.h>
#include <irsrv.h>
#include <msg.h>
#include <irsup.h>
#include <request.h>
#include "shellproc.h"
#include "withhost.h"

void
DumpHex(
	BYTE *pHex,
	int   length)
{
	while (length--)
		OutStr(TEXT(" %02X"), *pHex++);
}

/*-----------------------------------------------------------------------------
  Function		:	CompareSetValueAgainstQuery
  Description	:	compares the results of an IAS_QUERY against the value that
					was set.
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

static BOOL
CompareSetValueAgainstQuery(
	IAS_SET	*pSet,
	IAS_QUERY *pQuery)
{
	BOOL passed = FALSE;

	if (pQuery->irdaAttribType != pSet->irdaAttribType)
		OutStr(TEXT("FAILED: Query attribType %d should be %d\n"), pQuery->irdaAttribType, pSet->irdaAttribType);
	else switch (pQuery->irdaAttribType)
	{
	case IAS_ATTRIB_NO_ATTRIB:
		break;

	case IAS_ATTRIB_INT:
		if (pQuery->irdaAttribute.irdaAttribInt == pSet->irdaAttribute.irdaAttribInt)
			passed = TRUE;
		else
			OutStr(TEXT("FAILED: Query INT %d should be %d\n"), 
				pQuery->irdaAttribute.irdaAttribInt, 
				pSet->irdaAttribute.irdaAttribInt);
		break;

	case IAS_ATTRIB_OCTETSEQ:
		if (pQuery->irdaAttribute.irdaAttribOctetSeq.Len == 
			pSet->irdaAttribute.irdaAttribOctetSeq.Len)
		{
			if (memcmp(&pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0],
					   &pQuery->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0],
						pSet->irdaAttribute.irdaAttribOctetSeq.Len) == 0)
			{
				passed = TRUE;
			}
			else
			{
				OutStr(TEXT("FAILED: Query OCTETSEQ length %d==%d differs\n"), pQuery->irdaAttribute.irdaAttribOctetSeq.Len, pSet->irdaAttribute.irdaAttribOctetSeq.Len);
				DumpHex(&pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0], pSet->irdaAttribute.irdaAttribOctetSeq.Len);
				DumpHex(&pQuery->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0], pSet->irdaAttribute.irdaAttribOctetSeq.Len);

			}
		}
		else
			OutStr(TEXT("FAILED: Query OCTETSEQ len %d should be %d\n"), 
				pQuery->irdaAttribute.irdaAttribOctetSeq.Len, 
				pSet->irdaAttribute.irdaAttribOctetSeq.Len);

		break;

	case IAS_ATTRIB_STR:
		if (pQuery->irdaAttribute.irdaAttribUsrStr.Len == 
			pSet->irdaAttribute.irdaAttribUsrStr.Len)
		{
			if (pQuery->irdaAttribute.irdaAttribUsrStr.CharSet == 
				pSet->irdaAttribute.irdaAttribUsrStr.CharSet)
			{
				if (memcmp(&pSet->irdaAttribute.irdaAttribUsrStr.UsrStr[0],
						   &pQuery->irdaAttribute.irdaAttribUsrStr.UsrStr[0],
						   pSet->irdaAttribute.irdaAttribUsrStr.Len) == 0)
				{
					passed = TRUE;
				}
				else
					OutStr(TEXT("FAILED: Query USRSTR differs\n"));
			}
			else
				OutStr(TEXT("FAILED: Query USRSTR CharSet %d should be %d\n"),
					pQuery->irdaAttribute.irdaAttribUsrStr.CharSet,
					pSet->irdaAttribute.irdaAttribUsrStr.CharSet);

		}
		else
			OutStr(TEXT("FAILED: Query USRSTR len %d should be %d\n"), 
				pQuery->irdaAttribute.irdaAttribUsrStr.Len, 
				pSet->irdaAttribute.irdaAttribUsrStr.Len);

		break;

	default:
		ASSERT(FALSE);
		break;
	}
	return passed;
}

/*-----------------------------------------------------------------------------
  Function		:	LocalSetAndRemoteCheck
  Description	:	Do an IRLMP_IAS_SET and then, since it is not possible (nor
					necessarily desireable) to check the value locally, have a
					remote server check that the value has been set correctly.
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

static int
LocalSetAndRemoteCheck(
	Connection	*pConn,
	SOCKET		 sockSet,
	IAS_SET		*pValue,
	int			 valueSize)
{
	BYTE	  queryBuffer[sizeof(IAS_QUERY) + 1024];
	IAS_QUERY *pQuery = (IAS_QUERY *)&queryBuffer[0];
	int		  query_result;
	int		  WSAError;

	int setResult = setsockopt(sockSet, SOL_IRLMP, IRLMP_IAS_SET, (char *)pValue, valueSize);
	if (setResult != 0)
	{
		OutStr(TEXT("SKIPPED: IRLMP_IAS_SET gave error %d\n"), WSAGetLastError());
		return TPR_SKIP;
	}
	else
	{
		if (SendQueryRequest(pConn, pValue, pQuery, &query_result, &WSAError) == FALSE)
		{
			OutStr(TEXT("SKIPPED: SendQueryRequest failed\n"));
			return TPR_SKIP;
		}
		else
		{
			if (query_result != 0)
			{
				OutStr(TEXT("FAILED: Remote Query result = %d WSAError = %d\n"), query_result, WSAError);
				return TPR_FAIL;
			}
			else
			{
				if (CompareSetValueAgainstQuery(pValue, pQuery))
					return TPR_PASS;
				else
					return TPR_FAIL;
			}
		}
	}
}

/*-----------------------------------------------------------------------------
  Function		:	RemoteSetAndLocalCheck
  Description	:	Have the peer do an IRLMP_IAS_SET and then do an IRLMP_IAS_QUERY
					to verify that that the query operation works.
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

static int
RemoteSetAndLocalCheck(
	Connection	*pConn,
	SOCKET		 sockQuery,
	IAS_SET		*pSet,
	int			 valueSize)
{
	BYTE	  queryBuffer[sizeof(IAS_QUERY) + 1024];
	IAS_QUERY *pQuery = (IAS_QUERY *)&queryBuffer[0];
	int		  setResult, queryResult;
	int		  WSAError;

	if (SendSetRequest(pConn, pSet, &setResult, &WSAError) == FALSE)
	{
		OutStr(TEXT("SKIPPED: SendSetRequest failed\n"));
		return TPR_SKIP;
	}
	else if (setResult != 0)
	{
		OutStr(TEXT("SKIPPED: Remote IRLMP_IAS_SET failed, result=%d WSAError=%d\n"), setResult, WSAError);
		return TPR_SKIP;	
	}
	else
	{
		if (IRFindNewestDevice(&pQuery->irdaDeviceID[0], 1) != 0)
		{
			OutStr(TEXT("SKIPPED: FindDevice failed\n"));
			return TPR_SKIP;	
		}
		else
		{
			memcpy(&pQuery->irdaClassName[0], &pSet->irdaClassName[0], 61);
			memcpy(&pQuery->irdaAttribName[0], &pSet->irdaAttribName[0], 61);
			IRIasQuery(sockQuery, pQuery, &queryResult, &WSAError);
			if (queryResult != 0)
			{
				OutStr(TEXT("FAILED: IRLMP_IAS_QUERY failed, result=%d WSAError=%d\n"), queryResult, WSAError);
				return TPR_FAIL;
			}
			else
			{
				if (CompareSetValueAgainstQuery(pSet, pQuery))
					return TPR_PASS;
				else
					return TPR_FAIL;
			}
		}
	}
}


/*-----------------------------------------------------------------------------
  Function		:	SetAndCheckValues
  Description	:	This function encapsulates the code common to setting an
					attribute value and testing it, whether the set is local 
					and the query remote or vice versa.
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

int
SetAndCheckValues(
	char  *szClassName,
	char  *szAttribName,
	int    AttribType,
	int   *pValues,
	int    numValues,
	int  (*pSetAndCheck)(Connection *, SOCKET, IAS_SET *, int))
{
	Connection  connServer;
	int			result, length;
	BYTE		valueBuffer[sizeof(IAS_SET) + 1024];
	IAS_SET		*pValue = (IAS_SET *)&valueBuffer[0];
	int			i, j, size;
	SOCKET		s;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return TPR_SKIP;

	s = socket(AF_IRDA, SOCK_STREAM, 0);
	if (s != INVALID_SOCKET)
	{
		strncpy(&pValue->irdaClassName[0],  szClassName, sizeof(pValue->irdaClassName));

		// Iterate to test various values

		result = TPR_PASS;
		for (i = 0; i < numValues && result == TPR_PASS; i++)
		{
			// Must give each value a unique name because the current
			// IR implementation does not allow values to be changed once
			// set, and they can only be deleted by closing the socket.

			if (szAttribName != NULL)
				strncpy(&pValue->irdaAttribName[0], szAttribName, sizeof(pValue->irdaAttribName));
			else
				sprintf(&pValue->irdaAttribName[0], "%d", i);


			// Create the value to be set

			pValue->irdaAttribType = AttribType;
			switch(AttribType)
			{
			case IAS_ATTRIB_INT:
				pValue->irdaAttribute.irdaAttribInt = pValues[i];
				size = sizeof(IAS_SET);
				break;

			case IAS_ATTRIB_OCTETSEQ:
				length = pValues[i];
				pValue->irdaAttribute.irdaAttribOctetSeq.Len = length;
				for (j = 0; j < length; j++)
					pValue->irdaAttribute.irdaAttribOctetSeq.OctetSeq[j] = (BYTE)('0'+j);
				size = sizeof(IAS_SET) + length;
				break;

			case IAS_ATTRIB_STR:
				length = pValues[i];
				pValue->irdaAttribute.irdaAttribUsrStr.Len = length;
				pValue->irdaAttribute.irdaAttribUsrStr.CharSet = length;
				for (j = 0; j < length; j++)
					pValue->irdaAttribute.irdaAttribUsrStr.UsrStr[j] = (BYTE)('0'+j);
				size = sizeof(IAS_SET) + length;
				break;

			default:
				ASSERT(FALSE);
				break;
			}

			// Test Execution - Set the attribute and confirm

			result = (*pSetAndCheck)(&connServer, s, pValue, size);
		}
		closesocket(s);
	}

	ConnectionClose(&connServer);

	return result;
}

//------------------------- Tux Testing Functions ----------------------------- 


static int ValidIntValues[] =
{
	0,
	1,
	-1,
	127,
	128,
	255,
	256,
	65535,
	65536,
	INT_MAX,
	INT_MIN
};

TEST_FUNCTION(ConfirmSetIASValidInt)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_INT,
				&ValidIntValues[0],
				countof(ValidIntValues),
				LocalSetAndRemoteCheck);
}

TEST_FUNCTION(ConfirmQueryIASValidInt)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_INT,
				&ValidIntValues[0],
				countof(ValidIntValues),
				RemoteSetAndLocalCheck);
}

int ValidClassNameLengths[] =
{
	1,
	60,
	// A length of 61 should work since the IAS_SET should be truncated by the OS
	// to 60 chars, and the request that is sent to the remote is also truncated
	// to 60 chars.
	61
};

//
//	Try various Class Name lengths and ensure that they work.
//
TEST_FUNCTION(ConfirmQueryClassNameLen)
{
	int result = TPR_PASS;
	char className[62];

	TEST_ENTRY;

	for (int i = 0; i < countof(ValidClassNameLengths) && result == TPR_PASS; i++)
	{
		GenerateName(&className[0], ValidClassNameLengths[i]);
		result = SetAndCheckValues(
					&className[0],
					NULL,
					IAS_ATTRIB_INT,
					&ValidIntValues[0],
					1,
					LocalSetAndRemoteCheck);
	}
	return result;
}

int ValidAttribNameLengths[] =
{
	0,
	1,
	60,
	// A length of 61 should work since the IAS_SET should be truncated by the OS
	// to 60 chars, and the request that is sent to the remote is also truncated
	// to 60 chars.
	61
};

//
//	Try various Attribute Name lengths and ensure that they work.
//
TEST_FUNCTION(ConfirmQueryAttribNameLen)
{
	int result = TPR_PASS;
	char attribName[62];

	TEST_ENTRY;

	for (int i = 0; i < countof(ValidAttribNameLengths) && result == TPR_PASS; i++)
	{
		GenerateName(&attribName[0], ValidAttribNameLengths[i]);
		result = SetAndCheckValues(
					"test",
					&attribName[0],
					IAS_ATTRIB_INT,
					&ValidIntValues[0],
					1,
					LocalSetAndRemoteCheck);
	}
	return result;
}

#define MIN_OCTETSEQ_LENGTH 0
#define MAX_OCTETSEQ_LENGTH 1024

int ValidOctSeqLengths[] =
{
// There 
	
//	MIN_OCTETSEQ_LENGTH,
	1,
	10,
	127,
	128,
	255,
	256,
	512,
	1000,
	MAX_OCTETSEQ_LENGTH
};

TEST_FUNCTION(ConfirmSetIASValidOctetSeq)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_OCTETSEQ,
				&ValidOctSeqLengths[0],
				countof(ValidOctSeqLengths),
				LocalSetAndRemoteCheck);
}

TEST_FUNCTION(ConfirmQueryIASValidOctetSeq)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_OCTETSEQ,
				&ValidOctSeqLengths[0],
				countof(ValidOctSeqLengths),
				RemoteSetAndLocalCheck);
}

#define MIN_USRSTR_LENGTH 0
#define MAX_USRSTR_LENGTH 255

static int ValidUsrStrLengths[] =
{
	MIN_USRSTR_LENGTH,
	1,
	10,
	127,
	128,
	MAX_USRSTR_LENGTH
};

TEST_FUNCTION(ConfirmSetIASValidUsrStr)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_STR,
				&ValidUsrStrLengths[0],
				countof(ValidUsrStrLengths),
				LocalSetAndRemoteCheck);
}

TEST_FUNCTION(ConfirmQueryIASValidUsrStr)
{
	TEST_ENTRY;

	return SetAndCheckValues(
				"test",
				NULL,
				IAS_ATTRIB_STR,
				&ValidUsrStrLengths[0],
				countof(ValidUsrStrLengths),
				RemoteSetAndLocalCheck);
}


//
//	Confirm that an IAS object is deleted when its socket is closed.
//

TEST_FUNCTION(ConfirmDeleteIASAttribute)
{
	Connection  connServer;
	SOCKET		s;
	int			result = TPR_SKIP, WSAError, query_result;
	IAS_SET		value;
	IAS_SET		*pValue = &value;
	BYTE	     queryBuffer[sizeof(IAS_QUERY) + 1024];
	IAS_QUERY   *pQuery = (IAS_QUERY *)&queryBuffer[0];

	TEST_ENTRY;

	if (ConnectionOpen(g_af, g_szServerName, &connServer) == FALSE)
		return TPR_SKIP;

	s = socket(AF_IRDA, SOCK_STREAM, 0);
	if (s != INVALID_SOCKET)
	{
		strcpy(&pValue->irdaClassName[0],  "test");
		strcpy(&pValue->irdaAttribName[0], "1");

		pValue->irdaAttribType = IAS_ATTRIB_INT;
		pValue->irdaAttribute.irdaAttribInt = 0;

		// Set the attribute and make sure that it appears
		result = LocalSetAndRemoteCheck(&connServer, s, pValue, sizeof(IAS_SET));

		// Delete the attribute by deleting the socket
		closesocket(s);

		if (result == TPR_PASS)
		{
			// Check that the attribute was deleted
			if (SendQueryRequest(&connServer, pValue, pQuery, &query_result, &WSAError) == FALSE)
			{
				OutStr(TEXT("SKIPPED: SendQueryRequest failed\n"));
				result = TPR_SKIP;
			}
			else
			{
				if (query_result != 0)
				{
					// WinNT returns ECONNREFUSED when it fails to find an attribute, goofy
#define WINNT_ECONNREFUSED_HACK
#ifdef WINNT_ECONNREFUSED_HACK
					if (WSAError == WSAECONNREFUSED)
						result = TPR_PASS;
					else
#endif
					{
						OutStr(TEXT("FAILED: Remote Query result = %d WSAError = %d\n"), query_result, WSAError);
						result = TPR_FAIL;
					}
				}
				else
				{
					// An attribute value of NO_ATTRIB should be returned
					if (pQuery->irdaAttribType == IAS_ATTRIB_NO_ATTRIB)
						result = TPR_PASS;
					else
					{
						OutStr(TEXT("FAILED: Attribute was not deleted\n"));
						result = TPR_FAIL;
					}
				}
			}
		}
	}
	ConnectionClose(&connServer);

	return result;
}

// END OF FILE
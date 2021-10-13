//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    scnb.cxx

Abstract:

    NetBIOS support routines

--*/
#include <windows.h>
#include <winsock.h>
#include <svsutil.hxx>
#include <scnb.hxx>

typedef struct __nb_header {
	unsigned short usNAME_TRN_ID;			// Transaction ID
	unsigned short usFLAGS;					// Flags & opcode
	unsigned short usQDCOUNT;				// Number of entries in question section
	unsigned short usANCOUNT;				// Number of entries in answers section
	unsigned short usNSCOUNT;				// Number of resources in authority section
	unsigned short usARCOUNT;				// Number of additional records
} NbHeader;

#define REVS(n)			((((n) & 0xff) << 8) | (((n) >> 8) & 0xff))
#define TOBES(n)		REVS(n)
#define TOLES(n)		REVS(n)

#define REVI(n)			((REVS((n & 0xffff)) << 16) | REVS(((n >> 16) & 0xffff)))
#define TOBEI(n)		REVI(n)
#define TOLEI(n)		REVI(n)

#define OPCODE_MASK				(0xf << 11)
#define OPCODE_REGISTRATION		(5 << 11)
#define OPCODE_QUERY			(0 << 11)
#define OPCODE_RELEASE			(6 << 11)
#define OPCODE_WACK				(7 << 11)
#define OPCODE_REFRESH			(8 << 11)

#define FLAGS_RESPONSE			(1 << 15)
#define FLAGS_B					(1 << 4)
#define FLAGS_RA				(1 << 7)
#define FLAGS_RD				(1 << 8)
#define FLAGS_TC				(1 << 9)
#define FLAGS_AA				(1 << 10)

#define FLAGS_RCODE_MASK		0xf

#define MAX_DATAGRAM		576
#define MAX_NETBIOS_NAME	16

#define TCP_ADDON	L"\\Parms\\TcpIp"
#define ARRSZ(c)	(sizeof(c)/sizeof((c)[0]))

#define DEFAULT_TTL				300000

#define IP_STRLEN	20

static unsigned long		s_ulPktId = 0;

static unsigned short GetTranID (void) {
	static unsigned long s_ulPktId = 0;

	if (! s_ulPktId)
		s_ulPktId = (unsigned long)((GetCurrentProcessId () << 8) & 0xffff);

	return (unsigned short)InterlockedIncrement ((LONG *)&s_ulPktId);
}

//
//	NETBIOS domain names are not supported. Must occupy even numer of bytes.
//
static inline void NBEncodeName(unsigned char * &pBuf, char *pszName) {
	*pBuf++ = 0x20;

	int fEndName = FALSE;

	for (int i = 0; i < 15; ++i) {		// Name
		unsigned char c;
		if (fEndName)
			c = ' ';
		else if (pszName[i] == '\0') {
			c = ' ';
			fEndName = TRUE;
		} else {
			c = (unsigned char)pszName[i];
			if ((c >= 'a') && (c <= 'z'))
				c -= 'a' - 'A';
		}

		*pBuf++ = 'A' + (c >> 4);
		*pBuf++ = 'A' + (c & 0x0f);
	}

	*pBuf++ = 'A';						// Name type (0)
	*pBuf++ = 'A';

	*pBuf++ = 0;
}

int NBStatusQuery (char *szNameBuffer, int cBufferSize, unsigned long ip) {
	if (cBufferSize < MAX_NETBIOS_NAME)
		return FALSE;

	const int iDGramSize = 50;
	unsigned char ucb_out[iDGramSize];

	NbHeader *pHDR = (NbHeader *)ucb_out;
	memset (pHDR, 0, sizeof(*pHDR));

	pHDR->usNAME_TRN_ID = TOBES(GetTranID ());
	pHDR->usFLAGS       = TOBES(OPCODE_QUERY);
	pHDR->usQDCOUNT     = TOBES(0x0001);

	ucb_out[12] = 0x20;

	ucb_out[13] = 'A' + (('*' >> 4) & 0xf);		// 'Any' name
	ucb_out[14] = 'A' +  ('*'       & 0xf);

	for (int i = 0; i < 30; ++i)
		ucb_out[15 + i] = 'A';

	ucb_out[45] = 0;

	unsigned char *pBuf = ucb_out + 46;

	*(unsigned short *)pBuf = TOBES(0x0021);
	pBuf += 2;

	*(unsigned short *)pBuf = TOBES(0x0001);
	pBuf += 2;

	SVSUTIL_ASSERT (pBuf == ucb_out + iDGramSize);

	SOCKET s = socket (PF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in sin;
	memset (&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port   = htons (137);
	sin.sin_addr.S_un.S_addr = ip;

	int iRes = FALSE;

	for (i = 0 ; i < 3 ; ++i) {
		if (sendto (s, (char *)ucb_out, iDGramSize, 0, (struct sockaddr *)&sin, sizeof(sin)) != iDGramSize)
			break;

		fd_set f;
		FD_ZERO (&f);
		FD_SET (s, &f);

		timeval tv;
		tv.tv_sec  = 5;
		tv.tv_usec = 0;

		if (select (0, &f, NULL, NULL, &tv) <= 0)
			continue;

		unsigned char ucb_in[MAX_DATAGRAM];
		struct sockaddr_in sin2;
		int s2len = sizeof(sin2);
		memset (&sin2, 0, s2len);

		int iSize = recvfrom (s, (char *)ucb_in, sizeof(ucb_in), 0, (struct sockaddr *)&sin2, &s2len);

		if ((iSize >= (iDGramSize + 7)) &&								// Enough bytes and...
			(((ucb_in[1] << 8) | ucb_in[0]) == pHDR->usNAME_TRN_ID) &&	// Our transaction and ...
			(ucb_in[2] == 0x84) &&										// Really node status response
			(ucb_in[3] == 0x00) &&										// ...
			(memcmp(ucb_in + 8, ucb_out + 8, iDGramSize - 8) == 0) &&	// ...
			(ucb_in[iDGramSize] == 0) &&								// ...
			(ucb_in[iDGramSize + 1] == 0) &&							// ...
			(ucb_in[iDGramSize + 2] == 0) &&							// ...
			(ucb_in[iDGramSize + 3] == 0)) {							// ...
			int iNames = ucb_in[iDGramSize + 6];
			//
			//	Interpret netbios names
			//
			unsigned char *sznbname = ucb_in + iDGramSize + 7;
			for (int j = 0 ; j < iNames ; ++j, sznbname += 18) {
				if ((sznbname + 17 - ucb_in > iSize) || (sznbname[0] == ' '))
					break;												// Broken packet arrived. Retry...
				if (((sznbname[15] == 0x00) ||							// General name...
					(sznbname[15] == 0x03) ||							// or ?
					(sznbname[15] == 0x20)) &&							// or file server
					(! (sznbname[16] & 0x80)) &&						// not group
					(sznbname[16] & 0x04)) {							// and active...
					memcpy (szNameBuffer, sznbname, 15);
					szNameBuffer[15] = '\0';
					char *p = szNameBuffer + 14;
					while (*p == ' ')
						*p-- = '\0';
					iRes = TRUE;
					break;
				}
			}
			break;
		}
	}

	closesocket (s);

	return iRes;
}

unsigned long NBQueryWins (char *szName, unsigned long ipWINS) {
	if (strlen (szName) > MAX_NETBIOS_NAME)
		return INADDR_NONE;

	const int iDGramSize = 50;
	unsigned char ucb_out[iDGramSize];

	NbHeader *pHDR = (NbHeader *)ucb_out;
	memset (pHDR, 0, sizeof(*pHDR));

	pHDR->usNAME_TRN_ID = TOBES(GetTranID ());
	pHDR->usFLAGS       = TOBES(OPCODE_QUERY | FLAGS_RD);
	pHDR->usQDCOUNT     = TOBES(0x0001);

	unsigned char *pBuf = ucb_out + sizeof(*pHDR);
	NBEncodeName (pBuf, szName);

	*(unsigned short *)pBuf = TOBES(0x0020);
	pBuf += 2;

	*(unsigned short *)pBuf = TOBES(0x0001);
	pBuf += 2;

	SVSUTIL_ASSERT (pBuf == ucb_out + iDGramSize);

	SOCKET s = socket (PF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in sin;
	memset (&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port   = htons (137);
	sin.sin_addr.S_un.S_addr = ipWINS;

	unsigned long ulRes = INADDR_NONE;
	int fSkipResend = FALSE;

	for (int i = 0 ; i < 3 ; ++i) {
		if (! fSkipResend) {
			if (sendto (s, (char *)ucb_out, iDGramSize, 0, (struct sockaddr *)&sin, sizeof(sin)) != iDGramSize)
				break;
		} else
			fSkipResend = FALSE;

		fd_set f;
		FD_ZERO (&f);
		FD_SET (s, &f);

		timeval tv;
		tv.tv_sec  = 5;
		tv.tv_usec = 0;

		if (select (0, &f, NULL, NULL, &tv) <= 0)
			continue;

		unsigned char ucb_in[MAX_DATAGRAM];
		struct sockaddr_in sin2;
		int s2len = sizeof(sin2);
		memset (&sin2, 0, s2len);

		int iSize = recvfrom (s, (char *)ucb_in, sizeof(ucb_in), 0, (struct sockaddr *)&sin2, &s2len);

		if (iSize >= sizeof (NbHeader)) {
			NbHeader *pR = (NbHeader *)ucb_in;
			if (pR->usNAME_TRN_ID == pHDR->usNAME_TRN_ID) {
				unsigned short usFLAGS = TOLES(pR->usFLAGS);
				if ((usFLAGS & OPCODE_MASK) == OPCODE_WACK) {
					fSkipResend = TRUE;
					--i;
				} else {
					unsigned short usRdLength = *(unsigned short *)(ucb_in + iDGramSize + 4);
					usRdLength = TOLES(usRdLength);
					if ((usFLAGS & FLAGS_RESPONSE) && ((usFLAGS & FLAGS_RCODE_MASK) == 0) &&
						((usFLAGS & OPCODE_MASK) == OPCODE_QUERY) && (usRdLength + iDGramSize + 4 + 2 == iSize) ) {
						unsigned short *p = (unsigned short *)(ucb_in + iDGramSize + 6);
						int iNum = usRdLength / 6;
						if (iNum > 0) {
							++p;
							memcpy (&ulRes, p, sizeof(ulRes));
						}
					}
					break;
				}
			}
		}
	}

	closesocket (s);

	return ulRes;
}

static void NBAddToBuffer (unsigned long *ipWinsBuffer, int &iNum, unsigned long ulWhat) {
	for (int i = 0 ; i < iNum ; ++i) {
		if (ipWinsBuffer[i] == ulWhat)
			return;
	}
	ipWinsBuffer[iNum++] = ulWhat;
}

int NBGetWins (unsigned long *ipWinsBuffer, int nBufferSize) {
	if (nBufferSize <= 0)
		return 0;

	int iRes = 0;

#if defined (UNDER_CE)
#define MAX_ADNAME		64

	HKEY hkBASE = NULL;
	if (ERROR_SUCCESS != RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"COMM", 0, KEY_READ, &hkBASE))
		return iRes;

	DWORD dwIndex = 0;

	for ( ; ; ) {
		WCHAR szName[MAX_ADNAME + ARRSZ(TCP_ADDON) + 1];
		DWORD dwNameSize = MAX_ADNAME;

		FILETIME ft;

		LONG hr = RegEnumKeyEx(hkBASE, dwIndex, szName, &dwNameSize, NULL, NULL, NULL, &ft);
		if (hr == ERROR_SUCCESS) {
			wcscat (szName, TCP_ADDON);

			HKEY hkIP = NULL;
			if (ERROR_SUCCESS == RegOpenKeyEx (hkBASE, szName, 0, KEY_READ, &hkIP)) {

				WCHAR szBigBuffer[_MAX_PATH * 2];

				DWORD dwType;
				DWORD dwSize = sizeof(szBigBuffer);
				if ((ERROR_SUCCESS == RegQueryValueEx (hkIP, L"DhcpWINS", NULL, &dwType, (LPBYTE) szBigBuffer, &dwSize)) &&
					(dwType == REG_MULTI_SZ) && (dwSize < sizeof(szBigBuffer))){
					WCHAR *p = szBigBuffer;
					while (*p) {
						char szaddr[IP_STRLEN];
						if (WideCharToMultiByte (CP_ACP, 0, p, -1, szaddr, sizeof(szaddr), NULL, NULL) > 0) {
							int ip = inet_addr (szaddr);
							if ((ip != INADDR_NONE) && (ip != INADDR_ANY) && (iRes < nBufferSize))
								NBAddToBuffer (ipWinsBuffer, iRes, ip);
						}
						p += wcslen(p) + 1;
					}
				}

				dwSize = sizeof(szBigBuffer);
				if ((ERROR_SUCCESS == RegQueryValueEx (hkIP, L"WINS", NULL, &dwType, (LPBYTE) szBigBuffer, &dwSize)) &&
					(dwType == REG_MULTI_SZ) && (dwSize < sizeof(szBigBuffer))){
					WCHAR *p = szBigBuffer;
					while (*p) {
						char szaddr[IP_STRLEN];
						if (WideCharToMultiByte (CP_ACP, 0, p, -1, szaddr, sizeof(szaddr), NULL, NULL) > 0) {
							int ip = inet_addr (szaddr);
							if ((ip != INADDR_NONE) && (ip != INADDR_ANY) && (iRes < nBufferSize))
								NBAddToBuffer (ipWinsBuffer, iRes, ip);
						}
						p += wcslen(p) + 1;
					}
				}

				RegCloseKey (hkIP);
			}
		}

		if (hr == ERROR_NO_MORE_ITEMS)
			break;

		++dwIndex;
	}

	RegCloseKey (hkBASE);

#endif

	return iRes;
}

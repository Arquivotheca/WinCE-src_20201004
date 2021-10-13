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

    SrmpParse.cxx

Abstract:

    Contains basic parsing related functions.


--*/



#include <windows.h>
#include <msxml2.h>
#include <svsutil.hxx>

#include "sc.hxx"
#include "scdefs.hxx"
#include "SrmpAccept.hxx"
#include "SrmpParse.hxx"
#include "SrmpDefs.hxx"
#include "scutil.hxx"

const WCHAR cszMSMQPrefix[]    = L"MSMQ:";
const DWORD ccMSMQPrefix       = SVSUTIL_CONSTSTRLEN(cszMSMQPrefix);

BOOL IsUriHttpOrHttps(const WCHAR *szQueue) {
	return IsUriHttp(szQueue) || IsUriHttps(szQueue);
}

BOOL IsUriMSMQ(const WCHAR *szQueue) {
	return (0 == _wcsnicmp(szQueue,cszMSMQPrefix,ccMSMQPrefix));
}

BOOL UriToQueueFormat(const WCHAR *szQueue, DWORD dwQueueChars, QUEUE_FORMAT *pQueue, WCHAR **ppszQueueBuffer) {
	BOOL fMSMQ = FALSE;
	WCHAR *szFormatName;
	BOOL  fConverted = FALSE;

	gMem->Lock();

	// 1st try to route potential free-form URI to something MSMQ can understand.
	if (gMachine->RouteLocal((WCHAR*)szQueue,&szFormatName))
		fConverted = TRUE;
	else
		szFormatName = (WCHAR*)szQueue;

	gMem->Unlock();

	if (IsUriMSMQ(szQueue)) {
		szFormatName += ccMSMQPrefix; // increment past "MSMQ:"
		fMSMQ = TRUE;
	}
	else if (!IsUriHttpOrHttps(szQueue) && !fConverted)
		return FALSE;

	if (*ppszQueueBuffer) {
		g_funcFree(*ppszQueueBuffer,g_pvFreeData);
		*ppszQueueBuffer = NULL;
	}

	// skip past "MSMQ:" prefix if it's present.
	if (NULL == (*ppszQueueBuffer = svsutil_wcsdup(szFormatName)))
		return FALSE;

	if (fMSMQ || fConverted) {
		if (! scutil_StringToQF(pQueue,(WCHAR*)(*ppszQueueBuffer)))
			return FALSE;
	}
	else {
		pQueue->DirectID(*ppszQueueBuffer);
	}
	return TRUE; 
}

LARGE_INTEGER SecondsToStartOf1970 = {0xb6109100, 0x00000002};

// WinCE doesn't have gmtime, so we have to get fake it with a SYSTEMTIME and this fcn.
VOID SecondsSince1970ToSystemTime(ULONG ElapsedSeconds, SYSTEMTIME *pSystemTime) {
	LARGE_INTEGER Seconds;

	//  Move elapsed seconds to a large integer
	Seconds.LowPart = ElapsedSeconds;
	Seconds.HighPart = 0;

	//  Convert number of seconds from 1970 to number of seconds from 1601
	Seconds.QuadPart = Seconds.QuadPart + SecondsToStartOf1970.QuadPart;

	//  Convert seconds to 100ns resolution
	Seconds.QuadPart = Seconds.QuadPart * (10*1000*1000);

	FileTimeToSystemTime((FILETIME*)&Seconds,pSystemTime);
}

// converts time in time_t format into a string of ISO860 format.
BOOL UtlTimeToIso860Time(time_t tm, WCHAR *szBuffer) {
	SYSTEMTIME st;
	SecondsSince1970ToSystemTime(tm,&st);

	wsprintf(szBuffer,L"%04d%02d%02dT%02d%02d%02d",st.wYear,st.wMonth,
	                  st.wDay,st.wHour,st.wMinute,st.wSecond);
	return TRUE;
}

// convert Iso860 absolute time format to system time format
BOOL UtlIso8601TimeToSystemTime(const WCHAR *szIso860Time, DWORD cbIso860Time, SYSTEMTIME* pSysTime)  {
	const WCHAR *szTrav = szIso860Time;
	DWORD year;
	DWORD month;
	DWORD day;
	DWORD hour;

	if (cbIso860Time < 10)
	    return FALSE;

	memset(pSysTime, 0, sizeof(SYSTEMTIME));
	if (4 != swscanf(szTrav, L"%04d%02d%02dT%02d", &year, &month, &day, &hour))
		return FALSE;

	if ((month > 12) || (day > 31) ||(hour > 24))
		return FALSE;

	pSysTime->wYear = (WORD)year;
	pSysTime->wMonth = (WORD)month;
	pSysTime->wDay = (WORD)day;
	pSysTime->wHour = (WORD)hour;

	szTrav += 11;
	DWORD remaningLength = cbIso860Time - 11;  

	if (remaningLength == 0)
		return TRUE;

	if (remaningLength < 2)
		return FALSE;

	if (1 != swscanf(szTrav, L"%02d", &pSysTime->wMinute))
		return FALSE;

	if (pSysTime->wMinute  > 59)
		return FALSE;

	szTrav += 2;
	remaningLength -= 2;

	if (remaningLength == 0)
		return TRUE;

	if (remaningLength != 2)
		return FALSE;

	if (1 != swscanf(szTrav, L"%02d", &pSysTime->wSecond))
		return FALSE;

	if (pSysTime->wSecond > 59)
		return FALSE;

	return TRUE;
}

// 	convert  system time to c runtime time integer 
BOOL UtlSystemTimeToCrtTime(const SYSTEMTIME *pSysTime, time_t *pTime) {
	FILETIME FileTime;
	if (!SystemTimeToFileTime(pSysTime, &FileTime))
		return FALSE;

	// SystemTimeToFileTime() returns the system time in number 
	// of 100-nanosecond intervals since January 1, 1601. We
	// should return the number of seconds since January 1, 1970.
	// So we should subtract the number of 100-nanosecond intervals
	// since January 1, 1601 up until January 1, 1970, then divide
	// the result by 10**7.
	LARGE_INTEGER* pliFileTime = (LARGE_INTEGER*)&FileTime;
	pliFileTime->QuadPart -= 0x019db1ded53e8000;
	pliFileTime->QuadPart /= 10000000;

	SVSUTIL_ASSERT(FileTime.dwHighDateTime == 0);

	*pTime = min(FileTime.dwLowDateTime, LONG_MAX);
	return TRUE;
}

// 	convert relative time duration string (Iso8601 5.5.3.2) to integer
BOOL UtlIso8601TimeDuration(const WCHAR *szTimeDuration, DWORD cbTimeDuration, time_t *pTime) {
	const WCHAR szTimeDurationPrefix[] = L"P";
	const DWORD cbTimeDurationPrefix = SVSUTIL_CONSTSTRLEN(szTimeDurationPrefix);
	LPCWSTR pTrav = szTimeDuration + cbTimeDurationPrefix;
	LPCWSTR pEnd  = szTimeDuration + cbTimeDuration;

	if (cbTimeDuration <= cbTimeDurationPrefix)
		return FALSE;

	if (0 != wcsncmp(szTimeDuration,szTimeDurationPrefix,cbTimeDurationPrefix))
		return FALSE;

	DWORD years = 0;
	DWORD months = 0;
	DWORD hours = 0;
	DWORD days = 0;
	DWORD minutes = 0;
	DWORD seconds = 0;
	bool fTime = false;
	DWORD temp = 0;

	while(pTrav++ != pEnd)  {
		if (iswdigit(*pTrav)) {
		    temp = temp*10 + (*pTrav -L'0');
		    continue;
		}

		switch(*pTrav)  {
			case L'Y':
			case L'y':
				years = temp;
				break;

			case L'M':
			case L'm':
				if (fTime)
					minutes = temp;
				else
					months = temp;
				break;

			case L'D':
			case L'd':
				days = temp;
				break;

			case L'H':
			case L'h':
				hours = temp;
				break;

			case L'S':
			case L's':
				seconds = temp;
				break;

			case L'T':
			case L't':
				fTime = true;
				break;

			default:
				return FALSE;
				break;
			}

			temp = 0;
	}

	months += (years * 12);
	days += (months * 30);
	hours += (days * 24);
	minutes += (hours * 60);
	seconds += (minutes * 60);

	*pTime = min(seconds ,LONG_MAX);
	return TRUE;
}

const BYTE bAscii2Base64[128] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255,  62, 255, 255, 255,  63,  52,  53,
	54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
	255,   0, 255, 255, 255,   0,   1,   2,   3,   4,
	5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
	15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
	25, 255, 255, 255, 255, 255, 255,  26,  27,  28,
	29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
	39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	49,  50,  51, 255, 255, 255, 255, 255
};

inline BYTE GetBase64Value(WCHAR Base64CharW, BYTE *pByte) {
	if((Base64CharW >= 128) || (bAscii2Base64[Base64CharW] == 255))
		return FALSE;

	*pByte = bAscii2Base64[Base64CharW];
	return TRUE;
}

const int xFirst6bChar = 0x00fc0000;
const int xSecond6bChar = 0x0003f000;
const int xThird6bChar = 0x00000fc0;
const int xFourth6bChar = 0x0000003f;

const int xFirst6bShift = 18;
const int xSecond6bShift = 12;
const int xThird6bShift = 6;
const int xFourth6bShift = 0;

//
// Mask bits in 24 bit value for the three 8 bit units
// note the first char is the most significant byte !!
//
const int xFirst8bChar = 0x00ff0000;
const int xSecond8bChar = 0x0000ff00;
const int xThird8bChar = 0x000000ff;

const int xFirst8bShift = 16;
const int xSecond8bShift = 8;
const int xThird8bShift = 0;


// Transform wchar_t base64 string to Octet string
BYTE* Base642OctetW(LPCWSTR szBase64, DWORD cbBase64, DWORD *pdwOctLen) {
	DWORD Complete4Chars   = cbBase64 / 4;
	BYTE* OctetBuffer      = NULL;
	BOOL  fRet             = FALSE;
	BYTE* pOctetBuffer     = NULL;
	DWORD i;

	if((Complete4Chars * 4) != cbBase64)
		goto done;

	*pdwOctLen = Complete4Chars * 3;
	if (NULL == (OctetBuffer = (BYTE*)g_funcAlloc(*pdwOctLen,g_pvAllocData)))
		goto done;

	if(szBase64[cbBase64 - 2] == L'=') {
		// '==' padding --> only 1 of the last 3 in the char is used
		if (szBase64[cbBase64 - 1] != L'=')
			goto done;
		*pdwOctLen -= 2;
	}
	else if(szBase64[cbBase64 - 1] == L'=') {
		// '=' padding --> only 2 of the last 3 char are used
		*pdwOctLen -= 1;
	}
	pOctetBuffer = OctetBuffer;

	// Convert each 4 wchar_t of base64 (6 bits) to 3 Octet bytes of 8 bits
	// note '=' is mapped to 0 so no need to worry about last 4 base64 block
	// they might be extra Octet byte created in the last block
	// but we will ignore them because the correct value of pdwOctLen
	for(i=0; i< Complete4Chars; ++i, szBase64 += 4)  {
		// Calc 24 bits value - from 4 bytes of 6 bits
		BYTE b1;
		BYTE b2;
		BYTE b3;
		BYTE b4;
		if (!GetBase64Value(szBase64[0],&b1) || !GetBase64Value(szBase64[1],&b2) ||
		    !GetBase64Value(szBase64[2],&b3) || !GetBase64Value(szBase64[3],&b4))
			goto done;
		    
		DWORD Temp =   (b1 << xFirst6bShift) 
				     + (b2 << xSecond6bShift) 
				     + (b3 << xThird6bShift) 
				     + (b4 << xFourth6bShift);

		// Calc first 8 bits
		DWORD Res = ((Temp & xFirst8bChar) >> xFirst8bShift); 
		if (Res >= 256)
			goto done;
		*pOctetBuffer = static_cast<unsigned char>(Res);
		++pOctetBuffer;

		// Calc second 8 bits
		Res = ((Temp & xSecond8bChar) >> xSecond8bShift); 
		if (Res >= 256)
			goto done;
		*pOctetBuffer = static_cast<unsigned char>(Res);
		++pOctetBuffer;

		// Calc third 8 bits
		Res = ((Temp & xThird8bChar) >> xThird8bShift); 
		if (Res >= 256)
			goto done;
		*pOctetBuffer = static_cast<unsigned char>(Res); 
		++pOctetBuffer;
	}

	fRet = TRUE;
done:
	if (!fRet && OctetBuffer) {
		g_funcFree(OctetBuffer,g_pvFreeData);
		OctetBuffer = NULL;
	}

	return OctetBuffer;
}


const WCHAR xBase642AsciiW[] = {
	L'A', L'B', L'C', L'D', L'E', L'F', L'G', L'H', L'I', L'J',
	L'K', L'L', L'M', L'N', L'O', L'P', L'Q', L'R', L'S', L'T',
	L'U', L'V', L'W', L'X', L'Y', L'Z', L'a', L'b', L'c', L'd', 
	L'e', L'f', L'g', L'h', L'i', L'j', L'k', L'l', L'm', L'n', 
	L'o', L'p', L'q', L'r', L's', L't', L'u', L'v', L'w', L'x', 
	L'y', L'z', L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', 
	L'8', L'9', L'+', L'/'
	};


LPWSTR Convert3OctetTo4Base64(DWORD ThreeOctet,	LPWSTR pBase64Buffer)
/*++
Arguments:
	ThreeOctet - input 3Octet value (24 bit) 
	pBase64Buffer - base64 buffer that will be filled with 4 base64 wchar_t

Returned Value:
	updated pointer of pBase64Buffer which points to the next location
--*/
{
	DWORD Res = ((ThreeOctet & xFirst6bChar) >> xFirst6bShift);
	SVSUTIL_ASSERT(Res < 64);
	*pBase64Buffer = xBase642AsciiW[Res];
	++pBase64Buffer;

	//
	// Calc second 6 bits
	//
	Res = ((ThreeOctet & xSecond6bChar) >> xSecond6bShift); 
	SVSUTIL_ASSERT(Res < 64);
	*pBase64Buffer = xBase642AsciiW[Res];
	++pBase64Buffer;

	//
	// Calc third 6 bits
	//
	Res = ((ThreeOctet & xThird6bChar) >> xThird6bShift);
	SVSUTIL_ASSERT(Res < 64);
	*pBase64Buffer = xBase642AsciiW[Res];
	++pBase64Buffer;

	//
	// Calc fourth 6 bits
	//
	Res = ((ThreeOctet & xFourth6bChar) >> xFourth6bShift); 
	SVSUTIL_ASSERT(Res < 64);
	*pBase64Buffer = xBase642AsciiW[Res];
	++pBase64Buffer;

	return(pBase64Buffer);
}


LPWSTR Octet2Base64W(const BYTE* OctetBuffer, DWORD OctetLen, DWORD *Base64Len)
/*++
Arguments:
	OctetBuffer - input Octet buffer 
	OctetLen - Length of the Octet buffer (number of byte elements in buffer)
	Base64Len - (out) base64 buffer len (number of WCHAR elements in buffer)

Returned Value:
    wchar_t Base64 buffer

--*/
{
	*Base64Len =  ((OctetLen + 2) / 3) * 4;

	LPWSTR Base64Buffer = (LPWSTR) (g_funcAlloc(sizeof(WCHAR)*(*Base64Len+1),g_pvAllocData));
	LPWSTR pBase64Buffer = Base64Buffer;

	if (!Base64Buffer)
		return NULL;

	int Complete3Chars = OctetLen / 3;

	for(int i=0; i< Complete3Chars; ++i, OctetBuffer += 3) {
		DWORD Temp = ((OctetBuffer[0]) << xFirst8bShift) 
					 + ((OctetBuffer[1]) << xSecond8bShift) 
					 + ((OctetBuffer[2]) << xThird8bShift);

		pBase64Buffer = Convert3OctetTo4Base64(Temp, pBase64Buffer); 
	}

	int Remainder = OctetLen - Complete3Chars * 3;

	switch(Remainder)
	{
		DWORD Temp;

		case 0:
			break;

		case 1:
			Temp = ((OctetBuffer[0]) << xFirst8bShift); 

			pBase64Buffer = Convert3OctetTo4Base64(Temp, pBase64Buffer); 
			pBase64Buffer -= 2;
			*pBase64Buffer = L'='; 
			++pBase64Buffer;
			*pBase64Buffer = L'='; 
			++pBase64Buffer;
			break;

		case 2:
			Temp = ((OctetBuffer[0]) << xFirst8bShift) 
				   + ((OctetBuffer[1]) << xSecond8bShift);

			pBase64Buffer = Convert3OctetTo4Base64(Temp, pBase64Buffer); 

			--pBase64Buffer;
			*pBase64Buffer = L'='; 
			++pBase64Buffer;
			break;

		default:
			SVSUTIL_ASSERT(0);
			break;
	}

	Base64Buffer[*Base64Len] = L'\0';
	return(Base64Buffer);
}



PSTR FindMimeBoundary(PSTR pszHttpHeaders, BOOL *pfBadRequest) {
	PSTR pszContentType;
	PSTR pszEnd;
	PSTR pszTrav;
	PSTR pszEndBoundary;
	PSTR pszBoundary;

	// find the boundary value.
	pszContentType = FindHeader(pszHttpHeaders,cszContentType);
	if (!pszContentType) {
		SVSUTIL_ASSERT(0); // should've been caught by ISAPI extension already.
		if (pfBadRequest)
			*pfBadRequest = TRUE;
		return NULL;
	}
	
	pszEnd = strstr(pszContentType,cszCRLF);
	pszTrav = MyStrStrI(pszContentType,cszBoundary);

	if (NULL == pszTrav || pszTrav > pszEnd) {
		if (pfBadRequest)
			*pfBadRequest = TRUE;
		return NULL;
	}

	pszTrav += ccBoundary;
	pszTrav = RemoveLeadingSpace(pszTrav,pszEnd);
	
	// mime attribute value can be enclosed by '"' or not
	if (*pszTrav =='"')
		pszTrav++;

	// end is either CRLF or ;
	pszEndBoundary = strchr(pszTrav,';');
    if ((pszEndBoundary != NULL) && (pszEnd > pszEndBoundary))  {
        pszEnd = --pszEndBoundary;
    }

	pszEnd = RemoveTralingSpace(pszTrav, pszEnd);

	// set the pszEnd to one char behind buffer to temporarily make it a string.
	// mime attribute value can be enclosed by '"'; if it's present \0 it, otherwise advance one.
    if (*pszEnd !='"')
        pszEnd++;

	CHAR cSave = *pszEnd;
	*pszEnd = 0;

	if (NULL == (pszBoundary = svsutil_strdup(pszTrav))) {
		scerror_DebugOutM(VERBOSE_MASK_SRMP,(L"Unable to allocate memory.  GLE=0x%08x\r\n",GetLastError()));
		return NULL;
	}
	*pszEnd = cSave;
	return pszBoundary;
}


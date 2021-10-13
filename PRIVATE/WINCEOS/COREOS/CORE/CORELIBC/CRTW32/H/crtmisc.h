//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _INC_CRTMISC
#define _INC_CRTMISC

#define isleadbyte(_c)	 ( IsDBCSLeadByte(_c))

// from sys/stat.h
#define _S_IREAD	0000400 	/* read permission, owner */
#define _S_IWRITE	0000200 	/* write permission, owner */

// from cvt.h
#define MUL10(x)	( (((x)<<2) + (x))<<1 )
#define CVTBUFSIZE (309+40) /* # of digits in max. dp value + slop */

#define change_to_multibyte(buf, len, wchar) WideCharToMultiByte(CP_ACP, 0, &wchar, 1, buf, len, NULL, NULL)
#define change_to_widechar(pwc, sz, len)     MultiByteToWideChar(CP_ACP, 0, sz, len, pwc, 1)

// Shims to allow us to redirect output to OutputDebugString
#define MyReadFile(h, buffer, cnt, pRead, x) \
	(((h)==(HANDLE)(-2)) ? (*(pRead)=0, 0) : ReadFile((h), (buffer), (cnt), (pRead), (x)))

//#define MyWriteFile(h, buffer, cnt, pWrote, x) \
//	(((h)==(HANDLE)(-2)) ? (char temp, temp=buffer[cnt], buffer[cnt]='\0',OutputDebugStringA(buffer), (*(pWrote)=(cnt)), buffer[cnt]=temp, TRUE) : \
//			WriteFile((h), (buffer), (cnt), (pWrote), (x)))


#endif 

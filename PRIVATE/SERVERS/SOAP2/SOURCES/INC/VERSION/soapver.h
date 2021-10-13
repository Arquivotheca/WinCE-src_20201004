//+----------------------------------------------------------------------------
//
// Copyright (c) Microsoft Corporation
//
// File:   SoapVer.h
//
// Contents:
//
// Header File
// 
//      Versions of Soap SDK components
//		And build version strings
//
//-----------------------------------------------------------------------------

/*--------------------------------------------------------------*/
/* the following values should be modified by the official      */
/* builder for each build                                       */
/*                                                              */
/* the VER_PRODUCTBUILD lines must contain the product          */
/* comments and end with the build#<CR><LF>                     */
/*                                                              */
/* the VER_PRODUCTBETA_STR lines must  contain the product      */
/* comments and end with "some string"<CR><LF>                  */
/*--------------------------------------------------------------*/
#ifndef __SOAPVER_H_INCLUDED__
#define __SOAPVER_H_INCLUDED__

#define SOAP_SDK_VERSION_MAJOR      1
#define SOAP_SDK_VERSION_MINOR      0

#define SOAP_SDK_VERSION    SOAP_SDK_VERSION_MAJOR.SOAP_SDK_VERSION_MINOR

#if _MSC_VER > 1000
#pragma once
#endif
#include "version.h"			// where the build process updates the build numbers

//-----------------------------------------------------------------------------
// Constants

#ifndef VER_PRODBUILD_MAJOR
#define VER_PRODBUILD_MAJOR	rmj
#endif

#ifndef VER_PRODBUILD_MINOR
#define VER_PRODBUILD_MINOR	rmm
#endif

#define VER_PRODBUILD_BUILD	rup
#define VER_PRODUCTBUILD 	rup
#define VER_PRODBUILD_QFE	rbld
//-----------------------------------------------------------------------------
// Macros to make a string from version integers

#define VER_MAKESTR_1(x)		#x
#define VER_MAKESTR(x)			VER_MAKESTR_1( ##x)

#if VER_PRODBUILD_MAJOR < 10
#define VER_LPAD	"0"
#else
#define VER_LPAD	
#endif

#if 	(VER_PRODBUILD_BUILD < 10)
#define VER_BPAD "000"+VER_PRODBUILD_BUILD
#elif	(VER_PRODBUILD_BUILD < 100)
#define VER_BPAD "00"+VER_PRODBUILD_BUILD
#elif	(VER_PRODBUILD_BUILD < 1000)
#define VER_BPAD "0"+VER_PRODBUILD_BUILD
#else
#define VER_BPAD VER_PRODBUILD_BUILD
#endif

//-----------------------------------------------------------------------------
// Constants used in version resource

#define VER_FILEVERSION			VER_PRODBUILD_MAJOR,VER_PRODBUILD_MINOR,VER_PRODBUILD_BUILD,rbld
#define VER_FILEVERSION_STR		VER_MAKESTR(VER_PRODBUILD_MAJOR) "." VER_MAKESTR(VER_PRODBUILD_MINOR) "." VER_MAKESTR(VER_PRODBUILD_BUILD) "." VER_MAKESTR(VER_PRODBUILD_QFE)
#define VER_FILEVERSION_WSTR	 _T(VER_MAKESTR(VER_PRODBUILD_MAJOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_MINOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_BUILD)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_QFE))
#define VER_FILEVERSION_LPADWSTR _T(VER_LPAD) _T(VER_MAKESTR(VER_PRODBUILD_MAJOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_MINOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_BUILD)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_QFE))

#define VER_PRODUCTVERSION		VER_PRODBUILD_MAJOR,VER_PRODBUILD_MINOR,VER_PRODBUILD_BUILD,rbld
#define VER_PRODUCTVERSION_STR	VER_MAKESTR(VER_PRODBUILD_MAJOR) "." VER_MAKESTR(VER_PRODBUILD_MINOR) "." VER_MAKESTR(VER_PRODBUILD_BUILD) "." VER_MAKESTR(VER_PRODBUILD_QFE)
#define VER_PRODUCTVERSION_WSTR	_T(VER_MAKESTR(VER_PRODBUILD_MAJOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_MINOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_BUILD)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_QFE))
#define VER_PRODUCTVERSION_LPADWSTR	_T(VER_LPAD) _T(VER_MAKESTR(VER_PRODBUILD_MAJOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_MINOR)) _T(".") _T(VER_MAKESTR(VER_PRODBUILD_BUILD)) _T(".")_T(VER_MAKESTR(VER_PRODBUILD_QFE))


#define VER_COMPANYNAME_STR		"Microsoft Corporation\0"
#define VER_PRODUCTNAME_STR		"Microsoft Data Access Components\0"
#define VER_LASTLEGALCOPYRIGHT_YEAR	"2001"
#define VER_LEGALCOPYRIGHT_STR   "\251 1988-2001 Microsoft Corp. All rights reserved.\0"


#define VER_PRODUCTBETA_STR         /* NT */     ""
#define VER_PRODUCTVERSION_STRING   VER_FILEVERSION_STR
#define VER_PRODUCTVERSION_W        VER_FILEVERSION_WSTR
#define VER_PRODUCTVERSION_DW       VER_FILEVERSION_LPADWSTR


/*--------------------------------------------------------------*/
/* the following section defines values used in the version     */
/* data structure for all files, and which do not change.       */
/*--------------------------------------------------------------*/

/* default is nodebug */
#if DBG
#define VER_DEBUG                   VS_FF_DEBUG
#else
#define VER_DEBUG                   0
#endif

#define VER_FILEFLAGSMASK           VS_FFI_FILEFLAGSMASK
#define VER_FILEOS                  VOS_NT_WINDOWS32
#define VER_FILEFLAGS               (VER_DEBUG)

#define VER_LEGALTRADEMARKS_STR     \
"Microsoft(R) is a registered trademark of Microsoft Corporation. Windows (R) is a registered trademark of Microsoft Corporation."

#endif //__SOAPVER_H_INCLUDED__

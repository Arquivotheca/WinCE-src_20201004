//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#define RESBASE_ASP_ERROR   100

// These codes are uses in the m_aspErr to determine which error we have
#define IDS_E_PARSER				(RESBASE_ASP_ERROR + 1 )
#define IDS_E_MULTIPLE_INC_LINE		(RESBASE_ASP_ERROR + 2 )
#define IDS_E_NOMEM					(RESBASE_ASP_ERROR + 3 )
#define IDS_E_SENT_HEADERS			(RESBASE_ASP_ERROR + 4 )
#define IDS_E_HTTPD					(RESBASE_ASP_ERROR + 5 )

#define IDS_E_BUFFER_ON				(RESBASE_ASP_ERROR + 6 )
#define IDS_E_FILE_OPEN				(RESBASE_ASP_ERROR + 7 )
#define IDS_E_BAD_INDEX				(RESBASE_ASP_ERROR + 8 )
#define IDS_E_READ_ONLY				(RESBASE_ASP_ERROR + 9 )
#define IDS_E_EXPECTED_STRING		(RESBASE_ASP_ERROR + 10 )

#define IDS_E_NO_SCRIPT_TERMINATOR	(RESBASE_ASP_ERROR + 11 )
#define IDS_E_BAD_INCLUDE			(RESBASE_ASP_ERROR + 12 )
#define IDS_E_MAX_INCLUDE			(RESBASE_ASP_ERROR + 13 )
#define IDS_E_COMMENT_UNTERMINATED	(RESBASE_ASP_ERROR + 14 )
#define IDS_E_NO_VIRTUAL_DIR		(RESBASE_ASP_ERROR + 15 )

#define IDS_E_BAD_SCRIPT_LANG		(RESBASE_ASP_ERROR + 16 )
#define IDS_E_NO_ATTRIB				(RESBASE_ASP_ERROR + 17 )
#define IDS_E_UNKNOWN_ATTRIB		(RESBASE_ASP_ERROR + 18 )
#define IDS_E_SCRIPT_INIT			(RESBASE_ASP_ERROR + 19 )
#define IDS_E_NOT_IMPL				(RESBASE_ASP_ERROR + 20 )

#define IDS_E_BAD_SUBINDEX			(RESBASE_ASP_ERROR + 21 )
#define IDS_E_BAD_CODEPAGE			(RESBASE_ASP_ERROR + 22 )
#define IDS_E_BAD_LCID				(RESBASE_ASP_ERROR + 23 )
#define IDS_E_UNKNOWN				(RESBASE_ASP_ERROR + 24 )


// If additions are made to this table, update this value as well
#define MAX_ERROR_CODE				IDS_E_BAD_LCID


//  These are additional error messages that aren't generic like the ones above

#define RESBASE_ASP_MISC				200

// These are used in the Script Error reporting function.
#define IDS_LINE_FILE_ERROR     		(RESBASE_ASP_MISC + 1)
#define IDS_DESCRIPTION					(RESBASE_ASP_MISC + 2)

// These are used in the case that the engine wasn't able to report the error itself
#define IDS_PARSE_ERROR_FROM_IACTIVE	(RESBASE_ASP_MISC + 3)
#define IDS_ASP_ERROR_FROM_IACTIVE		(RESBASE_ASP_MISC + 4)
#define IDS_ERROR_CODE					(RESBASE_ASP_MISC + 5)


#define IDS_FALSE						(RESBASE_ASP_MISC + 6)
#define IDS_TRUE						(RESBASE_ASP_MISC + 7)


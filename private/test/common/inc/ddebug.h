//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*****************************************************************************\
 *
 *  module:             ddebug.h
 *
 *  purpose:    public header for simple debugging
 *
 *
 *  Usage:
 *
 *  DASSERT: basic assert
 *  CASSERT: assert that does a return( FALSE ) on fail
 *  PASSERT: assert that does a return( NULL ) on fail
 *  DERRORMSG( text ): print out error message text
 *  DBGCODE( code ): code is only included in debug build
 *  VERBOSE( code ): code is only included if _VERBOSE is defined
 *
\*****************************************************************************/

#ifndef __DDEBUG_H__                       
#define __DDEBUG_H__

#include <testlog.h>


#ifdef __cplusplus
	extern "C" {
#endif


#if defined DEBUG | defined _DEBUG

	#pragma message( "DDEBUG: DEBUG VERSION" )

	#define DBGCODE( code ) code

    #define DDEBUGBREAK \
        if( GetLSDebugLevel() > 0 ) {\
            DebugBreak();\
        }


	#if defined UNDER_CE | defined DDEBUG_NO_MSGBOX

		#define DASSERT( code ) \
			if ( !( code ) ) { \
				LSPrintf( LOG_CSR, TEXT( "DASSERT failed! Line: %i, File: %s\r\n" ), \
					__LINE__, TEXT( __FILE__ ) ); \
			}

		#define DERRORMSG( text ) \
			LSPrintf( LOG_CSR, TEXT( "ERROR: " ) TEXT( text ) TEXT( "\r\n" ) )

		#define PERRMSG( text ) LogDetail( TEXT( "ERROR: " ) TEXT( text ) )

        #ifndef CASSERT
		#define CASSERT( code ) \
		if ( !( code ) ) { \
			LSPrintf( LOG_CSR, TEXT( "CASSERT failed! Line: %i, File: %s\r\n" ), \
						__LINE__, TEXT( __FILE__ ) ); \
			return( FALSE ); \
		}
        #endif

		#define PASSERT( code ) \
		if ( !( code ) ) { \
			LSPrintf( LOG_CSR, TEXT( "PASSERT failed! Line: %i, File: %s\r\n" ), \
						__LINE__, TEXT( __FILE__ ) ); \
			return( NULL ); \
		}

        #define VRFY( code ) \
            if ( !( code ) ) { \
                LSPrintf( LOG_CSR, TEXT( "VRFY failed! Line: %i, File: %s\r\n" ), \
                    __LINE__, TEXT( __FILE__ ) ); \
                DDEBUGBREAK\
            }

        #define VRFYABORT_B( code ) \
            if ( !( code ) ) { \
                LSPrintf( LOG_CSR, TEXT( "VRFYABORT_B failed! Line: %i, File: %s\r\n" ), \
                    __LINE__, TEXT( __FILE__ ) ); \
                DDEBUGBREAK\
                return( FALSE ); \
            }

        #define VRFYABORT_P( code ) \
            if ( !( code ) ) { \
                LSPrintf( LOG_CSR, TEXT( "VRFYABORT_P failed! Line: %i, File: %s\r\n" ), \
                    __LINE__, TEXT( __FILE__ ) ); \
                DDEBUGBREAK\
                return( NULL ); \
            }


	#else

		#define DASSERT( code ) \
			if ( !( code ) ) { \
				MessageBox( NULL, TEXT( "DASSERT failed! Code: " )TEXT( #code ), \
					TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
			}

		#define DERRORMSG( text ) \
			MessageBox( NULL, TEXT( text ), TEXT( __FILE__ ), \
				MB_OK | MB_ICONEXCLAMATION )

		#define PERRMSG( text ) DERRORMSG( text )

        #ifndef CASSERT
		#define CASSERT( code ) \
			if ( !( code ) ) { \
    			MessageBox( NULL, TEXT( "CASSERT failed! Code: " )TEXT( #code ), \
    				TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
				return( FALSE ); \
			}
        #endif
        
		#define PASSERT( code ) \
			if ( !( code ) ) { \
    			MessageBox( NULL, TEXT( "PASSERT failed! Code: " )TEXT( #code ), \
    				TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
				return( NULL ); \
			}

        #define VRFY( code ) \
            if ( !( code ) ) { \
                MessageBox( NULL, TEXT( "VRFY failed! Code: " )TEXT( #code ), \
                    TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
            }

        #define VRFYABORT_B( code ) \
            if ( !( code ) ) { \
                MessageBox( NULL, TEXT( "VRFYABORT_B failed! Code: " )TEXT( #code ), \
                    TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
                return( FALSE ); \
            }

        #define VRFYABORT_P( code ) \
            if ( !( code ) ) { \
                MessageBox( NULL, TEXT( "VRFYABORT_P failed! Code: " )TEXT( #code ), \
                    TEXT( __FILE__ ), MB_OK | MB_ICONEXCLAMATION ); \
                return( NULL ); \
            }

	#endif

#else

	#pragma message( "DDEBUG: RETAIL VERSION" )

	#define DBGCODE( code )

    #define DDEBUGBREAK

	#define DASSERT( code ) \
		if ( (code) ) { \
		}

    #ifndef CASSERT
	#define CASSERT( code ) \
		if ( !( code ) ) { \
			return( FALSE ); \
		}
    #endif

	#define PASSERT( code ) \
		if ( !( code ) ) { \
			return( NULL ); \
		}

	#define VRFY( code ) \
		if ( (code) ) { \
		}

	#define VRFYABORT_B( code ) \
		if ( !( code ) ) { \
			return( FALSE ); \
		}

	#define VRFYABORT_P( code ) \
		if ( !( code ) ) { \
			return( NULL ); \
		}

	#define DERRORMSG( text )

	#define PERRMSG( text )

#endif                  /* ifdef DEBUG */


#ifdef _VERBOSE

	#pragma message( "DDEBUG: VERBOSE" )

	#define VERBOSE( code ) code

#else

	#define VERBOSE( code )

#endif                  /* ifdef _VERBOSE */

#define DRMSG( text ) \
	LSPrintf( LOG_CR, TEXT( text ) TEXT( "\r\n" ))

#define DSRMSG( text ) \
	LSPrintf( LOG_CSR, TEXT( text ) TEXT( "\r\n" ))

#define PMSG( text ) \
	LogDetail( TEXT( text )) 


#ifdef __cplusplus
	}
#endif

#endif          /* __DDEBUG_H__ */


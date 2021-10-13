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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
//  File Name:  crypt.h
//
//  Abstract:
//
//  This module contains the public data structures and API definitions
//  needed to utilize the encryption library
//
//
//

#ifndef _NTCRYPT_
#define _NTCRYPT_

#define CCHMAXPATH 260      // From RAS authtype.h

// Encryption Defines - gathered from other CHPPP header files


/////////////////////////////////////////////////////////////////////////
//                                                                     //
// Core encryption types                                               //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

#define CLEAR_BLOCK_LENGTH          8

typedef struct _CLEAR_BLOCK 
{
    CHAR    data[CLEAR_BLOCK_LENGTH];
}
CLEAR_BLOCK;

typedef CLEAR_BLOCK *               PCLEAR_BLOCK;
typedef CLEAR_BLOCK FAR *           LPCLEAR_BLOCK;


#define CYPHER_BLOCK_LENGTH         8

typedef struct _CYPHER_BLOCK 
{
    CHAR    data[CYPHER_BLOCK_LENGTH];
}
CYPHER_BLOCK;

typedef CYPHER_BLOCK *              PCYPHER_BLOCK;
typedef CYPHER_BLOCK FAR *          LPCYPHER_BLOCK;


#define BLOCK_KEY_LENGTH            7

typedef struct _BLOCK_KEY 
{
    CHAR    data[BLOCK_KEY_LENGTH];
}
BLOCK_KEY;

typedef BLOCK_KEY *                 PBLOCK_KEY;
typedef BLOCK_KEY FAR *             LPBLOCK_KEY;


/////////////////////////////////////////////////////////////////////////
//                                                                     //
// Arbitrary length data encryption types                              //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

typedef struct _CRYPT_BUFFER 
{
    ULONG   Length;         // Number of valid bytes in buffer
    ULONG   MaximumLength;  // Number of bytes pointed to by Buffer
    LPVOID   Buffer;
} 
CRYPT_BUFFER;

typedef CRYPT_BUFFER *  PCRYPT_BUFFER;
typedef CRYPT_BUFFER FAR *  LPCRYPT_BUFFER;

typedef CRYPT_BUFFER    CLEAR_DATA;
typedef CLEAR_DATA *    PCLEAR_DATA;
typedef CLEAR_DATA FAR *    LPCLEAR_DATA;

typedef CRYPT_BUFFER    DATA_KEY;
typedef DATA_KEY *      PDATA_KEY;
typedef DATA_KEY FAR *      LPDATA_KEY;

typedef CRYPT_BUFFER    CYPHER_DATA;
typedef CYPHER_DATA *   PCYPHER_DATA;
typedef CYPHER_DATA FAR *   LPCYPHER_DATA;


/////////////////////////////////////////////////////////////////////////
//                                                                     //
// Lan Manager data types                                              //
//                                                                     //
/////////////////////////////////////////////////////////////////////////


//
// Define a LanManager compatible password
//
// A LanManager password is a null-terminated ansi string consisting of a
// maximum of 14 characters (not including terminator)
//

typedef CHAR *                      PLM_PASSWORD;
typedef CHAR FAR *                  LPLM_PASSWORD;

//
// Define the result of the 'One Way Function' (OWF) on a LM password
//

#define LM_OWF_PASSWORD_LENGTH      (CYPHER_BLOCK_LENGTH * 2)

typedef struct _LM_OWF_PASSWORD 
{
    CYPHER_BLOCK data[2];
}
LM_OWF_PASSWORD;

typedef LM_OWF_PASSWORD *           PLM_OWF_PASSWORD;
typedef LM_OWF_PASSWORD FAR *       LPLM_OWF_PASSWORD;

//
// Define the challenge sent by the Lanman server during logon
//

#define LM_CHALLENGE_LENGTH         CLEAR_BLOCK_LENGTH

typedef CLEAR_BLOCK                 LM_CHALLENGE;
typedef LM_CHALLENGE *              PLM_CHALLENGE;
typedef LM_CHALLENGE FAR *          LPLM_CHALLENGE;

//
// Define the response sent by redirector in response to challenge from server
//

#define LM_RESPONSE_LENGTH          (CYPHER_BLOCK_LENGTH * 3)

typedef struct _LM_RESPONSE 
{
    CYPHER_BLOCK  data[3];
}
LM_RESPONSE;

typedef LM_RESPONSE *               PLM_RESPONSE;
typedef LM_RESPONSE FAR *           LPLM_RESPONSE;

//
// Define the result of the reversible encryption of an OWF'ed password.
//

#define ENCRYPTED_LM_OWF_PASSWORD_LENGTH (CYPHER_BLOCK_LENGTH * 2)

typedef struct _ENCRYPTED_LM_OWF_PASSWORD 
{
    CYPHER_BLOCK data[2];
}
ENCRYPTED_LM_OWF_PASSWORD;

typedef ENCRYPTED_LM_OWF_PASSWORD * PENCRYPTED_LM_OWF_PASSWORD;
typedef ENCRYPTED_LM_OWF_PASSWORD FAR * LPENCRYPTED_LM_OWF_PASSWORD;


//
// Define the session key maintained by the redirector and server
//

#define LM_SESSION_KEY_LENGTH       LM_CHALLENGE_LENGTH

typedef LM_CHALLENGE                LM_SESSION_KEY;
typedef LM_SESSION_KEY *            PLM_SESSION_KEY;
typedef LM_SESSION_KEY FAR *        LPLM_SESSION_KEY;


//
// Define the index type used to encrypt OWF Passwords
//

typedef LONG                        CRYPT_INDEX;
typedef CRYPT_INDEX *               PCRYPT_INDEX;
typedef CRYPT_INDEX FAR *           LPCRYPT_INDEX;

/////////////////////////////////////////////////////////////////////////
//                                                                     //
// 'NT' encryption types that are used to duplicate existing LM        //
//      functionality with improved algorithms.                        //
//                                                                     //
/////////////////////////////////////////////////////////////////////////


//typedef UNICODE_STRING              NT_PASSWORD;
//typedef NT_PASSWORD *               PNT_PASSWORD;
//typedef NT_PASSWORD FAR *           LPNT_PASSWORD;

#define NT_OWF_PASSWORD_LENGTH      LM_OWF_PASSWORD_LENGTH

typedef LM_OWF_PASSWORD             NT_OWF_PASSWORD;
typedef NT_OWF_PASSWORD *           PNT_OWF_PASSWORD;
typedef NT_OWF_PASSWORD FAR *       LPNT_OWF_PASSWORD;


#define NT_CHALLENGE_LENGTH         LM_CHALLENGE_LENGTH

typedef LM_CHALLENGE                NT_CHALLENGE;
typedef NT_CHALLENGE *              PNT_CHALLENGE;
typedef NT_CHALLENGE FAR *          LPNT_CHALLENGE;


#define NT_RESPONSE_LENGTH          LM_RESPONSE_LENGTH

typedef LM_RESPONSE                 NT_RESPONSE;
typedef NT_RESPONSE *               PNT_RESPONSE;
typedef NT_RESPONSE FAR *           LPNT_RESPONSE;


#define ENCRYPTED_NT_OWF_PASSWORD_LENGTH ENCRYPTED_LM_OWF_PASSWORD_LENGTH

typedef ENCRYPTED_LM_OWF_PASSWORD   ENCRYPTED_NT_OWF_PASSWORD;
typedef ENCRYPTED_NT_OWF_PASSWORD * PENCRYPTED_NT_OWF_PASSWORD;
typedef ENCRYPTED_NT_OWF_PASSWORD FAR * LPENCRYPTED_NT_OWF_PASSWORD;


#define NT_SESSION_KEY_LENGTH       LM_SESSION_KEY_LENGTH

typedef LM_SESSION_KEY              NT_SESSION_KEY;
typedef NT_SESSION_KEY *            PNT_SESSION_KEY;
typedef NT_SESSION_KEY FAR *        LPNT_SESSION_KEY;


/////////////////////////////////////////////////////////////////////////
//                                                                     //
// 'NT' encryption types for new functionality not present in LM       //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

//
// The user session key is similar to the LM and NT session key except it
// is different for each user on the system. This allows it to be used
// for secure user communication with a server.
//
#define USER_SESSION_KEY_LENGTH     (CYPHER_BLOCK_LENGTH * 2)

typedef struct _USER_SESSION_KEY 
{
    CYPHER_BLOCK data[2];
}
USER_SESSION_KEY;

typedef USER_SESSION_KEY          * PUSER_SESSION_KEY;
typedef USER_SESSION_KEY      FAR * LPUSER_SESSION_KEY;

//
// Exported encrypting functions
//
void
DesEncrypt(
	IN	BYTE	*Clear,		// 8-octet
	IN	BYTE	*Key,		// 7-octet
	OUT	BYTE	*Cypher);	// 8-octet

void 
DesHash(
	IN	BYTE	*Clear,		// 7-octet
	OUT	BYTE	*Cypher);	// 8-octet

void
ChallengeResponse(
	IN	BYTE *Challenge,		//  8 octet Challenge
	IN	BYTE *PasswordDigest,	// 16 octet PasswordHash
	OUT	BYTE *Response );		// 24 octet Response

#endif // _NTCRYPT_

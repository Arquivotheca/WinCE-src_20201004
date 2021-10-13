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
#include "SMB_Globals.h"
#include "Encrypt.h"

extern "C" {
#include <descrypt.h>
};
#include <lmcons.h>

//**    Encrypt - encrypt text with key
//
//  This routine takes a key and encrypts 8 bytes of data with the key
//  until the result buffer is filled.  The key is advanced ahead for each
//  iteration so it must be (bufLen/8)*7 bytes long.  Any partial buffer
//  left over is filled with zeroes.


void
Encrypt(
    char *key,                  // Key to encrypt with
    char *text,                 // 8 bytes of text to encrypt
    char *buf,                  // buffer to receive result
    int bufLen)
{
    TRACEMSG(ZONE_SECURITY,(L"Encrypt"));
    ASSERT(bufLen >= CRYPT_TXT_LEN);
    
    do {
        //InitKey(key);
        //des(text, buf, ENCRYPT);
        DES_ECB_LM(ENCR_KEY, key, (UCHAR *)text, (UCHAR *)buf);
        key += CRYPT_KEY_LEN;
        buf += CRYPT_TXT_LEN;
    } while ((bufLen -= CRYPT_TXT_LEN) >= CRYPT_TXT_LEN);

    if (bufLen != 0)
        memset(buf, 0, bufLen); 
    return;
}


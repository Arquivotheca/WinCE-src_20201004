//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


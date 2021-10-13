//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef ENCRYPT_H
#define ENCRYPT_H

void
Encrypt(
    char *key,                  // Key to encrypt with
    char *text,                 // 8 bytes of text to encrypt
    char *buf,                  // buffer to receive result
    int bufLen);

#endif

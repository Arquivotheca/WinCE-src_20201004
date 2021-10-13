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

    rand.c

Abstract:

    Random Generator for DHCPv6 client.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "rand.tmh"


DWORD
DhcpV6GenerateRandom(
    PUCHAR pucBuffer,
    ULONG uLength
    )
{
#ifdef UNDER_CE
    if (CeGenRandom(uLength, pucBuffer))
        return STATUS_SUCCESS;
    else
        return STATUS_INVALID_PARAMETER;

#else
    DWORD dwError = 0;
    HCRYPTPROV hCryptProv;


    if (!CryptAcquireContext(
            &hCryptProv,
            NULL,
            NULL,
            PROV_RSA_FULL,
            0))
    {
        dwError = GetLastError();


        if (dwError == NTE_BAD_KEYSET) {
            dwError = 0;
            if (!CryptAcquireContext(
                    &hCryptProv,
                    NULL,
                    NULL,
                    PROV_RSA_FULL,
                    CRYPT_NEWKEYSET))
            {
                dwError = GetLastError();
            }
        }
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (!CryptGenRandom(
            hCryptProv,
            uLength,
            pucBuffer))
    {
        dwError = GetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    if (!CryptReleaseContext(hCryptProv,0))
    {
        dwError = GetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:
    return dwError;
#endif  // else UNDER_CE

}

//
// Create uniform distribution random number between -0.1 and 0.1
//
DOUBLE
DhcpV6UniformRandom(
    )
{
    DOUBLE dbRand = 0;


    dbRand = rand() / ((RAND_MAX/2) + 1.0);
    dbRand -= 1;
    dbRand = dbRand/10;

    return dbRand;
}

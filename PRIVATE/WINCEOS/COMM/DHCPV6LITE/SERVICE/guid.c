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

    guid.c

Abstract:

    This Module implements the guid manipulation functions.

Author:

    George Shaw (GShaw) 9-Oct-1996

Environment:

    Pure Runtime Library Routine

Revision History:

--*/
#ifdef UNDER_CE
//#include "oscfg.h"
#include <winsock2.h>
#include <nspapi.h>
#include <wdm.h>
#include "md5.h"
#ifdef NTSYSAPI
#undef NTSYSAPI
#endif //NTSYSAPI
#define NTSYSAPI
#ifdef NTAPI
#undef NTAPI
#endif //NTAPI
#define NTAPI
#ifndef RTL_PAGED_CODE
#define RTL_PAGED_CODE()
#endif //RTL_PAGED_CODE

#else // UNDER_CE
#include "nt.h"
#include "ntrtlp.h"
#endif //UNDER_CE

const WCHAR GuidFormat[] = L"{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

#if defined(ALLOC_PRAGMA) && defined(NTOS_KERNEL_RUNTIME)
static
int
__cdecl
ScanHexFormat(
    IN const WCHAR* Buffer,
    IN ULONG MaximumLength,
    IN const WCHAR* Format,
    ...);

#pragma alloc_text(PAGE, RtlStringFromGUID)
#pragma alloc_text(PAGE, ScanHexFormat)
#pragma alloc_text(PAGE, RtlGUIDFromString)
#endif // ALLOC_PRAGMA && NTOS_KERNEL_RUNTIME

extern const WCHAR GuidFormat[];

#define GUID_STRING_SIZE 38


WCHAR *
RtlAllocateStringRoutine(DWORD dwBytes) 
{
	return malloc(dwBytes);

}   //  RtlAllocateStringRoutine()

VOID
RtlFreeUnicodeString(
    PUNICODE_STRING us)
{
    ASSERT(us);
    ASSERT(us->Buffer);
    free(us->Buffer);
}


NTSYSAPI
NTSTATUS
NTAPI
RtlStringFromGUID(
    IN REFGUID Guid,
    OUT PUNICODE_STRING GuidString
    )
/*++

Routine Description:

    Constructs the standard string version of a GUID, in the form:
    "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}".

Arguments:

    Guid -
        Contains the GUID to translate.

    GuidString -
        Returns a string that represents the textual format of the GUID.
        Caller must call RtlFreeUnicodeString to free the buffer when done with
        it.

Return Value:

    NTSTATUS - Returns STATUS_SUCCESS if the user string was succesfully
    initialized.

--*/
{
    RTL_PAGED_CODE();
    GuidString->Length = GUID_STRING_SIZE * sizeof(WCHAR);
    GuidString->MaximumLength = GuidString->Length + sizeof(UNICODE_NULL);
    if (!(GuidString->Buffer = RtlAllocateStringRoutine(GuidString->MaximumLength))) {
        return STATUS_NO_MEMORY;
    }
    swprintf(GuidString->Buffer, GuidFormat, Guid->Data1, Guid->Data2, Guid->Data3, Guid->Data4[0], Guid->Data4[1], Guid->Data4[2], Guid->Data4[3], Guid->Data4[4], Guid->Data4[5], Guid->Data4[6], Guid->Data4[7]);
    return STATUS_SUCCESS;
}


static
int
__cdecl
ScanHexFormat(
    IN const WCHAR* Buffer,
    IN ULONG MaximumLength,
    IN const WCHAR* Format,
    ...)
/*++

Routine Description:

    Scans a source Buffer and places values from that buffer into the parameters
    as specified by Format.

Arguments:

    Buffer -
        Contains the source buffer which is to be scanned.

    MaximumLength -
        Contains the maximum length in characters for which Buffer is searched.
        This implies that Buffer need not be UNICODE_NULL terminated.

    Format -
        Contains the format string which defines both the acceptable string format
        contained in Buffer, and the variable parameters which follow.

Return Value:

    Returns the number of parameters filled if the end of the Buffer is reached,
    else -1 on an error.

--*/
{
    va_list ArgList;
    int     FormatItems;

    va_start(ArgList, Format);
    for (FormatItems = 0;;) {
        switch (*Format) {
        case 0:
            return (MaximumLength && *Buffer) ? -1 : FormatItems;
        case '%':
            Format++;
            if (*Format != '%') {
                ULONG   Number;
                int     Width;
                int     Long;
                PVOID   Pointer;

                for (Long = 0, Width = 0;; Format++) {
                    if ((*Format >= '0') && (*Format <= '9')) {
                        Width = Width * 10 + *Format - '0';
                    } else if (*Format == 'l') {
                        Long++;
                    } else if ((*Format == 'X') || (*Format == 'x')) {
                        break;
                    }
                }
                Format++;
                for (Number = 0; Width--; Buffer++, MaximumLength--) {
                    if (!MaximumLength)
                        return -1;
                    Number *= 16;
                    if ((*Buffer >= '0') && (*Buffer <= '9')) {
                        Number += (*Buffer - '0');
                    } else if ((*Buffer >= 'a') && (*Buffer <= 'f')) {
                        Number += (*Buffer - 'a' + 10);
                    } else if ((*Buffer >= 'A') && (*Buffer <= 'F')) {
                        Number += (*Buffer - 'A' + 10);
                    } else {
                        return -1;
                    }
                }
                Pointer = va_arg(ArgList, PVOID);
                if (Long) {
                    *(PULONG)Pointer = Number;
                } else {
                    *(PUSHORT)Pointer = (USHORT)Number;
                }
                FormatItems++;
                break;
            }
            /* no break */
        default:
            if (!MaximumLength || (*Buffer != *Format)) {
                return -1;
            }
            Buffer++;
            MaximumLength--;
            Format++;
            break;
        }
    }
}

//
// This function is NOT equivalent to the NT function of the same name. It
// can take any unicode string, regardless of whether it is in Guid format.
// The Guid is generated from a hash, so it doesn't return a Guid equivalent
// to the string, but the same Guid is always generated for any given string.
//
extern void
CreateGUIDFromName(const char *Name, GUID *Guid); // In init.c

//* CreateGUIDFromName
//
//  Given the string name of an interface, creates a corresponding guid.
//  The guid is a hash of the string name.
//
void
CreateGUIDFromName(const char *Name, GUID *Guid)
{
    MD5_CTX Context;

    MD5Init(&Context);
    MD5Update(&Context, (uchar *)Name, (uint)strlen(Name));
    MD5Final(&Context);

#ifdef UNDER_CE
    memcpy(Guid, Context.digest, 8);
    memcpy(Guid->Data4, (char *)Context.digest + 8, 8);
#else // UNDER_CE
    memcpy(Guid, Context.digest, MD5DIGESTLEN);
#endif // UNDER_CE
}


NTSTATUS RtlGUIDFromString(IN PUNICODE_STRING GuidString, OUT GUID *Guid) {

	char GuidNameA[32];
	int GuidNameAScan;
	WCHAR *GuidNameScan;

	// NDIS doesn't pass a GUID on CE, so we need to generate an artificial
	// GUID using MD5 hashing...
	if (GuidString->Length > (32 * sizeof(WCHAR))) {
		GuidNameScan = GuidString->Buffer + 
			(GuidString->Length / sizeof(WCHAR) - 32);
	}
	else {
		GuidNameScan = GuidString->Buffer;
	}
	GuidNameAScan = 0;
	while ((*GuidNameScan) && (GuidNameAScan < 31)) {
		GuidNameA[GuidNameAScan ++] = (char)*GuidNameScan;
		GuidNameScan ++;
	}
	GuidNameA[GuidNameAScan] = 0;
	CreateGUIDFromName(GuidNameA, Guid);

	return STATUS_SUCCESS;
}

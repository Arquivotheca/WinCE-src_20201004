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

    name.c

Abstract:

    This file contains routines for manipulating FAT filenames.

Revision History:

--*/

#include "fatfs.h"

#define OEM_REPL_CHAR                   0x5F


/*  ChkSumName - compute 8.3 filename checksum
 *
 *  ENTRY
 *      pchOEM - pointer to 8.3 filename
 *
 *  EXIT
 *      8-bit checksum.  Note that it has to be computed the same way
 *      as all other LFN-enabled FAT file system implementations, because
 *      the checksum is stored on-disk.
 */

BYTE ChkSumName(const BYTE *pchOEM)
{
    int c;
    BYTE bSum = 0;

    for (c = 0 ; c < OEMNAMESIZE ; ++c) {
        bSum = (bSum>>1) | ((bSum&1)<<7);
        bSum += *pchOEM++;
    }
    return bSum;
}


/*  InitNumericTail - calculate starting position for numeric tail
 *
 *  This function calculates the 0-based index in the 8.3 (achOEM) name
 *  of where the tilde (~) should go when creating a numeric tail for the
 *  name.  The answer is always somewhere in the range 0-6, because the
 *  tilde is always followed by at least one digit (and as many as three).
 *
 *  Note that we are starting with the hopeful assumption that the tail
 *  will require only one digit.  If it requires two or three however, the
 *  code in CheckNumericTail and GenerateNumericTail may have to back up
 *  to insure the entire tail still fits within the 0-7 range.
 *
 *  ENTRY
 *      pdi - pointer to DIRINFO structure
 *
 *  EXIT
 *      None
 */

void InitNumericTail(PDIRINFO pdi)
{
    int i, iSpace, cSpaces;

    // We must walk forward through the single-byte character
    // string, counting how many sequential spaces we have encountered,
    // so that we can determine where the ideal spot for a numeric
    // tail would be.

    for (i=0,cSpaces=0; i<=6; i++) {
        if (cSpaces == 0)
            iSpace = i;
        if (IsDBCSLeadByte(pdi->di_achOEM[i])) {
            i++;
            cSpaces = 0;
            continue;
        }
        if (pdi->di_achOEM[i] != ' ') {
            cSpaces = 0;
            continue;
        }
        cSpaces++;
    }

    pdi->di_bNumericTail = (BYTE)iSpace;
}


/*  CheckNumericTail - check 8.3 name for numeric tail
 *
 *  This function examines the current directory entry, determines if
 *  it contains a numeric tail that could conflict with an auto-generated
 *  numeric tail, and flags it in our bit-array if so.
 *
 *  ENTRY
 *      pdi - pointer to DIRINFO structure
 *      pdwBitArray - pointer to numeric tail bit array
 *
 *  EXIT
 *      None
 */

void CheckNumericTail(PDIRINFO pdi, PDWORD pdwBitArray)
{
    int i, iTilde, cDigits, iNameLen;
    PCHAR pchComp;
    BYTE achComp[OEMNAMESIZE];

    ASSERT(pdi->di_bNumericTail <= 6);

    // We must walk forward through the single-byte character
    // string, and remember the position of the last tilde seen.

    iTilde = -1;
    for (i=0,pchComp=pdi->di_pde->de_name; i<=7; i++) {

        if (IsDBCSLeadByte(pchComp[i])) {
            i++;
            iTilde = -1;        // a DBCS char is never part of a tail
            continue;
        }

        if (pchComp[i] == '~') {
            if (i > pdi->di_bNumericTail || i < pdi->di_bNumericTail-2)
                iTilde = -1;    // this filename's tail is out of bounds
            else {
                iTilde = i;
                cDigits = 0;
            }
            continue;
        }

        // cDigits, if used, will be validated when we call pchtoi(),
        // so there's no need to verify this character really is a DIGIT.

        if (iTilde != -1 && pchComp[i] != ' ')
            cDigits++;
    }

    if (iTilde == -1 || cDigits == 0) {

        // This name doesn't have a tail, so if we've already seen
        // a match on the non-tail form, then we don't need to go any
        // further.

        if (TestBitArray(pdwBitArray, 0)) {
            DEBUGMSGBREAK(!memcmp(pdi->di_achOEM, pdi->di_pde->de_name, sizeof(pdi->di_achOEM)), (DBGTEXT("FATFS!CheckNumericTail: non-tail form already exists!\r\n")));
            return;
        }
        pchComp = pdi->di_achOEM;
        cDigits = 0;
    }
    else {

        // This name DOES have a tail...

        memcpy(achComp, pdi->di_achOEM, sizeof(achComp));
        memcpy(achComp+iTilde, pdi->di_pde->de_name+iTilde, cDigits+1);
        // Fill it with spaces if the name length is less than 8
        iNameLen = iTilde + cDigits + 1;
        if (iNameLen < 8)
        {
            memset(achComp + iNameLen, ' ', 8 - iNameLen);
        }
        pchComp = achComp;

        // If the previous char is also a ~, then copy that as well
        if (iTilde && pdi->di_pde->de_name[iTilde-1] == '~') {
            achComp[iTilde-1] = '~';
        }        
    }

    if (memcmp(pchComp, pdi->di_pde->de_name, sizeof(achComp)) == 0) {

        int v = 0;

        if (cDigits) {

            // Note that pchtoi() is unlike a typical atoi() function
            // in that it doesn't simply return a converted integer
            // when it reaches the first non-numeric character;  if you
            // tell it there should be cDigits digits, it INSISTS on
            // that many digits.

            v = pchtoi(pchComp+iTilde+1, cDigits);

            // Check for "~0" (which we neither track nor generate)
            // and non-numeric sequences (which pchtoi returns as -1).

            if (v <= 0)
                return;
        }

        // The directory is inconsistent if we find the same numeric
        // tail more than once!

        DEBUGMSGBREAK(TestBitArray(pdwBitArray, v), (DBGTEXT("FATFS!CheckNumericTail: %d already exists!\r\n"), v));

        SetBitArray(pdwBitArray, v);
    }
}


/*  GenerateNumericTail - generate an 8.3 name with numeric tail
 *
 *  ENTRY
 *      pdi - pointer to DIRINFO structure
 *      pdwBitArray - pointer to numeric tail bit array
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      Bit 0 of pdwBitArray corresponds to the unmodified/truncated 8.3
 *      version of the long filename, and bits 1-N correspond to names with
 *      numeric tails "~1" through "~N".  A SET bit means that short name
 *      tail is in use, a CLEAR bit means it is available.
 */

void GenerateNumericTail(PDIRINFO pdi, PDWORD pdwBitArray)
{
    if (!(pdi->di_flags & DIRINFO_OEM)) {

        // Compare the number of SET bits to the TOTAL number of
        // available bits;  we add 1 to SET bits in case bit 0 is clear,
        // because we're not currently interested in using that case.

        if (pdwBitArray[1]+1 >= pdwBitArray[0]) {

            // All the possibilities are in use, so just zero achOEM.

            pdi->di_achOEM[0] = 0;
        }
        else {

            int v;

            // We know there's at least TWO clear bits (one of which may
            // be bit 0).  Find the next clear bit and create the corresponding
            // numeric tail.

            for (v=1; v < (int)pdwBitArray[0]; v++) {

                if (!TestBitArray(pdwBitArray, v)) {

                    int i = pdi->di_bNumericTail;

                    if (v > 9) {
                        if (v <= 99) {
                            if (i > 5)
                                i = 5;
                        }
                        else if (v <= 999) {
                            if (i > 4)
                                i = 4;
                        }
                        else {
                            DEBUGMSGBREAK(TRUE, (DBGTEXT("FATFS!GenerateNumericTail: bit array too large! (%d)\r\n"), pdwBitArray[0]));
                            break;
                        }
                    }

                    // Make sure the ~ is not part of a DBCS char
                    if (i && IsUsedAsLeadByte(pdi->di_achOEM, i-1)) {
                        pdi->di_achOEM[i-1] = '~';
                    }
                    pdi->di_achOEM[i] = '~';
                    i += itopch(v, &pdi->di_achOEM[i+1]) + 1;

                    // Fill it with spaces if the name length is less than 8
                    if (i < 8)
                    {
                        memset(&pdi->di_achOEM[i], ' ', 8 - i);
                    }
                    break;
                }
            }
        }
    }
}

/*  IsUsedAsLeadByte
 *
 *  NOTES
 *      The API IsDBCSLeadByte determines if the byte is in the valid
 *      character range to be a lead byte.  However, it can also be a trailing byte.  
 *      To determine if it is actually being used as a lead byte, count the 
 *      number of consecutive lead byte chars at the index going backwards 
 *      and if the number is odd, then it is actually a lead byte.
 */
BOOL IsUsedAsLeadByte (char string[], int index)
{
    DWORD dwDBCSBytes = 0;
    int i;

    for (i = index; i >= 0; i--) {
        if (IsDBCSLeadByte(string[i])) {
            dwDBCSBytes++;
        } else {
            break;
        }
    }

    return ((dwDBCSBytes % 2) != 0);
}


/*  OEMToUniName
 *
 *  ENTRY
 *      pwsName - UNICODE buffer
 *      pchOEM - 11-byte OEM name
 *
 *  EXIT
 *      Number of UNICODE chars copied
 */

int OEMToUniName(PWSTR pws, PCHAR pchOEM, UINT nCodePage )
{
    BYTE    *p;
    int     cBytes;
    int     cConverted;
    int     cCopied = 0;

    // Count the characters in the name part.

    p = pchOEM;
    cBytes = 0;

    // Look until 8 bytes or a space

    while ( ( cBytes < 8 ) && ( *p != ' ' ) ) {
        if ( IsDBCSLeadByte(*p) ) {
            cBytes += 2;
            p += 2;
        }
        else {
            cBytes += 1;
            p += 1;
        }
    }

    // Paranoia.  We shouldn't ever see a DBCS lead byte as the 8th byte.

    if ( cBytes > 8 ) {
        ERRORMSG(1, (TEXT("fatfs/name.c: Saw DBCS lead byte in 8th byte.\r\n")));
    }

    if ( cBytes ) {
        cConverted = MultiByteToWideChar(
                                        nCodePage,
                                        MB_PRECOMPOSED,
                                        pchOEM,
                                        cBytes,
                                        pws,
                                        8);
        pws += cConverted;
        cCopied += cConverted;
    }

    // Count the characters in the extension part.
    p = pchOEM+8;
    cBytes = 0;

    // Look until 3 bytes or a space

    while ( ( cBytes < 3 ) && ( *p != ' ' ) ) {
        if ( IsDBCSLeadByte(*p) ) {
            cBytes += 2;
            p += 2;
        }
        else {
            cBytes += 1;
            p += 1;
        }
    }

    // Paranoia.  We shouldn't ever see a DBCS lead byte as the 3rd byte.

    if ( cBytes > 3 ) {
        ERRORMSG(1, (TEXT("fatfs/name.c: Saw DBCS lead byte in 3rd byte.\r\n")));
    }

    if ( cBytes ) {
        *pws++ = TEXTW('.');
        cCopied++;
        cConverted = MultiByteToWideChar(
                                        nCodePage,
                                        MB_PRECOMPOSED,
                                        pchOEM+8,
                                        cBytes,
                                        pws,
                                        3);
        pws += cConverted;
        cCopied += cConverted;
    }

    *pws = 0;
    return cCopied;
}















































BOOL UniToOEMName(PVOLUME pvol, PCHAR pchOEM, PCWSTR pwsName, int cwName, UINT nCodePage)
{
    WCHAR oc, wc;
    PCHAR pch;
    PCWCH pwcLastDot;
    int cbElement, cDots = 0;
    BOOL fLeadingDots, fReturn = 1, fUncertain = FALSE;

    pwcLastDot = wcsrchr(pwsName, '.');

  restart:
    pch = pchOEM;
    cbElement = 8;
    fLeadingDots = FALSE;
    memset(pchOEM, ' ', OEMNAMESIZE);

    for (; cwName ; cwName--) {

        oc = *pwsName++;

        // Certain characters that are not allowed in ANY names

        if ((oc < 32)   || (oc == '\\') || (oc == '/') ||
            (oc == '>') || (oc == '<')  || (oc == ':') ||
            (oc == '"') || (oc == '|')) {
            fReturn = -2;
            break;
        }

        // The rest of this does not apply if we've seen wildcards before

        if (fReturn < 0)
            continue;

        // Check for wildcards

        if (oc == '?' || oc == '*') {
            fReturn = -1;
            continue;
        }

        // Certain other characters are not allowed in 8.3 names;
        // NOTE that SPACE is normally allowed, but since Win95 prefers
        // to avoid it in auto-generated 8.3 names, we avoid it as well.

        if (oc == ' ') {
            fReturn = 0;
            continue;                   // avoid spaces in 8.3 names altogether
        }

        if (oc == '+' || oc == ',' || oc == ';' || oc == '=' || oc == '[' || oc == ']') {
            fReturn = 0;
            wc = TEXTW('_');            // map these to underscores in the 8.3 name
        }
        else
            wc = towupper(oc);          // could be an alpha char, so upper-case it

        // Lower-case requires UNICODE for creation, but as long as the name
        // is otherwise 8.3-conformant, it's perfectly good for 8.3 searches;
        // hence, the special return code.

        if (fReturn == 1 && wc != oc)
            fReturn = 2;

        // Perform "dot" validation and transitioning from the 8-part to the 3-part.

        if (wc == '.') {

            // Up to two leading dots are allowed

            if (++cDots <= 2 && cDots == 9-cbElement) {
                fLeadingDots = TRUE;
                goto copy;
            }

            // More than one dot anywhere else is not

            if (cDots > 1)
                fReturn = 0;

            // If this is the last dot, start storing at the extension

            if (pwsName > pwcLastDot) {
                pch = pchOEM + 8;       // point to extension
                cbElement = 3;
            }
            continue;
        }

        // We have a non-dot character, and we have already copied some
        // leading dots to the OEM buffer.  Since leading dots are allowed
        // in "." and ".." entries only, we must start the transfer over,
        // beginning with the current non-dot character.

        if (fLeadingDots) {
            fReturn = 0;
            pwsName--;
            goto restart;
        }

        // Disallow chars > 0x7f in order to force LFN generation, since the LFN 
        // would be in UNICODE and hence more portable;  This option is registry-settable
        // for backwards compatibility (it effectively disables shortname generation 
        // for DBCS (and consequently also makes our shortname generation inconsistent 
        // with Win95, resulting in spurious SCANDSKW errors).

        if (((pvol->v_flFATFS & FATFS_LFN_EXTENDED) && (wc > 0x7f)) || (cbElement == 0)) {
            fReturn = 0;
            continue;
        }

      copy:
        {
            int cBytes;
            BOOL fLossy;
            BYTE OEMCharBuf[2];
            CONST BYTE DefCharBuf[2] = {OEM_REPL_CHAR, 0};

            // Need to convert from Unicode to multibyte and then check for enough space

            cBytes = WideCharToMultiByte(
                                        nCodePage,
                                        0,      //REVIEW: do something special for unmapped/composite?
                                        &wc,
                                        1,
                                        &OEMCharBuf[0],
                                        2,
                                        &DefCharBuf[0],
                                        &fLossy);

            if ( cBytes && ( cBytes <= cbElement ) ) {
                int i;
                cbElement -= cBytes;
                for ( i = 0; i < cBytes; i++ ) {
                    if ((pch == pchOEM) && (OEMCharBuf[i] == DIRFREE)) {
                        // First char in de_name cannot be DIRFREE (0xe5)
                        *pch++ = OEM_REPL_CHAR;
                    } else {
                        *pch++ = OEMCharBuf[i];
                    }
                }
                if (fLossy || cBytes > 1)
                    fUncertain = TRUE;
            }
            else {
                cbElement = 0;
                fReturn = 0;
            }
        }
    }

    if (fReturn >= 1 && fUncertain)
        fReturn = 3;

    return fReturn;
}

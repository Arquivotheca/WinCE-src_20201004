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

#include "cnnclpth.h"

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

VOID
InitPath(
    PPATH pPath
    )
{
    PREFAST_DEBUGCHK(pPath);

    pPath->ulPathDepth = 0;
    pPath->ulPathLength = 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

VOID
PushPathNode(
    PPATH pPath,
    PATH_NODE PathNode
    )
{
    PREFAST_DEBUGCHK(pPath);

    // Push path on stack
    pPath->PathNodeStack[pPath->ulPathDepth] = PathNode;
    // Increment path depth
    pPath->ulPathDepth += 1;
    // Increment path length
    pPath->ulPathLength += PathNode.NameLength; // Don't include '\'
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

VOID
PopPathNode(
    PPATH pPath
    )
{
    PREFAST_DEBUGCHK(pPath);

    // Can't ascend if path is empty
    if (pPath->ulPathDepth == 0) return;
    // Decrement path length; Pop path node (uiPathDepth indexes next free stack slot)
    pPath->ulPathLength -= pPath->PathNodeStack[--pPath->ulPathDepth].NameLength; // Don't include '\'
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

ULONG
GetPathLength(
    PPATH pPath
    )
{
    ULONG cchPath = 0;

    PREFAST_DEBUGCHK(pPath);

    switch (pPath->PathType) {
        case PT_STANDARD:
            cchPath += 1;
            break;
        case PT_UNC:
            cchPath += 2;
            break;
    }
    cchPath += pPath->ulPathLength;
    if (pPath->ulPathDepth > 0) {
        cchPath += (pPath->ulPathDepth - 1); // Account for '\'-s
    }
    // Account for '\0'
    cchPath += 1;
    return cchPath;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

ULONG
PopPath(
    PPATH pPath,
    LPCTSTR lpszIn,
    LPTSTR lpszOut
    )
{
    ULONG ulCurPathNameIndex, ulCurPathDepth = 0, ulOutIndex = 0, cchOut = 0;

    PREFAST_DEBUGCHK(pPath);
    PREFAST_DEBUGCHK(lpszIn);
    PREFAST_DEBUGCHK(lpszOut);

    switch (pPath->PathType) {
        case PT_STANDARD:
            lpszOut[ulOutIndex++] = L'\\';
            cchOut += 1;
            break;
        case PT_UNC:
            lpszOut[ulOutIndex++] = L'\\';
            lpszOut[ulOutIndex++] = L'\\';
            cchOut += 2;
            break;
    }
    // Fill lpszOut; Skip if path empty
    while (ulCurPathDepth < pPath->ulPathDepth) {
        ulCurPathNameIndex = pPath->PathNodeStack[ulCurPathDepth].PathIndex;
        while (pPath->PathNodeStack[ulCurPathDepth].NameLength-- > 0) {
            lpszOut[ulOutIndex++] = lpszIn[ulCurPathNameIndex++];
            cchOut += 1;
        }
        if (++ulCurPathDepth != pPath->ulPathDepth) {
            lpszOut[ulOutIndex++] = L'\\';
            cchOut += 1;
        }
    }
    // Append null terminator
    lpszOut[ulOutIndex] = L'\0';
    cchOut += 1;
    return cchOut;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

VOID
GetPathNodeType(
    LPCTSTR lpszIn,
    ULONG cchIn,
    PPATH_NODE_TYPE pPathNodeType
    )
{
    PREFAST_DEBUGCHK(lpszIn);
    PREFAST_DEBUGCHK(pPathNodeType);

    // Test for self
    if ((cchIn == 1) && (IS_PERIOD(lpszIn[0]))) {
        *pPathNodeType = PNT_SELF;
    }
    // Test for parent
    else if ((cchIn == 2) && (IS_PERIOD(lpszIn[0])) && (IS_PERIOD(lpszIn[1]))) {
        *pPathNodeType = PNT_PARENT;
    }
    // Do not validate directory names
    else {
        *pPathNodeType = PNT_FILE;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

VOID
GetPathType(
    LPCTSTR lpszIn,
    ULONG cchIn,
    PPATH_TYPE pPathType
    )
{
    PREFAST_DEBUGCHK(lpszIn);
    PREFAST_DEBUGCHK(pPathType);

    if (cchIn >= 2) {
        if ((IS_BSLASH(lpszIn[0]) && IS_BSLASH(lpszIn[1])) ||
            (IS_FSLASH(lpszIn[0]) && IS_FSLASH(lpszIn[1]))
            )
        {
            if (cchIn > 2) {
                if (!IS_SLASH(lpszIn[2])) {
                    *pPathType = PT_UNC;
                    return;
                }
            }
            else {
                *pPathType = PT_UNC;
                return;
            }
        }
    }
    *pPathType = PT_STANDARD;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

WORD
GetIndexOfNextDirectoryInPath(
    LPCTSTR lpszIn,
    ULONG cchIn
    )
{
    BOOL fExists = FALSE;
    WORD Index = 0;

    PREFAST_DEBUGCHK(lpszIn);

    // Find 0-index of first '\\' or '/'
    while (cchIn-- > 0) {
        if (IS_SLASH(lpszIn[Index])) {
            fExists = TRUE;
            break;
        }
        Index += 1;
    }
    return (fExists ? Index : 0);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

WORD
GetIndexOfNextPathNodeInPath(
    LPCTSTR lpszIn,
    PULONG pcchIn
    )
{
    WORD Index = 0;

    PREFAST_DEBUGCHK(lpszIn);
    PREFAST_DEBUGCHK(pcchIn);

    while ((*pcchIn > 0) && (IS_SLASH(lpszIn[Index]))) {
        Index += 1;
        *pcchIn -= 1;
    }
    return Index;
}

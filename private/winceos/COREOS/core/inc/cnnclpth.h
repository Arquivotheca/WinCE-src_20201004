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

#ifndef __CNNCLPTH_H
#define __CNNCLPTH_H

#include <windows.h>
#include <strsafe.h>

#define IS_PERIOD(c) (c == L'.')
#define IS_FSLASH(c) (c == L'/')
#define IS_BSLASH(c) (c == L'\\')
#define IS_SLASH(c)  (IS_BSLASH(c) || IS_FSLASH(c))

// In the context of path name canonicalization, a "path" is string of the
// form: (\|\\)((<path node>\)*(<path node>))?.  Where a <path node> is either:
// a directory/file name, a self directory reference, i.e., ".", or a parent
// directory reference, i.e., "..".
//
// Since Windows CE does not support the concept of a relative path, all
// paths are absolute.  A Windows CE path is either explicitly absolute, i.e.,
// contains a leading '\', implicitly absolute, i.e., does not contain a
// leading '\', or a UNC path, i.e., starts with "\\".

// Path node type
typedef enum _PATH_NODE_TYPE {
    PNT_UNKNOWN = 0,                   // unknown path node
    PNT_SELF = 1,                      // self directory reference, i.e., "."
    PNT_PARENT = 2,                    // parent directory reference, i.e., ".."
    PNT_FILE = 3,                      // directory/file name
} PATH_NODE_TYPE, * PPATH_NODE_TYPE;

// Path node
// The PATH_NODE structure is packed into 32 bits. Both PathIndex and NameLength are bounded
// at MAX_PATH (260), which requires only 9 bits. We use 16 bits for PathIndex and 12 bits
// for NameLength to preserve alignment (rather than 14 and 14). The remaining 4 bits store
// a PATH_NODE_TYPE value, which has a max value of 3.
typedef struct _PATH_NODE {
    WORD           PathIndex;           // index(first character(path node name)) in path
    WORD           NameLength : 12;     // length(path node name)
    WORD           PathNodeType : 4;    // path node type
} PATH_NODE, * PPATH_NODE;

// Path type
typedef enum _PATH_TYPE {
    PT_STANDARD = 0,                   // implicitly/explicitly absolute
    PT_UNC                             // UNC
} PATH_TYPE, * PPATH_TYPE;

// Path object
// The upper bound on path elements is ceiling(MAX_PATH / 2) because each path item consists of
// a slash and at least one other character.
typedef struct _PATH {
    PATH_NODE PathNodeStack[(MAX_PATH + 1) / 2]; // path traversal, i.e., Push=Descend, Pop=Ascend
    ULONG     ulPathDepth;             // current (maximum) depth of path (and index next free stack slot)
    ULONG     ulPathLength;            // current path length
    PATH_TYPE PathType;                // path type
} PATH, * PPATH;

// Initialize path object
VOID
InitPath(
    PPATH pPath
    );

// Descend in path
VOID
PushPathNode(
    PPATH     pPath,
    PATH_NODE PathNode
    );

// Ascend in path
VOID
PopPathNode(
    PPATH pPath
    );

// Return current path string length (including zero-terminator)
ULONG
GetPathLength(
    PPATH pPath
    );

// Pop entire path off of stack and return length of path
ULONG
PopPath(
    PPATH   pPath,
    LPCTSTR lpszIn,
    LPTSTR  lpszOut
    );

// Return path node type of path node
VOID
GetPathNodeType(
    LPCTSTR         lpszIn,
    ULONG           cchIn,
    PPATH_NODE_TYPE pPathNodeType
    );

// Return path type of path
VOID
GetPathType(
    LPCTSTR    lpszIn,
    ULONG      cchIn,
    PPATH_TYPE pPathType
    );

// Return 0-index of first '\\' or '/'
WORD
GetIndexOfNextDirectoryInPath(
    LPCTSTR lpszIn,
    ULONG   cchIn
    );

// Return 0-index of first non-'\\' or non-'/' and subtract from cbIn
WORD
GetIndexOfNextPathNodeInPath(
    LPCTSTR lpszIn,
    PULONG  pcchIn
    );

#endif // __CNNCLPTH_H

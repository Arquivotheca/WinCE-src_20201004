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
#ifndef __MEMTRACK_H__
#define __MEMTRACK_H__ 1

#define REGTYPE_INUSE       0x00000001
#define REGTYPE_MASKED      0x00000002

typedef struct _tag_MEMTR_FILTERINFO {
    DWORD  dwFlags;
    DWORD  dwProcID;
} MEMTR_FILTERINFO, *LPMEMTR_FILTERINFO;

#define MAX_REGTYPES    32
#define MAX_REGTYPENAMESIZE 16
typedef struct _tag_MEMTR_ITEMTYPE {
    DWORD  dwFlags;
    WCHAR  szName[MAX_REGTYPENAMESIZE];
} MEMTR_ITEMTYPE, *LPMEMTR_ITEMTYPE;

#define NUM_TRACK_STACK_FRAMES 10

#define ITEM_SHOWN   (DWORD)0x00000001
#define ITEM_DELETED (DWORD)0x00000002
  
typedef struct _tag_TRACK_NODE {
    HANDLE handle;
    TRACKER_CALLBACK cb;
    HANDLE hProc;
    DWORD dwType;
    DWORD dwProcID;
    DWORD dwSize;                               
    DWORD dw1;
    DWORD dw2;
    DWORD Frames[NUM_TRACK_STACK_FRAMES];
    DWORD dwTime;
    DWORD dwFlags;
    struct _tag_TRACK_NODE *pnNext;                 // for sequential
    struct _tag_TRACK_NODE *pnPrev;                 // for sequential
} Track_Node, *pTrack_Node;

BOOL MEMTR_init(void);
BOOL MEMTR_deinit(void);
int MEMTR_hash(DWORD dwType, HANDLE handle);
void MEMTR_printnode(DWORD dwFlags, pTrack_Node pn);
BOOL MEMTR_GetStackFrames(DWORD lpFrames[]);
void MEMTR_deletenode(pTrack_Node pn);

#endif


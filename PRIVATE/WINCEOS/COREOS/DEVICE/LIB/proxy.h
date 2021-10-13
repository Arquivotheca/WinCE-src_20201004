//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef _proxy_h
#define _proxy_h

// Functions available from proxy.c module.
extern BOOL proxy_init(int slot_count, int buffer_size);
extern HANDLE CreateAPIHandle(HANDLE hDevFileApiHandle, fsopendev_t *lpopendev);
extern void wait_for_exit_event(void);
extern BOOL check_wceemuld(void);

#endif // _proxy_h


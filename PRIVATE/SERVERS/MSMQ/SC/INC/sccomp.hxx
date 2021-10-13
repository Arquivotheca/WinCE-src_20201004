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

    sccomp.hxx

Abstract:

    Misc unsigned comparison funcs


  Important note about lock hierarchy:
		QueueManager -> SessionManager -> Session -> Main Queue(s) -> Dead letter/journal(s) -> ACK/NACK queue

--*/
#if ! defined (__sccomp_HXX__)
#define __sccomp_HXX__	1

static inline int did_not_expire (unsigned int uiTime, unsigned int uiNow) {
	return (uiTime == INFINITE) || (((int)uiTime - (int)uiNow) >= 0);
}

static inline int did_expire (unsigned int uiTime, unsigned int uiNow) {
	return (uiTime != INFINITE) && (((int)uiTime - (int)uiNow) < 0);
}

static inline int time_less_equal (unsigned int t1, unsigned int t2) {
	return (int)t1 - (int)t2 <= 0;
}

static inline int time_greater_equal (unsigned int t1, unsigned int t2) {
	return (int)t1 - (int)t2 >= 0;
}

static inline int time_less (unsigned int t1, unsigned int t2) {
	return (int)t1 - (int)t2 < 0;
}

static inline int time_greater (unsigned int t1, unsigned int t2) {
	return (int)t1 - (int)t2 > 0;
}

static inline int sessnum_greater (unsigned short a, unsigned short b) {
	return (signed short)((signed short)a - (signed short)b) > 0;
}

static inline int sessnum_greater_equal (unsigned short a, unsigned short b) {
	return (signed short)((signed short)a - (signed short)b) >= 0;
}

static inline int sessnum_less (unsigned short a, unsigned short b) {
	return (signed short)((signed short)a - (signed short)b) < 0;
}

static inline int sessnum_less_equal (unsigned short a, unsigned short b) {
	return (signed short)((signed short)a - (signed short)b) <= 0;
}

#endif
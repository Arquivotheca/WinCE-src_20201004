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
/***
*ehstate.h - exception handling state management declarations
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       EH State management declarations.  Does target-dependent definitions.
*
*       Macros defined:
*
*       GetCurrentState - determines current state (may call function)
*       SetState - sets current state to specified value (may call function)
*
*       [Internal]
*
*Revision History:
*       05-21-93  BS    Module created.
*       03-03-94  TL    Added Mips (_M_MRX000 >= 4000) changes 
*       09-02-94  SKS   This header file added.
*       09-13-94  GJF   Merged in changes from/for DEC Alpha (from Al Doser,
*                       dated 6/20).
*       12-15-94  XY    merged with mac header
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       06-01-97  TL    Added P7 changes 
*       05-17-99  PML   Remove all Macintosh support.
*       06-05-01  GB    AMD64 Eh support Added.
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       11-22-02  GB    Added support for byte states on X86
*
****/

#pragma once

#ifndef _INC_EHSTATE
#define _INC_EHSTATE

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#if defined(_M_X64) /*IFSTRIP=IGN*/
extern __ehstate_t __GetCurrentState(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern __ehstate_t __GetUnwindState(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern VOID        __SetState(EHRegistrationNode*, DispatcherContext*, FuncInfo*, __ehstate_t); 
extern VOID        __SetUnwindTryBlock(EHRegistrationNode*, DispatcherContext*, FuncInfo*, INT); 
extern INT         __GetUnwindTryBlock(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern __ehstate_t __StateFromControlPc(FuncInfo*, DispatcherContext*);
extern __ehstate_t __StateFromIp(FuncInfo*, DispatcherContext*, __int64);

#elif   defined(_M_ARM) /*IFSTRIP=IGN*/

extern __ehstate_t __GetCurrentState(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern __ehstate_t __GetUnwindState(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern VOID        __SetState(EHRegistrationNode*, DispatcherContext*, FuncInfo*, __ehstate_t); 
extern VOID        __SetUnwindTryBlock(EHRegistrationNode*, DispatcherContext*, FuncInfo*, INT); 
extern INT         __GetUnwindTryBlock(EHRegistrationNode*, DispatcherContext*, FuncInfo*); 
extern __ehstate_t __StateFromControlPc(FuncInfo*, DispatcherContext*);
extern __ehstate_t __StateFromIp(FuncInfo*, DispatcherContext*, __int32);

#elif   _M_IX86 >= 300 /*IFSTRIP=IGN*/

//
// In the initial implementation, the state is simply stored in the 
// registration node.
//

//Added support for byte states when max state <= 128. Note that max state is 1+real max state
#define GetCurrentState( pRN, pDC, pFuncInfo )  (((pFuncInfo)->maxState<=128)?((__ehstate_t)(signed char)((pRN)->state&0xff)):(pRN)->state)

#define SetState( pRN, pDC, pFuncInfo, newState )       (pRN->state = newState)

#else
#error "State management unknown for this platform "
#endif

#endif  /* _INC_EHSTATE */


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
/*
 File:      dbgmacro.h

 Purpose:   provide DBG subsystem macros private to debug subsystem.

 */

#ifndef __DBGMACRO_H__
#define __DBGMACRO_H__

#define GET_DBGARG_ADDRESS(x)   (x)->Addr
#define GET_DBGARG_BUF(x)   (x)->Buf
#define GET_DBGARG_SIZE(x)  (x)->Size
#define GET_DBGARG_THRD(x)  (x)->pThd
#define GET_DBGARG_PRC(x)   (x)->pPrc

#define SET_DBGARG_ADDRESS(x,y) ((x)->Addr) = (y)
#define SET_DBGARG_BUF(x,y) ((x)->Buf)  = (y)
#define SET_DBGARG_SIZE(x,y)    ((x)->Size) = (y)
#define SET_DBGARG_THRD(x,y)    ((x)->pThd) = (y)
#define SET_DBGARG_PRC(x,y) ((x)->pPrc) = (y)


#define GET_DBGXBUF_LENGTH(x)       (x)->Length
#define GET_DBGXBUF_MAXLENGTH(x)    (x)->MaxLength
#define GET_DBGXBUF_BUFFER(x)       (x)->Buffer

#define SET_DBGXBUF_LENGTH(x,y)     ((x)->Length)   = (y)
#define SET_DBGXBUF_MAXLENGTH(x,y)  ((x)->MaxLength)= (y)
#define SET_DBGXBUF_BUFFER(x,y)     ((x)->Buffer)   = (y)


#define GET_DBGHEAD_F_ISCONNECTED(x)    (x)->fIsConnected
#define GET_DBGHEAD_TRANSPORTTYPE(x)    (x)->TransportType
#define GET_DBGHEAD_PDBGARGS(x)     (x)->pArgs
#define GET_DBGHEAD_DBGXBUF(x)      (x)->XBuffer

#define SET_DBGHEAD_F_ISCONNECTED(x,y)  ((x)->fIsConnected) = (y)
#define SET_DBGHEAD_TRANSPORTTYPE(x,y)  ((x)->TransportType)= (y)
#define SET_DBGHEAD_PDBGARGS(x,y)   ((x)->Args)     = (y)
#define SET_DBGHEAD_DBGXBUF(x,y)    ((x)->XBuffer)      = (y)

#define IS_BIGGER_THAN_PAGE(x)  (x&0xFFFF000)
#define GET_PICONTEXT(x) (&((x)->ctx))

#endif /* __DBGMACRO_H__ */

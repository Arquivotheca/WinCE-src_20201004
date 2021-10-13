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
*except.hpp - Declaration of CSysException class for NT kernel mode
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Implementation of kernel mode default exception
*
*       Entry points:
*           CException
*
*Revision History:
*       04-21-95  DAK   Module created
*       04-07-04  MSL   Double slash removal
*
****/

#ifndef _INC_CSYSEXCEPT_KERNEL_
#define _INC_CSYSEXCEPT_KERNEL_

/*-------------------------------------------------------------------------

  Class:      CException

  Purpose:    All exception objects (e.g. objects that are THROW)
              inherit from CException.

  History:    21-Apr-95   DwightKr    Created.

--------------------------------------------------------------------------*/

class CException
{
public:
             CException(long lError) : _lError(lError), _dbgInfo(0) {}
    long     GetErrorCode() { return _lError;}
    void     SetInfo(unsigned dbgInfo) { _dbgInfo = dbgInfo; }
    unsigned long GetInfo(void) const { return _dbgInfo; }

protected:

    long          _lError;
    unsigned long _dbgInfo;
};

#endif      /* _INC_CSYSEXCEPT_KERNEL_ */

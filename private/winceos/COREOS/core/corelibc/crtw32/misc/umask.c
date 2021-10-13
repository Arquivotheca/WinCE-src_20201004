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
*umask.c - set file permission mask
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _umask() - sets file permission mask of current process*
*	affecting files created by creat, open, or sopen.
*
*Revision History:
*	06-02-89  PHG	module created
*	03-16-90  GJF	Made calling type _CALLTYPE1, added #include
*			<cruntime.h> and fixed the copyright. Also, cleaned
*			up the formatting a bit.
*	04-05-90  GJF	Added #include <io.h>.
*	10-04-90  GJF	New-style function declarator.
*	01-17-91  GJF	ANSI naming.
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	09-11-03  SJ	Secure CRT Work - Assertions & Validations
*	10-20-03  SJ	errno shouldn't be set to 0 on success
*	04-15-04  AC	silently ignore non-Windows modes in _umask
*	            	VSW#285400
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <io.h>
#include <sys\stat.h>

/***
*errno_t _umask(mode, poldmode) - set the file mode mask
*
*Purpose : 
*    Works similiar to umask except it validates input params.
*
*
*******************************************************************************/

errno_t __cdecl _umask_s (
	int mode, int * poldmode
	)
{
	_VALIDATE_RETURN_ERRCODE((poldmode != NULL), EINVAL);
	*poldmode = _umaskval;
	_VALIDATE_RETURN_ERRCODE(((mode & ~(_S_IREAD | _S_IWRITE)) == 0), EINVAL);

	/* only user read/write permitted */
	mode &= (_S_IREAD | _S_IWRITE);	
	_umaskval = mode;
	return 0;
}

/***
*int _umask(mode) - set the file mode mask
*
*Purpose:
*	Sets the file-permission mask of the current process* which
*	modifies the permission setting of new files created by creat,
*	open, or sopen.
*
*Entry:
*	int mode - new file permission mask
*		   may contain _S_IWRITE, _S_IREAD, _S_IWRITE | _S_IREAD.
*		   The S_IREAD bit has no effect under Win32
*
*Exit:
*	returns the previous setting of the file permission mask.
*
*Exceptions:
*
*******************************************************************************/
int __cdecl _umask (
	int mode
	)
{
	int oldmode = 0;

    /* silently ignore non-Windows modes */
	mode &= (_S_IREAD | _S_IWRITE);	

	_umask_s(mode, &oldmode);
	return oldmode;
}

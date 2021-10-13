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
*strerror.c - Contains the strerror C runtime.
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	The strerror runtime accepts an error number as input
*	and returns the corresponding error string.
*
*	NOTE: The "old" strerror C runtime resides in file _strerr.c
*	and is now called _strerror.  The new strerror runtime
*	conforms to the ANSI standard.
*
*Revision History:
*	02-24-87  JCR	Module created.
*	12-11-87  JCR	Added "_LOAD_DS" to declaration
*	01-04-87  JCR	Improved code.
*	01-05-87  JCR	Multi-thread support
*	05-31-88  PHG	Merge DLL and normal versions
*	06-06-89  JCR	386 mthread support
*	03-16-90  GJF	Replaced _LOAD_DS with _CALLTYPE1, added #include
*			<cruntime.h> and fixed the copyright. Also, cleaned
*			up the formatting a bit.
*	10-04-90  GJF	New-style function declarator.
*	07-18-91  GJF	Multi-thread support for Win32 [_WIN32_].
*	02-17-93  GJF	Changed for new _getptd().
*	04-06-93  SKS	Replace _CRTAPI* with __cdecl
*	09-06-94  CFW	Remove Cruiser support.
*	09-06-94  CFW	Replace MTHREAD with _MT.
*	01-10-95  CFW	Debug CRT allocs.
*	11-24-99  GB 	Added support for wide char by adding wcserror()
*	10-19-01  BWT	If we're unable to allocate space for the error message
*	             	just return string from ENOMEM (Not enough space).
*	12-12-01  BWT	Replace getptd with getptd_noexit - return not enough
*	             	space if we can't retrieve the ptd - don't exit.
*	07-12-02  GB 	Fixed _ERRMSGLEN_ macro, should have it's value in ()
*	10-08-03  AC 	Added secure version.
*       04-01-05  MSL   Integer overflow protection
*
*******************************************************************************/

#include <cruntime.h>
#include <errmsg.h>
#include <stdlib.h>
#include <syserr.h>
#include <string.h>
#include <mtdll.h>
#include <tchar.h>
#include <malloc.h>
#include <stddef.h>
#include <dbgint.h>
#include <internal.h>

/* [NOTE: The error message buffer is shared by both strerror
   and _strerror so must be the max length of both. */
/* Max length of message = user_string(94)+system_string+2 */
#define _ERRMSGLEN_ (94+_SYS_MSGMAX+2)

#ifdef _UNICODE
#define _terrmsg    _werrmsg
#else
#define _terrmsg    _errmsg
#endif

/***
*char *strerror(errnum) - Map error number to error message string.
*
*Purpose:
*	The strerror runtime takes an error number for input and
*	returns the corresponding error message string.  This routine
*	conforms to the ANSI standard interface.
*
*Entry:
*	int errnum - Integer error number (corresponding to an errno value).
*
*Exit:
*	char * - Strerror returns a pointer to the error message string.
*	This string is internal to the strerror routine (i.e., not supplied
*	by the user).
*
*Exceptions:
*	None.
*
*******************************************************************************/

#ifdef _UNICODE
wchar_t * cdecl _wcserror(
#else
char * __cdecl strerror (
#endif
	int errnum
	)
{
	_TCHAR *errmsg;
	_ptiddata ptd = _getptd_noexit();
	if (!ptd)
		return _T("Visual C++ CRT: Not enough memory to complete call to strerror.");

	if ( (ptd->_terrmsg == NULL) && ((ptd->_terrmsg =
			_calloc_crt(_ERRMSGLEN_, sizeof(_TCHAR)))
			== NULL) )
		return _T("Visual C++ CRT: Not enough memory to complete call to strerror.");
	else
		errmsg = ptd->_terrmsg;

#ifdef _UNICODE
	_ERRCHECK(mbstowcs_s(NULL, errmsg, _ERRMSGLEN_, _get_sys_err_msg(errnum), _ERRMSGLEN_ - 1));
#else
	_ERRCHECK(strcpy_s(errmsg, _ERRMSGLEN_, _get_sys_err_msg(errnum)));
#endif

	return(errmsg);
}

/***
*errno_t strerror_s(buffer, sizeInTChars, errnum) - Map error number to error message string.
*
*Purpose:
*	The strerror_s runtime takes an error number for input and
*	copies the corresponding error message string in the destination
*	buffer. If the buffer is too small, the message is truncated.
*
*Entry:
*	TCHAR * buffer - Destination buffer.
*	size_t sizeInTChars - Size of the destination buffer.
*	int errnum - Integer error number (corresponding to an errno value).
*
*Exit:
*	The error code.
*
*Exceptions:
*	Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

#ifdef _UNICODE
errno_t __cdecl _wcserror_s(
#else
errno_t __cdecl strerror_s(
#endif
	TCHAR* buffer,
	size_t sizeInTChars,
	int errnum
	)
{
	errno_t e = 0;

	/* validation section */
	_VALIDATE_RETURN_ERRCODE(buffer != NULL, EINVAL);
	_VALIDATE_RETURN_ERRCODE(sizeInTChars > 0, EINVAL);

	/* we use mbstowcs_s or strncpy_s because we want to truncate the error string 
	 * if the destination is not big enough
	 */
#ifdef _UNICODE
	e = _ERRCHECK_EINVAL_ERANGE(mbstowcs_s(NULL, buffer, sizeInTChars, _get_sys_err_msg(errnum), _TRUNCATE));
	/* ignore the truncate information */
	if (e == STRUNCATE)
	{
		e = 0;
	}
#else
	_ERRCHECK(strncpy_s(buffer, sizeInTChars, _get_sys_err_msg(errnum), sizeInTChars - 1));
#endif
    return e;
}

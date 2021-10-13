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
// filesys.h -- filesystem support functions for Windows
// NOTE: no include guard

 #if FILESYS_WIDE
  #define TCHAR		wchar_t
  #define TFUN(x)	x##W
  #define TLIT(x)	L##x
  #define TSTRING	wstring

	// CRT functions
  #define TCHDIR	_wchdir
  #define TGETCWD	_wgetcwd
  #define TMKDIR	_wmkdir
  #define TRMDIR	_wrmdir
  #define TREMOVE	_wremove
  #define TRENAME	_wrename

 #else /* FILESYS_WIDE */
  #define TCHAR		char
  #define TFUN(x)	x##A
  #define TLIT(x)	x
  #define TSTRING	string

	// CRT functions
  #define TCHDIR	_chdir
  #define TGETCWD	_getcwd
  #define TMKDIR	_mkdir
  #define TRMDIR	_rmdir
  #define TREMOVE	::remove
  #define TRENAME	::rename
 #endif /* FILESYS_WIDE */

  #define _HAS_HARDLINKS	1
  #define _HAS_SYMLINKS		1

  #define WINAPI_PACKAGED_APP_RETURN(retval) \
	if (__crtIsPackagedApp()) \
		return (retval);

		// DIRECTORY FUNCTIONS
static TCHAR *_Strcpy(TCHAR *_Dest, const TCHAR *_Src)
	{	// copy an NTBS
	TCHAR *_Ans = _Dest;
	size_t _Left = _MAX_FILESYS_NAME - 1;
	for (; *_Src != TLIT('\0') && 0 < _Left; --_Left)
		*_Dest++ = *_Src++;
	*_Dest = TLIT('\0');
	return (_Ans);
	}

 #if FILESYS_WIDE
_FS_DLL TCHAR *__CLRCALL_PURE_OR_CDECL _Read_dir(TCHAR *_Dest, void *_Handle,
	file_type& _Ftype)
	{	// read a directory entry
	TFUN(WIN32_FIND_DATA) _Dentry;

	for (; ; )
		if (TFUN(FindNextFile)((HANDLE)_Handle, &_Dentry) == 0)
			{	// fail
			_Ftype = status_unknown;
			return (_Strcpy(_Dest, TLIT("")));
			}
		else if (_Dentry.cFileName[0] == TLIT('.')
			&& (_Dentry.cFileName[1] == TLIT('\0')
				|| _Dentry.cFileName[1] == TLIT('.')
					&& _Dentry.cFileName[2] == TLIT('\0')))
			;	// skip "." and ".."
		else
			{	// get file type and return name
			_Ftype = _Map_mode(_Dentry.dwFileAttributes);
			return (_Strcpy(_Dest, &_Dentry.cFileName[0]));
			}
	}

 #else /* FILESYS_WIDE */
static unsigned int _Filesys_code_page()
	{
  #ifdef _CORESYS
	return (CP_ACP);
  #else /* _CORESYS */
	if (__crtIsPackagedApp() || AreFileApisANSI())
		return (CP_ACP);
	else
		return (CP_OEMCP);
  #endif /* _CORESYS */
	}

static int _To_wide(const char *_Bsrc, wchar_t *_Wdest)
	{	// return nonzero on success
	return (MultiByteToWideChar(_Filesys_code_page(),
			0, _Bsrc, -1, _Wdest, _MAX_FILESYS_NAME));
	}

static int _To_byte(const wchar_t *_Wsrc, char *_Bdest)
	{	// return nonzero on success
	return (WideCharToMultiByte(_Filesys_code_page(),
			0, _Wsrc, -1, _Bdest, _MAX_FILESYS_NAME, NULL, NULL));
	}

_FS_DLL char *__CLRCALL_PURE_OR_CDECL _Read_dir(char *_Dest,
	void *_Handle, file_type& _Ftype)
	{	// read a directory entry
	wchar_t _Dest_wide[_MAX_FILESYS_NAME]; // Same size as _Dest
	_Read_dir(_Dest_wide, _Handle, _Ftype);

	// use default ANSI CP and default flags to convert to ANSI
	if (_Dest_wide[0] == L'\0'
		|| _To_byte(_Dest_wide, _Dest) == 0)
		return (_Strcpy(_Dest, ""));

	return (_Dest);
	}
 #endif /* FILESYS_WIDE */

 #if FILESYS_WIDE
_FS_DLL void *__CLRCALL_PURE_OR_CDECL _Open_dir(TCHAR *_Dest, const TCHAR *_Dirname,
	int& _Errno, file_type& _Ftype)
	{	// open a directory for reading
	TFUN(WIN32_FIND_DATA) _Dentry;
	TSTRING _Wildname(_Dirname);
	if (!_Wildname.empty())
		_Wildname.append(TLIT("\\*"));

	void *_Handle = TFUN(FindFirstFileEx)(_Wildname.c_str(),
		FindExInfoStandard, &_Dentry, FindExSearchNameMatch, NULL, 0);
	if (_Handle == INVALID_HANDLE_VALUE)
		{	// report failure
		_Errno = ERROR_BAD_PATHNAME;
		*_Dest = TLIT('\0');
		return (0);
		}
	else
		{	// success, get first directory entry
		_Errno = 0;
		if (_Dentry.cFileName[0] == TLIT('.')
			&& (_Dentry.cFileName[1] == TLIT('\0')
				|| _Dentry.cFileName[1] == TLIT('.')
					&& _Dentry.cFileName[2] == TLIT('\0')))
			{	// skip "." and ".."
			_Read_dir(_Dest, _Handle, _Ftype);
			if (_Dest[0] != TLIT('\0'))
				return (_Handle);
			else
				{	// no entries, release handle
				_Close_dir(_Handle);
				return (0);
				}
			}
		else
			{
			_Strcpy(_Dest, &_Dentry.cFileName[0]);
			_Ftype = _Map_mode(_Dentry.dwFileAttributes);
			return (_Handle);
			}
		}
	}

 #else /* FILESYS_WIDE */
_FS_DLL void *__CLRCALL_PURE_OR_CDECL _Open_dir(char *_Dest,
	const char *_Dirname, int& _Errno, file_type& _Ftype)
	{	// open a directory for reading
	wchar_t _Dest_wide[_MAX_FILESYS_NAME];
	wchar_t _Dirname_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Dirname, _Dirname_wide) == 0)
		{	// conversion to wide char failed
		_Errno = ERROR_BAD_PATHNAME;
		*_Dest = '\0';
		return (0);
		}

	void *_Handle = _Open_dir(_Dest_wide, _Dirname_wide, _Errno, _Ftype);

	// use default ANSI CP and default flags to convert dest to ANSI
	if (_Dest_wide[0] == L'\0'
		|| _To_byte(_Dest_wide, _Dest) == 0)
		*_Dest = '\0';

	return (_Handle);
	}
 #endif /* FILESYS_WIDE */

_FS_DLL TCHAR *__CLRCALL_PURE_OR_CDECL _Current_get(TCHAR *_Dest)
	{	// get current working directory
	TCHAR _Dentry[MAX_PATH];
	WINAPI_PACKAGED_APP_RETURN(_Strcpy(_Dest, TLIT("")));	// no support
	return (_Strcpy(_Dest,
		TGETCWD(&_Dentry[0], MAX_PATH) == NULL
			? TLIT("") : &_Dentry[0]));

	}

_FS_DLL bool __CLRCALL_PURE_OR_CDECL _Current_set(const TCHAR *_Dirname)
	{	// set current working directory
	WINAPI_PACKAGED_APP_RETURN(false);	// no support
	return (TCHDIR(_Dirname) == 0);

	}

_FS_DLL int __CLRCALL_PURE_OR_CDECL _Make_dir(const TCHAR *_Fname)
	{	// make a new directory
	if (TMKDIR(_Fname) != -1)
		return (1);
	else if (errno == EEXIST)
		return (0);
	else
		return (-1);

	}

_FS_DLL bool __CLRCALL_PURE_OR_CDECL _Remove_dir(const TCHAR *_Fname)
	{	// remove a directory
	return (TRMDIR(_Fname) != -1);

	}

		// FILE STATUS FUNCTIONS
 #if FILESYS_WIDE
_FS_DLL file_type __CLRCALL_PURE_OR_CDECL _Stat(const TCHAR *_Fname,
	int& _Errno)
	{	// get file status
	WIN32_FILE_ATTRIBUTE_DATA _Data;

	if (TFUN(GetFileAttributesEx)(_Fname, GetFileExInfoStandard, &_Data))
		{	// valid, return mapped status
		_Errno = 0;
		return (_Map_mode(_Data.dwFileAttributes));
		}
	else
		{	// invalid, get error code
		_Errno = GetLastError();

		if (_Errno == ERROR_BAD_NETPATH
			|| _Errno == ERROR_BAD_PATHNAME
			|| _Errno == ERROR_FILE_NOT_FOUND
			|| _Errno == ERROR_INVALID_DRIVE
			|| _Errno == ERROR_INVALID_NAME
			|| _Errno == ERROR_INVALID_PARAMETER
			|| _Errno == ERROR_PATH_NOT_FOUND)
			{	// file not found, report no error
			_Errno = 0;
			return (file_not_found);
			}
		else if (_Errno == ERROR_SHARING_VIOLATION)
			{	// sharing violation, report no error
			_Errno = 0;
			return (type_unknown);
			}
		else
			return (status_unknown);
		}
	}

 #else /* FILESYS_WIDE */
_FS_DLL file_type __CLRCALL_PURE_OR_CDECL _Stat(const char *_Fname, int& _Errno)
	{	// get file status
	wchar_t _Fname_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname, _Fname_wide) != 0)
		return (_Stat(_Fname_wide, _Errno));
	else
		{	// conversion failed, report unknown status
		_Errno = GetLastError();
		return (status_unknown);
		}
	}
 #endif /* FILESYS_WIDE */

_FS_DLL file_type __CLRCALL_PURE_OR_CDECL _Lstat(const TCHAR *_Fname, int& _Errno)
	{	// get symlink file status
	return (_Stat(_Fname, _Errno));
	}

 #if FILESYS_WIDE
_FS_DLL _ULonglong __CLRCALL_PURE_OR_CDECL _File_size(const wchar_t *_Fname)
	{	// get file size
	WIN32_FILE_ATTRIBUTE_DATA _Data;

	if (!TFUN(GetFileAttributesEx)(_Fname, GetFileExInfoStandard, &_Data))
		return (0);
	else
		return ((_ULonglong)_Data.nFileSizeHigh << 32 | _Data.nFileSizeLow);
	}

 #else /* FILESYS_WIDE */
_FS_DLL _ULonglong __CLRCALL_PURE_OR_CDECL _File_size(const char *_Fname)
	{	// get file size
	wchar_t _Fname_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname, _Fname_wide) == 0)
		return (0);
	else
		return (_File_size(_Fname_wide));
	}
 #endif /* FILESYS_WIDE */

// 3 centuries with 24 leap years each:
//     1600 is excluded, 1700/1800 are not leap years
// 1 partial century with 17 leap years:
//     1900 is not a leap year
//     1904 is leap year #1
//     1908 is leap year #2
//     1968 is leap year #17

 #define WIN_TICKS_PER_SECOND	10000000ULL

 #define WIN_TICKS_FROM_EPOCH \
	(((1970 - 1601) * 365 + 3 * 24 + 17) * 86400ULL * WIN_TICKS_PER_SECOND)

 #if FILESYS_WIDE
_FS_DLL time_t __CLRCALL_PURE_OR_CDECL _Last_write_time(const TCHAR *_Fname)
	{	// get last write time
	WIN32_FILE_ATTRIBUTE_DATA _Data;

	if (!TFUN(GetFileAttributesEx)(_Fname, GetFileExInfoStandard, &_Data))
		return (0);
	else
		{	// success, convert time
		_ULonglong _Wtime =
			(_ULonglong)_Data.ftLastWriteTime.dwHighDateTime << 32
				| _Data.ftLastWriteTime.dwLowDateTime;
		return ((time_t)((_Wtime - WIN_TICKS_FROM_EPOCH)
			/ WIN_TICKS_PER_SECOND));
		}
	}

 #else /* FILESYS_WIDE */
_FS_DLL time_t __CLRCALL_PURE_OR_CDECL _Last_write_time(const char *_Fname)
	{	// get last write time
	wchar_t _Fname_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname, _Fname_wide) == 0)
		return (0);
	else
		return (_Last_write_time(_Fname_wide));
	}
 #endif /* FILESYS_WIDE */

 #if FILESYS_WIDE
_FS_DLL void __CLRCALL_PURE_OR_CDECL _Last_write_time(const TCHAR *_Fname,
	time_t _When)
	{	// set last write time
	HANDLE _Handle = TFUN(CreateFile)(_Fname, FILE_WRITE_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		0, OPEN_EXISTING, 0, 0);
	if (_Handle != INVALID_HANDLE_VALUE)
		{	// convert time_t to FILETIME and set
		_ULonglong _Wtime = (_ULonglong)_When * WIN_TICKS_PER_SECOND
			+ WIN_TICKS_FROM_EPOCH;
		FILETIME _Ftime;

		_Ftime.dwLowDateTime = (DWORD)_Wtime;
		_Ftime.dwHighDateTime = (DWORD)(_Wtime >> 32);
		SetFileTime(_Handle, 0, 0, &_Ftime);
		CloseHandle(_Handle);
		}
	}

 #else /* FILESYS_WIDE */
_FS_DLL void __CLRCALL_PURE_OR_CDECL _Last_write_time(const char *_Fname, time_t _When)
	{	// set last write time
	wchar_t _Fname_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname, _Fname_wide) != 0)
		_Last_write_time(_Fname_wide, _When);
	}
 #endif /* FILESYS_WIDE */

 #if FILESYS_WIDE
_FS_DLL space_info __CLRCALL_PURE_OR_CDECL _Statvfs(const TCHAR *_Fname)
	{	// get space information for volume
	space_info _Ans = {0, 0, 0};
	TSTRING _Devname = _Fname;

	if (_Devname.empty()
		|| _Devname.back() != TLIT('/') && _Devname.back() != TLIT('\\'))
		_Devname.append(TLIT("/"));
	_ULARGE_INTEGER _Available, _Capacity, _Free;

	if (TFUN(GetDiskFreeSpaceEx)(_Devname.c_str(),
		&_Available, &_Capacity, &_Free))
		{	// convert values
		_Ans.capacity = _Capacity.QuadPart;
		_Ans.free = _Free.QuadPart;
		_Ans.available = _Available.QuadPart;
		}
	return (_Ans);
	}

 #else /* FILESYS_WIDE */
_FS_DLL space_info __CLRCALL_PURE_OR_CDECL _Statvfs(const char *_Fname)
	{	// get space information for volume
	wchar_t _Fname_wide[_MAX_FILESYS_NAME];
	space_info _Ans = {0, 0, 0};

	if (_To_wide(_Fname, _Fname_wide) == 0)
		return (_Ans);
	else
		return (_Statvfs(_Fname_wide));
	}
 #endif /* FILESYS_WIDE */

 #if FILESYS_WIDE
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Equivalent(const TCHAR *_Fname1, const TCHAR *_Fname2)
	{	// test for equivalent file names
	BY_HANDLE_FILE_INFORMATION _Info1 = {0};
	BY_HANDLE_FILE_INFORMATION _Info2 = {0};
	bool _Ok1 = false;
	bool _Ok2 = false;

	HANDLE _Handle = TFUN(CreateFile)(_Fname1, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		0, OPEN_EXISTING, 0, 0);
	if (_Handle != INVALID_HANDLE_VALUE)
		{	// get file1 info
		_Ok1 = GetFileInformationByHandle(_Handle, &_Info1) != 0;
		CloseHandle(_Handle);
		}

	_Handle = TFUN(CreateFile)(_Fname2, FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		0, OPEN_EXISTING, 0, 0);
	if (_Handle != INVALID_HANDLE_VALUE)
		{	// get file2 info
		_Ok2 = GetFileInformationByHandle(_Handle, &_Info2) != 0;
		CloseHandle(_Handle);
		}

	if (!_Ok1 && !_Ok2)
		return (-1);
	else if (!_Ok1 || !_Ok2)
		return (0);
	else
		{	// test existing files for equivalence
		return (_Info1.dwVolumeSerialNumber != _Info2.dwVolumeSerialNumber
			|| _Info1.nFileIndexHigh != _Info2.nFileIndexHigh
			|| _Info1.nFileIndexLow != _Info2.nFileIndexLow
				? 0 : 1);
		}
	}

 #else /* FILESYS_WIDE */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Equivalent(const char *_Fname1,
	const char *_Fname2)
	{	// test for equivalent file names
	wchar_t _Fname1_wide[_MAX_FILESYS_NAME];
	wchar_t _Fname2_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname1, _Fname1_wide) == 0
		|| _To_wide(_Fname2, _Fname2_wide) == 0)
		return (-1);
	else
		return (_Equivalent(_Fname1_wide, _Fname2_wide));
	}
 #endif /* FILESYS_WIDE */

		// FILE LINKAGE FUNCTIONS
 #if _HAS_HARDLINKS
 #if FILESYS_WIDE
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Link(const TCHAR *_Fname1, const TCHAR *_Fname2)
	{	// link _Fname2 to _Fname1
	WINAPI_PACKAGED_APP_RETURN(errno = EDOM);	// no support

	return (CreateHardLinkW(_Fname2, _Fname1, 0) != 0
		? 0 : GetLastError());
	}
 #else /* FILESYS_WIDE */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Link(const char *_Fname1, const char *_Fname2)
	{	// link _Fname2 to _Fname1
	WINAPI_PACKAGED_APP_RETURN(errno = EDOM);	// no support
	wchar_t _Fname1_wide[_MAX_FILESYS_NAME];
	wchar_t _Fname2_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname1, _Fname1_wide) == 0
		|| _To_wide(_Fname2, _Fname2_wide) == 0)
		return (GetLastError());
	else
		return (_Link(_Fname1_wide, _Fname2_wide));
	}
 #endif /* FILESYS_WIDE */
 #else /* _HAS_HARDLINKS */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Link(const TCHAR *, const TCHAR *)
	{	// link _Fname2 to _Fname1
	return (errno = EDOM);	// hardlinks not supported
	}
 #endif /* _HAS_HARDLINKS */

 #if _HAS_SYMLINKS
 #if FILESYS_WIDE
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Symlink(const TCHAR *_Fname1, const TCHAR *_Fname2)
	{	// link _Fname2 to _Fname1
	WINAPI_PACKAGED_APP_RETURN(errno = EDOM);	// no support

	return (CreateSymbolicLinkW(_Fname2, _Fname1, 0) != 0
		? 0 : GetLastError());
	}
 #else /* FILESYS_WIDE */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Symlink(const char *_Fname1, const char *_Fname2)
	{	// link _Fname2 to _Fname1
	WINAPI_PACKAGED_APP_RETURN(errno = EDOM);	// no support
	wchar_t _Fname1_wide[_MAX_FILESYS_NAME];
	wchar_t _Fname2_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname1, _Fname1_wide) == 0
		|| _To_wide(_Fname2, _Fname2_wide) == 0)
		return (GetLastError());
	else
		return (_Symlink(_Fname1_wide, _Fname2_wide));
	}
 #endif /* FILESYS_WIDE */
 #else /* _HAS_SYMLINKS */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Symlink(const TCHAR *, const TCHAR *)
	{	// link _Fname2 to _Fname1
	return (errno = EDOM);	// symlinks not supported
	}
 #endif /* _HAS_SYMLINKS */

_FS_DLL int __CLRCALL_PURE_OR_CDECL _Rename(const TCHAR *_Fname1, const TCHAR *_Fname2)
	{	// rename _Fname1 as _Fname2
	return (TRENAME(_Fname1, _Fname2) == 0
		? 0 : GetLastError());

	}

_FS_DLL int __CLRCALL_PURE_OR_CDECL _Unlink(const TCHAR *_Fname)
	{	// unlink _Fname
	return (TREMOVE(_Fname) == 0
		? 0 : GetLastError());

	}

 #if FILESYS_WIDE
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Copy_file(const TCHAR *_Fname1, const TCHAR *_Fname2,
	bool _Fail_if_exists)
	{	// copy _Fname1 to _Fname2
	return (TFUN(CopyFile)(_Fname1, _Fname2, _Fail_if_exists) != 0
		? 0 : GetLastError());
	}

 #else /* FILESYS_WIDE */
_FS_DLL int __CLRCALL_PURE_OR_CDECL _Copy_file(const char *_Fname1,
	const char *_Fname2, bool _Fail_if_exists)
	{	// copy _Fname1 to _Fname2
	wchar_t _Fname1_wide[_MAX_FILESYS_NAME];
	wchar_t _Fname2_wide[_MAX_FILESYS_NAME];

	if (_To_wide(_Fname1, _Fname1_wide) == 0
		|| _To_wide(_Fname2, _Fname2_wide) == 0)
		return (GetLastError());
	else
		return (_Copy_file(_Fname1_wide, _Fname2_wide, _Fail_if_exists));
	}
 #endif /* FILESYS_WIDE */

  #undef FILESYS_WIDE
  #undef TCHAR
  #undef TFUN
  #undef TLIT
  #undef TSTRING

  #undef TCHDIR
  #undef TGETCWD
  #undef TMKDIR
  #undef TRMDIR
  #undef TREMOVE
  #undef TRENAME

  #undef _HAS_HARDLINKS
  #undef _HAS_SYMLINKS

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */

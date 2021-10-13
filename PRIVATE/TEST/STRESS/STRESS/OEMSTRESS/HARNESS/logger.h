//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
________________________________________________________________________________
THIS  CODE AND  INFORMATION IS  PROVIDED "AS IS"  WITHOUT WARRANTY  OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.

Module Name: logger.h

Abstract: Contains class declarations for the classes containing logging routines
(history, result)

Notes: None

________________________________________________________________________________
*/

#ifndef __LOGGER_H__


#define U_O_RDONLY     			0x0000	// open for reading only
#define U_O_WRONLY     			0x0001	// open for writing only
#define U_O_RDWR       			0x0002	// open for reading and writing
#define U_O_APPEND     			0x0008	// writes done at eof
#define U_O_SEQUENTIAL 			0x0020
#define	U_O_CREAT				0x0100	// create and open file
#define U_O_TRUNC    			0x0200  // open and truncate
#define U_O_EXCL     			0x0400	// open only if file doesn't already exist
#define	U_SEEK_END				0x002	// end of the file
#define	U_SEEK_CUR				0x001	// current position of file pointer
#define	U_SEEK_SET				0x000	// beginning of the file


enum XmlTagType
{
	BEGIN,
	END,
	EMPTY
};


/*

@class:	IFile, the common interface to access various OEMStress files (result,
history and initialization)

@note:	This is an abstract base class. All the member functions are self
explanatory

@fault:	None

@pre:	None

@post:	None

*/

class IFile
{
protected:
	WCHAR _szFileName[MAX_FILE_NAME];
public:
	virtual bool Open() = 0;
	virtual bool Close() = 0;
	virtual DWORD Read(IN OUT void* lpBuf, IN DWORD dwCount) = 0;
	virtual bool Write(IN const void *lpBuf, IN DWORD dwCount) = 0;
	virtual bool WriteWideStringInANSI(IN LPCWSTR lpszStr, IN bool fAppend = true) = 0;
	virtual bool Seek(IN INT nOffset, IN INT nOrigin) = 0;
	virtual bool SeekToBegin() = 0;
	virtual bool SeekToEnd() = 0;
	virtual bool Truncate() = 0;
};


/*

@class:	_ceshfile_t, Implements IFile over cesh (files accessed for the host
machine)

@note:	All the member functions are self explanatory

@fault:	None

@pre:	None

@post:	None

*/

class _ceshfile_t : public IFile
{
private:
	INT _fd;										// file descriptor for U_ropen, U_rclose and U_rwrite
	INT _uOpenFlags;
	bool _fFileOpen;
public:
	_ceshfile_t(IN LPCWSTR lpszFileName, IN UINT uOpenFlags = 0x90002);
	~_ceshfile_t(void);
	bool Open();
	bool Close();
	DWORD Read(IN void* lpBuf, OUT DWORD dwCount);
	bool WriteWideStringInANSI(IN LPCWSTR lpszStr, IN bool fAppend = true);
	bool Write(IN const void *lpBuf, IN DWORD dwCount);
	bool Seek(IN INT nOffset, IN INT nOrigin = U_SEEK_CUR);
	bool SeekToBegin() {	return Seek(0, U_SEEK_SET);	}
	bool SeekToEnd() {	return Seek(0, U_SEEK_END);	}
	bool Truncate();
	DWORD GetLength();
private:
    _ceshfile_t();									// default constructor NOT implemented
	_ceshfile_t(const _ceshfile_t&);				// default copy constructor NOT implemented
	_ceshfile_t& operator = (const _ceshfile_t&);	// default assignment operator NOT implemented
};


/*

@class:	_file_t, Implements IFile over normal filesystem (files can be local or
on a network share)

@note:	All the member functions are self explanatory

@fault:	None

@pre:	None

@post:	None

*/

class _file_t : public IFile
{
protected:
	FILE *_stream;
	WCHAR _szMode[MAX_PATH];
public:
	_file_t(IN LPCWSTR lpszFileName, IN LPCWSTR lpszMode = L"a+b");
	~_file_t(void);
	bool Open();
	bool Close();
	DWORD Read(IN OUT void* lpBuf, IN DWORD dwCount);
	bool Write(IN const void *lpBuf, IN DWORD dwCount);
	bool WriteWideStringInANSI(IN LPCWSTR lpszStr, IN bool fAppend = true);
	bool Seek(IN INT nOffset, IN INT nOrigin = SEEK_CUR);
	bool SeekToBegin() {	return !fseek(_stream, 0, SEEK_SET);	}
	bool SeekToEnd() {	return !fseek(_stream, 0, SEEK_END);	}
	bool Truncate();
private:
    _file_t();									// default constructor NOT implemented
	_file_t(const _file_t&);					// default copy constructor NOT implemented
	_file_t& operator = (const _file_t&);		// default assignment operator NOT implemented
};


/*

@class:	_history_t, Implements routines for logging history information

@note:	All the functions are self explanatory

@fault:	None

@pre:	IFile *pFile must be valid prior to creation of an object of
this class. Using methods exposed by IFile, history file can be created
over cesh, on a network share or locally.

@post:	None

*/

class _history_t
{
protected:
	WCHAR _szBuffer[MAX_BUFFER_SIZE];
	IFile *_pFile;
	bool _fByRef;
	bool _fFileAlreadyAccessed;
public:
	_history_t(IN IFile *pFile, IN bool fByRef = false);
	~_history_t();
	bool LogResults(IN LPCRITICAL_SECTION lpcs = NULL);
private:
	void DumpHeader();
};


/*

@class:	_result_t, Implements routines for logging history information

@note:	All the functions are self explanatory

@fault:	None

@pre:	IFile *pFile must be valid prior to creation of an object of
this class. Using methods exposed by IFile, history file can be created
over cesh, on a network share or locally.

@post:	None

*/

class _result_t
{
protected:
	WCHAR _szBuffer[MAX_BUFFER_SIZE];
	IFile *_pFile;
	bool _fByRef;
	bool _fXML;
public:
	_result_t(IN IFile *pFile, IN bool fXML = true, IN bool fByRef = false);
	~_result_t();
	bool LogResults(IN LPCRITICAL_SECTION lpcs = NULL, IN BOOL fHangStatus = false,
		IN BOOL fStressCompleted = false, IN BOOL fMemTrackThresholdHit = false);
	bool LogFailure(IN bool fNoRunnableModules = false);
private:
	void LogSystemDetails();
	void LogMemoryAndObjectStoreValues(IN MEMORYSTATUS& ms, IN STORE_INFORMATION& si);
	void LogLaunchParams();
	void LogHangStatus();
	void DumpTextToFileAndOutput(IN LPCWSTR lpszStr, IN bool fBypassXmlModeCheck = false);
	bool DumpXmlTag(IN LPCWSTR lpszTag, IN XmlTagType type = EMPTY, IN bool fTrailingNewline = false);

	inline void BeginXmlTag(IN LPCWSTR lpszTag, IN bool fTrailingNewline = false)
	{
		DumpXmlTag(lpszTag, BEGIN, fTrailingNewline);
	}

	inline void EndXmlTag(IN LPCWSTR lpszTag, IN bool fTrailingNewline = false)
	{
		DumpXmlTag(lpszTag, END, fTrailingNewline);
	}

	inline void EmptyXmlTag(IN LPCWSTR lpszTag, IN bool fTrailingNewline = true)
	{
		DumpXmlTag(lpszTag, EMPTY, fTrailingNewline);
	}
};

#define __LOGGER_H__
#endif /* __LOGGER_H__ */

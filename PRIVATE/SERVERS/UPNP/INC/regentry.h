//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***************************************************************************/
/**                  Microsoft Windows                                    **/
/***************************************************************************/


/****************************************************************************

regentry.h

Oct. 95		LenS

Wrapper for registry access

Construct a RegEntry object by specifying the subkey (under
HKEY_CURRENT_USER by default, but can be overridden.)

All member functions (except the destructor) set an internal
error state which can be retrieved with GetError().
Zero indicates no error.

RegEntry works only with strings and DWORDS which are both set
using the overloaded function SetValue()

	SetValue("valuename", "string");
	SetValue("valuename", 42);
	
Values are retrieved with GetString() and GetNumber().  
GetNumber() allows you to specificy a default if the valuename doesn't
exist.
GetString() returns a pointer to a string internal to RegEntry that is 
invalidated when another fuction is called on the same RegEntry object
(e.g. its destructor) so, if you want to use the string beyond this
time, then you must copy it out of the RegEntry object first.

DeleteValue() removes the valuename and value pair.

Registry flushes are automatic when RegEntry is destroys or moves to
another key.

****************************************************************************/

#ifndef	REGENTRY_INC
#define	REGENTRY_INC

class RegEntry
{
	public:
		RegEntry(	LPCTSTR pszSubKey,
					HKEY hkey = HKEY_LOCAL_MACHINE,
					BOOL fCreate = TRUE,
					REGSAM samDesired = 0
					);
		~RegEntry();
		
		long	GetError()	{ return m_error;}
		BOOL    Success() { return m_error == ERROR_SUCCESS;}
		VOID	ClearError() {m_error = ERROR_SUCCESS;}
		long	SetValue(LPCTSTR pszValueName, LPCTSTR string);
		long	SetValue(LPCTSTR pszValueName, unsigned long dwNumber);
		long	SetValue(LPCTSTR pszValue, void* pData, DWORD cbLength);
		LPTSTR	GetString(LPCTSTR pszValueName);
		DWORD	GetBinary(LPCTSTR pszValueName, void** ppvData);
		long	GetNumber(LPCTSTR pszValueName, long dwDefault = 0);
		ULONG	GetNumberIniStyle(LPCTSTR pszValueName, ULONG dwDefault = 0);
		long	DeleteValue(LPCTSTR pszValueName);
		long	FlushKey();
        VOID    MoveToSubKey(LPCTSTR pszSubKeyName);
        HKEY    GetKey()    { return m_hkey; }
        DWORD   GetDisposition() { return m_dwDisposition; }
        VOID    DeleteKey() {ChangeKey(NULL); m_fhkeyValid = NULL;}

	private:
		VOID	ChangeKey(HKEY hNewKey);
		VOID	UpdateWrittenStatus();
		VOID	ResizeValueBuffer(DWORD length);

		HKEY	m_hkey;
		DWORD   m_dwDisposition;
		long	m_error;
        BOOL    m_fhkeyValid;
		LPBYTE  m_pbValueBuffer;
        DWORD   m_cbValueBuffer;
		BOOL	m_fValuesWritten;
		TCHAR	m_szNULL;
};

inline long 
RegEntry::FlushKey()
{
    if (m_fhkeyValid) {
		m_error = RegFlushKey(m_hkey);
    }
	return m_error;
}


class RegEnumValues
{
	public:
		RegEnumValues(RegEntry *pRegEntry);
		~RegEnumValues();
		long	Next();
		LPTSTR 	GetName()       {return m_pchName;}
        DWORD   GetType()       {return m_dwType;}
        LPBYTE  GetData()       {return m_pbValue;}
        DWORD   GetDataLength() {return m_dwDataLength;}
		DWORD	GetCount()      {return m_cEntries;}

	private:
        RegEntry * m_pRegEntry;
		DWORD   m_iEnum;
        DWORD   m_cEntries;
		LPTSTR  m_pchName;
		LPBYTE  m_pbValue;
        DWORD   m_dwType;
        DWORD   m_dwDataLength;
        DWORD   m_cMaxValueName;
        DWORD   m_cMaxData;
        LONG    m_error;
};

class RegEnumSubKeys
{
	public:
		RegEnumSubKeys(RegEntry *pRegEntry);
		~RegEnumSubKeys();
		long    Next();
		LPTSTR 	GetName()       {return m_pchName;}
		DWORD	GetCount()      {return m_cEntries;}
		DWORD   GetMaxSubKeyLength()    {return m_cMaxKeyName;}

	protected:
        RegEntry * m_pRegEntry;
		DWORD   m_iEnum;
        DWORD   m_cEntries;
		LPTSTR  m_pchName;
        DWORD   m_cMaxKeyName;
        LONG    m_error;
};

#endif // REGENTRY_INC

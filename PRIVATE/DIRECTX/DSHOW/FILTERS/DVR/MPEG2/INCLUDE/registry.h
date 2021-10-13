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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __REGISTRY_PREFS_H__
#define __REGISTRY_PREFS_H__
//
// Registry Prefs.H
//

// Classes to support storing values in the registry
// Over the long term these will become more general
// purpose.


//
// RegKey
//
// Represents a registry key. Most of these functions are thin wrappers,
// so I could put them in the header to gain inline properties, at the
// cost of obscuring the declaration. Let me know if you want them here...
//
// Provides a weak guarantee of construction - any opened keys will be
// closed, but it is possible to construct a NULL object, that all operations
// will fail on. exceptions would be needed to provide a stronger
// guarantee, and the world does not seem ready for that yet ;-)
class RegKey {
    HKEY   hKey;
    REGSAM sam;

protected:

    // Takes ownership, assumed already open
    RegKey(HKEY h, REGSAM r) : hKey(h), sam(r) {}

    // I hope you know what you are doing if you call these
    void Close() { RegCloseKey(hKey); }
    void Open( HKEY    hParent
	     , LPCTSTR szName
	     , REGSAM  Desired
	     );

public:
    enum { GUIDStringLen = 39 };

    // Allocate szString to be GUIDStringLen characters.
    // makes szString as a registry style string: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
    static void APIENTRY StringFromGUID(REFGUID guid, LPTSTR szString, unsigned Len = GUIDStringLen);

    RegKey( HKEY    hParent
	  , LPCTSTR szName  = TEXT("")
	  , REGSAM  Desired = KEY_ALL_ACCESS
	  );
    RegKey( const RegKey& hParent
	  , LPCTSTR szName	// use copy constructor when no szName specified
	  , REGSAM  Desired = KEY_ALL_ACCESS
	  );
    RegKey( HKEY    hParent
	  , REFGUID guid
	  , REGSAM  Desired = KEY_ALL_ACCESS
	  );
    RegKey( const RegKey& hParent
	  , REFGUID guid
	  , REGSAM  Desired = KEY_ALL_ACCESS
	  );
    RegKey(const RegKey&);
    RegKey(const RegKey&, REGSAM samDesired);
    virtual ~RegKey();

    REGSAM SAM() const { return sam; }
    bool   IsOpen() const { return hKey != NULL; }

    const RegKey& operator=(const RegKey&);

    RegKey CreateSubKey( LPCTSTR               szName
		       , REGSAM                samDesired           = KEY_ALL_ACCESS
		       , LPTSTR                lpClass              = NULL
		       , DWORD                 dwOptions            = REG_OPTION_NON_VOLATILE
		       , LPSECURITY_ATTRIBUTES lpSecurityAttributes = NULL
		       , LPDWORD               lpdwDisposition      = NULL
		       ) const;

    RegKey OpenSubKey( LPCTSTR szName
	             , REGSAM  samDesired = KEY_ALL_ACCESS
		     ) const;

    LONG DeleteSubKey(LPCTSTR szName) const;

    LONG SetValue( LPCTSTR     lpValueName
	         , DWORD       dwType
		 , CONST BYTE* lpData
		 , DWORD       cbData
		 ) const;

    LONG SetValue( LPCTSTR     lpValueName
	         , LPCTSTR     szData
		 ) const;

    LONG QueryValue( LPCTSTR lpValueName
                   , LPDWORD lpType
		   , LPBYTE  lpData
		   , LPDWORD lpcbData
		   ) const;

    LONG EnumValue( DWORD   dwIndex
		  , LPTSTR  lpValueName
	          , LPDWORD lpcbValueName
		  , LPDWORD lpType
		  , LPBYTE  lpData
		  , LPDWORD lpcbData
		  ) const;

    // Guarantees the key is written to disk
    LONG Flush() const { return RegFlushKey(hKey); }

private:    // private, until I find a compelling use for it
    // be sure to duplicate/reopen this if you want to call regclosekey()
    operator HKEY() const { return hKey; }
};


// implementation. here to help guarantee inline properties
inline RegKey::RegKey(const RegKey& k, REGSAM samDesired) : hKey(NULL), sam(samDesired)
    { Open(k, NULL, sam); }

inline LONG RegKey::DeleteSubKey(LPCTSTR szName) const { return RegDeleteKey(hKey, szName); }


//
// RegDWORDPref
//
// Aims to look like a DWORD to the user. Has a read cache
// and write-through semantics to the registry
class RegDWORDPref {

    DWORD  value;
    RegKey Key;
    LPTSTR szName;

    bool IsInRegistry() const { return Key.IsOpen(); }
    void WriteThrough();

public:

    RegDWORDPref(const RegKey& key, LPCTSTR szValName, DWORD Default = 0);
    ~RegDWORDPref();

    operator const DWORD() const { return value; }

    void Refresh();

    RegDWORDPref& operator=(const DWORD NewValue) { value = NewValue; WriteThrough(); return *this; }

    RegDWORDPref& operator=(const RegDWORDPref& lhs) { value = lhs.value; WriteThrough(); return *this; }

    RegDWORDPref& operator&=(const DWORD lhs) { value &= lhs; WriteThrough(); return *this; }
    RegDWORDPref& operator&=(const RegDWORDPref& lhs) { value &= lhs.value; WriteThrough(); return *this; }

    RegDWORDPref& operator|=(const DWORD lhs) { value |= lhs; WriteThrough(); return *this; }
    RegDWORDPref& operator|=(const RegDWORDPref& lhs) { value |= lhs.value; WriteThrough(); return *this; }

private:
    RegDWORDPref(const RegDWORDPref&);
};


// Shell/Registry utilites

//
// AddShellFileExtention
//
// Add a file extension to the registry:
// HKCR\szExtention
//  @=szType
//  "Content Type"=szContentType
// eg:
// HKCR\.AC3
//   @="AC3File"
//   "Content Type"="audio\ac3"
// returns a RegXXX result code
extern LONG APIENTRY AddShellFileExtention(LPCTSTR szExtention, LPCTSTR szType, LPCTSTR szContentType = NULL);
extern LONG APIENTRY RemoveShellFileExtention(LPCTSTR szExtention);

//  Register media type stuff
// Add media check bytes
STDAPI RegisterCheckBytes(
    REFGUID Type,
    REFGUID SubType, 
    REFCLSID clsidSource,
    LPCTSTR szN, 
    LPCTSTR szCheckBytes
);

#endif //__REGISTRY_PREFS_H__

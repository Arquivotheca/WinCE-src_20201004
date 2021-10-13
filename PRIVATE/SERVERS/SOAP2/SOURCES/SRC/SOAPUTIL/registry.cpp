//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      registry.cpp
//
// Contents:
//
//      Contains the implementation for the actual change code of the registry.
//
//----------------------------------------------------------------------------------

// Includes ------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


#include "headers.h"

#ifndef UNDER_CE
#include <crtdbg.h>
#endif


static void _UnregisterLevel(
    const REGENTRY *    rRegEntry,
    const int           iCount,
    const HKEY           hkeyParent,
    int &               iPos);

#ifdef CE_NO_EXCEPTIONS
static HRESULT _RegisterCurrentLevel(
#else
static void _RegisterCurrentLevel(
#endif
    const REGENTRY *    rRegEntry,
    const int           iCount,
    const char *        strFileName,
    const char **       astrAlternateFileNames,
    const HKEY          hkeyParent,
    int &               iPos);

static void _RemoveHKEYSubkeys(
    const HKEY          hkey);

static void _SkipChildEntries(
    const REGENTRY *    rRegEntry,
    const int           ciCount,
    const int           ciCurrentLevel,
    int &               iPos);



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void RegisterRegEntry
//
//  parameters:
//
//  description:
//      Called on DLLRegisterServer to add our keys to the registry.
//      This funtion is parameter array driven, so all action can be
//      defined in the rRegEntry case
//
//      In case of an error the function will throw and it is possible
//      that only parts of the rRegEntry array have been processed.
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef CE_NO_EXCEPTIONS
HRESULT RegisterRegEntry(
#else
void RegisterRegEntry(
#endif
   const REGENTRY *    rRegEntry,        // @parm The array of registry entries
                                        //    to process
    const int            iCount,            // @parm The number of elements in the
                                        //    registry array rRegEntry
    const char *        strFileName,    // @parm The name of the DLL to be
                                        //    registered
    const char **        astrAlternates,    // @parm array of alternate file names
    HKEY                hkey            // @parm The parent key to start the
                                        //  opening from
    )
    {
    ASSERT (iCount > 0);
    ASSERT (rRegEntry);
    ASSERT (rRegEntry[0].iLevel == REG_ROOT);

    int iPos    = 0;    // will point to the current position in the array,
                        // so inital we start at 0
#ifdef CE_NO_EXCEPTIONS
    HRESULT hr = S_OK;
#endif
    // loop over all entries
    while (iPos < iCount)
        {
#ifndef UNDER_CE
        _RPT1(_CRT_WARN,"::RegisterRegEntry    iPos:%i\n",iPos);
#endif

        // Call the subfunction to register all the entries which share
        // level 1
#ifdef CE_NO_EXCEPTIONS
        hr =  _RegisterCurrentLevel(
#else
        _RegisterCurrentLevel(
#endif
                rRegEntry,                // the array
                iCount,                    // maximum number of arguments
                strFileName,            // filename of the DLL
                astrAlternates,
                hkey,                    // this is the parent key, in this
                                        // special case it is the root
                iPos                    // the current position is passed
                                        // as a reference parameter, it will
                                        // change with the calls.
                );
    
#ifdef CE_NO_EXCEPTIONS
        if(FAILED(hr))
            return hr;
#endif
        // When we return to this point, we got iPos pointing
        // - beyond elements in rRegEntry
        // - at an entry in level 1
        }

#ifdef CE_NO_EXCEPTIONS
    return hr;
#endif
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void UnregisterRegEntry
//
//  parameters:
//
//  description:
//      Called on DLLUnregisterServer to remove keys from the registry.
//      This funtion is parameter array driven, so all action can be
//      defined in the rRegEntry case
//
//      The function will try to proceed independent from erros, no error
//      code is returned,
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void UnregisterRegEntry(
    const REGENTRY *    rRegEntry,      // The array of registry entries
                                        //    to process
    const int            iCount,        // The number of elements in the
                                        //    registry array rRegEntry
    HKEY                hkey            // The parent key to start from
    )
{
    ASSERT (iCount > 0);
    ASSERT (rRegEntry);
    ASSERT (rRegEntry[0].iLevel == REG_ROOT);

    int iPos    = 0;    // will point to the current position in the array,
                        // so inital we start at 0

    // loop over all entries
    while (iPos < iCount)
    {
        // Call the subfunction to unregister all the entries which share
        // level 1
#ifndef UNDER_CE     
            _RPT1(_CRT_WARN,"::UnregisterRegEntry iPos:%i\n",iPos);
#endif          

        _UnregisterLevel(
                rRegEntry,       // the array
                iCount,          // maximum number of arguments
                hkey,            // this is the parent key, in this
                                 // special case it is the root
                iPos             // the current position is passed
                                 // as a reference parameter, it will
                                 // change with the calls.
                );

        // When we return to this point, we got iPos pointing
        // - beyond elements in rRegEntry
        // - at an entry in level 1
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static void _UnregisterLevel
//
//  parameters:
//
//  description:
//      Called on DLLUnregisterServer and will remove all entries in
//      rRegEntry which are on the same level and have the same parentkey.
//
//      The function will try to proceed independent from erros, no error code is returned
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static void _UnregisterLevel(
    const REGENTRY *    rRegEntry,      // The array of registry entries
                                        //    to process
    const int            iCount,        // The number of elements in the
                                        //    registry array rRegEntry
    const HKEY            hkeyParent,   // The parent key for the current
                                        //  key
    int &                iPos           // The current position in
                                        //    rRegEntry. A reference parameter!
    )
{
    ASSERT (iCount > 0);
    ASSERT (rRegEntry);
    ASSERT (iPos < iCount);

    // The current level we will handle in this function
    const int    ciCurrentLevel = rRegEntry[iPos].iLevel;

    // lets set the outer boundary
    while (iPos < iCount)
    {
        if (ciCurrentLevel > rRegEntry[iPos].iLevel)
        {
            // oops ... we just moved into a parent level, we better
            // return to the parent level !

            return;
        }

        if (rRegEntry[iPos].iAction & UNREG_IGNORE)
        {
            // we will ignore the current key and all child levels
            // of this key
            _SkipChildEntries(rRegEntry, iCount, ciCurrentLevel, iPos);

            // Either iPos is greater than ciCount or we returned to
            // our level or a parent level of it
            continue;
        }

        // now let's open the key we got here ....
        long    stat;
        CHkey    hkey;
#ifndef UNDER_CE  
        _RPT3(_CRT_WARN,"::_UnregisterLevel   iLevel: %i   iPos:%i   key: %s\n", ciCurrentLevel, iPos, rRegEntry[iPos].strRegKey);
#endif

        // Use lower security setting if we are only opening this key, not removing!
        REGSAM    regsam;
        regsam =  (rRegEntry[iPos].iAction & UNREG_OPEN) ?
                    KEY_ENUMERATE_SUB_KEYS : KEY_ALL_ACCESS;

        stat = RegOpenKeyExA(
                    hkeyParent,                  // handle of open key
                    rRegEntry[iPos].strRegKey,   // subkey to open
                    NULL,                        // reserved
                    regsam,                      // security access mask
                    &hkey.hkey                   // address of handle of open key
                    );

        // Any action makes only sense if we could open the key at all !
        if (stat != ERROR_SUCCESS)
        {
            // we couldn't open this thing, so lets forget about the sublevels

            // we will ignore the current key and all child levels
            // of this key
            _SkipChildEntries(rRegEntry, iCount, ciCurrentLevel, iPos);

            // Either iPos is greater than ciCount or we returned to
            // our level or a parent level of it
            continue;
        }

        if (rRegEntry[iPos].iAction & UNREG_REMOVE_AND_SUBKEYS)
        {
            // we are going to remove the current key and subkeys
            _RemoveHKEYSubkeys(hkey.hkey);

            // Lets close the current key ...
            hkey.Close();

            // ... and finaly delete the key
            RegDeleteKeyA(hkeyParent, rRegEntry[iPos].strRegKey);

            // we will ignore the current key and all child levels
            // of this key
            _SkipChildEntries(rRegEntry, iCount, ciCurrentLevel, iPos);

            // Either iPos is greater than ciCount or we returned to
            // our level or a parent level of it
            continue;
        }


        if (rRegEntry[iPos].iAction & UNREG_OPEN)
        {
            // In this case we are asked to keep browsing into the subkeys
            // This key will not be deleted!

            // A requirement is that the next entry in the array is
            // exactly one level deeper ! This will only be asserted, not
            // verified in debug builds.
            ASSERT(iPos + 1 < iCount);
            ASSERT(ciCurrentLevel + 1 == rRegEntry[iPos+1].iLevel);

            iPos ++;
            _UnregisterLevel(
                    rRegEntry,      // the array
                    iCount,         // and it's size
                    hkey.hkey,      // the parent key for the next level
                    iPos            // the current position
                                    // A reference parameter!
                    );

            // At this point we are finished whith hkey as the current
            // entry. Either iPos is greater than ciCount or we returned to
            // our level or a parent level of it
            continue;
        }

        if (rRegEntry[iPos].iAction & UNREG_REMOVE)
        {
            // In this case we are asked to remove the subkey. First we have
            // to process all possible subkeys of it!
            // This key will then be deleted!

            // we need this to remove the key later
            const char * pcRegString = rRegEntry[iPos].strRegKey;

            iPos ++;
            if (iPos < iCount)
            {
                // additional entries are available, are those child-levels
                if (ciCurrentLevel + 1 == rRegEntry[iPos].iLevel)
                {
                    // yes, so process those child-levels first
                    _UnregisterLevel(
                        rRegEntry,        // the array
                        iCount,            // and it's size
                        hkey.hkey,        // the parent key for the next level
                        iPos            // the current position
                                        // A reference parameter!
                        );
                }
            }

            // add this point in time we have processed the child levels and
            // we are ready to get rid of the hkey

            // Lets close the current key ...
            hkey.Close();

            // ... and finaly delete the key
            RegDeleteKeyA(hkeyParent, pcRegString);

            // and we are done with this guy
            continue;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static void _RegisterCurrentLevel
//
//  parameters:
//
//  description:
//        Called on RegisterDLL and will register all entries in
//        rRegEntry which are on the same level and have the same parentkey.
//
//        In case of an error the function will throw and it is possible
//        that only parts of the rRegEntry array have been processed.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef CE_NO_EXCEPTIONS
HRESULT _RegisterCurrentLevel(
#else
static void _RegisterCurrentLevel(
#endif

    const REGENTRY *rRegEntry,        // The array of registry entries
                                      //    to process
    const int       iCount,           // The number of elements in the
                                      //    registry array rRegEntry
    const char     *strFileName,      // The name of the DLL to be
                                      //    registered
    const char    **astrFileAlt,      // alternate file names

    const HKEY      hkeyParent,       // The parent key for the current
                                      //  key
    int &           iPos              // The current position in
                                      //    rRegEntry. A reference parameter!
    )
{
    ASSERT (iCount > 0);
    ASSERT (rRegEntry);
    ASSERT (iPos < iCount);

    // The current level we will handle in this function
    const int    ciCurrentLevel = rRegEntry[iPos].iLevel;

    // lets set the outer boundary
    while (iPos < iCount)
    {
        if (ciCurrentLevel > rRegEntry[iPos].iLevel)
        {
            // oops ... we just moved into a parent level, we better
            // return to the parent level !
#ifdef CE_NO_EXCEPTIONS
            return S_OK;
#else   
            return;
#endif

        }

        if (rRegEntry[iPos].iAction & REG_IGNORE)
        {
            // we will ignore the current key and all child levels
            // of this key
            _SkipChildEntries(rRegEntry, iCount, ciCurrentLevel, iPos);

            // Either iPos is greater than ciCount or we returned to
            // our level or a parent level of it
            continue;
        }

        CHkey    hkey;      // takes the key which was opened and closes
                            // the key as soon as it goes out of scope
        long    stat;       // error-status
#ifndef UNDER_CE
        _RPT3(_CRT_WARN,"::_RegisterLevel   iLevel: %i   iPos:%i   key: %s\n", ciCurrentLevel, iPos, rRegEntry[iPos].strRegKey);
#endif
        if ( (rRegEntry[iPos].iAction & REG_EXISTING) ||
             (rRegEntry[iPos].iAction & REG_OPEN) )
        {
            // We want to open an existing key with or without error checking
            stat = RegOpenKeyExA(
                        hkeyParent,                  // handle of open key
                        rRegEntry[iPos].strRegKey,   // subkey to open
                        NULL,                        // reserved
                        REG_OPEN,                    // security access mask
                        &hkey.hkey                   // address of handle of open key
                        );

            if (rRegEntry[iPos].iAction & REG_OPEN)
            {
                // We asked to open the key and proceed with the sublevels.
                // If open failed we are happy to ignore all sublevels.
                if (stat != ERROR_SUCCESS)
                {
                    // Dang, open failed, so we ignore all sublevels and start
                    // over with a new key.
                    _SkipChildEntries(rRegEntry, iCount, ciCurrentLevel, iPos);

                    // Either iPos greater than ciCount or we are back at our level
                    // or parent level
                    continue;
                }
            }
        }
        if (rRegEntry[iPos].iAction & REG_ADD)
        {
            // Create the Key.    If it exists, we open it.
            DWORD    dwDisposition;                // not used

            stat = RegCreateKeyExA(
                    hkeyParent,                  // handle of open key
                    rRegEntry[iPos].strRegKey,   // subkey to open
                    0,                           // dwReserved
                    NULL,                        // reserved
                    REG_OPTION_NON_VOLATILE,
                    KEY_ALL_ACCESS,              // security access mask
                    NULL,                        // lpSecurityAttributes
                    &hkey.hkey,                  // address of handle of open key
                    &dwDisposition               // address of disposition value buffer
                    );
        }


        // we got an open key, let's see if we have to do something with the
        // key value
        if ( (stat == ERROR_SUCCESS) && (rRegEntry[iPos].regValue.pData) )
        {
            // hmmm, there is a value, now we have to get it in the registry

            // local constant pointing to the actual data and the length
            BYTE *     pData         = (BYTE *) rRegEntry[iPos].regValue.pData;
            DWORD    dwLength    = rRegEntry[iPos].regValue.dwLength;

            if (rRegEntry[iPos].regValue.dwType == REG_SZ)
            {
                // For "%s" arguments substitute filename.
                if (strcmp((char *)(pData), "%s") == 0)
                {
                    ASSERT(strFileName);
                    pData = (BYTE *) strFileName;
                }
                else if (strncmp((char *)(pData), "%s", 2) == 0)
                {
                    ASSERT(astrFileAlt);
                    unsigned i = (unsigned) ((char *) pData)[2] - '0';
                    // ASSERT(i >= 0 && i <= 9);
                    ASSERT(i <= 9);
                    pData = (BYTE *) astrFileAlt[i];
                    ASSERT(pData);
                }

                // calculate the actual length of the string
                dwLength = strlen ((char *)pData) + 1;
            }

            stat = RegSetValueExA(
                        hkey.hkey,                          // created above
                        rRegEntry[iPos].strRegValueName,    // lpszValueName
                        0,                                  // dwReserved
                        rRegEntry[iPos].regValue.dwType,    // fdwType
                        pData,                              // value
                        dwLength                            // cbData
                        );
        }

        // check if open, exist and the eventual addkey succeded
        if (stat != ERROR_SUCCESS)
        {
#ifdef CE_NO_EXCEPTIONS
            return E_FAIL;
#else
    #ifndef UNDER_CE
            throw E_FAIL;
    #else
            throw;
    #endif
#endif

        }


        // at this point all action for the current item worked, so let's get
        // moving on the next one
        iPos ++;
        if (iPos  < iCount)
        {
            if (ciCurrentLevel < rRegEntry[iPos].iLevel)
            {
                // interesting case, the new entry is a child level of
                // the entry we just finshed processing, so we call
                // ourselves recursively

                // just ASSERT that the difference between those levels is 1
                ASSERT (ciCurrentLevel + 1 == rRegEntry[iPos].iLevel);
#ifdef CE_NO_EXCEPTIONS
                HRESULT hr =
#endif
                _RegisterCurrentLevel(
                    rRegEntry,
                    iCount,
                    strFileName,
                    astrFileAlt,
                    hkey.hkey,          // this is the parent for the
                                        // following level
                    iPos);

#ifdef CE_NO_EXCEPTIONS
                if(FAILED(hr))
                    return hr;
#endif
                // When we return to this point, we got iPos pointing
                // - beyond elements in rRegEntry
                // - at a entry in level ciCurrentLevel
                // - at a parent entry for ciCurrentLevel
            }
        }

        // the local variable hkey goes out of scope here, if we had a key
        // open it is closed in the destructor of this class.
    }  // while (iPos < iCount)
#ifdef CE_NO_EXCEPTIONS
    return S_OK;
#endif   
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static void _RemoveHKEYSubkeys
//
//  parameters:
//
//  description:
//      This function removes all subkeys of the the passed in HKEY
//      No error-checking is performed
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static void _RemoveHKEYSubkeys(
    const HKEY        hkey            // key to be removed including subkeys
    )
{
    long        stat;        // error status;

    do
    {
        char        acName[512];
        ULONG        cbName = 511;
        FILETIME    ftTemp;
        stat = RegEnumKeyExA(
                    hkey,                // handle of key to enumerate
                    0,                    // index of subkey to enumerate
                    acName,                // address of buffer for subkey name
                    &cbName,            // address for size of subkey buffer
                    NULL,                // reserved
                    NULL,                // address of buffer for class string
                    NULL,                // address for size of class buffer
                    &ftTemp             // address for time key last written to
                    );
#ifndef UNDER_CE
        _RPT1(_CRT_WARN,"::_RemoveHKEYSubkeys   key: %s\n", acName);
#endif
        if (stat == ERROR_SUCCESS)
        {
            // we found another subkey (name is in acName),
            // now let's process this one

            HKEY    hkeyWork;

            if (ERROR_SUCCESS == RegOpenKeyExA(hkey, acName, 0,
                                        KEY_ALL_ACCESS, &hkeyWork))
            {
                // we got the key open, so let's remove this one and all
                // subkeys of it recursive
                _RemoveHKEYSubkeys(hkeyWork);

                // Then close this key ...
                RegCloseKey(hkeyWork);

                // ... and finaly delete the key
                RegDeleteKeyA(hkey, acName);
            }
        }
    }
    while (stat == ERROR_SUCCESS);
    // no subkeys are available, hkey is subkey free and can be removed by
    // the caller
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static void _SkipChildEntries
//
//  parameters:
//
//  description:
//        This function skips over the current entry and all child entries
//        of this entry. It returns either at a entry a the same level as
//        the initial one (or below that), or with iPos > iCount
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static void _SkipChildEntries(
    const REGENTRY *rRegEntry,      // The array of registry entries
                                    //    to process
    const int       iCount,         // The number of elements in the
                                    //    registry array rRegEntry
    const int       ciCurrentLevel, // The current level
    int &           iPos            // The current position in
                                    //    rRegEntry. A reference parameter!

    )
{
    ASSERT (iCount > 0);
    ASSERT (rRegEntry);
    ASSERT (iPos < iCount);
    ASSERT (rRegEntry[iPos].iLevel == ciCurrentLevel);
    iPos ++;
    while (iPos < iCount)
    {
#ifndef UNDER_CE
        _RPT3(_CRT_WARN,"::_Skip   iLevel: %i   iPos:%i    key:%s \n", rRegEntry[iPos].iLevel, iPos, rRegEntry[iPos].strRegKey);
#endif
        if (rRegEntry[iPos].iLevel <= ciCurrentLevel)
        {
            // we have reached a parent level or are back at
            // the ciCurrentLevel ... we are done
            return;
        }

        // lets try the next entry
        iPos ++;
    }

    // we have exceeded iCount
    return;
}


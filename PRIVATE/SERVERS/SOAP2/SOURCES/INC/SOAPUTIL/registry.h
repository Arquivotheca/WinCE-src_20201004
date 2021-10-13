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
//      registry.h
//
// Contents:
//
//      This module contains the structure definition and function prototypes for
//      the generic registry functions. These allow a generic way to add and remove
//      registry entries. The only written outside these functions is an array
//      definition and the actual call to the interface functions
//
//----------------------------------------------------------------------------------

#ifndef _REGISTRY_H

#define _REGISTRY_H
//
// Small helper class to create a HKEY object
//
class CHkey
{
public:
    HKEY hkey;

    inline CHkey()
    {
        hkey = NULL;
    }

    inline void Close(void)
    {
        if (hkey)
        {
            RegCloseKey(hkey);
            hkey = NULL;
        }
    };

    inline ~CHkey()
    {
        Close();
    }
};



/*------------------------------------------------------------------------------
    REG_ROOT - marks the root of the registry
------------------------------------------------------------------------------*/
const int        REG_ROOT                    = 1;


/*------------------------------------------------------------------------------
    REG_IGNORE   - Ignore this entry and child-entries of this entry during Register
    REG_EXISTING - This key has to exist on Register. If this key is missing register will fail.
    REG_OPEN     - If key exists it is opened, otherwise this key and all childkeys are ignored
    REG_ADD      - Add this entry on Register
    UNREG_IGNORE - Ignore this entry on Unregister
    UNREG_OPEN   - Opens and processes this entry on Unregister
    UNREG_REMOVE - Remove this entry on Unregister
    UNREG_REMOVE_AND_SUBKEYS - Remove this entry and all entries below on Unregister
------------------------------------------------------------------------------*/

const int        REG_IGNORE                  = 0x01;
const int        REG_EXISTING                = 0x02;
const int        REG_OPEN                    = 0x04;
const int        REG_ADD                     = 0x08;
const int        UNREG_IGNORE                = 0x10;
const int        UNREG_OPEN                  = 0x20;
const int        UNREG_REMOVE                = 0x40;
const int        UNREG_REMOVE_AND_SUBKEYS    = 0x80;

const int        REG_DEFAULT                 = REG_ADD |UNREG_REMOVE_AND_SUBKEYS;

/*------------------------------------------------------------------------------
    struct REGVALUE - Defines the value put into the registry.
------------------------------------------------------------------------------*/
typedef struct
{
    DWORD    dwType;        // Describes the type of the data
                            // in pData. Use values matching the dwType
                            // parameter in RegSetValueEx()
    void *   pData;         // A pointer to the actual data
    DWORD    dwLength;      // The length of the data, this can be 0
                            // if dwType is REG_SZ.
} REGVALUE;


/*------------------------------------------------------------------------------
    REGENTRY - This is the structure which defines the reguistry entries.
               Each element of the structure contains sufficient information to
               determine the action on this element in the Register and Unregister
               action.
------------------------------------------------------------------------------*/

typedef struct
{
    char *    strRegKey;        // Contains the key element
    char *    strRegValueName;  // contails the name of the value
    REGVALUE  regValue;         // Contains the value for the key
    int       iLevel;           // The level is supposed to appear in.
                                // REG_ROOT (1) for the registry root level.
    int       iAction;          //  Describes the action for this key during
                                //        the Register and Unregister function. This
                                //        can be a mix of REG_IGNORE, REG_ADD,
                                //        UNREG_IGNORE, UNREG_REMOVE,
                                //        UNREG_REMOVE_AND_SUBKEYS
} REGENTRY;


/*------------------------------------------------------------------------------
    Called on DLLRegisterServer to add our keys to the registry.
    This funtion is parameter array driven, so all action can be
    defined in the rRegEntry case

    In case of an error the function will throw and it is possible
    that only parts of the rRegEntry array have been processed.
------------------------------------------------------------------------------*/
#ifdef CE_NO_EXCEPTIONS
	HRESULT RegisterRegEntry(
#else
void RegisterRegEntry(
#endif

    const REGENTRY *    rRegEntry,      // The array of registry entries
                                        //    to process
    const int           iCount,         // The number of elements in the
                                        //    registry array rRegEntry
    const char *        strFileName,    // The name of the DLL to be
                                        //    registered,
    const char **       astrFileAlt,    // alternate file names
    HKEY                hkey            // The paraent key to start the
                                        //  opening from
    );


/*------------------------------------------------------------------------------
    Called on DLLUnregisterServer to remove keys from the registry.
    This funtion is parameter array driven, so all action can be
    defined in the rRegEntry case

    The function will try to proceed independent from erros, no error
    code is returned,
------------------------------------------------------------------------------*/

void UnregisterRegEntry(
    const REGENTRY *    rRegEntry,        // The array of registry entries
                                          // to process
    const int           iCount,           // The number of elements in the
                                          //    registry array rRegEntry
    HKEY                hkey              // The parent key to start from
    );


#endif


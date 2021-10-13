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
#if !defined(__CONFIGUTY_H__)
#define __CONFIGUTY_H__

/* Configuration file format:
-------------- Sample config file -----------------
# Lines beginning with '#' will be ignored.
# Whitespace is not ignored, so start each entry
# with no spaces in front.
 # This line would be treated as a key because of the leading ' '

# Entries that do not belong in any section belong on top
# These entries will only be used if no section is specified.
#  With no value specified, QueryInt will return 1
pfEntry1

# With a value specified, QueryInt will return the value
pfEntry2=5

# Section headers are surrounded by '[' and ']'
# Everything is case sensitive.
[NewSection]
# is different from
[newsection]

# Sections can be specified multiple times
# The last value found will be used.
[NewSection]
---------------- End sample config file -----------
*/



#define MAX_KEYSIZE 64

// Allow for the key, an =, two ", and a full path
#define MAX_LINESIZE (MAX_PATH + MAX_KEYSIZE+3)

namespace ConfigUty
{
    class CConfig;
    
    typedef enum _EValueType 
    { 
        evtString, 
        evtInt 
    } EValueType, * LPEValueType;
    
    typedef struct _ConfigMap
    {
        int m_iKey;
        TCHAR m_tszKey[MAX_KEYSIZE];
        union {
            int m_iValue;
            TCHAR* m_tszValue;
        };
        EValueType m_evt;
    } ConfigMap, * LPConfigMap;    

    class CConfig
    {
    public:
        //---------------------------------------------------------------
        // Set everything up. Initialize m_iNumEntries to -1 because it is
        // conceivable to have 0 entries in a valid table (though not very
        // useful)
        CConfig() : m_pcm(NULL), m_iNumEntries(-1), m_fInitialized(false) {};

        //---------------------------------------------------------------
        // Copy constructor. Internally it calls Copy, ignoring the value
        // Copy returns. Calling Copy directly is recommended, otherwise
        // error checking is trickier.
        CConfig(const CConfig & ccOther);

        //---------------------------------------------------------------
        // Destructor. Calls Clean to delete any memory allocated.
        ~CConfig();

        //---------------------------------------------------------------
        // Assignment operator. Internally it calls Copy, then returns
        // a reference to (*this). As in the copy constructor, calling
        // Copy directly is preferred, to simplify error checking problems.
        CConfig& operator= (const CConfig& ccOther);

        //---------------------------------------------------------------
        // Makes this class the same as ccOther by creating a new mapping
        // table and copying all the keys and values over.
        BOOL Copy(const CConfig& ccOther);

        //---------------------------------------------------------------
        // Initializes the object using the data found in tszFilename and
        // the keys found in pcmTable. Creates an internal copy of pcmTable
        // so that the original table can be used to initialize multiple
        // copies of the CConfig object.
        // If tszSection is not NULL, Initialize will only load values found
        // under matching section headers ('[<section>]').
        // This call will fail if:
        //   the file cannot be loaded
        //   a copy of the pcmTable cannot be created
        //   a section is specified that isn't in the config file
        BOOL Initialize(const TCHAR * tszFilename, const ConfigMap * pcmTable, const TCHAR * tszSection = NULL);

        //---------------------------------------------------------------
        // QueryInt looks up the value associated to a key. The default
        // value associated to every key is 0. If the key is listed in
        // the configuration file without an associated value, it defaults
        // to 1, though other values can be specified in the 
        // configuration file. If a string is specified in the configuration
        // file, then QueryInt will return 1.
        // The lookup can be by either the integer value or by the string
        // equivalent.
        int QueryInt(int iKey) const;
        int QueryInt(const TCHAR * tszKey) const;

        //---------------------------------------------------------------
        // QueryString looks up the string value associated to a key. 
        //
        //   iKey or tszKey: The integer value or name of the key on  
        //      which to perform the lookup.
        //   tszValue: A pointer to a buffer that has already been
        //      allocated. If NULL, QueryString will return the length
        //      necessary to hold the string value, including the
        //      null terminating character, or 0 if there is no
        //      string associated with the key.
        //   iLen: The length of the tszValue buffer in TCHARs.
        //   
        //   Return Value: The number of characters copied into tszValue,
        //      not include the null terminating character.
        //      If tszValue is NULL, then the length in TCHARs of the 
        //      string associated with the key, including the null 
        //      terminating character. If there is no string associated
        //      with the key, -1 will be returned in either case.
        int QueryString(int iKey, TCHAR * tszValue, int iLen) const;
        int QueryString(const TCHAR * tszKey, TCHAR * tszValue, int iLen) const;

        //---------------------------------------------------------------
        // QueryKey translates between int key and string key
        const TCHAR * QueryKey(int iKey) const;
        int QueryKey(const TCHAR* tszKey) const;

        //---------------------------------------------------------------
        // IsInitialized returns true if the Initialize call did not fail.
        // If this returns true, the data in the config object is ready
        // for use. If this isn't true, then any data obtained from the
        // object is invalid.
        BOOL IsInitialized() const { return m_fInitialized; };


    private:
        //---------------------------------------------------------------
        // LoadConfigTable takes a configuration table (generated by the
        // BEGIN_CONFIG_TABLE/CONFIG_ENTRY/END_CONFIG_TABLE macros) and 
        // makes a copy for internal use.
        BOOL LoadConfigTable(const ConfigMap * pcmTable);

        //---------------------------------------------------------------
        // LoadConfigFile parses through a configuration file.
        // If tszSection is NULL, it looks through and sets every key 
        // value to the value specified in the file (default is 1).
        // If tszSection is not NULL, it looks through and locates every
        // key/value located under the appropriate section.
        BOOL LoadConfigFile(const TCHAR * tszFilename, const TCHAR * tszSection);

        //---------------------------------------------------------------
        // Clean resets all member variables to uninitialized defaults.
        // Clean also frees any memory currently allocated for this object
        void Clean();
        
        ConfigMap * m_pcm;
        int m_iNumEntries;
        bool m_fInitialized;
    };
}

//--------------------------------------------------
// These macros are used to create a template config
// table named TABLENAME, which can then be passed into the
// CConfig::Initialize function.
// This is designed to be used with enums, but using
// plain integers is allowed as well. All that matters
// is the ENTRY needs to be a valid expression that
// evaluates to an int. If more than one entry evaluate
// to the same integer value, QueryInt (int) will
// return an undefined value. QueryInt (const TCHAR *)
// should be used in this case.
#define BEGIN_CONFIG_TABLE(TABLENAME) ConfigUty::ConfigMap TABLENAME [] = {
#define CONFIG_ENTRY(ENTRY) { ENTRY, TEXT(#ENTRY), 0 },
#define END_CONFIG_TABLE { 0, 0, 0 } };

#endif

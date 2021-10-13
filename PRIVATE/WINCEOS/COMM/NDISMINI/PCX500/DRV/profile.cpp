//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//---------------------------------------------------------------------------
// profile.cpp
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 11/10/00     jbeaujon        -Initial Revision
// 02/21/01     jbeaujon        -use default profile names if none specified.
//
//---------------------------------------------------------------------------

#include "profile.h"
#include "alloc.h"
#include "support.h"
#include "keywords.h"


//---------------------------------------------------------------------------
// Private function prototypes.
//---------------------------------------------------------------------------

BOOLEAN         allocateProfile     (STPROFILE          **profiles, 
                                     USHORT             count,
                                     STPROFILE          **newProf);

USHORT          decodeZFlags        (STPROFILE      *profile, 
                                     UCHAR          *zFlagsBuf);

BOOLEAN         readProfile          (NDIS_HANDLE    ConfigHandle, 
                                     STPROFILE      *profile, 
                                     USHORT         index,
                                     PUSHORT        checkSum);


//===========================================================================
    STPROFILE* addProfile (PCARD              card,
                           PROFILE_PROPERTIES *profileProps)
//===========================================================================
// 
// Description: Add a new profile specifying only the profile properties.
//              All config info (the zflags) are set to 0.
//    
//      Inputs: card            - pointer to card structure.
//              profileProps    - pointer to profile properties
//    
//     Returns: Pointer to new profile if successful, NULL otherwise.
//    
//      (02/22/01)
//---------------------------------------------------------------------------
{
    STPROFILE *newProfile = NULL;
    if (allocateProfile(&card->m_profiles, card->m_numProfiles, &newProfile)) {
        memcpy(&newProfile->properties, profileProps, sizeof(PROFILE_PROPERTIES));
        newProfile->isValid = TRUE;
        // 
        // fix active and current pointers.
        // 
        card->m_activeProfile = &card->m_profiles[card->m_activeProfileIndex];
        card->m_currentProfile = newProfile;
        card->m_numProfiles++;
        }
    return(newProfile);
}

//===========================================================================
    STPROFILE* addProfile (PCARD    card,
                           PROFILE  *profile)
//===========================================================================
// 
// Description: Add a new profile.
//    
//      Inputs: card    - pointer to card structure.
//              profile - pointer to profile data.
//    
//     Returns: Pointer to new profile if successful, NULL otherwise.
//    
//      (02/22/01)
//---------------------------------------------------------------------------
{
    STPROFILE *newProfile = NULL;
    if (allocateProfile(&card->m_profiles, card->m_numProfiles, &newProfile)) {
        memcpy(&newProfile->zFlags.cfg, profile, sizeof(PROFILE));
        newProfile->isValid = TRUE;
        // 
        // fix active and current pointers.
        // 
        card->m_activeProfile = &card->m_profiles[card->m_activeProfileIndex];
        card->m_currentProfile = newProfile;
        card->m_numProfiles++;
        }
    return(newProfile);
}

//===========================================================================
    BOOLEAN getProfiles (NDIS_HANDLE  ConfigHandle, 
                         STPROFILE    **profiles, 
                         PUSHORT      count)
//===========================================================================
// 
// Description: Read all profiles from the registry.
//    
//      Inputs: ConfigHandle    - opaque registry handle
//              zFlags          - pointer to pointer to array of profiles.
//              count           - point to USHORT in which to store the number
//                                of profiles read.
//    
//     Returns: TRUE if successful, FALSE otherwise.
//    
//      (10/27/00)
//---------------------------------------------------------------------------
{
    BOOLEAN retval  = TRUE;
    USHORT  cnt     = 0;

    STPROFILE pf;

    *profiles       = NULL;

    USHORT      checkSum;
    STPROFILE   *newProf;
    // 
    // Loop until we've read all profiles.
    // 
    while (readProfile(ConfigHandle, &pf, cnt, &checkSum)) {

        if (allocateProfile(profiles, cnt, &newProf)) {
            // 
            // copy new profile to array.  Set valid flag based on checksum.
            // 
            memcpy(newProf, &pf, sizeof(STPROFILE));
            newProf->isValid = (checkSum == pf.zFlags.sum);
            // 
            // bump profile count.
            // 
            cnt++;
            }
        else {
            retval = FALSE;
            break;
            }
        }

    *count = cnt;

    return(retval);
}

//===========================================================================
    BOOLEAN allocateProfile (STPROFILE **profiles, USHORT count, STPROFILE **newProf)
//===========================================================================
// 
// Description: Allocate an additional profile at the end of the array of profiles
//              pointed to by "profiles".  The array initially contains "count" 
//              elements.  Upon successful return, the array contains count+1 
//              elements and the contents of the old array have been copied to 
//              the new array.  *newProf will point to the new profile structure
//              at the end of the array.
//    
//      Inputs: profiles    - pointer to pointer to an array of count profile 
//                            structures.  This array is reallocated to 
//                            accomodate one additional profile structure.
//              count       - number of elements in the array.
//              newProf     - used to return a pointer to the new profile (the
//                            last element in the array).
//    
//     Returns: TRUE if successful, FALSE otherwise.
// 
//     Caveats: Since the array of profiles is deallocated and then reallocated, 
//              any pointers referencing individual array elements will be invalid 
//              upon successful return of this function.
//    
//      (02/21/01)
//---------------------------------------------------------------------------
{
    BOOLEAN retval = FALSE;
    if (profiles) {
        // 
        // Save pointer to previous profile array.
        // 
        STPROFILE *saveProfiles = *profiles;
        // 
        // Allocate new array with one additional element.
        // 
        *profiles = new STPROFILE[count + 1];
        if (*profiles) {
            NdisZeroMemory(*profiles, sizeof(STPROFILE) * (count + 1));
            // 
            // copy old array to new if necessary
            // 
            if (saveProfiles) {
                memcpy(*profiles, saveProfiles, sizeof(STPROFILE) * count);
                }
            if (newProf) {
                *newProf = &((*profiles)[count]);
                }
            delete [] saveProfiles;
            retval = TRUE;
            }
        else {
            *profiles   = saveProfiles;
            *newProf    = NULL;
            }
        }

    return(retval);
}

//===========================================================================
    STPROFILE* findProfileByName (char      *name,
                                  STPROFILE *profiles,
                                  USHORT    count,
                                  USHORT    *index)
//===========================================================================
// 
// Description: Find a profile with the specified name in the array of count
//              profiles pointed to by profiles.
//    
//      Inputs: name        - pointer to name of profile to find.
//              profiles    - pointer to array of profiles.
//              count       - number of profiles in array.
//              index       - pointer to USHORT in which the array index of 
//                            profile is returned.
//    
//     Returns: pointer to profile is successful, NULL otherwise.
//    
//      (11/10/00)
//---------------------------------------------------------------------------
{
    STPROFILE *retval = NULL;
    for (USHORT i = 0; i < count; i++) {
        if (strcmp(name, profiles[i].properties.name) == 0) {
            retval = (profiles + i);
            *index = i;
            break;
            }
        }
    return(retval);
}

//===========================================================================
    BOOLEAN readProfile (NDIS_HANDLE  ConfigHandle, 
                         STPROFILE    *profile, 
                         USHORT       index,
                         PUSHORT      checkSum)
//===========================================================================
// 
// Description: Read the specified profile from the registry and store it in
//              the buffer pointed to by profile.
//    
//      Inputs: ConfigHandle    - opaque registry handle
//              profile         - pointer to structure in which to store profile.
//              index           - index of which profile to read.  0 indicates 
//                                the default profile stored in the registry 
//                                under the keys zFlags1, zFlags2, zFlags3, 
//                                zFlags4, zFlags5.  Alternate profiles (index == N, 
//                                where N != 0) are stored under registry keys 
//                                NzFlags1, NzFlags2, NzFlags3, NzFlags4, NzFlags5. 
//              checkSum        - pointer to USHORT in which to store calculated
//                                checksum of zFlags portion of the profile.
//    
//     Returns: TRUE if successful, FALSE otherwise.
//    
//      (10/24/00)
//---------------------------------------------------------------------------
{

    int i;
    // 
    // Assume failure.
    // 
    BOOLEAN retval = FALSE;

    char prefix[33]; // should be plenty big enough.
    NdisZeroMemory(prefix, sizeof(prefix));
    // 
    // Index of 0 uses the default zFlag keys (i.e. no prefix).
    // For index > 0 we prefix the index on the default registry keys.
    // 
    if (index > 0) {
        strcpy(prefix, NumStr((ULONG)index, 10));
        }
    // 
    // Buffer to hold all encoded zflags sections
    // 
    UCHAR zFlagsBuf[(2 * sizeof(STPROFILE)) + 1];
    memset(zFlagsBuf, '0', sizeof(zFlagsBuf));
    // 
    // Sizes of each zflags section.
    // 
    UINT sizes[NUM_ZFLAG_KEYS]  =   {
                                        2 * sizeof(profile->zFlags.sum),            // 4
                                        2 * 127,                                    // 254
                                        2 * (sizeof(profile->zFlags.cfg) - 127),    // 58
                                        2 * sizeof(profile->zFlags.SSIDList),       // 208
                                        2 * sizeof(profile->zFlags.APList),         // 52
                                        2 * sizeof(profile->properties)             // 35
                                    };
    // 
    // Read each zflag section from the registry (ASCII encoded).
    // 
    BOOLEAN breakout    = FALSE;
    char    *regkey     = new char[strlen(__zFlagsRegKeyBaseName) + strlen(prefix) + strlen(NumStr(NUM_ZFLAG_KEYS, 10)) + 1];
    int     offset      = 0;
    if (regkey) {
        for (i = 0; (i < NUM_ZFLAG_KEYS) && !breakout; i++) {
            // 
            // Build reg key
            // 
            strcpy(regkey, prefix);
            strcat(regkey, __zFlagsRegKeyBaseName);
            strcat(regkey, NumStr(i+1, 10));
            // 
            // Convert to unicode
            // 
            NDIS_STRING uniRegKey;
            NdisInitString(&uniRegKey, (PUCHAR)regkey);

            if (!GetConfigEntry(ConfigHandle, &uniRegKey, zFlagsBuf + offset, sizes[i]) && 
                (i <= NUM_REQUIRED_ZFLAG_KEYS)) {
                breakout = TRUE;
                }

            offset += sizes[i];

            NdisFreeString(uniRegKey);
            }
        // 
        // Decode 'em if we got 'em all.
        // 
        if (i == NUM_ZFLAG_KEYS) {
            *checkSum   = decodeZFlags(profile, zFlagsBuf);
            retval      = TRUE;
            }
        delete regkey;
        }

    if (retval) {
        // 
        // Set default name if none specified.
        // 
        if (*profile->properties.name == '\0') {
            // 
            // Name is set based on home config bit
            // 
            if (profile->zFlags.cfg.HomeConfiguration & 0x0001) {
                strcpy(profile->properties.name, DEFAULT_HOME_PROFILE_NAME);
                }
            else {
                strcpy(profile->properties.name, DEFAULT_ENTERPRISE_PROFILE_NAME);
                }
            // 
            // Only the zeroth profile can be the default.
            // 
            profile->properties.isDefault       = (index == 0);
            profile->properties.autoSelectable  = TRUE;
            }
        }

    return retval;
}

//===========================================================================
    USHORT decodeZFlags (STPROFILE *profile, UCHAR *zFlagsBuf)
//===========================================================================
// 
// Description: Decode ASCII encoded zflags.
//    
//      Inputs: profile     - pointer to PROFILE structure in which to store
//                            the decoded zFlags.
//              zFlagsBuf   - pointer to a buffer containing ASCII encoded
//                            zFlags.
//    
//     Returns: a checksum calculated from the decoded zFlags structure.
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    // 
    // Decode ascii zflags into zFlags buffer.
    // 
    UCHAR *profilePtr = (UCHAR*)profile;
    for (int i = 0; i < (2 * sizeof(STPROFILE)); i += 2) {
        *profilePtr++ = AsciiWordToHex(*(USHORT*)(zFlagsBuf+i)); 
        }
    // 
    // Calculate check sum on zflags and return it.
    // (NOTE: the sum field is excluded from the calculation).
    // 
    USHORT checkSum = 0;
    profilePtr = (UCHAR*)&profile->zFlags.cfg;
    for (i = 2; i < sizeof(STZFLAGS); i++) {
        UCHAR ptr = *profilePtr++;
        checkSum += ptr;
        }
    return(checkSum);
}

 
#ifdef DBG


//===========================================================================
    void listProfiles (STPROFILE *profile, int count)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (02/22/01)
//---------------------------------------------------------------------------
{
    for (int i = 0; i < count; i++) {
        DbgPrint("%-2d) name = %s, autosel = %s, default = %s, valid = %s\n",
                 i,
                 profile[i].properties.name,
                 profile[i].properties.autoSelectable ? "TRUE" : "FALSE",
                 profile[i].properties.isDefault ? "TRUE" : "FALSE",
                 profile[i].isValid ? "TRUE" : "FALSE");
        }
}

//===========================================================================
    void dumpProfile (STPROFILE *profile, BOOLEAN detailed)
//===========================================================================
// 
// Description: 
//    
//      Inputs: 
//    
//     Returns: 
//    
//      (11/13/00)
//---------------------------------------------------------------------------
{
    DbgPrint("--\n");
    DbgPrint("          PROFILE: ");   DbgPrint(profile->properties.name);                                  DbgPrint("\n");
    DbgPrint("   autoSelectable: ");   DbgPrint(profile->properties.autoSelectable ? "TRUE" : "FALSE");     DbgPrint("\n");
    DbgPrint("        isDefault: ");   DbgPrint(profile->properties.isDefault ? "TRUE" : "FALSE");          DbgPrint("\n");
    DbgPrint("          isValid: ");   DbgPrint(profile->isValid ? "TRUE" : "FALSE");                       DbgPrint("\n");

    if (!detailed) {
        DbgPrint("--\n\n");
        }
    else {
        DbgPrint("   ------------------------------------------\n");    
        DbgPrint("   size               = ");   DbgPrint(NumStr(profile->zFlags.cfg.size, 10));                     DbgPrint("\n");
        DbgPrint("   OpMode             = ");   DbgPrint(NumStr(profile->zFlags.cfg.OpMode, 10));                   DbgPrint("\n");
        DbgPrint("   RxMode             = ");   DbgPrint(NumStr(profile->zFlags.cfg.RxMode, 10));                   DbgPrint("\n");
        DbgPrint("   FragThreshhold     = ");   DbgPrint(NumStr(profile->zFlags.cfg.FragThreshhold, 10));           DbgPrint("\n");
        DbgPrint("   RTSThreshhold      = ");   DbgPrint(NumStr(profile->zFlags.cfg.RTSThreshhold, 10));            DbgPrint("\n");

        DbgPrint("   MacId              = ");   
        for (int i = 0; i < 6; i++) {
            DbgPrint(NumStr(profile->zFlags.cfg.MacId[i], 16));
            DbgPrint(":");
            }
        DbgPrint("\n");

        DbgPrint("   SupRates           = ");   
        for (i = 0; i < 8; i++) {
            DbgPrint(NumStr(profile->zFlags.cfg.SupRates[i], 10));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        DbgPrint("   ShortRetryLimit    = ");   DbgPrint(NumStr(profile->zFlags.cfg.ShortRetryLimit, 10));          DbgPrint("\n");
        DbgPrint("   LongRetryLimit     = ");   DbgPrint(NumStr(profile->zFlags.cfg.LongRetryLimit, 10));           DbgPrint("\n");
        DbgPrint("   TxMSDULifetime     = ");   DbgPrint(NumStr(profile->zFlags.cfg.TxMSDULifetime, 10));           DbgPrint("\n");
        DbgPrint("   RxMSDULifetime     = ");   DbgPrint(NumStr(profile->zFlags.cfg.RxMSDULifetime, 10));           DbgPrint("\n");
        DbgPrint("   stationary         = ");   DbgPrint(NumStr(profile->zFlags.cfg.stationary, 10));               DbgPrint("\n");
        DbgPrint("   ordering           = ");   DbgPrint(NumStr(profile->zFlags.cfg.ordering, 10));                 DbgPrint("\n");

        DbgPrint("   RESERVED1          = ");   
        for (i = 0; i < 12; i++) {
            DbgPrint("0x");
            DbgPrint(NumStr(profile->zFlags.cfg.RESERVED1[i], 16));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        DbgPrint("   ScanMode           = ");   DbgPrint(NumStr(profile->zFlags.cfg.ScanMode, 10));                 DbgPrint("\n");
        DbgPrint("   ProbeDelay         = ");   DbgPrint(NumStr(profile->zFlags.cfg.ProbeDelay, 10));               DbgPrint("\n");
        DbgPrint("   ProbeEnergyTO      = ");   DbgPrint(NumStr(profile->zFlags.cfg.ProbeEnergyTO, 10));            DbgPrint("\n");
        DbgPrint("   ProbeRespTO        = ");   DbgPrint(NumStr(profile->zFlags.cfg.ProbeRespTO, 10));              DbgPrint("\n");
        DbgPrint("   BeaconListenTO     = ");   DbgPrint(NumStr(profile->zFlags.cfg.BeaconListenTO, 10));           DbgPrint("\n");
        DbgPrint("   JoinNetTO          = ");   DbgPrint(NumStr(profile->zFlags.cfg.JoinNetTO, 10));                DbgPrint("\n");
        DbgPrint("   AuthenTO           = ");   DbgPrint(NumStr(profile->zFlags.cfg.AuthenTO, 10));                 DbgPrint("\n");
        DbgPrint("   AuthenType         = ");   DbgPrint(NumStr(profile->zFlags.cfg.AuthenType, 10));               DbgPrint("\n");
        DbgPrint("   AssociationTO      = ");   DbgPrint(NumStr(profile->zFlags.cfg.AssociationTO, 10));            DbgPrint("\n");
        DbgPrint("   SpecAPTO           = ");   DbgPrint(NumStr(profile->zFlags.cfg.SpecAPTO, 10));                 DbgPrint("\n");
        DbgPrint("   OfflineScanInterv  = ");   DbgPrint(NumStr(profile->zFlags.cfg.OfflineScanInterv, 10));        DbgPrint("\n");
        DbgPrint("   OfflineScanDuar    = ");   DbgPrint(NumStr(profile->zFlags.cfg.OfflineScanDuar, 10));          DbgPrint("\n");
        DbgPrint("   LinkLossDelay      = ");   DbgPrint(NumStr(profile->zFlags.cfg.LinkLossDelay, 10));            DbgPrint("\n");
        DbgPrint("   MaxBeaconLT        = ");   DbgPrint(NumStr(profile->zFlags.cfg.MaxBeaconLT, 10));              DbgPrint("\n");
        DbgPrint("   ResfreshInterv     = ");   DbgPrint(NumStr(profile->zFlags.cfg.ResfreshInterv, 10));           DbgPrint("\n");

        DbgPrint("   RESERVED2          = ");   
        for (i = 0; i < 2; i++) {
            DbgPrint("0x");
//            DbgPrint(NumStr(profile->zFlags.cfg.RESERVED2[i], 16));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        DbgPrint("   PowerSaveMode      = ");   DbgPrint(NumStr(profile->zFlags.cfg.PowerSaveMode, 10));            DbgPrint("\n");
        DbgPrint("   RcvDTIM            = ");   DbgPrint(NumStr(profile->zFlags.cfg.RcvDTIM, 10));                  DbgPrint("\n");
        DbgPrint("   ListenInterv       = ");   DbgPrint(NumStr(profile->zFlags.cfg.ListenInterv, 10));             DbgPrint("\n");
        DbgPrint("   FastListenInterv   = ");   DbgPrint(NumStr(profile->zFlags.cfg.FastListenInterv, 10));         DbgPrint("\n");
        DbgPrint("   ListenDecay        = ");   DbgPrint(NumStr(profile->zFlags.cfg.ListenDecay, 10));              DbgPrint("\n");
        DbgPrint("   FastListenDecay    = ");   DbgPrint(NumStr(profile->zFlags.cfg.FastListenDecay, 10));          DbgPrint("\n");

        DbgPrint("   RESERVED3          = ");   
        for (i = 0; i < 4; i++) {
            DbgPrint("0x");
            DbgPrint(NumStr(profile->zFlags.cfg.RESERVED3[i], 16));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        DbgPrint("   BeaconPeriod       = ");   DbgPrint(NumStr(profile->zFlags.cfg.BeaconPeriod, 10));             DbgPrint("\n");
        DbgPrint("   AtimDuration       = ");   DbgPrint(NumStr(profile->zFlags.cfg.AtimDuration, 10));             DbgPrint("\n");
        DbgPrint("   HopPeriod          = ");   DbgPrint(NumStr(profile->zFlags.cfg.HopPeriod, 10));                DbgPrint("\n");
        DbgPrint("   HopSet             = ");   DbgPrint(NumStr(profile->zFlags.cfg.HopSet, 10));                   DbgPrint("\n");
        DbgPrint("   DSChannel          = ");   DbgPrint(NumStr(profile->zFlags.cfg.DSChannel, 10));                DbgPrint("\n");
        DbgPrint("   HopPattern         = ");   DbgPrint(NumStr(profile->zFlags.cfg.HopPattern, 10));               DbgPrint("\n");
        DbgPrint("   DTIMPeriod         = ");   DbgPrint(NumStr(profile->zFlags.cfg.DTIMPeriod, 10));               DbgPrint("\n");

        DbgPrint("   RESERVED4          = ");   
        for (i = 0; i < 4; i++) {
            DbgPrint("0x");
  //          DbgPrint(NumStr(profile->zFlags.cfg.RESERVED4[i], 16));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        DbgPrint("   RadioType          = ");   DbgPrint(NumStr(profile->zFlags.cfg.RadioType, 10));                DbgPrint("\n");
        DbgPrint("   Diversity          = ");   DbgPrint(NumStr(profile->zFlags.cfg.Diversity, 10));                DbgPrint("\n");
        DbgPrint("   TransmitPower      = ");   DbgPrint(NumStr(profile->zFlags.cfg.TransmitPower, 10));            DbgPrint("\n");
        DbgPrint("   RSSIThreshhold     = ");   DbgPrint(NumStr(profile->zFlags.cfg.RSSIThreshhold, 10));           DbgPrint("\n");
        DbgPrint("   Modulation         = ");   DbgPrint(NumStr(profile->zFlags.cfg.Modulation, 10));               DbgPrint("\n");
        DbgPrint("   ShortHeaders       = ");   DbgPrint(NumStr(profile->zFlags.cfg.ShortHeaders, 10));             DbgPrint("\n");
        DbgPrint("   HomeConfiguration  = ");   DbgPrint(NumStr(profile->zFlags.cfg.HomeConfiguration, 10));        DbgPrint("\n");

        DbgPrint("   RESERVED5          = ");   
        for (i = 0; i < 2; i++) {
            DbgPrint("0x");
            DbgPrint(NumStr(profile->zFlags.cfg.RESERVED5[i], 16));
            DbgPrint(", ");
            }
        DbgPrint("\n");

        char s[17];
        NdisZeroMemory(s, 17);
        memcpy(s, profile->zFlags.cfg.NodeName, 16);
        DbgPrint("   NodeName           = ");   DbgPrint(s);                                                        DbgPrint("\n");
        DbgPrint("   ARLThreshhold      = ");   DbgPrint(NumStr(profile->zFlags.cfg.ARLThreshhold, 10));            DbgPrint("\n");
        DbgPrint("   ARLDecay           = ");   DbgPrint(NumStr(profile->zFlags.cfg.ARLDecay, 10));                 DbgPrint("\n");
        DbgPrint("   ARLDelay           = ");   DbgPrint(NumStr(profile->zFlags.cfg.ARLDelay, 10));                 DbgPrint("\n");
        DbgPrint("   Reserved1          = ");   DbgPrint(NumStr(profile->zFlags.cfg.Reserved1, 10));                DbgPrint("\n");
        DbgPrint("   MagicMode          = ");   DbgPrint(NumStr(profile->zFlags.cfg.MagicMode, 10));                DbgPrint("\n");
        DbgPrint("   MaxPowerSave       = ");   DbgPrint(NumStr(profile->zFlags.cfg.MaxPowerSave, 10));             DbgPrint("\n");
        }
}

#endif

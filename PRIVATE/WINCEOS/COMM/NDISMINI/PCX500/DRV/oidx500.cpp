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
// oidX500.cpp
//---------------------------------------------------------------------------
// 
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 10/23/00     jbeaujon    -Added OID_X500_SOFT_RESET, OID_X500_GET_CURRENT_PROFILE.
//
// 05/25/01     spb006      -Removed GET_ACTIVE_PROFILE so ACU will think we
//                           aren't a new driver he will turn of auto profile switching
//                           For Cisco West Coast LEAP and auto profile problem
//
// 05/28/01     spb008      -Moved m_IsFlashing to before FlashInit called
//
// 06/12/01     spb010      -Added a capability oid.  Also flattened the rid/reg
//                           buff passed from NDIS because there was a 
//                           "context" issue with the pointer because
//                           NDIS has no way of knowing that it is a pointer
//                           to another buffer and so the memory was being
//                           paged out.
//
// 07/13/01     spb021      -Fixed spb010 so all buffer offsets work ulongs
//
// 07/23/01     spb026      -Fixed OID_QUERY_RADIO_STATE so that we don't
//                           access the card.  It shouldn't matter if 
//                           the card is awake at that point
//
// 08/01/01     spb027      -Defined iscardinserted in header file.
//
//---------------------------------------------------------------------------
#pragma code_seg("LCODE")
#include "NDISVER.h"

extern "C"{
    #include <ndis.h>
}

#include "Aironet.h"
#include "HWX500P.h"
#include "CardX500.h"
#include <memory.h>
#include <AiroOid.h>
#include "flash.h"
#include "debug.h"
#include "profile.h"
#include "version.h"


int AironetSupportedOids[] = {
    STANDARD_OIDS,
//
    OID_RADIO_ON,           
    OID_RADIO_OFF,          
    OID_QUERY_RADIO_STATE,  
    OID_GETVERSION,                 
//
    OID_GET_REGISTRATION_STATUS,
    OID_GET_REGISTERED_ROUTER,
//
    OID_GET_CONFIGUATION,
    OID_SET_CONFIGUATION,
    OID_GET_STATUS,
    OID_GET_STATISTICS,
    OID_RESET_STATISTICS,
//
    OID_X500_GET_SIGNAL_STRENGTH,
    OID_X500_GET_SIGNAL_QUALITY,
    OID_X500_GET_SIGNAL_PARAMS,
    OID_X500_GET_REGISTERED_ROUTER,
    OID_X500_GET_REGISTRATION_STATUS,
    OID_X500_GET_SYSTEM_ID,
    OID_X500_GOTO_SLOW_POLL,
    OID_X500_GET_CONFIGURATION,
    OID_X500_SET_CONFIGURATION,
    OID_X500_GET_APS,
    OID_X500_SET_APS,
    OID_X500_GET_SSIDS,
    OID_X500_SET_SSIDS,
    OID_X500_GET_STATISTICS,
    OID_X500_GET_STATUS,
    OID_X500_RESET_STATS,
    OID_X500_GET_CAPS,
    OID_X500_FLASH_OPEN,
    OID_X500_FLASH_PROGRESS,
    OID_X500_FLASH_WRITE,
    OID_X500_FLASH_CLOSE,
    OID_X500_QUERY_RADIO_TYPE,
    OID_X500_QUERY_ADAPTER_TYPE,
    OID_X500_ISINSERTED,
    OID_X500_HARD_RESET,
    OID_X500_GET_32STATISTICS,
    OID_X500_RESET_32STATISTICS,
    OID_X500_READ_RID,
    OID_X500_WRITE_RID,             // can't write config RID, use config function
    OID_X500_GET_BUSTYPE,           // can't write config RID, use config function
    OID_X500_AWAKEN,
    OID_X500_SLEEP,
    OID_X500_KEEPAWAKE,
    OID_X500_ISMAXPOWERSAVEON,
    OID_X500_READBUF,
    OID_X500_WRITEBUF,
    OID_X500_READAUXBUF,
    OID_X500_WRITEAUXBUF,
    OID_X500_GET_COREDUMP,
    OID_X500_WRITE_RID_IGNORE_MAC,
    OID_X500_SOFT_RESET,

    OID_X500_GET_ACTIVE_PROFILE,
    OID_X500_SET_ACTIVE_PROFILE,
    OID_X500_SELECT_PROFILE,
    OID_X500_GET_PROFILE,
    OID_X500_SET_PROFILE,
    OID_X500_GET_CAPABILITY,
};

int
GetSupOidSize()
{
    return sizeof(AironetSupportedOids);
}

//spb027 BOOLEAN IsCardInserted( PCARD card );

NDIS_STATUS 
ExtendedOids(
    PCARD           card,
    IN NDIS_OID     Oid,
    IN PVOID        InfBuff,        // also OutBuff
    IN ULONG        InfBuffLen,     // also OutBuffLen
    OUT PULONG      BytesCopied,
    OUT PULONG      BytesNeeded
    ) 
{
    NDIS_STATUS             StatusToReturn  = NDIS_STATUS_SUCCESS;
    LONG                    *ReturnCode = (LONG *)BytesCopied;

    #ifdef DEBUG_AUTO_CONFIG_OID
    static int profileOIDCount = 0;
    #endif
    
    *BytesNeeded = 0;
    *BytesCopied = 0;
    switch( Oid ){

    case OID_X500_GET_COREDUMP: {
        if( NULL == InfBuff || 0==InfBuffLen){
            *ReturnCode = sizeof(CARD);
            break;
        }

        if( InfBuffLen < sizeof(CARD) ){
            *ReturnCode = -1;
            break;
        }
        *(CARD *)InfBuff =  *card;
        *ReturnCode = sizeof(CARD);
        break;
        }

    case OID_X500_ISINSERTED: {
        if (InfBuffLen >= sizeof(ULONG)) {
            *(ULONG *)InfBuff =  IsCardInserted(card) ? 1 : 0;
            *ReturnCode = sizeof(ULONG);
            }
        else {
            *ReturnCode = -1;
            }
        break;
        }
        
    case OID_X500_HARD_RESET: {
        
        if(!card->m_AttribMemRegistered || (! cmdAwaken(card, TRUE )) ) { //spbMgc
            *ReturnCode = -1000;
            break;
        }
        NdisWriteRegisterUchar( card->m_pAttribMemBase+0x3e0, 0x80 );
        DelayMS(50);
        NdisWriteRegisterUchar( card->m_pAttribMemBase+0x3e0, 0x00 );
        DelayMS(700);
        NdisWriteRegisterUchar( card->m_pAttribMemBase+0x3e0, 0x45 );
        DelayMS(50);
        *ReturnCode = 0;
        break;
        }

    case OID_X500_SOFT_RESET: {
        InitFW(card, TRUE);
        break;
        }

    case OID_RADIO_ON: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        card->m_CardStarted = TRUE;
        *ReturnCode =  cmdEnable( card ) ? 0 : (ULONG)-1;
        break;
        }

    case OID_RADIO_OFF: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        card->m_CardStarted = FALSE;
        *ReturnCode =  cmdDisable( card )  ? 0 : (ULONG)-1;
        break;
        }

    case OID_X500_ISAWAKE: {
        if (InfBuffLen >= sizeof(ULONG)) {
            *(ULONG *)InfBuff   = (ULONG)card->IsAwake; 
            *ReturnCode = sizeof(ULONG); 
            }
        else {
            *ReturnCode = -1;
            }
        break;
        }

    case OID_X500_AWAKEN: {
        //if( ! cmdAwaken(card, TRUE ) ){
        //  *ReturnCode = -1000;
        //  break;
        //}
        *ReturnCode =  0;
        break;
        }

    case OID_X500_SLEEP: {
        //cmdSleep(card );
        //card->KeepAwake = 0;
        *ReturnCode     = 0;
        break;
        }
    
    case OID_X500_KEEPAWAKE: {
        card->KeepAwake = *(ULONG *)InfBuff ? 3 : 0;
        break;
        }

    case OID_X500_ISMAXPOWERSAVEON: {
        if (InfBuffLen >= sizeof(ULONG)) {
            *(ULONG *)InfBuff   = card->m_MaxPSP ? 1 : 0; 
            *ReturnCode = sizeof(ULONG); 
            }
        else {
            *ReturnCode = -1;
            }
        break;
        }

    case OID_QUERY_RADIO_STATE: {
        if (InfBuffLen >= sizeof(ULONG)) {
//spb026            if (!cmdAwaken(card, TRUE)) {
//                *ReturnCode = -1000;
//                break;
//                }
            *(ULONG *)InfBuff   = (ULONG)card->m_IsMacEnabled; 
            *ReturnCode = sizeof(ULONG); 
            }
        else {
            *ReturnCode = -1;
            }
        break;
        }

    case OID_GETVERSION: {

        *BytesCopied = strlen(Ver_FileVersionStr);   

        if (*BytesCopied < InfBuffLen) {
            strcpy((char *)InfBuff, Ver_FileVersionStr);
            }
        else {
            memcpy((char *)InfBuff, Ver_FileVersionStr, InfBuffLen);

            ((char*)InfBuff)[InfBuffLen - 1] = 0;

            *BytesCopied = InfBuffLen - 1;
            }

/*
        *BytesCopied = strlen(Ver_FileVersionStr);   

        if( *BytesCopied < InfBuffLen )
            strcpy( (char *)InfBuff, Ver_FileVersionStr );
        else{
            memcpy((char *)InfBuff, Ver_FileVersionStr, InfBuffLen );
            ((char *)InfBuff)[ InfBuffLen-1 ] = 0;
            *BytesCopied = InfBuffLen-1;
        }
*/
        
        break;
        }

    case OID_X500_GET_BUSTYPE: {

        *BytesCopied = strlen(card->m_FormFactor);   
        
        if (*BytesCopied < InfBuffLen) {
            strcpy((char *)InfBuff, (char *)card->m_FormFactor);
            }
        else {
            memcpy((char *)InfBuff, card->m_FormFactor, InfBuffLen);

            ((char *)InfBuff)[InfBuffLen - 1] = 0;

            *BytesCopied = InfBuffLen - 1;
            }

/*
        *BytesCopied = strlen(card->m_FormFactor);   
        
        if( *BytesCopied < InfBuffLen )
            strcpy( (char *)InfBuff, (char *)card->m_FormFactor );
        else {
            memcpy((char *)InfBuff, card->m_FormFactor, InfBuffLen );
            ((char *)InfBuff)[ InfBuffLen-1 ] = 0;
            *BytesCopied = InfBuffLen-1;
        }
*/
        break;
        }
        
//
    case OID_GET_REGISTERED_ROUTER:
    case OID_X500_GET_REGISTERED_ROUTER: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        else
        {
            STSTATUS    status;
            BOOLEAN ret = cmdStatusGet(card, &status ); 
            if( FALSE == ret ){
                *ReturnCode = (ULONG)-1;
                break;
            }
            *BytesCopied    = MIN(sizeof(status.au8CurrentBssid), InfBuffLen); 
            NdisMoveMemory(InfBuff, status.au8CurrentBssid, *BytesCopied);
        }
        break;
        }

    case OID_GET_REGISTRATION_STATUS:
    case OID_X500_GET_REGISTRATION_STATUS: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        {
            STSTATUS    status;
            BOOLEAN ret = cmdStatusGet(card, &status ); 
            if( FALSE == ret ){
                *ReturnCode = (ULONG)-1;
                break;
            }
            if (InfBuffLen >= sizeof(UINT)) {
                *(UINT *)InfBuff    = (0x20 & status.u16OperationalMode ) ? 1 : 0;
                *BytesCopied        = sizeof(UINT); 
                }
            else {
                *ReturnCode = -1; 
                }
        }   
        break;
        }

    case OID_X500_GET_SYSTEM_ID: {
        *BytesCopied = MIN(32, InfBuffLen);
//        NdisMoveMemory( InfBuff, card->m_ESS_ID1, *BytesCopied);
        NdisMoveMemory(InfBuff, card->m_activeProfile->zFlags.SSIDList.ID1, *BytesCopied);
        break;
        }


#if 0   //spb006
    case OID_X500_GET_ACTIVE_PROFILE: {
        #ifdef DEBUG_AUTO_CONFIG_OID
        profileOIDCount++;
        DbgPrint("%d\n", profileOIDCount);
        
        DbgPrint("OID_X500_GET_ACTIVE_PROFILE:\n");
        listProfiles(card->m_profiles, card->m_numProfiles);
        DbgPrint("\n");
        #endif

        // 
        // Return the currently active profile (i.e. the one in the card).
        // 
        *BytesCopied = MIN(sizeof(PROFILE), InfBuffLen);
        NdisMoveMemory(InfBuff, &card->m_activeProfile->zFlags.cfg, *BytesCopied);

        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("   current = %s; active = %s; default = %s\n",
                card->m_currentProfile->properties.name,
                card->m_activeProfile->properties.name,
                card->m_profiles->properties.name);
        DbgPrint("------------------------------------------------------------------\n");
        #endif

        break;
        }
#endif

    case OID_X500_SET_ACTIVE_PROFILE: {

        PROFILE *profile = (PROFILE*)InfBuff;

        #ifdef DEBUG_AUTO_CONFIG_OID
        profileOIDCount++;
        DbgPrint("%d\n", profileOIDCount);
        DbgPrint("OID_X500_SET_ACTIVE_PROFILE(%s):\n", profile->properties.name);
        listProfiles(card->m_profiles, card->m_numProfiles);
        DbgPrint("\n");
        #endif

        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        USHORT index;
        STPROFILE *stProfile = findProfileByName(profile->properties.name,
                                               card->m_profiles,
                                               card->m_numProfiles,
                                               &index);

        if (stProfile == NULL) {
            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("Profile %s not found -- adding it\n", profile->properties.name);
            #endif
            stProfile = addProfile(card, profile);
            }

        if (stProfile != NULL) {

            card->m_activeProfile       = stProfile;
            card->m_activeProfileIndex  = index;

            // 
            // Set the currently active profile (i.e. the one in the card).
            // 
            *BytesCopied = sizeof(PROFILE);
            // 
            // update in-memory copy.
            // 
            NdisMoveMemory(&card->m_activeProfile->zFlags.cfg, profile, *BytesCopied);
            // 
            // write to the card.
            // 
            cmdConfigSet(card, &(profile->zFlags.cfg));
            cmdSSIDSet(card, &(profile->zFlags.SSIDList)); 
            cmdAPsSet(card, &(profile->zFlags.APList)); 

//            card->m_activeProfileSwitched = TRUE;

            setAutoConfigTimer(card);

            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("   InfBuffLen = %d\n", (int)InfBuffLen);
            DbgPrint("   sizeof(PROFILE) = %d\n", (int)sizeof(PROFILE));
            DbgPrint("   Profile name = %s\n", card->m_activeProfile->properties.name);
            #endif

            }

        else {
            *BytesCopied = (ULONG)-1;

            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("   Error adding profile.\n");
            #endif
            }

        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("   current = %s; active = %s; default = %s\n",
                 card->m_currentProfile->properties.name,
                 card->m_activeProfile->properties.name,
                 card->m_profiles->properties.name);
        DbgPrint("------------------------------------------------------------------\n");
        #endif

        break;
        }

    case OID_X500_SELECT_PROFILE: {

        PROFILE_PROPERTIES  *profileProps = (PROFILE_PROPERTIES*)InfBuff;

        #ifdef DEBUG_AUTO_CONFIG_OID
        profileOIDCount++;
        DbgPrint("%d\n", profileOIDCount);
        DbgPrint("OID_X500_SELECT_PROFILE(%s):\n", profileProps->name);
        listProfiles(card->m_profiles, card->m_numProfiles);
        DbgPrint("\n");
        #endif

        *BytesCopied = sizeof(PROFILE_PROPERTIES);
        // 
        // Select the current profile by name.
        // 
        // First, see if we have a profile with the specified name.
        // 
        USHORT index;
        STPROFILE *profile = findProfileByName(profileProps->name,
                                               card->m_profiles,
                                               card->m_numProfiles,
                                               &index);
        if (profile != NULL) {
            // 
            // found one.
            // 
            card->m_currentProfile = profile;

            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("   Selected profile %s.\n");
            #endif

            }
        else {
            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("Profile %s not found -- adding it\n", profileProps->name);
            #endif

            // 
            // profile doesn't exist so add it.
            // 
            if (addProfile(card, profileProps) == NULL) {
                *BytesCopied = (ULONG)-1;

                #ifdef DEBUG_AUTO_CONFIG_OID
                DbgPrint("   Error adding profile.\n");
                #endif

                }
            }

        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("   current = %s; active = %s; default = %s\n",
                 card->m_currentProfile->properties.name,
                 card->m_activeProfile->properties.name,
                 card->m_profiles->properties.name);
        DbgPrint("------------------------------------------------------------------\n");
        #endif

        break;
        }

    case OID_X500_GET_PROFILE: {

        #ifdef DEBUG_AUTO_CONFIG_OID
        profileOIDCount++;
        DbgPrint("%d\n", profileOIDCount);
        DbgPrint("OID_X500_GET_PROFILE:\n");
        listProfiles(card->m_profiles, card->m_numProfiles);
        DbgPrint("\n");
        #endif

        // 
        // Return the the current profile.
        // 
        *BytesCopied = MIN(sizeof(ZFLAGS), InfBuffLen);
        NdisMoveMemory(InfBuff, &card->m_currentProfile->zFlags.cfg, *BytesCopied);

        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("   current = %s; active = %s; default = %s\n",
                 card->m_currentProfile->properties.name,
                 card->m_activeProfile->properties.name,
                 card->m_profiles->properties.name);
        DbgPrint("------------------------------------------------------------------\n");
        #endif

        break;
        }

    case OID_X500_SET_PROFILE: {

        #ifdef DEBUG_AUTO_CONFIG_OID
        profileOIDCount++;
        DbgPrint("%d\n", profileOIDCount);
        DbgPrint("OID_X500_SET_PROFILE:\n");
        listProfiles(card->m_profiles, card->m_numProfiles);
        DbgPrint("\n");
        #endif

        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }
        // 
        // Set the current profile.
        // 
        *BytesCopied = sizeof(ZFLAGS);
        NdisMoveMemory(&card->m_currentProfile->zFlags.cfg, InfBuff, *BytesCopied);
        // 
        // If the current profile is also active then write to the card too.
        // 
        if (card->m_currentProfile == card->m_activeProfile) {

            #ifdef DEBUG_AUTO_CONFIG_OID
            DbgPrint("   current == active");
            DbgPrint("\n");
            #endif

            cmdConfigSet(card, &((ZFLAGS*)InfBuff)->cfg);
            cmdSSIDSet(card, &((ZFLAGS*)InfBuff)->SSIDList); 
            cmdAPsSet(card, &((ZFLAGS*)InfBuff)->APList); 
            }

        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("   current = %s; active = %s; default = %s\n",
                 card->m_currentProfile->properties.name,
                 card->m_activeProfile->properties.name,
                 card->m_profiles->properties.name);
        DbgPrint("------------------------------------------------------------------\n");
        #endif

        break;
        }

    case OID_X500_ENABLE_AUTO_PROFILE_SWITCH: {
        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("OID_X500_ENABLE_AUTO_PROFILE_SWITCH:\n");
        #endif
        card->m_tempDisableAutoSwitch = FALSE;
        break;
        }

    case OID_X500_DISABLE_AUTO_PROFILE_SWITCH: {
        #ifdef DEBUG_AUTO_CONFIG_OID
        DbgPrint("OID_X500_DISABLE_AUTO_PROFILE_SWITCH:\n");
        #endif
        card->m_tempDisableAutoSwitch = TRUE;
        break;
        }

    case OID_GET_CONFIGUATION:
    case OID_X500_GET_CONFIGURATION: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(CFG_X500), InfBuffLen);

        CFG_X500 cfg;
        if (cmdConfigGet(card, &cfg)) {
            NdisMoveMemory(InfBuff, &cfg, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdConfigGet(card, (CFG_X500 *)InfBuff) ? sizeof(CFG_X500) :(ULONG)-1 ;
*/
        break;
        }

    case OID_X500_EE_GET_CONFIGURATION: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }
        *BytesCopied = MIN(sizeof(CFG_X500), InfBuffLen);

        CFG_X500 cfg;
        if (EEReadConfig(card, &cfg)) {
            NdisMoveMemory(InfBuff, &cfg, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = EEReadConfig(card, (CFG_X500 *)InfBuff) ? sizeof(CFG_X500) :(ULONG)-1 ;
*/
        break;
        }

    case OID_SET_CONFIGUATION:
    case OID_X500_SET_CONFIGURATION: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }
        if (cmdConfigSet(card, (CFG_X500 *)InfBuff)) {
            *BytesCopied =  sizeof(CFG_X500);
            NdisMoveMemory(&card->m_activeProfile->zFlags.cfg, InfBuff, sizeof(CFG_X500));
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
        break;
        }

    case OID_X500_EE_SET_CONFIGURATION: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = EEWriteConfig(card, (CFG_X500 *)InfBuff) ? sizeof(CFG_X500) :(ULONG)-1 ;
        break;
        }

    case OID_X500_GET_APS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STAPLIST), InfBuffLen);

        STAPLIST apList;
        if (cmdAPsGet(card, &apList)) {
            NdisMoveMemory(InfBuff, &apList, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdAPsGet(card, (STAPLIST *)InfBuff)? sizeof(STAPLIST) : (ULONG)-1; 
*/
        break;
        }

    case OID_X500_EE_GET_APS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STAPLIST), InfBuffLen);
        STAPLIST apList;
        if (EEReadAPS(card, &apList)) {
            NdisMoveMemory(InfBuff, &apList, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = EEReadAPS(card, (STAPLIST *)InfBuff)? sizeof(STAPLIST) : (ULONG)-1; 
*/
        break;
        }

    case OID_X500_SET_APS: {
        if (!cmdAwaken(card, TRUE)) { 
            *ReturnCode = -1000;
            break;
            }

        if (cmdAPsSet(card, (STAPLIST *)InfBuff)) {
            *BytesCopied =  sizeof(STAPLIST);
            NdisMoveMemory(&card->m_activeProfile->zFlags.APList, InfBuff, sizeof(STAPLIST));
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
        break;
        }

    case OID_X500_EE_SET_APS: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = EEWriteAPS(card, (STAPLIST *)InfBuff)? sizeof(STAPLIST) : (ULONG)-1; 
    
        break;
        }

    case OID_X500_GET_SSIDS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }
        
        *BytesCopied = MIN(sizeof(STSSID), InfBuffLen);

        STSSID ssidList;
        if (cmdSSIDGet(card, &ssidList)) {
            NdisMoveMemory(InfBuff, &ssidList, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdSSIDGet(card, (STSSID *)InfBuff)? sizeof(STSSID) : (ULONG)-1; 
*/
        break;
        }

    case OID_X500_EE_GET_SSIDS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSSID), InfBuffLen);

        STSSID ssidList;
        if (EEReadSSIDS(card, &ssidList)) {
            NdisMoveMemory(InfBuff, &ssidList, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }

        *BytesCopied = EEReadSSIDS(card, (STSSID *)InfBuff)? sizeof(STSSID) : (ULONG)-1; 
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = EEReadSSIDS(card, (STSSID *)InfBuff)? sizeof(STSSID) : (ULONG)-1; 
*/
        break;
        }

    case OID_X500_SET_SSIDS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        if (cmdSSIDSet(card, (STSSID *)InfBuff)) {
            *BytesCopied =  sizeof(STSSID);
            NdisMoveMemory(&card->m_activeProfile->zFlags.SSIDList, InfBuff, sizeof(STSSID));
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
        break;
        }

    case OID_X500_EE_SET_SSIDS: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied    = EEWriteSSIDS(card, (STSSID *)InfBuff)? sizeof(STSSID) : (ULONG)-1; 
        break;
        }

    case OID_GET_STATUS:
    case OID_X500_GET_STATUS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSTATUS), InfBuffLen);
        
        STSTATUS status;
        if (cmdStatusGet(card, &status)) {
            NdisMoveMemory(InfBuff, &status, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdStatusGet(card, (STSTATUS *)InfBuff ) ? sizeof(STSTATUS): (ULONG)-1;
*/
        break;
        }

    case OID_GET_STATISTICS:
    case OID_X500_GET_STATISTICS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSTATISTICS), InfBuffLen);

        STSTATISTICS stats;
        if (cmdStatisticsGet(card, &stats)) {
            NdisMoveMemory(InfBuff, &stats, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdStatisticsGet(card, (STSTATISTICS *)InfBuff ) ? sizeof(STSTATISTICS): (ULONG)-1;
*/
        break;
        }
        
    case OID_RESET_STATISTICS:
    case OID_X500_RESET_STATS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSTATISTICS), InfBuffLen);

        STSTATISTICS stats;
        if (cmdStatisticsClear(card, &stats)) {
            NdisMoveMemory(InfBuff, &stats, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdStatisticsClear(card, (STSTATISTICS *)InfBuff ) ? sizeof(STSTATISTICS): (ULONG)-1;
*/
        break;
        }

    case OID_X500_GET_SIGNAL_QUALITY: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = (ULONG)-1;

        STSTATUS status;
        if (cmdStatusGet(card, &status)) {
            if (InfBuffLen >= sizeof(UINT)) {
                *(UINT *)InfBuff    = status.u16SignalStrength;
                *BytesCopied        = sizeof(UINT); 
                }
            }
/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        else
        {
            STSTATUS    status;
            BOOLEAN ret = cmdStatusGet(card, &status ); 
            if( FALSE == ret ){
                *ReturnCode = (ULONG)-1;
                break;
            }
            *(UINT *)InfBuff    = status.u16SignalStrength;
            *BytesCopied        = sizeof(UINT); 
        }   
*/
        break;
        }


    case OID_X500_GET_SIGNAL_STRENGTH:
    case OID_X500_GET_SIGNAL_PARAMS:
    case OID_X500_GOTO_SLOW_POLL: {
        *ReturnCode = (ULONG)-2; 
        break;
        }

    case OID_X500_GET_CAPS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STCAPS), InfBuffLen);

        STCAPS caps;
        if (cmdCapsGet(card, &caps)) {
            NdisMoveMemory(InfBuff, &caps, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }

/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdCapsGet(card, (STCAPS *)InfBuff)? sizeof(STCAPS) : (ULONG)-1; 
*/
        break;
        }

    case OID_X500_FLASH_ADAPTER:
        break;

    case OID_X500_FLASH_PROGRESS: {
        if (InfBuffLen >= sizeof(STFLASH_PROGRESS)) {
            *(STFLASH_PROGRESS*)InfBuff = card->m_FlashProgress; 
            *BytesCopied = sizeof(STFLASH_PROGRESS);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }
/*
        *(STFLASH_PROGRESS*)InfBuff = card->m_FlashProgress; 
        *BytesCopied = sizeof(STFLASH_PROGRESS);
*/
        break;
        }

    case OID_X500_FLASH_OPEN: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        card->KeepAwake = 180;  // 3 minutes
        if( card->m_IsFlashing ){
            *ReturnCode     = -100;
            break;
        }
        card->m_IsFlashing  = TRUE;
        FlashInit(card);
        *ReturnCode     = 0;
        break;
        }

    case OID_X500_FLASH_WRITE: {
        card->KeepAwake = 180;
        if(  ! card->m_IsFlashing ){
            *ReturnCode     = -200;
            break;
        }
        FlashCopyNextBlock(card, (char *)InfBuff);
        *ReturnCode     = 0;
        break;
        }

    case OID_X500_FLASH_CLOSE: {
        if(  ! card->m_IsFlashing ){
            *ReturnCode     = -200;
            break;
        }
        FlashInitTimer(card);
        FlashStartFlashing(card);
        *ReturnCode     = 0;
        break;
        }

    case OID_X500_QUERY_RADIO_TYPE:
    case OID_X500_QUERY_ADAPTER_TYPE: {
        if (InfBuffLen >= sizeof(UINT)) {
            *(UINT *)InfBuff    = card->m_CardType;
            *BytesCopied        = sizeof(UINT); 
            }
        else {
            *BytesCopied = (ULONG)-1; 
            }
/*
        *(UINT *)InfBuff    = card->m_CardType;
        *BytesCopied        = sizeof(ULONG); 
*/
        break;
        }

    case OID_X500_GET_32STATISTICS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSTATISTICS32), InfBuffLen);

        STSTATISTICS32  stats32;
        if (cmdStatistics32Get(card, &stats32)) {
            NdisMoveMemory(InfBuff, &stats32, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }

/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdStatistics32Get(card, (STSTATISTICS32 *)InfBuff ) ? 
            sizeof(STSTATISTICS32): (ULONG)-1;
*/
        break;
        }

    case OID_X500_RESET_32STATISTICS: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
            }

        *BytesCopied = MIN(sizeof(STSTATISTICS32), InfBuffLen);

        STSTATISTICS32 stats32;
        if (cmdStatistics32Clear(card, &stats32)) {
            NdisMoveMemory(InfBuff, &stats32, *BytesCopied);
            }
        else {
            *BytesCopied = (ULONG)-1;
            }

/*
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }
        *BytesCopied = cmdStatistics32Clear(card, (STSTATISTICS32 *)InfBuff ) ? 
            sizeof(STSTATISTICS32): (ULONG)-1;
*/
        break;
        }

    case OID_X500_READ_RID: {
        if (!cmdAwaken(card, TRUE )) {
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;
        if (NULL == InfBuff) {
            break;
        }

        void        *BufPtr     =   ((STRIDACCESS*)InfBuff)->BufPtr;
        int         BufLen      =   ((STRIDACCESS*)InfBuff)->BufLen;
        USHORT      RID         =   ((STRIDACCESS*)InfBuff)->RID;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            STRIDACCESS_V2 *ptr=(STRIDACCESS_V2*)InfBuff;

            RID     = ((STRIDACCESS_V2*)InfBuff)->RID;
            BufPtr  = (UCHAR *)InfBuff + ((STRIDACCESS_V2*)InfBuff)->BufOffset;
            BufLen  = ((STRIDACCESS_V2*)InfBuff)->BufLen;
        }

        if (NULL != BufPtr) {
            if (RidGet(card, (USHORT)RID, BufPtr, BufLen)) {
                *BytesCopied = BufLen;
            }
        }
    break;
    }
    
    case OID_X500_WRITE_RID: {
        if (!cmdAwaken(card, TRUE)) {
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        USHORT      RID         =   ((STRIDACCESS*)InfBuff)->RID;
        void        *BufPtr     =   ((STRIDACCESS*)InfBuff)->BufPtr;
        int         BufLen      =   ((STRIDACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            RID     = ((STRIDACCESS_V2*)InfBuff)->RID;
            BufPtr  = (UCHAR *)InfBuff + ((STRIDACCESS_V2*)InfBuff)->BufOffset;
            BufLen  = ((STRIDACCESS_V2*)InfBuff)->BufLen;
        }

        if (NULL != BufPtr) {
            // Save to in-memory copy.
            if (RID_CONFIG == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.cfg, BufPtr, sizeof(CFG_X500));
            }
            else if (RID_SSID == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.SSIDList, BufPtr, sizeof(STSSID));
            }
            else if (RID_APP == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.APList, BufPtr, sizeof(STAPLIST));
            }

            if ( RidSet(card, RID, BufPtr, BufLen)) {
                *BytesCopied = BufLen ;
            }
        }
    break;
    }
    
    case OID_X500_WRITE_RID_IGNORE_MAC: {
        if( ! cmdAwaken(card, TRUE ) ) {
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        USHORT      RID         =   ((STRIDACCESS*)InfBuff)->RID;
        void        *BufPtr     =   ((STRIDACCESS*)InfBuff)->BufPtr;
        int         BufLen      =   ((STRIDACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            RID     = ((STRIDACCESS_V2*)InfBuff)->RID;
            BufPtr  = (UCHAR *)InfBuff + ((STRIDACCESS_V2*)InfBuff)->BufOffset;
            BufLen  = ((STRIDACCESS_V2*)InfBuff)->BufLen;
        }


        if (NULL != BufPtr) {
            // Save to in-memory copy.
            if (RID_CONFIG == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.cfg, BufPtr, sizeof(CFG_X500));
            }
            else if (RID_SSID == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.SSIDList, BufPtr, sizeof(STSSID));
            }
            else if (RID_APP == (RID_X500)RID) {
                NdisMoveMemory(&card->m_activeProfile->zFlags.APList, BufPtr, sizeof(STAPLIST));
            }

            if ( RidSetIgnoreMAC(card, RID, BufPtr, BufLen) ) {
                *BytesCopied = BufLen ;
            }
        }
    break;
    }

    case OID_X500_READBUF: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        int     Regoffset   =   ((STREGACCESS*)InfBuff)->Regoffset;
        void    *BufPtr     =   ((STREGACCESS*)InfBuff)->BufPtr;
        int     BufLen      =   ((STREGACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            Regoffset   = ((STREGACCESS_V2*)InfBuff)->Regoffset;
            BufPtr      = (UCHAR *)InfBuff + ((STREGACCESS_V2*)InfBuff)->BufOffset;
            BufLen      = ((STREGACCESS_V2*)InfBuff)->BufLen;
        }


        if (NULL != BufPtr) {
            X500ReadBytes(card, BufPtr, (UINT)Regoffset, (USHORT)BufLen );
            *BytesCopied = (ULONG)BufLen;
        }
    break;
    }

    case OID_X500_WRITEBUF: {
        if( ! cmdAwaken(card, TRUE ) ) {
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        int     Regoffset   =   ((STREGACCESS*)InfBuff)->Regoffset;
        void    *BufPtr     =   ((STREGACCESS*)InfBuff)->BufPtr;
        int      BufLen      =   ((STREGACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            Regoffset   = ((STREGACCESS_V2*)InfBuff)->Regoffset;
            BufPtr      = (UCHAR *)InfBuff + ((STREGACCESS_V2*)InfBuff)->BufOffset;
            BufLen      = ((STREGACCESS_V2*)InfBuff)->BufLen;
        }

        if (NULL != BufPtr) {
            X500WriteBytes(card, BufPtr, (UINT)Regoffset, (USHORT)BufLen );
            *BytesCopied = (ULONG)BufLen;
        }
    break;
    }

    case OID_X500_READAUXBUF: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        int     Regoffset   =   ((STREGACCESS*)InfBuff)->Regoffset;
        void    *BufPtr     =   ((STREGACCESS*)InfBuff)->BufPtr;
        int     BufLen      =   ((STREGACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            Regoffset   = ((STREGACCESS_V2*)InfBuff)->Regoffset;
            BufPtr      = (UCHAR *)InfBuff + ((STREGACCESS_V2*)InfBuff)->BufOffset;
            BufLen      = ((STREGACCESS_V2*)InfBuff)->BufLen;
        }

        if (NULL != BufPtr) {
            X500ReadAuxBytes(card, BufPtr, (UINT)Regoffset,  (USHORT)BufLen );
            *BytesCopied =  (ULONG)BufLen;
        }
    break;
    }

    case OID_X500_WRITEAUXBUF: {
        if( ! cmdAwaken(card, TRUE ) ){
            *ReturnCode = -1000;
            break;
        }

        *BytesCopied = (ULONG)-1;

        if (NULL == InfBuff) {
            break;
        }

        int     Regoffset   =   ((STREGACCESS*)InfBuff)->Regoffset;
        void    *BufPtr     =   ((STREGACCESS*)InfBuff)->BufPtr;
        int     BufLen      =   ((STREGACCESS*)InfBuff)->BufLen;

        if (NULL == BufPtr && !BufLen) {
            //Assume it is a "new" flat buffer  spb010
            Regoffset   = ((STREGACCESS_V2*)InfBuff)->Regoffset;
            BufPtr      = (UCHAR *)InfBuff + ((STREGACCESS_V2*)InfBuff)->BufOffset;
            BufLen      = ((STREGACCESS_V2*)InfBuff)->BufLen;
        }

        if (NULL != BufPtr) {
            X500WriteAuxBytes(card, BufPtr, (UINT)Regoffset , (USHORT)BufLen );
            *BytesCopied =  (ULONG)BufLen;
        }
    break;
    }

    case OID_X500_GET_CAPABILITY: {
        //spb010
        STDRIVERCAPS caps;
        USHORT size=sizeof(STDRIVERCAPS);
        NdisZeroMemory(&caps,size);

        caps.Size                   =   size;
        caps.AutoProfile            =   FALSE;
        caps.ProfileVersion         =   1;
        caps.RIDInterfaceVersion    =   2;
        caps.MaxPSP                 =   TRUE;
        caps.MagicPacket            =   FALSE;

        strcpy((char *)caps.VendorName,"Cisco");
        strcpy((char *)caps.DriverVersion,VER_FILEVERSION_STR);
        
        NdisMoveMemory(InfBuff,&caps,size);
    break;
    }

    default: {
        return NDIS_STATUS_INVALID_OID;
        }
    }
    
    return StatusToReturn;
}




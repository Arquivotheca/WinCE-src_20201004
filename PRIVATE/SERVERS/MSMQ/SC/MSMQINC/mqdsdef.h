//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

	mqdsdef.h

Abstract:

	Message Queuing's Directory Service defines

--*/
#ifndef __MQDSDEF_H__
#define __MQDSDEF_H__


//********************************************************************
//				DS object types
//********************************************************************

// dwObjectType values
#define	MQDS_QUEUE		    1
#define MQDS_MACHINE	    2
#define	MQDS_SITE		    3
#define MQDS_DELETEDOBJECT	4
#define MQDS_CN			    5
#define MQDS_ENTERPRISE	    6
#define MQDS_USER           7
#define MQDS_SITELINK       8

#define MQDS_PURGE		    9
#define MQDS_BSCACK		    10

//  for internal use only
#define MQDS_MACHINE_CN     11

//
// MAX_MQIS_TYPES is used in NT5 replication service, as array size for
// several arrays which map from object type to propid.
//
#define MAX_MQIS_TYPES      11

//
//  ADS objects
//
#define MQDS_SERVER     50
#define MQDS_SETTING    51
#define MQDS_COMPUTER   52
    //
    //  This is a temporary object : until msmq is in NT5 schema.
    //  It is required for displaying MSMQ queues on left pane of MMC
    //
#define MQDS_LEFTPANEQUEUE 53

//
// special types for migration and replication.
// MQDS_NT4_USER- used to insert users with given SID. When using MQDS_USER,
//    it's the mqdssrv code which create the SID field, not the caller.
//
#define MQDS_NT4_USER   100

#endif


//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef PC_NET_PROG_H
#define PC_NET_PROG_H

#include <list.hxx>
#include <sspi.h>

/*
 *  parameters for NT NEG packet
 */
#define CAP_RAW_MODE            0x0001 //The server supports SMB_COM_READ_RAW and SMB_COM_WRITE_RAW. 
#define CAP_MPX_MODE            0x0002 //The server supports SMB_COM_READ_MPX and SMB_COM_WRITE_MPX. 
#define CAP_UNICODE             0x0004 //The server supports Unicode strings. 
#define CAP_LARGE_FILES         0x0008 //The server supports large files with 64 bit offsets. 
#define CAP_NT_SMBS             0x0010 //The server supports the commands particular to the NT LM 0.12 dialect. 
#define CAP_RPC_REMOTE_APIS     0x0020 //The sever supports remote API requests via RPC. 
#define CAP_STATUS32            0x0040 //The server can respond with 32 bit status codes in Status.Status. 
#define CAP_LEVEL_II_OPLOCKS    0x0080 //The server supports level 2 oplocks. 
#define CAP_LOCK_AND_READ       0x0100 //The server supports the SMB_COM_LOCK_AND_READ command. 
#define CAP_NT_FIND             0x0200 // 
#define CAP_DFS                 0x1000 //This server is DFS aware. 
#define CAP_INFOLEVEL_PASSTHRU  0x2000 //The server supports NT information level requests passing through. 
#define CAP_LARGE_READX         0x4000 //The server supports large reads. 
#define CAP_LARGE_WRITEX        0x8000  
#define CAP_EXTENDED_SECURITY   0x80000000 // extended security

/*
 *  parameters for flags 2 field in SMB
 */
#define SMB_FLAGS2_NT_STATUS_CODE       0x4000 //bit 14
#define SMB_FLAGS2_EXTENDED_SECURITY    0x0800
#define SMB_FLAGS2_UNICODE              0x8000


/*
 * definitions for the APIs supported by this server -- see LAN Manager table
 */
#define API_WShareEnum             0
#define API_NetShareGetInfo        1
#define API_WServerGetInfo        13
#define API_WNetWkstaGetInfo      63     
#define API_DosPrintQGetInfo      70
#define API_PrntQueueGetInfo      77  //0x4D
#define API_PrntQueueDelJob       81
#define API_DosPrintJobPause      82
#define API_DosPrintJobResume     83
#define API_PrntQueueSetInfo     147



/*
 *  Values for SMB_SERVER_INFO_1 (type field)
 */
#define SV_TYPE_WORKSTATION        0x00000001  //All workstations
#define SV_TYPE_SERVER             0x00000002  //All servers
#define SV_TYPE_SQLSERVER          0x00000004  //Any server running with SQL

#define SV_TYPE_DOMAIN_CTRL        0x00000008  //Primary domain controller
#define SV_TYPE_DOMAIN_BAKCTRL     0x00000010  //Backup domain controller
#define SV_TYPE_TIME_SOURCE        0x00000020  //Server running the timesource
                                               //service
#define SV_TYPE_AFP                0x00000040  //Apple File Protocol servers
#define SV_TYPE_NOVELL             0x00000080  //Novell servers
#define SV_TYPE_DOMAIN_MEMBER      0x00000100  //Domain Member
#define SV_TYPE_PRINTQ_SERVER      0x00000200  //Server sharing print queue
#define SV_TYPE_DIALIN_SERVER      0x00000400  //Server running dialin service.
#define SV_TYPE_XENIX_SERVER       0x00000800  //Xenix server
#define SV_TYPE_NT                 0x00001000  //NT server
#define SV_TYPE_WFW                0x00002000  //Server running Windows for
                                               //Workgroups
#define SV_TYPE_SERVER_NT          0x00008000  //Windows NT non DC server
#define SV_TYPE_POTENTIAL_BROWSER  0x00010000  //Server that can run the browser
                                               //service
#define SV_TYPE_BACKUP_BROWSER     0x00020000  //Backup browser server
#define SV_TYPE_MASTER_BROWSER     0x00040000  //Master browser server
#define SV_TYPE_DOMAIN_MASTER      0x00080000  //Domain Master Browser server
#define SV_TYPE_LOCAL_LIST_ONLY    0x40000000  //Enumerate only entries marked
                                               //"local"
#define SV_TYPE_DOMAIN_ENUM        0x80000000  //Enumerate Domains. The pszServer
                                               //and pszDomain parameters must be
                                               //NULL.


//
// Forward declare anything we might need w/o pulling in the main headers
struct SMB_PACKET;

namespace PC_NET_PROG
{   
    DWORD CrackSMB(SMB_HEADER *pRequestSMB, 
                   UINT uiRequestSize, 
                   SMB_HEADER *pResponseSMB, 
                   UINT uiResponseRemaining, 
                   UINT *puiUsed,
                   SMB_PACKET *pSMB);
}


#endif

//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef SMB_ERRORS_H
#define SMB_ERRORS_H


struct ERROR_SMB {
    UCHAR WordCount;
    USHORT ByteCount;
};

/* internal error packer */
#define ERRCLS_SHIFT 14
#define ERR_MASK 0x3FFF

/*
 * SMB Error classes
 */
#define SUCCESS       0x0        /* No error                */
#define ERRDOS        0x1        /* Error is a DOS error         */
#define ERRSRV        0x2        /* Error is a server error        */
#define ERRHRD        0x3        /* Error is a hard-error        */
#define ERRXOS        0x4        /* Error is from extended-OS set    */
#define ERRRMX1       0xE1       /* Reserved for RMX...            */
#define ERRRMX2       0xE2
#define ERRRMX3       0xE3
#define ERRCMD        0xFF       /* Error due to bad SMB format        */
#define NTERR         0xC000


//
// Error code for buffer being too small (trans packets)
#define NERRBASE  2100 /* exact same base as NERR_BASE in neterr.h */
#define  NERR_BufTooSmall  (NERRBASE + 23)

/*
 * Error codes for NTERR
 */
#define ERRMoreProcess 0x16
#define ERRLogonFailure 0x6D
/*
 * Error codes for the ERRSRV error class.
 */
#define ERRerror         1
#define ERRbadpw         2
#define ERRbadtype       3
#define ERRaccess        4
#define ERRinvtid        5
#define ERRinvnetname    6
#define ERRinvdevice     7


#define ERRqfull        0x31
#define ERRqtoobig      0x32
#define ERRqeof         0x33
#define ERRinvpfid      0x34
#define ERRsmbcmd       0x40
#define ERRsrverror     0x41
#define ERRbadbid       0x42
#define ERRfilespecs    0x43
#define ERRbadlink      0x44
#define ERRbadpermits   0x45
#define ERRbadpid       0x46
#define ERRsetattrmode  0x47
#define ERRpaused       0x51
#define ERRmsgoff       0x52
#define ERRnoroom       0x53
#define ERRrmuns        0x57
#define ERRtimeout      0x58
#define ERRnoresource   0x59
#define ERRtoomanyuids  0x5A
#define ERRbaduid       0x5B
#define NERR_passwordexpired       NERRBASE+142    /* same as defined in neterr.h */
#define NERR_invalidworkstation    NERRBASE+140    /*     "    */
#define NERR_invalidlogonhours     NERRBASE+141    /*    "    */
#define NERR_accountexpired        NERRBASE+139    /*    "    */
#define ERRuseMPX       0xFA
#define ERRuseSTD       0xFB
#define ERRcontMPX      0xFC
#define ERRBadPW        0xFE   /* Pseudo Error to indicate BadPW from Core
                                * Level Server.                            */
#define ERRnosupport    0xFFFF



/*
 * Error codes for the ERRDOS error class.
 */
#define ERRbadfunc    1
#define ERRbadfile    2
#define ERRbadpath    3
#define ERRnofids     4
#define ERRnoaccess   5
#define ERRbadfid     6
#define ERRbadmcb     7
#define ERRnomem      8
#define ERRbadmem     9
#define ERRbadenv     10
#define ERRbadformat  11
#define ERRbadaccess  12
#define ERRbaddata    13

#define ERRbaddrive   15
#define ERRremcd      16
#define ERRdiffdev    17
#define ERRnofiles    18

#define ERRbadshare   32
#define ERRlock       33

#define ERRfilexists  80


//for TRANSACT
#define ERRNotSupported 50

// 
// Printer error codes (from cifsprt.doc)
#define NERR_Success                   0
//#define ERROR_ACCESS_DENIED            5
//#define ERROR_MORE_DATA              234
#define NERR_QNotFound              2150
#define NERR_SpoolerNotLoaded       2161


/*
#define SMB_ERR(class, err)    (((class)-1 << ERRCLS_SHIFT) | ((err) & ERR_MASK))
#define SMB_ERR_CLASS(x) (UCHAR)(x & ERR_MASK)
#define SMB_ERR_ERR(x)  (USHORT)((x >> ERRCLS_SHIFT) + 1) */

#define SMB_ERR(class, err) ((class << 16) | err)
#define SMB_ERR_CLASS(x) (USHORT)((x >> 16) & 0x0000FFFF)
#define SMB_ERR_ERR(x)   (USHORT)(x & 0x0000FFFF)

#endif

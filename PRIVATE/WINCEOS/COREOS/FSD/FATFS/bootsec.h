//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/****************************************************************************\
*
*       BOOTSEC (Boot Sector) Data Structures
*
*
\****************************************************************************/


/*
 * NOTE that BPB.H/.INC must be included before including BOOTSEC.H/.INC
 */

#ifndef INCLUDE_BOOTSEC
#define INCLUDE_BOOTSEC 1

#pragma pack(1)


#define BOOT_SIZE               DEFAULT_SECTOR_SIZE

typedef struct  BOOTSEC { /* */
    BYTE        bsJump[3];          /* "EB XX 90" "E9 XX XX" jump to boot code*/
    BYTE        bsOemName[8];       /* OEM name and version                   */
    BPB         bsBPB;              /* Bios Parameter Block for volume        */
    BYTE        bsDriveNumber;      /* (INT 13h) Drive Number                 */
    BYTE        bsReserved1;        /* Reserved                               */
    BYTE        bsBootSignature;    /* == BSEXTSIG if this is extended boot   */
    DWORD       bsVolumeID;         /* Volume serial number                   */
    BYTE        bsVolumeLabel[OEMNAMESIZE]; /* Volume label                   */
    BYTE        bsFileSysType[8];   /* File system format                     */
} BOOTSEC, *PBOOTSEC;

typedef struct  BIGFATBOOTSEC { /* */
    BYTE        bgbsJump[3];        /* "EB XX 90" "E9 XX XX" jump to boot code*/
    BYTE        bgbsOemName[8];     /* OEM name and version                   */
    BIGFATBPB   bgbsBPB;            /* Bios Parameter Block for volume        */
    BYTE        bgbsDriveNumber;    /* (INT 13h) Drive Number                 */
    BYTE        bgbsReserved1;      /* Reserved                               */
    BYTE        bgbsBootSignature;  /* == BSEXTSIG if this is extended boot   */
    DWORD       bgbsVolumeID;       /* Volume serial number                   */
    BYTE        bgbsVolumeLabel[OEMNAMESIZE]; /* Volume label                 */
    BYTE        bgbsFileSysType[8]; /* File system format                     */
} BIGFATBOOTSEC, *PBIGFATBOOTSEC;


/*
 * Boot sectors have a signature at the end of them. Here are the defines
 * for the trailing signature
 */
#define BOOTSECTRAILSIGL        0x0000
#define BOOTSECTRAILSIGH        0xAA55
#define BOOTSECTRAILSIG         0xAA550000
#define OFFSETTRLSIG            (BOOT_SIZE-4)

#define BOOT_SIGNATURE          (BOOT_SIZE-2)
#define BOOT_TRAILSIG           BOOTSECTRAILSIGH


/*
 * Boot sectors that contain a volume name will have a BOOTSECVOLUMESIG at
 * OFFSETVOLUMESIG, followed by the volume name (stored as UNICODE) at OFFSETVOLUMENAME,
 * which will be no longer than MAXVOLUMENAME characters (including the required NULL
 * terminator); in addition, unused characters must be padded with NULLs.
 */
#define MAXVOLUMENAME           32
#define OFFSETVOLUMENAME        (OFFSETTRLSIG - MAXVOLUMENAME*sizeof(WCHAR))
#define BOOTSECVOLUMESIG        0xFF50544A
#define OFFSETVOLUMESIG         (OFFSETVOLUMENAME - 4)

























#define DOREAD_OFFSET           0x014B
#define DOREAD_MOVDL_OFFSET     0x0016
#define DOREAD_LENGTH           0x0035


/*
 * The following applies to BIGFAT partition types and defines a sector
 * increment which the Microsoft partioned media MBR will use to try
 * and find a backup copy of the boot sector to use if the normal boot
 * sector can't be read or is invalid.
 */
#define MBR_BOOTFAILBACKUP      6
#define MBR_BOOTFLBCKUP         MBR_BOOTFAILBACKUP


/*
 * Validation signature for second boot sector of the FAT32 boot record
 * This is a DWORD at offset 0 in the second boot sector (or offset 512
 * relative to the start of the complete two sector boot record). NOTE
 * that the second boot sector of the FAT32 boot record also contains
 * the 0xAA550000 signature in the last 4 bytes of the sector just like
 * the first sector does.
 */
#define SECONDBOOTSECSIG        0x41615252L
#define SECONDBOOTSECSIGL       0x5252
#define SECONDBOOTSECSIGH       0x4161


/*
 * The FAT32 boot record has some additional data located in
 * the second boot sector. This is its structure and offset defines.
 */
typedef struct  BIGFATBOOTFSINFO {
    DWORD       bfFSInf_Sig;
    DWORD       bfFSInf_free_clus_cnt;
    DWORD       bfFSInf_next_free_clus;
    DWORD       bfFSInf_resvd[3];
} BIGFATBOOTFSINFO, *PBIGFATBOOTFSINFO;

// Legacy definitions of the above

#define EXT_BIGIBMBOOT_FSINFO           BIGFATBOOTFSINFO
#define EXT_BGFSInf_Sig                 bfFSInf_Sig
#define EXT_BGFSInf_free_clus_cnt       bfFSInf_free_clus_cnt
#define EXT_BGFSInf_resvd               bfFSInf_next_free_clus

#define FSINFOSIG                       0x61417272L
#define FSINFOSIGL                      0x7272
#define FSINFOSIGH                      0x6141

#define OFFSETFSINFOFRMSECSTRT          (OFFSETTRLSIG - sizeof(BIGFATBOOTFSINFO))
#define OFFSETEXTFSINFOFRMSECSTRT       OFFSETFSINFOFRMSECSTRT


/*
 * Opcode values for first byte of bsJump and bgbsJump
 */
#define BSNOP                   0x90
#define BS2BYTJMP               0xEB
#define BS3BYTJMP               0xE9
// Legacy definitions of the above
#define BOOT_2BYTJMP            BS2BYTJMP
#define BOOT_3BYTJMP            BS3BYTJMP


/*
 * Sig value for bsBootSignature and bgbsBootSignature
 */
#define BSEXTSIG                0x29
#define EXT_BOOT_SIGNATURE      BSEXTSIG


/*
 * Strings for bsFileSysType and bgbsFileSysType
 */
#define BSFSTYPFAT12            "FAT12   "
#define BSFSTYPFAT16            "FAT16   "
#define BSFSTYPFAT32            "FAT32   "


/*  This is the number of sectors (of size 512 bytes) that will cover
 *  the size of MSLOAD program.  BOOT program has to at least read this
 *  many sectors, and these sectors should be the first cluster and consecutive.
 *  Make sure MSLOAD program uses the same value as this.
 */
#ifdef NOTFAT32
#define IBMLOADSIZE     3       // Number of sectors BOOT program should read in
#else
#define IBMLOADSIZE     4       // Number of sectors BOOT program should read in
#endif


#pragma pack()

#endif // INCLUDE_BOOTSEC

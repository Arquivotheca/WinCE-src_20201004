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
*       BPB (Bios Parameter Block) Data Structure
*
*
\****************************************************************************/

#ifndef INCLUDE_DOSBPB
#define INCLUDE_DOSBPB 1

#ifndef HVOLB
#define HVOLB   HANDLE
#endif

//
//  Media descriptor values for different floppy drives
//  NOTE: these are not all unique!
//
#define MEDIA_160   0xFE    // 160KB
#define MEDIA_320   0xFF    // 320KB
#define MEDIA_180   0xFC    // 180KB
#define MEDIA_360   0xFD    // 360KB
#define MEDIA_1200  0xF9    // 1.2MB
#define MEDIA_720   0xF9    // 720KB
#define MEDIA_HD    0xF8    // hard disk
#define MEDIA_1440  0xF0    // 1.44M
#define MEDIA_2880  0xF0    // 2.88M


#pragma pack(1)

typedef struct  BPB { /* */
        WORD    BPB_BytesPerSector;     /* Sector size                      */
        BYTE    BPB_SectorsPerCluster;  /* sectors per allocation unit      */
        WORD    BPB_ReservedSectors;    /* DOS reserved sectors             */
        BYTE    BPB_NumberOfFATs;       /* Number of FATS                   */
        WORD    BPB_RootEntries;        /* Number of directories            */
        WORD    BPB_TotalSectors;       /* Partition size in sectors        */
        BYTE    BPB_MediaDescriptor;    /* Media descriptor                 */
        WORD    BPB_SectorsPerFAT;      /* Fat sectors                      */
        WORD    BPB_SectorsPerTrack;    /* Sectors Per Track                */
        WORD    BPB_Heads;              /* Number heads                     */
        DWORD   BPB_HiddenSectors;      /* Hidden sectors                   */
        DWORD   BPB_BigTotalSectors;    /* Number sectors                   */
} BPB;
typedef BPB UNALIGNED *PBPB;
typedef BPB UNALIGNED FAR *LPBPB;


typedef struct  BIGFATBPB { /* */
        BPB     oldBPB;
        DWORD   BGBPB_BigSectorsPerFAT;   /* BigFAT Fat sectors              */
        WORD    BGBPB_ExtFlags;           /* Other flags                     */
        WORD    BGBPB_FS_Version;         /* File system version             */
        DWORD   BGBPB_RootDirStrtClus;    /* Starting cluster of root directory */
        WORD    BGBPB_FSInfoSec;          /* Sector number in the reserved   */
                                          /* area where the BIGFATBOOTFSINFO */
                                          /* structure is. If this is >=     */
                                          /* oldBPB.BPB_ReservedSectors or   */
                                          /* == 0 there is no FSInfoSec      */
        WORD    BGBPB_BkUpBootSec;        /* Sector number in the reserved   */
                                          /* area where there is a backup    */
                                          /* copy of all of the boot sectors */
                                          /* If this is >=                   */
                                          /* oldBPB.BPB_ReservedSectors or   */
                                          /* == 0 there is no backup copy.   */
        WORD    BGBPB_Reserved[6];        /* Reserved for future expansion   */
} BIGFATBPB;
typedef BIGFATBPB UNALIGNED *PBIGFATBPB;
typedef BIGFATBPB UNALIGNED FAR *LPBIGFATBPB;


/*
 * Bits of BGBPB_ExtFlags
 *    The low 4 bits are the 0 based FAT number of the Active FAT
 */
#define BGBPB_F_ActiveFATMsk    0x000F
#define BGBPB_F_NoFATMirror     0x0080  /* Do not Mirror active FAT to inactive FATs */
#define BGBPB_F_CompressedVol   0x0100  /* Volume is compressed             */


/*
 * Current version of the file system. Basically this allows a version
 *  bind to prevent old versions of the file system drivers from recognizing
 *  new versions of the on disk format. The proper bind is:
 *
 *  if(BIGFATBPB.BGBPB_FS_Version > FAT32_Curr_FS_Version)
 *      do not recognize volume
 */
#define FAT32_Curr_FS_Version   0x0000
#define FAT32_Curr_Version      FAT32_Curr_FS_Version


/* The final two bytes of this BPB variant were originally intended for     */
/* future expansion.  They are never used for anything by the system.       */
/* In most cases where BPBs are manipulated, these bytes are not even       */
/* included.  They are stored in the recommended BPB structure in the BDS,  */
/* and they are passed in through the Get/Set Device Parameters Generic     */
/* IOCTL call.                                                              */

typedef struct A_BPB { /* */
        WORD    A_BPB_BytesPerSector;     /* Sector size                    */
        BYTE    A_BPB_SectorsPerCluster;  /* sectors per allocation unit    */
        WORD    A_BPB_ReservedSectors;    /* DOS reserved sectors           */
        BYTE    A_BPB_NumberOfFATs;       /* Number of FATS                 */
        WORD    A_BPB_RootEntries;        /* Number of directories          */
        WORD    A_BPB_TotalSectors;       /* Partition size in sectors      */
        BYTE    A_BPB_MediaDescriptor;    /* Media descriptor               */
        WORD    A_BPB_SectorsPerFAT;      /* Fat sectors                    */
        WORD    A_BPB_SectorsPerTrack;    /* Sectors Per Track              */
        WORD    A_BPB_Heads;              /* Number heads                   */
        DWORD   A_BPB_HiddenSectors;      /* Hidden sectors                 */
        DWORD   A_BPB_BigTotalSectors;    /* Number sectors                 */
        BYTE    A_BPB_Reserved[6];        /* Unused                         */
} A_BPB;
typedef A_BPB UNALIGNED *PA_BPB;


typedef struct A_BF_BPB { /* */
        WORD    A_BF_BPB_BytesPerSector;     /* Sector size                    */
        BYTE    A_BF_BPB_SectorsPerCluster;  /* sectors per allocation unit    */
        WORD    A_BF_BPB_ReservedSectors;    /* DOS reserved sectors           */
        BYTE    A_BF_BPB_NumberOfFATs;       /* Number of FATS             */
        WORD    A_BF_BPB_RootEntries;        /* Number of directories          */
        WORD    A_BF_BPB_TotalSectors;       /* Partition size in sectors      */
        BYTE    A_BF_BPB_MediaDescriptor;    /* Media descriptor           */
        WORD    A_BF_BPB_SectorsPerFAT;      /* Fat sectors                    */
        WORD    A_BF_BPB_SectorsPerTrack;    /* Sectors Per Track              */
        WORD    A_BF_BPB_Heads;              /* Number heads                   */
        DWORD   A_BF_BPB_HiddenSectors;      /* Hidden sectors                 */
        DWORD   A_BF_BPB_BigTotalSectors;    /* Number sectors                 */
        DWORD   A_BF_BPB_BigSectorsPerFAT;   /* BigFAT Fat sectors             */
        WORD    A_BF_BPB_ExtFlags;           /* Other flags                    */
        WORD    A_BF_BPB_FS_Version;         /* File system version            */
        DWORD   A_BF_BPB_RootDirStrtClus;    /* Starting cluster of root directory */
        WORD    A_BF_BPB_FSInfoSec;          /* Sector number of FSInfo sec    */
        WORD    A_BF_BPB_BkUpBootSec;        /* Sector number of backupboot    */
        WORD    A_BF_BPB_Reserved[6];        /* Reserved for future expansion   */
} A_BF_BPB;
typedef A_BF_BPB UNALIGNED *PA_BF_BPB;


// Redundant definitions (kept to avoid lots of work)

typedef struct EXT_BPB_INFO { /* */
        WORD    EBPB_BYTESPERSECTOR;
        BYTE    EBPB_SECTORSPERCLUSTER;
        WORD    EBPB_RESERVEDSECTORS;
        BYTE    EBPB_NUMBEROFFATS;
        WORD    EBPB_ROOTENTRIES;
        WORD    EBPB_TOTALSECTORS;
        BYTE    EBPB_MEDIADESCRIPTOR;
        WORD    EBPB_SECTORSPERFAT;
        WORD    EBPB_SECTORSPERTRACK;
        WORD    EBPB_HEADS;
        DWORD   EBPB_HIDDENSECTOR;
        DWORD   EBPB_BIGTOTALSECTORS;
} EXT_BPB_INFO;

typedef struct EXT_BIGBPB_INFO { /* */
        WORD    EBGBPB_BYTESPERSECTOR;
        BYTE    EBGBPB_SECTORSPERCLUSTER;
        WORD    EBGBPB_RESERVEDSECTORS;
        BYTE    EBGBPB_NUMBEROFFATS;
        WORD    EBGBPB_ROOTENTRIES;
        WORD    EBGBPB_TOTALSECTORS;
        BYTE    EBGBPB_MEDIADESCRIPTOR;
        WORD    EBGBPB_SECTORSPERFAT;
        WORD    EBGBPB_SECTORSPERTRACK;
        WORD    EBGBPB_HEADS;
        DWORD   EBGBPB_HIDDENSECTOR;
        DWORD   EBGBPB_BIGTOTALSECTORS;
        DWORD   EBGBPB_BIGSECTORSPERFAT;
        WORD    EBGBPB_EXTFLAGS;
        WORD    EBGBPB_FS_VERSION;
        DWORD   EBGBPB_ROOTDIRSTRTCLUS;
        WORD    EBGBPB_FSINFOSEC;
        WORD    EBGBPB_BKUPBOOTSEC;
        DWORD   EBGBPB_RESERVED[3];
} EXT_BIGBPB_INFO;

/* ASM
.errnz  EXT_BPB_INFO.EBPB_BYTESPERSECTOR            NE  BPB.BPB_BytesPerSector
.errnz  EXT_BPB_INFO.EBPB_SECTORSPERCLUSTER         NE  BPB.BPB_SectorsPerCluster
.errnz  EXT_BPB_INFO.EBPB_RESERVEDSECTORS           NE  BPB.BPB_ReservedSectors
.errnz  EXT_BPB_INFO.EBPB_NUMBEROFFATS              NE  BPB.BPB_NumberOfFATs
.errnz  EXT_BPB_INFO.EBPB_ROOTENTRIES               NE  BPB.BPB_RootEntries
.errnz  EXT_BPB_INFO.EBPB_TOTALSECTORS              NE  BPB.BPB_TotalSectors
.errnz  EXT_BPB_INFO.EBPB_MEDIADESCRIPTOR           NE  BPB.BPB_MediaDescriptor
.errnz  EXT_BPB_INFO.EBPB_SECTORSPERFAT             NE  BPB.BPB_SectorsPerFAT
.errnz  EXT_BPB_INFO.EBPB_SECTORSPERTRACK           NE  BPB.BPB_SectorsPerTrack
.errnz  EXT_BPB_INFO.EBPB_HEADS                     NE  BPB.BPB_Heads
.errnz  EXT_BPB_INFO.EBPB_HIDDENSECTOR              NE  BPB.BPB_HiddenSectors
.errnz  EXT_BPB_INFO.EBPB_BIGTOTALSECTORS           NE  BPB.BPB_BigTotalSectors
.errnz  (SIZE EXT_BPB_INFO) NE  (SIZE BPB)

.errnz  EXT_BIGBPB_INFO.EBGBPB_BYTESPERSECTOR       NE  EXT_BPB_INFO.EBPB_BYTESPERSECTOR
.errnz  EXT_BIGBPB_INFO.EBGBPB_SECTORSPERCLUSTER    NE  EXT_BPB_INFO.EBPB_SECTORSPERCLUSTER
.errnz  EXT_BIGBPB_INFO.EBGBPB_RESERVEDSECTORS      NE  EXT_BPB_INFO.EBPB_RESERVEDSECTORS
.errnz  EXT_BIGBPB_INFO.EBGBPB_NUMBEROFFATS         NE  EXT_BPB_INFO.EBPB_NUMBEROFFATS
.errnz  EXT_BIGBPB_INFO.EBGBPB_ROOTENTRIES          NE  EXT_BPB_INFO.EBPB_ROOTENTRIES
.errnz  EXT_BIGBPB_INFO.EBGBPB_TOTALSECTORS         NE  EXT_BPB_INFO.EBPB_TOTALSECTORS
.errnz  EXT_BIGBPB_INFO.EBGBPB_MEDIADESCRIPTOR      NE  EXT_BPB_INFO.EBPB_MEDIADESCRIPTOR
.errnz  EXT_BIGBPB_INFO.EBGBPB_SECTORSPERFAT        NE  EXT_BPB_INFO.EBPB_SECTORSPERFAT
.errnz  EXT_BIGBPB_INFO.EBGBPB_SECTORSPERTRACK      NE  EXT_BPB_INFO.EBPB_SECTORSPERTRACK
.errnz  EXT_BIGBPB_INFO.EBGBPB_HEADS                NE  EXT_BPB_INFO.EBPB_HEADS
.errnz  EXT_BIGBPB_INFO.EBGBPB_HIDDENSECTOR         NE  EXT_BPB_INFO.EBPB_HIDDENSECTOR
.errnz  EXT_BIGBPB_INFO.EBGBPB_BIGTOTALSECTORS      NE  EXT_BPB_INFO.EBPB_BIGTOTALSECTORS
;.errnz  (SIZE EXT_BIGBPB_INFO.EBGBPB_BIGTOTALSECTORS) NE (SIZE EXT_BPB_INFO.EBPB_BIGTOTALSECTORS)
 */

typedef struct EXT_IBMBOOT_HEADER { /* */
        BYTE    EXT_BOOT_JUMP[3];
        BYTE    EXT_BOOT_OEM[8];
        EXT_BPB_INFO EXT_BOOT_BPB;
        BYTE    EXT_PHYDRV;
        BYTE    EXT_CURHD;
        BYTE    EXT_BOOT_SIG;
        DWORD   EXT_BOOT_SERIAL;
        BYTE    EXT_BOOT_VOL_LABEL[11];
        BYTE    EXT_SYSTEM_ID[8];
} EXT_IBMBOOT_HEADER;

typedef struct EXT_BIGIBMBOOT_HEADER { /* */
        BYTE    EXT_BGBOOT_JUMP[3];
        BYTE    EXT_BGBOOT_OEM[8];
        EXT_BIGBPB_INFO EXT_BGBOOT_BPB;
        BYTE    EXT_BGPHYDRV;
        BYTE    EXT_BGCURHD;
        BYTE    EXT_BGBOOT_SIG;
        DWORD   EXT_BGBOOT_SERIAL;
        BYTE    EXT_BGBOOT_VOL_LABEL[11];
        BYTE    EXT_BGSYSTEM_ID[8];
} EXT_BIGIBMBOOT_HEADER;

/* ASM
.errnz  EXT_BIGIBMBOOT_HEADER.EXT_BGBOOT_JUMP   NE  EXT_IBMBOOT_HEADER.EXT_BOOT_JUMP
.errnz  EXT_BIGIBMBOOT_HEADER.EXT_BGBOOT_OEM    NE  EXT_IBMBOOT_HEADER.EXT_BOOT_OEM
.errnz  EXT_BIGIBMBOOT_HEADER.EXT_BGBOOT_BPB    NE  EXT_IBMBOOT_HEADER.EXT_BOOT_BPB
 */

#pragma pack()










typedef struct DPB {
    HVOLB   dpb_drive;                  /* Logical drive identifier */
    BYTE    dpb_unit;                   /* Driver unit number of DPB */
    UINT    dpb_sector_size;            /* Size of physical sector in bytes */
    BYTE    dpb_cluster_mask;           /* Sectors/cluster - 1 */
    BYTE    dpb_cluster_shift;          /* Log2 of sectors/cluster */
    UINT    dpb_first_fat;              /* Starting record of FATs */
    BYTE    dpb_fat_count;              /* Number of FATs for this drive */
    UINT    dpb_root_entries;           /* Number of directory entries */
    UINT    dpb_first_sector;           /* First sector of first cluster */
    UINT    dpb_max_cluster;            /* Number of clusters on drive + 1 */
    /*
     * If the following field is 0, this is a BigFAT drive and the
     * extdpb fields MUST be used. For non BigFAT drives (this field is non-0),
     * the extdpb_ fields MUST NOT be used internally by MS-DOS (there are non
     * MS-DOS folks like compression drivers that create DPBs at the old size).
     * For a non-BigFAT DPB gotten through the INT 21h API (AX=7302h) the
     * extdpb_ fields are extended versions of the matching dpb_ fields and
     * either the dpb_ or extdpb_ field can be used.
     */
    UINT    dpb_fat_size;               /* Number of records occupied by FAT */
    UINT    dpb_dir_sector;             /* Starting record of directory */
    DWORD   dpb_driver_addr;            /* Pointer to driver */
    BYTE    dpb_media;                  /* Media byte */
#ifdef NOTFAT32
    BYTE    dpb_first_access;           /* This is initialized to -1 to force a
                     * media check the first time this DPB
                     * is used.
                     */
#else
    BYTE   dpb_flags;                   /* This is initialized to 80h to force a
                     * media check the first time this DPB
                     * is used.
                     */
#endif
    DWORD   dpb_next_dpb;               /* Pointer to next DPB */
    UINT    dpb_next_free;              /* # of last allocated cluster */
    UINT    dpb_free_cnt;               /* Count of free clusters, -1 if unknown */
#ifndef NOTFAT32
    UINT    extdpb_free_cnt_hi;         /* high word of free_cnt if BigFAT */
    UINT    extdpb_flags;               /* FAT32 flags from BPB */
    UINT    extdpb_FSInfoSec;           /* Sec # of file system info sector (-1 if none) */
    UINT    extdpb_BkUpBootSec;         /* Sec # of backup boot sector (-1 if none) */
    DWORD   extdpb_first_sector;        /* First sector of first cluster */
    DWORD   extdpb_max_cluster;         /* Number of clusters on drive + 1 */
    DWORD   extdpb_fat_size;            /* Number of records occupied by FAT */
    DWORD   extdpb_root_clus;           /* cluster # of first cluster of root dir */
    DWORD   extdpb_next_free;           /* # of last allocated cluster */
#endif
} DPB;
typedef DPB *PDPB;
typedef DPB FAR *LPDPB;

#define DPBSIZ      (sizeof(DPB))   /* Size of the structure in bytes */


#define DSKSIZ      dpb_max_cluster /* Size of disk (used during init only) */

#define DPB_NEXTFREEOFF 29              // offset of next_free


// dpb_flags bits
// PLEASE NOTE CAREFULLY!!!! This used to be a value field in the old pre-FAT32
//               DPB. Value 0xFF was equivalent to DPB_F_FIRSTACCESS.
//               For this reason it must be OK if all the bits in
//               here get SET by some random app. This may cause
//               unwanted side effects, but it must not cause
//               DAMAGE.
//
//               There is clean up for this special case and
//               for this reason AT LEAST ONE OF THE 8 BITS
//               IN THIS BYTE MUST NEVER BE DEFINED TO MEAN
//               ANYTHING!!!!!! So that the special case "some
//               piece of software other than MS-DOS shoved
//               0FFh in here" can be detected.
//
#define DPB_F_FREECNTCHANGED     0x01   /* Free cnt has changed  (FAT32 ONLY) */
#define DPB_F_FREECNTCHANGEDBIT  0
#define DPB_F_ROOTCLUSCHANGED    0x02   /* Root Clus has changed  (FAT32 ONLY) */
#define DPB_F_ROOTCLUSCHANGEDBIT 1

#define DPB_F_EXTFLGSCHANGED     0x08   /* Mirror setting changed (FAT32 ONLY) */
#define DPB_F_EXTFLGSCHANGEDBIT  3

#define DPB_F_FIRSTACCESS    0x80   /* This MUST be 80h (the sign bit, see FAT.ASM) */
#define DPB_F_FIRSTACCESSBIT     7

// extdpb_flags bits
//  NOTE: It is a design point of this field that the proper "default"
//        initialization is 0.
//
#define DPB_EF_ActiveFATMsk 0x000F  /* 0 based active FAT # */
#define DPB_EF_NoFATMirror  0x0080  /* Do not Mirror active FAT to inactive FATs */
#define DPB_EF_NoFATMirrorBit   7
#define DPB_EF_CompressedVol    0x0100  /* Volume is compressed */
#define DPB_EF_CompressedVolBit 8

// End of definitions and types migrated from DPB.H










/*--------------------------------------------------------------------------*/
/*  Format IOCTL Device Parameter Block Structure -                  */
/*--------------------------------------------------------------------------*/

#define MAX_SEC_PER_TRACK   64

typedef struct tagODEVPB {
    BYTE    SplFunctions;
    BYTE    devType;
    UINT    devAtt;
    UINT    NumCyls;
    BYTE    bMediaType;         /* 0=>1.2MB and 1=>360KB */
    BPB     BPB;
    BYTE    A_BPB_Reserved[6];           // Unused 6 BPB bytes
    BYTE    TrackLayout[MAX_SEC_PER_TRACK * 4 + 2];
} ODEVPB;

#define OLDTRACKLAYOUT_OFFSET   (7+sizeof(BPB)+6)  // Offset of tracklayout

typedef struct tagEDEVPB {
    BYTE    SplFunctions;
    BYTE    devType;
    UINT    devAtt;
    UINT    NumCyls;
    BYTE    bMediaType;         /* 0=>1.2MB and 1=>360KB */
    BIGFATBPB BPB;
    BYTE    reserved1[32];
    BYTE    TrackLayout[MAX_SEC_PER_TRACK * 4 + 2];
} EDEVPB;

#define EXTTRACKLAYOUT_OFFSET   (7+sizeof(BIGFATBPB)+32)// Offset of tracklayout
                                                        // in a Device Parameter Block */
//
// NOTE that it is pretty irrelevant whether you use NonExtDevPB or ExtDevPB
//  until you get up to the new FAT32 fields in the BPB and/or the
//  TrackLayout area as the structures are identical andinterchangeable
//  up to that point.
//
typedef struct tagDEVPB {
    union   {
    ODEVPB  OldDevPB;
    EDEVPB  ExtDevPB;
    } DevPrm;
    BYTE    CallForm;
} DEVPB;
typedef DEVPB *PDEVPB;
typedef DEVPB FAR *LPDEVPB;

//
// Defines for devType field
//
#define DEVPB_DEVTYP_525_0360   0
#define DEVPB_DEVTYP_525_1200   1
#define DEVPB_DEVTYP_350_0720   2
#define DEVPB_DEVTYP_800_SD 3
#define DEVPB_DEVTYP_800_DD 4
#define DEVPB_DEVTYP_FIXED  5
#define DEVPB_DEVTYP_TAPE   6
#define DEVPB_DEVTYP_350_1440   7
#define DEVPB_DEVTYP_350_2880   9

//
// Bit masks for devAtt
//
#define DEVPB_DEVATT_NONREM 0x0001  // set if non removable
#define DEVPB_DEVATT_CHNGLN 0x0002  // set if changeline supported on drive
#define DEVPB_DEVATT_FAKE_BPB   0x0004  // when set, don't do a build bpb
#define DEVPB_DEVATT_GD_TRKL    0x0008  // set if track layout has no funny sectors
#define DEVPB_DEVATT_MLT_LOG    0x0010  // set if more than one drive letter mapped
#define DEVPB_DEVATT_OWN_PHY    0x0020  // set if this drive currently is mapped to the phys unit
#define DEVPB_DEVATT_CHNGD  0x0040  // set if media changed
#define DEVPB_DEVATT_SET_DASD   0x0080  // do a set DASD before next format
#define DEVPB_DEVATT_FMT_CHNG   0x0100  // media changed by format
#define DEVPB_DEVATT_UNF    0x0200  // unformatted media (non-rem only)
#define DEVPB_DEVATT_X13    0x0400  // extended INT 13 used for drive
#define DEVPB_DEVATT_DMF    0x0800  // Last access was a DMF floppy

//
// DEFINES for the AX=4302h GetVolumeInformation BX FileSysFlags return
//
#undef  FS_CASE_IS_PRESERVED            // undefine in case WINBASE.H was included
#define FS_CASE_IS_PRESERVED            0x0002

#undef  FS_VOL_IS_COMPRESSED            // undefine in case WINBASE.H was included
#define FS_VOL_IS_COMPRESSED            0x8000

#undef  FS_VOL_SUPPORTS_LONG_NAMES      // undefine in case WINBASE.H was included
#define FS_VOL_SUPPORTS_LONG_NAMES  0x4000

// Transaction status
#ifdef TFAT
    #define TS_PENDING      0xfffe   // There are new transaction since last commit
    #define TS_INPROGRESS   0xfffd   // Copying FAT1 to FAT0, not done yet
    #define TS_COMPLETED    0xfffc   // Copying is completed
#endif
// End of definitions and types migrated from DMAINT.H


#endif // INCLUDE_DOSBPB

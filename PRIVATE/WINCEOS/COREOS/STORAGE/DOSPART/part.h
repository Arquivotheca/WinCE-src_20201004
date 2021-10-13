//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 *  This defines the partition structures found on the first sector of every hard drive
 */


#ifndef INCLUDE_PARTITON
#define INCLUDE_PARTITON 1


//#define HIDDEN_PARTITIONS 1


//typedef struct PartState PartState;
//typedef struct DriverState DriverState;
//typedef struct SearchState SearchState;
//typedef struct OnDiskPartState OnDiskPartState;

typedef struct tagDriverState
{
    FILETIME    ftCreate;
    FILETIME    ftAccess;
    DWORD       dwChecksum;
    DWORD       dwNumParts;
    HANDLE      hStore;         // add extra fields after this one - fields prior to this offset are written to disk
    BOOL        bFormatState;
    BOOL        bUseHiddenPartitions;
    DISK_INFO   diskInfo;
    struct tagPartState   *pPartState;
    struct tagSearchState *pSearchState;
    struct tagPartState   *pHiddenPartState;  // pPartState the encompasses the hidden partition
    SECTORNUM   snExtPartSector;    // absolute LBA (0 based) sector number - start of the extended partition
    SECTORNUM   snExtPartEndSec;    // absolute LBA (0 based) sector number - end of the extended partition
} DriverState, *PDriverState;

typedef struct tagSearchState
{

    DriverState  *pState;
    struct tagPartState    *pPartState;
    struct tagSearchState  *pNextSearchState;

} SearchState, *PSearchState;


// These two structures must agree 
typedef struct tagPartState
{
    TCHAR        cPartName[PARTITIONNAMESIZE];
    SECTORNUM    snStartSector;                 // absolute LBA (0 based) sector number - start of data area
    SECTORNUM    snNumSectors;                  // length of the data area - does not include PBR track 
    FILETIME     ftCreation;
    FILETIME     ftModified;
    DWORD        dwAttributes;
    SECTORNUM    snPBRSector;                   // absolute LBA (0 based) sector number of the PBR
    struct tagPartState    *pNextPartState;
    struct tagDriverState  *pState;
    BYTE         partType;
} PartState, *PPartState;


#pragma pack(1)

typedef struct tagOnDiskPartState
{
    TCHAR        cPartName[PARTITIONNAMESIZE];
    SECTORNUM    snStartSector;
    SECTORNUM    snNumSectors;
    FILETIME     ftCreation;
    FILETIME     ftModified;
    DWORD        dwAttributes;
    BYTE         partType;
} OnDiskPartState, *POnDiskPartState;



// helper.c functions
BOOL ConvertLBAtoCHS (DriverState * state, SECTORNUM snSectorNum, PBYTE pCyl, PBYTE pHead, PBYTE pSector);
BOOL WriteHiddenPartitionTable(DriverState * state);
BOOL InitPartition(DriverState * state, PartState * pPartState);
void UpdateFileTime(DriverState * state, PartState * pPartState, BOOL bUpdateStateCreate, BOOL bUpdatePartCreate);
void GetPartition(DriverState * state, PartState ** ppPartState, PartState ** ppPrevPartState, LPCTSTR partName);
DWORD GetDOSPartitions(DriverState * state, PartState ** pPrevPartState, PBYTE pSector, BOOL bExtPart, SECTORNUM *pStartSec);
BOOL GetHiddenPartitions(DriverState * pState);
BOOL AddHiddenPartition(DriverState * state);
BOOL ReadSectors(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, PBYTE *pBuffer);
BOOL WriteSectors(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, PBYTE pBuffer);
BOOL GetPartLocation (DriverState * state, SECTORNUM numSectors, PartState **ppPartState, SECTORNUM *pStartSector, SECTORNUM *pPBRSector, SECTORNUM *pSectorCount);
BOOL WriteMBR(DriverState *state, SECTORNUM snSectorNum, SECTORNUM snNumSectors, BYTE fileSysType, BOOL bCreateMBR);
BOOL CreatePBR(DriverState *state, SECTORNUM snLastPBRSec, SECTORNUM snNextPBRSec, SECTORNUM snThisSectorNum, SECTORNUM snNumSectors, BYTE fileSysType);
BOOL DeleteMBR(DriverState *state, PartState *pPartState);
BOOL DeletePBR(DriverState *state, PartState *pPartState, PartState *pPrevPartState);
void GeneratePartitionNames(DriverState *state);
BOOL GeneratePartitionChecksum(DriverState *state, PBYTE buffer, DWORD *pChecksum);
void GenerateFATFSType(DriverState *state, SECTORNUM snNumSectors, PBYTE pFileSys);
BOOL CalculatePartitionSpace(DriverState *state, SECTORNUM *pFreeSpace, SECTORNUM *pLargestPart);
void ClearPartitionList(DriverState *state);
void WritePartitionName(PartState *pPartState);


#define HIDDEN_PART_START       1
#define HIDDEN_PART_SEC_COUNT   10
#define HIDDEN_PART_TYPE        0x18                    // our partition type for the hidden data

#define BUFFER_SIZE             (128*1024)
#define STORESTART              4
#define STORESIG                0xAA5555AA
#define PARTITION_SECTORS       10


#define DEFAULT_SECTOR_SIZE     512
//#define BOOT_SIGNATURE          DEFAULT_SECTOR_SIZE -2
//#define BOOTSECTRAILSIG         0xAA55

// defines for generation of FATFS type

#define FAT1216_THRESHOLD       0x00000FF6
#define FAT1632_THRESHOLD       0x0000FFF6
#define FAT32_BAD               0x0FFFFFF7
#define MAX_CLUS_SIZE           32768
#define DATA_CLUSTER            2

#define OEMNAMESIZE             11

typedef struct _DIRENTRY {
    BYTE    de_name[OEMNAMESIZE];   // file name
    BYTE    de_attr;                // file attribute bits (system, hidden, etc.)
    BYTE    de_NTflags;             // ???
    BYTE    de_createdTimeMsec;     // ??? (milliseconds needs 11 bits for 0-2000)
    WORD    de_createdTime;         // time of file creation
    WORD    de_createdDate;         // date of file creation
    WORD    de_lastAccessDate;      // date of last file access
    WORD    de_clusFirstHigh;       // high word of first cluster
    WORD    de_time;                // time of last file change
    WORD    de_date;                // date of last file change
    WORD    de_clusFirst;           // low word of first cluster
    DWORD   de_size;                // file size in bytes
} DIRENTRY, *PDIRENTRY;


// PARTITION defines

// Part_BootInd

#define PART_BOOTABLE           0x80    // bootable partition
#define PART_ACTIVE             PART_BOOTABLE
#define BOOT_IND_ACTIVE         PART_BOOTABLE
#define PART_NON_BOOTABLE       0       // non-bootable partition
#define BOOT_IND_INACTIVE       PART_NON_BOOTABLE

#define PART_READONLY   0x02

// Part_FileSystem

#define PART_UNKNOWN            0
#define PART_DOS2_FAT           0x01    // legit DOS partition
#define PART_DOS3_FAT           0x04    // legit DOS partition
#define PART_EXTENDED           0x05    // legit DOS partition
#define PART_DOS4_FAT           0x06    // legit DOS partition
#define PART_DOS32              0x0B    // legit DOS partition (FAT32)
#define PART_DOS32X13           0x0C    // Same as 0x0B only "use LBA"
#define PART_DOSX13             0x0E    // Same as 0x06 only "use LBA"
#define PART_DOSX13X            0x0F    // Same as 0x05 only "use LBA"

#define LEGACY

#ifdef LEGACY

// Legacy definitions for Part_FileSystem

#define PRIMARY                 0x00
#define NEC_98_INVALID          0x00
#define DOS12                   PART_DOS2_FAT   /* MS-DOS 12-bit FAT */
#define XENIX1                  0x02
#define XENIX2                  0x03
#define DOS16                   PART_DOS3_FAT   /* MS-DOS original 16-bit FAT */
#define EXTENDED                PART_EXTENDED   /* extended partition */
#define LOGICAL                 PART_EXTENDED
#define DOSNEW                  PART_DOS4_FAT   /* MS-DOS 16-bit FAT > 32Meg partition */
#define HPFS                    0x07            /* Also used by NTFS OFS and other IFS file systems */
#define PARTID_IFS              HPFS
#define COMMODOREid             0x08
#define BM_PART                 0x0A            /* OS/2 Boot Manager (BM) partition */
#define DOS32                   PART_DOS32      /* MS-DOS (32bit FAT) */
#define DOS32X13                PART_DOS32X13   /* MS-DOS (32bit FAT EXTENDED) */
#define DOSX32                  DOS32X13
#define DOSX13                  PART_DOSX13     /* Ext INT 13 drive comment out this line to turn off X13 sup. */
#define DOS1024                 DOSX13
#define DOSX13X                 PART_DOSX13X    /* EXTENDED partition XI13 */
#define DOSX1024                DOSX13X

#define NEC_98_DOS16            0x11            /* NEC MS-DOS (16bit FAT) */

#ifdef RESERVED_PARTITIONS
#define BM_FAT12_PART           0x11            /* OS/2 Boot Manager (BM) partition */
#define L_EDGEid                0x11
#define BM_FAT16_PART           0x14            /* OS/2 Boot Manager (BM) partition */
#define ASTid                   0x14
#define BM_NEW_PART             0x16            /* OS/2 Boot Manager (BM) partition */
#define BM_HPFS_PART            0x17            /* OS/2 Boot Manager (BM) partition */
#endif

#define NEC_98_DOSNEW           0x21            /* NEC MS-DOS (large partition) */

#ifdef RESERVED_PARTITIONS
#define OTHR3DPARTYI            0x23
#define NECid                   0x24
#define OTHR3DPARTYK            0x26
#define OTHR3DPARTYL            0x31
#define OTHR3DPARTYM            0x33
#define OTHR3DPARTYN            0x34
#define OTHR3DPARTYO            0x36
#define IBM_PPC_PREP            0x41        /* IBM PowerPC prep partition */
#define VERITASVXVM1            0x42        /* Veritas Volume Manager 1 partition */
#define VERITASVXVM2            0x43        /* Veritas Volume Manager 2 partition */
#define DM_DRIVERid1            0x50
#define DM_DRIVERid2            0x51
#define ATNTid                  0x56
#define OTHR3DPARTY1            0x61
#define OTHR3DPARTY2            0x63
#define NOVELL                  0x64
#define OTHR3DPARTY4            0x66
#define OTHR3DPARTY5            0x71
#define OTHR3DPARTY6            0x73
#define OTHR3DPARTY7            0x74
#define PCIX                    0x75
#define OTHR3DPARTY8            0x76
#define PLAN9                   0x9B            /* Bell Labs (Dennis Richie!!!) */
#define OTHR3DPARTYP            0xA1
#define OTHR3DPARTYQ            0xA3
#define OTHR3DPARTYR            0xA4
#define OTHR3DPARTYS            0xA6
#define OTHR3DPARTYT            0xB1
#define OTHR3DPARTYU            0xB3
#define OTHR3DPARTYV            0xB4
#define OTHR3DPARTYW            0xB6
#define PP_FAT12_PART           0xC1            /* DR-DOS Password Protected */
#define PP_FAT16_PART           0xC4            /* DR-DOS Password Protected */
#define PP_EXT_PART             0xC5            /* DR-DOS Password Protected */
#define PP_NEW_PART             0xC6            /* DR-DOS Password Protected */
#define CPM                     0xDB
#define OTHR3DPARTY9            0xE1
#define OTHR3DPARTYA            0xE3
#define OTHR3DPARTYB            0xE4
#define TANDYid                 0xE5
#define OTHR3DPARTYC            0xE6
#define OTHR3DPARTYD            0xF1
#define UNISYSid                0xF2
#define OTHR3DPARTYE            0xF3
#define OTHR3DPARTYF            0xF4
#define OTHR3DPARTYG            0xF6
#define INVALID                 0xFF            /* invalid or not used */
#endif  // RESERVED_PARTITIONS

#endif  // LEGACY

/*
 *  The following symbol defines the offset of 6 bytes in the MBR used by IOS.
 *  The DWORD at MBR_IOS_OFFSET+2 is used IFF the WORD at MBR_IOS_OFFSET is 0.
 */
#define MBR_IOS_OFFSET          0x00DA
#define MBR_IOS_RESVDSZBYTES    6

#define FATFS                   L"fatfs.dll"

// PARTENTRY struc

// The First & Last numbers below are INCLUDED in the part of the disk
// in this partition.  Head & Track are 0-based, Sector is 1-based.

typedef struct _PARTENTRY {
        BYTE            Part_BootInd;           // If 80h means this is boot partition
        BYTE            Part_FirstHead;         // Partition starting head based 0
        BYTE            Part_FirstSector;       // Partition starting sector based 1
        BYTE            Part_FirstTrack;        // Partition starting track based 0
        BYTE            Part_FileSystem;        // Partition type signature field
        BYTE            Part_LastHead;          // Partition ending head based 0
        BYTE            Part_LastSector;        // Partition ending sector based 1
        BYTE            Part_LastTrack;         // Partition ending track based 0
        DWORD           Part_StartSector;       // Physical starting sector based 0
        DWORD           Part_TotalSectors;      // Total physical sectors in partition
} PARTENTRY;
typedef PARTENTRY UNALIGNED *PPARTENTRY;


/*
 *  Following define is the offset of the PARTTABLE structure in the Master Boot Record (MBR)
 */
#define VOLBOOT_ORG             0x7C00          // where MBR is loaded (0:7C00h) on Intel PC systems

#define MAX_PARTTABLE_ENTRIES   4
#define SIZE_PARTTABLE_ENTRIES  16

#define PARTTABLE_OFFSET        (DEFAULT_SECTOR_SIZE - 2 - (SIZE_PARTTABLE_ENTRIES * MAX_PARTTABLE_ENTRIES))

typedef struct _PARTTABLE {
        PARTENTRY       PartEntry[MAX_PARTTABLE_ENTRIES];
} PARTTABLE;
typedef PARTTABLE UNALIGNED *PPARTTABLE;


#pragma pack()

#endif // INCLUDE_PARTITON

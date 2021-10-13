//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++ BUILD Version: 0001     Increment this if a change has global effects


Module Name:

    fatfmt.h

Abstract:

    This file defines the layout of the on disk data structures for the
    FAT file system.

Revision History:

--*/

#pragma pack(1)


#define FAT12_EOF_SPECIAL       0x00000FF0
#define FAT1216_THRESHOLD       0x00000FF6
#define FAT12_BAD               0x00000FF7
#define FAT12_EOF_MIN           0x00000FF8
#define FAT12_EOF_MAX           0x00000FFF

//  If the number of clusters on a volume is < FAT1216_THRESHOLD,
//  then a 12-bit FAT is being used;  this means there are at most FF5h
//  clusters, and that cluster indexes range from 002h to FF6h (at most).

#define FAT1632_THRESHOLD       0x0000FFF6
#define FAT16_BAD               0x0000FFF7
#define FAT16_EOF_MIN           0x0000FFF8
#define FAT16_EOF_MAX           0x0000FFFF

#define FAT32_BAD               0x0FFFFFF7
#define FAT32_EOF_MIN           0x0FFFFFF8
#define FAT32_EOF_MAX           0x0FFFFFFF

#define MAX_CLUS_SIZE           32768   // NT supports larger FAT cluster sizes (64K at least), but
                                        // such volumes are not DOS-compatible, so we don't support them

#define OEMNAMESIZE             11

#define DEFAULT_SECTOR_SIZE     512

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


#define ATTR_NONE       0x00
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_DEVICE     0x40    // This is a VERY special bit.
                                //   NO directory entry on a disk EVER
                                //   has this bit set. It is set non-zero
                                //   when a device is found by GETPATH

#define ATTR_ALL                (ATTR_HIDDEN+ATTR_SYSTEM+ATTR_DIRECTORY)
                                // OR of hard attributes for FINDENTRY

#define ATTR_VALID              (ATTR_READ_ONLY+ATTR_HIDDEN+ATTR_SYSTEM+ATTR_VOLUME_ID+ATTR_DIRECTORY+ATTR_ARCHIVE)
                                // OR of all valid bits that a dirent can possess


#define ATTR_IGNORE             (ATTR_READ_ONLY+ATTR_ARCHIVE+ATTR_DEVICE)
                                // ignore these attributes during search first/next

#define ATTR_CHANGEABLE         (ATTR_READ_ONLY+ATTR_HIDDEN+ATTR_SYSTEM+ATTR_ARCHIVE)
                                // changeable via CHMOD

#define ATTR_LONG_MASK          (ATTR_READ_ONLY+ATTR_HIDDEN+ATTR_SYSTEM+ATTR_VOLUME_ID+ATTR_DIRECTORY)

#define ATTR_LONG               (ATTR_READ_ONLY+ATTR_HIDDEN+ATTR_SYSTEM+ATTR_VOLUME_ID)
                                // overloading of existing attributes for long names

#define ATTR_DIR_MASK           (ATTR_VOLUME_ID+ATTR_DIRECTORY)


#define ISLFNDIRENTRY(pde)      (((pde)->de_attr & ATTR_LONG_MASK) == ATTR_LONG)

#define DIRFREE         0xE5    // stored in de_name[0] to indicate free slot
#define DIREND          0       // stored in de_name[0] to indicate no more entries
#define MAX_DIRENTRIES  65536   // max directory entries MS-DOS can handle

//  Long name directory/file entry structure
//
//  A long file/dir name entry overlays the existing DOS file/dir entry
//  structure and must be the same size.  A sequence of long name entries are
//  ALWAYS associated with an existing DOS file/dir entry which succeeds them.
//  If there is no DOS file/dir entry, then the long dir/file entries are to be
//  considered 'orphans' (i.e. they do not exist).  Note that the long names
//  are in 'reverse' order.
//
//                       +-------------------------+
//                       |   nth long name entry   |
//                       +-------------------------+
//                       |   ...                   |
//                       +-------------------------+
//                       |   2nd long name entry   |
//                       +-------------------------+
//                       |   1st long name entry   |
//                       +-------------------------+
//                       |   OEM 8.3 name entry    |
//                       +-------------------------+

#define MAX_LFN_LEN     255     // maximum file name length (excluding terminating NULL)
#define LDE_LEN1        5       // # of Unicode chars in lde_name1 field
#define LDE_LEN2        6       // # of Unicode chars in lde_name2 field
#define LDE_LEN3        2       // # of Unicode chars in lde_name3 field
#define LDE_NAME_LEN    (LDE_LEN1 + LDE_LEN2 + LDE_LEN3)

typedef struct _LFNDIRENTRY {
    BYTE    lde_ord;            // The order of this entry in the sequence of
                                //   long name entries associated with the OEM 8.3
                                //   dir entry at the end of the sequence.
    WCHAR   lde_name1[LDE_LEN1];// Chars 1-5 of long name string in this entry.
    BYTE    lde_attr;           // File/dir attributes - MUST be SYS-HID-VOL.
    BYTE    lde_flags;          // To-be-determined.
    BYTE    lde_chksum;         // Checksum of OEM 8.3 name.
    WCHAR   lde_name2[LDE_LEN2];// Chars 6-11 of long name string in this entry.
    WORD    lde_clusFirst;      // "First cluster" number - an artifact of the DOS
                                //   dir entry.  This must be zero for compatibility
                                //   with disk utils.  It's meaningless in the
                                //   context of a long name entry.
    WCHAR   lde_name3[LDE_LEN3];// Chars 12-13 of long name string in this entry.
} LFNDIRENTRY;
typedef LFNDIRENTRY *PLFNDIRENTRY;

#ifndef ERRFALSE
#define ERRFALSE(foo)
#endif

ERRFALSE(sizeof(LFNDIRENTRY) == sizeof(DIRENTRY));


//  Current value for lde_flags field in long-name dir entry

#define LN_FLAGS        0       // default value for lde_flags

//   Long-name component ordinal field min/max value
//   and flags used in the lde_ord field.

#define LN_MIN_ORD_NUM  1
#define LN_MAX_ORD_NUM  ((MAX_LFN_LEN+LDE_NAME_LEN-1)/LDE_NAME_LEN)   // must include trailing NUL

#define LN_ORD_LAST_MASK    0x40        // Indicates the last long-name
                                        // component in a set of long-name
                                        // components.  Used to determine
                                        // if a set of long-name components
                                        // has been truncated by a disk util.

ERRFALSE(LN_MAX_ORD_NUM < LN_ORD_LAST_MASK);


#define LN_MAX_VAL_UNICD    0xFFFC      // FFFD FFFE and FFFF are invalid unicode chars

#pragma pack()

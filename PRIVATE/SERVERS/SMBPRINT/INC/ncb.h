//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1996          **/
/********************************************************************/
/* :ts=4 */

//***    ncb.h - Definitions and structures for handling NCBs
//


#ifndef NCB_INC    // protect for duplicate inclusion
#define NCB_INC 1

#include "SMB_Globals.h"
#include "FixedAlloc.h"

class ncb {
    public:
        int         ncb_lsn;                         // local session number
        char        ncb_callname[16];                // blank-padded name of remote
        BYTE        ncb_name[16];                    // our blank-padded netname    
        BYTE        ncb_reserve[14];                 // reserved, used by BIOS  
        WORD        ncb_length;                      // size of message buffer  
        DWORD       ncb_post;                        // Async notification handle
        BYTE       *ncb_buffer;                      // address of message buffer  
        BYTE        ncb_command;                     // command code
        BYTE        ncb_retcode;                     // return code
        BYTE        ncb_num;                         // number of our network name
        BYTE        ncb_rto;                         // rcv timeout/retry count  
        BYTE        ncb_sto;                         // send timeout/sys timeout
        BYTE        ncb_lana_num;                    // lana (adapter) number
        BYTE        ncb_cmd_cplt;                    // 0xff => commmand pending


        void * operator new(size_t size) { 
             return SMB_Globals::g_NCBAllocator.allocate(size);
        }
        void   operator delete(void *mem) {
            SMB_Globals::g_NCBAllocator.deallocate(mem);
        }  
}; /* ncb */


/*
 *    NCB Command codes
 */

#define NCBCALL          0x10        /* NCB CALL                          */
#define NCBLISTEN        0x11        /* NCB LISTEN                        */
#define NCBHANGUP        0x12        /* NCB HANG UP                       */
#define NCBSEND          0x14        /* NCB SEND                          */
#define NCBRECV          0x15        /* NCB RECEIVE                       */
#define NCBRECVANY       0x16        /* NCB RECEIVE ANY                   */
#define NCBCHAINSEND     0x17        /* NCB CHAIN SEND                    */
#define NCBDGSEND        0x20        /* NCB SEND DATAGRAM                 */
#define NCBDGRECV        0x21        /* NCB RECEIVE DATAGRAM              */
#define NCBDGSENDBC      0x22        /* NCB SEND BROADCAST DATAGRAM       */
#define NCBDGRECVBC      0x23        /* NCB RECEIVE BROADCAST DATAGRAM    */
#define NCBADDNAME       0x30        /* NCB ADD NAME                      */
#define NCBDELNAME       0x31        /* NCB DELETE NAME                   */
#define NCBRESET         0x32        /* NCB RESET                         */
#define NCBASTAT         0x33        /* NCB ADAPTER STATUS                */
#define NCBSSTAT         0x34        /* NCB SESSION STATUS                */
#define NCBCANCEL        0x35        /* NCB CANCEL                        */
#define NCBADDGRNAME     0x36        /* NCB ADD GROUP NAME                */
#define NCBENUM          0x37        /* NCB ENUMERATE LANA NUMBERS        */
#define NCBGATHERSEND    0x40        /* NCB GATHER SEND                   */
#define NCBSCATTERRCV    0x41        /* NCB SCATTER RECEIVE               */
#define NCBSEND_RCVANY   0x48        /* NCB TRANCEIVE                     */
#define NCBUNLINK        0x70        /* NCB UNLINK                        */
#define NCBSENDNA        0x71        /* NCB SEND NO ACK                   */
#define NCBCHAINSENDNA   0x72        /* NCB CHAIN SEND NO ACK             */
#define NCBHANGUPANY     0x73
#define NCBCALLNIU       0x74        /* UB special                        */
#define NCBRCVPKT        0x78        /* UB special                        */
#define NCBQUERYLANA     0x50 
#define ASYNCH           0x80        /* high bit set == asynchronous      */
#define NCBADDMHNAME     0x81
/*
 *    NCB Return codes
 */

#define NRC_GOODRET      0x00    /* good return                               */
#define NRC_BUFLEN       0x01    /* illegal buffer length                     */
#define NRC_BFULL        0x02    /* buffers full, no receive issued           */
#define NRC_ILLCMD       0x03    /* illegal command                           */
#define NRC_CMDTMO       0x05    /* command timed out                         */
#define NRC_INCOMP       0x06    /* message incomplete, issue another comman  */
#define NRC_BADDR        0x07    /* illegal buffer address                    */
#define NRC_SNUMOUT      0x08    /* session number out of range               */
#define NRC_NORES        0x09    /* no resource available                     */
#define NRC_SCLOSED      0x0a    /* session closed                            */
#define NRC_CMDCAN       0x0b    /* command cancelled                         */
#define NRC_DMAFAIL      0x0c    /* PC DMA failed                             */
#define NRC_DUPNAME      0x0d    /* duplicate name                            */
#define NRC_NAMTFUL      0x0e    /* name table full                           */
#define NRC_ACTSES       0x0f    /* no deletions, name has active sessions    */
#define NRC_INVALID      0x10    /* name not found or no valid name           */
#define NRC_LOCTFUL      0x11    /* local session table full                  */
#define NRC_REMTFUL      0x12    /* remote session table full                 */
#define NRC_ILLNN        0x13    /* illegal name number                       */
#define NRC_NOCALL       0x14    /* no callname                               */
#define NRC_NOWILD       0x15    /* cannot put * in NCB_NAME                  */
#define NRC_INUSE        0x16    /* name in use on remote adapter             */
#define NRC_NAMERR       0x17    /* called name cannot == name or name #      */
#define NRC_SABORT       0x18    /* session ended abnormally                  */
#define NRC_NAMCONF      0x19    /* name conflict detected                    */
#define NRC_BADREM       0x1a    /* incompatible remote                       */
#define NRC_IFBUSY       0x21    /* interface busy, IRET before retrying      */
#define NRC_TOOMANY      0x22    /* too many commands outstanding, retry later*/
#define NRC_BRIDGE       0x23    /* ncb_bridge field not 00 or 0              */
#define NRC_CANOCCR      0x24    /* command completed while cancel occurring  */
#define NRC_RESNAME      0x25    /* reserved name specified                   */
#define NRC_CANCEL       0x26    /* command not valid to cancel               */
#define NRC_MULT         0x33    /* multiple requests for same session        */
#define NRC_MAXAPPS      0x36    /* max number of applications exceeded       */
#define NRC_NORESOURCES  0x38    /* requested resources are not available     */
#define NRC_SYSTEM       0x40    /* system error                              */
#define NRC_ROM          0x41    /* ROM checksum failure                      */
#define NRC_RAM          0x42    /* RAM test failure                          */
#define NRC_DLF          0x43    /* digital loopback failure                  */
#define NRC_ALF          0x44    /* analog loopback failure                   */
#define NRC_IFAIL        0x45    /* interface failure                         */
#define NRC_ADPTMALFN    0x50    /* network adapter malfunction               */

#define NRC_PENDING        0xff    /* asynchronous command is not yet finished*/

#define INVALID_NAMENUM    0xff    /* Invalid name number */

#endif



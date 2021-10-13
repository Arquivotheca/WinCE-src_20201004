#ifndef __PARPORT_H__
#define __PARPORT_H__

#include "main.h"
#include "globals.h"
#include "util.h"
#include <windows.h>
#ifdef UNDER_CE
#include <pegdpar.h>    // parallel port driver definitions 
#endif

/******************************************************************************

the parallel port register structure:

        7   6   5   4   3   2   1   0   I/O PORT
------------------------------------------------
DATA    x   x   x   x   x   x   x   x   BASE
STAT   ~x   x   x   x   x   x   -   -   BASE + 1
CTRL    -   -   -   -  ~x   x  ~x  ~x   BASE + 2

DATA Register:

BIT     Value           Pin     Inverted
7       +Data Bit 0     2       NO
6       +Data Bit 1     3       NO
5       +Data Bit 2     4       NO
4       +Data Bit 3     5       NO
3       +Data Bit 4     6       NO
2       +Data Bit 5     7       NO
1       +Data Bit 6     8       NO
0       +Data Bit 7     9       NO

STATUS Register:

BIT     Value           Pin     Inverted    
7       +Busy           11      YES
6       -Ack            10      NO 
5       +Paper Out      12      NO
4       +Select In      13      NO
3       -Error          15      NO
2       x               x       x
1       x               x       x
0       x               x       x

COMMAND Register:

BIT     Value           Pin     Inverted
7       x               x       x
6       x               x       x
5       x               x       x
4       x               x       x
3       -Select         17      YES
2       -Initialize     16      NO
1       -Auto Feed      14      YES
0       -Strobe         1       YES

**************************************************************************

the parallel port lap-link connection:

Signal      Pin         Pin     Signal
------------------------------------------
bit 0       2   <---->  15      Error 
bit 1       3   <---->  13      Selected 
bit 2       4   <---->  12      Paper out 
bit 3       5   <---->  10      Ack
bit 4       6   <---->  11      Busy 
Error       15  <---->  2       bit 0 
Selected    13  <---->  3       bit 1 
Paper out   12  <---->  4       bit 2 
Ack         10  <---->  5       bit 3 
Busy        11  <---->  6       bit 4 
Ground      25  <---->  25      Ground 

*************************************************************************/

//
// lap-link specific data packets -- send to control
// the status bits of the connected PC
// probably don't need these
//
#define PACKET_ERR          0x01    //  0000 0001
#define PACKET_SEL          0x02    //  0000 0010
#define PACKET_PPO          0x04    //  0000 0100
#define PACKET_ACK          0x08    //  0000 1000
#define PACKET_BSY          0x10    //  0001 0000

class PARPORT 
{
    public:
    	PARPORT();
    	~PARPORT();
        BOOL Open( void );
        BOOL Open( LPTSTR );
        BOOL Close( void );
        BOOL GetTimeouts( LPCOMMTIMEOUTS, BOOL fUseWinAPI = FALSE );
        BOOL SetTimeouts( LPCOMMTIMEOUTS, BOOL fUseWinAPI = FALSE );
        BOOL GetDeviceID( LPBYTE, DWORD, LPDWORD );
        BOOL GetStatus( LPDWORD );
        BOOL GetECPChannel32( LPDWORD );
        BOOL Write( LPBYTE, DWORD, LPDWORD, BOOL fUseWinAPI = FALSE ); 
        BOOL Read( LPBYTE, DWORD, LPDWORD );      
        HANDLE GetHandle( void );

    protected:

        HANDLE m_hParallel;

};

#define LPPARPORT PARPORT*

#endif

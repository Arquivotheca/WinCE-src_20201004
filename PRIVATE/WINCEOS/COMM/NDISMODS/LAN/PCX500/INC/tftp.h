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
// TFTP.h : header file
//
#ifndef __tftp_h__
#define __tftp_h__

#include <fcntl.h>      /* Needed only for _O_RDWR definition */
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

#undef ERROR

#define	SEGSIZE		512		// data segment size 
#define PKTSIZE    SEGSIZE+4
//
#define BF_ALLOC -3				// alloc'd but not yet filled 
#define BF_FREE  -2				// free 
//
//-------------Packet types.
//
#define	RRQ	01			// read request 
#define	WRQ	02			// write request 
#define	DATA	03		// data packet 
#define	ACK	04			// acknowledgement 
#define	ERROR	05		// error code 
//
//////////////////////////////////////////
//
#define	EUNDEF		0		// not defined 
#define	ENOTFOUND	1		// file not found 
#define	EACCESS		2		// access violation 
#define	ENOSPACE	3		// disk full or allocation exceeded 
#define	EBADOP		4		// illegal TFTP operation 
#define	EBADID		5		// unknown transfer ID 
#define	EEXISTS		6		// file already exists
#define	ENOUSER		7		// no such user 
//
///////////////////////////////////////////////////
//
typedef struct _bf {
	int counter;            // size of data in buffer, or flag 
	char buf[PKTSIZE];      // room for data packet 
}bf;

typedef struct _errmsg {
	int e_code;
	const char *e_msg;
} errmsg;


typedef struct	_tftphdr {
	short	th_opcode;		// packet type 
	union {
		short	tu_block;	// block # 
		short	tu_code;	// error code 
		char	tu_stuff[1];	// request packet stuff 
	} th_u;
	char	th_data[1];		// data or error string 
}tftphdr;

#define	th_block	th_u.tu_block
#define	th_code		th_u.tu_code
#define	th_stuff	th_u.tu_stuff
#define	th_msg		th_data

class  CTFTP;
void nak( CTFTP *pSock, int error, SOCKADDR & from);


typedef struct _formats {
	const char *f_mode;
	int (*f_validate)(const char *, int, FILE **file, char *);
	int f_convert;
} formats;

extern int	makerequest(int request, LPCSTR name, tftphdr *tp, LPCSTR mode);
extern int	validate_access(const char *, int, FILE **file, char *);

/////////////////////////////////////////////////////////////////////////////
// CTFTP command target

class CTFTP : public CAsyncSocket
{
// Attributes
public:
	virtual ~CTFTP();
	CTFTP(){}
	CTFTP(FILE *file, formats *pf, SOCKADDR & from, BOOL IsCli=FALSE );
	int			receive(void *vp, int len);
	int			send(void *vp, int len);
	int			synchnet();
//	void		nak(int error);
	void		SetPath(  char *path );
	void		SetMode(  char *mode );

// Operations
public:
	tftphdr			*rw_init(int);
	tftphdr			*r_init();
	tftphdr			*w_init();
//
public:
//	int				m_hSock;
//
protected:
	CString			m_Path;
	CString			m_mode;
public:
	CFile			m_File;	
	FILE			*m_file;

	SOCKADDR		m_from;
	int				m_FromLen;
	
	formats			*m_pf;
	tftphdr			*m_dp;
	tftphdr			*m_ap;    // ack packet 

//	FILEBUFINITST	m_FBInitSt;
	bf				m_bfs[2];
	char			m_ackbuf[PKTSIZE];

	// FILE INIT 	
	int				m_newline;			// fillbuf: in middle of newline expansion 
	int				m_prevchar;			// putbuf: previous char (cr check) 
	int				m_nextone;			// index of next buffer to use
	int				m_current;			// index of buffer in use	
	BOOL			m_IsCli;
	BOOL			m_connected;
// Overrides
public:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTFTP)
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CTFTP)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

// Implementation
protected:
	int			m_block;
};

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CRcvTFTP command target

	FILE	* OpenFile( char *path, char *mode);

class CRcvTFTP : public CTFTP
{
// Attributes
public:

// Operations
public:
	virtual ~CRcvTFTP();
	CRcvTFTP(FILE *file, formats *pf, SOCKADDR & from, BOOL IsCli=FALSE  );
	BOOL		recvfile();
	int			writeit(int ct, int convert);
	int			write_behind(int convert);
// Overrides
public:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRcvTFTP)
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CRcvTFTP)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

// Implementation
protected:
};

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// CSendTFTP command target

class CSendTFTP : public CTFTP
{
// Attributes
public:

// Operations
public:
	virtual ~CSendTFTP();
	CSendTFTP(FILE *file, formats *pf, SOCKADDR & from, BOOL IsCli=FALSE  );
	BOOL	SendFile();
	void	read_ahead(int convert );
	int		readit(int convert );				 // if true, convert to ascii 

// Overrides
public:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSendTFTP)
	//}}AFX_VIRTUAL

	// Generated message map functions
	//{{AFX_MSG(CSendTFTP)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

// Implementation
protected:
};

/////////////////////////////////////////////////////////////////////////////
#endif
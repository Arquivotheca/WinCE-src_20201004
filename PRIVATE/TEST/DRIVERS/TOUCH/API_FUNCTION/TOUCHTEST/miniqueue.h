// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#ifndef __MINIQUEUE_H__
#define __MINIQUEUE_H__

#include <windows.h>

// --------------------------------------------------------------------
// 	MiniQueue Template Class
//
// 	Implementation of a simple, fixed size Queue with standard
//	Enqueue() and Dequeue() methods. Also includes a Clear() method
//	to empty the queueue, and an IsEmpty() method.
//
//	Note that this queue locally copies all data, so
//
template <class COPYABLE, UINT SIZE> class MiniQueue
{

public:
	//
	// constructor/destructor
	//
	MiniQueue( );
	~MiniQueue( );

	//
	// public functions
	//
	BOOL Enqueue( COPYABLE );
	BOOL Dequeue( COPYABLE * );
	BOOL IsEmpty( );
	VOID Clear( );

private:
	//
	// private data
	//
	COPYABLE m_rgQueue[SIZE];
	UINT m_nHead;
	UINT m_nTail;
	UINT m_nCount;	
	CRITICAL_SECTION m_csQueue;
};

// --------------------------------------------------------------------
// constructor     
template <class COPYABLE, UINT SIZE>
MiniQueue<COPYABLE, SIZE>::MiniQueue( ) :
    m_nHead(0),
    m_nTail(0),
    m_nCount(0)
{ 
    InitializeCriticalSection( &m_csQueue );
};

// --------------------------------------------------------------------
// destructor
template <class COPYABLE, UINT SIZE>
MiniQueue<COPYABLE, SIZE>::~MiniQueue( )
{ };

// --------------------------------------------------------------------
// is empty
template <class COPYABLE, UINT SIZE>
BOOL MiniQueue<COPYABLE, SIZE>::IsEmpty( )
{
    return ( m_nCount == 0 );
};    

// --------------------------------------------------------------------
// clear
template <class COPYABLE, UINT SIZE>
VOID MiniQueue<COPYABLE, SIZE>::Clear( )
{
    EnterCriticalSection( &m_csQueue );
    m_nCount = 0;
    m_nHead = 0;
    m_nTail = 0;
    LeaveCriticalSection( &m_csQueue );
};  

// --------------------------------------------------------------------
// enqueue
template <class COPYABLE, UINT SIZE>
BOOL MiniQueue<COPYABLE, SIZE>::Enqueue( COPYABLE element )
{
    BOOL fRet = TRUE;
    
    EnterCriticalSection( &m_csQueue );    
    if( m_nCount >= SIZE )
    {
        fRet = FALSE;
        goto Exit;
    }    
    m_rgQueue[m_nTail] = element;    
    m_nTail = (m_nTail + 1) % SIZE;
    m_nCount++;

Exit:
    LeaveCriticalSection( &m_csQueue );
    
    return TRUE;
};

// --------------------------------------------------------------------
// dequeue
template <class COPYABLE, UINT SIZE>
BOOL MiniQueue<COPYABLE, SIZE>::Dequeue( COPYABLE *element )
{
    BOOL fRet = TRUE;

    EnterCriticalSection( &m_csQueue );
    if( m_nCount == 0 )
    {
        fRet = FALSE;
        goto Exit;
    }
    
    *element = m_rgQueue[m_nHead];
    m_nHead = (m_nHead + 1) % SIZE;
    m_nCount--;   
	
Exit:
    LeaveCriticalSection( &m_csQueue );    
    return fRet;    
};

#endif

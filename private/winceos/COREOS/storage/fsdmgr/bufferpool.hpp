//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __BUFFERPOOL_HPP__
#define __BUFFERPOOL_HPP__

template <typename BufferType, size_t BufferSize>
struct BufferPoolItem
{
    union {
        struct BufferPoolItem<BufferType, BufferSize>* pNext;
        BufferType Buffer[BufferSize];
    };

};

template <typename BufferType, size_t BufferSize>
class BufferPool
{
    public: 
        BufferPool (DWORD MaxPoolSize) :
            m_MaxPoolSize (MaxPoolSize),
            m_PoolSize (0),
            m_pFirstPoolItem (0)
        {
            ::InitializeCriticalSection (&m_cs);
        }

        ~BufferPool ()
        {
            ::DeleteCriticalSection (&m_cs);
        }

        inline BufferType* AllocateBuffer () 
        {
            BufferType* pBuffer = NULL;

            ::EnterCriticalSection (&m_cs);
            
            if (m_pFirstPoolItem) {
                // Use the first pool item on the list.
                pBuffer = m_pFirstPoolItem->Buffer;
                m_pFirstPoolItem = m_pFirstPoolItem->pNext;
                m_PoolSize --;

            } else {

#pragma prefast(disable: 14, "This item is intentionally leaked; once the pool has reached max size, buffers will be freed in FreeBuffer")
                BufferPoolItem<BufferType, BufferSize>* pPoolItem = 
                    new BufferPoolItem<BufferType, BufferSize>;
#pragma prefast(pop)

                if (pPoolItem) {
                    pBuffer = pPoolItem->Buffer;
                }
            }

            ::LeaveCriticalSection (&m_cs);

            return pBuffer;
        }

        inline void FreeBuffer (BufferType* pBuffer)
        {
            ::EnterCriticalSection (&m_cs);

            // Cast the incoming buffer item to a buffer pool item.
            BufferPoolItem<BufferType, BufferSize>* pPoolItem = 
                reinterpret_cast<BufferPoolItem<BufferType, BufferSize>*> (pBuffer);

            if (m_PoolSize >= m_MaxPoolSize) {
                // Buffer pool is full, just free the memory.
                delete pPoolItem;

            } else {
                // Add this item to the head of the pool item list.
                pPoolItem->pNext = m_pFirstPoolItem;
                m_pFirstPoolItem = pPoolItem;
                m_PoolSize ++;
            }

            ::LeaveCriticalSection (&m_cs);
        }

    private:
        operator=(BufferPool<BufferType, BufferSize>&);
        const DWORD m_MaxPoolSize;
        CRITICAL_SECTION m_cs;
        DWORD m_PoolSize;
        BufferPoolItem<BufferType, BufferSize>* m_pFirstPoolItem;
};

#endif // __BUFFERPOOL_HPP__

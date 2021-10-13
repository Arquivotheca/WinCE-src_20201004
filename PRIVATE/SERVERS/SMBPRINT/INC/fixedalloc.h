#ifndef FIXED_ALLOC
#define FIXED_ALLOC

#include <svsutil.hxx>
#include "SMB_Globals.h"

class InitClass {
public:
    InitClass() {
        static BOOL fInited = FALSE;
        if(FALSE == fInited) {
            svsutil_Initialize();
            fInited = TRUE;
        } else {
            ASSERT(FALSE);
        }        
    }
    ~InitClass() {
        svsutil_DeInitialize();
    }
};

template <size_t InitCount, typename T>
class pool_allocator
{
public:    
        pool_allocator() {
            #ifdef DEBUG
                uiAllocated = 0;
                uiRequested = InitCount;
            #endif
        }
        
        //
        // Copy ctor must initialize free list to empty;
        // otherwise, two instances of free_list allocator 
        // would end up using the same "free" blocks.
        // 
        pool_allocator(const pool_allocator&) {            
            #ifdef DEBUG
                uiAllocated = 0;
                uiRequested = InitCount;
            #endif
        }
        
        //
        // The lifetime of the allocator is typically the same as
        // lifetime of the container using it.
        //
        ~pool_allocator() {
            #ifdef DEBUG
                ASSERT(0 == uiAllocated);
            #endif
        }    
        void* allocate(size_t Size) {
            FixedMemDescr *pBlock = get_block_alloc();
            if(NULL == get_block_alloc()) {
                pBlock = get_block_alloc(svsutil_AllocFixedMemDescr(Size, InitCount));
                
                if(NULL != pBlock) {
                    #ifdef DEBUG
                        GetSize(&Size);
                        uiAllocated = 0;
                    #endif
                } else {
                    return NULL;
                }
            }  

            #ifdef DEBUG
                ASSERT(Size == GetSize());
                ASSERT(NULL != pBlock);
            #endif
            void *pRet = svsutil_GetFixed(pBlock);     
            
            #ifdef DEBUG
                if(NULL != pRet) { 
                    uiAllocated ++; 
                }
                if(uiAllocated >= uiRequested) {
                    RETAILMSG(1, (L"Pool Alloc: many nodes on list! %d nodes %d size", uiAllocated, GetSize()));
                };
            #endif
            return pRet; 
        }

        void deallocate(void* Ptr) {
            FixedMemDescr *pBlock = get_block_alloc();
            ASSERT(NULL != pBlock);            
            svsutil_FreeFixed(Ptr, pBlock); 
            #ifdef DEBUG
                uiAllocated --;
            #endif
        }

        private:
            FixedMemDescr *get_block_alloc(FixedMemDescr *_FixedMemDesc = NULL)
            {
                static FixedMemDescr *m_FixedMemDesc = NULL;
                
                if(NULL == m_FixedMemDesc) {                   
                    m_FixedMemDesc = _FixedMemDesc;
                }
                return m_FixedMemDesc;
            }
            #ifdef DEBUG 
                UINT GetSize(UINT *_puiSize = NULL) {
                    static UINT uiSize = 0;
                    
                    if(NULL == _puiSize) {
                        return uiSize;
                    } else {
                        uiSize = *_puiSize;
                        return uiSize;
                    } 
                }
            #endif
        private:
            
            #ifdef DEBUG
                UINT uiAllocated, uiRequested;
            #endif

            
};


#endif

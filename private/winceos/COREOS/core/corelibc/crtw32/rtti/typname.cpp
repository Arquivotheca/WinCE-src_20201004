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
/***
*typename.cpp - Implementation of type_info.name() for RTTI.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module provides an implementation of the class member function
*       type_info.name() for Run-Time Type Information (RTTI).
****/

#define _USE_ANSI_CPP   /* Don't emit /lib:libcp directive */

#ifndef _SYSCRT
#include <sect_attribs.h>
#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <typeinfo.h>
#include <mtdll.h>
#include <string.h>
#include <dbgint.h>
#include <undname.h>

static __type_info_node __type_info_root_node;
/*
 * type_info::~type_info() has been moved from typinfo.cpp to typname.cpp.
 * The reason being we need to clean the link list when destructing the
 * object.
 */
_CRTIMP void __cdecl type_info::_Type_info_dtor(type_info *_This)
{
    _mlock(_TYPEINFO_LOCK);
    __TRY
        if (_This->_M_data != NULL) {
            /*
             * We should first check the global link list before freeing _M_data.
             * Ideally we should always find _M_data in the linklist.
             */
            for(__type_info_node *pNode = __type_info_root_node._Next,*tmpNode = &__type_info_root_node;
                pNode!=NULL;
                pNode = tmpNode)
            {
                if(pNode->_MemPtr == _This->_M_data) {
                    /*
                     * Once the node is found, delete it from the list and
                     * free the memroy.
                     */
                    tmpNode->_Next = pNode->_Next;
                    _free_base(pNode);
                    break;
                }
                tmpNode=pNode;
                /*
                 * This should always be true. i.e. we should always find _M_data
                 * int the global linklist.
                 */
                _ASSERTE(pNode->_Next != NULL);
            }
            /*
             * Ideally we should be freeing this in the loop but just in case
             * something is wrong, we make sure we don't leak the memory.
             */
            _free_base(_This->_M_data);

            /*
             * Note that the same object can exist in different threads. This
             * means to be very sure, we must always set _M_data to NULL so that
             * we don't land in the _ASSERTE in the previous lines.
             */
            _This->_M_data = NULL;
        }
    __FINALLY
        _munlock(_TYPEINFO_LOCK);
    __END_TRY_FINALLY

}

#ifdef _GUARDED_CRT
extern "C" void __cdecl GuardGrantWriteLocked(const void *_p, size_t _s);
#endif

_CRTIMP const char *__cdecl type_info::_Name_base(const type_info *_This,__type_info_node* __ptype_info_node)
{
        void *pTmpUndName;
        size_t len;

        if (_This->_M_data == NULL) {
            /*
             * Note that we don't really have to specifically pass the parameters
             * as _malloc_base or malloc. defines in dbgint.h should take care
             * of all these memory macros.
             */
            _mlock (_TYPEINFO_LOCK);
            if ((pTmpUndName = __unDName(NULL,
                                         (_This->_M_d_name)+1,
                                         0,
                                         &_malloc_base,
                                         &_free_base,
                                         UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY)) == NULL){
                _munlock(_TYPEINFO_LOCK);
                return NULL;
                }

            /*
             * Pad all the trailing spaces with null. Note that len-- > 0 is used
             * at left side which depends on operator associativity. Also note
             * that len will be used later so don't trash.
             */
            for (len=strlen((char *)pTmpUndName); len-- > 0 && ((char *)pTmpUndName)[len] == ' ';) {
                ((char *)pTmpUndName)[len] = '\0';
            }

            __TRY
                /*
                 * We need to check if this->_M_data is still NULL, this will
                 * prevent the memory leak.
                 */
                if (_This->_M_data == NULL) {
                    /*
                     * allocate a node which will store the pointer to the memory
                     * allocated for this->_M_data. We need to store all this in
                     * linklist so that we can free them as process exit. Note
                     * that __clean_type_info_names is freeing this memory.
                     */
                    __type_info_node *pNode = (__type_info_node *)_malloc_base(sizeof(__type_info_node));
                    if (pNode != NULL) {

                        /*
                         * We should be doing only if we are sucessful in allocating
                         * node pointer. Note that we need to add 2 to len, this
                         * is because len = strlen(pTmpUndName)-1.
                         */
#ifdef _GUARDED_CRT
                        // this routine writes to a data structure allocated by the compiler that we are not coloring.
                        // Alternatively could use guard(ignore) on all CRT routines that modify the structure or free
                        // the data structures allocated here.
                        GuardGrantWriteLocked(&((type_info *)_This)->_M_data, sizeof(((type_info *)_This)->_M_data));
#endif

                        if ((((type_info *)_This)->_M_data = _malloc_base(len+2)) != NULL) {
                            _ERRCHECK(strcpy_s ((char *)((type_info *)_This)->_M_data, len+2, (char *)pTmpUndName));
                            pNode->_MemPtr = _This->_M_data;

                            /*
                             * Add this to global linklist. Note that we always
                             * add this as second element in linklist.
                             */
                            pNode->_Next = __ptype_info_node->_Next;
                            __ptype_info_node->_Next = pNode;
                        } else {
                            /*
                             * Free node pointer as there is no allocation for
                             * this->_M_data, this means that we don't really
                             * need this in the link list.
                             */
                            _free_base(pNode);
                        }
                    }
                }
                /*
                 * Free the temporary undecorated name.
                 */
                _free_base (pTmpUndName);
            __FINALLY
                _munlock(_TYPEINFO_LOCK);
            __END_TRY_FINALLY


        }

        return (char *) _This->_M_data;
}

/*
 * type_info::~type_info() has been moved from typinfo.cpp to typname.cpp.
 * The reason being we need to clean the link list when destructing the
 * object.
 */
_CRTIMP void __cdecl type_info::_Type_info_dtor_internal(type_info *_This)
{
    _mlock(_TYPEINFO_LOCK);
    __TRY
        if (_This->_M_data != NULL) {
            /*
             * We should first check the global link list before freeing _M_data.
             * Ideally we should always find _M_data in the linklist.
             */
            for(__type_info_node *pNode = __type_info_root_node._Next,*tmpNode = &__type_info_root_node;
                pNode!=NULL;
                pNode = tmpNode)
            {
                if(pNode->_MemPtr == _This->_M_data) {
                    /*
                     * Once the node is found, delete it from the list and
                     * free the memroy.
                     */
                    tmpNode->_Next = pNode->_Next;
                    _free_base(pNode);
                    break;
                }
                tmpNode=pNode;
                /*
                 * This should always be true. i.e. we should always find _M_data
                 * int the global linklist.
                 */
                _ASSERTE(pNode->_Next != NULL);
            }
            /*
             * Ideally we should be freeing this in the loop but just in case
             * something is wrong, we make sure we don't leak the memory.
             */
            _free_base(_This->_M_data);

            /*
             * Note that the same object can exist in different threads. This
             * means to be very sure, we must always set _M_data to NULL so that
             * we don't land in the _ASSERTE in the previous lines.
             */
            _This->_M_data = NULL;
        }
    __FINALLY
        _munlock(_TYPEINFO_LOCK);
    __END_TRY_FINALLY

}

// Pure can't call undname because it can't materialize native function pointers.
extern "C" _CRTIMP void* __cdecl __unDNameHelper(pchar_t outputString,
        pcchar_t name,
        int maxStringLength,
        unsigned short disableFlags
){
    if (disableFlags == 0)
    {
        disableFlags = UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY;
    }

   /*
    * Note that we don't really have to specifically pass the parameters
    * as _malloc_base or malloc. defines in dbgint.h should take care
    * of all these memory macros.
    */
    return __unDName(outputString, name, maxStringLength, &_malloc_base, &_free_base, disableFlags);
}


_CRTIMP const char *__cdecl type_info::_Name_base_internal(const type_info *_This,__type_info_node* __ptype_info_node)
{
        void *pTmpUndName;
        size_t len;

        if (_This->_M_data == NULL) {
            _mlock (_TYPEINFO_LOCK);
            __TRY
                /*
                 * We need to check if this->_M_data is still NULL, this will
                 * prevent the memory leak.
                 */
                if (_This->_M_data == NULL) {
                        if ((pTmpUndName = __unDNameHelper(NULL,
                                                     (_This->_M_d_name)+1,
                                                     0,
                                                     UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY)) == NULL){
                            _munlock(_TYPEINFO_LOCK);
                            return NULL;
                            }

                        /*
                         * Pad all the trailing spaces with null. Note that len-- > 0 is used
                         * at left side which depends on operator associativity. Also note
                         * that len will be used later so don't trash.
                         */
                        for (len=strlen((char *)pTmpUndName); len-- > 0 && ((char *)pTmpUndName)[len] == ' ';) {
                            ((char *)pTmpUndName)[len] = '\0';
                        }

                    /*
                     * allocate a node which will store the pointer to the memory
                     * allocated for this->_M_data. We need to store all this in
                     * linklist so that we can free them as process exit. Note
                     * that __clean_type_info_names is freeing this memory.
                     */
                    __type_info_node *pNode = (__type_info_node *)_malloc_base(sizeof(__type_info_node));
                    if (pNode != NULL) {

                        /*
                         * We should be doing only if we are sucessful in allocating
                         * node pointer. Note that we need to add 2 to len, this
                         * is because len = strlen(pTmpUndName)-1.
                         */
                        char *pTmpTypeName = NULL;
                        if ((pTmpTypeName = (char *)_malloc_base(len+2)) != NULL) {
                            _ERRCHECK(strcpy_s (pTmpTypeName, len+2, (char *)pTmpUndName));
                            ((type_info *)_This)->_M_data = pTmpTypeName;
                            pNode->_MemPtr = _This->_M_data;

                            /*
                             * Add this to global linklist. Note that we always
                             * add this as second element in linklist.
                             */
                            pNode->_Next = __ptype_info_node->_Next;
                            __ptype_info_node->_Next = pNode;
                        } else {
                            /*
                             * Free node pointer as there is no allocation for
                             * this->_M_data, this means that we don't really
                             * need this in the link list.
                             */
                            _free_base(pNode);
                        }
                    }

                        /*
                         * Free the temporary undecorated name.
                         */
                        _free_base (pTmpUndName);
                }
            __FINALLY
                _munlock(_TYPEINFO_LOCK);
            __END_TRY_FINALLY


        }

        return (char *) _This->_M_data;
}

/*

/*
 * __clean_type_info_names_internal is invoked by __clean_type_info_names on dll unload.
 */
extern "C" _CRTIMP void __cdecl __clean_type_info_names_internal(__type_info_node * p_type_info_root_node)
{
    _mlock(_TYPEINFO_LOCK);
    __TRY
        /*
         * Loop through the link list and delete all the entries.
         */
        for (__type_info_node *pNode = p_type_info_root_node->_Next, *tmpNode=NULL;
             pNode!=NULL;
             pNode = tmpNode)
        {
            tmpNode = pNode->_Next;
            _free_base(pNode->_MemPtr);
            _free_base(pNode);
        }
    __FINALLY
        _munlock(_TYPEINFO_LOCK);
    __END_TRY_FINALLY
}

#else
#include <stdlib.h>
#include <typeinfo.h>
#include <mtdll.h>
#include <string.h>
#include <dbgint.h>
#include <undname.h>

_CRTIMP const char *__thiscall type_info::name() const
{
        void *pTmpUndName;


        if (this->_M_data == NULL) {
            _mlock (_TYPEINFO_LOCK);
            if ((pTmpUndName = __unDName(NULL, (this->_M_d_name)+1, 0, &_malloc_base, &_free_base, UNDNAME_32_BIT_DECODE | UNDNAME_TYPE_ONLY)) == NULL){
                _munlock(_TYPEINFO_LOCK);
                return NULL;
            }
            for (int l=(int)strlen((char *)pTmpUndName)-1; ((char *)pTmpUndName)[l] == ' '; l--)
                ((char *)pTmpUndName)[l] = '\0';

            __TRY
                size_t cch = strlen((char *)pTmpUndName) + 1;
                if ((((type_info *)this)->_M_data = _malloc_base (cch)) != NULL)
                    strcpy_s ((char *)((type_info *)this)->_M_data, cch, (char *)pTmpUndName);
                _free_base (pTmpUndName);
            __FINALLY
                _munlock(_TYPEINFO_LOCK);
            __END_TRY_FINALLY


        }


        return (char *) this->_M_data;
}
#endif

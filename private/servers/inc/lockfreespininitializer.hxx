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
#include <windows.h>


// NOTE:  lock free implementation implemented via a spin-lock
//        all waiting threads will spin. thus users of this class
//        must not block in there initialization routines
class LockFreeSpinInitializer
{
    public:
        LockFreeSpinInitializer() : m_State(UNINITIALIZED) {}
    
        // InitializeIfNecessary:
        //  
        //      - initialization routine will be called once
        //      - once initialized it will not be possible to reinitialize
        //      - supports single instance lazy instantiation
        //
        template <typename FUNCPTR>
        inline void InitializeIfNecessary(FUNCPTR);
        inline BOOL Initialized();
    
    private:
        typedef enum _State { UNINITIALIZED, INITIALIZING, INITIALIZED } State;
        volatile State m_State;
};


inline BOOL LockFreeSpinInitializer::Initialized()
{
    volatile State curReadState = m_State;
    return (INITIALIZED == curReadState);
}

template <typename FUNCPTR>
inline void LockFreeSpinInitializer::InitializeIfNecessary(FUNCPTR pfcnInit)
{
    volatile State curReadState = m_State;
    if(UNINITIALIZED == curReadState)
        {
        if(UNINITIALIZED == InterlockedCompareExchange((LPLONG)&m_State,
                                                       (LONG)INITIALIZING,
                                                       (LONG)UNINITIALIZED))
            {
            // initialization state properly changed
            // perform initialization here
            
            // FUNCPTR type must be either a void function taking no parameters
            // or it must be a class supporting an operator()
            // use a functor object to pass parameters and get return values
            __try { pfcnInit(); } __except(EXCEPTION_EXECUTE_HANDLER) { /* ignore exception for time being */ }

            // mark initialized and continue
            LONG m_CurrentState = InterlockedExchange((LPLONG)&m_State,(LONG)INITIALIZED);
            ASSERT(INITIALIZING == m_CurrentState);
            }
        // no else block
        // let fall through
        }

    if(INITIALIZING == curReadState)        
        {
        // some thread is currently initializing the routine
        // spin until initialization is complete
        // this is a bounded spin since the initialization routines are bounded
        while(true) { curReadState = m_State; if(INITIALIZED == curReadState) break; }
        }


    // else state is initialized
    // this implementation does not allow reinitialization
    // other implementations might keep track of access to initialized objects
    return;
}

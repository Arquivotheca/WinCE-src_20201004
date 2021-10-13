//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma warning(push)

// 4284 warning for operator-> returning non-pointer;
//      compiler issues it even if -> is not used for the specific instance
#pragma warning(disable: 4284) 


#if 0
/**/    // 1. instances of auto_xxx template can be used to automatically close a resource
/**/    
/**/    ce::auto_handle hEvent;
/**/    
/**/    hEvent = CreateEvent(/*...*/)
/**/    
/**/    // event handle will be closed when hEvent goes out of scope
/**/    // if hEvent was a class member it would be closed when object
/**/    // of the class is destroyed
/**/
/**/    // 2. use valid() to check if resource is valid
/**/    
/**/    ce::auto_bstr bstr;
/**/    
/**/    bstr = SysAllocString(/*...*/);
/**/    
/**/    if(!bstr.valid())
/**/        return E_OUT_OF_MEMORY;
/**/
/**/    // 3. use close() to explicitly close resource
/**/    
/**/    ce::auto_hfile fFile;
/**/
/**/    hFile = CreateFile(L"filename", /*...*/);
/**/                                
/**/    /*...*/
/**/
/**/    hFile.close();           // close file
/**/    DeleteFile(L"filename"); // and delete it
/**/
/**/    // 4. auto_xxx has the same memory representation as managed resource
/**/
/**/    ce::auto_handle hEvents[3];
/**/
/**/    hEvents[0] = CreateEvent(/*...*/);
/**/    hEvents[1] = CreateEvent(/*...*/);
/**/    hEvents[2] = CreateEvent(/*...*/);
/**/
/**/    WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]), &hEvents[0], false, INFINITE);
/**/
/**/    // 5. define your own instances of auto_xxx as needed (add to this header if generic)
/**/
/**/    ce::auto_hfile   hFile;
/**/    ce::auto_handle  hMap;
/**/    // char*                        - type of resource
/**/    // BOOL (__stdcall *)(LPCVOID)  - type of poionter to "close" function for resource
/**/    // UnmapViewOfFile              - "close" function for resource
/**/    // NULL                         - invalid value for resource
/**/    ce::auto_xxx<char*, BOOL (__stdcall *)(LPCVOID), UnmapViewOfFile, NULL> pchFile;
/**/    
/**/    hFile = CreateFile(/*...*/);
/**/    hMap = CreateFileMapping(/*...*/);
/**/    pchFile = reinterpret_cast<char*>(MapViewOfFile(hMap, /*...*/);
/**/
#endif

namespace ce
{

#ifndef _AUTO_XXX_
#define _AUTO_XXX_

// template class auto_xxx
//
// T        - resource type
// Fn       - type of pointer to "close" function for the resource
// fnClose  - "close" function for the resource
// _Invalid - invalid value for the resource (should be of type T when compiler can handle this)
//
template<class T, class Fn, Fn fnClose, LONG_PTR _Invalid>
class auto_xxx
{
    // disallow assigment of auto_xxx to auto_xxx 
    void operator=(const auto_xxx<T, Fn, fnClose, _Invalid>& _Y)
		{Assert(false) };

public:
	explicit auto_xxx(T _X = reinterpret_cast<T>(_Invalid))
		: x(_X) 
		{}

	auto_xxx(const auto_xxx<T, Fn, fnClose, _Invalid>& _Y)
		: x(_Invalid)
	{
		// because there is no notion of ownership it is not generally illegal
		// to copy auto_xxx object; 
		// it is valid only if the managed handle is invalid; this case is also
		// quite useful since it allows adding object with uninitialized auto_xxx
		// to collections
		Assert(_Y.x == reinterpret_cast<T>(_Invalid));
	}
	
	auto_xxx<T, Fn, fnClose, _Invalid>& 
    operator=(const T& _Y)
	{
		if(x != reinterpret_cast<T>(_Invalid))
			(*fnClose)(x);

		x = _Y;

		return (*this); 
	}
	
	~auto_xxx()
	{
		if(x != reinterpret_cast<T>(_Invalid))
			(*fnClose)(x);
	}
	
    void close()
    {
        if(x != reinterpret_cast<T>(_Invalid))
		{
			(*fnClose)(x);

			x = reinterpret_cast<T>(_Invalid);
		}
    }
	
	bool valid()
	    {return x != reinterpret_cast<T>(_Invalid); }

    T operator->()
		{return x; }

	const T* operator&() const
        {return &x; }

    T* operator&()
        {return &x; }
    
    operator T()
		{return x; }

private:
	T x;
};

typedef auto_xxx<HINSTANCE, BOOL(__stdcall *)(HINSTANCE), FreeLibrary, 0>               auto_hlibrary;
typedef auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), CloseHandle, NULL>                 auto_handle;
typedef auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), CloseHandle, -1>                   auto_hfile;
typedef auto_xxx<HKEY, LONG (__stdcall *)(HKEY), RegCloseKey, NULL>						auto_hkey;

#endif // _AUTO_XXX_

#ifdef __MSGQUEUE_H__
#	ifndef _AUTO_MSG_QUEUE_
#	define _AUTO_MSG_QUEUE_

typedef auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), CloseMsgQueue,	NULL>				auto_msg_queue;

#	endif // _AUTO_MSG_QUEUE_
#endif // __MSGQUEUE_H__

#ifdef _OLEAUTO_H_
#	ifndef _AUTO_OLEAUTO_
#	define _AUTO_OLEAUTO_

typedef auto_xxx<BSTR, void (__stdcall *)(BSTR), SysFreeString, NULL>                   auto_bstr;

#	endif // _AUTO_OLEAUTO_
#endif  // _OLEAUTO_H_

#if defined _WININET_ || defined _DUBINET_
#	ifndef _AUTO_WININET_
#	define _AUTO_WININET_

typedef auto_xxx<HINTERNET, BOOL (__stdcall *)(HINTERNET), InternetCloseHandle, NULL>   auto_hinternet;

#	endif // _AUTO_WININET_
#endif // _WININET_ || _DUBINET_

}; // namespace ce

#pragma warning(pop)


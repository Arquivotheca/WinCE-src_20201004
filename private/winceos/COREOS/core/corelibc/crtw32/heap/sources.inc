!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft shared
source or premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license agreement,
you are not authorized to use this source code. For the terms of the license,
please see the license agreement between you and Microsoft or, if applicable,
see the SOURCE.RTF on your install media or the root of your tools installation.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif

TARGETTYPE=LIBRARY

# Include STL headers from internal location, not from publics
INCLUDES=..\..\stdhpp;..\..\..\..\inc;..\..\h
#INCLUDES=..\..\..\inc;..\h
NO_WCHAR_T=1

SOURCES= \
    ..\align.c \
    ..\calloc.c \
    ..\calloc_impl.c \
    ..\crtheap.c \
#   defined in stdcpp\delet4.cpp
#    delaopnt.cpp \
    ..\delete.cpp \
    ..\delete2.cpp \
#   defined in stdcpp\delet3.cpp
#    delopnt.cpp \
    ..\expand.c \
    ..\free.c \
#   _freea is define inline in malloc.h
#    freea.c \
    ..\handler.cpp \
    ..\heapadd.c \
    ..\heapchk.c \
    ..\heaphook.c \
    ..\heapinit.c \
    ..\heapmin.c \
    ..\heapused.c \
# CE doesn't support _heapwalk api from CRT
#    heapwalk.c \
    ..\hpabort.c \
    ..\malloc.c \
    ..\msize.c \
#   Defined in stdcpp\newop_s.c.
#TODO: This version uses _heap_alloc(). Is it needed?
#    new.cpp \
# Defined in stdcpp\newop3.cpp
#    newaopnt.cpp \
# Defined in stcpp\newop2.cpp
#    newopnt.cpp \
# Defined in stdcpp\new_aop.cpp
#    new2.cpp \
    ..\new_mode.cpp \
#defined in stdcpp\nomemory.cpp
#    nothrow0.cpp \
    ..\realloc.c \
    ..\recalloc.c \
    ..\resetstk.c \
    ..\setnewh.cpp \
    ..\_newmode.c

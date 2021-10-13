;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft shared
; source or premium shared source license agreement under which you licensed
; this source code. If you did not accept the terms of the license agreement,
; you are not authorized to use this source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the SOURCE.RTF on your install media or the root of your tools installation.
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
;
;-------------------------------------------------------------------------------
;
;
;-------------------------------------------------------------------------------
.486p

include kxx86.inc

PAGE_SIZE               EQU 1000h
PAGE_ATTRIB_RW          EQU 000001100011b
PAGE_ATTRIB_RO_USER     EQU 000100100101b
PDE_PS                  EQU 080H
PDE_G                   EQU 100h

CPUID_PSE               EQU   0008H
CPUID_PGE               EQU   2000H

SYSTEM_PAGES            EQU 3000h               ; 3 system pages:

; offset within SYSTEM_PAGES
SYSPAGE_PAGEDIR         EQU 0                   ; Page Directory:       1 page
SYSPAGE_SYSCALL         EQU 1000h               ; Syscall Page:         1 page
SYSPAGE_SYSCALLDIR      EQU 2000h               ; Syscall Page Dir:     1 page

ZERO_BLOCK_SIZE         EQU 1000h               ; page needs to be zero'd (page directory)

KGDT_R0_CODE            EQU 8h
KGDT_R0_DATA            EQU 10h

PG_MASK                 EQU 80000000h
WP_MASK                 EQU 00010000h

LIN_TO_PHYS_OFFSET      EQU 80000000h

OFFSET32                EQU <OFFSET FLAT:>

; offsets in pTOC
OFFSET_ULRAMFREE        EQU 24                  ; offset into pTOC for ulRamFree field

; offsets in kData
; all the offsets below must match nkx86.h
OFFSET_MEM_FOR_PT       EQU 009ch               ; offset into _KDATA for nMemForPT field
OFFSET_CPU_CAP          EQU 02a4h               ; offset into _KDATA for dwCpuCap field
OFFSET_ADDRMAP          EQU 02ach               ; offset into _KDATA for pAddrMap field

;-------------------------------------------------------------------------------
; Kernel Data Segment ; KStack, Page Directory, KPage, Page Table pool
;-------------------------------------------------------------------------------
.KDATA segment para public 'STACK'

                        db 1000h dup (?)        ;
_KDBase                 db 07fch dup (?)        ; 6k Kernel Stack
_KStack                 dd ?
_KData                  db 0800h dup (?)        ; 2k KData

.KDATA ends


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
_TEXT segment para public 'TEXT'

extrn _x86Init:near
extrn _pTOC:near

        align   4

;-------------------------------------------------------------------------------
; Figure out Virtual Address of a Physical Address
;
; uses ecx, edx (uses registers per calling convention allowed)
;
; (esi) = ptr to OEMAddressTable
; (eax) = PA
;
; On return
; (eax) = VA
;-------------------------------------------------------------------------------
VaFromPa PROC NEAR 
        mov     edx, esi                ; (edx) = &OEMAddressTable[0]

TestNextPa:
        cmp     dword ptr [edx+8], 0    ; last entry?
        je      short PaNotFound        ; entry not found
        mov     ecx, dword ptr [edx+4]  ; (ecx) = OEMAddressTable[i].paddr
        cmp     eax, ecx                ; Is (PA >= OEMAddressTable[i].paddr)?
        jb      short NextPa            ; go to next entry if not
        add     ecx, dword ptr [edx+8]  ; (ecx) = OEMAddressTable[i].paddr + OEMAddressTable[i].size
        cmp     eax, ecx                ; Is (PA < OEMAddressTable[i].paddr + OEMAddressTable[i].size)?
        jb      short PaFound           ; Found if it's true

NextPa:
        add     edx, 12                 ; i ++
        jmp     short TestNextPa        ; test next entry

PaNotFound:
        jmp     short PaNotFound        ; not much we can do if can't find the mapping
                                        ; spin forever

PaFound:
        sub     eax, dword ptr [edx+4]  ; PA -= PAStart
        add     eax, dword ptr [edx]    ; PA += VAStart
        ret

VaFromPa ENDP

;-------------------------------------------------------------------------------
; Figure out Physical Address of a Virtual Address
;
; uses ecx, edx (uses registers per calling convention allowed)
;
; (esi) = ptr to OEMAddressTable
; (eax) = VA
;
; On return
; (eax) = PA
;-------------------------------------------------------------------------------
PaFromVa PROC NEAR 
        mov     edx, esi                ; (edx) = &OEMAddressTable[0]

TestNextVa:
        mov     ecx, dword ptr [edx]    ; (ecx) = OEMAddressTable[i].vaddr
        test    ecx, ecx                ; last entry?
        jz      short VaNotFound        ; entry not found
        cmp     eax, ecx                ; Is (VA >= OEMAddressTable[i].vaddr)?
        jb      short NextVa            ; go to next entry if not
        add     ecx, dword ptr [edx+8]  ; (ecx) = OEMAddressTable[i].vaddr + OEMAddressTable[i].size
        cmp     eax, ecx                ; Is (VA < OEMAddressTable[i].vaddr + OEMAddressTable[i].size)?
        jb      short VaFound           ; Found if it's true

NextVa:
        add     edx, 12                 ; i ++
        jmp     short TestNextVa        ; test next entry

VaNotFound:
        jmp     short VaNotFound        ; not much we can do if can't find the mapping
                                        ; spin forever

VaFound:
        sub     eax, dword ptr [edx]    ; VA -= VAStart
        add     eax, dword ptr [edx+4]  ; VA += PAStart
        ret

PaFromVa ENDP



;
; Fill PageDirectory of an OEMAddressTable entry using 4M pages
; On Entry
; (eax) = PA of an OEMAddressTable Entry
; (ebx) = PA of Page Directory
; (edx) = PDE capability bits (PDE_PS and PDE_G)
; 
;
; !!! All Reigsters preserved !!!
;
FillPD4M PROC NEAR
        pushad

        mov     esi, dword ptr[eax]     ; (esi) = VA of region
        shr     esi, 20                 ; (esi) = offset into the Page Directory
        add     esi, ebx                ; (esi) = ptr to Page Directory cached entry

        mov     ecx, dword ptr [eax+8]  ; (ecx) = size of the region
        
        add     ecx, 0003fffffh         ; round up to multiple of 4M
        and     ecx, 0ffc00000h         ; 
        ; DEBUGCHK (ecx != 0)

        or      edx, dword ptr [eax+4]  ; (edx) = PA | Capability BITs
        or      edx, PAGE_ATTRIB_RW             ; (edx) = PDE entry for 1st 4M (cached)

Next4M:
        mov     dword ptr [esi], edx    ; setup one cached entry
        add     esi, 4                  ; (esi) = ptr to next PDE (cached)
        add     edx, 0400000h           ; (edx) = PDE entry value for next 4M (cached)
        sub     ecx, 0400000h           ; decrement size by 4M
        jg      short Next4M

        popad

        ret
FillPD4M ENDP


;       Fill a secondary page table to Physical pointed by ebx
;       (ebx) = PA to map to
;       (edx & ~0xfff) = PA of Page table (1 pages)
;               NOTE: Low bit of edx might be set, MUST clear it before using it
;
;       !!! ALL REGITSTERS PRESERVED !!!
;
FillPTE PROc NEAR
        pushad

        mov     eax, ebx
        or      eax, PAGE_ATTRIB_RW             ; (eax) = page entry value (cached)

        and     edx, 0fffff000h                 ; (edx) = Page table for cached mapping
                                                ; the low bits might contain gargage when passed in

        mov     ecx, 1024                       ; (ecx) = counter (1024 entries)

NextPTE:
        mov     dword ptr [edx], eax
        add     edx, 4
        add     eax, PAGE_SIZE
        dec     ecx
        jne     short NextPTE
        
        popad
        ret
FillPTE ENDP


;
; Fill PageDirectory/PageTable of an OEMAddressTable entry using 4K pages
; On Entry
; (eax) = PA of an OEMAddressTable Entry
; (ebx) = PA of Page Directory
; (edx) = PageTables enough to map this region
;
; !!! All Reigsters preserved !!!
;
FillPD4K PROC NEAR
        pushad

        mov     ecx, dword ptr [eax+8]  ; (ecx) = size of the region

        add     ecx, 0003fffffh         ; round up to multiple of 4M
        and     ecx, 0ffc00000h         ; 
        ; DEBUGCHK (ecx != 0)

        mov     esi, dword ptr [eax]    ; (esi) = VA of region
        shr     esi, 20                 ; (esi) = offset into the Page Directory
        add     esi, ebx                ; (esi) = ptr to Page Directory cached entry

        mov     ebx, dword ptr [eax+4]  ; (ebx) = PA of the region

        or      edx, PAGE_ATTRIB_RW     ; (edx) = PDE entry for 1st 4M (cached)

Next4MPT:
        ; arguments to FillPTE:
        ;       (edx & ~0xfff) == Page Table address for 2 consective pages
        ;       (ebx) == PA to map to
        call    FillPTE                 ; fill the page tables
                                        ; NOTE: FillPTE perserves all registers 

        mov     dword ptr [esi], edx    ; setup one cached entry
        add     esi, 4                  ; (esi) = ptr to next PDE (cached)
        add     ebx, 0400000h           ; (ebx) = PA of the next 4M
        add     edx, PAGE_SIZE          ; (edx) = PDE entry value for next 4M (cached)
        sub     ecx, 0400000h           ; decrement size by 4M
        jg      short Next4MPT

        popad
        ret
FillPD4K ENDP

;-------------------------------------------------------------------------------
; ZeroData: function to zero the KData area
;
; at entrance:  (eax) = address of data to be zero'd
;               (ecx) = # of DWORDs to be zero'd
; on exit: Data pointed to by eax zero'd.
;
ZeroData PROC NEAR
        push    edi                     ; save edi

        cld
        mov     edi, eax                ; (edi) = destination
        xor     eax, eax                ; eax = 0
        rep     stosd                   ; clear the page
        
        pop     edi                     ; restore edi
        ret
ZeroData ENDP

        public      _KernelStart

;-------------------------------------------------------------------------------
; (esi) = PA of OEMAddressTable
;       o each entry is of the format ( VA, PA, cbSize )
;       o cbSize must be multiple of 4M
;       o last entry must be (0, 0, 0)
;       o must have at least one non-zero entry
; (edi) = CPU capability bits
;
; throughout this function
; (esi) = PA of OEMAddressTable
;-------------------------------------------------------------------------------
_KernelInitialize PROC NEAR PUBLIC

_KernelStart LABEL PROC
;        xor     edi, edi               ; uncomment this line to force 4K page, useful for debugging
        cli

        mov     ebx, esi                ; (ebx) = PA of OEMAddressTable
        
        ; check if it's using new table format, skip 1st entry if so
        cmp     dword ptr [esi], CE_NEW_MAPPING_TABLE
        jne     BootContinue
        add     esi, 12
        
BootContinue:
        ; (ebx) = physical address of &OEMAddressTable[0]
        ; (esi) = physical address of &OEMAddressTable[0] iff old table format
        ;         physical address of &OEMAddressTabel[1] iff new table format
        ;

        ; get the address of KData
        mov     eax, OFFSET32 _KData
        call    PaFromVa

        mov     ebp, eax
        ; (ebp) = PA of KData
        
        ; setup esp to use KStack (physical here)
        mov     esp, ebp
        sub     esp, 4                          ; esp = &KData - 4 == &KSTack

        ; zero KData
        ; (eax) = PA of KData
        mov     ecx, 0200h                      ; sizeof (KData)/sizeof(DWORD)
        call    ZeroData

        ; get viretual address of OEMAddressTable
        mov     eax, ebx
        call    VaFromPa                        ; (eax) = VA of OEMAddressTable

        ; Update KData.nMemForPT, KData.dwCpuCap
        ; (ebp) = PA of KData
        mov     dword ptr [ebp+OFFSET_ADDRMAP], eax     ; KData.pAddrMap = OEMAddressTable
        mov     dword ptr [ebp+OFFSET_MEM_FOR_PT], SYSTEM_PAGES; MemForPT = memeory used for system pages
        mov     dword ptr [ebp+OFFSET_CPU_CAP], edi  ; save CPU capability bits

        ; get pTOC
        mov     eax, OFFSET32 _pTOC             ; (eax) = VA of &pTOC
        call    PaFromVa                        ; (eax) = PA of &pTOC

        mov     eax, dword ptr [eax]            ; (eax) = VA of pTOC
        call    PaFromVa                        ; (eax) = PA of pTOC

        mov     eax, dword ptr [eax+OFFSET_ULRAMFREE] ; (eax) = VA free RAM
        call    PaFromVa                        ; (eax) = PA of free RAM

        mov     ebx, eax                        ; (ebx) = PA of free RAM = page directory
        
        ; zero the 1st pages of free ram, used as page directory
        mov     ecx, ZERO_BLOCK_SIZE / 4
        call    ZeroData

        ; (ebx) = PA of pTOC->ulRamFree == Ptr to Page Directory
        ; (esi) = PA of OEMAddressTable copy
        ; (ebp) = PA of KData

        mov     edx, edi                        ; (edx) = cpu capability bits

        ; (edx) = CPU capaibility
        test    edx, CPUID_PSE
        jnz     short Use4MPage

        ;
        ; no 4M page support, use 4k pages
        ;
        
        ; calculate size for Page Tables
        mov     ecx, esi                        ; (ecx) = OEMPageTable
        xor     edi, edi                        ; size init to 0

cntNext:
        add     edi, [ecx+8]                    ; (edi) = accumulated size

        ; round up to multiple of 4M
        add     edi, 0003fffffh                 ; round up to multiple of 4M
        and     edi, 0ffc00000h                 ; 
        
        add     ecx, 12                         ; (ecx) = next OEMPageTable Entry
        cmp     dword ptr [ecx], 0
        jne     short cntNext
        
        ; (edi) = total memory sizes

        shr     edi, 10                         ; (edi) = size of page tables
                                                ;       = (size / 4M) * PAGE_SIZE

        ; update KData.nMemForPT
        add     dword ptr [ebp+OFFSET_MEM_FOR_PT], edi ; update KData.nMemForPT

        mov     edx, ebx                        ; (edx) = PA of free RAM
        add     edx, SYSTEM_PAGES               ; skip the system pages
        mov     eax, esi                        ; (eax) = OEMPageTable
NextTable4K:
        Call    FillPD4K                        ; fill the page talbe for one entry
        mov     ecx, dword ptr [eax+8]          ; (ecx) = size of the entry
        shr     ecx, 10                         ; (ecx) = size of page tables for this entry
                                                ;       = (size / 4M) * PAGE_SIZE
        add     edx, ecx                        ; (edx) = next page table entry
        add     eax, 12                         ; (eax) = next OEMPageTableEntry
        cmp     dword ptr [eax], 0
        jne     short NextTable4K

        jmp     short DoneSettingPD


Use4MPage:
        ;
        ; with 4M page support, use 4M page
        ; (edx) = CPU capability bits

        ; figure out PDE bits
        mov     eax, edx                        ; (eax) = cpu capability bits
        mov     edx, PDE_PS                     ; (edx) = support 4M page bit for PDE
        test    eax, CPUID_PGE                  ; support global page?
        jz      short SetupPDE4M
        or      edx, PDE_G

SetupPDE4M:
        ; setup page directory
        mov     eax, esi
NextTable4M:
        Call    FillPD4M
        add     eax, 12                         ; (eax) = next OEMPageTableEntry
        cmp     dword ptr [eax], 0
        jne     short NextTable4M


.586p

CR4_PSE         EQU     010H
CR4_PGE         EQU     080H

        mov     ecx, CR4_PSE                    ; 4M page enabled
        test    edx, PDE_G                      ; support global page?
        jz      short SetupCr4
        
        ; yes, support Global Page
        or      ecx, CR4_PGE

SetupCr4:
        ; update CR4
        mov     eax, cr4
        or      eax, ecx
        mov     cr4, eax
.486p

DoneSettingPD:

        cld
        
        ; Setup value in  Syscall page
        mov     edi, ebx                        ;
        add     edi, SYSPAGE_SYSCALL            ; (edi) = PA of Syscall page
        mov     eax, 20cd20cdh                  ; (eax) = int 0x20 x 2
        mov     ecx, 1024                       ; fill syscall page with int 0x20 instructions
        rep     stosd

        ; Setup value in  SyscallDir
        mov     edi, ebx                        ;
        add     edi, SYSPAGE_SYSCALLDIR         ; (edi) = PA of Syscall page directory
        mov     edx, edi                        ; (edx) = PA of Syscall page directory
        mov     eax, ebx                        
        add     eax, SYSPAGE_SYSCALL+PAGE_ATTRIB_RO_USER  ; (eax) = Page directory entry of Syscall page directory
        mov     ecx, 1024                       ; every entry in SyscallDir point to to same Page (syscall page)
        rep     stosd

        ; setup page directory for syscall dir
        add     edx, PAGE_ATTRIB_RO_USER        ; (edx) = page directory entry for syscall directory
        mov     DWORD PTR [ebx][0FFCh], edx     ; setup global page directory
        
        ;
        ; Create the identity-mapped addresses (physical = linear), because
        ; Pentium-II requires that once paging is enabled, the next instruction
        ; must come from the same address as if paging wasn't enabled.
        ;       
        ; First, the mapping for the eip register.
        ;
        mov     eax, OFFSET32 PagingEnabled
        call    PaFromVa                
        mov     edi, eax                        ; (edi)  = PA of PageEnabled

        ; Next, the mapping for the gdt.
        sub     esp, 8
        sgdt    [esp]
        mov     eax, dword ptr 2[esp]
        add     esp, 8

        ; (eax) = PA of gdtBase                 ; save PA of gdtbase
        push    eax
        call    VaFromPa                        ; (eax) = VA of gdtbase

        mov     ecx, OFFSET32 PagingEnabled     ; (ecx) = VA of PageEnabled
        mov     edx, edi                        ; (edx) = PA of PageEnabled

        pop     edi                             ; (edi) = PA of gdtbase
        
        ;
        ; !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        ; !!! from here on, esi no longer points to OEMAddressTable
        ; !!! ANY CALL after this TO PaFromVa / VaFromPa will FAIL
        ; !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        ;
        mov     esi, eax                        ; (esi) = VA of gdtbase

        ;
        ; ECX = current linear addr, EDX = current physical addr.
        ;
        shr     ecx, 22                 ; Which 4MB?
        shl     ecx, 2                  ; DWORD index
        shr     edx, 22                 ; Which 4MB?
        shl     edx, 2                  ; DWORD index

        ;
        ; ESI = linear gdtbase, EDI = physical gdtbase.
        ;
        shr     esi, 22                 ; Which 4MB?
        shl     esi, 2                  ; DWORD index
        shr     edi, 22                 ; Which 4MB?
        shl     edi, 2                  ; DWORD index

        ;
        ; Save off the original page directory entry for the physical address.
        ;
        push    DWORD PTR [ebx][edx]
        push    DWORD PTR [ebx][edi]
        ;
        ; Copy the page directory entry for the current linear 
        ; address to the physical address.
        ;
        mov     eax, DWORD PTR [ebx][ecx]
        and     eax, not PDE_G          ; only works if PDE_G implies PDE_PS
        mov     DWORD PTR [ebx][edx], eax
        mov     eax, DWORD PTR [ebx][esi]
        and     eax, not PDE_G          ; only works if PDE_G implies PDE_PS
        mov     DWORD PTR [ebx][edi], eax
        ;
        ; Get the saved values off the stack.
        ;
        mov     cr3, ebx
        pop     ebx                     ; Original pde value for gdtbase
        pop     ebp                     ; Original pde value for PagingEnabled
        ;
        ; We are now identity-mapped, okay to enable paging.
        ;
        mov     esi, OFFSET32 PagingEnabled
        mov     eax, cr0
        or      eax, PG_MASK OR WP_MASK ; enable paging & write-protection for Ring0
        and     eax, not 060000000h     ; clear cache disable & cache write-thru bits
        align   16
        mov     cr0, eax
        jmp     esi
        align   4
PagingEnabled:
        wbinvd                          ; clear out the cache
        ;
        ; Switch to virtual addressing for the stack and save of all these registers.
        ;
        mov     esp, OFFSET32 _KStack
        
        push    edx     ; eip pde index
        push    ebp     ; eip pde value
        push    edi     ; gdt pde index
        push    ebx     ; gdt pde value

        push    0
        popfd

        push    OFFSET32 _KData                 ; argument to x86Init: ptr to KData
        call    _x86Init                        ; Load new gdtr, among other things.
        pop     ecx                             ; pop off the argument
        ; return value is the entry point to kernel.dll
        mov     esi, eax                        ; (esi) = entry point of kernel.dll

        mov     eax, dword ptr [_pTOC]          ; (eax) = pTOC
        mov     ebx, [eax+OFFSET_ULRAMFREE]     ; (ebx) = pTOC->ulRamFree == page directory

        ;
        ; Restore the original page directory entry for the physical addresses.
        ; We cannot restore these PDEs until _SystemInitialization has reloaded
        ; gdtr.
        ;
        pop     eax                             ; gdt pde value
        pop     edx                             ; gdt pde index
        mov     DWORD PTR [ebx][edx], eax       ; Restore GDT phys addr mapping
        pop     eax                             ; PagingEnabled pde value
        pop     edx                             ; PagingEnalbled pde index
        mov     DWORD PTR [ebx][edx], eax       ; Restore PaginEnabled phys addr mapping
        ;
        ; Flush the tlb, since we are unmapping.
        ;
        mov     eax, cr3                        ; Flush the TLB
        mov     cr3, eax

        ; Jump to Entry point of kernel.dll
        push    OFFSET32 _KData                 ; &KData == argument to kernel entry funciton
        call    esi

        ; should never return
        cli
        hlt
        
_KernelInitialize ENDP


_TEXT ENDS 

        END
        


;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
;
;-------------------------------------------------------------------------------
;
;
;-------------------------------------------------------------------------------
.486p


PAGE_SIZE               EQU 1000h
PAGE_ATTRIB_RW          EQU 000001100011b
PAGE_ATTRIB_RW_NOCACHE  EQU 000001111011b
PAGE_ATTRIB_RO_USER     EQU 000000100101b
PAGE_ATTRIB_RW_USER     EQU 000001100111b
PDE_PS                  EQU 080H
PDE_G                   EQU 100h

CPUID_PSE               EQU   0008H
CPUID_PGE               EQU   2000H

SYSTEM_PAGES            EQU 4000h               ; 4 system pages:

; offset within SYSTEM_PAGES
SYSPAGE_PAGEDIR         EQU 0                   ; Page Directory:       1 page
SYSPAGE_SHADOW          EQU 1000h               ; Shadow Page Dir:      1 page
SYSPAGE_SYSCALL         EQU 2000h               ; Syscall Page:         1 page
SYSPAGE_SYSCALLDIR      EQU 3000h               ; Syscall Page Dir:     1 page

ZERO_BLOCK_SIZE         EQU 2000h               ; page needs to be zero'd (page directory and shadwo)

KGDT_R0_CODE            EQU 8h
KGDT_R0_DATA            EQU 10h

PG_MASK                 EQU 80000000h
WP_MASK                 EQU 00010000h

LIN_TO_PHYS_OFFSET      EQU 80000000h

OFFSET32                EQU <OFFSET FLAT:>

OFFSET_MEM_FOR_PT       EQU 02a8h               ; offset into _KDATA for nMemForPT field
OFFSET_CPU_CAP          EQU 02ach               ; offset into _KDATA for dwCpuCap field
                                                ;        -- must match nkx86.h
                                                
OFFSET_ULRAMFREE        EQU 24                  ; offset into pTOC for ulRamFree field

EFLAGS_IF_BIT           EQU 9                   ; bit 9 of EFLAGS is IF bit

;-------------------------------------------------------------------------------
; Kernel Data Segment ; KStack, Page Directory, KPage, Page Table pool
;-------------------------------------------------------------------------------
.KDATA segment para public 'STACK'

        public _KDBase
        public _KStack
        public _KData
        public _CurMSec, _DiffMSec
        
                        db 1000h dup (?)        ; reserve an extra page for the debugger
_KDBase                 db 07fch dup (?)        ; 2k Kernel Stack
_KStack                 dd ?
_KData                  db 0088h dup (?)        ; 2k KData
_CurMSec                dd ?
_DiffMSec               dd ?
                        db (800h - ($ - _KData)) dup (?)
.KDATA ends


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
_TEXT segment para public 'TEXT'

extrn _SystemInitialization:near
extrn _KernelInit:near
extrn _Reschedule:near
extrn _SafeIdentifyCpu:near
extrn _pTOC:near

        public  _NullSection
        align   4
_NullSection dd 512 dup (0)

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

        mov     edi, esi
        add     edi, 0200h              ; (edi) = ptr to Page Directory uncached entry

        mov     ecx, dword ptr [eax+8]  ; (ecx) = size of the region
        
        add     ecx, 0003fffffh         ; round up to multiple of 4M
        and     ecx, 0ffc00000h         ; 
        ; DEBUGCHK (ecx != 0)

        or      edx, dword ptr [eax+4]  ; (edx) = PA | Capability BITs
        mov     eax, edx

        or      edx, PAGE_ATTRIB_RW             ; (edx) = PDE entry for 1st 4M (cached)
        or      eax, PAGE_ATTRIB_RW_NOCACHE     ; (eax) = PDE entry value for 1st 4M (uncached)

Next4M:
        mov     dword ptr [esi], edx    ; setup one cached entry
        mov     dword ptr [edi], eax    ; setup one uncached entry

        add     esi, 4                  ; (esi) = ptr to next PDE (cached)
        add     edi, 4                  ; (edi) = ptr to next PDE (uncached)

        add     edx, 0400000h           ; (edx) = PDE entry value for next 4M (cached)
        add     eax, 0400000h           ; (eax) = PDE entry value for next 4M (uncached)
        
        sub     ecx, 0400000h           ; decrement size by 4M
        jg      short Next4M

        popad

        ret
FillPD4M ENDP


;       Fill a secondary page table (2 actually, cached and uncached) + the page directory entry
;       (ebx) = PA to map to
;       (edx & ~0xfff) = PA of Page table (2 consective pages, one for cached, one for uncached)
;               NOTE: Low bit of edx might be set, MUST clear it before using it
;
;       !!! ALL REGITSTERS PRESERVED !!!
;
FillPTE PROc NEAR
        pushad

        mov     eax, ebx
        or      eax, PAGE_ATTRIB_RW             ; (eax) = page entry value (cached)
        or      ebx, PAGE_ATTRIB_RW_NOCACHE     ; (ebx) = page entry value (uncached)

        and     edx, 0fffff000h                 ; (edx) = Page table for cached mapping
                                                ; the low bits might contain gargage when passed in
        mov     esi, edx                        ; (esi) = Page table
        add     esi, PAGE_SIZE                  ; (esi) = Page Table for uncached mapping

        mov     ecx, 1024                       ; (ecx) = counter (1024 entries)

NextPTE:
        mov     dword ptr [edx], eax
        mov     dword ptr [esi], ebx

        add     edx, 4
        add     esi, 4

        add     eax, PAGE_SIZE
        add     ebx, PAGE_SIZE

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

        mov     edi, esi
        add     edi, 0200h              ; (edi) = ptr to Page Directory uncached entry

        mov     ebx, dword ptr [eax+4]  ; (ebx) = PA of the region

        or      edx, PAGE_ATTRIB_RW     ; (edx) = PDE entry for 1st 4M (cached)
        mov     eax, edx
        add     eax, PAGE_SIZE          ; (edx, eax) are PT for (cached, uncached) addresses

Next4MPT:
        ; arguments to FillPTE:
        ;       (edx & ~0xfff) == Page Table address for 2 consective pages
        ;       (ebx) == PA to map to
        call    FillPTE                 ; fill the page tables
                                        ; NOTE: FillPTE perserves all registers 

        mov     dword ptr [esi], edx    ; setup one cached entry
        mov     dword ptr [edi], eax    ; setup one uncached entry

        add     esi, 4                  ; (esi) = ptr to next PDE (cached)
        add     edi, 4                  ; (edi) = ptr to next PDE (uncached)

        add     ebx, 0400000h           ; (ebx) = PA of the next 4M
        
        add     edx, PAGE_SIZE*2        ; (edx) = PDE entry value for next 4M (cached)
        add     eax, PAGe_SIZE*2        ; (eax) = PDE entry value for next 4M (uncached)
        
        sub     ecx, 0400000h           ; decrement size by 4M
        jg      short Next4MPT

        popad
        ret
FillPD4K ENDP


;-------------------------------------------------------------------------------
; (esi) = PA of OEMAddressTable
;       o each entry is of the format ( VA, PA, cbSize )
;       o cbSize must be multiple of 4M
;       o last entry must be (0, 0, 0)
;       o must have at least one non-zero entry
; (edi) = CPU capability bits
;
; throughout this function
; (ebx) = PA of Page Directory
; (esi) = PA of OEMAddressTable
;-------------------------------------------------------------------------------
_KernelInitialize PROC NEAR PUBLIC

;        xor     edi, edi               ; uncomment this line to force 4K page, useful for debugging

        ; set KData.nMemForPT to 0 first
        ; get the address of KData
        mov     eax, OFFSET32 _KData
        call    PaFromVa
        ; (eax) = PA of KData
        mov     dword ptr [eax+OFFSET_MEM_FOR_PT], SYSTEM_PAGES; MemForPT = memeory used for system pages
        mov     dword ptr [eax+OFFSET_CPU_CAP], edi  ; save CPU capability bits

        ; setup esp to use KStack (physical here)
        mov     esp, eax
        sub     esp, 4                          ; esp = &KData - 4 == &KSTack

        ; save the VA of OEMAddressTable onto stack
        mov     eax, esi
        call    VaFromPa                        ; (eax) = VA of OEMAddressTable upon return
        mov     dword ptr [esp], eax            ; save address of OEMAddressTable on stack
                                                ; NOTE: since push is pre-decrement, the conent
                                                ;       won't be over-written by "push" operation

        ; Figure out pTOC->ulRamFree
        mov     eax, OFFSET32 _pTOC             ; (eax) = VA of &pTOC
        call    PaFromVa                        ; (eax) = PA of &pTOC

        mov     eax, dword ptr [eax]            ; (eax) = VA of pTOC
        call    PaFromVa                        ; (eax) = PA of pTOC
        
        mov     eax, dword ptr [eax+OFFSET_ULRAMFREE] ; (eax) = VA free RAM
        push    eax                             ; save the VA of Page Directory onto stack
        call    PaFromVa                        ; (eax) = PA of free RAM

        ; (eax) = PA of the beginning of the free RAM
        ; 1st pages of Free RAM is going to be used as Page Directory
        mov     ebx, eax                        ; (ebx) = PA of Page Directory
        mov     edx, edi                        ; (edx) = cpu capability bits

        ; clear the system pages
        cld
        mov     ecx, ZERO_BLOCK_SIZE / 4
        mov     edi, ebx
        xor     eax, eax
        rep     stosd

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

        shr     edi, 9                          ; (edi) = size of page tables
                                                ;       = (size / 4M) * 2 * PAGE_SIZE

        ; update KData.nMemForPT
        mov     eax, OFFSET32 _KData            ; (eax) = VA of KData
        call    PaFromVa
        add     dword ptr [eax+OFFSET_MEM_FOR_PT], edi ; update KData.nMemForPT

        mov     edx, ebx                        ; (edx) = PA of free RAM
        add     edx, SYSTEM_PAGES               ; skip the system pages
        mov     eax, esi                        ; (eax) = OEMPageTable
NextTable4K:
        Call    FillPD4K                        ; fill the page talbe for one entry
        mov     ecx, dword ptr [eax+8]          ; (ecx) = size of the entry
        shr     ecx, 9                          ; (ecx) = size of page tables for this entry
                                                ;       = (size / 4M) * 2 * PAGE_SIZE
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

        ;
        ; the value for gdtBase is 0xffffffff!!!!
        ; not a physical address at all
        ;
      
        ;call    VaFromPa                        ; (eax) = VA of gdtbase
        sub     eax, LIN_TO_PHYS_OFFSET

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
        mov     eax, dword ptr [esp]    ; retrieve VA of OEMAddressTable
        mov     ecx, dword ptr [esp-4]  ; retrieve VA of Page Directory
        push    edx     ; eip pde index
        push    ebp     ; eip pde value
        push    edi     ; gdt pde index
        push    ebx     ; gdt pde value

        push    0
        popfd

        mov     ebx, ecx                ; (ebx) = VA of Page Directory
        push    ebx                     ; arg1 - VA of PageDir
        push    eax                     ; arg0 - VA of OEMAddressTable
        call    _SystemInitialization   ; Load new gdtr, among other things.
        add     sp, 8                   ; pop the arguments
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

        call    _SafeIdentifyCpu
        call    _KernelInit
        xor     edi, edi
        jmp     _Reschedule

_KernelInitialize ENDP


;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
_INTERRUPTS_ON proc NEAR PUBLIC
        sti
        ret
_INTERRUPTS_ON endp



;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
_INTERRUPTS_OFF proc NEAR PUBLIC
        cli
        ret
_INTERRUPTS_OFF endp

;-------------------------------------------------------------------------------
; INTERRUPTS_ENABLE - enable/disable interrupts based on arguemnt and return current status
;-------------------------------------------------------------------------------
_INTERRUPTS_ENABLE proc NEAR PUBLIC
        mov     ecx, [esp+4]            ; (ecx) = argument
        pushfd
        pop     eax                     ; (eax) = current flags
        shr     eax, EFLAGS_IF_BIT      ; 
        and     eax, 1                  ; (eax) = 0 if interrupt was disabled, 1 if enabled
        
        test    ecx, ecx                ; enable or disable?

        jne     short _INTERRUPTS_ON

        ; disable interrupt
        cli
        ret
_INTERRUPTS_ENABLE endp


_TEXT ENDS 

        END
        


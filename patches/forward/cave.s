@ SPDX-License-Identifier: GPL-3.0-or-later
@ Runs first when a patched game opens (the game's first instruction, "bl 0x100024", calls this).
@ Most opens: compares the clock with "next check" and carries on. When a check is due it asks APT to
@ open the app that keeps this game's patches (BadgeCade helper or ScribbleLit), which checks for
@ updates and opens the game again. Position independent; the app fills the last 16 bytes.
    .arm
    .text
    .global _start
_start:
    push {r0-r12, lr}
    mov r0, #0x1F000000
    orr r0, r0, #0x00F80000
    orr r0, r0, #0x00001000      @ shared page, the clock the system keeps
    ldr r1, [r0, #4]
    tst r1, #1
    addeq r0, r0, #0x20
    addne r0, r0, #0x40
    ldrd r2, [r0]                @ ms since 1900
    lsr r2, r2, #22
    orr r2, r2, r3, lsl #10      @ in units of about 70 minutes
    ldr r9, table_at
    ldr r3, [r9, #NEXT]
    subs r2, r2, r3
    bmi clock_early
    cmp r2, #0x700               @ months unopened: the app may be gone, so don't count on it
    bcs resume
    b due
clock_early:
    ldr r3, [r9, #SPAN]          @ more than a check's wait early: the clock was moved back
    adds r2, r2, r3
    bpl resume
due:
    mrc p15, 0, r4, c13, c0, 3
    add r4, r4, #0x80
    adr r1, port
    svc 0x2D                     @ ConnectToPort "srv:"
    cmp r0, #0
    bne resume
    mov r6, r1
    bl send                      @ RegisterClient
    bl send                      @ GetServiceHandle "APT:U"
    ldr r0, [r4, #4]
    cmp r0, #0
    blt resume                   @ no APT: just play
    ldr r6, [r4, #12]
    bl send                      @ GetLockHandle
    bl send                      @ Initialize
    ldr r7, [r4, #16]
    bl send                      @ Enable
    bl send                      @ NotifyToWait
    mov r0, r7
    ldr r2, wait_ns
    mov r3, #0
    svc 0x24                     @ wait for the HOME Menu's wake-up
    mov r0, #2
    str r0, [r4, #0x100]         @ an empty buffer for it
    str r0, [r4, #0x104]
    bl send                      @ ReceiveParameter
    bl send                      @ PrepareToDoApplicationJump
    bl send                      @ DoApplicationJump
    svc 0x03                     @ ExitProcess, the system opens the app
resume:
    pop {r0-r12, lr}
    ldr pc, game_start

@ one call from the table: its size comes from the IPC header
send:
    ldr r2, [r9]
    and r3, r2, #0x3F
    lsr r2, r2, #6
    and r2, r2, #0x3F
    add r3, r3, r2
    add r3, r3, #1
    mov r0, r4
1:  ldr r2, [r9], #4
    str r2, [r0], #4
    subs r3, r3, #1
    bne 1b
    mov r0, r6
    svc 0x32
    bx lr

port: .asciz "srv:"
    .align 2
game_start: .word 0x00100024
wait_ns: .word 3000000000
table_at: .word 0x55555555
code_end:
    .equ NEXT, next_check - table
    .equ SPAN, span - table
table:
    .word 0x00010002, 0x20, 0
    .word 0x00050100, 0x3A545041, 0x00000055, 5, 0   @ "APT:U"
    .word 0x00010040, 0
    .word 0x00020080, 0x300, 0
    .word 0x00030040, 0
    .word 0x00430040, 0x300
    .word 0x000D0080, 0x300, 0
    .word 0x00310100, 0
tid_lo: .word 0x11111111
tid_hi: .word 0x22222222
    .word 1                       @ on the SD card
    .word 0x00320084, 0x300, 0x20, 0xC0002
param_at: .word 0x66666666        @ what the app receives: the HOME Menu passes it on
    .word 0x80802, 0x100000
next_check: .word 0x33333333
span: .word 0x44444444
magic: .ascii "BCFW"
game_lo: .word 0x77777777
game_hi: .word 0x88888888

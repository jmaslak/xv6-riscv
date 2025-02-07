        #
        # interrupts and exceptions while in supervisor
        # mode come here.
        #
        # the current stack is a kernel stack.
        # push registers, call kerneltrap().
        # when kerneltrap() returns, restore registers, return.
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        # make room to save registers.
        addi sp, sp, -256

        # save caller-saved registers.
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        # call the C trap handler in trap.c
        call kerneltrap

        # restore registers.
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        # not tp (contains hartid), in case we moved CPUs
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        addi sp, sp, 256

        # return to whatever we were doing in the kernel.
        sret

.globl machine_eom_vec
.globl eom_marker
.align 4
# For this handler, we assume the ONLY possible exception is an invalid memory
# access instruction.  If it is anything else, we'll have issues!
#
# For the emulator, this is probably fine, but we would probably want to
# check what exception was actually thrown and what instruction caused it
# to be thrown on a real machine, and to panic if it doesn't meet our
# expectations.
#
# We also assume the instruction that throws this exception is 4 bytes (32
# bits) long.
machine_eom_vec:
        # Save registers on stack.
        addi sp, sp, -16

        # save registers.
        sd a5, 0(sp)
        sd a6, 8(sp)

        # call the C trap handler in kalloc.c to set the eom_marker
        # call set_eom_marker
        # jal ra, set_eom_marker
        la a5, eom_marker
        li a6, 1
        sd a6, 0(a5)

        csrr a5, mepc   # Get the exception instruction address
        addi a5, a5, 4  # Move to next instruction (it better be a 32 bit wide instruction!)
        csrw mepc, a5   # Write back the new address to the exception PC

        # restore registers.
        ld a5, 0(sp)
        ld a6, 8(sp)

        addi sp, sp, 16

        # return from exception
        mret

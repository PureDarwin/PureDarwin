.set noreorder
# Some macros for n32 / n64 compatibility
#ifdef _ABI64
#define LP             ld
#define SP             sd
#define DTABLE_OFFSET  64
#define SMALLOBJ_MASK  7
#define SHIFT_OFFSET   4
#define DATA_OFFSET    16
#define SLOT_OFFSET    32
#else
#warning N32 is untested, O32 is unsupported.
#define LP             lw
#define SP             sw
#define DTABLE_OFFSET  32
#define SMALLOBJ_MASK  1
#define SHIFT_OFFSET   4
#define DATA_OFFSET    12
#define SLOT_OFFSET    16
#endif

.macro dump_and_crash reg
nop
move   $a0, \reg
ld     $25, %got_disp(logInt)($t8) 
jalr   $25
nop
lw     $zero, ($zero)
.endm

// FIXME: CHERI needs (or, at least, strongly encourages) 32-byte aligned
// stacks.  
#ifndef __mips_soft_float
#define SAVE_SIZE      136
#else
#define SAVE_SIZE      72
#endif

.macro MSGSEND receiver, sel
0:
	.cfi_startproc                          # Start emitting unwind data.  We
	                                        # don't actually care about any of
	                                        # the stuff except the slow call,
	                                        # because that's the only one that
	                                        # can throw.
	beq    \receiver, $0, 4f                # If the receiver is nil, return nil
	nop

	lui    $t8, %hi(%neg(%gp_rel(0b)))      # Load the GOT address that we use for relocations into $t8
	daddu  $t8, $t8, $t9
	daddiu $t8, $t8, %lo(%neg(%gp_rel(0b)))


	andi   $t4, \receiver, SMALLOBJ_MASK    # Check if the receiver is a small object
	bne    $t4, $0, 6f                      # Get the small object class
	nop

	LP     $t5, (\sel)

	                                        # By this point, we have a non-nil
	                                        # receiver that is a real pointer
	LP     $t4, (\receiver)                 # Load the class

1:                                          # class loaded, stored in $t4
	LP     $t4, DTABLE_OFFSET($t4)          # Load the dtable from the class
	lw     $t6, SHIFT_OFFSET($t4)           # Load the shift (dtable size)
                                            # $t4 = dtable, $t5 = sel index
	LP     $t7, DATA_OFFSET($t4)            # Load the address of the start of the array


	beq    $0, $t6, 3f                      # If this is a small dtable, jump to the small dtable handlers
	daddi  $v0, $t6, -8

	beq    $0, $v0, 2f
	lui    $t6, 0x00ff                      # The mask for a big dtable won't fit in an and immediate
	and    $t6, $t6, $t5                    # mask the selector
#ifdef _ABI64         
	dsrl   $t6, $t6, 13                     # Right shift 16, but then left shift by pointer size
#else
	srl    $t6, $t6, 14
#endif
	dadd   $t6, $t6, $t7
	LP     $t7, ($t6)
	LP     $t7, DATA_OFFSET($t7)
2:                                          # dtable16:
	andi   $t6, $t5, 0xff00                 # mask the selector
#ifdef _ABI64        
	dsrl   $t6, $t6, 5                      # Right shift 8, but then left shift by pointer size
#else
	srl    $t6, $t6, 6
#endif
	dadd   $t6, $t6, $t7
	LP     $t7, ($t6)
	LP     $t7, DATA_OFFSET($t7)
3:                                          # dtable8:
	andi   $t6, $t5, 0xff                   # mask the selector
#ifdef _ABI64         
	dsll   $t6, $t6, 3                      # Left shift by pointer size
#else
	sll    $t6, $t6, 2
#endif
	dadd   $t6, $t6, $t7
	LP     $t7, ($t6)

	
	beq    $0, $t7, 5f                      # Nil slot - invoke some kind of forwarding mechanism
	nop

	LP     $25, SLOT_OFFSET($t7)
	jr     $25
	nop
4:                                          # returnNil:
	                                        # All of the return registers are
	                                        # callee-save, so we can
	                                        # return 0 in both in the same code:
#ifndef __mips_soft_float
	dmtc1  $0, $f0                          # Return 0 as a floating point value (only if we're not a soft-float target)
	dmtc1  $0, $f2
#endif
	daddi  $v0, $0, 0                        # Return 0 as an integer
	jr     $ra
	daddi  $v1, $0, 0
	
5:                                          # slowSend:
	# Load the address of the slow lookup function now, so that we don't get
	# pipeline stalls on the jump.  This is more important on CHERI than proper
	# MIPS implementations.
	# Note: A better linker ought to be able to turn this into a single
	# jump-immediate, so revisit this decision later...
	LP     $25, %got_disp(CDECL(slowMsgLookup))($t8) 

	daddiu $sp, $sp, -SAVE_SIZE             # We need to preserve all registers that may contain arguments:

	SP     $a0, ($sp)
	SP     $a1, 8($sp)
	SP     $a2, 16($sp)
	SP     $a3, 24($sp)
	SP     $a4, 32($sp)
	SP     $a5, 40($sp)
	SP     $a6, 48($sp)
	SP     $a7, 56($sp)
	SP     $ra, 64($sp)
#ifndef __mips_soft_float
	sdc1   $f12, 72($sp)
	sdc1   $f13, 80($sp)
	sdc1   $f14, 88($sp)
	sdc1   $f15, 96($sp)
	sdc1   $f16, 104($sp)
	sdc1   $f17, 112($sp)
	sdc1   $f18, 120($sp)
	sdc1   $f19, 128($sp)
#endif     

	# We're (potentially) modifying the self argument with the lookup.  Use the
	# address of the stack save slot for the address so that when we reload it
	# we get the old or new version automatically.  Note that we must reload it
	# anyway, because argument registers are not guaranteed to be preserved
	# across calls.
.ifc "\receiver", "$a0"
	daddiu $a0, $sp, 0                      # replace self with &self in $a0
.else
	daddiu $a0, $sp, 8                      # replace sret pointer with &self in $a0
	daddiu $a1, $a2, 0                      # replace self with _cmd in $a1
.endif

	.cfi_adjust_cfa_offset -SAVE_SIZE
	jalr   $25                              # Call the slow lookup function
	nop

	move   $25, $v0                         # Move the return value to $25 for use with the call

	LP     $a0, ($sp)                       # Restore all of the arguments.  Note
	LP     $a1, 8($sp)                      # that the receiver may have been
	LP     $a2, 16($sp)                     # modified during the call
	LP     $a3, 24($sp)
	LP     $a4, 32($sp)
	LP     $a5, 40($sp)
	LP     $a6, 48($sp)
	LP     $a7, 56($sp)
	LP     $ra, 64($sp)
#ifndef __mips_soft_float
	ldc1   $f12, 72($sp)
	ldc1   $f13, 80($sp)
	ldc1   $f14, 88($sp)
	ldc1   $f15, 96($sp)
	ldc1   $f16, 104($sp)
	ldc1   $f17, 112($sp)
	ldc1   $f18, 120($sp)
	ldc1   $f19, 128($sp)
#endif     
	jr     $25
	daddiu $sp, $sp, SAVE_SIZE                 
6:                                          # smallObject:
#if _ABI64
	dsll   $t4, $t4, 3                      # Convert tag to pointer offset
	LP     $t6, %got_disp(CDECL(SmallObjectClasses))($t8) # Load small object classes array address
	daddu  $t4, $t4, $t6                    # Add the base address to the offset
	b      1b                               # Return to the normal path
	LP     $t4, ($t4)                       # Load the class (in delay slot)
#else
	b      1b
	LP     $t4, %got_disp(CDECL(SmallIntClass))($t8)
#endif
	.cfi_endproc
.endm
.globl CDECL(objc_msgSend)
TYPE_DIRECTIVE(CDECL(objc_msgSend), @function)
.globl CDECL(objc_msgSend_fpret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_fpret), @function)
CDECL(objc_msgSend_fpret):
CDECL(objc_msgSend):
	MSGSEND $a0, $a1
.globl CDECL(objc_msgSend_stret)
TYPE_DIRECTIVE(CDECL(objc_msgSend_stret), @function)
CDECL(objc_msgSend_stret):
	MSGSEND $a1, $a2

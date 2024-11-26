@ setSize.s

/* REGISTERS
   r0 : Sprite pointer
   r1 : Shape bits
   r2 : Size bits
   r3 : Attribute
   r4 : Mask
   r5 : Shift amount
   */

.global setSize
setSize:
    @ Creates a new stack frame
    sub sp, sp, #4
    str r4, [sp]

    @ Construct mask
    mov r4, #0
    orr r4, r4, #0xff
    mov r4, r4, lsl #4
    orr r4, r4, #0xf
    mov r4, r4, lsl #2
    orr r4, r4, #0x3

    @ Change shape bits
    ldrh r3, [r0]
    and r3, r3, r4
    mov r1, r1, lsl #14
    orr r3, r3, r1
    strh r3, [r0]

    @ Change size bits
    ldrh r3, [r0, #2]
    and r3, r3, r4
    mov r2, r2, lsl #14
    orr r3, r3, r2
    strh r3, [r0, #2]

    ldr r4, [sp]
    add sp, sp, #4
    mov pc, lr

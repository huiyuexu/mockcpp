   .text

   .p2align 2
   .global mockcpp_cross_so_short_target
   .type mockcpp_cross_so_short_target, %function
mockcpp_cross_so_short_target:
   add w0, w0, #1
   ret
   .size mockcpp_cross_so_short_target, .-mockcpp_cross_so_short_target

   // Keep this function immediately after the 8-byte target.  An unsafe
   // 16-byte replacement of the target would overwrite it completely.
   .global mockcpp_cross_so_short_neighbor
   .type mockcpp_cross_so_short_neighbor, %function
mockcpp_cross_so_short_neighbor:
   mov w0, #123
   ret
   .size mockcpp_cross_so_short_neighbor, .-mockcpp_cross_so_short_neighbor

   .p2align 2
   .global mockcpp_cross_so_bti_target
   .type mockcpp_cross_so_bti_target, %function
mockcpp_cross_so_bti_target:
   .inst 0xd503245f // bti c
   nop
   nop
   nop
   nop
   add w0, w0, #1
   ret
   .size mockcpp_cross_so_bti_target, .-mockcpp_cross_so_bti_target

   .p2align 2
   .global mockcpp_cross_so_pac_target
   .type mockcpp_cross_so_pac_target, %function
mockcpp_cross_so_pac_target:
   .inst 0xd503233f // paciasp
   add w0, w0, #1
   .inst 0xd50323bf // autiasp
   ret
   .size mockcpp_cross_so_pac_target, .-mockcpp_cross_so_pac_target

   .global mockcpp_cross_so_pac_neighbor
   .type mockcpp_cross_so_pac_neighbor, %function
mockcpp_cross_so_pac_neighbor:
   mov w0, #124
   ret
   .size mockcpp_cross_so_pac_neighbor, .-mockcpp_cross_so_pac_neighbor

   .section .note.GNU-stack,"",%progbits

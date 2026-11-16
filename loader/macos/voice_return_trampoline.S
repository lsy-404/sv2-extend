#if defined(__arm64__)
// Opaque C++ return forwarding: x8 remains the caller-owned indirect result slot.
// This veneer never copies, destroys or assumes a size for the returned value.
.text
.p2align 2
.globl _loader_voice_return_trampoline
.private_extern _loader_voice_return_trampoline
_loader_voice_return_trampoline:
.cfi_startproc
stp x29, x30, [sp, #-64]!
.cfi_def_cfa_offset 64
.cfi_offset x29, -64
.cfi_offset x30, -56
mov x29, sp
.cfi_def_cfa_register x29
stp x0, x1, [sp, #16]
str x8, [sp, #32]
adrp x16, _loader_voice_original_target@PAGE
ldr x16, [x16, _loader_voice_original_target@PAGEOFF]
blr x16
stp x0, x1, [sp, #48]
ldp x0, x1, [sp, #16]
bl _loader_voice_observe_after
ldp x0, x1, [sp, #48]
ldr x8, [sp, #32]
ldp x29, x30, [sp], #64
.cfi_def_cfa sp, 0
.cfi_restore x29
.cfi_restore x30
ret
.cfi_endproc
// Opaque C++ return forwarding: x8 remains the caller-owned indirect result slot.
// This veneer never copies, destroys or assumes a size for the returned value.
.text
.p2align 2
.globl _loader_export_return_trampoline
.private_extern _loader_export_return_trampoline
_loader_export_return_trampoline:
.cfi_startproc
stp x29, x30, [sp, #-64]!
.cfi_def_cfa_offset 64
.cfi_offset x29, -64
.cfi_offset x30, -56
mov x29, sp
.cfi_def_cfa_register x29
stp x0, x1, [sp, #16]
str x8, [sp, #32]
adrp x16, _loader_export_original_target@PAGE
ldr x16, [x16, _loader_export_original_target@PAGEOFF]
blr x16
stp x0, x1, [sp, #48]
ldp x0, x1, [sp, #16]
bl _loader_export_observe_after
ldp x0, x1, [sp, #48]
ldr x8, [sp, #32]
ldp x29, x30, [sp], #64
.cfi_def_cfa sp, 0
.cfi_restore x29
.cfi_restore x30
ret
.cfi_endproc
#endif

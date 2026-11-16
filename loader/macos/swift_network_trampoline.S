#if defined(__arm64__)
// Swift allocating init returns a retained class reference in x0. Preserve it
// while observing weakly; all constructor arguments reach the original intact.
.text
.p2align 2
.globl _loader_swift_group_init
_loader_swift_group_init:
.cfi_startproc
stp x29, x30, [sp, #-32]!
.cfi_def_cfa_offset 32
.cfi_offset x29, -32
.cfi_offset x30, -24
mov x29, sp
bl _$s7Network17NWConnectionGroupC4with5usingAcA17NWGroupDescriptor_p_AA12NWParametersCtcfC
str x0, [sp, #16]
bl _loader_swift_group_created
ldr x0, [sp, #16]
ldp x29, x30, [sp], #32
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_group_init
.quad _$s7Network17NWConnectionGroupC4with5usingAcA17NWGroupDescriptor_p_AA12NWParametersCtcfC

.text
.p2align 2
.globl _loader_swift_group_start
_loader_swift_group_start:
.cfi_startproc
stp x29, x30, [sp, #-48]!
.cfi_def_cfa_offset 48
.cfi_offset x29, -48
.cfi_offset x30, -40
mov x29, sp
stp x0, x8, [sp, #16]
mov x0, x20
bl _loader_swift_group_start_begin
cbz w0, 1f
ldp x0, x8, [sp, #16]
bl _$s7Network17NWConnectionGroupC5start5queueySo012OS_dispatch_E0C_tF
bl _loader_swift_network_start_end
ldp x29, x30, [sp], #48
ret
1:
ldp x29, x30, [sp], #48
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_group_start
.quad _$s7Network17NWConnectionGroupC5start5queueySo012OS_dispatch_E0C_tF
.text
.p2align 2
.globl _loader_swift_connection_init_endpoint
_loader_swift_connection_init_endpoint:
.cfi_startproc
stp x29, x30, [sp, #-32]!
.cfi_def_cfa_offset 32
.cfi_offset x29, -32
.cfi_offset x30, -24
mov x29, sp
bl _$s7Network12NWConnectionC2to5usingAcA10NWEndpointO_AA12NWParametersCtcfC
str x0, [sp, #16]
bl _loader_swift_connection_created
ldr x0, [sp, #16]
ldp x29, x30, [sp], #32
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_connection_init_endpoint
.quad _$s7Network12NWConnectionC2to5usingAcA10NWEndpointO_AA12NWParametersCtcfC

.text
.p2align 2
.globl _loader_swift_connection_init_host
_loader_swift_connection_init_host:
.cfi_startproc
stp x29, x30, [sp, #-32]!
.cfi_def_cfa_offset 32
.cfi_offset x29, -32
.cfi_offset x30, -24
mov x29, sp
bl _$s7Network12NWConnectionC4host4port5usingAcA10NWEndpointO4HostO_AH4PortVAA12NWParametersCtcfC
str x0, [sp, #16]
bl _loader_swift_connection_created
ldr x0, [sp, #16]
ldp x29, x30, [sp], #32
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_connection_init_host
.quad _$s7Network12NWConnectionC4host4port5usingAcA10NWEndpointO4HostO_AH4PortVAA12NWParametersCtcfC


.text
.p2align 2
.globl _loader_swift_connection_start
_loader_swift_connection_start:
.cfi_startproc
stp x29, x30, [sp, #-48]!
.cfi_def_cfa_offset 48
.cfi_offset x29, -48
.cfi_offset x30, -40
mov x29, sp
stp x0, x8, [sp, #16]
mov x0, x20
bl _loader_swift_connection_start_begin
cbz w0, 1f
ldp x0, x8, [sp, #16]
bl _$s7Network12NWConnectionC5start5queueySo012OS_dispatch_D0C_tF
bl _loader_swift_network_start_end
ldp x29, x30, [sp], #48
ret
1:
ldp x29, x30, [sp], #48
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_connection_start
.quad _$s7Network12NWConnectionC5start5queueySo012OS_dispatch_D0C_tF


.text
.p2align 2
.globl _loader_swift_connection_restart
_loader_swift_connection_restart:
.cfi_startproc
stp x29, x30, [sp, #-48]!
.cfi_def_cfa_offset 48
.cfi_offset x29, -48
.cfi_offset x30, -40
mov x29, sp
stp x0, x8, [sp, #16]
mov x0, x20
bl _loader_swift_connection_start_begin
cbz w0, 1f
ldp x0, x8, [sp, #16]
bl _$s7Network12NWConnectionC7restartyyF
bl _loader_swift_network_start_end
ldp x29, x30, [sp], #48
ret
1:
ldp x29, x30, [sp], #48
ret
.cfi_endproc
.section __DATA,__interpose
.p2align 3
.quad _loader_swift_connection_restart
.quad _$s7Network12NWConnectionC7restartyyF

#endif

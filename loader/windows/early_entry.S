/* x64-only, position independent primary-thread APC. No imported addresses,
   relocations or writable executable pages. Initialization runs outside DllMain. */
.intel_syntax noprefix
.text
.global extend_apc_begin
.global extend_apc_end
extend_apc_begin:
 push rbx
 push rsi
 push rdi
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rcx
 mov dword ptr [rbx+4], 1
 mov rax, gs:[0x60]
 mov rax, [rax+0x18]
 lea r12, [rax+0x20]
 mov rsi, [r12]
 mov edi, 128
1:
 cmp rsi, r12
 je 9f
 cmp word ptr [rsi+0x48], 18
 jne 2f
 mov rdx, [rsi+0x50]
 mov rax, 0x006c00640074006e
 cmp [rdx], rax
 jne 2f
 mov rax, 0x006c0064002e006c
 cmp [rdx+8], rax
 jne 2f
 cmp word ptr [rdx+16], 0x6c
 jne 2f
 mov r15, [rsi+0x20]
 jmp 3f
2:
 mov rsi, [rsi]
 dec edi
 jnz 1b
 jmp 9f
3:
 mov eax, [r15+0x3c]
 lea rdx, [r15+rax]
 mov eax, [rdx+0x88]
 lea r12, [r15+rax]
 mov r14d, [r12+0x18]
 mov eax, [r12+0x20]
 lea rsi, [r15+rax]
 xor edi, edi
4:
 cmp edi, r14d
 jae 9f
 mov eax, [rsi+rdi*4]
 lea rdx, [r15+rax]
 mov rax, 0x4464616f4c72644c
 cmp [rdx], rax
 jne 5f
 cmp word ptr [rdx+8], 0x6c6c
 jne 5f
 cmp byte ptr [rdx+10], 0
 je 6f
5:
 inc edi
 jmp 4b
6:
 mov eax, [r12+0x24]
 add rax, r15
 movzx eax, word ptr [rax+rdi*2]
 mov edx, [r12+0x1c]
 add rdx, r15
 mov eax, [rdx+rax*4]
 add rax, r15
 xor ecx, ecx
 xor edx, edx
 lea r8, [rbx+24]
 lea r9, [rbx+8]
 call rax
 mov [rbx+4], eax
 test eax, eax
 js 9f
 mov rax, [rbx+8]
 mov edx, [rbx+16]
 add rax, rdx
 xor ecx, ecx
 call rax
 mov [rbx+4], eax
9:
 cmp dword ptr [rbx+4], 0
 je 10f
 /* Never continue into an uninitialized host after bootstrap failure. */
 mov ecx, 7
 int 0x29
10:
 mov dword ptr [rbx], 1
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rdi
 pop rsi
 pop rbx
 ret
extend_apc_end:

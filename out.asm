section .bss
   IO_buf resb 256

section .text
print_str:                    ; rdi = ptr, rsi = len, rdx = newline flag
   push rbp
   mov rbp, rsp
   push rdx
   mov rdx, rsi
   mov rsi, rdi
   mov rax, 1
   mov rdi, 1
   syscall
   pop rdx
   test dl, dl
   jz .print_str_done
   sub rsp, 16
   mov byte [rbp - 8], 10
   lea rsi, [rbp - 8]
   mov rdx, 1
   mov rdi, 1
   mov rax, 1
   syscall
.print_str_done:
   mov rsp, rbp
   pop rbp
   ret

print_char:                   ; rdi = char, rsi = newline flag (0/1)
   push rbp
   mov rbp, rsp
   sub rsp, 16
   mov byte [rbp - 2], dil
   test sil, sil
   jz .print_char_one
   mov byte [rbp - 1], 10
   mov rdx, 2
   jmp .print_char_write
.print_char_one:
   mov rdx, 1
.print_char_write:
   lea rsi, [rbp - 2]
   mov rax, 1
   mov rdi, 1
   syscall
   mov rsp, rbp
   pop rbp
   ret

print_int:                     ; rdi = num, rsi = newline flag (0/1)
   mov r11, rsi                ; save flag, rsi gets clobbered below
   mov rax, rdi
   mov byte [IO_buf+31], 10
   lea r10, [IO_buf+30]
   mov rcx, 10
   mov r8, 0
   test rax, rax
   jnz .pi_sign
   mov byte [r10], '0'
   dec r10
   jmp .pi_write
.pi_sign:
   jns .pi_convert
   mov r8, 1
   neg rax
.pi_convert:
   xor rdx, rdx
   div rcx
   add dl, '0'
   mov [r10], dl
   dec r10
   test rax, rax
   jnz .pi_convert
   cmp r8, 1
   jne .pi_write
   mov byte [r10], '-'
   dec r10
.pi_write:
   inc r10
   lea rdx, [IO_buf+31]
   sub rdx, r10
   add rdx, r11
   mov rax, 1
   mov rdi, 1
   mov rsi, r10
   syscall
   ret

global _start
_start:
   call main
   mov rdi, rax
   mov rax, 60
   syscall


test_func:
   push rbp
   mov rbp, rsp
   sub rsp, 80
   mov [rbp-8], rdi
   mov [rbp-16], rsi
.test_func_L0:
   lea rax, [rbp-64]
   mov [rbp-24], rax
   mov rbx, [rbp-8]
   mov qword [rax], rbx
   lea rax, [rbp-72]
   mov [rbp-32], rax
   mov rbx, [rbp-16]
   mov qword [rax], rbx
   mov rax, [rbp-24]
   mov rbx, [rax]
   mov [rbp-40], rbx
   mov rax, [rbp-32]
   mov rbx, [rax]
   mov [rbp-48], rbx
   mov rax, [rbp-40]
   add rax, rbx
   mov [rbp-56], rax
   mov rsp, rbp
   pop rbp
   ret

a_thing:
   push rbp
   mov rbp, rsp
   sub rsp, 80
   mov [rbp-8], rdi
   mov [rbp-16], rsi
.a_thing_L0:
   lea rax, [rbp-64]
   mov [rbp-24], rax
   mov rbx, [rbp-8]
   mov qword [rax], rbx
   lea rax, [rbp-72]
   mov [rbp-32], rax
   mov rbx, [rbp-16]
   mov qword [rax], rbx
   mov rax, [rbp-24]
   mov rbx, [rax]
   mov [rbp-40], rbx
   mov rax, [rbp-32]
   mov rbx, [rax]
   mov [rbp-48], rbx
   mov rax, [rbp-40]
   add rax, rbx
   mov [rbp-56], rax
   mov rsp, rbp
   pop rbp
   ret

loops:
   push rbp
   mov rbp, rsp
   sub rsp, 800
.loops_L0:
   lea rax, [rbp-680]
   mov [rbp-8], rax
   lea rax, [rbp-696]
   mov [rbp-16], rax
   lea rax, [rbp-712]
   mov [rbp-24], rax
   lea rax, [str_0]
   mov [rbp-32], rax
   mov rax, [rbp-24]
   mov rbx, [rbp-32]
   mov qword [rax], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-40], rax
   mov rax, 6
   mov [rbp-48], rax
   mov rax, [rbp-40]
   mov rbx, [rbp-48]
   mov qword [rax], rbx
   mov rax, [rbp-24]
   mov rbx, [rax]
   mov [rbp-56], rbx
   mov rax, [rbp-16]
   mov qword [rax], rbx
   mov rax, [rbp-24]
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-64], rax
   mov rax, [rbp-16]
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-72], rax
   mov rax, [rbp-64]
   mov rbx, [rax]
   mov [rbp-80], rbx
   mov rax, [rbp-72]
   mov qword [rax], rbx
   lea rax, [rbp-720]
   mov [rbp-88], rax
   mov rax, 0
   mov [rbp-96], rax
   mov rax, [rbp-88]
   mov rbx, [rbp-96]
   mov qword [rax], rbx
   jmp .loops_L1
.loops_L1:
   mov rax, [rbp-88]
   mov rbx, [rax]
   mov [rbp-104], rbx
   mov rax, 10
   mov [rbp-112], rax
   mov rax, rbx
   mov rbx, [rbp-112]
   cmp rax, rbx
   setl al
   movzx rax, al
   mov [rbp-120], rax
   cmp rax, 0
   jne .loops_L2
   jmp .loops_L3
.loops_L2:
   mov rax, [rbp-88]
   mov rbx, [rax]
   mov [rbp-128], rbx
   mov rbx, [rax]
   mov [rbp-136], rbx
   mov rax, [rbp-8]
   imul rbx, 8
   add rax, rbx
   mov [rbp-144], rax
   mov rbx, [rbp-128]
   mov qword [rax], rbx
   mov rax, [rbp-88]
   mov rbx, [rax]
   mov [rbp-152], rbx
   mov rax, 1
   mov [rbp-160], rax
   mov rax, rbx
   mov rbx, [rbp-160]
   add rax, rbx
   mov [rbp-168], rax
   mov rax, [rbp-88]
   mov rbx, [rbp-168]
   mov qword [rax], rbx
   jmp .loops_L1
.loops_L3:
   lea rax, [rbp-728]
   mov [rbp-176], rax
   mov rax, 0
   mov [rbp-184], rax
   mov rax, [rbp-176]
   mov rbx, [rbp-184]
   mov qword [rax], rbx
   jmp .loops_L4
.loops_L4:
   mov rax, [rbp-176]
   mov rbx, [rax]
   mov [rbp-192], rbx
   mov rax, 10
   mov [rbp-200], rax
   mov rax, rbx
   mov rbx, [rbp-200]
   cmp rax, rbx
   setl al
   movzx rax, al
   mov [rbp-208], rax
   cmp rax, 0
   jne .loops_L5
   jmp .loops_L6
.loops_L5:
   mov rax, [rbp-176]
   mov rbx, [rax]
   mov [rbp-216], rbx
   mov rax, [rbp-8]
   imul rbx, 8
   add rax, rbx
   mov [rbp-224], rax
   mov rbx, [rax]
   mov [rbp-232], rbx
   mov rdi, [rbp-232]
   mov rsi, 0
   call print_int
   lea rax, [rbp-744]
   mov [rbp-240], rax
   lea rax, [str_1]
   mov [rbp-248], rax
   mov rax, [rbp-240]
   mov rbx, [rbp-248]
   mov qword [rax], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-256], rax
   mov rax, 1
   mov [rbp-264], rax
   mov rax, [rbp-256]
   mov rbx, [rbp-264]
   mov qword [rax], rbx
   mov rax, [rbp-240]
   mov rbx, [rax]
   mov [rbp-272], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-280], rax
   mov rbx, [rax]
   mov [rbp-288], rbx
   mov rdi, [rbp-272]
   mov rsi, [rbp-288]
   mov rdx, 0
   call print_str
   mov rax, [rbp-176]
   mov rbx, [rax]
   mov [rbp-296], rbx
   mov rax, 1
   mov [rbp-304], rax
   mov rax, rbx
   mov rbx, [rbp-304]
   add rax, rbx
   mov [rbp-312], rax
   mov rax, [rbp-176]
   mov rbx, [rbp-312]
   mov qword [rax], rbx
   jmp .loops_L4
.loops_L6:
   mov rax, 10
   mov [rbp-320], rax
   mov rdi, [rbp-320]
   mov rsi, 1
   call print_char
   lea rax, [rbp-752]
   mov [rbp-328], rax
   mov rax, 0
   mov [rbp-336], rax
   mov rax, [rbp-328]
   mov rbx, [rbp-336]
   mov qword [rax], rbx
   jmp .loops_L7
.loops_L7:
   mov rax, [rbp-328]
   mov rbx, [rax]
   mov [rbp-344], rbx
   mov rax, 6
   mov [rbp-352], rax
   mov rax, rbx
   mov rbx, [rbp-352]
   cmp rax, rbx
   setl al
   movzx rax, al
   mov [rbp-360], rax
   cmp rax, 0
   jne .loops_L8
   jmp .loops_L9
.loops_L8:
   lea rax, [rbp-768]
   mov [rbp-368], rax
   lea rax, [str_2]
   mov [rbp-376], rax
   mov rax, [rbp-368]
   mov rbx, [rbp-376]
   mov qword [rax], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-384], rax
   mov rax, 1
   mov [rbp-392], rax
   mov rax, [rbp-384]
   mov rbx, [rbp-392]
   mov qword [rax], rbx
   mov rax, [rbp-368]
   mov rbx, [rax]
   mov [rbp-400], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-408], rax
   mov rbx, [rax]
   mov [rbp-416], rbx
   mov rdi, [rbp-400]
   mov rsi, [rbp-416]
   mov rdx, 0
   call print_str
   mov rax, [rbp-16]
   mov rbx, [rax]
   mov [rbp-424], rbx
   mov rax, [rbp-328]
   mov rbx, [rax]
   mov [rbp-432], rbx
   mov rax, [rbp-424]
   imul rbx, 1
   add rax, rbx
   mov [rbp-440], rax
   movzx rbx, byte [rax]
   mov [rbp-448], rbx
   mov rdi, [rbp-448]
   mov rsi, 0
   call print_char
   lea rax, [rbp-784]
   mov [rbp-456], rax
   lea rax, [str_3]
   mov [rbp-464], rax
   mov rax, [rbp-456]
   mov rbx, [rbp-464]
   mov qword [rax], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-472], rax
   mov rax, 1
   mov [rbp-480], rax
   mov rax, [rbp-472]
   mov rbx, [rbp-480]
   mov qword [rax], rbx
   mov rax, [rbp-456]
   mov rbx, [rax]
   mov [rbp-488], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-496], rax
   mov rbx, [rax]
   mov [rbp-504], rbx
   mov rdi, [rbp-488]
   mov rsi, [rbp-504]
   mov rdx, 0
   call print_str
   lea rax, [rbp-800]
   mov [rbp-512], rax
   lea rax, [str_4]
   mov [rbp-520], rax
   mov rax, [rbp-512]
   mov rbx, [rbp-520]
   mov qword [rax], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-528], rax
   mov rax, 1
   mov [rbp-536], rax
   mov rax, [rbp-528]
   mov rbx, [rbp-536]
   mov qword [rax], rbx
   mov rax, [rbp-512]
   mov rbx, [rax]
   mov [rbp-544], rbx
   mov rbx, 1
   imul rbx, 8
   add rax, rbx
   mov [rbp-552], rax
   mov rbx, [rax]
   mov [rbp-560], rbx
   mov rdi, [rbp-544]
   mov rsi, [rbp-560]
   mov rdx, 0
   call print_str
   mov rax, [rbp-328]
   mov rbx, [rax]
   mov [rbp-568], rbx
   mov rax, 1
   mov [rbp-576], rax
   mov rax, rbx
   mov rbx, [rbp-576]
   add rax, rbx
   mov [rbp-584], rax
   mov rax, [rbp-328]
   mov rbx, [rbp-584]
   mov qword [rax], rbx
   jmp .loops_L7
.loops_L9:
   mov rax, 10
   mov [rbp-592], rax
   mov rdi, [rbp-592]
   mov rsi, 1
   call print_char
   mov rax, 9
   mov [rbp-600], rax
   mov rsp, rbp
   pop rbp
   ret

main:
   push rbp
   mov rbp, rsp
   sub rsp, 16
.main_L0:
   call loops
   mov [rbp-8], rax
   mov rsp, rbp
   pop rbp
   ret

section .data
   str_0: db 72, 101, 108, 108, 111, 33
   str_1: db 32
   str_2: db 39
   str_3: db 39
   str_4: db 32

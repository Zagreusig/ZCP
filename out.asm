LF           equ 10     ; newline
NULL         equ 0      ; null term
TRUE         equ 1
FALSE        equ 0

EXIT_SUCCESS equ 0

STDIN        equ 0
STDOUT       equ 1
STDERR       equ 2

SYS_read     equ 0
SYS_write    equ 1
SYS_open     equ 2     ; file open
SYS_close    equ 3     ; file close
SYS_fork     equ 57    ; fork
SYS_exit     equ 60    ; terminate
SYS_creat    equ 85    ; file open/create
SYS_time     equ 201   ; get time

STDIN_LEN    equ 64    ; max len to read
newLine      db  LF, NULL
section .bss
   print_buf resb 32
   read_buf  resb 64
   chr       resb 1

section .text
global _start
_start:
   call main
   mov rdi, rax
   mov rax, SYS_exit
   syscall

print_int:                 ; Positive ints for now
   ; rax = num, rdi = newline flag (0/1)
   mov byte [print_buf + 31], LF   ; park '\n' at the end
   lea rsi, [print_buf + 30]       ; digits fill from here back
   mov rcx, LF
   xor r8d, r8d
   test rax, rax
   jnz .pi_sign
   mov byte [rsi], '0'
   dec rsi
   jmp .pi_write
.pi_sign:
   jns .pi_convert
   mov r8, 1
   neg rax
.pi_convert:
   xor rdx, rdx
   div rcx
   add dl, '0'
   mov [rsi], dl
   dec rsi
   test rax, rax
   jnz .pi_convert
   cmp r8, 1
   jne .pi_write
   mov byte [rsi], '-'
   dec rsi
.pi_write:
   inc rsi
   ; base length: up to the byte BEFORE newline slot
   lea rdx, [print_buf + 31]
   sub rdx, rsi    ; length WITHOUT newline
   add rdx, rdi    ; + flag (if using newline, adds the byte back)
   mov rax, SYS_write
   mov rdi, STDOUT ; NOTE: this overwrites flag.
   syscall
   ret
read_int:
   push rbx
   push r12
   push r13

   xor rax, rax          ; accumulator = 0
   xor r13, r13          ; sign = 0 (pos)
   xor r12d, r12d

.ri_loop:
   push rax
   mov rax, SYS_read
   mov rdi, STDIN
   lea rsi, [chr]
   mov rdx, 1
   syscall
   cmp rax, FALSE
   pop rax
   je .ri_done           ; EOF -> stop

   movzx rcx, byte [chr] ; the char, zero-extended (ONE byte)
   cmp rcx, LF
   je .ri_done
   cmp rcx, '0'
   jb .ri_done
   cmp rcx, '9'
   ja .ri_done

   sub rcx, '0'          ; ASCII -> digit value
   mov rdx, rax
   shl rax, 3            ; rax * 8
   shl rdx, 1            ; rdx * 2
   add rax, rdx          ; rax * 10
   add rax, rcx          ; + digit
   jmp .ri_loop

.ri_done:
   pop r13
   pop r12
   pop rbx
   ret
read_char:
   mov rax, SYS_read
   mov rdi, STDIN
   lea rsi, [chr]
   mov rdx, 1
   syscall
   movzx rax, byte [chr]
   ret
greet:
   push rbp
   push r12
   mov rbp, rsp
   sub rsp, 16
   mov r12, rsp
   mov QWORD [r12 + 0], rdi
   mov QWORD [r12 + 8], rsi
   mov rsi, QWORD [r12 + 0]
   mov rdx, QWORD [r12 + 8]
   mov rax, SYS_write
   mov rdi, STDOUT
   syscall
   mov byte [print_buf], LF
   mov rax, SYS_write
   mov rdi, STDOUT
   mov rsi, print_buf
   mov rdx, 1
   syscall
   mov rsp, rbp
   pop r12
   pop rbp
   ret
foo:
   push rbp
   push r12
   mov rbp, rsp
   sub rsp, 32
   mov r12, rsp
   mov QWORD [r12 + 0], rdi
   mov QWORD [r12 + 8], rsi
   mov QWORD [r12 + 16], rdx
   mov QWORD [r12 + 24], rcx
   mov rsi, QWORD [r12 + 0]
   mov rdx, QWORD [r12 + 8]
   mov rax, SYS_write
   mov rdi, STDOUT
   syscall
   mov byte [print_buf], LF
   mov rax, SYS_write
   mov rdi, STDOUT
   mov rsi, print_buf
   mov rdx, 1
   syscall
   mov rsi, QWORD [r12 + 16]
   mov rdx, QWORD [r12 + 24]
   mov rax, SYS_write
   mov rdi, STDOUT
   syscall
   mov byte [print_buf], LF
   mov rax, SYS_write
   mov rdi, STDOUT
   mov rsi, print_buf
   mov rdx, 1
   syscall
   mov rsp, rbp
   pop r12
   pop rbp
   ret
bar:
   push rbp
   push r12
   mov rbp, rsp
   sub rsp, 32
   mov r12, rsp
   mov QWORD [r12 + 0], rdi
   mov QWORD [r12 + 8], rsi
   mov QWORD [r12 + 16], rdx
   mov rax, QWORD [r12 + 0]
   mov rdi, 1
   call print_int
   mov rsi, QWORD [r12 + 8]
   mov rdx, QWORD [r12 + 16]
   mov rax, SYS_write
   mov rdi, STDOUT
   syscall
   mov byte [print_buf], LF
   mov rax, SYS_write
   mov rdi, STDOUT
   mov rsi, print_buf
   mov rdx, 1
   syscall
   mov rsp, rbp
   pop r12
   pop rbp
   ret
third:
   push rbp
   push r12
   mov rbp, rsp
   sub rsp, 32
   mov r12, rsp
   mov QWORD [r12 + 0], rdi
   mov byte [r12 + 8], sil
   mov QWORD [r12 + 9], rdx
   mov QWORD [r12 + 17], rcx
   mov rax, QWORD [r12 + 0]
   mov rdi, 1
   call print_int
   mov rax, QWORD [r12 + 8]
   mov byte [print_buf], al
   mov byte [print_buf + 1], LF
   mov rsi, print_buf
   mov rdx, 2
   mov rdi, STDOUT
   mov rax, SYS_write
   syscall
   mov rsi, QWORD [r12 + 9]
   mov rdx, QWORD [r12 + 17]
   mov rax, SYS_write
   mov rdi, STDOUT
   syscall
   mov byte [print_buf], LF
   mov rax, SYS_write
   mov rdi, STDOUT
   mov rsi, print_buf
   mov rdx, 1
   syscall
   mov rsp, rbp
   pop r12
   pop rbp
   ret
main:
   push rbp
   push r12
   mov rbp, rsp
   sub rsp, 16
   mov r12, rsp
   lea rax, [str_1]
   mov QWORD [r12 + 0], rax
   mov QWORD [r12 + 8], 11
   lea rdi, [str_2]
   mov rsi, 2
   lea rdx, [str_3]
   mov rcx, 3
   call foo
   mov rdi, QWORD [r12 + 0]
   mov rsi, QWORD [r12 + 8]
   lea rdx, [str_4]
   mov rcx, 2
   call foo
   mov rax, 5
   mov rdi, rax
   lea rsi, [str_5]
   mov rdx, 5
   call bar
   mov rax, 5
   push rax
   mov rbx, 6
   pop rax
   add rax, rbx
   push rax
   mov rbx, 2
   pop rax
   imul rax, rbx
   mov rdi, rax
   lea rsi, [str_6]
   mov rdx, 4
   call bar
   mov rax, 1
   mov rdi, rax
   mov rax, 'a'
   mov rsi, rax
   mov rdx, QWORD [r12 + 0]
   mov rcx, QWORD [r12 + 8]
   call third
   xor eax, eax
   mov rsp, rbp
   pop r12
   pop rbp
   ret
section .data
   str_0: db 72, 101, 108, 108, 111, 32, 116, 104, 101, 114, 101
   str_0_len: equ $ - str_0
   str_1: db 72, 101, 108, 108, 111, 32, 116, 104, 101, 114, 101
   str_1_len: equ $ - str_1
   str_2: db 104, 105
   str_2_len: equ $ - str_2
   str_3: db 121, 117, 104
   str_3_len: equ $ - str_3
   str_4: db 104, 105
   str_4_len: equ $ - str_4
   str_5: db 104, 101, 108, 108, 111
   str_5_len: equ $ - str_5
   str_6: db 98, 97, 114, 33
   str_6_len: equ $ - str_6

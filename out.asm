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
   mov r8, 0
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
   mov r12, 0            ; dig count guard

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



main:
   push rbp
   mov rbp, rsp
   sub rsp, 112
   mov QWORD [rbp + -40], 0
   mov QWORD [rbp + -32], 0
   mov QWORD [rbp + -24], 0
   mov QWORD [rbp + -16], 0
   mov QWORD [rbp + -8], 0
   mov rax, 1
   push rax
   pop rax
   mov QWORD [rbp + -64], rax
   mov rax, 2
   push rax
   pop rax
   mov QWORD [rbp + -56], rax
   mov rax, 3
   push rax
   pop rax
   mov QWORD [rbp + -48], rax
   mov rax, 1
   push rax
   pop rax
   mov QWORD [rbp + -88], rax
   mov rax, 7
   push rax
   pop rax
   mov QWORD [rbp + -80], rax
   mov rax, 9
   push rax
   pop rax
   mov QWORD [rbp + -72], rax
   mov rax, 'h'
   push rax
   pop rax
   mov byte [rbp + -93], al
   mov rax, 'e'
   push rax
   pop rax
   mov byte [rbp + -92], al
   mov rax, 'l'
   push rax
   pop rax
   mov byte [rbp + -91], al
   mov rax, 'l'
   push rax
   pop rax
   mov byte [rbp + -90], al
   mov rax, 'o'
   push rax
   pop rax
   mov byte [rbp + -89], al
   mov rax, 0
   push rax
   pop rax
   mov QWORD [rbp + -101], rax
.L0:
   mov rax, QWORD [rbp + -101]
   push rax
   mov rax, 5
   push rax
   pop rbx
   pop rax
   cmp rax, rbx
   je .L1
   mov rax, QWORD [rbp + -101]
   push rax
   add QWORD [rbp + -101], 1
   pop rax
   mov rax, QWORD [rbp + -93]
   push rax
   pop rax
   mov byte [print_buf], al
   mov byte [print_buf + 1], LF
   mov rsi, print_buf
   mov rdx, 2
   mov rdi, STDOUT
   mov rax, SYS_write
   syscall
   jmp .L0
.L1:
   mov rax, 0
   push rax
   pop rax
   mov QWORD [rbp + -109], rax
   mov rax, QWORD [rbp + -109]
   push rax
   mov rax, 5
   push rax
   pop rbx
   pop rax
   cmp rax, rbx
   jge .L3
.L2:
   mov rax, QWORD [rbp + -109]
   push rax
   mov rax, 3
   push rax
   pop rbx
   pop rax
   cmp rax, rbx
   jne .L4
   mov rax, 1
   push rax
   pop rax
   mov rsp, rbp
   pop rbp
   ret
.L4:
   mov rax, QWORD [rbp + -109]
   push rax
   add QWORD [rbp + -109], 1
   pop rax
   jmp .L2
.L3:
   mov rax, 0
   push rax
   pop rax
   mov rsp, rbp
   pop rbp
   ret
   mov rsp, rbp
   pop rbp
   ret

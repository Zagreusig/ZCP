global _start
_start:
   call main
   mov rdi, rax
   mov rax, 60
   syscall



add:
   push rbp
   mov rbp, rsp
   sub rsp, 16
   mov QWORD [rbp + -8], rdi
   mov QWORD [rbp + -16], rsi
   mov rax, QWORD [rbp + -8]
   push rax
   mov rax, QWORD [rbp + -16]
   push rax
   pop rbx
   pop rax
   add rax, rbx
   push rax
   pop rax
   mov rsp, rbp
   pop rbp
   ret
   mov rsp, rbp
   pop rbp
   ret


main:
   push rbp
   mov rbp, rsp
   sub rsp, 16
   mov rax, 21
   push rax
   mov rax, 21
   push rax
   pop rsi
   pop rdi
   call add
   push rax
   pop rax
   mov QWORD [rbp + -8], rax
   mov rax, QWORD [rbp + -8]
   push rax
   pop rax
   mov rsp, rbp
   pop rbp
   ret
   mov rsp, rbp
   pop rbp
   ret

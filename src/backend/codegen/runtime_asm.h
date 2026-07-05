#ifndef RUNTIME_ASM_H
#define RUNTIME_ASM_H

#include "utils/msc.h"

enum class Runtime { 
   None       = 0,
   PrintInt   = BIT(0), 
   PrintChar  = BIT(1), 
   PrintBuf   = BIT(2), 
   ReadInt    = BIT(3), 
   ReadChar   = BIT(4), 
   ReadBuf    = BIT(5) 
};
/** TODO: Add floats and bools once developed. */

constexpr const char* PRINT_INT = {
   "; Entry:\n"\
   ";    rdi = value to print (signed 64-bit)\n"\
   ";    sil = newline flag (nonzero -> append '\n') [low byte of rsi]\n"\
   ";\n"\
   ";    Clobbers: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11\n"\
   ";    Preserves: rbx, rbp, r12-r15\n"\
   "print_int:\n"\
   "   push rbp\n"\
   "   mov rbp, rsp\n"\
   "   sub rsp, 32\n"
   "   ; r8 = write cursor"\
   "   ; --- optional trailing newline ---\n"\
   "   test sil, sil\n"\
   "   jz .no_newline\n"\
   "   dec r8\n"\
   "   mov byte [r8], 10     ; 1 past last buffer byte.\n\n"\
   ".no_newline:\n" \
   "   test rdi, rdi\n"\
   "   jnz .nonzero\n"\
   "   dec r8\n"\
   "   mov byte [r8], '0'\n"\
   "   jmp .emit\n\n"\
   ".nonzero:\n"\
   "   xor r9, r9       ; r9 = is_negative flag(0)\n"\
   "   mov rax, rdi\n"\
   "   test rax, rax\n"\
   "   jns .positive\n"\
   "   mov r9, 1         ; negative\n"\
   "   neg rax           ; rax = -value\n\n"\
   ".positive:\n"\
   "   mov rcx, 10       ; divisor\n"\
   ".digit_loop:\n"\
   "   xor rdx, rdx      ; clear high\n"\
   "   div rcx           ; rax = rax / 10, rdx = rax % 10\n"\
   "   add dl, '0'       ; remainder -> ASCII digit\n"\
   "   dec r8\n"\
   "   mov [r8], dl\n"\
   "   test rax, rax\n"\
   "   jnz .digit_loop\n\n"\
   "   ; --- leading '-' if negative ---\n"\
   "   test r9, r9\n"\
   "   jz .emit\n"\
   "   dec r8\n"\
   "   mov byte [r8], '-'\n\n"\
   ".emit:\n"\
   "   ; write(STDOUT, r8, rbp - r8)\n"\
   "   lea rdx, [rbp]\n"\
   "   sub rdx, r8\n"\
   "   mov rsi, r8       ; rsi ptr to first char\n"\
   "   mov rdi, 1\n"\
   "   syscall\n\n"\
   "   mov rsp, rbp\n"\
   "   pop rbp\n"\
   "   ret\n"
};

#endif // RUNTIME_ASM_H
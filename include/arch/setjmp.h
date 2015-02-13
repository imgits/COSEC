#ifndef __COSEC_X86_SETJMP_H__
#define __COSEC_X86_SETJMP_H__

#define MY_JMPBUF_SIZE 6

#define JMPBUF_EIP_OFFSET 0
#define JMPBUF_ESP_OFFSET 1
#define JMPBUF_EBX_OFFSET 2
#define JMPBUF_ESI_OFFSET 3
#define JMPBUF_EDI_OFFSET 4
#define JMPBUF_EBP_OFFSET 5

#ifndef NOT_CC

typedef int i386_jmp_buf[MY_JMPBUF_SIZE];

int i386_setjmp(i386_jmp_buf env);

void i386_longjmp(i386_jmp_buf env, int val);

#endif // NOT_CC
#endif // __COSEC_X86_SETJMP_H__

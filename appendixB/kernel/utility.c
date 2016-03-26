/* This file contains a collection of miscellaneous procedures:
 *   panic:	    abort MINIX due to a fatal error
 *   kprintf:	    diagnostic output for the kernel 
 *
 * Changes:
 *   Dec 10, 2004   kernel printing to circular buffer  (Jorrit N. Herder)
 * 
 * This file contains the routines that take care of kernel messages, i.e.,
 * diagnostic output within the kernel. Kernel messages are not directly
 * displayed on the console, because this must be done by the output driver. 
 * Instead, the kernel accumulates characters in a buffer and notifies the
 * output driver when a new message is ready. 
 */
/*
 * 这个文件包含一些其他例程:
 *	panic:		因为一个致命的错误, 中止 MINIX;
 *	kprintf:	内核的诊断信息输出.
 *
 * 更改:
 *	...
 * 
 * 这个文件所包含的例程关注了内核消息, 也就是内核的诊断信息输出. 内核消息
 * 并不是直接在控制台上显示, 在控制台上显示消息是由输出驱动程序完成的.
 * 作为替代, 内核把字符累积在一个缓冲区中, 当一个新的消息准备好时通知输出
 * 驱动程序.
 */

#include <minix/com.h>
#include "kernel.h"
#include <stdarg.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include "proc.h"

#define END_OF_KMESS 	-1
FORWARD _PROTOTYPE(void kputc, (int c));

/*===========================================================================*
 *				panic                                        *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
/* 系统遇到一个严重的内核错误. 终止运行 */
  static int panicking = 0;
  // panic() 不可递归
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("\nKernel panic: %s", mess);
	if (nr != NO_NUM) kprintf(" %d", nr);
	kprintf("\n",NO_NUM);
  }

  /* Abort MINIX. */
  prepare_shutdown(RBT_PANIC);
}

/*===========================================================================*
 *				kprintf					     *
 *===========================================================================*/
// 内核的 printf
PUBLIC void kprintf(const char *fmt, ...) 	/* format to be printed */
{
  int c;					/* next character in fmt */
  int d;
  unsigned long u;				/* hold number argument */
  int base;					/* base of number arg */
  int negative = 0;				/* print minus sign */
  static char x2c[] = "0123456789ABCDEF";	/* nr conversion table */
  char ascii[8 * sizeof(long) / 3 + 2];		/* string for ascii number */
  char *s = NULL;				/* string to be printed */
  va_list argp;					/* optional arguments */
  
  va_start(argp, fmt);				/* init variable arguments */

  while((c=*fmt++) != 0) {

      if (c == '%') {				/* expect format '%key' */
          switch(c = *fmt++) {			/* determine what to do */

          /* Known keys are %d, %u, %x, %s, and %%. This is easily extended 
           * with number types like %b and %o by providing a different base.
           * Number type keys don't set a string to 's', but use the general
           * conversion after the switch statement.
           */ 
          case 'd':				/* output decimal */
              d = va_arg(argp, signed int);
              if (d < 0) { negative = 1; u = -d; }  else { u = d; }
              base = 10;
              break;
          case 'u':				/* output unsigned long */
              u = va_arg(argp, unsigned long);
              base = 10;
              break;
          case 'x':				/* output hexadecimal */
              u = va_arg(argp, unsigned long);
              base = 0x10;
              break;
          case 's': 				/* output string */
              s = va_arg(argp, char *);
              if (s == NULL) s = "(null)";
              break;
          case '%':				/* output percent */
              s = "%";				 
              break;			

          /* Unrecognized key. */
          default:				/* echo back %key */
              s = "%?";				
              s[1] = c;				/* set unknown key */
          }

          /* Assume a number if no string is set. Convert to ascii. */
          if (s == NULL) {
              s = ascii + sizeof(ascii)-1;
              *s = 0;			
              do {  *--s = x2c[(u % base)]; }	/* work backwards */
              while ((u /= base) > 0); 
          }

          /* This is where the actual output for format "%key" is done. */
          if (negative) kputc('-');  		/* print sign if negative */
          while(*s != 0) { kputc(*s++); }	/* print string/ number */
	  s = NULL;				/* reset for next round */
      }
      else {
          kputc(c);				/* print and continue */
      }
  }
  kputc(END_OF_KMESS);				/* terminate output */
  va_end(argp);					/* end variable arguments */
}

/*===========================================================================*
 *				kputc				     	     *
 *===========================================================================*/
PRIVATE void kputc(c)
int c;					/* character to append */
{
/* Accumulate a single character for a kernel message. Send a notification
 * to the output driver if an END_OF_KMESS is encountered. 
 */
/*
 * 为一个内核消息累积一个字符. 如果遇到 END_OF_KMESS 就向输出驱动程序发送
 * 一个通知.
 */
  if (c != END_OF_KMESS) {
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      if (kmess.km_size < KMESS_BUF_SIZE)
          kmess.km_size += 1;		
      kmess.km_next = (kmess.km_next + 1) % KMESS_BUF_SIZE;
  } else {
	// 如果内核的诊断信息可以输出了, 就向输出驱动程序发送一个
	// 信号.
      send_sig(OUTPUT_PROC_NR, SIGKMESS);
  }
}


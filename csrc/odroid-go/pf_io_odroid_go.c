/* $Id$ */
/***************************************************************
** I/O subsystem for PForth based on 'C'
**
** Author: Phil Burk
** Copyright 1994 3DO, Phil Burk, Larry Polansky, David Rosenboom
**
** ODROID GO programming by Mike Schwartz
**
** The pForth software code is dedicated to the public domain,
** and any third party may reproduce, distribute and modify
** the pForth software code or any derivative works thereof
** without any compensation or license.  The pForth software
** code is provided on an "as is" basis without any warranty
** of any kind, including, without limitation, the implied
** warranties of merchantability and fitness for a particular
** purpose and their equivalents under the laws of any jurisdiction.
**
****************************************************************
** 941004 PLB Extracted IO calls from pforth_main.c
** 090220 PLB Fixed broken sdQueryTerminal on Mac. It always returned true.
***************************************************************/

#include "../pf_all.h"

static int putback = -1;

int sdTerminalOut(char c) { return putchar(c); }
int sdTerminalEcho(char c) {
  putchar(c);
  return 0;
}
int sdTerminalIn(void) {
  while (putback == -1) {
    putback = getchar();
    usleep(1);
  }
  int c = putback;
  putback = -1;
  return c;
}

int sdTerminalFlush(void) {
#ifdef PF_NO_FILEIO
  return -1;
#else
  return fflush(PF_STDOUT);
#endif
}

int sdQueryTerminal(void) {
  if (putback != -1) {
    return FFALSE;
  }
  putback = getchar();
  return putback == -1 ? FFALSE : FTRUE;
}

void sdTerminalInit(void) {
  // we don't need to do anything for console since posix is implemented in
  // ESP32 lib.
}

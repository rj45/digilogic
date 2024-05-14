/*
        Copyright (C) 2016 Florian Cabot
        Copyright (C) 2024 Ryan "rj45" Sanche (modifications to support mac/ios)

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 3 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License along
        with this program; if not, write to the Free Software Foundation, Inc.,
        51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef EXCEPTIONS
#define EXCEPTIONS

#include <err.h>
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRITICAL(str)                                                          \
  {                                                                            \
    print_stacktrace(0);                                                       \
    fprintf(                                                                   \
      stderr, "%s (in %s at %s:%i)\n", str, __func__, __FILE__, __LINE__);     \
    exit(EXIT_FAILURE);                                                        \
  }

#define MAX_BACKTRACE_LINES 64

char *_programName;

void print_stacktrace(int calledFromSigInt);
void posix_signal_handler(int sig);
void set_signal_handler(sig_t handler);
void init_exceptions(char *programName);
int addr2line(
  char const *const program_name, void const *const addr, int lineNb);

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
uint64_t loadAddress(void) {
  // get the address of the __TEXT,__text section
  const struct section_64 *sect = getsectbyname("__TEXT", "__text");

  // ignore the offset caused by the macho header
  uint64_t addr = sect->addr & ~(0xffffff);

  // get the slide amount
  uint64_t slide = _dyld_get_image_vmaddr_slide(0);

  // load address is the slide + the address of the binary
  return slide + addr;
}
#else
uint64_t loadAddress(void) { return 0; }
#endif

// prints formated stack trace with most information as possible
// parameter indicates if the function is called by the signal handler or not
//(to hide the call to the signal handler)
void print_stacktrace(int calledFromSigInt) {
  void *buffer[MAX_BACKTRACE_LINES];
  char **strings;

  int nptrs = backtrace(buffer, MAX_BACKTRACE_LINES);
  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }

  unsigned int i = 1;
  if (calledFromSigInt != 0)
    ++i;
  for (; i < (unsigned int)(nptrs); ++i) {
    // if addr2line failed, print what we can
    if (addr2line(_programName, buffer[i], nptrs - i - 1) != 0)
      fprintf(stderr, "[%i] %s\n", nptrs - i - 1, strings[i]);
  }

  free(strings);
}

void posix_signal_handler(int sig) {
  print_stacktrace(1);
  switch (sig) {
  case SIGABRT:
    fputs("Caught SIGABRT: usually caused by an abort() or assert()\n", stderr);
    break;
  case SIGFPE:
    fputs(
      "Caught SIGFPE: arithmetic exception, such as divide by zero\n", stderr);
    break;
  case SIGILL:
    fputs("Caught SIGILL: illegal instruction\n", stderr);
    break;
  case SIGINT:
    fputs(
      "Caught SIGINT: interactive attention signal, probably a ctrl+c\n",
      stderr);
    break;
  case SIGSEGV:
    fputs("Caught SIGSEGV: segfault\n", stderr);
    break;
  case SIGTERM:
  default:
    fputs(
      "Caught SIGTERM: a termination request was sent to the program\n",
      stderr);
    break;
  }
  _Exit(EXIT_FAILURE);
}

void set_signal_handler(sig_t handler) {
  signal(SIGABRT, handler);
  signal(SIGFPE, handler);
  signal(SIGILL, handler);
  signal(SIGINT, handler);
  signal(SIGSEGV, handler);
  signal(SIGTERM, handler);
}

// lib activation, first thing to do in main
// programName should be argv[0]
void init_exceptions(char *programName) {
  set_signal_handler(posix_signal_handler);
  _programName = programName;
}

/* Resolve symbol name and source location given the path to the executable
   and an address */
// returns 0 if address has been resolved and a message has been printed; else
// returns 1
int addr2line(
  char const *const program_name, void const *const addr, int lineNb) {
  char addr2line_cmd[512] = {0};

/* have addr2line map the address to the relent line in the code */
#ifdef __APPLE__
  uint64_t loadAddr = loadAddress();

  /* apple does things differently... */
  snprintf(
    addr2line_cmd, 512, "atos -o %.256s -l 0x%llx 0x%llx", program_name,
    loadAddr, (uint64_t)addr);
#else
  snprintf(addr2line_cmd, 512, "addr2line -f -e %.256s %p", program_name, addr);
#endif

  /* This will print a nicely formatted string specifying the
  function and source line of the address */

  FILE *fp;
  char outLine1[1035];
  char outLine2[1035];

  /* Open the command for reading. */
  fp = popen(addr2line_cmd, "r");
  if (fp == NULL)
    return 1;

  while (fgets(outLine1, sizeof(outLine1) - 1, fp) != NULL) {
    // if we have a pair of lines
    if (fgets(outLine2, sizeof(outLine2) - 1, fp) != NULL) {
      // if symbols are readable
      if (outLine2[0] != '?') {
        // only let func name in outLine1
        int i;
        for (i = 0; i < 1035; ++i) {
          if (outLine1[i] == '\r' || outLine1[i] == '\n') {
            outLine1[i] = '\0';
            break;
          }
        }

        // don't display the whole path
        int lastSlashPos = 0;

        for (i = 0; i < 1035; ++i) {
          if (outLine2[i] == '\0')
            break;
          if (outLine2[i] == '/')
            lastSlashPos = i + 1;
        }
        fprintf(
          stderr, "[%i] %p in %s at %s", lineNb, addr, outLine1,
          outLine2 + lastSlashPos);
        fflush(stderr);
      } else {
        pclose(fp);
        return 1;
      }
    } else {
#ifdef __APPLE__
      int i;
      for (i = 0; i < 1035; ++i) {
        if (outLine1[i] == '\r' || outLine1[i] == '\n') {
          outLine1[i] = '\0';
          break;
        }
      }

      if (outLine1[0] != '0' || outLine1[1] != 'x') {
        fprintf(stderr, "[%i] %p %s\n", lineNb, addr, outLine1);
        fflush(stderr);
        pclose(fp);
        return 0;
      }
#endif

      pclose(fp);
      return 1;
    }
  }

  /* close */
  pclose(fp);

  return 0;
}

#endif

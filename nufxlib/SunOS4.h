/*
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * This file was adapted from Devin Reade's "sunos4.h" in NuLib 3.2.5.
 * It is provided for compilation under SunOS 4.x, when an ANSI compiler
 * (such as gcc) is used.  The system header files aren't quite sufficient
 * to eliminate hordes of warnings.
 */
#ifndef __SunOS4__
#define __SunOS4__

#ifdef __GNUC__
extern int      _flsbuf(int, FILE*);
extern int      _filbuf(FILE*);
#endif

extern void     bcopy(char*, char*, int);
extern int      fclose(FILE*);
extern int      fflush(FILE*);
extern int      fprintf(FILE*, const char*, ...);
extern int      fread(char*, int, int, FILE *);
extern int      fseek(FILE*, long, int);
extern int      ftruncate(int, off_t);
extern int      fwrite(const char*, int, int, FILE*);
extern char*    mktemp(char *template);
extern time_t   mktime(struct tm*);
extern int      perror(const char*);
extern int      printf(const char*, ...);
extern int      remove(const char*);
extern int      rename(const char*, const char*);
extern int      tolower(int);
extern int      setvbuf(FILE*, char*, int, int);
extern int      sscanf(char*, const char*, ...);
extern int      strcasecmp(const char*, const char*);
extern int      strncasecmp(const char*, const char*, size_t);
extern long     strtol(const char *, char **, int);
extern int      system(const char*);
extern time_t   timelocal(struct tm*);
extern time_t   time(time_t*);
extern int      toupper(int);
extern int      vfprintf(FILE*, const char *, va_list);
extern char*    vsprintf(char *str, const char *format, va_list ap);

#endif /*__SunOS4__*/

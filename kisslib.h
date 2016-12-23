#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h> // tolower

#ifndef _KISSLIB_H
#define _KISSLIB_H

typedef struct fileInfo {
  char** entries;
  int* types;
  int count;
} fileInfo;

//------------------------------------------------------------------------------
extern bool read_pdf(char[], fileInfo*);
int check_known_pdf_meta(char[]);

extern bool read_mobi(char[], fileInfo*);
extern bool read_chm(char[], fileInfo*);

//------------------------------------------------------------------------------
// NOTE: Epub handling requires libzip c library, from https://nih.at/libzip/
#include <zip.h>
extern bool read_epub(char[], fileInfo*);

#endif

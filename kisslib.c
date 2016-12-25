#include "kisslib.h"

//------------------------------------------------------------------------------

#include <gtk/gtk.h> // gtk_main_iteration (read_pdf)

//------------------------------------------------------------------------------
bool read_pdf(char fileName[], fileInfo *fileData) {
  FILE *inputFile;

  inputFile = fopen(fileName, "r");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  } else {
    printf("Start parsing file \"%s\"...\n", fileName);
  }

  //----------------------------------------------------------------------------
  char currentChar = '\0';
  char nextChar = '\0';
  bool doRecord = false;

  char attributeBuffer[1024];
  int attributeWP = 0;
  bool collectAttribute = false;
  unsigned int offset = 0;

  fseek(inputFile, 0, SEEK_END);
  unsigned int fileEnd = ftell(inputFile);
  fseek(inputFile, fileEnd-64, SEEK_SET);
  char xRef[64];
  int pos = 0;

  while (!feof(inputFile)) {
    currentChar = fgetc(inputFile);

    switch(currentChar) {
      case 'x':
        doRecord = true;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        if (doRecord) {
          xRef[pos++] = currentChar;
        }
        break;
      default:
        break;
    }
  }

  xRef[pos] = '\0';

  int readUntil = atol(xRef);
  rewind(inputFile);


  if (readUntil == -1) {
    readUntil = fileEnd;
  }

  doRecord = false;
  unsigned int currentPos = 0;

  unsigned int updateCount = 4096;

  while (currentPos++ < readUntil) {
    if (--updateCount == 0) {
      gtk_main_iteration_do(false);
      updateCount = 4096;
    }

    currentChar = fgetc(inputFile);

    if (currentChar < 0) {
      continue;
    }

    if (currentChar == '<' || currentChar == '>') {
        nextChar = fgetc(inputFile);

        if (currentChar == nextChar) {
          if (currentChar == '<') {
            doRecord = true;
          } else {
            if (collectAttribute && attributeWP != 0) {
              attributeBuffer[attributeWP] = '\0';

              int retVal = check_known_pdf_meta(attributeBuffer);
              if (retVal != 0) {
                fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
                fileData->entries[fileData->count] = malloc(sizeof(char) * (attributeWP+1));
                strcpy(fileData->entries[fileData->count], attributeBuffer);

                fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
                fileData->types[fileData->count] = retVal;
                ++fileData->count;
              }

              if (offset != 0) {
                currentPos += offset;
                fseek(inputFile, currentPos, SEEK_SET);
                offset = 0;
              }

              attributeWP = 0;
              collectAttribute = false;
            }

            doRecord = false;
          }

          continue;
        }

        fseek(inputFile, ftell(inputFile)-1, SEEK_SET);
    }

    if (doRecord) {
      nextChar = fgetc(inputFile);
      if (attributeWP == 1024 || currentChar == '\0') {
        attributeBuffer[0] = '\0';
        attributeWP = 0;
      }

      if (collectAttribute && currentChar == '/' && nextChar == '/') {
        attributeBuffer[attributeWP++] = '/';
        attributeBuffer[attributeWP++] = '/';
        continue;
      } else {
        fseek(inputFile, ftell(inputFile)-1, SEEK_SET);
      }

      if (collectAttribute) {
        if (strncmp(attributeBuffer, "Length ", 7) == 0) {
          attributeBuffer[attributeWP] = '\0';
          offset = atoi(attributeBuffer);

          collectAttribute = false;
          attributeBuffer[0] = '\0';
          attributeWP = 0;

          if (offset != 0) {
            currentPos += offset;
            fseek(inputFile, currentPos, SEEK_SET);
            offset = 0;
          }

          continue;
        }
      }

      if (currentChar == '/') {
        if (collectAttribute && \
          ( (strncmp(attributeBuffer, "URI", 3) == 0 && strncmp(&attributeBuffer[attributeWP-3], "URI", 3) != 0) \
          || (strncmp(attributeBuffer, "Creator", 7) == 0 && strncmp(&attributeBuffer[attributeWP-2], "))", 2) == 0))\
        ) {
          attributeBuffer[attributeWP++] = currentChar;
          continue;
        }

        if (collectAttribute && attributeWP != 0) {
          attributeBuffer[attributeWP++] = '\0';

          int retVal = check_known_pdf_meta(attributeBuffer);

          if (retVal != 0) {
            fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
            fileData->entries[fileData->count] = malloc(sizeof(char) * attributeWP);
            strcpy(fileData->entries[fileData->count], attributeBuffer);

            fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
            fileData->types[fileData->count] = retVal;
            ++fileData->count;
          }
        }

        attributeWP = 0;
        attributeBuffer[0] = '\0';
        collectAttribute = true;
        continue;
      } else if (collectAttribute) {
        attributeBuffer[attributeWP++] = currentChar;
      }
    }

  }

  fclose(inputFile);
  return true;
}

int check_known_pdf_meta(char attributeBuffer[]) {
  if (strncmp(attributeBuffer, "Title(", 6) == 0) {
    //printf("%s\n", attributeBuffer);
    return true;
  } else if (strncmp(attributeBuffer, "Author(", 7) == 0) {
    //printf("%s\n", attributeBuffer);
    return 2;
  /*} else if (strncmp(attributeBuffer, "URI(http", 8) == 0 ||\
              strncmp(attributeBuffer, "URI(ftp", 7) == 0 || \
              strncmp(attributeBuffer, "URI(mailto", 10) == 0) {

    //printf("%s\n", attributeBuffer);
    return true; */
  } else if (strncmp(attributeBuffer, "Subject(", 8) == 0) {
    //printf("%s\n", attributeBuffer);
    return 3;
  } else if (strncmp(attributeBuffer, "Creator(", 8) == 0) {
    //printf("%s\n", attributeBuffer);
    return 4;
  } else if (strncmp(attributeBuffer, "Producer(", 9) == 0) {
    //printf("%s\n", attributeBuffer);
    return 5;
  } else if (strncmp(attributeBuffer, "CreationDate(", 13) == 0) {
    // (D:YYYYMMDDHHmmSSOHH'mm')
    // YYYY = Year - 2008
    // MM = Month 01-12
    // DD = Day 01-31
    // HH = Hour 00 - 23
    // mm = Minute 00 - 59
    // SS = Second 00 - 59
    // O = Relationship of local time to Universal Time either + or - or Z
    //    + = greater, - = earlier, Z = equal
    // HH' = offset hours to Universal Time
    // mm' = offset minutes to Universal Time

    //printf("%s\n", attributeBuffer);
    return 6;
  } else if (strncmp(attributeBuffer, "ModDate(", 8) == 0) {
    // (D:YYYYMMDDHHmmSSOHH'mm')
    // YYYY = Year - 2008
    // MM = Month 01-12
    // DD = Day 01-31
    // HH = Hour 00 - 23
    // mm = Minute 00 - 59
    // SS = Second 00 - 59
    // O = Relationship of local time to Universal Time either + or - or Z
    //    + = greater, - = earlier, Z = equal
    // HH' = offset hours to Universal Time
    // mm' = offset minutes to Universal Time
    //printf("%s\n", attributeBuffer);
    return 7;
  }

  return false;
}


//------------------------------------------------------------------------------
bool read_mobi(char fileName[], fileInfo *fileData) {
  FILE *inputFile;

  inputFile = fopen(fileName, "rb");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  }

  //----------------------------------------------------------------------------

  char currentChar = '\0';
  char attributeBuffer[1024];
  int attributeWP = 0;

  while (!feof(inputFile)) {
    currentChar = fgetc(inputFile);
    if (currentChar == '\0') {
      break;
    }

    attributeBuffer[attributeWP++] = currentChar;
  }

  attributeBuffer[attributeWP] = '\0';

  char title[attributeWP+8];
  sprintf(title, "Title(%s)", attributeBuffer);

  fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
  fileData->entries[fileData->count] = malloc(sizeof(char) * (attributeWP+9));
  strcpy(fileData->entries[fileData->count], title);

  fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
  fileData->types[fileData->count] = 1;
  ++fileData->count;

  //printf("Title(%s)\n", attributeBuffer);

  fclose(inputFile);
  return true;
}

//------------------------------------------------------------------------------
bool read_chm(char fileName[], fileInfo *fileData) {
  FILE *inputFile;

  inputFile = fopen(fileName, "r");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  }

  char readBuffer[1024];
  unsigned int readBufferPos = 0;

  bool doRecord = false;
  bool captureTitle = false;

  char currentChar = '\0';

  while (!feof(inputFile)) {
    currentChar = fgetc(inputFile);

    if (!doRecord && currentChar == 'H' && fgetc(inputFile) == 'H' && fgetc(inputFile) == 'A' && fgetc(inputFile) == ' ') {
      doRecord = true;
      continue;
    }

    if (doRecord) {
      if (currentChar > 16) {
        readBuffer[readBufferPos++] = currentChar;
      }
      if (captureTitle && currentChar == 0) {
        break;
      } else if (strncmp(readBuffer, "Version", 7) == 0) {
        fseek(inputFile, ftell(inputFile)+64, SEEK_SET);
        readBufferPos = 0;
        readBuffer[0] = '\0';
        char key = fgetc(inputFile);

        if (key > 0) {
          while((key = fgetc(inputFile)) > 0) {
            continue;
          }

          while((key = fgetc(inputFile)) < 32) {
            continue;
          }
        } else {
          while((key = fgetc(inputFile)) < 32) {
            continue;
          }
        }

        key = fgetc(inputFile);

        if (key == 0) {
          while((key = fgetc(inputFile)) < 32) {
            continue;
          }

          fseek(inputFile, ftell(inputFile)-1, SEEK_SET);
        } else {
          fseek(inputFile, ftell(inputFile)-2, SEEK_SET);
        }

        captureTitle = true;
      }

      if (readBufferPos == 1024) {
        readBufferPos = 0;
      }
    }

  }

  readBuffer[readBufferPos] = '\0';
  //printf("%s\n", readBuffer);

  char title[readBufferPos+8];
  sprintf(title, "Title(%s)", readBuffer);

  fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
  fileData->entries[fileData->count] = malloc(sizeof(char) * (readBufferPos+9));
  strcpy(fileData->entries[fileData->count], title);

  fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
  fileData->types[fileData->count] = 1;
  ++fileData->count;


  fclose(inputFile);
  return true;
}

//------------------------------------------------------------------------------
bool read_epub(char fileName[], fileInfo *fileData) {
  FILE *inputFile;

  inputFile = fopen(fileName, "rb");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  }

  fclose(inputFile);

  //----------------------------------------------------------------------------

  int error;
  zip_t *epubFile = zip_open(fileName, ZIP_RDONLY, &error);

  if (epubFile == NULL) {
    printf("libzip error code \"%d\" - cannot open file \"%s\".\n", error, fileName);
    return false;
  }

  zip_int64_t tocIndex = zip_name_locate(epubFile, "toc.ncx", ZIP_FL_NOCASE|ZIP_FL_NODIR);
  if (tocIndex == -1) {
    printf("Cannot find information file in epub.\n");
    zip_discard(epubFile);
    return false;
  }

  zip_stat_t *tocStat = malloc(sizeof(zip_stat_t));
  if (zip_stat_index(epubFile, tocIndex, ZIP_FL_UNCHANGED, tocStat) == -1) {
    printf("libzip error, cannot read out toc file stats.\n");
    zip_discard(epubFile);
    free(tocStat);

    return false;
  }


  zip_file_t *tocFile = zip_fopen_index(epubFile, tocIndex, ZIP_FL_UNCHANGED);
  if (tocFile == NULL) {
    printf("libzip error, cannot open toc file.\n");
    zip_discard(epubFile);
    return false;
  }


  long unsigned int buffer = tocStat->size;
  char data[buffer];

  if (buffer != zip_fread(tocFile, data, buffer)) {
    printf("libzip error, size of data read of toc file mismatch estimate.");
    return false;
  }

  //----------------------------------------------------------------------------

  unsigned int dataPos = 0;

  char readBuffer[1024];
  unsigned int readBufferPos = 0;

  bool doRecord = false;
  char currentChar = '\0';

  bool inTitle = false;
  bool skippedInnerText = false;

  while((currentChar = data[dataPos++]) != '\0') {
    if (currentChar == '<') {
      doRecord = true;

      if (skippedInnerText) {
        break;
      } else if (!inTitle) {
        readBufferPos = 0;
      }

      continue;
    } else if (currentChar == '>') {
      if (strncmp(readBuffer, "docTitle", 8) == 0) {
        inTitle = true;
      } else if (inTitle) {
        if (strncmp(readBuffer, "/docTitle", 9) == 0) {
          break;
        } else if (strncmp(readBuffer, "text", 4) == 0) {
          skippedInnerText = true;
        }
      }

      readBufferPos = 0;
      continue;
    }

    if (doRecord) {
      if (currentChar > 32) {
        readBuffer[readBufferPos++] = currentChar;
      }
    }
  }

  //----------------------------------------------------------------------------
  // Cleanup
  zip_discard(epubFile);
  free(tocStat);
  free(tocFile);

  //----------------------------------------------------------------------------
  // Output
  char title[readBufferPos != 0 ? readBufferPos+8 : strlen(fileName)+8];
  if (readBufferPos != 0) {
    readBuffer[readBufferPos] = '\0';
    //printf("Title(%s)\n", readBuffer);
    sprintf(title, "Title(%s)", readBuffer);
  } else {
    sprintf(title, "Title(%s)", fileName);
  }

  fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
  fileData->entries[fileData->count] = malloc(sizeof(char) * (readBufferPos+9));
  strcpy(fileData->entries[fileData->count], title);

  fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
  fileData->types[fileData->count] = 1;
  ++fileData->count;

  return true;
}

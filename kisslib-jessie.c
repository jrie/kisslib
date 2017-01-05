#include "kisslib.h"


//------------------------------------------------------------------------------

#include <gtk/gtk.h> // gtk_main_iteration (read_pdf)



#define bufferSize 1024

//------------------------------------------------------------------------------
bool read_pdf(char fileName[], fileInfo *fileData) {
  FILE *inputFile;

  inputFile = fopen(fileName, "rb");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  } else {
    printf("Start parsing file \"%s\"...\n", fileName);
  }

  //----------------------------------------------------------------------------
  char nextChar = '\0';
  bool doRecord = false;

  char attributeBuffer[bufferSize];
  int attributeWP = 0;
  bool collectAttribute = false;
  unsigned int offset = 0;

  fseek(inputFile, 0, SEEK_END);
  unsigned int fileEnd = ftell(inputFile);
  fseek(inputFile, fileEnd-64, SEEK_SET);
  char xRef[64];
  int pos = 0;

  char currentChar = '\0';
  int charVal = 0;

  while ((charVal = fgetc(inputFile)) != EOF) {
    currentChar = (char) charVal;

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

  long int readUntil = strtol(xRef, NULL, 10);
  rewind(inputFile);


  if (readUntil == 0) {
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
      if (attributeWP == bufferSize || currentChar == '\0') {
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

  char attributeBuffer[bufferSize];
  int attributeWP = 0;

  char currentChar = '\0';
  int charVal = 0;

  while ((charVal = fgetc(inputFile)) != EOF) {
    currentChar = (char) charVal;
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

  inputFile = fopen(fileName, "rb");
  if (inputFile == NULL) {
    printf("Error, cannot open input file \"%s\" to read out information.\n", fileName);
    return false;
  }

  char readBuffer[bufferSize];
  unsigned int readBufferPos = 0;

  bool doRecord = false;
  bool captureTitle = false;

  char currentChar = '\0';
  int charVal = 0;

  while ((charVal = fgetc(inputFile)) != EOF) {
    currentChar = (char) charVal;
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

      if (readBufferPos == bufferSize) {
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
  struct zip *epubFile = zip_open(fileName, 16, &error);
  bool useToc = false;

  if (epubFile == NULL) {
    printf("libzip error code \"%d\" - cannot open file \"%s\".\n", error, fileName);
    return false;
  }

  zip_int64_t tocIndex = zip_name_locate(epubFile, "content.opf", ZIP_FL_NOCASE|ZIP_FL_NODIR);

  if (tocIndex == -1) {
    tocIndex = zip_name_locate(epubFile, "toc.ncx", ZIP_FL_NOCASE|ZIP_FL_NODIR);
    if (tocIndex == -1) {
      printf("Cannot find information file in epub.\n");
      zip_discard(epubFile);
      return false;
    } else {
      useToc = true;
    }
  }

  struct zip_stat *tocStat = malloc(sizeof(struct zip_stat));
  if (zip_stat_index(epubFile, tocIndex, ZIP_FL_UNCHANGED, tocStat) == -1) {
    printf("libzip error, cannot read out %s file stats.\n", useToc ? "toc" : "content");
    zip_discard(epubFile);
    free(tocStat);

    return false;
  }


  struct zip_file *tocFile = zip_fopen_index(epubFile, tocIndex, ZIP_FL_UNCHANGED);
  if (tocFile == NULL) {
    printf("libzip error, cannot open %s file.\n", useToc ? "toc" : "content");
    zip_discard(epubFile);
    return false;
  }


  long unsigned int buffer = tocStat->size;
  char data[buffer];

  if (buffer != zip_fread(tocFile, data, buffer)) {
    printf("libzip error, size of data read of %s file estimate mismatch.\n", useToc ? "toc" : "content");
    return false;
  }

  //----------------------------------------------------------------------------

  if (useToc) {
    char currentChar = '\0';

    char readBuffer[bufferSize];
    unsigned int readBufferPos = 0;
    unsigned int dataPos = 0;
    bool doRecord = false;
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
        if (currentChar > 24) {
          readBuffer[readBufferPos++] = currentChar;

          if (readBufferPos == bufferSize) {
            readBufferPos = 0;
          }
        }
      }
    }

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
    fileData->entries[fileData->count] = malloc(sizeof(char) * (strlen(title)+1));
    strcpy(fileData->entries[fileData->count], title);

    fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
    fileData->types[fileData->count] = 1;
    ++fileData->count;

  } else {
    // Use content.opf

    char *readBuffer = calloc(buffer+1, sizeof(char));
    char key = '\0';
    unsigned int dataPos = 0;
    unsigned int readBufferPos = 0;

    bool inAttribute = false;
    bool inMeta = false;
    bool inXMLData = false;
    bool nextIsMeta = false;

    char *dataTitle = NULL;
    char *dataAuthor = NULL;
    char **dataPointer = NULL;

    while ((key = data[dataPos++]) != '\0') {
      if (!inAttribute && key == '<') {
        inAttribute = true;
        inXMLData = false;
        readBufferPos = 0;
      } else if (!inXMLData && key == '/') {
        inAttribute = false;
        inMeta = false;
        inXMLData = false;
        readBufferPos = 0;
      } else {
        readBuffer[readBufferPos++] = key;
      }

      if (inAttribute) {
        if (!inXMLData && !inMeta) {
          if (strncmp(readBuffer, "meta ", 5) == 0) {
            inMeta = true;
            readBufferPos = 0;
          }
        } else if (inMeta && !inXMLData) {
          if (key == ' ' || key == '\"') {
            readBuffer[readBufferPos-1] = '\0';
            if (strncmp(readBuffer, "dcterms:", 8) == 0) {
              nextIsMeta = true;
              char metaType[strlen(&readBuffer[8]) + 1];
              strcpy(metaType, &readBuffer[8]);

              //printf("Meta: %s\n", metaType);

              if (strcmp(metaType, "title") == 0) {
                dataPointer = &dataTitle;
                *dataPointer = (char*) malloc(buffer * sizeof(char));
              } else if (strcmp(metaType, "creator") == 0) {
                dataPointer = &dataAuthor;
                *dataPointer = (char*) malloc(buffer * sizeof(char));
              } else {
                dataPointer = NULL;
              }
            }

            readBufferPos = 0;
          }
        }

        if (!inXMLData && key == '>') {
          readBufferPos = 0;
          inXMLData = true;
        }

        if (inXMLData && key == '<') {
          inXMLData = false;
          if (nextIsMeta) {
            nextIsMeta = false;
            readBuffer[readBufferPos-1] = '\0';
            if (dataPointer != NULL) {
              strcpy(*dataPointer, readBuffer);
            }

            //printf("Metadata:\n%s\n", readBuffer);
          }

          readBufferPos = 0;
        }
      }
    }

    //--------------------------------------------------------------------------
    char title[dataTitle != NULL ? strlen(dataTitle)+8 : strlen(fileName)+8];
    if (dataTitle != NULL) {
      sprintf(title, "Title(%s)", dataTitle);
    } else {
      sprintf(title, "Title(%s)", fileName);
    }

    fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
    fileData->entries[fileData->count] = malloc(sizeof(char) * (strlen(title)+1));
    strcpy(fileData->entries[fileData->count], title);

    fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
    fileData->types[fileData->count] = 1;
    ++fileData->count;

    free(dataTitle);

    if (dataAuthor != NULL) {
      char author[strlen(dataAuthor)+8];
      sprintf(author, "Author(%s)", dataAuthor);
      fileData->entries = (char**) realloc(fileData->entries, sizeof(char*) * (fileData->count + 1));
      fileData->entries[fileData->count] = malloc(sizeof(char) * (strlen(author)+1));
      strcpy(fileData->entries[fileData->count], author);

      fileData->types = (int*) realloc(fileData->types, sizeof(int) * (fileData->count+1));
      fileData->types[fileData->count] = 2;
      ++fileData->count;

      free(dataAuthor);
    }
  }


  //----------------------------------------------------------------------------
  // Cleanup
  zip_discard(epubFile);
  free(tocStat);
  free(tocFile);

  //----------------------------------------------------------------------------

  return true;
}

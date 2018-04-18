#Makefile template from Lazyfoo Productions
#OBJS specifies which files to compile as part of the project
OBJS = kisslib.c kiss-front.c

#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
# -w suppresses all warnings
COMPILER_FLAGS = -Wall -std=c99 -O3 `pkg-config --cflags gtk+-3.0`

#LINKER_FLAGS specifies the libraries we're linking against
LINKER_FLAGS = -lzip -lsqlite3 `pkg-config --libs gtk+-3.0`

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = kissebook

INSTALL_PATH = /usr/share/kissebook

#This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)

install : all
						mkdir $(INSTALL_PATH)
						cp -r translations $(INSTALL_PATH)
						cp kissebook.png $(INSTALL_PATH)
						cp kissebook $(INSTALL_PATH)
						cp kissebook.desktop /usr/share/applications/

uninstall : $(INSTALL_PATH)
								rm -R $(INSTALL_PATH)

clean : $(OBJ_NAME)
						rm kissebook

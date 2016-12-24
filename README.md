# kisslib
A kiss principle ebook launcher for Unix.

### What it is
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an user-defined viewer for the filetype. While displaying general information about a ebook file like format, author and title.

### Usage
Currently there is no launch feature itself, this is early development. But its possible to import files and folders using the menu "Import files and folders" or by dragging and dropping files and or folders onto the ebook list, data then is processed in order to display information like author, title and file format when available of the supported ebook file formats.

The data is then stored into the sqlite3 database "kisslib.db" and is then again available if you open kisslib.

Supported file types are:
* pdf
* epub
* mobi
* chm

### Requirements
* GTK3
* libzip / libzipdev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* Unix like system
* gcc - to compile

### Compiling
Compile using gcc:

""gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

And then run "./kisslib" to open kisslib.

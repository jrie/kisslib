# kisslib
A kiss principle ebook launcher for Unix.

### What it is
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an user-defined viewer for the filetype. While displaying general information about a ebook file like format, author and title.

### Usage and file support

Currently there is no launch feature itself but there is a placeholder quick start icon which later on should launch the particular file, this is early development. But its possible to import files and folders using the menu "Import files and folders" or by dragging and dropping files and or folders onto the ebook list, data then is processed in order to display information like author, title and file format when available of the supported ebook file formats.

The data is then stored into the sqlite3 database "kisslib.db" and is then again available if you open kisslib.
Entries can be removed by pressing the "Del"-key.

Also the author and title fields can be edited and are saved instantly. In case a title is misleading, it is possible to get the actual file name into the field. To do so, the field has to be emptied (meaning to remove all text in it) and if there is no data present, the actual filename becomes added to the list cell. No data is stored in the database until another edit is made! Please be aware of that.

If the author field is cleared, the generel placeholder "Unknown" is set and directly saved to the database.

For a quick search in the collection, it is possible to use type ahead find on the titles. Meaning, searching the titles can be done by simply typing in the application going with the first letters of the title as displayed.

Supported file types are:
* pdf
* epub
* mobi
* chm

### Requirements
* libzip / libzipdev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* Unix like system (having GNU C extensions, for directory listing)
* GTK3 (GUI)
* gcc - to compile

### Compiling
Compile using gcc:

""gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

And then run "./kisslib" to open kisslib.

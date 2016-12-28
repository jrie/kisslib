# kisslib
A kiss principle ebook launcher for Unix.

### What it is
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an user-defined viewer for the filetype. While displaying general information about a ebook file like format, author and title.

### File support and usage

Supported file types which are handled are:
* pdf
* epub
* mobi
* chm

In order to make use of KISS Ebook, you have to first import any supported ebook files.
Its possible to import files and folders using the menu "Import files and folders" or by dragging and dropping files and or folders onto the ebook list, data then is processed in order to display information like author, title and file format when available of the supported ebook file formats.

The data is then stored into the sqlite3 database "kisslib.db" and is then again available if you open kisslib.
Entries can be removed by pressing the "Del"-key.

In order to quickstart any ebook file, you have to go under the menu "Operations" and select "Set launcher applications".
In this dialog, you can enter for each ebook type a program name, for example "evince" for .pdf files.

After saving and doing so once pdfs and other set then can be opened by clicking in the "Open icon" in the "Open" column of the particular ebook in the list or by pressing "Control-S" when the ebook is selected. If thats a pdf, "evince /home/tux/YourPDF.pdf" becomes executed and should happily open the PDF for you in this viewer.

Also the author and title fields can be edited and are saved instantly in the table view or by pressing "Control-E", this will open the edit ebook details dialog, which is also available in "Operations" > "Edit ebook details".

In case a title is misleading, it is possible to get the actual file name into the field when editing in the table view. To do so, the field has to be emptied (meaning to remove all text in it) and if there is no data present, the actual filename becomes added to the list cell. If the author field is cleared in the table view, the generel placeholder "Unknown" is set and directly saved to the database.

For a quick search in the collection, it is possible to use type ahead find on the titles. Meaning, searching the titles can be done by simply typing in the application going with the first letters of the title as displayed in KISS Ebook.

### Requirements
* libzip / libzipdev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* libgtk-3-dev (GUI)
* Unix like system (having GNU C extensions, for directory listing)
* gcc - to compile

### Compiling
Compile using gcc:

""gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

And then run "./kisslib" to open kisslib.

### Or to directly use kisslib
Download "kisslib" which is a ELF Linux binary and install the following dependencies to run it:
* libzip4
* libsqlite3-0
* libgtk-3-0

And then start using "./kisslib".

### Known issues
* kisslib is known not to run on Debian Jessie at present

As only libzip2 is present and a version of GTK3 which is not compatible with the current code too, the easiest solution at present would be, to upgrade to Stretch/Testing of Debian. But I will try to release a version, which is compatible to those libraries provided in Jessie, if possible.


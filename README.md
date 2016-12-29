# kisslib
A kiss principle ebook quick launcher application for Linux

### What it is
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an user-defined viewer for the filetype. While displaying general information about a ebook file like format, author and title.

### Screenshot gallery
A gallery with screenshots (also development steps), newest first, can be found here:
https://www.picflash.org/gallery.php?id=9RGDIIE7K8

The screenshots are taken under Debian Sid using Xfce desktop and Debian Testing with Xfce Defaults.

### Tested working on
* Debian Sid/Unstable and Stretch/Testing (Tested @ 28.12.2016)
* Debian Jessie/Stable requires "kisslib-jessie" or related "kisslib-jessie.c" due to earlier library versions (28.12.2016)
* more to come...

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

Please note, launcher applications can have now parameters added to them, for example "evince -f" would start evince in fullscreen when launched.

Also the author and title fields can be edited and are saved instantly in the table view or by pressing "Control-E", this will open the edit ebook details dialog, which is also available in "Operations" > "Edit ebook details".

In case a title is misleading, it is possible to get the actual file name into the field when editing in the table view. To do so, the field has to be emptied (meaning to remove all text in it) and if there is no data present, the actual filename becomes added to the list cell. If the author field is cleared in the table view, the generel placeholder "Unknown" is set and directly saved to the database.

For a quick search in the collection, it is possible to use type ahead find on the titles. Meaning, searching the titles can be done by simply typing in the application going with the first letters of the title as displayed in KISS Ebook.

### Requirements
* libzip4/libzip-dev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* libgtk-3-dev (GUI)
* Unix like system (having GNU C extensions, for directory listing)
* gcc - to compile

### Compiling
Compile using gcc:

""gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

If you want to link against libzip2+libzipdev and an a earlier version of GTK3 (if you run Debian Jessie for example) - you can, but you have to run the following command:

""gcc kisslib-jessie.c kiss-front-jessie.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

Then "chmod +x kisslib" to allow the system to execute it.
And then run "./kisslib" to open kisslib.


### Or to directly use kisslib (not Debian Jessie)
Download "kisslib" which is a Linux binary and install the following dependencies to run it:
* libzip4
* libsqlite3-0
* libgtk-3-0

Then "chmod +x kisslib" to allow the system to execute it.
And then start using "./kisslib".


### Debian Jessie
Download "kisslib-jessie" which is a Linux binary.

* libzip2
* libsqlite3-0
* libgtk-3-0

Then "chmod +x kisslib-jessie" to allow the system to execute it.
And then start using "./kisslib-jessie".


### Known issues
* If there are any database errors, you most likely once need to delete the "kisslib.db" so that all sqlite tables are properly created (due to changes for the launcher applications) - this is a one time action, but youve got to reimport your data

* In LXDE/using Openbox in Debian Jessie, the dialog windows for file import, edit ebook details and set launcher applciations are all opened but hidden behind the main window. Please note that when those windows are open, the main application state is set to background and not handle input at this time, so you must close the dialog in order to do anything in kisslib

# kisslib
A kiss principle ebook quick launcher application for Linux systems

## Whats the deal?
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files, works completely offline and helps you manage and launch your ebooks from a single place, giving you hints about the authors and tiles and the format of your ebooks.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an own defined viewer (if you like with commands) for the supported ebook types. While displaying general information about a ebook file like format, author and title.

### But why? There are tons of good readers, like XY...
The motivation to write this application is simple. Most programs read out particular formats or maybe dont support all of them, so you need a special applciation to handle pdf, but not the same to handle chm files.

Kiss Ebook Starter simply allows you to read all supported ebook formats into Kiss Ebook and from there, launch the particular application you want, with the settings/commands you provide, for example opening all pdfs in full screen if you like that, if that is a feature supported by the viewer - for example through a command line option.

Kiss Ebook shows you therefore the format, the authors and the titles of the ebooks as possible, in order to allow you to quickly find a particular ebook and then, launch it, either by double clicking a starter icon or by using a keyboard shortcut.

You also can, edit the authors and details inside Kisslib, which allow you a quicker navigation. The ebook itself is untouched in this process and those informations are only stored in the database.

## Show me! - Screenshot gallery
A gallery with screenshots (also development steps), newest first, can be found here:
https://www.picflash.org/gallery.php?id=9RGDIIE7K8

The screenshots are taken under Debian Sid using Xfce desktop and Debian Testing with Xfce Defaults.

## Where does it work? - Tested working on...
* Debian Sid/Unstable and Stretch/Testing (Tested 28.12.2016)
* Debian Jessie/Stable requires "kisslib-jessie" or related "kisslib-jessie.c" due to earlier library versions, please see known issues (Tested 28.12.2016)
* Fedora Workstation 25 - and can run with the "kisslib" binary (Tested 29.12.2016)
* CentOS 7 using "kisslib-jessie" and related files, please see known issues (Tested 29.12.2016)
* openSuse Tumbleweed (Tested 30.12.2016)
* your favourite Linux distro here! (please get in touch and I try to give my best to get it working)

## Can I..., is it easy to use? - File support and usage guide

Supported file types which are handled are:
* pdf
* epub
* mobi
* chm

In order to make use of KISS Ebook, you have to first import any supported ebook files.
Its possible to import files and folders using the menu "Import files and folders" or by dragging and dropping files and or folders onto the ebook list, data then is processed in order to display information like author, title and file format when available of the supported ebook file formats, for pdf-files if no title is found, the filename is used in the list view.

The data is then stored into the sqlite3 database "kisslib.db" and is then again available if you open kisslib.
Entries can be removed by pressing the "Del"-key.

In order to quickstart any ebook file, you have to go under the menu "Operations" and select "Set launcher applications".
In this dialog, you can enter for each ebook type a program name, for example "evince" for .pdf files.

After saving and doing so once pdfs and other set then can be opened by clicking in the "Open icon" in the "Open" column of the particular ebook in the list or by pressing "Control-S" when the ebook is selected. If thats a pdf, "evince /home/tux/YourPDF.pdf" becomes executed and should happily open the PDF for you in this viewer.

Please note, launcher applications can have now parameters added to them, for example "evince -f" would start evince in fullscreen when launched.

Also the author and title fields can be edited and are saved instantly in the table view or by pressing "Control-E", this will open the edit ebook details dialog, which is also available in "Operations" > "Edit ebook details".

In case a title is misleading, it is possible to get the actual file name into the field when editing in the table view. To do so, the field has to be emptied (meaning to remove all text in it) and if there is no data present, the actual filename becomes added to the list cell. If the author field is cleared in the table view, the generel placeholder "Unknown" is set and directly saved to the database.

For a quick search in the collection, it is possible to use type ahead find on the titles. Meaning, searching the titles can be done by simply typing in the application going with the first letters of the title as displayed in KISS Ebook.

## I want to use it, not to compile! Heres how:
Download "kisslib" which is a Linux 64bit binary and install the following dependencies to run it, please note that the library names may wary on your distribution.

* libzip4 
* libsqlite3
* libgtk-3-0

Then "chmod +x kisslib" to allow the system to execute it.
And then start using "./kisslib".

### Debian Jessie
Download "kisslib-jessie" which is a Linux 64bit binary.

* libzip2
* libsqlite3
* libgtk-3-0

Then "chmod +x kisslib-jessie" to allow the system to execute it.
And then start using "./kisslib-jessie".

## Compiling

## Requirements for compilation (kisslib non "jessie" version)
* libzip4/libzip-dev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* libgtk-3-dev (GUI)
* Linux like system (having GNU C extensions, for directory listing and exec)
* gcc - to compile

Compile using gcc:

""gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

If you want to link against libzip2+libzipdev and an a earlier version of GTK3 (if you run Debian Jessie or want to compile the "kisslib-jessie.c" and "kisslib-front-jessie.c" for example) - you can, but you have to run the following command:

""gcc kisslib-jessie.c kiss-front-jessie.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib""

Then "chmod +x kisslib" to allow the system to execute it.
And then run "./kisslib" to open kisslib.


## Known issues
* If there are any database errors, you most likely once need to delete the "kisslib.db" so that all sqlite tables are properly created (due to changes for the launcher applications) - this is a one time action, but youve got to reimport your data

* In Debian Jessie except when using Xfce and in CentOS, the dialog windows for file import, edit ebook details and set launcher applciations are all opened but hidden behind the main window. Please note that when those windows are open, the main application state is set to background and not handle input at this time, so you must move the main window away in order to reach the dialogs

## ToDo and plans
* Introduce the usage of "content.opf" for epub reading, which includes the ebook title and the author(s)
* Add a option dialog, which has at least one option/setting (turned on by default) to ignore read out of already read out files
* Add the ability to sort by format, author and title
* Add a option to sort by default on format, authors or title on startup
* Add a search option to only show files with particular tile, author or format
* your feature suggestion (please contact me by email)

# kisslib
A kiss principle ebook quick launcher application for Linux systems

#### Quick navigation

- [What is it about?](https://github.com/jrie/kisslib#whats-is-it-about)
- [But why? Doesnt ebook reader XY do that?](https://github.com/jrie/kisslib#but-why-there-are-tons-of-good-readers-like-xy-the-motivation-behind-kiss-ebook)
- [Show me! - Screenshot gallery link and previews](https://github.com/jrie/kisslib#show-me---screenshot-gallery)
- [Where does it work? - Tested on...](https://github.com/jrie/kisslib#where-does-it-work---tested-working-on)
- [I want to use it! - Not to compile...](https://github.com/jrie/kisslib#i-want-to-use-it-not-to-compile-heres-how)
- [Debian Jessie or Distros with older libraries (Jessie Version code)](https://github.com/jrie/kisslib#debian-jessie-or-distros-with-older-libraries)
- [Can I? - Is it easy to use? - File support and usage guide](https://github.com/jrie/kisslib#can-i-is-it-easy-to-use---file-support-and-usage-guide)
- [Compiling](https://github.com/jrie/kisslib#compiling)
- [Compiling of the "jessie" versions, not only Debian Jessie](https://github.com/jrie/kisslib#compilation-of-the-jessie-version-not-only-debian-jessie)
- [ToDos and future plans / Wishlist](https://github.com/jrie/kisslib#todo-and-future-plans--wishlist)
- [Known issues](https://github.com/jrie/kisslib#known-issues)


## Whats is it about?
kisslib aka "KISS Ebook" or "KISS Ebook Starter" is a work in progress ebook launcher supporting .pdf, .epub, .mobi, .chm ebook files, works completely offline and helps you manage and launch your ebooks from a single place, giving you hints about the authors and tiles and the format of your ebooks.

Its not a ebook viewer of any kind, its whole purpose is to allow to quickly go through an ebook collection and launching an own defined viewer (if you like with commands) for the supported ebook types. While displaying general information about a ebook file like format, author and title.

### But why? There are tons of good readers, like XY... the motivation behind KISS Ebook
The motivation to write this application is simple. Most programs read out particular formats or maybe dont support all of them, so you need a special program to handle pdf ebooks, but not the same to handle chm files.

Kiss Ebook Starter simply allows you to read all supported ebook formats into Kiss Ebook and from there, launch the particular application you want, with the settings/commands you provide, for example opening all pdfs in full screen if you like that, if that is a feature supported by the viewer - for example through a command line option.

Kiss Ebook shows you therefore the format, the authors and the titles of the ebooks as possible, in order to allow you to quickly find a particular ebook and then, launch it, either by double clicking a starter icon or by using a keyboard shortcut.

You also can, edit the authors and details inside Kisslib, which allow you a quicker navigation. The ebook itself is untouched in this process and those informations are only stored in the database.

Its possible to filter the ebook list for ebooks. In order to do so, you use the search field. When you enter something and hit enter/return key, only ebooks where the titles and or authors match are shown in KISS Ebook. Sorting is still possible then. If you want to get rid of the filter again, you clear all text and hit Enter again.
If you see the clear icon in the search bar right, a default GTK3 icon to clear a search, you can press this and then normal list, without any filter, is shown. This icon is not shown on any flavour so, unfortunately.

Please also see the future plans and ToDo list of what Kiss Ebook should be able to do in the future, to aid you in managing your ebook collection(s).


## Show me! - Screenshot gallery
The screenshots are taken under Debian Stretch using Xfce desktop with default settings and the dark screenshots with Debian Sid with Xfce Breeze theme.

A gallery with screenshots (also development steps), newest first, can be found here:
https://www.picflash.org/gallery.php?id=9RGDIIE7K8

Examples of KISS Ebook:

Main interfance (dark 'Breeze' theme):

[![Screenshot of main interface with search](https://www.picflash.org/img/2016/12/31/TBffvpana7qjimmfj.png "Main interface with integrated search")] (https://www.picflash.org/viewer.php?img=ffvpana7qjimmfj.png)

Screenshot during import:

[![Screenshot Kiss Ebook during import](https://www.picflash.org/img/2016/12/22/TB2e8osytx7rywios.png "Screenshot Kiss Ebook during import")] (https://www.picflash.org/viewer.php?img=2e8osytx7rywios.png)


And on default Xfce with base theme on Debian Testing, older screenshot without search:

[![Screenshot Kiss Ebook with opened edit ebook details-dialog](https://www.picflash.org/img/2016/12/29/TBtiobsygvs14u1mu.png "Screenshot Kiss Ebook with opened edit ebook details-dialog")] (https://www.picflash.org/viewer.php?img=tiobsygvs14u1mu.png)


## Where does it work? - Tested working on...
* Debian Sid/Unstable and Stretch/Testing "kisslib" binary (Tested 19.01.2017)
* Debian Jessie/Stable requires "kisslib-jessie" (Tested 19.01.2017)
* Fedora Workstation 25 - and can run with the "kisslib" binary (Tested 21.01.2017)
* CentOS 7 using "kisslib-jessie" - there is a issue with GTK+ style "oxygen-gtk" (Tested 19.01.2017)
* openSuse Tumbleweed (Tested 18.01.2017)
* Sparky linux using "kisslib" binary (Tested 30.12.2016)
* Ubuntu 16.10 (Yakkety Yak) "kisslib" binary, working - there is just a note in "known issues" (Tested 19.01.2017)
* Arch OS "kisslib" binary (Tested 18.01.2017)
* your favourite Linux distro here! (please get in touch and I try to test it if it runs on your flavour)


## I want to use it, not to compile! Heres how:
Download "kisslib" which is a Linux 64bit binary and install the following dependencies to run it, please note that the library names may wary on your distribution.

* libzip4 
* libsqlite3
* libgtk-3-0

Then give kisslib executeable permissions and start it using:
```
chmod +x kisslib
./kisslib
```


### Debian Jessie or distros with older libraries
Please see also the known issues at the bottom, as there are slight issues with this version.

Download "kisslib-jessie" which is a Linux 64bit binary.

* libzip2
* libsqlite3
* libgtk-3-0

Then give kisslib executeable permissions and start it using:
```
chmod +x kisslib-jessie
./kisslib-jessie
```


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

Please note, launcher applications can have now parameters added to them, for example "evince -f" would start evince in fullscreen when launched with all files associated with this command.

Also the author and title fields can be edited and are saved instantly in the table view or by pressing "Control-E", this will open the edit ebook details dialog, which is also available in "Operations" > "Edit ebook details".

In case a title is misleading, it is possible to get the actual file name into the field when editing in the table view. To do so, the field has to be emptied (meaning to remove all text in it) and if there is no data present, the actual filename becomes added to the list cell. If the author field is cleared in the table view, the generel placeholder "Unknown" is set and directly saved to the database.

Also the columns which are shown, can be set temporary in the "View" > "Show or hide columns" menu or permanently by using "Options" dialog, selecting or deselecting the desired columns to display and saving.

In the "Options" dialog, it is also possible to set if ebooks on import of existing filenames should be overwritten. Resetting the author(s) and title columns, but leaving all other data intact.

For a quick search in the collection, it is possible to use type ahead find on the titles. Meaning, searching the titles can be done by simply typing in the application going with the first letters of the title as displayed in KISS Ebook.

Shortcuts:
* "Ctrl-S" (Open a ebook)
* "Ctrl-E" (Edit ebook details)
* "Ctrl-W" (Set launcher dialog)
* "Ctrl-A" (Add files or folders dialog)

## Compiling

### Requirements and compilation (kisslib non "jessie" version)
* libzip4/libzip-dev for compiling (used for epub support)
* libsqlite3-dev (as datastore)
* libgtk-3-dev (GUI)
* Linux like system (having GNU C extensions, for directory listing and exec)
* gcc - to compile

Compile using gcc:

```gcc kisslib.c kiss-front.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib```


Then "chmod +x kisslib" to allow the system to execute it.
And then run "./kisslib" to open kisslib.


### Compilation of the "jessie" version (not only Debian Jessie!)

The same may wary slightly, depending on your distro.

If you want to link against an earlier version, you need:
* libzip-dev/libzip2
* libsqlite3-dev

```gcc kisslib-jessie.c kiss-front-jessie.c -Wall -std=c99 -O3 -g `pkg-config --cflags gtk+-3.0` -lzip -lsqlite3 `pkg-config --libs gtk+-3.0` -o kisslib-jessie```

Then "chmod +x kisslib-jessie" to allow the system to execute it.
And then run "./kisslib-jessie" to start kisslib.


## ToDo and future plans / Wishlist

Striked out items are implemented already and thank you for all your feedback so far who contributed!

I try to make things happen, if possible.

* ~~Introduce the usage of "content.opf" for epub reading, which includes the ebook title and the author(s)~~
* ~~Add a option dialog, which has at least one option/setting (turned on by default) to ignore read out of already read out files~~
* ~~Add the ability to sort by format, author and title columns~~
* ~~Add a option to sort by default on format, authors or title on startup~~
* ~~Add a search option to only show files with particular title and author~~
* ~~Add a check to detect "default readers" installed, which can access particular ebook formats, which are then set as default launchers~~ (please propose your favorite readers by raising an issue or by email)
* ~~Using "gettext" to make translations of the application available~~ (translation is possible now, please raise an issue or email me if you would like to make any translations of KISS Ebook
* Try to set colors for particular ebook genres in the ebook overview list, for example blue for "IT" books, and any other color for "romance" books, like red or any other choosen color, perhaps pre-defined color values to pick from
* ~~Add the option to show/hide particular columns from the main view, for example by using a popup or in an options dialog~~
* ~~Add tags, so ebooks can be tagged with short terms to make a search for particular books easier~~
* ~~Add a checked column for "books already read" in the overview which can set or unset a book as read and which can be filtered too~~
* ~~Add a priority list, for example ranging from -10 to 0 to 10 in order to set a priority list, by default books should be added with 0, make this orderable like other columns~~
* Add parsing/read out of cover images from ebooks, showing a thumbnail in "edit ebook details" dialog
* Add a GtkAppChooserDialog to select a launcher for a particular ebook file type
* ~~Add a switch of what columns to search in using the search bar~~
* ~~Make the columns reorderable and save there state on exit - in addition with an option to reset the column order to default~~ Column order can be reset in "View" > "Reset column order"
* your feedback and feature suggestions! (please open an issue or contact me by email)


## Known issues

##### General
* If you upgrade from a previous version and there are any sql errors while saving the launcher applications or importing files, you most likely once need to delete the "kisslib.db" so that all sqlite tables are properly recreated (due to changes for the launcher applications and read out ebook information table) - this is a one time action, but youve got to reimport your ebooks and reenter the launcher applications after this step
* found another issue? (please report it or email me)


##### Ubuntu 16.10

Using the "kisslib" binary version:

* The open icon "document-open" is not present in Ubuntu and is instead as icon rendered as a non checkable checkbox which can be double clicked to open an ebook, this on purpose
* found another issue? (please report it or email me)


##### Cent OS 7

Using the "kisslib-jessie" binary version:

* The sort column select box in "Options" > "Edit options" > "Sort column on startup:" - does get stuck when using GTK style "oxygen-gtk" and a mouse selection is not possible in my test, to close the selection use the "Up" or "Down" key or "Enter" in this case, other styles dont seem to be affected when tested
* found another issue? (please report it or email me)


##### Arch Linux

Using the "kisslib" binary version:

~~* The "LIMIT" clause in UPDATE and DELETE statements, which are used in KISS Ebook, are not supported in reason of compile options for sqlite(3) in Arch, some operation like delete and updates (splash screen for example) result in an SQL error which is printed in the command line/shell~~
* found another issue? (please report it or email me)

There is a issue in the bugtracker of Arch requesting possible support for LIMIT in UPDATE and DELETE sql statements: [Arch Bugtracker - Issue] (https://bugs.archlinux.org/task/52584)


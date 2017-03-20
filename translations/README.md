## Helping to translate

If you would like to translate KissEbook into your language, you are more than welcome to help!

It shouldnt be too complicated, but you might need to read up on some things first.

### How to translate

There is a decent guide to be found here:

[A guide on how to use gettext but also on how to create translations](http://www.mirrorservice.org/pub/NetBSD/NetBSD-release-6/src/gnu/dist/gettext/gettext-tools/doc/tutorial.html)

Recommended is also the usage of POEdit which should be available on all distros. In order to translate, you can use the "KISSebook.pot" located in the application folder.


For example, to create a polish translation, the folder structure should be as follows:

- translations (the root folder where gettext in our case looks for translations)
- language code and country code: pl_PL
- /LC_MESSAGES/
- KISSebook.mo


Which sums up to:

translations/pl_PL/LC_MESSAGES/KISSebook.mo


The ISO language codes can be found, in example, here:
http://www.lingoes.net/en/translator/langcode.htm

Note: All codes have to be written with underscore instead of minus, so instead of "de-DE" from this page, it would be "de_DE"!

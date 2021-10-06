=====
TITLE:

Filer


=====
CATCHY_TAGLINE:

File filer for filing file files.


=====
USAGE:

./filer <filename>


=====
SUMMARY:

This program will take a file in a format such as this one and parse it and
provide navigation options. In such a file, sections are separated by a token
of 5+ '=' characters on their own line, followed by a line with a label for the
section.

Try running this readme through it.

The program will display a menu with options for listing out the sections,
showing the contents of a given section, and running through a test block on
the given file. The file to process should be specified on the command line as
the sole argument to the filer program.


=====
PIECES_OF_INTEREST:

(TODO:
(file content buffer which is used if file fits within a certain size or can be manually disabled)
(state machine for finding locations of file elements; I thought it was cool, at least)
(the structs for handling file data -- would be nice to explain more about those)
(get_user_number is pretty slick, idk)
)


=====
TODO:

Make things more organized, both in code and output.
Make the UI more sensible to use.
More extensive and consistent error checking.
Etc.


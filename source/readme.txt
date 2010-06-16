LabVIEW ZIP library, version 2.5.2
----------------------------------

Copyright 2002-2009 Rolf Kalbermatter


Please read this document before you upgrade from a version of LVZIP
older than 2.2 to this version.

Version 2.2 of LVZIP has some modifications to the VI connector panes
in comparison to earlier versions which might require you to slightly redo
your project.

Release 2.6.1, Released: Jun. xx, 2010
======================================

New features:
-------------
1) Upgraded to zlib 1.2.5 and minizip 1.1 sources
2) Support for ZIP64 Archives (>4GB) (an individual file inside the archive still
   needs to be smaller than 4GB when getting added or extracted and memory stream
   based retrieval is limited to < 2GB large archives due to LabVIEWs 2GB limit for
   string handles)
3) Support for LabVIEW for 64 Bit


Release 2.5.2, Released: Sep. 3, 2009
=====================================

Bug fixes:
----------
1) Fixed the use of LabVIEW config file VIs that are now considered private in 2009.


Release 2.5.1, Released: April 28, 2009
=======================================

New features:
-------------
1) Optimized ZLIB Extract All Files To Dir.vi and ZLIB Delete Files From Archive.vi to
not index files uneccessarily to speed up the operation considerably. 

Bug fixes:
----------
1) Fixed palette files to include the polymorphic icons.


Release 2.4.1, Released: Jan. 2, 2009
=====================================

New features:
-------------
1) Support for ZIP and UNZIP streams directly located in memory contrary to
requiring disk based files at all times.

2) Support for adding to and extracting from an archive directly memory based
streams without requiring to turn them into files first.

3) Minor tweaks to support compilation for LabVIEW realtime targets. This is
still considered an experimental feature as testing of this was not really
possible due to lack of available hardware. The standard Windows DLL should
work for all Pharlap based RT systems so far. LabVIEW 7.1, 8.2 and 8.5 were
checked. Added support for vxWorks based RT targets for LabVIEW 8.2 and
8.5/8.6.

Note, that for VxWorks based targets you have to copy the lvzlib.out file found
typically under "user.lib/_OpenG.lib/lvzip" manually to your RT target using
some FTP utility to copy this file into the "ni-rt/system" folder. LabVIEW will
currently not deploy or download that file automatically for those targets.

Bug fixes:
----------
1) Fixed a problem in ZIP Open File.vi where one could not open an empty
already created archive for appending new files.

2) Fixed a problem in ZLIB Delete Files From Archive.vi to use a correct
temporary filename for the intermediate archive file.

3) Fixed a problem in ZLIB Store File.vi to also wirk for LabVIEW files
located inside an LLB.

4) Fixed a potential problem with uninitialized memory in a function in
the shared library that could cause ZLIB Get Global Info to error on an
not properly closed archive.

Removed support:
----------------
1) Removed MacOS 9 Shared library.


Release 2.3.2, Released: Feb. 6, 2007
=====================================

Bug fix:
--------
1) changed incorrect dependency on dynamicpalette to >= 0.2


Release 2.3.1, Released: Sep. 23, 2006
======================================

New features:
-------------
1) Transparent MacBinary support on the Macintosh (experimental feature)

If the compression routine encounters a Macintosh file with a resource fork
the file is automatically encoded using the MacBinary algorithme before it
is actually compressed. On extraction of such files on the Macintosh the
file is restored with its resource fork. On non-Macintosh platforms extracting
a MacBinary file will result in the data fork being stored in the original
file and the resource fork being stored in the filename extended with the
.rsrc file ending.

2) LabVIEW 8 uncovered a problem when unextracting read-only files. The
application of the original creation and modification time to such files
failed now. This was fixed by always creating the extracted file with write
access and only change it to read-only after the original creation and
modification time has been applied.


Release 2.2, (never really properly released)
=============================================

New features:
-------------

1) Password support (limited testing)

ZLIB Extract All Files To Dir.vi
ZLIB Compress Directory.vi
ZLIB Compress Files.vi
ZLIB Store File.vi
ZLIB Extract File.vi

A new string parameter was added to specify a password to use to extract
files or to encrypt files. This function is not thouroghly tested at this
time.


2) Adding of files into existing ZIP archive supported

ZLIB Compress Directory.vi
ZLIB Compress Files.vi
ZLIB Open Archive.vi

The boolean parameter to append or truncate has been replaced by an
enumeration to support addition of files into an existing archive.
The old append value meant that the ZIP file was tacked to the end
of the existing file which might have been useful for a selfextracting
executable only.

Following table shows the old and new settings

old value   new value          remarks

False       create new         truncates existing file to 0
True        append to end      appends to end of existing file
NA          append to archive  appends new files into the archive


3) Deleting of files from an existing ZIP archive.

ZLIB Delete Files From Archive.vi

This function will create a new archive and move all files except the
ones to remove into this new archive, replacing the old one with it if
the entire transfer was successful. The transfer between the old and
new archive happens in "raw mode" which means the data is transfered
compressed into the new file, speeding up the whole operation but even
more importantly eliminating the need to know the password for any of
the files in the archive for mere deleting of a file.
Of course the files in the new archive will be password protected just
as they have been in the old file.


Changes:
--------

1) To support the new append mode the "append" parameter for the
compression functions has been modified from a boolean to an
enumeration. This could cause bad wires when this function was
used in a program and this parameter was wired with a constant
or terminal. Since the non-default value was rarely useful, this
should not be a big problem for most users as it is unlikely this
parameter was ever wired.

2) To support specifying a password when compressing into an archive,
the connector pane of a few VIs had to be changed. You will need to
relink existing applications if you have used following four functions:

ZLIB Extract All Files To Dir.vi
ZLIB Compress Directory.vi
ZLIB Compress Files.vi
ZLIB Store File.vi

3) To simplify the interface of the file info structure the according
cluster was modified to be more in LabVIEW style. This caused the
change to the connector pane of two VIs, namely:

ZLIB Get Current File Info.vi
ZLIB Enumerate File Contents.vi

and the according strict typedef custom control

ZLIB File Info.ctl

The old VIs and custom control with the old connector pane are stored
under the same name but with the appendix "Old". This is a list of the
compatibility functions:

ZLIB Get Current File Info Old.vi
ZLIB Enumerate File Contents Old.vi
ZLIB File Info Old.ctl

If you have an older application where you directly used one of the
VIs described here from the old lvzip package you can either replace
those VIs by the compatibility VIs or rewire your application to work
with the new functions.

5) I was successful in making the "File Info.vi" function from the file
package work again. In doing so the "ZLIB Time Info.vi" function, which
really was a subset implementation of "File Info.vi" is now obsolete and
has been removed both from the VI library as well as the shared library.

If you should have happened to use this function you should use the
according File Info.vi from the file package instead and adapt your
cluster wiring.

Used libraries:
---------------

The lvzip package makes use of the error package, the file package and through
the last one also of the array package. Some functions in the file package have
been added in comparison to earlier released packages and therefore this
version of lvzip will only work with the version 2.5 of the file package.

Have fun

Rolf Kalbermatter

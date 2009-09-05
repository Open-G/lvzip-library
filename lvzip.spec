[Package Name]
Name=oglib_lvzip
Version=2.5.1
Release=1

[Description]
Description="The lvzip package contains several routines for operating on zip files.\0D\0A\0D\0AVersion 2.5.1: Fixes issue with broken VIs in LabVIEW 2009 due to calling (now) private config utilities.\0D\0AVersion 2.5: Changes license of LabVIEW sources from LGPL to BSD, Adds Memory Stream VIs to the palettes, Optimizes ZLIB Extract All Files to Dir, Fixes potential problem where error such as out of disk on finishing to add a file into an archive might get lost meaning that a corrupted archive could result without the user being informed about it, Fixes issue preventing adding files specified by UNC network path to zip archive.\0D\0AVersion 2.4 adds support for direct memory stream handling.\0D\0AVersion 2.3 adds support for transparent Mac Resource file handling through MacBinary encoding.\0D\0AVersion 2.2 adds support for appending files to an existing archive, deleting files from an archive and password support for adding and extracting files."

Summary="OpenG Zip Tools"
License="BSD (VIs), LGPL (Shared Library)"
Copyright="1995-2004 Mark Adler, Jean-loup Gailly\0A1998-2004 Gilles Vollant\0A2002 - 2009 Christophe Salzmann, Jim Kring, Rolf Kalbermatter"
Distribution="OpenG Toolkit"
Icon=lvzip.bmp
Vendor=OpenG.org
URL=http://wiki.openg.org/oglib_lvzip
Packager="Jim Kring <jim@jimkring.com>"

[Script VIs]
PostInstall=PostInstall.vi
PreUninstall=PreUninstall.vi

[Dependencies]
Requires="ogrsc_dynamicpalette >= 0.2, oglib_error >= 2.0, oglib_file >= 2.5"
AutoReqProv=FALSE

[Platform]
Exclusive_LabVIEW_Version= >=6.0
Exclusive_LabVIEW_System=All
Exclusive_OS=All

[Files]
Num File Groups=8

[File Group 0]
Source Dir=built/lvzip
Target Dir=<user.lib>/_OpenG.lib/lvzip
Replace Mode=Always

Num Files=83

File 0=macbin.mnu
File 1=readme.txt
File 2=unzip.mnu
File 3=zip.mnu
File 4=zlib-string.mnu
File 5=macbin.llb/DecodemacBinary__ogtk.vi
File 6=macbin.llb/EncodemacBinary__ogtk.vi
File 7=macbin.llb/MACBIN CCITT_CRC16__ogtk.vi
File 8=macbin.llb/MACBIN Copy Fork From File__ogtk.vi
File 9=macbin.llb/MACBIN Copy Fork To File__ogtk.vi
File 10=macbin.llb/MACBIN Create Header__ogtk.vi
File 11=macbin.llb/MACBIN Decode MacBinary__ogtk.vi
File 12=macbin.llb/MACBIN Encode MacBinary__ogtk.vi
File 13=macbin.llb/MACBIN Has Resource Fork__ogtk.vi
File 14=macbin.llb/MACBIN Macbinary Header__ogtk.ctl
File 15=macbin.llb/MACBIN Open File Refnum__ogtk.vi
File 16=macbin.llb/MACBIN Read Header__ogtk.vi
File 17=macbin.llb/MACBIN Resource File Info Core__ogtk.vi
File 18=macbin.llb/MACBIN Resource File Info__ogtk.vi
File 19=macbin.llb/MACBIN Resource Header__ogtk.ctl
File 20=macbin.llb/MACBIN Size Padding__ogtk.vi
File 21=macbin.llb/MACBIN Verify Header__ogtk.vi
File 22=macbin.llb/MACBIN VI Tree__ogtk.vi
File 23=lvzip.llb/ZLIB Block Size__ogtk.vi
File 24=lvzip.llb/ZLIB Close Read File__ogtk.vi
File 25=lvzip.llb/ZLIB Close Unzip Archive__ogtk.vi
File 26=lvzip.llb/ZLIB Close Write File__ogtk.vi
File 27=lvzip.llb/ZLIB Close Zip Archive__ogtk.vi
File 28=lvzip.llb/ZLIB Common Path to Specific Path__ogtk.vi
File 29=lvzip.llb/ZLIB Compress Directory__ogtk.vi
File 30=lvzip.llb/ZLIB Compress File Info__ogtk.ctl
File 31=lvzip.llb/ZLIB Compress Files__ogtk.vi
File 32=lvzip.llb/ZLIB Convert File Info__ogtk.vi
File 33=lvzip.llb/ZLIB Copy Raw File__ogtk.vi
File 34=lvzip.llb/ZLIB CRC32__ogtk.vi
File 35=lvzip.llb/ZLIB Deflate__ogtk.vi
File 36=lvzip.llb/ZLIB Delete Files From Archive__ogtk.vi
File 37=lvzip.llb/ZLIB Enumerate File Contents Old__ogtk.vi
File 38=lvzip.llb/ZLIB Enumerate File Contents__ogtk.vi
File 39=lvzip.llb/ZLIB Extract All Files To Dir__ogtk.vi
File 40=lvzip.llb/ZLIB Extract File__ogtk.vi
File 41=lvzip.llb/ZLIB Extract Stream__ogtk.vi
File 42=lvzip.llb/ZLIB Extract__ogtk.vi
File 43=lvzip.llb/ZLIB File Info Old__ogtk.ctl
File 44=lvzip.llb/ZLIB File Info__ogtk.ctl
File 45=lvzip.llb/ZLIB File Information__ogtk.vi
File 46=lvzip.llb/ZLIB Get Current File Info Old__ogtk.vi
File 47=lvzip.llb/ZLIB Get Current File Info__ogtk.vi
File 48=lvzip.llb/ZLIB Get File CRC32__ogtk.vi
File 49=lvzip.llb/ZLIB Get File__ogtk.vi
File 50=lvzip.llb/ZLIB Get Global Info__ogtk.vi
File 51=lvzip.llb/ZLIB Get Version__ogtk.vi
File 52=lvzip.llb/ZLIB Go To First File__ogtk.vi
File 53=lvzip.llb/ZLIB Go To Next File__ogtk.vi
File 54=lvzip.llb/ZLIB Inflate__ogtk.vi
File 55=lvzip.llb/ZLIB Initialize File Functions__ogtk.vi
File 56=lvzip.llb/ZLIB Initialize Stream Functions__ogtk.vi
File 57=lvzip.llb/ZLIB Locate File__ogtk.vi
File 58=lvzip.llb/ZLIB Move Raw File__ogtk.vi
File 59=lvzip.llb/ZLIB Open Read File__ogtk.vi
File 60=lvzip.llb/ZLIB Open Unzip Archive__ogtk.vi
File 61=lvzip.llb/ZLIB Open Unzip Stream__ogtk.vi
File 62=lvzip.llb/ZLIB Open Unzip__ogtk.vi
File 63=lvzip.llb/ZLIB Open Write File__ogtk.vi
File 64=lvzip.llb/ZLIB Open Zip Archive__ogtk.vi
File 65=lvzip.llb/ZLIB Open ZIP Mode__ogtk.ctl
File 66=lvzip.llb/ZLIB Open Zip Stream__ogtk.vi
File 67=lvzip.llb/ZLIB Open Zip__ogtk.vi
File 68=lvzip.llb/ZLIB Path to Path String__ogtk.vi
File 69=lvzip.llb/ZLIB Read Compressed File__ogtk.vi
File 70=lvzip.llb/ZLIB Read Compressed Stream__ogtk.vi
File 71=lvzip.llb/ZLIB Read Local Extra Data__ogtk.vi
File 72=lvzip.llb/ZLIB Specific Path to Common Path__ogtk.vi
File 73=lvzip.llb/ZLIB Store File__ogtk.vi
File 74=lvzip.llb/ZLIB Store Stream__ogtk.vi
File 75=lvzip.llb/ZLIB Store__ogtk.vi
File 76=lvzip.llb/ZLIB Transfer Raw File__ogtk.vi
File 77=lvzip.llb/ZLIB Uncompress File Info__ogtk.ctl
File 78=lvzip.llb/ZLIB Unzip Handle__ogtk.ctl
File 79=lvzip.llb/ZLIB VI Tree__ogtk.vi
File 80=lvzip.llb/ZLIB Write File__ogtk.vi
File 81=lvzip.llb/ZLIB Write Stream__ogtk.vi
File 82=lvzip.llb/ZLIB Zip Handle__ogtk.ctl

[File Group 1]
Source Dir=built/lvzip
Target Dir=<user.lib>/_OpenG.lib/lvzip
Exclusive_OS=Windows NT, Windows 9x
Replace Mode=Never
Num Files=1
File 0=lvzlib.dll

[File Group 2]
Source Dir=built/lvzip
Target Dir=<user.lib>/_OpenG.lib/lvzip
Exclusive_OS=Linux
Replace Mode=Never
Num Files=1
File 0=lvzlib.so

[File Group 3]
Source Dir=built/lvzip
Target Dir=<user.lib>/_OpenG.lib/lvzip
Exclusive_OS=MacOS>=10
Replace Mode=Never
Num Files=1
File 0=lvzlib.framework.zip

[File Group 4]
Source Dir=built/lvzip/vxWorks82
Target Dir=<OpenG.lib>/lvzip
Exclusive_LabVIEW_Version=<=8.2
Num Files=1
File 0=lvzlib.out

[File Group 5]
Source Dir=built/lvzip/vxWorks85
Target Dir=<OpenG.lib>/lvzip
Exclusive_LabVIEW_Version=8.5,8.6
Num Files=1
File 0=lvzlib.out

[File Group 6]
Source Dir="Dynamic Palette MNUs"
Target Dir="<user.lib>/_dynamicpalette_dirs/file"
Replace Mode=Always
Num Files=1
File 0=oglib_lvzip.mnu

[File Group 7]
Source Dir="Dynamic Palette MNUs"
Target Dir="<user.lib>/_dynamicpalette_dirs/OpenG"
Replace Mode=Always
Num Files=1
File 0=oglib_lvzip.mnu
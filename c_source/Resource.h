//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
//
#ifdef RC_INVOKED
#define ID(id) id
#else
#define ID(id) MAKEINTRESOURCE(id)
#endif

#ifdef IDC_STATIC
#undef IDC_STATIC
#endif 
#define IDC_STATIC            (-1)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

//---------------------------------------------------------------------------
// Version info.
//---------------------------------------------------------------------------
#define OFFICIAL   1 
#define FINAL      1 

#ifndef FINAL
#define VER_PRERELEASE           VS_FF_PRERELEASE
#else
#define VER_PRERELEASE           0
#endif

#ifndef NDEBUG
#define VER_DEBUG                VS_FF_DEBUG
#else
#define VER_DEBUG                0
#endif

#define VERSION_MAJOR            4
#define VERSION_MINOR            3
#define VERSION_REVISION         0
#define VERSION_BUILD            127

#define VER_FILEFLAGSMASK        VS_FFI_FILEFLAGSMASK
#define VER_FILEOS               VOS_DOS_WINDOWS16
#define VER_FILEFLAGS            (VER_PRERELEASE | VER_DEBUG)

#define VER_VERSION_STR			 STRINGIZE(VERSION_MAJOR) "." STRINGIZE(VERSION_MINOR) "." STRINGIZE(VERSION_REVISION)
#define VER_FILE_VERSION         VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD
#define VER_FILE_VERSION_STR     STRINGIZE(VERSION_MAJOR) "." STRINGIZE(VERSION_MINOR) "." STRINGIZE(VERSION_REVISION) ", Build " STRINGIZE(VERSION_BUILD)

#define VER_COMPANYNAME_STR      "Jean-loup Gailly & Mark Adler"
#define VER_PRODUCTNAME_STR      "LabVIEW ZIP Library"
#define VER_LEGALTRADEMARKS_STR  "LabVIEW\256 is a registered trademark of National Instruments Corporation."

#define VER_FILETYPE             VFT_DLL
#define VER_FILESUBTYPE          VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR  "LabVIEW zlib data compression library"
#define VER_INTERNALNAME_STR     "lvzlib"
#define VER_LEGALCOPYRIGHT_YEARS "\2511995-2017"
#define VER_LEGALCOPYRIGHT_STR   VER_LEGALCOPYRIGHT_YEARS " " VER_COMPANYNAME_STR
#define VER_ORIGINALFILENAME_STR "lvzlib.dll"
#define VER_FILEVERSION          VER_FILE_VERSION
#define VER_FILEVERSION_STR      "Version " VER_FILE_VERSION_STR
#define VER_COMMENTS_STR         "This DLL provides file functions to use ZIP archives in LabVIEW."
#define VER_PRODUCTVERSION       VER_FILE_VERSION
#define VER_PRODUCTVERSION_STR   "Version " VER_VERSION_STR

#define ABOUT_PRODUCTTITLE       VER_PRODUCTNAME_STR ", " VER_FILE_VERSION_STR

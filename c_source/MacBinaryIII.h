//MacBinaryIII.h
// Ch, 28.02.2003,  OSX port, see TARGET_API_MAC_CARBON macro
// Ch. 19.07.2005,  Xcode 1.5 port, from CW project

#ifdef TARGET_API_MAC_CARBON

//	#include <Types.h>	// not for OSX
#endif
//#include <Files.h>
//#include <Script.h>
#include <CoreServices/CoreServices.h> // instead of #include <Script.h> and /#include <Files.h>
#include <String.h>

#pragma once
//#pragma options align=mac68k
/*
000
            Byte
                            old version number, must be kept at zero for compatibility
 001
            Byte
                            Length of filename (must be in the range 1-31)
 002
            1 to 63 Bytes
                            filename (only "length" bytes are significant).
 065
            Long Word
                            file type (normally expressed as four characters)
 069
            Long Word
                            file creator (normally expressed as four characters)
 073
            Byte
                            original Finder flags 
                            Bit 7 - isAlias. 
                            Bit 6 - isInvisible. 
                            Bit 5 - hasBundle. 
                            Bit 4 - nameLocked. 
                            Bit 3 - isStationery. 
                            Bit 2 - hasCustomIcon. 
                            Bit 1 - reserved. 
                            Bit 0 - hasBeenInited.
 074
            Byte
                            zero fill, must be zero for compatibility
 075
            Word
                            file's vertical position within its window.
 077
            Word
                            file's horizontal position within its window.
 079
            Word
                            file's window or folder ID.
 081
            Byte
                            "Protected" flag (in low order bit).
 082
            Byte
                            zero fill, must be zero for compatibility
 083
            Long Word
                            Data Fork length (bytes, zero if no Data Fork).
 087
            Long Word
                            Resource Fork length (bytes, zero if no R.F.).
 091
            Long Word
                            File's creation date
 095
            Long Word
                            File's "last modified" date.
 099
            Word
                            length of Get Info comment to be sent after the resource fork (if
                            implemented, see below).
 101
            Byte
                            Finder Flags, bits 0-7. (Bits 8-15 are already in byte 73) 
                            Bit 7 - hasNoInits 
                            Bit 6 - isShared 
                            Bit 5 - requiresSwitchLaunch 
                            Bit 4 - ColorReserved 
                            Bits 1-3 - color 
                            Bit 0 - isOnDesk
 *102
            Long Word
                            signature for indentification purposes ('mBIN')
 *106
            Byte
                            script of file name (from the fdScript field of an fxInfo record)
 *107
            Byte
                            extended Finder flags (from the fdXFlags field of an fxInfo record)
 108-115

                            Unused (must be zeroed by creators, must be ignored by readers)
 116
            Long Word
                            Length of total files when packed files are unpacked. As of the writing
                            of this document, this field has never been used.
 120
            Word
                            Length of a secondary header. If this is non-zero, skip this many bytes
                            (rounded up to the next multiple of 128). This is for future expansion
                            only, when sending files with MacBinary, this word should be zero.
 *122
            Byte
                            Version number of MacBinary III that the uploading program is written
                            for (the version is 130 for MacBinary III)
 123
            Byte
                            Minimum MacBinary version needed to read this file (set this value at
                            129 for backwards compatibility with MacBinary II)
 124
            Word
                            CRC of previous 124 bytes
*/

typedef 	unsigned short		Word;

#define kOldVersionOffset				0
#define kFileNameLengthOffset			1
#define	kFileNameOffset					2
#define kFileTypeOffset					65
#define kFileCreatorOffset				69
#define kFinderFlagsHiOffset			73
#define kVersionCheckZero				74
#define kFileVPositionOffset			75
#define kFileHPositionOffset			77
#define kFileFolderIDOffset				79	
#define kProtectedFlagOffset			81
#define	kVersionOneCheckZero			82
#define kDataForkLengthOffset			83
#define kResourceForkLengthOffset		87
#define kFileCreationDateOffset			91
#define kFileModificationDateOffset		95
#define kGetInfoCommentLengthOffset		99
#define kFinderFlagsLowOffset			101
#define kMacbinarySigOffset				102
#define kFilenameScriptOffset			106
#define kExtendedFinderFlagsOffset		107
#define kTotalFileLengthOffset			116
#define kSecondaryHeaderLengthOffset	120
#define kCurrentVersionOffset			122
#define kMinimumVersionOffset			123
#define kCRCOffset						124

#define LONG_AT_OFFSET(data, offset)	*((long *)((unsigned char *)&data[offset]))
#define WORD_AT_OFFSET(data, offset)	*((Word *)((unsigned char *)&data[offset]))
#define BYTE_AT_OFFSET(data, offset)	*((Byte *)((unsigned char *)&data[offset]))
#define PTR_AT_OFFSET(data, offset)		((Ptr)((unsigned char *)&data[offset]))


#ifdef __cplusplus
extern "C" {
#endif

Handle EncodeMacbinary(FSSpec *file);
OSErr	EncodeMacbinaryFile(FSSpec *file);
OSErr EncodeMacbinaryFiles(FSSpec *file, FSSpec*	outfile);

OSErr	DecodeMacBinary(Handle data, FSSpec *destination);
OSErr 	DecodeMacBinaryFile(FSSpec *source);
OSErr	DecodeMacBinaryFiles(FSSpec* inputfile,FSSpec*	outputfile);
Boolean	FSpIsMacBinary(FSSpec *file);

#ifdef __cplusplus
}
#endif

//#pragma options align=reset

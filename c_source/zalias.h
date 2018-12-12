/*
   zalias.h -- name aliasing for all the zlib and minizip library functions to avoid name clashes in the VxWorks
   implementation. VxWorks seems to only use a single gloabal symbol table where all the exported functions of the
   process and any loaded shared library are placed into. Since the LabVIEW runtime contains a subset of the zlib
   and minizip library for its own ZIP functionality, loading of this shared library silently fails if it uses the
   same exported symbols. This was found and reported by some NI developer after I asked about why the cRIO shared
   library would not work at all, so the solution is to rename all the symbols here to avoid any name clashes.

   Copyright (C) 2007-2018 Rolf Kalbermatter

   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
   following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the
	   following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
       following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of SciWare, James Kring, Inc., nor the names of its contributors may be used to endorse
	   or promote products derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define Z_PREFIX_SET
#define _dist_code                  lvzip__dist_code
#define _length_code                lvzip__length_code
#define _tr_align                   lvzip__tr_align
#define _tr_flush_block             lvzip__tr_flush_block
#define _tr_init                    lvzip__tr_init
#define _tr_stored_block            lvzip__tr_stored_block
#define _tr_tally                   lvzip__tr_tally
#define adler32                     lvzip_adler32
#define adler32_combine             lvzip_adler32_combine
#define adler32_combine64           lvzip_adler32_combine64
#define compress                    lvzip_compress
#define compress2                   lvzip_compress2
#define compressBound               lvzip_compressBound
#define crc32                       lvzip_crc32
#define crc32_combine               lvzip_crc32_combine
#define crc32_combine64             lvzip_crc32_combine64
#define deflate                     lvzip_deflate
#define deflate_copyright           lvzip_deflate_copyright
#define deflateBound                lvzip_deflateBound
#define deflateCopy                 lvzip_deflateCopy
#define deflateEnd                  lvzip_deflateEnd
#define deflateInit_                lvzip_deflateInit
#define deflateInit2_               lvzip_deflateInit2
#define deflateParams               lvzip_deflateParams
#define deflatePrime                lvzip_deflatePrime
#define deflateReset                lvzip_deflateReset
#define deflateSetDictionary        lvzip_deflateSetDictionary
#define deflateSetHeader            lvzip_deflateSetHeader
#define deflateTune                 lvzip_deflateTune
#define get_crc_table               lvzip_get_crc_table
#define inflate                     lvzip_inflate
#define inflate_copyright           lvzip_inflate_copyright
#define inflate_table               lvzip_inflate_table
#define inflateBack                 lvzip_inflateBack
#define inflateBackEnd              lvzip_inflateBackEnd
#define inflateBackInit_            lvzip_inflateBackInit
#define inflateCopy                 lvzip_inflateCopy
#define inflateEnd                  lvzip_inflateEnd
#define inflateGetHeader            lvzip_inflateGetHeader
#define inflate_fast                lvzip_inflate_fast
#define inflateInit_                lvzip_inflateInit
#define inflateInit2_               lvzip_inflateInit2
#define inflateMark                 lvzip_inflateMark
#define inflatePrime                lvzip_inflatePrime
#define inflateReset                lvzip_inflateReset
#define inflateReset2               lvzip_inflateReset2
#define inflateSetDictionary        lvzip_inflateSetDictionary
#define inflateSync                 lvzip_inflateSync
#define inflateSyncPoint            lvzip_inflateSyncPoint
#define inflateUndermine            lvzip_inflateUndermine
#define uncompress                  lvzip_uncompress
#define z_errmsg                    z_z_errmsg
#define zcalloc                     lvzip_zcalloc
#define zcfree                      lvzip_zcfree
#define zError                      lvzip_zError
#define zlibCompileFlags            lvzip_zlibCompileFlags
#define zlibVersion                 lvzip_zlibVersion

#define unzClose                    lvzip_unzClose
#define unzClose2                   lvzip_unzClose2
#define unzCloseCurrentFile         lvzip_unzCloseCurrentFile
#define unzEndOfFile                lvzip_unzEndOfFile
#define unzGetCurrentFileInfo       lvzip_unzGetCurrentFileInfo
#define unzGetCurrentFileInfo64     lvzip_unzGetCurrentFileInfo64
#define unzGetCurrentFileZStreamPos64   lvzip_unzGetCurrentFileZStreamPos64
#define unzGetFilePos               lvzip_unzGetFilePos
#define unzGetFilePos64             lvzip_unzGetFilePos64
#define unzGetGlobalComment         lvzip_unzGetGlobalComment
#define unzGetGlobalInfo            lvzip_unzGetGlobalInfo
#define unzGetGlobalInfo64          lvzip_unzGetGlobalInfo64
#define unzGetLocalExtrafield       lvzip_unzGetLocalExtrafield
#define unzGetOffset                lvzip_unzGetOffset
#define unzGetOffset64              lvzip_unzGetOffset64
#define unzGoToFilePos              lvzip_unzGoToFilePos
#define unzGoToFilePos64            lvzip_unzGoToFilePos64
#define unzGoToFirstFile            lvzip_unzGoToFirstFile
#define unzGoToFirstFile2           lvzip_unzGoToFirstFile2
#define unzGoToNextFile             lvzip_unzGoToNextFile
#define unzGoToNextFile2            lvzip_unzGoToNextFile2
#define unzLocateFile               lvzip_unzLocateFile
#define unzOpen                     lvzip_unzOpen
#define unzOpen64                   lvzip_unzOpen64
#define unzOpen2                    lvzip_unzOpen2
#define unzOpen2_64                 lvzip_unzOpen2_64
#define unzOpenCurrentFile          lvzip_unzOpenCurrentFile
#define unzOpenCurrentFile2         lvzip_unzOpenCurrentFile2
#define unzOpenCurrentFile3         lvzip_unzOpenCurrentFile3
#define unzOpenCurrentFilePassword  lvzip_unzOpenCurrentFilePassword
#define unzReadCurrentFile          lvzip_unzReadCurrentFile
#define unzRepair				    lvzip_unzRepair
#define unzSetOffset                lvzip_unzSetOffset
#define unzSetOffset64              lvzip_unzSetOffset64
#define unzStringFileNameCompare    lvzip_unzStringFileNameCompare
#define unzSeek			            lvzip_unzSeek
#define unzSeek64				    lvzip_unzSeek64
#define unzTell				        lvzip_unzTell
#define unzTell64				    lvzip_unzTell64
#define zipClose                    lvzip_zipClose
#define zipClose2                   lvzip_zipClose2
#define zipCloseFileInZip           lvzip_zipCloseFileInZip
#define zipCloseFileInZipRaw        lvzip_zipCloseFileInZipRaw
#define zipCloseFileInZipRaw64      lvzip_zipCloseFileInZipRaw64
#define zipOpen                     lvzip_zipOpen
#define zipOpen64                   lvzip_zipOpen64
#define zipOpen2                    lvzip_zipOpen2
#define zipOpen2_64                 lvzip_zipOpen2_64
#define zipOpen3                    lvzip_zipOpen3
#define zipOpen3_64                 lvzip_zipOpen3_64
#define zipOpenNewFileInZip         lvzip_zipOpenNewFileInZip
#define zipOpenNewFileInZip64       lvzip_zipOpenNewFileInZip64
#define zipOpenNewFileInZip2        lvzip_zipOpenNewFileInZip2
#define zipOpenNewFileInZip2_64     lvzip_zipOpenNewFileInZip2_64
#define zipOpenNewFileInZip3        lvzip_zipOpenNewFileInZip3
#define zipOpenNewFileInZip3_64     lvzip_zipOpenNewFileInZip3_64
#define zipOpenNewFileInZip4        lvzip_zipOpenNewFileInZip4
#define zipOpenNewFileInZip4_64     lvzip_zipOpenNewFileInZip4_64
#define zipOpenNewFileInZip5        lvzip_zipOpenNewFileInZip5
#define zipRemoveExtraInfoBlock     lvzip_zipRemoveExtraInfoBlock
#define zipWriteInFileInZip         lvzip_zipWriteInFileInZip
#define gzopen						lvzip_gzopen
#define gzopen64					lvzip_gzopen64
#define gzdopen						lvzip_gzdopen
#define gzbuffer					lvzip_gzbuffer
#define gzseek						lvzip_gzseek
#define gzseek64					lvzip_gzseek64
#define gzsetparams					lvzip_gzsetparams
#define gzread						lvzip_gzread
#define gzwrite						lvzip_gzwrite
#define gzprintf					lvzip_gzprintf
#define gzputc						lvzip_gzputc
#define gzputs						lvzip_gzputs
#define gzgetc						lvzip_gzgetc
#define gzgets						lvzip_gzgets
#define gzungetc					lvzip_gzungetc
#define gzrewind					lvzip_gzrewind
#define gzeof						lvzip_gzeof
#define gzclearerr					lvzip_gzclearerr
#define gzflush						lvzip_gzflush
#define gzdirect					lvzip_gzdirect
#define gzerror						lvzip_gzerror
#define gzclose						lvzip_gzclose
#define gzclose_r					lvzip_gzclose_r
#define gzclose_w					lvzip_gzclose_w
#define gztell						lvzip_gztell
#define gztell64					lvzip_gztell64
#define gzoffset					lvzip_gzoffset
#define gzoffset64					lvzip_gzoffset64
#define gcrc32_combine				lvzip_gcrc32_combine
#define gcrc32_combine64			lvzip_gcrc32_combine64

/* Do some standard C type finicking for compilers that do not support the _t standard types */
#if defined(_MSC_VER) && (_MSC_VER < 1300)
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef int ssize_t;
typedef unsigned int size_t;
#else
#include <stdint.h>
#endif

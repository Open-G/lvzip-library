/* iowin32.h -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API
   This IO API version uses the Win32 API (for Microsoft Windows)

   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

void fill_mem_filefunc OF((zlib_filefunc64_def* pzlib_filefunc_def, LStrHandle *memory));

#ifdef __cplusplus
}
#endif

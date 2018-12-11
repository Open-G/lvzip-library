/* iomem.h -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Copyright (C) 2009-2018 Rolf Kalbermatter
*/

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

void fill_mem_filefunc(zlib_filefunc64_def* pzlib_filefunc_def, LStrHandle *memory);

#ifdef __cplusplus
}
#endif

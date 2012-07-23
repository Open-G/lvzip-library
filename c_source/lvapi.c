#define ZLIB_INTERNAL
#include "zlib.h"
#include "lvutil.h"
#include "zip.h"
#include "unzip.h"

#ifndef VERSIONMADEBY
# define VERSIONMADEBY   (0x0) /* platform depedent */
#endif

LibAPI(const char *) lvzlib_zlibVersion(void)
{
	return zlibVersion();
}

LibAPI(int) lvzlib_compress(Bytef *dest, uLongf *destLen,
                             const Bytef *source, uLong sourceLen, int level)
{
	return compress2(dest, destLen, source, sourceLen, level);
}

LibAPI(int) lvzlib_uncompress(Bytef *dest, uLongf *destLen,
                             const Bytef *source, uLong sourceLen)
{
	return uncompress(dest, destLen, source, sourceLen);
}

LibAPI(uInt32) lvzlib_crc32(uInt32 crc, const Bytef *buf, uInt32 len)
{
	return crc32(crc, buf, len);
}

LibAPI(int) lvzlib__openDummy(LStrHandle *name, UPtr *node)
{
	Unused(name);
    *node = NULL;
    return mgNotSupported;
}

LibAPI(int) lvzlib_noProp(UPtr node, uInt32 attr, UPtr data)
{
	Unused(node);
	Unused(attr);
	Unused(data);
    return mgNotSupported;
}

LibAPI(int) lvzlib_zipCleanup(zipFile node)
{
	return zipClose2(node, NULL, NULL);
}

LibAPI(int) lvzlib_unzipCleanup(unzFile node)
{
	return unzClose(node);
}

LibAPI(int) lvzlib_zipOpen(const void *pathname, int append, LStrHandle *globalcomment,
                           zlib_filefunc64_def* pzlib_filefunc_def, zipFile *file)
{
	zipcharpc comment;
	*file = zipOpen2_64(pathname, append, &comment, pzlib_filefunc_def);
	if (*file)
	{
		if (comment)
		{
			size_t len = strlen(comment);
			MgErr err = NumericArrayResize(uB, 1, (UHandle*)globalcomment, len);
			if (!err)
			{
				MoveBlock(comment, LStrBuf(**globalcomment), len);
				LStrLen(**globalcomment) = (int32)len;
			}
		}
		return Z_OK;
	}
	return Z_ERRNO;
}

LibAPI(int) lvzlib_zipOpenNewFileInZip(zipFile *file, const char* filename,
                           const zip_fileinfo* zipfi, const void* extrafield_local,
						   uInt size_extrafield_local, const void* extrafield_global,
                           uInt size_extrafield_global, const char* comment, int method,
                           int level, int raw, int windowBits, int memLevel, int strategy,
                           const char* password, uLong crcForCrypting, int zip64)
{
	return zipOpenNewFileInZip4_64(*file, filename, zipfi, extrafield_local, size_extrafield_local,
		                         extrafield_global, size_extrafield_global, comment, method, level,
								 raw, windowBits, memLevel, strategy, password[0] ? password : NULL,
								 crcForCrypting, VERSIONMADEBY, 0, zip64);
}

LibAPI(int) lvzlib_zipWriteInFileInZip(zipFile *file, const voidp buf, uInt32 len)
{
	return zipWriteInFileInZip(*file, buf, len);
}

LibAPI(int) lvzlib_zipCloseFileInZipRaw(zipFile *file, uInt32 uncompressedSize, uInt32 crc32)
{
	return zipCloseFileInZipRaw64(*file, uncompressedSize, crc32);
}

LibAPI(int) lvzlib_zipCloseFileInZipRaw64(zipFile *file, uInt64 uncompressedSize, uInt32 crc32)
{
	return zipCloseFileInZipRaw64(*file, uncompressedSize, crc32);
}

LibAPI(int) lvzlib_zipClose(zipFile *file, const char *globalComment, LStrHandle *stream)
{
	return zipClose2(*file, globalComment, stream);
}

LibAPI(int) lvzlib_unzOpen(const void *pathname, zlib_filefunc64_def* pzlib_filefunc_def, unzFile *file)
{
	*file = unzOpen2_64(pathname, pzlib_filefunc_def);
	if (*file)
		return Z_OK;
	return Z_ERRNO;
}

LibAPI(int) lvzlib_unzClose(unzFile *file)
{
	return unzClose(*file);
}

LibAPI(int) lvzlib_unzGetGlobalInfo(unzFile *file, unz_global_info* pglobal_info)
{
	return unzGetGlobalInfo(*file, pglobal_info);
}

LibAPI(int) lvzlib_unzGetGlobalInfo64(unzFile *file, unz_global_info64* pglobal_info)
{
	return unzGetGlobalInfo64(*file, pglobal_info);
}

LibAPI(int) lvzlib_unzGetGlobalComment(unzFile *file, char *szComment, uInt32 len)
{
	return unzGetGlobalComment(*file, szComment, len);
}

LibAPI(int) lvzlib_unzLocateFile(unzFile *file, const char *szFileName, int iCaseSensitivity)
{
	return unzLocateFile(*file, szFileName, iCaseSensitivity);
}

LibAPI(int) lvzlib_unzGetCurrentFileInfoLen(unzFile *file, unz_file_info * pfile_info)
{
	return unzGetCurrentFileInfo(*file, pfile_info, NULL, 0, NULL, 0, NULL, 0);
}

LibAPI(int) lvzlib_unzGetCurrentFileInfo(unzFile *file, unz_file_info * pfile_info,
                                          char * szFileName, uInt32 fileNameBufferSize,
                                          void *extraField, uInt32 extraFieldBufferSize,
                                          char* szComment,  uInt32 commentBufferSize)
{
	return unzGetCurrentFileInfo(*file, pfile_info, szFileName, fileNameBufferSize,
                                 extraField, extraFieldBufferSize, szComment,  commentBufferSize);
}

LibAPI(int) lvzlib_unzOpenCurrentFile(unzFile *file, int32* method, int32* level, int16 raw, const char* password)
{
	return unzOpenCurrentFile3(*file, method, level, raw, password[0] ? password : NULL);
}

LibAPI(int) lvzlib_unzGetLocalExtrafield(unzFile *file, LStrHandle *extra)
{
	int len = unzGetLocalExtrafield(*file, NULL, 0);
	if (len > 0)
	{
		MgErr err = NumericArrayResize(uB, 1, (UHandle*)extra, len);
		if (!err)
		{
			LStrLen(**extra) = len;
			return unzGetLocalExtrafield(*file, LStrBuf(**extra), len);
		}
		else
			len = Z_MEM_ERROR;
	}
	return len;
}

LibAPI(int) lvzlib_unzReadCurrentFile(unzFile *file, voidp buffer, uInt32 size)
{
	return unzReadCurrentFile(*file, buffer, size);
}

LibAPI(int) lvzlib_unzCloseCurrentFile(unzFile *file)
{
	return unzCloseCurrentFile(*file);
}

LibAPI(int) lvzlib_unzGoToFirstFile(unzFile *file)
{
	return unzGoToFirstFile(*file);
}

LibAPI(int) lvzlib_unzGoToNextFile(unzFile *file)
{
	return unzGoToNextFile(*file);
}

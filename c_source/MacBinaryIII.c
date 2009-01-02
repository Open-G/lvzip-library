/****************************************************************
	MacBinaryIII.c
	
	Copyright © 1997 Christopher Evans (cevans@poppybank.com)
	
	Basic encoding and decoding of Macintosh files to the 
	MacBinary III spec.
****************************************************************/

#include <string.h>
#if TARGET_API_MAC_CARBON
	//#include <MacHeaders.c>
#endif
#include "MacBinaryIII.h"


static unsigned short crctable[] = {
     0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
     0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
     0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
     0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
     0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
     0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
     0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
     0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
     0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
     0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
     0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
     0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
     0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
     0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
     0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
     0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
     0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
     0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
     0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
     0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
     0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
     0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
     0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
     0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
     0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
     0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
     0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
     0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
     0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
     0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
   	 0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
     0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};


static Boolean FSpExists(FSSpec *file)
{
	FInfo	fndrInfo;
	
	return FSpGetFInfo(file, &fndrInfo) == noErr; 
}

/* taken from the mcvert source code */
static short CalculateCRC(unsigned char *p, long len, short seed)
{
 	short hold;		/* crc computed so far */
 	long i;			/* index into data */
 
 	hold = seed;			       /* start with seed */
 	for (i = 0; i < len; i++, p++) {
 		hold ^= (*p << 8);
 		hold = (hold << 8) ^ crctable[(unsigned char) (hold >> 8)];
 	}
 
 	return (hold);
}				/* calc_crc() */

static OSErr	GetDesktopComment(FSSpec *file, char*comment, long *length)
{
	DTPBRec		pb;
	OSErr		err;

	pb.ioCompletion = nil;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = file->vRefNum;

	err = PBDTGetPath(&pb);
	
	if(err == noErr)
	{
		pb.ioNamePtr = file->name;
		pb.ioDTBuffer = comment;
		pb.ioDirID = file->parID;
		err = PBDTGetComment(&pb, false);
		*length = pb.ioDTActCount;
	}
	return err;
}

static OSErr	SetDesktopComment(FSSpec *file, char*comment, long length)
{
	DTPBRec		pb;
	OSErr		err;

	pb.ioCompletion = nil;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = file->vRefNum;

	err = PBDTGetPath(&pb);
	
	if(err == noErr)
	{
		pb.ioNamePtr = file->name;
		pb.ioDTBuffer = comment;
		pb.ioDirID = file->parID;
		pb.ioDTReqCount = length;
		err = PBDTSetComment(&pb, false);
	}
	return err;
}


OSErr	EncodeMacbinaryFile(FSSpec *file)
{
	Handle data;
	OSErr	err = paramErr;
	short	dfRefNum;
	long 	len;//add
	
	data = EncodeMacbinary(file);
	
	if(data)
	{
		if(file->name[0] > 27)
			file->name[0] = 27;
			
	/////	PtoCstr(file->name);
	////	strcat((char*)file->name, ".bin");
	////	CtoPstr((char *)file->name);
			len = file->name[0];
			file->name[len+1]='.';
			file->name[len+2]='b';
			file->name[len+3]='i';
			file->name[len+4]='n';
			file->name[0] +=4;
			

		FSpDelete(file);
		if(FSpCreate(file, 'dMB3', 'mBIN', smSystemScript) == noErr)
		{
			err = FSpOpenDF(file, fsWrPerm, &dfRefNum);
			if(err == noErr)
			{
				long	inOutCount = GetHandleSize(data);
				
				HLock(data);
				err = FSWrite(dfRefNum,&inOutCount,*data);
				HUnlock(data);
				FSClose(dfRefNum);
			}
		}
		DisposeHandle(data);
	}
	return err;
}



OSErr EncodeMacbinaryFiles(FSSpec *file, FSSpec*	outfile)
{
	Handle result = nil;
	FInfo	fndrInfo;
	FXInfo	fndrXInfo;
	OSErr	err;
	CInfoPBRec	pb;
	short	dfRefNum, rfRefNum;
	short	outrefnum=0;
	long	ioCount;
	char* 	buffer;
	char	header[128];
	char	comment[256];
	long	resourceForkLength, dataForkLength, commentLength;
	
	memset(&header, 0, sizeof(header));
	err = FSpGetFInfo(file, &fndrInfo);

	memset(&pb, 0, sizeof(CInfoPBRec));
	pb.hFileInfo.ioNamePtr = file->name;
	pb.hFileInfo.ioVRefNum = file->vRefNum;
	pb.hFileInfo.ioFDirIndex = 0;			//query a file
	pb.hFileInfo.ioDirID = file->parID;
	err = PBGetCatInfo(&pb,false);
	
	fndrXInfo = pb.hFileInfo.ioFlXFndrInfo;
	
	BYTE_AT_OFFSET(header, kFileNameLengthOffset) = file->name[0];
	BlockMoveData( &(file->name[1]), PTR_AT_OFFSET(header, kFileNameOffset), file->name[0]);
	LONG_AT_OFFSET(header, kFileTypeOffset) = fndrInfo.fdType;
	LONG_AT_OFFSET(header, kFileCreatorOffset) = fndrInfo.fdCreator;
		
	BYTE_AT_OFFSET(header, kFinderFlagsHiOffset) = (fndrInfo.fdFlags & 0xFF00) >> 8;
	BYTE_AT_OFFSET(header, kFinderFlagsLowOffset) = (fndrInfo.fdFlags & 0x00FF);
	
	WORD_AT_OFFSET(header, kFileVPositionOffset) = fndrInfo.fdLocation.v;
	WORD_AT_OFFSET(header, kFileHPositionOffset) = fndrInfo.fdLocation.h;
	WORD_AT_OFFSET(header, kFileFolderIDOffset) = fndrInfo.fdFldr;

	LONG_AT_OFFSET(header, kFileCreationDateOffset) = pb.hFileInfo.ioFlCrDat;	
	LONG_AT_OFFSET(header, kFileModificationDateOffset) = pb.hFileInfo.ioFlMdDat;	


	LONG_AT_OFFSET(header, kMacbinarySigOffset) = 'mBIN';	
	BYTE_AT_OFFSET(header, kFilenameScriptOffset) = fndrXInfo.fdScript;
	BYTE_AT_OFFSET(header, kExtendedFinderFlagsOffset) = fndrXInfo.fdXFlags;


	LONG_AT_OFFSET(header, kTotalFileLengthOffset) = 0;	
	WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset) = 0;	

	WORD_AT_OFFSET(header, kCurrentVersionOffset) = 130;	
	WORD_AT_OFFSET(header, kMinimumVersionOffset) = 129;	
	
	err = FSpOpenDF(file,fsRdPerm,&dfRefNum);
	if(err == noErr)
	{
		err = GetEOF(dfRefNum,&dataForkLength);
		LONG_AT_OFFSET(header, kDataForkLengthOffset) = dataForkLength;	
	}
	else
	{
		dfRefNum = 0;
	}
	
	err = FSpOpenRF(file,fsRdPerm,&rfRefNum);
	if(err == noErr)
	{
		err = GetEOF(rfRefNum,&resourceForkLength);
		LONG_AT_OFFSET(header, kResourceForkLengthOffset) = resourceForkLength;	
	}
	else
	{
		rfRefNum = 0;
	}

	FSpCreate(outfile, kUnknownType, 'BINA', smSystemScript);
	
	err = FSpOpenDF(outfile,fsRdWrPerm,&outrefnum);
	if (err != noErr)
	{
		return err;
	}
	
	memset(comment, 0, 256);
	if(GetDesktopComment(file, comment, &commentLength) != noErr)
		commentLength = 0;
	
	WORD_AT_OFFSET(header, kGetInfoCommentLengthOffset) = commentLength;	
	
	WORD_AT_OFFSET(header, kCRCOffset) = CalculateCRC((unsigned char*) &header, 124, 0);


	
	
	if(1)
	{	
		ioCount=128;
		
		buffer=NewPtr(32768);
		if (!buffer)
		{
			return dsMemFullErr;
		}
		
		FSWrite(outrefnum,&ioCount,header);
	
		if(dfRefNum && dataForkLength)
		{			
			err = noErr;
			while(dataForkLength > 0 && err == noErr)
			{
				long	count2=128;
				ioCount = 128;
				err = FSRead(dfRefNum,&ioCount,buffer);
				
				count2=128;
				FSWrite(outrefnum,&count2,buffer);
				
				dataForkLength -= ioCount;
				
			}			
		}
		
		if(rfRefNum && resourceForkLength)
		{
			err = noErr;
			while(resourceForkLength > 0 && err == noErr)
			{
				long		count2=128;
				ioCount = 128;
				err = FSRead(rfRefNum,&ioCount,buffer);
				
				count2=128;
				FSWrite(outrefnum,&count2,buffer);
				
				resourceForkLength -= ioCount;
			}			
		}
		
		if(commentLength)
		{
			PtrAndHand(&comment,result,commentLength);
		}
	}
	
	if (buffer)
	{
		DisposePtr(buffer);
		buffer=NULL;
	}
	
	if(rfRefNum)
		FSClose(rfRefNum);
	
	if(dfRefNum)
		FSClose(dfRefNum);
	
	if (outrefnum)
	{
		FSClose(outrefnum);
	}
	
	return noErr;
}



Handle EncodeMacbinary(FSSpec *file)
{
	Handle result = nil;
	FInfo	fndrInfo;
	FXInfo	fndrXInfo;
	OSErr	err;
	CInfoPBRec	pb;
	short	dfRefNum, rfRefNum;
	long	ioCount;
	char 	buffer[128];
	char	header[128];
	char	comment[256];
	long	resourceForkLength, dataForkLength, commentLength;
	
	memset(&header, 0, sizeof(header));
	err = FSpGetFInfo(file, &fndrInfo);

	memset(&pb, 0, sizeof(CInfoPBRec));
	pb.hFileInfo.ioNamePtr = file->name;
	pb.hFileInfo.ioVRefNum = file->vRefNum;
	pb.hFileInfo.ioFDirIndex = 0;			//query a file
	pb.hFileInfo.ioDirID = file->parID;
	err = PBGetCatInfo(&pb,false);
	
	fndrXInfo = pb.hFileInfo.ioFlXFndrInfo;
	
	BYTE_AT_OFFSET(header, kFileNameLengthOffset) = file->name[0];
	BlockMoveData( &(file->name[1]), PTR_AT_OFFSET(header, kFileNameOffset), file->name[0]);
	LONG_AT_OFFSET(header, kFileTypeOffset) = fndrInfo.fdType;
	LONG_AT_OFFSET(header, kFileCreatorOffset) = fndrInfo.fdCreator;
		
	BYTE_AT_OFFSET(header, kFinderFlagsHiOffset) = (fndrInfo.fdFlags & 0xFF00) >> 8;
	BYTE_AT_OFFSET(header, kFinderFlagsLowOffset) = (fndrInfo.fdFlags & 0x00FF);
	
	WORD_AT_OFFSET(header, kFileVPositionOffset) = fndrInfo.fdLocation.v;
	WORD_AT_OFFSET(header, kFileHPositionOffset) = fndrInfo.fdLocation.h;
	WORD_AT_OFFSET(header, kFileFolderIDOffset) = fndrInfo.fdFldr;

	LONG_AT_OFFSET(header, kFileCreationDateOffset) = pb.hFileInfo.ioFlCrDat;	
	LONG_AT_OFFSET(header, kFileModificationDateOffset) = pb.hFileInfo.ioFlMdDat;	

	LONG_AT_OFFSET(header, kMacbinarySigOffset) = 'mBIN';	
	BYTE_AT_OFFSET(header, kFilenameScriptOffset) = fndrXInfo.fdScript;
	BYTE_AT_OFFSET(header, kExtendedFinderFlagsOffset) = fndrXInfo.fdXFlags;

	LONG_AT_OFFSET(header, kTotalFileLengthOffset) = 0;	
	WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset) = 0;	

	WORD_AT_OFFSET(header, kCurrentVersionOffset) = 130;	
	WORD_AT_OFFSET(header, kMinimumVersionOffset) = 129;	
	
	err = FSpOpenDF(file,fsRdPerm,&dfRefNum);
	if(err == noErr)
	{
		err = GetEOF(dfRefNum,&dataForkLength);
		LONG_AT_OFFSET(header, kDataForkLengthOffset) = dataForkLength;	
	}
	else
	{
		dfRefNum = 0;
	}
	
	err = FSpOpenRF(file,fsRdPerm,&rfRefNum);
	if(err == noErr)
	{
		err = GetEOF(rfRefNum,&resourceForkLength);
		LONG_AT_OFFSET(header, kResourceForkLengthOffset) = resourceForkLength;	
	}
	else
	{
		rfRefNum = 0;
	}
	
	memset(comment, 0, 256);
	if(GetDesktopComment(file, comment, &commentLength) != noErr)
		commentLength = 0;
	
	WORD_AT_OFFSET(header, kGetInfoCommentLengthOffset) = commentLength;	
	
	WORD_AT_OFFSET(header, kCRCOffset) = CalculateCRC((unsigned char*) &header, 124, 0);
	
	result = TempNewHandle(0, &err);
	if(result)
	{	
		err = PtrAndHand(&header,result,128);
	
		if(dfRefNum && dataForkLength)
		{			
			err = noErr;
			while(dataForkLength > 0 && err == noErr)
			{
				ioCount = 128;
				err = FSRead(dfRefNum,&ioCount,&buffer);
				
				if(err == noErr || err == eofErr)
					err = PtrAndHand(&buffer,result,128);
				
				dataForkLength -= ioCount;
			}			
		}
		
		if(rfRefNum && resourceForkLength)
		{
			err = noErr;
			while(resourceForkLength > 0 && err == noErr)
			{
				ioCount = 128;
				err = FSRead(rfRefNum,&ioCount,&buffer);
				
				if(err == noErr || err == eofErr)
					err = PtrAndHand(&buffer,result,128);
				
				resourceForkLength -= ioCount;
			}			
		}
		
		if(commentLength)
		{
			PtrAndHand(&comment,result,commentLength);
		}
	}
	
	if(rfRefNum)
		FSClose(rfRefNum);
	
	if(dfRefNum)
		FSClose(dfRefNum);
	
	return result;
}

OSErr 	DecodeMacBinaryFile(FSSpec *source)
{
	Handle	data = nil;
	OSErr	err;
	short	dfRefNum = 0;
	long	size;
	
	err = FSpOpenDF(source, fsRdPerm, &dfRefNum);
	if(!err)
	{
		err = GetEOF(dfRefNum, &size);
	
		data = TempNewHandle(size, &err);
		
		if(data)
		{
			HLock(data);
			err = FSRead(dfRefNum,&size,*data);
			HUnlock(data);
		}
		FSClose(dfRefNum);
	}
	
	if(data && err == noErr)
	{
		err =  DecodeMacBinary(data, source);
		DisposeHandle(data);
	}
	return err;
}


static Boolean	HeaderIsMacBinary(char *header, Word *version)
{
	Boolean	isIt = false;
	long	resourceForkLength, dataForkLength, commentLength;
	Byte	mbVersion;
	
	if(LONG_AT_OFFSET(header, kMacbinarySigOffset) == 'mBIN')
	{
		isIt = true;
		mbVersion = 130;
	}
	else
		if(BYTE_AT_OFFSET(header, kVersionCheckZero) == 0 &&
			BYTE_AT_OFFSET(header, kOldVersionOffset) == 0)
		{
			if(WORD_AT_OFFSET(header, kCRCOffset) == CalculateCRC((unsigned char*) &header, 124, 0))
			{
				isIt = true;
				mbVersion = 129;
			}
			else
			{
				if(BYTE_AT_OFFSET(header, kVersionOneCheckZero) == 0)
				{
					isIt = true;
					mbVersion = 128;
				}
			}
		}

	resourceForkLength = LONG_AT_OFFSET(header, kResourceForkLengthOffset);
	dataForkLength = LONG_AT_OFFSET(header, kDataForkLengthOffset);
	commentLength = WORD_AT_OFFSET(header, kGetInfoCommentLengthOffset);

	if(isIt && BYTE_AT_OFFSET(header, kFileNameLengthOffset) >= 1 &&
		BYTE_AT_OFFSET(header, kFileNameLengthOffset) <= 31 &&
		dataForkLength >= 0 &&
		dataForkLength <= 0x007fffff &&
		resourceForkLength >= 0 &&
		resourceForkLength <= 0x007fffff)
	{
		isIt = true;
	}
	else
		isIt = false;	//something is wrong with the header

	if(version)
		*version = mbVersion;
	return isIt;

}

Boolean	FSpIsMacBinary(FSSpec *file)
{
	char header[128];
	short dfRefNum = 0;
	OSErr	err;
	long	size;
	Boolean	isIt = false;
		
	memset(header, 0, 128);
	
	err = FSpOpenDF(file, fsRdPerm, &dfRefNum);
	if(!err)
	{
		err = GetEOF(dfRefNum, &size);
		if(size > 128)
		{
			size = 128;
			err = FSRead(dfRefNum, &size, &header); 
			
			if(err == noErr)
			{
				isIt = HeaderIsMacBinary(header, nil);
			}
		}
		FSClose(dfRefNum);
	}
	return isIt;
}


//
//	Returns an FSSpec for the output file.
//	Also this has a really small memory footprint, by reading directly
//	from the file, instead of having to have the entire file in
//	memory.  While it is slower, it works better for plugins and such.
//
OSErr	DecodeMacBinaryFiles(FSSpec* inputfile,FSSpec*	outputfile)
{
	FInfo	fndrInfo;
	OSErr	err;
	CInfoPBRec	pb;
	short	dfRefNum, rfRefNum;
	long	ioCount;
	char	header[128];
	char	comment[256];
	long	resourceForkLength, dataForkLength, commentLength;
	Boolean	isMacBinaryFile = false;
	Word	mbVersion;
	short	headerEnd = 128, index;
	long	rfOffset=0;
	short	refnum=0;
	FSSpec	checkFile;
	char*	buffer=NULL;
	
	buffer=NewPtr(32768);
	if (buffer==NULL)
	{
		return memFullErr;
	}
	
	err=FSpOpenDF(inputfile,fsCurPerm,&refnum);
	if (err==noErr)
	{
		long	inout=headerEnd;
		err=FSRead(refnum,&inout,header);
		if (err!=noErr)
		{
			FSClose(refnum);
			return err;
		}
	}
	else
	{
		return err;
	}
	
	isMacBinaryFile = HeaderIsMacBinary(header, &mbVersion);
	
	if(!isMacBinaryFile)
		return paramErr;

	if(WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset))
	{
		headerEnd = WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset);
		headerEnd = (headerEnd + 127) & ~127L;
	}

	resourceForkLength = LONG_AT_OFFSET(header, kResourceForkLengthOffset);
	dataForkLength = LONG_AT_OFFSET(header, kDataForkLengthOffset);
	commentLength = WORD_AT_OFFSET(header, kGetInfoCommentLengthOffset);

		
	memcpy( outputfile->name, PTR_AT_OFFSET(header, kFileNameLengthOffset), BYTE_AT_OFFSET(header, kFileNameLengthOffset)+1);

	fndrInfo.fdType = LONG_AT_OFFSET(header, kFileTypeOffset);
	fndrInfo.fdCreator = LONG_AT_OFFSET(header, kFileCreatorOffset);
		
	fndrInfo.fdFlags = BYTE_AT_OFFSET(header, kFinderFlagsHiOffset) << 8;
	fndrInfo.fdFlags |= BYTE_AT_OFFSET(header, kFinderFlagsLowOffset);
	
	fndrInfo.fdLocation.v = WORD_AT_OFFSET(header, kFileVPositionOffset);
	fndrInfo.fdLocation.h = WORD_AT_OFFSET(header, kFileHPositionOffset);
	fndrInfo.fdFldr = WORD_AT_OFFSET(header, kFileFolderIDOffset);

	index = 1;
	checkFile = *inputfile;
	BlockMove(outputfile->name,checkFile.name,outputfile->name[0]+1);
	while(FSpExists(&checkFile))
	{	
		checkFile = *inputfile;
		BlockMove(outputfile->name,checkFile.name,outputfile->name[0]+1);
		
		if(index < 10)
			checkFile.name[++checkFile.name[0]] = '0' + index;
		else
			checkFile.name[++checkFile.name[0]] = ('a' - 10) + index;
		index++;
	}
	*outputfile = checkFile;

	err = FSpCreate(outputfile, fndrInfo.fdCreator, fndrInfo.fdType, smSystemScript);		

	dfRefNum = 0;
	if(err == noErr)
	{
		err = FSpOpenDF(outputfile, fsRdWrPerm, &dfRefNum);
	}
	
	

	if(err == noErr && dfRefNum)
	{
		while (dataForkLength > 0)
		{
			ioCount=128;				
			err = FSRead(refnum,&ioCount,buffer);
			
			if (dataForkLength < 128)
			{	
				long	count2=dataForkLength;
				err = FSWrite(dfRefNum,&count2,buffer);
			}
			else
			{
				err = FSWrite(dfRefNum,&ioCount,buffer);
			}
			
			dataForkLength-=ioCount;
		}
		FSClose(dfRefNum);
	}
	
	
	rfRefNum = 0;
	if(err == noErr)
	{
		err = FSpOpenRF(outputfile, fsRdWrPerm, &rfRefNum);
	}
	
	if(err == noErr && rfRefNum)
	{
		while (resourceForkLength > 0)
		{
			ioCount=128;
			err = FSRead(refnum,&ioCount,buffer);

			if (resourceForkLength<128)
			{
				long count2=resourceForkLength;
				err = FSWrite(rfRefNum,&count2,buffer);
			}	
			else
			{
				err = FSWrite(rfRefNum,&ioCount,buffer);		
			}
			
			resourceForkLength-=ioCount;
		}
		FSClose(rfRefNum);
	}
	
	rfOffset += resourceForkLength;
	rfOffset = (rfOffset + 127) & ~127L;

	if(err == noErr)
	{
		FSpSetFInfo(outputfile,&fndrInfo);


		memset(&pb, 0, sizeof(CInfoPBRec));
		pb.hFileInfo.ioNamePtr = outputfile->name;
		pb.hFileInfo.ioVRefNum = outputfile->vRefNum;
		pb.hFileInfo.ioFDirIndex = 0;			//query a file
		pb.hFileInfo.ioDirID = outputfile->parID;
		err = PBGetCatInfo(&pb,false);
		
		if(err == noErr)
		{
			pb.hFileInfo.ioDirID = outputfile->parID;
			pb.hFileInfo.ioFlCrDat = LONG_AT_OFFSET(header, kFileCreationDateOffset);	
			pb.hFileInfo.ioFlMdDat = LONG_AT_OFFSET(header, kFileModificationDateOffset);	

			pb.hFileInfo.ioFlXFndrInfo.fdXFlags = BYTE_AT_OFFSET(header, kExtendedFinderFlagsOffset);
			pb.hFileInfo.ioFlXFndrInfo.fdScript = BYTE_AT_OFFSET(header, kFilenameScriptOffset);
						
			err = PBSetCatInfo(&pb, false);
		}
		
		if(commentLength)
		{
			//memcpy(comment,*data + rfOffset, commentLength);
			SetDesktopComment(outputfile, comment, commentLength);
		}
		
	}	

	if (refnum)
	{
		FSClose(refnum);
	}
	
	if (buffer)
	{
		DisposePtr(buffer);
	}

	return err;	
}


OSErr	DecodeMacBinary(Handle data, FSSpec *destination)
{
	FInfo	fndrInfo;
	OSErr	err;
	CInfoPBRec	pb;
	short	dfRefNum, rfRefNum;
	long	ioCount;
	char	header[128];
	char	comment[256];
	long	resourceForkLength, dataForkLength, commentLength;
	Boolean	isMacBinaryFile = false;
	Word	mbVersion;
	short	headerEnd = 128, index;
	long	rfOffset;
	FSSpec 	checkFile;
	
	HLock(data);
	memcpy(header, *data, 128);
	
	isMacBinaryFile = HeaderIsMacBinary(header, &mbVersion);

	
	if(!isMacBinaryFile)
		return paramErr;

	if(WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset))
	{
		headerEnd = WORD_AT_OFFSET(header, kSecondaryHeaderLengthOffset);
		headerEnd = (headerEnd + 127) & ~127L;
	}

	resourceForkLength = LONG_AT_OFFSET(header, kResourceForkLengthOffset);
	dataForkLength = LONG_AT_OFFSET(header, kDataForkLengthOffset);
	commentLength = WORD_AT_OFFSET(header, kGetInfoCommentLengthOffset);

		
	memcpy( &destination->name, PTR_AT_OFFSET(header, kFileNameLengthOffset), BYTE_AT_OFFSET(header, kFileNameLengthOffset));

	fndrInfo.fdType = LONG_AT_OFFSET(header, kFileTypeOffset);
	fndrInfo.fdCreator = LONG_AT_OFFSET(header, kFileCreatorOffset);
		
	fndrInfo.fdFlags = BYTE_AT_OFFSET(header, kFinderFlagsHiOffset) << 8;
	fndrInfo.fdFlags |= BYTE_AT_OFFSET(header, kFinderFlagsLowOffset);
	
	fndrInfo.fdLocation.v = WORD_AT_OFFSET(header, kFileVPositionOffset);
	fndrInfo.fdLocation.h = WORD_AT_OFFSET(header, kFileHPositionOffset);
	fndrInfo.fdFldr = WORD_AT_OFFSET(header, kFileFolderIDOffset);

	index = 1;
	checkFile = *destination;
	while(FSpExists(&checkFile))
	{	
		checkFile = *destination;
		
		if(index < 10)
			checkFile.name[++checkFile.name[0]] = '0' + index;
		else
			checkFile.name[++checkFile.name[0]] = ('a' - 10) + index;
		index++;
	}
	*destination = checkFile;

	err = FSpCreate(destination, fndrInfo.fdCreator, fndrInfo.fdType, smSystemScript);		

	dfRefNum = 0;
	if(err == noErr)
	{
		err = FSpOpenDF(destination, fsRdWrPerm, &dfRefNum);
	}
	
	if(err == noErr && dfRefNum)
	{
		ioCount = dataForkLength;
		err = FSWrite(dfRefNum,&ioCount,*data + headerEnd);
		FSClose(dfRefNum);
	}
	
	rfRefNum = 0;
	if(err == noErr)
	{
		err = FSpOpenRF(destination, fsRdWrPerm, &rfRefNum);
	}
	
	rfOffset = headerEnd + dataForkLength;
	rfOffset = (rfOffset + 127) & ~127L;
	
	if(err == noErr && rfRefNum)
	{
		ioCount = resourceForkLength;
		err = FSWrite(rfRefNum,&ioCount,*data + rfOffset);
		FSClose(rfRefNum);
	}
	
	rfOffset += resourceForkLength;
	rfOffset = (rfOffset + 127) & ~127L;

	if(err == noErr)
	{
		FSpSetFInfo(destination,&fndrInfo);


		memset(&pb, 0, sizeof(CInfoPBRec));
		pb.hFileInfo.ioNamePtr = destination->name;
		pb.hFileInfo.ioVRefNum = destination->vRefNum;
		pb.hFileInfo.ioFDirIndex = 0;			//query a file
		pb.hFileInfo.ioDirID = destination->parID;
		err = PBGetCatInfo(&pb,false);
		
		if(err == noErr)
		{
			pb.hFileInfo.ioDirID = destination->parID;
			pb.hFileInfo.ioFlCrDat = LONG_AT_OFFSET(header, kFileCreationDateOffset);	
			pb.hFileInfo.ioFlMdDat = LONG_AT_OFFSET(header, kFileModificationDateOffset);	

			pb.hFileInfo.ioFlXFndrInfo.fdXFlags = BYTE_AT_OFFSET(header, kExtendedFinderFlagsOffset);
			pb.hFileInfo.ioFlXFndrInfo.fdScript = BYTE_AT_OFFSET(header, kFilenameScriptOffset);
						
			err = PBSetCatInfo(&pb, false);
		}
		
		if(commentLength)
		{
			memcpy(comment,*data + rfOffset, commentLength);
			SetDesktopComment(destination, comment, commentLength);
		}
		
	}	
	HUnlock(data);
	return err;	
}

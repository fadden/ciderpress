/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for 2MG/2IMG wrapper files.
 *
 * This needs to be directly accessible from external applications, so try not
 * to hook into private DiskImg state.
 */
#include "StdAfx.h"
#include "TwoImg.h"
#include "DiskImgPriv.h"

///*static*/ const char* TwoImgHeader::kMagic = "2IMG";	// file magic #
///*static*/ const char* TwoImgHeader::kCreator = "CdrP";	// our "creator" ID


/*
 * Initialize a header to default values, using the supplied image length
 * where appropriate.
 *
 * Sets up a header for a 2MG file with the disk data and nothing else.
 *
 * Returns 0 on success, -1 if one of the arguments was bad.
 */
int
TwoImgHeader::InitHeader(int imageFormat, long imageSize, long imageBlockCount)
{
	if (imageSize <= 0)
		return -1;
	if (imageFormat < kImageFormatDOS || imageFormat > kImageFormatNibble)
		return -1;

    if (imageFormat != kImageFormatNibble &&
        imageSize != imageBlockCount * 512)
    {
        WMSG3("2MG InitHeader: bad sizes %d %ld %ld\n", imageFormat,
            imageSize, imageBlockCount);
        return -1;
    }

	assert(fComment == nil);

	//memcpy(fMagic, kMagic, 4);
	//memcpy(fCreator, kCreator, 4);
	fMagic = kMagic;
	fCreator = kCreatorCiderPress;
	fHeaderLen = kOurHeaderLen;
	fVersion = kOurVersion;
	fImageFormat = imageFormat;
	fFlags = 0;
	fNumBlocks = imageBlockCount;
	fDataOffset = kOurHeaderLen;
	fDataLen = imageSize;
	fCmtOffset = 0;
	fCmtLen = 0;
	fCreatorOffset = 0;
	fCreatorLen = 0;
	fSpare[0] = fSpare[1] = fSpare[2] = fSpare[3] = 0;

	return 0;
}

/*
 * Get the DOS volume number.
 *
 * If not set, we currently return the initial value (-1), rather than the
 * default volume number.  For the way we currently make use of this, this
 * makes the most sense.
 */
short
TwoImgHeader::GetDOSVolumeNum(void) const
{
	assert(fFlags & kDOSVolumeSet);
	return fDOSVolumeNum;
}

/*
 * Set the DOS volume number.
 */
void
TwoImgHeader::SetDOSVolumeNum(short dosVolumeNum)
{
    assert(dosVolumeNum >= 0 && dosVolumeNum < 256);
    fFlags |= dosVolumeNum;
    fFlags |= kDOSVolumeSet;
}

/*
 * Set the comment.
 */
void
TwoImgHeader::SetComment(const char* comment)
{
	delete[] fComment;
	if (comment == nil) {
		fComment = nil;
	} else {
		fComment = new char[strlen(comment)+1];
		if (fComment != nil)
			strcpy(fComment, comment);
		// else throw alloc failure
	}

	if (fComment == nil) {
		fCmtLen = 0;
		fCmtOffset = 0;
		if (fCreatorOffset > 0)
			fCreatorOffset = fDataOffset + fDataLen;
	} else {
		fCmtLen = strlen(fComment);
		fCmtOffset = fDataOffset + fDataLen;
		if (fCreatorOffset > 0)
			fCreatorOffset = fCmtOffset + fCmtLen;
	}
}

/*
 * Set the creator chunk.
 */
void
TwoImgHeader::SetCreatorChunk(const void* chunk, long len)
{
	assert(len >= 0);

	delete[] fCreatorChunk;
	if (chunk == nil || len == 0) {
		fCreatorChunk = nil;
	} else {
		fCreatorChunk = new char[len];
		if (fCreatorChunk != nil)
			memcpy(fCreatorChunk, chunk, len);
		// else throw alloc failure
	}

	if (fCreatorChunk == nil) {
		fCreatorLen = 0;
		fCreatorOffset = 0;
	} else {
		fCreatorLen = len;
		if (fCmtOffset > 0)
			fCreatorOffset = fCmtOffset + fCmtLen;
		else
			fCreatorOffset = fDataOffset + fDataLen;
	}
}

/*
 * Read the header from a 2IMG file.  Pass in "totalLength" as a sanity check.
 *
 * THOUGHT: provide a simple GenericFD conversion for FILE*, and then just
 * call the GenericFD version of ReadHeader.
 *
 * Returns 0 on success, nonzero on error or invalid header.
 */
int
TwoImgHeader::ReadHeader(FILE* fp, long totalLength)
{
	unsigned char buf[kOurHeaderLen];

	fread(buf, kOurHeaderLen, 1, fp);
	if (ferror(fp))
		return errno ? errno : -1;

	if (UnpackHeader(buf, totalLength) != 0)
		return -1;

	/*
	 * Extract the comment, if any.
	 */
	if (fCmtOffset > 0 && fCmtLen > 0) {
		if (GetChunk(fp, fCmtOffset - kOurHeaderLen, fCmtLen,
			(void**) &fComment) != 0)
		{
			WMSG0("Throwing comment away\n");
			fCmtLen = 0;
			fCmtOffset = 0;
		} else {
			WMSG1("Got comment: '%s'\n", fComment);
		}
	}

	/*
	 * Extract the creator chunk, if any.
	 */
	if (fCreatorOffset > 0 && fCreatorLen > 0) {
		if (GetChunk(fp, fCreatorOffset - kOurHeaderLen, fCreatorLen,
			(void**) &fCreatorChunk) != 0)
		{
			WMSG0("Throwing creator chunk away\n");
			fCreatorLen = 0;
			fCreatorOffset = 0;
		} else {
			//WMSG1("Got creator chunk: '%s'\n", fCreatorChunk);
		}
	}

	return 0;
}

/*
 * Read the header from a 2IMG file.  Pass in "totalLength" as a sanity check.
 *
 * Returns 0 on success, nonzero on error or invalid header.
 */
int
TwoImgHeader::ReadHeader(GenericFD* pGFD, long totalLength)
{
	DIError dierr;
	unsigned char buf[kOurHeaderLen];

	dierr = pGFD->Read(buf, kOurHeaderLen);
	if (dierr != kDIErrNone)
		return -1;

	if (UnpackHeader(buf, totalLength) != 0)
		return -1;

	/*
	 * Extract the comment, if any.
	 */
	if (fCmtOffset > 0 && fCmtLen > 0) {
		if (GetChunk(pGFD, fCmtOffset - kOurHeaderLen, fCmtLen,
			(void**) &fComment) != 0)
		{
			WMSG0("Throwing comment away\n");
			fCmtLen = 0;
			fCmtOffset = 0;
		} else {
			WMSG1("Got comment: '%s'\n", fComment);
		}
	}

	/*
	 * Extract the creator chunk, if any.
	 */
	if (fCreatorOffset > 0 && fCreatorLen > 0) {
		if (GetChunk(pGFD, fCreatorOffset - kOurHeaderLen, fCreatorLen,
			(void**) &fCreatorChunk) != 0)
		{
			WMSG0("Throwing creator chunk away\n");
			fCreatorLen = 0;
			fCreatorOffset = 0;
		} else {
			//WMSG1("Got creator chunk: '%s'\n", fCreatorChunk);
		}
	}

	return 0;
}

/*
 * Grab a chunk of data from a relative offset.
 */
int
TwoImgHeader::GetChunk(GenericFD* pGFD, di_off_t relOffset, long len,
	void** pBuf)
{
	DIError dierr;
	di_off_t curPos;

	/* remember current offset */
	curPos = pGFD->Tell();

	/* seek out to chunk and grab it */
	dierr = pGFD->Seek(relOffset, kSeekCur);
	if (dierr != kDIErrNone) {
		WMSG0("2MG seek to chunk failed\n");
		return -1;
	}

	assert(*pBuf == nil);
	*pBuf = new char[len+1];		// one extra, for null termination

	dierr = pGFD->Read(*pBuf, len);
	if (dierr != kDIErrNone) {
		WMSG0("2MG chunk read failed\n");
		delete[] (char*) (*pBuf);
		*pBuf = nil;
		(void) pGFD->Seek(curPos, kSeekSet);
		return -1;
	}

	/* null-terminate, in case this was a string */
	((char*) *pBuf)[len] = '\0';

	/* seek back to where we were */
	(void) pGFD->Seek(curPos, kSeekSet);

	return 0;
}

/*
 * Grab a chunk of data from a relative offset.
 */
int
TwoImgHeader::GetChunk(FILE* fp, di_off_t relOffset, long len,
	void** pBuf)
{
	long curPos;
	int count;

	/* remember current offset */
	curPos = ftell(fp);
	WMSG1("Current offset=%ld\n", curPos);

	/* seek out to chunk and grab it */
	if (fseek(fp, (long) relOffset, SEEK_CUR) == -1) {
		WMSG0("2MG seek to chunk failed\n");
		return errno ? errno : -1;;
	}

	assert(*pBuf == nil);
	*pBuf = new char[len+1];		// one extra, for null termination

	count = fread(*pBuf, len, 1, fp);
	if (!count || ferror(fp) || feof(fp)) {
		WMSG0("2MG chunk read failed\n");
		delete[] (char*) (*pBuf);
		*pBuf = nil;
		(void) fseek(fp, curPos, SEEK_SET);
		clearerr(fp);
		return errno ? errno : -1;;
	}

	/* null-terminate, in case this was a string */
	((char*) *pBuf)[len] = '\0';

	/* seek back to where we were */
	(void) fseek(fp, curPos, SEEK_SET);

	return 0;
}


/*
 * Unpack the 64-byte 2MG header.
 *
 * Performs some sanity checks.  Returns 0 on success, -1 on failure.
 */
int
TwoImgHeader::UnpackHeader(const unsigned char* buf, long totalLength)
{
	fMagic = GetLongBE(&buf[0x00]);
	fCreator = GetLongBE(&buf[0x04]);
	fHeaderLen = GetShortLE(&buf[0x08]);
	fVersion = GetShortLE(&buf[0x0a]);
	fImageFormat = GetLongLE(&buf[0x0c]);
	fFlags = GetLongLE(&buf[0x10]);
	fNumBlocks = GetLongLE(&buf[0x14]);
	fDataOffset = GetLongLE(&buf[0x18]);
	fDataLen = GetLongLE(&buf[0x1c]);
	fCmtOffset = GetLongLE(&buf[0x20]);
	fCmtLen = GetLongLE(&buf[0x24]);
	fCreatorOffset = GetLongLE(&buf[0x28]);
	fCreatorLen = GetLongLE(&buf[0x2c]);
	fSpare[0] = GetLongLE(&buf[0x30]);
	fSpare[1] = GetLongLE(&buf[0x34]);
	fSpare[2] = GetLongLE(&buf[0x38]);
	fSpare[3] = GetLongLE(&buf[0x3c]);

	fMagicStr[0] = (char) (fMagic >> 24);
	fMagicStr[1] = (char) (fMagic >> 16);
	fMagicStr[2] = (char) (fMagic >> 8);
	fMagicStr[3] = (char) fMagic;
	fMagicStr[4] = '\0';
	fCreatorStr[0] = (char) (fCreator >> 24);
	fCreatorStr[1] = (char) (fCreator >> 16);
	fCreatorStr[2] = (char) (fCreator >> 8);
	fCreatorStr[3] = (char) fCreator;
	fCreatorStr[4] = '\0';

    if (fMagic != kMagic) {
        WMSG0("Magic number does not match 2IMG\n");
        return -1;
    }

    if (fVersion > 1) {
        WMSG1("ERROR: unsupported version=%d\n", fVersion);
		return -1;		// bad header until I hear otherwise
    }

	if (fFlags & kDOSVolumeSet)
		fDOSVolumeNum = fFlags & kDOSVolumeMask;

	DumpHeader();

	/* fix broken 'WOOF' images from Sweet-16 */
	if (fCreator == kCreatorSweet16 && fDataLen == 0 &&
		fImageFormat != kImageFormatNibble)
	{
		fDataLen = fNumBlocks * kBlockSize;
		WMSG1("NOTE: fixing zero dataLen in 'WOOF' image (set to %ld)\n",
			fDataLen);
	}

	/*
	 * Perform some sanity checks.
	 */
	if (fImageFormat != kImageFormatNibble &&
        fNumBlocks * kBlockSize != fDataLen)
    {
		WMSG2("numBlocks/dataLen mismatch (%ld vs %ld)\n",
			fNumBlocks * kBlockSize, fDataLen);
		return -1;
	}
	if (fDataLen + fDataOffset > totalLength) {
		WMSG3("Invalid dataLen/offset/fileLength (dl=%ld, off=%ld, tlen=%ld)\n",
			fDataLen, fDataOffset, totalLength);
		return -1;
	}
    if (fImageFormat < kImageFormatDOS || fImageFormat > kImageFormatNibble) {
        WMSG1("Invalid image format %ld\n", fImageFormat);
        return -1;
    }

	if (fCmtOffset > 0 && fCmtOffset < fDataOffset + fDataLen) {
		WMSG2("2MG comment is inside the data section (off=%ld, data end=%ld)\n",
			fCmtOffset, fDataOffset+fDataLen);
		DebugBreak();
		// ignore the comment
		fCmtOffset = 0;
		fCmtLen = 0;
	}
	if (fCreatorOffset > 0 && fCreatorLen > 0) {
		long prevEnd = fDataOffset + fDataLen + fCmtLen;

		if (fCreatorOffset < prevEnd) {
			WMSG2("2MG creator chunk is inside prev data (off=%ld, data end=%ld)\n",
				fCreatorOffset, prevEnd);
			DebugBreak();
			// ignore the creator chunk
			fCreatorOffset = 0;
			fCreatorLen = 0;
		}
	}

    return 0;
}

/*
 * Write the header to a 2IMG file.
 *
 * Returns 0 on success, or an errno value on failure.
 */
int
TwoImgHeader::WriteHeader(FILE* fp) const
{
	unsigned char buf[kOurHeaderLen];

	PackHeader(buf);
	if (fwrite(buf, kOurHeaderLen, 1, fp) != 1)
		return errno ? errno : -1;
	return 0;
}

/*
 * Write the header to a 2IMG file.
 *
 * Returns 0 on success, or an errno value on failure.
 */
int
TwoImgHeader::WriteHeader(GenericFD* pGFD) const
{
	unsigned char buf[kOurHeaderLen];

	PackHeader(buf);

	if (pGFD->Write(buf, kOurHeaderLen) != kDIErrNone)
		return -1;

	return 0;
}

/*
 * Write the footer.  File must be seeked to end of data chunk.
 */
int
TwoImgHeader::WriteFooter(FILE* fp) const
{
	WMSG1("Writing footer at offset=%ld\n", (long) ftell(fp));

	if (fCmtLen) {
		fwrite(fComment, fCmtLen, 1, fp);
	}
	if (fCreatorLen) {
		fwrite(fCreatorChunk, fCreatorLen, 1, fp);
	}
	if (ferror(fp))
		return errno ? errno : -1;

	return 0;
}


/*
 * Write the footer.  File must be seeked to end of data chunk.
 */
int
TwoImgHeader::WriteFooter(GenericFD* pGFD) const
{
	WMSG1("Writing footer at offset=%ld\n", (long) pGFD->Tell());

	if (fCmtLen) {
		if (pGFD->Write(fComment, fCmtLen) != kDIErrNone)
			return -1;
	}
	if (fCreatorLen) {
		if (pGFD->Write(fCreatorChunk, fCreatorLen) != kDIErrNone)
			return -1;
	}

	return 0;
}

/*
 * Pack the header values into a 64-byte buffer.
 */
void
TwoImgHeader::PackHeader(unsigned char* buf) const
{
	if (fCmtLen > 0 && fCmtOffset == 0) {
		assert(false);
	}
	if (fCreatorLen > 0 && fCreatorOffset == 0) {
		assert(false);
	}

	PutLongBE(&buf[0x00], fMagic);
	PutLongBE(&buf[0x04], fCreator);
	PutShortLE(&buf[0x08], fHeaderLen);
	PutShortLE(&buf[0x0a], fVersion);
	PutLongLE(&buf[0x0c], fImageFormat);
	PutLongLE(&buf[0x10], fFlags);
	PutLongLE(&buf[0x14], fNumBlocks);
	PutLongLE(&buf[0x18], fDataOffset);
	PutLongLE(&buf[0x1c], fDataLen);
	PutLongLE(&buf[0x20], fCmtOffset);
	PutLongLE(&buf[0x24], fCmtLen);
	PutLongLE(&buf[0x28], fCreatorOffset);
	PutLongLE(&buf[0x2c], fCreatorLen);
	PutLongLE(&buf[0x30], fSpare[0]);
	PutLongLE(&buf[0x34], fSpare[1]);
	PutLongLE(&buf[0x38], fSpare[2]);
	PutLongLE(&buf[0x3c], fSpare[3]);
}

/*
 * Dump the contents of an ImgHeader.
 */
void
TwoImgHeader::DumpHeader(void) const
{
	WMSG0("--- header contents:\n");
	WMSG2("\tmagic         = '%s' (0x%08lx)\n", fMagicStr, fMagic);
	WMSG2("\tcreator       = '%s' (0x%08lx)\n", fCreatorStr, fCreator);
	WMSG1("\theaderLen     = %d\n", fHeaderLen);
	WMSG1("\tversion       = %d\n", fVersion);
	WMSG1("\timageFormat   = %ld\n", fImageFormat);
	WMSG1("\tflags         = 0x%08lx\n", fFlags);
	WMSG1("\t  locked      = %s\n",
		(fFlags & kFlagLocked) ? "true" : "false");
	WMSG2("\t  DOS volume  = %s (%ld)\n",
		(fFlags & kDOSVolumeSet) ? "true" : "false",
		fFlags & kDOSVolumeMask);
	WMSG1("\tnumBlocks     = %ld\n", fNumBlocks);
	WMSG1("\tdataOffset    = %ld\n", fDataOffset);
	WMSG1("\tdataLen       = %ld\n", fDataLen);
	WMSG1("\tcmtOffset     = %ld\n", fCmtOffset);
	WMSG1("\tcmtLen        = %ld\n", fCmtLen);
	WMSG1("\tcreatorOffset = %ld\n", fCreatorOffset);
	WMSG1("\tcreatorLen    = %ld\n", fCreatorLen);
	WMSG0("\n");
}

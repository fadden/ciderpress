/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert from one image format to another.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include "../diskimg/DiskImg.h"
#include "../nufxlib/NufxLib.h"

using namespace DiskImgLib;

#define nil NULL
#define ASSERT assert
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

FILE* gLog = nil;
pid_t gPid = getpid();


/*
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MsgHandler(const char* file, int line, const char* msg)
{
    ASSERT(file != nil);
    ASSERT(msg != nil);

    fprintf(gLog, "%05u %s", gPid, msg);
}
/*
 * Handle a global error message from the NufxLib library by shoving it
 * through the DiskImgLib message function.
 */
NuResult
NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    if (pErrorMessage->isDebug) {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> [D] %s\n", pErrorMessage->message);
    } else {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> %s\n", pErrorMessage->message);
    }

    return kNuOK;
}

/*
 * Convert one disk image to another.
 */
DIError
Convert(const char* infile, const char* outfile)
{
    DIError dierr = kDIErrNone;
    DiskImg srcImg, dstImg;
    const char* storageName = nil;

    printf("Converting in='%s' out='%s'\n", infile, outfile);

    /*
     * Prepare the source image.
     */
    dierr = srcImg.OpenImage(infile, '/', true);
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Unable to open disk image: %s.\n",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    dierr = srcImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Unable to determine source image format.\n");
        goto bail;
    }

    if (!srcImg.GetHasBlocks() && !srcImg.GetHasSectors()) {
        /* add nibble tracks someday */
        fprintf(stderr,
            "Sorry, only block- or sector-addressable images allowed.\n");
        dierr = kDIErrUnsupportedPhysicalFmt;
        goto bail;
    }
    if (srcImg.GetHasBlocks()) {
        assert(srcImg.GetNumBlocks() > 0);
    } else {
        assert(srcImg.GetNumTracks() > 0);
    }

    if (srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown) {
        fprintf(stderr, "(QUERY) don't know sector order\n");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    storageName = "MyHappyDisk";

    /* force the access to be ProDOS-ordered */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Couldn't switch to generic ProDOS: %s.\n",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /* transfer the DOS volume num, if one was set */
    printf("DOS volume number set to %d\n", srcImg.GetDOSVolumeNum());
    dstImg.SetDOSVolumeNum(srcImg.GetDOSVolumeNum());

    const DiskImg::NibbleDescr* pNibbleDescr;
    pNibbleDescr = nil;

    /*
     * Prepare the destination image.
     *
     * We *always* use DiskImg::kFormatGenericProDOSOrd here, because it
     * must match up with what we selected above.
     *
     * We could enable "skipFormat" on all of these but the nibble images,
     * but we go ahead and set it to "false" on all of them just for fun.
     */
    switch (18) {
    case 0:
        /* 16-sector nibble image, by blocks */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatNib525_6656,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 1:
        /* 16-sector nibble image, by tracks/sectors */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatNib525_6656,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 16,
                    false);
        break;
    case 2:
        /* 16-sector NB2 nibble image, by tracks/sectors */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatNib525_6384,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 16,
                    false);
        break;
    case 3:
        /* 13-sector nibble image, by tracks/sectors */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS32Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatNib525_6656,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 13,
                    false);
        break;
    case 4:
        /* 16-sector nb2 image, by tracks/sectors */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatNib525_6384,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 16,
                    false);
        break;
    case 5:
        /* sector image, by blocks, ProDOS order */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 6:
        /* sector image, by blocks, DOS order */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 7:
        /* sector image, by blocks, ProDOS order, Sim2e */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatSim2eHDV,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 8:
        /* odd-length HUGE sector image, by blocks */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    65535,
                    false);
        break;
    case 9:
        /* sector image, by blocks, physical order, with gzip */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatGzip,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 10:
        /* sector image, by blocks, ProDOS order, with gzip */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatGzip,
                    DiskImg::kFileFormatSim2eHDV,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 11:
        /* sector image, by blocks, ProDOS order, 2MG */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormat2MG,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 12:
        /* 16-sector nibble image, by tracks/sectors, 2MG */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormat2MG,
                    DiskImg::kPhysicalFormatNib525_6656,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 16,
                    false);
        break;
    case 13:
        /* 16-sector nibble image, by tracks/sectors, 2MG, gzip */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatGzip,
                    DiskImg::kFileFormat2MG,
                    DiskImg::kPhysicalFormatNib525_6656,
                    pNibbleDescr,
                    DiskImg::kSectorOrderPhysical,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 16,
                    false);
        break;
    case 14:
        /* sector image, by blocks, for DC42 (800K only) */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatDiskCopy42,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 15:
        /* sector image, by blocks, for NuFX */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatNuFX,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 16:
        /* sector image, by blocks, for DDD */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatDDD,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 17:
        /* sector image, by blocks, ProDOS order, stored in ZIP (.po.zip) */
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatZip,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false);
        break;
    case 18:
        /* 13-sector nibble image, by tracks/sectors */
        pNibbleDescr= DiskImg::GetStdNibbleDescr(DiskImg::kNibbleDescrDOS33Std);
        dierr = dstImg.CreateImage(outfile, storageName,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    pNibbleDescr,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    35, 13,
                    false);
        break;
    default:
        fprintf(stderr, "UNEXPECTED NUMBER\n");
        abort();
    }
    if (dierr != kDIErrNone) {
        fprintf(stderr, "Couldn't create new image file '%s': %s.\n",
            outfile, DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /*
     * Copy blocks or sectors from source to destination.
     */
    if (srcImg.GetHasBlocks()) {
        int numBlocks;
        numBlocks = srcImg.GetNumBlocks();
        if (dstImg.GetNumBlocks() < srcImg.GetNumBlocks())
            numBlocks = dstImg.GetNumBlocks();
        printf("Copying %d blocks\n", numBlocks);

        unsigned char blkBuf[512];
        for (int block = 0; block < numBlocks; block++) {
            dierr = srcImg.ReadBlock(block, blkBuf);
            if (dierr != kDIErrNone) {
                fprintf(stderr, "ERROR: ReadBlock failed (err=%d)\n", dierr);
                goto bail;
            }
            dierr = dstImg.WriteBlock(block, blkBuf);
            if (dierr != kDIErrNone) {
                fprintf(stderr, "ERROR: WriteBlock failed (err=%d)\n", dierr);
                goto bail;
            }
        }
    } else {
        int numTracks, numSectPerTrack;
        numTracks = srcImg.GetNumTracks();
        numSectPerTrack = srcImg.GetNumSectPerTrack();
        if (dstImg.GetNumTracks() < srcImg.GetNumTracks())
            numTracks = dstImg.GetNumTracks();
        if (dstImg.GetNumSectPerTrack() < srcImg.GetNumSectPerTrack())
            numSectPerTrack = dstImg.GetNumSectPerTrack();
        printf("Copying %d tracks of %d sectors\n", numTracks, numSectPerTrack);

        unsigned char sctBuf[256];
        for (int track = 0; track < numTracks; track++) {
            for (int sector = 0; sector < numSectPerTrack; sector++) {
                dierr = srcImg.ReadTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    fprintf(stderr,
                        "WARNING: ReadTrackSector failed on T=%d S=%d (err=%d)\n",
                        track, sector, dierr);
                    dierr = kDIErrNone;     // allow bad blocks
                    memset(sctBuf, 0, sizeof(sctBuf));
                }
                dierr = dstImg.WriteTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    fprintf(stderr,
                        "ERROR: WriteBlock failed on T=%d S=%d (err=%d)\n",
                        track, sector, dierr);
                    goto bail;
                }
            }
        }
    }

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: srcImg close failed?!\n");
        goto bail;
    }

    dierr = dstImg.CloseImage();
    if (dierr != kDIErrNone) {
        fprintf(stderr, "ERROR: dstImg close failed (err=%d)\n", dierr);
        goto bail;
    }

    assert(dierr == kDIErrNone);
bail:
    return dierr;
}

/*
 * Process every argument.
 */
int
main(int argc, char** argv)
{
    const char* kLogFile = "iconv-log.txt";

    if (argc != 3) {
        fprintf(stderr, "%s: infile outfile\n", argv[0]);
        exit(2);
    }

    gLog = fopen(kLogFile, "w");
    if (gLog == nil) {
        fprintf(stderr, "ERROR: unable to open log file\n");
        exit(1);
    }

    printf("Image Converter for Linux v1.0\n");
    printf("Copyright (C) 2014 by faddenSoft.  All rights reserved.\n");
    int32_t major, minor, bug;
    Global::GetVersion(&major, &minor, &bug);
    printf("Linked against DiskImg library v%d.%d.%d\n",
        major, minor, bug);
    printf("Log file is '%s'\n", kLogFile);
    printf("\n");

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

    Convert(argv[1], argv[2]);

    Global::AppCleanup();
    fclose(gLog);

    exit(0);
}


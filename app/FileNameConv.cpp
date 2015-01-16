/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Filename manipulation, including file type preservation.  This is
 * substantially ripped from NuLib2.
 */
#include "stdafx.h"
#include "FileNameConv.h"
#include "GenericArchive.h"
#include "AddFilesDialog.h"
#include <ctype.h>


#define WINDOWS_LIKE

/* replace unsupported chars with '%xx' */
#define kForeignIndic   '%'

/* convert single hex digit char to number */
#define HexDigit(x) ( !isxdigit((int)(x)) ? -1 : \
            (x) <= '9' ? (x) - '0' : toupper(x) +10 - 'A' )

/* convert number from 0-15 to hex digit */
#define HexConv(x)  ( ((unsigned int)(x)) <= 15 ? \
            ( (x) <= 9 ? (x) + '0' : (x) -10 + 'A') : -1 )


/*
 * ===========================================================================
 *      Common definitions
 * ===========================================================================
 */

#define kPreserveIndic  '#'     /* use # rather than $ for hex indication */
#define kFilenameExtDelim '.'   /* separates extension from filename */
#define kResourceFlag   'r'
#define kDiskImageFlag  'i'
#define kMaxExtLen      5       /* ".1234" */
#define kResourceStr    L"_rsrc_"

/* must be longer then strlen(kResourceStr)... no problem there */
#define kMaxPathGrowth  (sizeof(L"#XXXXXXXXYYYYYYYYZ")-1 + kMaxExtLen+1)


/* ProDOS file type names; must be entirely in upper case */
static const WCHAR gFileTypeNames[256][4] = {
    L"NON", L"BAD", L"PCD", L"PTX", L"TXT", L"PDA", L"BIN", L"FNT",
    L"FOT", L"BA3", L"DA3", L"WPF", L"SOS", L"$0D", L"$0E", L"DIR",
    L"RPD", L"RPI", L"AFD", L"AFM", L"AFR", L"SCL", L"PFS", L"$17",
    L"$18", L"ADB", L"AWP", L"ASP", L"$1C", L"$1D", L"$1E", L"$1F",
    L"TDM", L"$21", L"$22", L"$23", L"$24", L"$25", L"$26", L"$27",
    L"$28", L"$29", L"8SC", L"8OB", L"8IC", L"8LD", L"P8C", L"$2F",
    L"$30", L"$31", L"$32", L"$33", L"$34", L"$35", L"$36", L"$37",
    L"$38", L"$39", L"$3A", L"$3B", L"$3C", L"$3D", L"$3E", L"$3F",
    L"DIC", L"OCR", L"FTD", L"$43", L"$44", L"$45", L"$46", L"$47",
    L"$48", L"$49", L"$4A", L"$4B", L"$4C", L"$4D", L"$4E", L"$4F",
    L"GWP", L"GSS", L"GDB", L"DRW", L"GDP", L"HMD", L"EDU", L"STN",
    L"HLP", L"COM", L"CFG", L"ANM", L"MUM", L"ENT", L"DVU", L"FIN",
    L"$60", L"$61", L"$62", L"$63", L"$64", L"$65", L"$66", L"$67",
    L"$68", L"$69", L"$6A", L"BIO", L"$6C", L"TDR", L"PRE", L"HDV",
    L"$70", L"$71", L"$72", L"$73", L"$74", L"$75", L"$76", L"$77",
    L"$78", L"$79", L"$7A", L"$7B", L"$7C", L"$7D", L"$7E", L"$7F",
    L"$80", L"$81", L"$82", L"$83", L"$84", L"$85", L"$86", L"$87",
    L"$88", L"$89", L"$8A", L"$8B", L"$8C", L"$8D", L"$8E", L"$8F",
    L"$90", L"$91", L"$92", L"$93", L"$94", L"$95", L"$96", L"$97",
    L"$98", L"$99", L"$9A", L"$9B", L"$9C", L"$9D", L"$9E", L"$9F",
    L"WP ", L"$A1", L"$A2", L"$A3", L"$A4", L"$A5", L"$A6", L"$A7",
    L"$A8", L"$A9", L"$AA", L"GSB", L"TDF", L"BDF", L"$AE", L"$AF",
    L"SRC", L"OBJ", L"LIB", L"S16", L"RTL", L"EXE", L"PIF", L"TIF",
    L"NDA", L"CDA", L"TOL", L"DVR", L"LDF", L"FST", L"$BE", L"DOC",
    L"PNT", L"PIC", L"ANI", L"PAL", L"$C4", L"OOG", L"SCR", L"CDV",
    L"FON", L"FND", L"ICN", L"$CB", L"$CC", L"$CD", L"$CE", L"$CF",
    L"$D0", L"$D1", L"$D2", L"$D3", L"$D4", L"MUS", L"INS", L"MDI",
    L"SND", L"$D9", L"$DA", L"DBM", L"$DC", L"DDD", L"$DE", L"$DF",
    L"LBR", L"$E1", L"ATK", L"$E3", L"$E4", L"$E5", L"$E6", L"$E7",
    L"$E8", L"$E9", L"$EA", L"$EB", L"$EC", L"$ED", L"R16", L"PAS",
    L"CMD", L"$F1", L"$F2", L"$F3", L"$F4", L"$F5", L"$F6", L"$F7",
    L"$F8", L"OS ", L"INT", L"IVR", L"BAS", L"VAR", L"REL", L"SYS"
};

static const WCHAR kUnknownTypeStr[] = L"???";

/*static*/ const WCHAR* PathProposal::FileTypeString(uint32_t fileType)
{
    // Note to self: code down below tests first char for '?'.
    if (fileType < NELEM(gFileTypeNames))
        return gFileTypeNames[fileType];
    else
        return kUnknownTypeStr;
}


/*
 * Some file extensions we recognize.  When adding files with "extended"
 * preservation mode, we try to assign types to files that weren't
 * explicitly preserved, but nevertheless have a recognizeable type.
 *
 * geoff at gwlink.net pointed out that this really ought to be in an external
 * file rather than a hard-coded table.  Ought to fix that someday.
 */
static const struct {
    const WCHAR*    label;
    uint8_t         fileType;
    uint16_t        auxType;
} gRecognizedExtensions[] = {
    { L"ASM",  0xb0, 0x0003 },      /* APW assembly source */
    { L"C",    0xb0, 0x000a },      /* APW C source */
    { L"H",    0xb0, 0x000a },      /* APW C header */
    { L"CPP",  0xb0, 0x0000 },      /* generic source file */
    { L"BNY",  0xe0, 0x8000 },      /* Binary II lib */
    { L"BQY",  0xe0, 0x8000 },      /* Binary II lib, w/ compress */
    { L"BXY",  0xe0, 0x8000 },      /* Binary II wrap around SHK */
    { L"BSE",  0xe0, 0x8000 },      /* Binary II wrap around SEA */
    { L"SEA",  0xb3, 0xdb07 },      /* GSHK SEA */
    { L"TEXT", 0x04, 0x0000 },      /* ASCII Text */
    { L"GIF",  0xc0, 0x8006 },      /* GIF image */
    { L"JPG",  0x06, 0x0000 },      /* JPEG (nicer than 'NON') */
    { L"JPEG", 0x06, 0x0000 },      /* JPEG (nicer than 'NON') */
    //{ L"ACU",  0xe0, 0x8001 },      /* ACU archive */
    { L"SHK",  0xe0, 0x8002 },      /* ShrinkIt archive */
};

/*
 * Description table.
 *
 * The first item that matches will be used, but the table is searched
 * bottom-up, so it's important to have the most general entry first.
 *
 * In retrospect, it might have made sense to use the same format as the
 * "FTD" file type description file that the IIgs Finder used.  Might have
 * made sense to just ship that and load it on startup (although copyright
 * issues would have to be investigated).
 *
 * This list should be complete as of the May 1992 "about" note.
 */
static const struct {
    uint8_t         fileType;
    uint16_t        minAuxType;     // start of range for which this applies
    uint16_t        maxAuxType;     // end of range
    const WCHAR*    descr;
} gTypeDescriptions[] = {
    /*NON*/ { 0x00, 0x0000, 0xffff, L"Untyped file" },
    /*BAD*/ { 0x01, 0x0000, 0xffff, L"Bad blocks" },
    /*PCD*/ { 0x02, 0x0000, 0xffff, L"Pascal code" },
    /*PTX*/ { 0x03, 0x0000, 0xffff, L"Pascal text" },
    /*TXT*/ { 0x04, 0x0000, 0xffff, L"ASCII text" },
    /*PDA*/ { 0x05, 0x0000, 0xffff, L"Pascal data" },
    /*BIN*/ { 0x06, 0x0000, 0xffff, L"Binary" },
    /*FNT*/ { 0x07, 0x0000, 0xffff, L"Apple /// font" },
    /*FOT*/ { 0x08, 0x0000, 0xffff, L"Apple II or /// graphics" },
    /*   */ { 0x08, 0x0000, 0x3fff, L"Apple II graphics" },
    /*   */ { 0x08, 0x4000, 0x4000, L"Packed hi-res image" },
    /*   */ { 0x08, 0x4001, 0x4001, L"Packed double hi-res image" },
    /*   */ { 0x08, 0x8001, 0x8001, L"Printographer packed HGR file" },
    /*   */ { 0x08, 0x8002, 0x8002, L"Printographer packed DHGR file" },
    /*   */ { 0x08, 0x8003, 0x8003, L"Softdisk hi-res image" },
    /*   */ { 0x08, 0x8004, 0x8004, L"Softdisk double hi-res image" },
    /*BA3*/ { 0x09, 0x0000, 0xffff, L"Apple /// BASIC program" },
    /*DA3*/ { 0x0a, 0x0000, 0xffff, L"Apple /// BASIC data" },
    /*WPF*/ { 0x0b, 0x0000, 0xffff, L"Apple II or /// word processor" },
    /*   */ { 0x0b, 0x8001, 0x8001, L"Write This Way document" },
    /*   */ { 0x0b, 0x8002, 0x8002, L"Writing & Publishing document" },
    /*SOS*/ { 0x0c, 0x0000, 0xffff, L"Apple /// SOS system" },
    /*DIR*/ { 0x0f, 0x0000, 0xffff, L"Folder" },
    /*RPD*/ { 0x10, 0x0000, 0xffff, L"Apple /// RPS data" },
    /*RPI*/ { 0x11, 0x0000, 0xffff, L"Apple /// RPS index" },
    /*AFD*/ { 0x12, 0x0000, 0xffff, L"Apple /// AppleFile discard" },
    /*AFM*/ { 0x13, 0x0000, 0xffff, L"Apple /// AppleFile model" },
    /*AFR*/ { 0x14, 0x0000, 0xffff, L"Apple /// AppleFile report format" },
    /*SCL*/ { 0x15, 0x0000, 0xffff, L"Apple /// screen library" },
    /*PFS*/ { 0x16, 0x0000, 0xffff, L"PFS document" },
    /*   */ { 0x16, 0x0001, 0x0001, L"PFS:File document" },
    /*   */ { 0x16, 0x0002, 0x0002, L"PFS:Write document" },
    /*   */ { 0x16, 0x0003, 0x0003, L"PFS:Graph document" },
    /*   */ { 0x16, 0x0004, 0x0004, L"PFS:Plan document" },
    /*   */ { 0x16, 0x0016, 0x0016, L"PFS internal data" },
    /*ADB*/ { 0x19, 0x0000, 0xffff, L"AppleWorks data base" },
    /*AWP*/ { 0x1a, 0x0000, 0xffff, L"AppleWorks word processor" },
    /*ASP*/ { 0x1b, 0x0000, 0xffff, L"AppleWorks spreadsheet" },
    /*TDM*/ { 0x20, 0x0000, 0xffff, L"Desktop Manager document" },
    /*???*/ { 0x21, 0x0000, 0xffff, L"Instant Pascal source" },
    /*???*/ { 0x22, 0x0000, 0xffff, L"UCSD Pascal volume" },
    /*???*/ { 0x29, 0x0000, 0xffff, L"Apple /// SOS dictionary" },
    /*8SC*/ { 0x2a, 0x0000, 0xffff, L"Apple II source code" },
    /*   */ { 0x2a, 0x8001, 0x8001, L"EBBS command script" },
    /*8OB*/ { 0x2b, 0x0000, 0xffff, L"Apple II object code" },
    /*   */ { 0x2b, 0x8001, 0x8001, L"GBBS Pro object Code" },
    /*8IC*/ { 0x2c, 0x0000, 0xffff, L"Apple II interpreted code" },
    /*   */ { 0x2c, 0x8003, 0x8003, L"APEX Program File" },
    /*   */ { 0x2c, 0x8005, 0x8005, L"EBBS tokenized command script" },
    /*8LD*/ { 0x2d, 0x0000, 0xffff, L"Apple II language data" },
    /*   */ { 0x2d, 0x8006, 0x8005, L"EBBS message bundle" },
    /*   */ { 0x2d, 0x8007, 0x8007, L"EBBS compressed message bundle" },
    /*P8C*/ { 0x2e, 0x0000, 0xffff, L"ProDOS 8 code module" },
    /*   */ { 0x2e, 0x8001, 0x8001, L"Davex 8 Command" },
    /*PTP*/ { 0x2e, 0x8002, 0x8002, L"Point-to-Point drivers" },
    /*PTP*/ { 0x2e, 0x8003, 0x8003, L"Point-to-Point code" },
    /*   */ { 0x2e, 0x8004, 0x8004, L"Softdisk printer driver" },
    /*DIC*/ { 0x40, 0x0000, 0xffff, L"Dictionary file" },
    /*???*/ { 0x41, 0x0000, 0xffff, L"OCR data" },
    /*   */ { 0x41, 0x8001, 0x8001, L"InWords OCR font table" },
    /*FTD*/ { 0x42, 0x0000, 0xffff, L"File type names" },
    /*???*/ { 0x43, 0x0000, 0xffff, L"Peripheral data" },
    /*   */ { 0x43, 0x8001, 0x8001, L"Express document" },
    /*???*/ { 0x44, 0x0000, 0xffff, L"Personal information" },
    /*   */ { 0x44, 0x8001, 0x8001, L"ResuMaker personal information" },
    /*   */ { 0x44, 0x8002, 0x8002, L"ResuMaker resume" },
    /*   */ { 0x44, 0x8003, 0x8003, L"II Notes document" },
    /*   */ { 0x44, 0x8004, 0x8004, L"Softdisk scrapbook document" },
    /*   */ { 0x44, 0x8005, 0x8005, L"Don't Forget document" },
    /*   */ { 0x44, 0x80ff, 0x80ff, L"What To Do data" },
    /*   */ { 0x44, 0xbeef, 0xbeef, L"Table Scraps scrapbook" },
    /*???*/ { 0x45, 0x0000, 0xffff, L"Mathematical document" },
    /*   */ { 0x45, 0x8001, 0x8001, L"GSymbolix 3D graph document" },
    /*   */ { 0x45, 0x8002, 0x8002, L"GSymbolix formula document" },
    /*???*/ { 0x46, 0x0000, 0xffff, L"AutoSave profiles" },
    /*   */ { 0x46, 0x8001, 0x8001, L"AutoSave profiles" },
    /*GWP*/ { 0x50, 0x0000, 0xffff, L"Apple IIgs Word Processor" },
    /*   */ { 0x50, 0x8001, 0x8001, L"DeluxeWrite document" },
    /*   */ { 0x50, 0x8003, 0x8003, L"Personal Journal document" },
    /*   */ { 0x50, 0x8010, 0x8010, L"AppleWorks GS word processor" },
    /*   */ { 0x50, 0x8011, 0x8011, L"Softdisk issue text" },
    /*   */ { 0x50, 0x5445, 0x5445, L"Teach document" },
    /*GSS*/ { 0x51, 0x0000, 0xffff, L"Apple IIgs spreadsheet" },
    /*   */ { 0x51, 0x8010, 0x8010, L"AppleWorks GS spreadsheet" },
    /*   */ { 0x51, 0x2358, 0x2358, L"QC Calc spreadsheet " },
    /*GDB*/ { 0x52, 0x0000, 0xffff, L"Apple IIgs data base" },
    /*   */ { 0x52, 0x8001, 0x8001, L"GTv database" },
    /*   */ { 0x52, 0x8010, 0x8010, L"AppleWorks GS data base" },
    /*   */ { 0x52, 0x8011, 0x8011, L"AppleWorks GS DB template" },
    /*   */ { 0x52, 0x8013, 0x8013, L"GSAS database" },
    /*   */ { 0x52, 0x8014, 0x8014, L"GSAS accounting journals" },
    /*   */ { 0x52, 0x8015, 0x8015, L"Address Manager document" },
    /*   */ { 0x52, 0x8016, 0x8016, L"Address Manager defaults" },
    /*   */ { 0x52, 0x8017, 0x8017, L"Address Manager index" },
    /*DRW*/ { 0x53, 0x0000, 0xffff, L"Drawing" },
    /*   */ { 0x53, 0x8002, 0x8002, L"Graphic Disk Labeler document" },
    /*   */ { 0x53, 0x8010, 0x8010, L"AppleWorks GS graphics" },
    /*GDP*/ { 0x54, 0x0000, 0xffff, L"Desktop publishing" },
    /*   */ { 0x54, 0x8002, 0x8002, L"GraphicWriter document" },
    /*   */ { 0x54, 0x8003, 0x8003, L"Label It document" },
    /*   */ { 0x54, 0x8010, 0x8010, L"AppleWorks GS Page Layout" },
    /*   */ { 0x54, 0xdd3e, 0xdd3e, L"Medley document" },
    /*HMD*/ { 0x55, 0x0000, 0xffff, L"Hypermedia" },
    /*   */ { 0x55, 0x0001, 0x0001, L"HyperCard IIgs stack" },
    /*   */ { 0x55, 0x8001, 0x8001, L"Tutor-Tech document" },
    /*   */ { 0x55, 0x8002, 0x8002, L"HyperStudio document" },
    /*   */ { 0x55, 0x8003, 0x8003, L"Nexus document" },
    /*   */ { 0x55, 0x8004, 0x8004, L"HyperSoft stack" },
    /*   */ { 0x55, 0x8005, 0x8005, L"HyperSoft card" },
    /*   */ { 0x55, 0x8006, 0x8006, L"HyperSoft external command" },
    /*EDU*/ { 0x56, 0x0000, 0xffff, L"Educational Data" },
    /*   */ { 0x56, 0x8001, 0x8001, L"Tutor-Tech scores" },
    /*   */ { 0x56, 0x8007, 0x8007, L"GradeBook data" },
    /*STN*/ { 0x57, 0x0000, 0xffff, L"Stationery" },
    /*   */ { 0x57, 0x8003, 0x8003, L"Music Writer format" },
    /*HLP*/ { 0x58, 0x0000, 0xffff, L"Help file" },
    /*   */ { 0x58, 0x8002, 0x8002, L"Davex 8 help file" },
    /*   */ { 0x58, 0x8005, 0x8005, L"Micol Advanced Basic help file" },
    /*   */ { 0x58, 0x8006, 0x8006, L"Locator help document" },
    /*   */ { 0x58, 0x8007, 0x8007, L"Personal Journal help" },
    /*   */ { 0x58, 0x8008, 0x8008, L"Home Refinancer help" },
    /*   */ { 0x58, 0x8009, 0x8009, L"The Optimizer help" },
    /*   */ { 0x58, 0x800a, 0x800a, L"Text Wizard help" },
    /*   */ { 0x58, 0x800b, 0x800b, L"WordWorks Pro help system" },
    /*   */ { 0x58, 0x800c, 0x800c, L"Sound Wizard help" },
    /*   */ { 0x58, 0x800d, 0x800d, L"SeeHear help system" },
    /*   */ { 0x58, 0x800e, 0x800e, L"QuickForms help system" },
    /*   */ { 0x58, 0x800f, 0x800f, L"Don't Forget help system" },
    /*COM*/ { 0x59, 0x0000, 0xffff, L"Communications file" },
    /*   */ { 0x59, 0x8002, 0x8002, L"AppleWorks GS communications" },
    /*CFG*/ { 0x5a, 0x0000, 0xffff, L"Configuration file" },
    /*   */ { 0x5a, 0x0000, 0x0000, L"Sound settings files" },
    /*   */ { 0x5a, 0x0002, 0x0002, L"Battery RAM configuration" },
    /*   */ { 0x5a, 0x0003, 0x0003, L"AutoLaunch preferences" },
    /*   */ { 0x5a, 0x0004, 0x0004, L"SetStart preferences" },
    /*   */ { 0x5a, 0x0005, 0x0005, L"GSBug configuration" },
    /*   */ { 0x5a, 0x0006, 0x0006, L"Archiver preferences" },
    /*   */ { 0x5a, 0x0007, 0x0007, L"Archiver table of contents" },
    /*   */ { 0x5a, 0x0008, 0x0008, L"Font Manager data" },
    /*   */ { 0x5a, 0x0009, 0x0009, L"Print Manager data" },
    /*   */ { 0x5a, 0x000a, 0x000a, L"IR preferences" },
    /*   */ { 0x5a, 0x8001, 0x8001, L"Master Tracks Jr. preferences" },
    /*   */ { 0x5a, 0x8002, 0x8002, L"GraphicWriter preferences" },
    /*   */ { 0x5a, 0x8003, 0x8003, L"Z-Link configuration" },
    /*   */ { 0x5a, 0x8004, 0x8004, L"JumpStart configuration" },
    /*   */ { 0x5a, 0x8005, 0x8005, L"Davex 8 configuration" },
    /*   */ { 0x5a, 0x8006, 0x8006, L"Nifty List configuration" },
    /*   */ { 0x5a, 0x8007, 0x8007, L"GTv videodisc configuration" },
    /*   */ { 0x5a, 0x8008, 0x8008, L"GTv Workshop configuration" },
    /*PTP*/ { 0x5a, 0x8009, 0x8009, L"Point-to-Point preferences" },
    /*   */ { 0x5a, 0x800a, 0x800a, L"ORCA/Disassembler preferences" },
    /*   */ { 0x5a, 0x800b, 0x800b, L"SnowTerm preferences" },
    /*   */ { 0x5a, 0x800c, 0x800c, L"My Word! preferences" },
    /*   */ { 0x5a, 0x800d, 0x800d, L"Chipmunk configuration" },
    /*   */ { 0x5a, 0x8010, 0x8010, L"AppleWorks GS configuration" },
    /*   */ { 0x5a, 0x8011, 0x8011, L"SDE Shell preferences" },
    /*   */ { 0x5a, 0x8012, 0x8012, L"SDE Editor preferences" },
    /*   */ { 0x5a, 0x8013, 0x8013, L"SDE system tab ruler" },
    /*   */ { 0x5a, 0x8014, 0x8014, L"Nexus configuration" },
    /*   */ { 0x5a, 0x8015, 0x8015, L"DesignMaster preferences" },
    /*   */ { 0x5a, 0x801a, 0x801a, L"MAX/Edit keyboard template" },
    /*   */ { 0x5a, 0x801b, 0x801b, L"MAX/Edit tab ruler set" },
    /*   */ { 0x5a, 0x801c, 0x801c, L"Platinum Paint preferences" },
    /*   */ { 0x5a, 0x801d, 0x801d, L"Sea Scan 1000" },
    /*   */ { 0x5a, 0x801e, 0x801e, L"Allison preferences" },
    /*   */ { 0x5a, 0x801f, 0x801f, L"Gold of the Americas options" },
    /*   */ { 0x5a, 0x8021, 0x8021, L"GSAS accounting setup" },
    /*   */ { 0x5a, 0x8022, 0x8022, L"GSAS accounting document" },
    /*   */ { 0x5a, 0x8023, 0x8023, L"UtilityLaunch preferences" },
    /*   */ { 0x5a, 0x8024, 0x8024, L"Softdisk configuration" },
    /*   */ { 0x5a, 0x8025, 0x8025, L"Quit-To configuration" },
    /*   */ { 0x5a, 0x8026, 0x8026, L"Big Edit Thing" },
    /*   */ { 0x5a, 0x8027, 0x8027, L"ZMaker preferences" },
    /*   */ { 0x5a, 0x8028, 0x8028, L"Minstrel configuration" },
    /*   */ { 0x5a, 0x8029, 0x8029, L"WordWorks Pro preferences" },
    /*   */ { 0x5a, 0x802b, 0x802b, L"Pointless preferences" },
    /*   */ { 0x5a, 0x802c, 0x802c, L"Micol Advanced Basic config" },
    /*   */ { 0x5a, 0x802e, 0x802e, L"Label It configuration" },
    /*   */ { 0x5a, 0x802f, 0x802f, L"Cool Cursor document" },
    /*   */ { 0x5a, 0x8030, 0x8030, L"Locator preferences" },
    /*   */ { 0x5a, 0x8031, 0x8031, L"Replicator preferences" },
    /*   */ { 0x5a, 0x8032, 0x8032, L"Kangaroo configuration" },
    /*   */ { 0x5a, 0x8033, 0x8033, L"Kangaroo data" },
    /*   */ { 0x5a, 0x8034, 0x8034, L"TransProg III configuration" },
    /*   */ { 0x5a, 0x8035, 0x8035, L"Home Refinancer preferences" },
    /*   */ { 0x5a, 0x8036, 0x8036, L"Easy Eyes settings" },
    /*   */ { 0x5a, 0x8037, 0x8037, L"The Optimizer settings" },
    /*   */ { 0x5a, 0x8038, 0x8038, L"Text Wizard settings" },
    /*   */ { 0x5a, 0x803b, 0x803b, L"Disk Access II preferences" },
    /*   */ { 0x5a, 0x803d, 0x803d, L"Quick DA configuration" },
    /*   */ { 0x5a, 0x803e, 0x803e, L"Crazy 8s preferences" },
    /*   */ { 0x5a, 0x803f, 0x803f, L"Sound Wizard settings" },
    /*   */ { 0x5a, 0x8041, 0x8041, L"Quick Window configuration" },
    /*   */ { 0x5a, 0x8044, 0x8044, L"Universe Master disk map" },
    /*   */ { 0x5a, 0x8046, 0x8046, L"Autopilot configuration" },
    /*   */ { 0x5a, 0x8047, 0x8047, L"EGOed preferences" },
    /*   */ { 0x5a, 0x8049, 0x8049, L"Quick DA preferences" },
    /*   */ { 0x5a, 0x804b, 0x804b, L"HardPressed volume preferences" },
    /*   */ { 0x5a, 0x804c, 0x804c, L"HardPressed global preferences" },
    /*   */ { 0x5a, 0x804d, 0x804d, L"HardPressed profile" },
    /*   */ { 0x5a, 0x8050, 0x8050, L"Don't Forget settings" },
    /*   */ { 0x5a, 0x8052, 0x8052, L"ProBOOT preferences" },
    /*   */ { 0x5a, 0x8054, 0x8054, L"Battery Brain preferences" },
    /*   */ { 0x5a, 0x8055, 0x8055, L"Rainbow configuration" },
    /*   */ { 0x5a, 0x8061, 0x8061, L"TypeSet preferences" },
    /*   */ { 0x5a, 0x8063, 0x8063, L"Cool Cursor preferences" },
    /*   */ { 0x5a, 0x806e, 0x806e, L"Balloon preferences" },
    /*   */ { 0x5a, 0x80fe, 0x80fe, L"Special Edition configuration" },
    /*   */ { 0x5a, 0x80ff, 0x80ff, L"Sun Dial preferences" },
    /*ANM*/ { 0x5b, 0x0000, 0xffff, L"Animation file" },
    /*   */ { 0x5b, 0x8001, 0x8001, L"Cartooners movie" },
    /*   */ { 0x5b, 0x8002, 0x8002, L"Cartooners actors" },
    /*   */ { 0x5b, 0x8005, 0x8005, L"Arcade King Super document" },
    /*   */ { 0x5b, 0x8006, 0x8006, L"Arcade King DHRG document" },
    /*   */ { 0x5b, 0x8007, 0x8007, L"DreamVision movie" },
    /*MUM*/ { 0x5c, 0x0000, 0xffff, L"Multimedia document" },
    /*   */ { 0x5c, 0x8001, 0x8001, L"GTv multimedia playlist" },
    /*ENT*/ { 0x5d, 0x0000, 0xffff, L"Game/Entertainment document" },
    /*   */ { 0x5d, 0x8001, 0x8001, L"Solitaire Royale document" },
    /*   */ { 0x5d, 0x8002, 0x8002, L"BattleFront scenario" },
    /*   */ { 0x5d, 0x8003, 0x8003, L"BattleFront saved game" },
    /*   */ { 0x5d, 0x8004, 0x8004, L"Gold of the Americas game" },
    /*   */ { 0x5d, 0x8006, 0x8006, L"Blackjack Tutor document" },
    /*   */ { 0x5d, 0x8008, 0x8008, L"Canasta document" },
    /*   */ { 0x5d, 0x800b, 0x800b, L"Word Search document" },
    /*   */ { 0x5d, 0x800c, 0x800c, L"Tarot deal" },
    /*   */ { 0x5d, 0x800d, 0x800d, L"Tarot tournament" },
    /*   */ { 0x5d, 0x800e, 0x800e, L"Full Metal Planet game" },
    /*   */ { 0x5d, 0x800f, 0x800f, L"Full Metal Planet player" },
    /*   */ { 0x5d, 0x8010, 0x8010, L"Quizzical high scores" },
    /*   */ { 0x5d, 0x8011, 0x8011, L"Meltdown high scores" },
    /*   */ { 0x5d, 0x8012, 0x8012, L"BlockWords high scores" },
    /*   */ { 0x5d, 0x8013, 0x8013, L"Lift-A-Gon scores" },
    /*   */ { 0x5d, 0x8014, 0x8014, L"Softdisk Adventure" },
    /*   */ { 0x5d, 0x8015, 0x8015, L"Blankety Blank document" },
    /*   */ { 0x5d, 0x8016, 0x8016, L"Son of Star Axe champion" },
    /*   */ { 0x5d, 0x8017, 0x8017, L"Digit Fidget high scores" },
    /*   */ { 0x5d, 0x8018, 0x8018, L"Eddie map" },
    /*   */ { 0x5d, 0x8019, 0x8019, L"Eddie tile set" },
    /*   */ { 0x5d, 0x8122, 0x8122, L"Wolfenstein 3D scenario" },
    /*   */ { 0x5d, 0x8123, 0x8123, L"Wolfenstein 3D saved game" },
    /*DVU*/ { 0x5e, 0x0000, 0xffff, L"Development utility document" },
    /*   */ { 0x5e, 0x0001, 0x0001, L"Resource file" },
    /*   */ { 0x5e, 0x8001, 0x8001, L"ORCA/Disassembler template" },
    /*   */ { 0x5e, 0x8003, 0x8003, L"DesignMaster document" },
    /*   */ { 0x5e, 0x8008, 0x8008, L"ORCA/C symbol file" },
    /*FIN*/ { 0x5f, 0x0000, 0xffff, L"Financial document" },
    /*   */ { 0x5f, 0x8001, 0x8001, L"Your Money Matters document" },
    /*   */ { 0x5f, 0x8002, 0x8002, L"Home Refinancer document" },
    /*BIO*/ { 0x6b, 0x0000, 0xffff, L"PC Transporter BIOS" },
    /*TDR*/ { 0x6d, 0x0000, 0xffff, L"PC Transporter driver" },
    /*PRE*/ { 0x6e, 0x0000, 0xffff, L"PC Transporter pre-boot" },
    /*HDV*/ { 0x6f, 0x0000, 0xffff, L"PC Transporter volume" },
    /*WP */ { 0xa0, 0x0000, 0xffff, L"WordPerfect document" },
    /*GSB*/ { 0xab, 0x0000, 0xffff, L"Apple IIgs BASIC program" },
    /*TDF*/ { 0xac, 0x0000, 0xffff, L"Apple IIgs BASIC TDF" },
    /*BDF*/ { 0xad, 0x0000, 0xffff, L"Apple IIgs BASIC data" },
    /*SRC*/ { 0xb0, 0x0000, 0xffff, L"Apple IIgs source code" },
    /*   */ { 0xb0, 0x0001, 0x0001, L"APW Text file" },
    /*   */ { 0xb0, 0x0003, 0x0003, L"APW 65816 Assembly source code" },
    /*   */ { 0xb0, 0x0005, 0x0005, L"ORCA/Pascal source code" },
    /*   */ { 0xb0, 0x0006, 0x0006, L"APW command file" },
    /*   */ { 0xb0, 0x0008, 0x0008, L"ORCA/C source code" },
    /*   */ { 0xb0, 0x0009, 0x0009, L"APW Linker command file" },
    /*   */ { 0xb0, 0x000a, 0x000a, L"APW C source code" },
    /*   */ { 0xb0, 0x000c, 0x000c, L"ORCA/Desktop command file" },
    /*   */ { 0xb0, 0x0015, 0x0015, L"APW Rez source file" },
    /*   */ { 0xb0, 0x0017, 0x0017, L"Installer script" },
    /*   */ { 0xb0, 0x001e, 0x001e, L"TML Pascal source code" },
    /*   */ { 0xb0, 0x0116, 0x0116, L"ORCA/Disassembler script" },
    /*   */ { 0xb0, 0x0503, 0x0503, L"SDE Assembler source code" },
    /*   */ { 0xb0, 0x0506, 0x0506, L"SDE command script" },
    /*   */ { 0xb0, 0x0601, 0x0601, L"Nifty List data" },
    /*   */ { 0xb0, 0x0719, 0x0719, L"PostScript file" },
    /*OBJ*/ { 0xb1, 0x0000, 0xffff, L"Apple IIgs object code" },
    /*LIB*/ { 0xb2, 0x0000, 0xffff, L"Apple IIgs Library file" },
    /*S16*/ { 0xb3, 0x0000, 0xffff, L"GS/OS application" },
    /*RTL*/ { 0xb4, 0x0000, 0xffff, L"GS/OS run-time library" },
    /*EXE*/ { 0xb5, 0x0000, 0xffff, L"GS/OS shell application" },
    /*PIF*/ { 0xb6, 0x0000, 0xffff, L"Permanent initialization file" },
    /*TIF*/ { 0xb7, 0x0000, 0xffff, L"Temporary initialization file" },
    /*NDA*/ { 0xb8, 0x0000, 0xffff, L"New desk accessory" },
    /*CDA*/ { 0xb9, 0x0000, 0xffff, L"Classic desk accessory" },
    /*TOL*/ { 0xba, 0x0000, 0xffff, L"Tool" },
    /*DVR*/ { 0xbb, 0x0000, 0xffff, L"Apple IIgs device driver file" },
    /*   */ { 0xbb, 0x7e01, 0x7e01, L"GNO/ME terminal device driver" },
    /*   */ { 0xbb, 0x7f01, 0x7f01, L"GTv videodisc serial driver" },
    /*   */ { 0xbb, 0x7f02, 0x7f02, L"GTv videodisc game port driver" },
    /*LDF*/ { 0xbc, 0x0000, 0xffff, L"Load file (generic)" },
    /*   */ { 0xbc, 0x4001, 0x4001, L"Nifty List module" },
    /*   */ { 0xbc, 0xc001, 0xc001, L"Nifty List module" },
    /*   */ { 0xbc, 0x4002, 0x4002, L"Super Info module" },
    /*   */ { 0xbc, 0xc002, 0xc002, L"Super Info module" },
    /*   */ { 0xbc, 0x4004, 0x4004, L"Twilight document" },
    /*   */ { 0xbc, 0xc004, 0xc004, L"Twilight document" },
    /*   */ { 0xbc, 0x4006, 0x4006, L"Foundation resource editor" },
    /*   */ { 0xbc, 0xc006, 0xc006, L"Foundation resource editor" },
    /*   */ { 0xbc, 0x4007, 0x4007, L"HyperStudio new button action" },
    /*   */ { 0xbc, 0xc007, 0xc007, L"HyperStudio new button action" },
    /*   */ { 0xbc, 0x4008, 0x4008, L"HyperStudio screen transition" },
    /*   */ { 0xbc, 0xc008, 0xc008, L"HyperStudio screen transition" },
    /*   */ { 0xbc, 0x4009, 0x4009, L"DreamGrafix module" },
    /*   */ { 0xbc, 0xc009, 0xc009, L"DreamGrafix module" },
    /*   */ { 0xbc, 0x400a, 0x400a, L"HyperStudio Extra utility" },
    /*   */ { 0xbc, 0xc00a, 0xc00a, L"HyperStudio Extra utility" },
    /*   */ { 0xbc, 0x400f, 0x400f, L"HardPressed module" },
    /*   */ { 0xbc, 0xc00f, 0xc00f, L"HardPressed module" },
    /*   */ { 0xbc, 0x4010, 0x4010, L"Graphic Exchange translator" },
    /*   */ { 0xbc, 0xc010, 0xc010, L"Graphic Exchange translator" },
    /*   */ { 0xbc, 0x4011, 0x4011, L"Desktop Enhancer blanker" },
    /*   */ { 0xbc, 0xc011, 0xc011, L"Desktop Enhancer blanker" },
    /*   */ { 0xbc, 0x4083, 0x4083, L"Marinetti link layer module" },
    /*   */ { 0xbc, 0xc083, 0xc083, L"Marinetti link layer module" },
    /*FST*/ { 0xbd, 0x0000, 0xffff, L"GS/OS File System Translator" },
    /*DOC*/ { 0xbf, 0x0000, 0xffff, L"GS/OS document" },
    /*PNT*/ { 0xc0, 0x0000, 0xffff, L"Packed super hi-res picture" },
    /*   */ { 0xc0, 0x0000, 0x0000, L"Paintworks packed picture" },
    /*   */ { 0xc0, 0x0001, 0x0001, L"Packed super hi-res image" },
    /*   */ { 0xc0, 0x0002, 0x0002, L"Apple Preferred Format picture" },
    /*   */ { 0xc0, 0x0003, 0x0003, L"Packed QuickDraw II PICT file" },
    /*   */ { 0xc0, 0x0080, 0x0080, L"TIFF document" },
    /*   */ { 0xc0, 0x0081, 0x0081, L"JFIF (JPEG) document" },
    /*   */ { 0xc0, 0x8001, 0x8001, L"GTv background image" },
    /*   */ { 0xc0, 0x8005, 0x8005, L"DreamGrafix document" },
    /*   */ { 0xc0, 0x8006, 0x8006, L"GIF document" },
    /*PIC*/ { 0xc1, 0x0000, 0xffff, L"Super hi-res picture" },
    /*   */ { 0xc1, 0x0000, 0x0000, L"Super hi-res screen image" },
    /*   */ { 0xc1, 0x0001, 0x0001, L"QuickDraw PICT file" },
    /*   */ { 0xc1, 0x0002, 0x0002, L"Super hi-res 3200-color screen image" },
    /*   */ { 0xc1, 0x8001, 0x8001, L"Allison raw image doc" },
    /*   */ { 0xc1, 0x8002, 0x8002, L"ThunderScan image doc" },
    /*   */ { 0xc1, 0x8003, 0x8003, L"DreamGrafix document" },
    /*ANI*/ { 0xc2, 0x0000, 0xffff, L"Paintworks animation" },
    /*PAL*/ { 0xc3, 0x0000, 0xffff, L"Paintworks palette" },
    /*OOG*/ { 0xc5, 0x0000, 0xffff, L"Object-oriented graphics" },
    /*   */ { 0xc5, 0x8000, 0x8000, L"Draw Plus document" },
    /*   */ { 0xc5, 0xc000, 0xc000, L"DYOH architecture doc" },
    /*   */ { 0xc5, 0xc001, 0xc001, L"DYOH predrawn objects" },
    /*   */ { 0xc5, 0xc002, 0xc002, L"DYOH custom objects" },
    /*   */ { 0xc5, 0xc003, 0xc003, L"DYOH clipboard" },
    /*   */ { 0xc5, 0xc004, 0xc004, L"DYOH interiors document" },
    /*   */ { 0xc5, 0xc005, 0xc005, L"DYOH patterns" },
    /*   */ { 0xc5, 0xc006, 0xc006, L"DYOH landscape document" },
    /*   */ { 0xc5, 0xc007, 0xc007, L"PyWare Document" },
    /*SCR*/ { 0xc6, 0x0000, 0xffff, L"Script" },
    /*   */ { 0xc6, 0x8001, 0x8001, L"Davex 8 script" },
    /*   */ { 0xc6, 0x8002, 0x8002, L"Universe Master backup script" },
    /*   */ { 0xc6, 0x8003, 0x8003, L"Universe Master Chain script" },
    /*CDV*/ { 0xc7, 0x0000, 0xffff, L"Control Panel document" },
    /*FON*/ { 0xc8, 0x0000, 0xffff, L"Font" },
    /*   */ { 0xc8, 0x0000, 0x0000, L"Font (Standard Apple IIgs QuickDraw II Font)" },
    /*   */ { 0xc8, 0x0001, 0x0001, L"TrueType font resource" },
    /*   */ { 0xc8, 0x0008, 0x0008, L"Postscript font resource" },
    /*   */ { 0xc8, 0x0081, 0x0081, L"TrueType font file" },
    /*   */ { 0xc8, 0x0088, 0x0088, L"Postscript font file" },
    /*FND*/ { 0xc9, 0x0000, 0xffff, L"Finder data" },
    /*ICN*/ { 0xca, 0x0000, 0xffff, L"Icons" },
    /*MUS*/ { 0xd5, 0x0000, 0xffff, L"Music sequence" },
    /*   */ { 0xd5, 0x0000, 0x0000, L"Music Construction Set song" },
    /*   */ { 0xd5, 0x0001, 0x0001, L"MIDI Synth sequence" },
    /*   */ { 0xd5, 0x0007, 0x0007, L"SoundSmith document" },
    /*   */ { 0xd5, 0x8002, 0x8002, L"Diversi-Tune sequence" },
    /*   */ { 0xd5, 0x8003, 0x8003, L"Master Tracks Jr. sequence" },
    /*   */ { 0xd5, 0x8004, 0x8004, L"Music Writer document" },
    /*   */ { 0xd5, 0x8005, 0x8005, L"Arcade King Super music" },
    /*   */ { 0xd5, 0x8006, 0x8006, L"Music Composer file" },
    /*INS*/ { 0xd6, 0x0000, 0xffff, L"Instrument" },
    /*   */ { 0xd6, 0x0000, 0x0000, L"Music Construction Set instrument" },
    /*   */ { 0xd6, 0x0001, 0x0001, L"MIDI Synth instrument" },
    /*   */ { 0xd6, 0x8002, 0x8002, L"Diversi-Tune instrument" },
    /*MDI*/ { 0xd7, 0x0000, 0xffff, L"MIDI data" },
    /*   */ { 0xd7, 0x0000, 0x0000, L"MIDI standard data" },
    /*   */ { 0xd7, 0x0080, 0x0080, L"MIDI System Exclusive data" },
    /*   */ { 0xd7, 0x8001, 0x8001, L"MasterTracks Pro Sysex file" },
    /*SND*/ { 0xd8, 0x0000, 0xffff, L"Sampled sound" },
    /*   */ { 0xd8, 0x0000, 0x0000, L"Audio IFF document" },
    /*   */ { 0xd8, 0x0001, 0x0001, L"AIFF-C document" },
    /*   */ { 0xd8, 0x0002, 0x0002, L"ASIF instrument" },
    /*   */ { 0xd8, 0x0003, 0x0003, L"Sound resource file" },
    /*   */ { 0xd8, 0x0004, 0x0004, L"MIDI Synth wave data" },
    /*   */ { 0xd8, 0x8001, 0x8001, L"HyperStudio sound" },
    /*   */ { 0xd8, 0x8002, 0x8002, L"Arcade King Super sound" },
    /*   */ { 0xd8, 0x8003, 0x8003, L"SoundOff! sound bank" },
    /*DBM*/ { 0xdb, 0x0000, 0xffff, L"DB Master document" },
    /*   */ { 0xdb, 0x0001, 0x0001, L"DB Master document" },
    /*???*/ { 0xdd, 0x0000, 0xffff, L"DDD Deluxe archive" }, // unofficial
    /*LBR*/ { 0xe0, 0x0000, 0xffff, L"Archival library" },
    /*   */ { 0xe0, 0x0000, 0x0000, L"ALU library" },
    /*   */ { 0xe0, 0x0001, 0x0001, L"AppleSingle file" },
    /*   */ { 0xe0, 0x0002, 0x0002, L"AppleDouble header file" },
    /*   */ { 0xe0, 0x0003, 0x0003, L"AppleDouble data file" },
    /*   */ { 0xe0, 0x0004, 0x0004, L"Archiver archive" },
    /*   */ { 0xe0, 0x0005, 0x0005, L"DiskCopy 4.2 disk image" },
    /*   */ { 0xe0, 0x0100, 0x0100, L"Apple 5.25 disk image" },
    /*   */ { 0xe0, 0x0101, 0x0101, L"Profile 5MB disk image" },
    /*   */ { 0xe0, 0x0102, 0x0102, L"Profile 10MB disk image" },
    /*   */ { 0xe0, 0x0103, 0x0103, L"Apple 3.5 disk image" },
    /*   */ { 0xe0, 0x0104, 0x0104, L"SCSI device image" },
    /*   */ { 0xe0, 0x0105, 0x0105, L"SCSI hard disk image" },
    /*   */ { 0xe0, 0x0106, 0x0106, L"SCSI tape image" },
    /*   */ { 0xe0, 0x0107, 0x0107, L"SCSI CD-ROM image" },
    /*   */ { 0xe0, 0x010e, 0x010e, L"RAM disk image" },
    /*   */ { 0xe0, 0x010f, 0x010f, L"ROM disk image" },
    /*   */ { 0xe0, 0x0110, 0x0110, L"File server image" },
    /*   */ { 0xe0, 0x0113, 0x0113, L"Hard disk image" },
    /*   */ { 0xe0, 0x0114, 0x0114, L"Floppy disk image" },
    /*   */ { 0xe0, 0x0115, 0x0115, L"Tape image" },
    /*   */ { 0xe0, 0x011e, 0x011e, L"AppleTalk file server image" },
    /*   */ { 0xe0, 0x0120, 0x0120, L"DiskCopy 6 disk image" },
    /*   */ { 0xe0, 0x0130, 0x0130, L"Universal Disk Image file" },
    /*   */ { 0xe0, 0x8000, 0x8000, L"Binary II file" },
    /*   */ { 0xe0, 0x8001, 0x8001, L"AppleLink ACU document" },
    /*   */ { 0xe0, 0x8002, 0x8002, L"ShrinkIt (NuFX) document" },
    /*   */ { 0xe0, 0x8003, 0x8003, L"Universal Disk Image file" },
    /*   */ { 0xe0, 0x8004, 0x8004, L"Davex archived volume" },
    /*   */ { 0xe0, 0x8006, 0x8006, L"EZ Backup Saveset doc" },
    /*   */ { 0xe0, 0x8007, 0x8007, L"ELS DOS 3.3 volume" },
    /*   */ { 0xe0, 0x8008, 0x8008, L"UtilityWorks document" },
    /*   */ { 0xe0, 0x800a, 0x800a, L"Replicator document" },
    /*   */ { 0xe0, 0x800b, 0x800b, L"AutoArk compressed document" },
    /*   */ { 0xe0, 0x800d, 0x800d, L"HardPressed compressed data (data fork)" },
    /*   */ { 0xe0, 0x800e, 0x800e, L"HardPressed compressed data (rsrc fork)" },
    /*   */ { 0xe0, 0x800f, 0x800f, L"HardPressed compressed data (both forks)" },
    /*   */ { 0xe0, 0x8010, 0x8010, L"LHA archive" },
    /*ATK*/ { 0xe2, 0x0000, 0xffff, L"AppleTalk data" },
    /*   */ { 0xe2, 0xffff, 0xffff, L"EasyMount document" },
    /*R16*/ { 0xee, 0x0000, 0xffff, L"EDASM 816 relocatable file" },
    /*PAS*/ { 0xef, 0x0000, 0xffff, L"Pascal area" },
    /*CMD*/ { 0xf0, 0x0000, 0xffff, L"BASIC command" },
    /*???*/ { 0xf1, 0x0000, 0xffff, L"User type #1" },
    /*???*/ { 0xf2, 0x0000, 0xffff, L"User type #2" },
    /*???*/ { 0xf3, 0x0000, 0xffff, L"User type #3" },
    /*???*/ { 0xf4, 0x0000, 0xffff, L"User type #4" },
    /*???*/ { 0xf5, 0x0000, 0xffff, L"User type #5" },
    /*???*/ { 0xf6, 0x0000, 0xffff, L"User type #6" },
    /*???*/ { 0xf7, 0x0000, 0xffff, L"User type #7" },
    /*???*/ { 0xf8, 0x0000, 0xffff, L"User type #8" },
    /*OS */ { 0xf9, 0x0000, 0xffff, L"GS/OS system file" },
    /*OS */ { 0xfa, 0x0000, 0xffff, L"Integer BASIC program" },
    /*OS */ { 0xfb, 0x0000, 0xffff, L"Integer BASIC variables" },
    /*OS */ { 0xfc, 0x0000, 0xffff, L"AppleSoft BASIC program" },
    /*OS */ { 0xfd, 0x0000, 0xffff, L"AppleSoft BASIC variables" },
    /*OS */ { 0xfe, 0x0000, 0xffff, L"Relocatable code" },
    /*OS */ { 0xff, 0x0000, 0xffff, L"ProDOS 8 application" },
};

/*static*/ const WCHAR* PathProposal::FileTypeDescription(long fileType,
    long auxType)
{
    for (int i = NELEM(gTypeDescriptions)-1; i >= 0; i--) {
        if (fileType == gTypeDescriptions[i].fileType &&
            auxType >= gTypeDescriptions[i].minAuxType &&
            auxType <= gTypeDescriptions[i].maxAuxType)
        {
            return gTypeDescriptions[i].descr;
        }
    }

    return NULL;
}


/*
 * ===========================================================================
 *      Filename/filetype conversion
 * ===========================================================================
 */

void PathProposal::Init(GenericEntry* pEntry)
{
    // TODO(Unicode)
    //fStoredPathName = Charset::ConvertMORToUNI(pEntry->GetPathNameMOR());
    // can't do this yet -- the rest of the extraction path isn't ready
    fStoredPathName = pEntry->GetPathNameMOR();
    fStoredFssep = pEntry->GetFssep();
    //if (fStoredFssep == '\0')         // e.g. embedded DOS 3.3 volume
    //  fStoredFssep = kDefaultStoredFssep;
    fFileType = pEntry->GetFileType();
    fAuxType = pEntry->GetAuxType();
    //fThreadKind set from SelectionEntry
    // reset the "output" fields
    fLocalPathName = L":HOSED:";
    fLocalFssep = ']';
    // I expect these to be as-yet unset; check it
    ASSERT(!fPreservation);
    ASSERT(!fAddExtension);
    ASSERT(!fJunkPaths);
}

// init the "add to archive" side
void PathProposal::Init(const WCHAR* localPathName) {
    //ASSERT(basePathName[strlen(basePathName)-1] != kLocalFssep);
    //fLocalPathName = localPathName + strlen(basePathName)+1;
    fLocalPathName = localPathName;
    fLocalFssep = kLocalFssep;
    // reset the "output" fields
    fStoredPathName = L":HOSED:";
    fStoredFssep = '[';
    fFileType = 0;
    fAuxType = 0;
    fThreadKind = GenericEntry::kDataThread;
    // I expect these to be as-yet unset; check it
    ASSERT(!fPreservation);
    ASSERT(!fAddExtension);
    ASSERT(!fJunkPaths);
}

void PathProposal::ArchiveToLocal(void)
{
    WCHAR* pathBuf;
    const WCHAR* startp;
    const WCHAR* endp;
    WCHAR* dstp;
    int newBufLen;

    /* init output fields */
    fLocalFssep = kLocalFssep;
    fLocalPathName = L"";

    /*
     * Set up temporary buffer space.  The maximum possible expansion
     * requires converting all chars to '%' codes and adding the longest
     * possible preservation string.
     */
    newBufLen = fStoredPathName.GetLength()*3 + kMaxPathGrowth +1;
    pathBuf = fLocalPathName.GetBuffer(newBufLen);
    ASSERT(pathBuf != NULL);

    startp = fStoredPathName;
    dstp = pathBuf;
    while (*startp == fStoredFssep) {
        /* ignore leading path sep; always extract to current dir */
        startp++;
    }

    /* normalize all directory components and the filename component */
    while (startp != NULL) {
        endp = NULL;
        if (fStoredFssep != '\0')
            endp = wcschr(startp, fStoredFssep);
        if (endp != NULL && endp == startp) {
            /* zero-length subdir component */
            LOGI("WARNING: zero-length subdir component in '%ls'", startp);
            startp++;
            continue;
        }
        if (endp != NULL) {
            /* normalize directory component */
            NormalizeDirectoryName(startp, endp - startp,
                    fStoredFssep, &dstp, newBufLen);

            *dstp++ = fLocalFssep;

            startp = endp +1;
        } else {
            /* normalize filename component */
            NormalizeFileName(startp, wcslen(startp),
                    fStoredFssep, &dstp, newBufLen);
            *dstp++ = '\0';

            /* add/replace extension if necessary */
            WCHAR extBuf[kMaxPathGrowth +1] = L"";
            if (fPreservation) {
                AddPreservationString(pathBuf, extBuf);
            } else if (fThreadKind == GenericEntry::kRsrcThread) {
                /* add this in lieu of the preservation extension */
                wcscat(pathBuf, kResourceStr);
            }
            if (fAddExtension) {
                AddTypeExtension(pathBuf, extBuf);
            }
            ASSERT(wcslen(extBuf) <= kMaxPathGrowth);
            wcscat(pathBuf, extBuf);

            startp = NULL;   /* we're done, break out of loop */
        }
    }

    /* check for overflow */
    ASSERT(dstp - pathBuf <= newBufLen);

    /*
     * If "junk paths" is set, drop everything but the last component.
     */
    if (fJunkPaths) {
        WCHAR* lastFssep;
        lastFssep = wcsrchr(pathBuf, fLocalFssep);
        if (lastFssep != NULL) {
            ASSERT(*(lastFssep+1) != '\0'); /* should already have been caught*/
            memmove(pathBuf, lastFssep+1,
                (wcslen(lastFssep+1)+1) * sizeof(WCHAR));
        }
    }

    fLocalPathName.ReleaseBuffer();
}

#if defined(WINDOWS_LIKE)
/*
 * You can't create files or directories with these names on a FAT filesystem,
 * because they're MS-DOS "device special files".
 *
 * The list originally came from the Linux kernel's fs/msdos/namei.c; a
 * better reference is "Naming Files, Paths, and Namespaces":
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
 *
 * The trick is that the name can't start with any of these.  That could mean
 * that the name is just "aux", or it could be "aux.this.txt".
 */
static const WCHAR* gFatReservedNames3[] = {
    L"CON", L"PRN", L"NUL", L"AUX", NULL
};
static const WCHAR* gFatReservedNames4[] = {
    L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9",
    L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
    NULL
};

void PathProposal::Win32NormalizeFileName(const WCHAR* srcp, long srcLen,
    char fssep, WCHAR** pDstp, long dstLen)
{
    /*
     * TODO(Unicode): do proper conversion
     * TODO: don't allow the filename to end with a space or period (Windows
     *   requirement)
     */
    WCHAR* dstp = *pDstp;
    const WCHAR* startp = srcp;
    static const WCHAR* kInvalid = L"\\/:*?\"<>|";

    /* match on "aux" or "aux.blah" */
    if (srcLen >= 3) {
        const WCHAR** ppcch;

        for (ppcch = gFatReservedNames3; *ppcch != NULL; ppcch++) {
            if (wcsnicmp(srcp, *ppcch, 3) == 0 &&
                (srcp[3] == '.' || srcLen == 3))
            {
                LOGD("--- fixing '%ls'", *ppcch);
                if (fPreservation) {
                    *dstp++ = kForeignIndic;
                    *dstp++ = '0';
                    *dstp++ = '0';
                } else
                    *dstp++ = '_';
                break;
            }
        }
    }
    if (srcLen >= 4) {
        const WCHAR** ppcch;

        for (ppcch = gFatReservedNames4; *ppcch != NULL; ppcch++) {
            if (wcsnicmp(srcp, *ppcch, 4) == 0 &&
                (srcp[4] == '.' || srcLen == 4))
            {
                LOGD("--- fixing '%ls'", *ppcch);
                if (fPreservation) {
                    *dstp++ = kForeignIndic;
                    *dstp++ = '0';
                    *dstp++ = '0';
                } else
                    *dstp++ = '_';
                break;
            }
        }
    }


    while (srcLen--) {      /* don't go until null found! */
        ASSERT(*srcp != '\0');

        if (*srcp == kForeignIndic) {
            /* change '%' to "%%" */
            if (fPreservation)
                *dstp++ = *srcp;
            *dstp++ = *srcp++;
        } else if (wcschr(kInvalid, *srcp) != NULL ||
            *srcp < 0x20 || *srcp >= 0x7f)
        {
            /* change invalid char to "%2f" or '_' */
            if (fPreservation) {
                *dstp++ = kForeignIndic;
                *dstp++ = HexConv(*srcp >> 4 & 0x0f);
                *dstp++ = HexConv(*srcp & 0x0f);
            } else {
                *dstp++ = '_';
            }
            srcp++;
        } else {
            /* no need to fiddle with it */
            *dstp++ = *srcp++;
        }
    }

    *dstp = '\0';       /* end the string, but don't advance past the null */
    ASSERT(*pDstp - dstp <= dstLen);    /* make sure we didn't overflow */
    *pDstp = dstp;
}
#endif

void PathProposal::NormalizeFileName(const WCHAR* srcp, long srcLen,
    char fssep, WCHAR** pDstp, long dstLen)
{
    ASSERT(srcp != NULL);
    ASSERT(srcLen > 0);
    ASSERT(dstLen > srcLen);
    ASSERT(pDstp != NULL);
    ASSERT(*pDstp != NULL);

#if defined(UNIX_LIKE)
    UNIXNormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
#elif defined(WINDOWS_LIKE)
    Win32NormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
#else
    #error "port this"
#endif
}

void PathProposal::NormalizeDirectoryName(const WCHAR* srcp, long srcLen,
    char fssep, WCHAR** pDstp, long dstLen)
{
    /* in general, directories and filenames are the same */
    ASSERT(fssep > ' ' && fssep < 0x7f);
    NormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
}

void PathProposal::AddPreservationString(const WCHAR* pathBuf, WCHAR* extBuf)
{
    WCHAR* cp;

    ASSERT(pathBuf != NULL);
    ASSERT(extBuf != NULL);
    ASSERT(fPreservation);

    cp = extBuf + wcslen(extBuf);

    /*
     * Cons up a preservation string.  On some platforms "sprintf" doesn't
     * return the #of characters written, so we add it up manually.
     */
    if (fFileType < 0x100 && fAuxType < 0x10000) {
        wsprintf(cp, L"%c%02lx%04lx", kPreserveIndic, fFileType, fAuxType);
        cp += 7;
    } else {
        wsprintf(cp, L"%c%08lx%08lx", kPreserveIndic, fFileType, fAuxType);
        cp += 17;
    }

    if (fThreadKind == GenericEntry::kRsrcThread)
        *cp++ = kResourceFlag;
    else if (fThreadKind == GenericEntry::kDiskImageThread)
        *cp++ = kDiskImageFlag;


    /* make sure it's terminated */
    *cp = '\0';
}

void PathProposal::AddTypeExtension(const WCHAR* pathBuf, WCHAR* extBuf)
{
    const WCHAR* pPathExt = NULL;
    const WCHAR* pWantedExt = NULL;
    const WCHAR* pTypeExt = NULL;
    WCHAR* end;
    WCHAR* cp;

    cp = extBuf + wcslen(extBuf);

    /*
     * Find extension in the local filename that we've prepared so far.
     * Note FindExtension guarantees there's at least one char after '.'.
     */
    pPathExt = PathName::FindExtension(pathBuf, fLocalFssep);
    if (pPathExt == NULL) {
        /*
         * There's no extension on the filename.  Use the standard
         * ProDOS type, if one exists for this entry.  We don't use
         * the table if it's NON, "???", or a hex value.
         */
        if (fFileType) {
            pTypeExt = FileTypeString(fFileType);
            if (pTypeExt[0] == '?' || pTypeExt[0] == '$')
                pTypeExt = NULL;
        }
    } else {
        pPathExt++;     // skip leading '.'
    }

    /*
     * Figure out what extension we want this file to have.  Files of type
     * text are *always* ".TXT", and our extracted disk images are always
     * ".PO".  If it's not one of these two, we either retain the file's
     * original extension, or generate one for it from the ProDOS file type.
     */
    if (fFileType == 0x04)
        pWantedExt = L"TXT";
    else if (fThreadKind == GenericEntry::kDiskImageThread)
        pWantedExt = L"PO";
    else {
        /*
         * We want to use the extension currently on the file, if it has one.
         * If not, use the one from the file type.
         */
        if (pPathExt != NULL) {
            pWantedExt = pPathExt;
        } else {
            pWantedExt = pTypeExt;
        }
    }
    /* pWantedExt != NULL unless we failed to find a pTypeExt */


    /*
     * Now we know which one we want.  Figure out if we want to add it.
     */
    if (pWantedExt != NULL) {
        if (extBuf[0] == '\0' && pPathExt != NULL &&
            wcsicmp(pPathExt, pWantedExt) == 0)
        {
            /* don't add an extension that's already there */
            pWantedExt = NULL;
            goto know_ext;
        }

        if (wcslen(pWantedExt) >= kMaxExtLen) {
            /* too long, forget it */
            pWantedExt = NULL;
            goto know_ext;
        }

        /* if it's strictly decimal-numeric, don't use it (.1, .2, etc) */
        (void) wcstoul(pWantedExt, &end, 10);
        if (*end == '\0') {
            pWantedExt = NULL;
            goto know_ext;
        }

        /* if '#' appears in it, don't use it -- it'll confuse us */
        //LOGI("LOOKING FOR '%c' in '%ls'", kPreserveIndic, ccp);
        const WCHAR* ccp = pWantedExt;
        while (*ccp != '\0') {
            if (*ccp == kPreserveIndic) {
                pWantedExt = NULL;
                goto know_ext;
            }
            ccp++;
        }
    }
know_ext:

    /*
     * If pWantedExt is non-NULL, it points to a filename extension without
     * the leading '.'.
     */
    if (pWantedExt != NULL) {
        *cp++ = kFilenameExtDelim;
        wcscpy(cp, pWantedExt);
        //cp += strlen(pWantedExt);
    }
}


/*
 * ===========================================================================
 *      File type restoration
 * ===========================================================================
 */

typedef bool Boolean;

void PathProposal::LocalToArchive(const AddFilesDialog* pAddOpts)
{
    Boolean wasPreserved;
    Boolean doJunk = false;
    Boolean adjusted;
    WCHAR slashDotDotSlash[5] = L"_.._";

    fStoredPathName = fLocalPathName;
    WCHAR* livePathStr = fStoredPathName.GetBuffer(1);

    fStoredFssep = kDefaultStoredFssep;

    /* convert '/' to '\' */
    ReplaceFssep(livePathStr,
        kAltLocalFssep, //NState_GetAltSystemPathSeparator(pState),
        kLocalFssep,    //NState_GetSystemPathSeparator(pState),
        kLocalFssep);   //NState_GetSystemPathSeparator(pState));

    /*
     * Check for file type preservation info in the filename.  If present,
     * set the file type values and truncate the filename.
     */
    wasPreserved = false;
    if (pAddOpts->fTypePreservation == AddFilesDialog::kPreserveTypes ||
        pAddOpts->fTypePreservation == AddFilesDialog::kPreserveAndExtend)
    {
        wasPreserved = ExtractPreservationString(livePathStr);
    }

    /*
     * Do a "denormalization" pass, where we convert invalid chars (such
     * as '/') from percent-codes back to 8-bit characters.  The filename
     * will always be the same size or smaller, so we can do it in place.
     */
    if (wasPreserved)
        DenormalizePath(livePathStr);

    /*
     * If we're in "extended" mode, and the file wasn't preserved, take a
     * guess at what the file type should be based on the file extension.
     */
    if (!wasPreserved &&
        pAddOpts->fTypePreservation == AddFilesDialog::kPreserveAndExtend)
    {
        InterpretExtension(livePathStr);
    }

    if (fStripDiskImageSuffix)
        StripDiskImageSuffix(livePathStr);

    /*
     * Strip bad chars off the front of the pathname.  Every time we
     * remove one thing we potentially expose another, so we have to
     * loop until it's sanitized.
     *
     * The outer loop isn't really necessary under Win32, because you'd
     * need to do something like ".\\foo", which isn't allowed.  UNIX
     * silently allows ".//foo", so this is a problem there.  (We could
     * probably do away with the inner loops, but those were already
     * written when I saw the larger problem.)
     */
    do {
        adjusted = false;

        /*
         * Check for other unpleasantness, such as a leading fssep.
         */
        ASSERT(kLocalFssep != '\0');
        while (livePathStr[0] == kLocalFssep) {
            /* slide it down, len is (strlen +1), -1 (dropping first char)*/
            memmove(livePathStr, livePathStr+1,
                wcslen(livePathStr) * sizeof(WCHAR));
            adjusted = true;
        }

        /*
         * Remove leading "./".
         */
        while (livePathStr[0] == '.' && livePathStr[1] == kLocalFssep)
        {
            /* slide it down, len is (strlen +1) -2 (dropping two chars) */
            memmove(livePathStr, livePathStr+2,
                (wcslen(livePathStr)-1) * sizeof(WCHAR));
            adjusted = true;
        }
    } while (adjusted);

    /*
     * If there's a "/../" present anywhere in the name, junk everything
     * but the filename.
     *
     * This won't catch "foo/bar/..", but that should've been caught as
     * a directory anyway.
     */
    slashDotDotSlash[0] = kLocalFssep;
    slashDotDotSlash[3] = kLocalFssep;
    if ((livePathStr[0] == '.' && livePathStr[1] == '.') ||
        (wcsstr(livePathStr, slashDotDotSlash) != NULL))
    {
        LOGD("Found dot dot in '%ls', keeping only filename", livePathStr);
        doJunk = true;
    }

    /*
     * Scan for and remove "/./" and trailing "/.".  They're filesystem
     * no-ops that work just fine under Win32 and UNIX but could confuse
     * a IIgs.  (Of course, the user could just omit them from the pathname.)
     */
    /* TO DO 20030208 */

    /*
     * If "junk paths" is set, drop everything before the last fssep char.
     */
    if (pAddOpts->fStripFolderNames || doJunk) {
        WCHAR* lastFssep;
        lastFssep = wcsrchr(livePathStr, kLocalFssep);
        if (lastFssep != NULL) {
            ASSERT(*(lastFssep+1) != '\0'); /* should already have been caught*/
            memmove(livePathStr, lastFssep+1,
                (wcslen(lastFssep+1)+1) * sizeof(WCHAR));
        }
    }

    /*
     * Finally, substitute our generally-accepted path separator in place of
     * the local one, stomping on anything with a ':' in it as we do.  The
     * goal is to avoid having "subdir:foo/bar" turn into "subdir/foo/bar",
     * so we change it to "subdirXfoo:bar".  Were we a general-purpose
     * archiver, this might be a mistake, but we're not.  NuFX doesn't really
     * give us a choice.
     */
    ReplaceFssep(livePathStr, kLocalFssep,
        PathProposal::kDefaultStoredFssep, 'X');

    /* let the CString manage itself again */
    fStoredPathName.ReleaseBuffer();
}

void PathProposal::ReplaceFssep(WCHAR* str, char oldc, char newc, char newSubst)
{
    while (*str != '\0') {
        if (*str == oldc)
            *str = newc;
        else if (*str == newc)
            *str = newSubst;
        str++;
    }
}

void PathProposal::LookupExtension(const WCHAR* ext)
{
    WCHAR uext3[4];
    int i, extLen;

    extLen = wcslen(ext);
    ASSERT(extLen > 0);

    /*
     * First step is to try to find it in the recognized types list.
     */
    for (i = 0; i < NELEM(gRecognizedExtensions); i++) {
        if (wcsicmp(ext, gRecognizedExtensions[i].label) == 0) {
            fFileType = gRecognizedExtensions[i].fileType;
            fAuxType = gRecognizedExtensions[i].auxType;
            goto bail;
        }
    }

    /*
     * Second step is to try to find it in the ProDOS types list.
     *
     * The extension is converted to upper case and padded with spaces.
     *
     * [do we want to obstruct matching on things like '$f7' here?]
     */
    if (extLen <= 3) {
        for (i = 2; i >= extLen; i--)
            uext3[i] = ' ';
        for ( ; i >= 0; i--)
            uext3[i] = toupper(ext[i]);
        uext3[3] = '\0';

        for (i = 0; i < NELEM(gFileTypeNames); i++) {
            if (wcscmp(uext3, gFileTypeNames[i]) == 0) {
                fFileType = i;
                goto bail;
            }
        }
    }

bail:
    return;
}

void PathProposal::InterpretExtension(const WCHAR* pathName)
{
    const WCHAR* pExt;

    ASSERT(pathName != NULL);

    pExt = PathName::FindExtension(pathName, fLocalFssep);
    if (pExt != NULL)
        LookupExtension(pExt+1);
}

Boolean PathProposal::ExtractPreservationString(WCHAR* pathname)
{
    /*
     * We have to be careful not to trip on false-positive occurrences of '#'
     * in the filename.
     */
    WCHAR numBuf[9];
    unsigned long fileType, auxType;
    int threadMask;
    WCHAR* pPreserve;
    WCHAR* cp;
    int digitCount;

    ASSERT(pathname != NULL);

    pPreserve = wcsrchr(pathname, kPreserveIndic);
    if (pPreserve == NULL)
        return false;

    /* count up the #of hex digits */
    digitCount = 0;
    for (cp = pPreserve+1; *cp != '\0' && isxdigit((int)*cp); cp++)
        digitCount++;

    /* extract the file and aux type */
    switch (digitCount) {
    case 6:
        /* ProDOS 1-byte type and 2-byte aux */
        memcpy(numBuf, pPreserve+1, 2 * sizeof(WCHAR));
        numBuf[2] = 0;
        fileType = wcstoul(numBuf, &cp, 16);
        ASSERT(cp == numBuf + 2);

        auxType = wcstoul(pPreserve+3, &cp, 16);
        ASSERT(cp == pPreserve + 7);
        break;
    case 16:
        /* HFS 4-byte type and 4-byte creator */
        memcpy(numBuf, pPreserve+1, 8 * sizeof(WCHAR));
        numBuf[8] = 0;
        fileType = wcstoul(numBuf, &cp, 16);
        ASSERT(cp == numBuf + 8);

        auxType = wcstoul(pPreserve+9, &cp, 16);
        ASSERT(cp == pPreserve + 17);
        break;
    default:
        /* not valid */
        return false;
    }

    /* check for a threadID specifier */
    //threadID = kNuThreadIDDataFork;
    threadMask = GenericEntry::kDataThread;
    switch (*cp) {
    case kResourceFlag:
        //threadID = kNuThreadIDRsrcFork;
        threadMask = GenericEntry::kRsrcThread;
        cp++;
        break;
    case kDiskImageFlag:
        //threadID = kNuThreadIDDiskImage;
        threadMask = GenericEntry::kDiskImageThread;
        cp++;
        break;
    default:
        /* do nothing... yet */
        break;
    }

    /* make sure we were the very last component */
    switch (*cp) {
    case kFilenameExtDelim:     /* redundant "-ee" extension */
    case '\0':                  /* end of string! */
        break;
    default:
        return false;
    }

    /* truncate the original string, and return what we got */
    *pPreserve = '\0';
    fFileType = fileType;
    fAuxType = auxType;
    fThreadKind = threadMask;
    //*pThreadID = threadID;

    return true;
}

void PathProposal::DenormalizePath(WCHAR* pathBuf)
{
    const WCHAR* srcp;
    WCHAR* dstp;
    WCHAR ch;

    srcp = pathBuf;
    dstp = pathBuf;

    while (*srcp != '\0') {
        if (*srcp == kForeignIndic) {
            srcp++;
            if (*srcp == kForeignIndic) {
                *dstp++ = kForeignIndic;
                srcp++;
            } else if (isxdigit((int)*srcp)) {
                ch = HexDigit(*srcp) << 4;
                srcp++;
                if (isxdigit((int)*srcp)) {
                    /* valid, output char (unless it's a %00 place-holder) */
                    ch += HexDigit(*srcp);
                    if (ch != '\0') {
                        *dstp++ = ch;
                    }
                    srcp++;
                } else {
                    /* bogus '%' with trailing hex digit found! */
                    *dstp++ = kForeignIndic;
                    *dstp++ = *(srcp-1);
                }
            } else {
                /* bogus lone '%s' found! */
                *dstp++ = kForeignIndic;
            }

        } else {
            *dstp++ = *srcp++;
        }
    }

    *dstp = '\0';
    ASSERT(dstp <= srcp);
}

void PathProposal::StripDiskImageSuffix(WCHAR* pathName)
{
    static const WCHAR diskExt[][4] = {
        L"SHK", L"SDK", L"IMG", L"PO", L"DO", L"2MG", L"DSK"
    };
    const WCHAR* pExt;
    int i;

    pExt = PathName::FindExtension(pathName, fLocalFssep);
    if (pExt == NULL || pExt == pathName)
        return;

    for (i = 0; i < NELEM(diskExt); i++) {
        if (wcsicmp(pExt+1, diskExt[i]) == 0) {
            LOGI("Dropping '%ls' from '%ls'", pExt, pathName);
            *const_cast<WCHAR*>(pExt) = '\0';
            return;
        }
    }
}

/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Filename manipulation, including file type preservation.  This is
 * substantially ripped from NuLib2, which would be a GPL violation if
 * it weren't my code to begin with.
 */
#include "stdafx.h"
#include "FileNameConv.h"
#include "GenericArchive.h"
#include "AddFilesDialog.h"
#include <ctype.h>


#define WINDOWS_LIKE
/* replace unsupported chars with '%xx' */
#define kForeignIndic	'%'

/* convert single hex digit char to number */
#define HexDigit(x) ( !isxdigit((int)(x)) ? -1 : \
            (x) <= '9' ? (x) - '0' : toupper(x) +10 - 'A' )

/* convert number from 0-15 to hex digit */
#define HexConv(x)  ( ((unsigned int)(x)) <= 15 ? \
            ( (x) <= 9 ? (x) + '0' : (x) -10 + 'A') : -1 )


/*
 * ===========================================================================
 *		Common definitions
 * ===========================================================================
 */

#define kPreserveIndic	'#' 	/* use # rather than $ for hex indication */
#define kFilenameExtDelim '.'	/* separates extension from filename */
#define kResourceFlag	'r'
#define kDiskImageFlag	'i'
#define kMaxExtLen		5		/* ".1234" */
#define kResourceStr	_T("_rsrc_")

/* must be longer then strlen(kResourceStr)... no problem there */
#define kMaxPathGrowth	(sizeof("#XXXXXXXXYYYYYYYYZ")-1 + kMaxExtLen+1)


/* ProDOS file type names; must be entirely in upper case */
static const char gFileTypeNames[256][4] = {
	"NON", "BAD", "PCD", "PTX", "TXT", "PDA", "BIN", "FNT",
	"FOT", "BA3", "DA3", "WPF", "SOS", "$0D", "$0E", "DIR",
	"RPD", "RPI", "AFD", "AFM", "AFR", "SCL", "PFS", "$17",
	"$18", "ADB", "AWP", "ASP", "$1C", "$1D", "$1E", "$1F",
	"TDM", "$21", "$22", "$23", "$24", "$25", "$26", "$27",
	"$28", "$29", "8SC", "8OB", "8IC", "8LD", "P8C", "$2F",
	"$30", "$31", "$32", "$33", "$34", "$35", "$36", "$37",
	"$38", "$39", "$3A", "$3B", "$3C", "$3D", "$3E", "$3F",
	"DIC", "OCR", "FTD", "$43", "$44", "$45", "$46", "$47",
	"$48", "$49", "$4A", "$4B", "$4C", "$4D", "$4E", "$4F",
	"GWP", "GSS", "GDB", "DRW", "GDP", "HMD", "EDU", "STN",
	"HLP", "COM", "CFG", "ANM", "MUM", "ENT", "DVU", "FIN",
	"$60", "$61", "$62", "$63", "$64", "$65", "$66", "$67",
	"$68", "$69", "$6A", "BIO", "$6C", "TDR", "PRE", "HDV",
	"$70", "$71", "$72", "$73", "$74", "$75", "$76", "$77",
	"$78", "$79", "$7A", "$7B", "$7C", "$7D", "$7E", "$7F",
	"$80", "$81", "$82", "$83", "$84", "$85", "$86", "$87",
	"$88", "$89", "$8A", "$8B", "$8C", "$8D", "$8E", "$8F",
	"$90", "$91", "$92", "$93", "$94", "$95", "$96", "$97",
	"$98", "$99", "$9A", "$9B", "$9C", "$9D", "$9E", "$9F",
	"WP ", "$A1", "$A2", "$A3", "$A4", "$A5", "$A6", "$A7",
	"$A8", "$A9", "$AA", "GSB", "TDF", "BDF", "$AE", "$AF",
	"SRC", "OBJ", "LIB", "S16", "RTL", "EXE", "PIF", "TIF",
	"NDA", "CDA", "TOL", "DVR", "LDF", "FST", "$BE", "DOC",
	"PNT", "PIC", "ANI", "PAL", "$C4", "OOG", "SCR", "CDV",
	"FON", "FND", "ICN", "$CB", "$CC", "$CD", "$CE", "$CF",
	"$D0", "$D1", "$D2", "$D3", "$D4", "MUS", "INS", "MDI",
	"SND", "$D9", "$DA", "DBM", "$DC", "DDD", "$DE", "$DF",
	"LBR", "$E1", "ATK", "$E3", "$E4", "$E5", "$E6", "$E7",
	"$E8", "$E9", "$EA", "$EB", "$EC", "$ED", "R16", "PAS",
	"CMD", "$F1", "$F2", "$F3", "$F4", "$F5", "$F6", "$F7",
	"$F8", "OS ", "INT", "IVR", "BAS", "VAR", "REL", "SYS"
};

/*
 * Some file extensions we recognize.  When adding files with "extended"
 * preservation mode, we try to assign types to files that weren't
 * explicitly preserved, but nevertheless have a recognizeable type.
 *
 * geoff at gwlink.net pointed out that this really ought to be in an external
 * file rather than a hard-coded table.  Ought to fix that someday.
 */
static const struct {
	const char* 		label;
	unsigned short		fileType;
	unsigned long		auxType;
	unsigned char		flags;
} gRecognizedExtensions[] = {
	{ "ASM",  0xb0, 0x0003, 0 },		/* APW assembly source */
	{ "C",	  0xb0, 0x000a, 0 },		/* APW C source */
	{ "H",	  0xb0, 0x000a, 0 },		/* APW C header */
	{ "CPP",  0xb0, 0x0000, 0 },		/* generic source file */
	{ "BNY",  0xe0, 0x8000, 0 },		/* Binary II lib */
	{ "BQY",  0xe0, 0x8000, 0 },		/* Binary II lib, w/ compress */
	{ "BXY",  0xe0, 0x8000, 0 },		/* Binary II wrap around SHK */
	{ "BSE",  0xe0, 0x8000, 0 },		/* Binary II wrap around SEA */
	{ "SEA",  0xb3, 0xdb07, 0 },		/* GSHK SEA */
	{ "TEXT", 0x04, 0x0000, 0 },		/* ASCII Text */
	{ "GIF",  0xc0, 0x8006, 0 },		/* GIF image */
	{ "JPG",  0x06, 0x0000, 0 },		/* JPEG (nicer than 'NON') */
	{ "JPEG", 0x06, 0x0000, 0 },		/* JPEG (nicer than 'NON') */
	//{ "ACU",  0xe0, 0x8001, 0 },		/* ACU archive */
	{ "SHK",  0xe0, 0x8002, 0 },		/* ShrinkIt archive */
};


/*
 * Return a pointer to the three-letter representation of the file type name.
 *
 * Note to self: code down below tests first char for '?'.
 */
/*static*/ const char*
PathProposal::FileTypeString(unsigned long fileType)
{
	if (fileType < NELEM(gFileTypeNames))
		return gFileTypeNames[fileType];
	else
		return kUnknownTypeStr;
}

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
 */
static const struct {
	unsigned short	fileType;
	unsigned short	minAuxType;		// start of range for which this applies
	unsigned short	maxAuxType;		// end of range
	const char*		descr;
} gTypeDescriptions[] = {
	/*NON*/ { 0x00, 0x0000, 0xffff, "Untyped file" },
	/*BAD*/ { 0x01, 0x0000, 0xffff, "Bad blocks" },
	/*PCD*/ { 0x02, 0x0000, 0xffff, "Pascal code" },
	/*PTX*/ { 0x03, 0x0000, 0xffff, "Pascal text" },
	/*TXT*/ { 0x04, 0x0000, 0xffff, "ASCII text" },
	/*PDA*/ { 0x05, 0x0000, 0xffff, "Pascal data" },
	/*BIN*/ { 0x06, 0x0000, 0xffff, "Binary" },
	/*FNT*/ { 0x07, 0x0000, 0xffff, "Apple /// font" },
	/*FOT*/ { 0x08, 0x0000, 0xffff, "Apple II or /// graphics" },
	/*   */ { 0x08, 0x0000, 0x3fff, "Apple II graphics" },
	/*   */ { 0x08, 0x4000, 0x4000, "Packed hi-res image" },
	/*   */ { 0x08, 0x4001, 0x4001, "Packed double hi-res image" },
	/*   */ { 0x08, 0x8001, 0x8001, "Printographer packed HGR file" },
	/*   */ { 0x08, 0x8002, 0x8002, "Printographer packed DHGR file" },
	/*   */ { 0x08, 0x8003, 0x8003, "Softdisk hi-res image" },
	/*   */ { 0x08, 0x8004, 0x8004, "Softdisk double hi-res image" },
	/*BA3*/ { 0x09, 0x0000, 0xffff, "Apple /// BASIC program" },
	/*DA3*/ { 0x0a, 0x0000, 0xffff, "Apple /// BASIC data" },
	/*WPF*/ { 0x0b, 0x0000, 0xffff, "Apple II or /// word processor" },
	/*   */ { 0x0b, 0x8001, 0x8001, "Write This Way document" },
	/*   */ { 0x0b, 0x8002, 0x8002, "Writing & Publishing document" },
	/*SOS*/ { 0x0c, 0x0000, 0xffff, "Apple /// SOS system" },
	/*DIR*/ { 0x0f, 0x0000, 0xffff, "Folder" },
	/*RPD*/ { 0x10, 0x0000, 0xffff, "Apple /// RPS data" },
	/*RPI*/ { 0x11, 0x0000, 0xffff, "Apple /// RPS index" },
	/*AFD*/ { 0x12, 0x0000, 0xffff, "Apple /// AppleFile discard" },
	/*AFM*/ { 0x13, 0x0000, 0xffff, "Apple /// AppleFile model" },
	/*AFR*/ { 0x14, 0x0000, 0xffff, "Apple /// AppleFile report format" },
	/*SCL*/ { 0x15, 0x0000, 0xffff, "Apple /// screen library" },
	/*PFS*/ { 0x16, 0x0000, 0xffff, "PFS document" },
	/*   */ { 0x16, 0x0001, 0x0001, "PFS:File document" },
	/*   */ { 0x16, 0x0002, 0x0002, "PFS:Write document" },
	/*   */ { 0x16, 0x0003, 0x0003, "PFS:Graph document" },
	/*   */ { 0x16, 0x0004, 0x0004, "PFS:Plan document" },
	/*   */ { 0x16, 0x0016, 0x0016, "PFS internal data" },
	/*ADB*/ { 0x19, 0x0000, 0xffff, "AppleWorks data base" },
	/*AWP*/ { 0x1a, 0x0000, 0xffff, "AppleWorks word processor" },
	/*ASP*/ { 0x1b, 0x0000, 0xffff, "AppleWorks spreadsheet" },
	/*TDM*/ { 0x20, 0x0000, 0xffff, "Desktop Manager document" },
	/*???*/ { 0x21, 0x0000, 0xffff, "Instant Pascal source" },
	/*???*/ { 0x22, 0x0000, 0xffff, "UCSD Pascal volume" },
	/*???*/ { 0x29, 0x0000, 0xffff, "Apple /// SOS dictionary" },
	/*8SC*/ { 0x2a, 0x0000, 0xffff, "Apple II source code" },
	/*   */ { 0x2a, 0x8001, 0x8001, "EBBS command script" },
	/*8OB*/ { 0x2b, 0x0000, 0xffff, "Apple II object code" },
	/*   */ { 0x2b, 0x8001, 0x8001, "GBBS Pro object Code" },
	/*8IC*/ { 0x2c, 0x0000, 0xffff, "Apple II interpreted code" },
	/*   */ { 0x2c, 0x8003, 0x8003, "APEX Program File" },
	/*   */ { 0x2c, 0x8005, 0x8005, "EBBS tokenized command script" },
	/*8LD*/ { 0x2d, 0x0000, 0xffff, "Apple II language data" },
	/*   */ { 0x2d, 0x8006, 0x8005, "EBBS message bundle" },
	/*   */ { 0x2d, 0x8007, 0x8007, "EBBS compressed message bundle" },
	/*P8C*/ { 0x2e, 0x0000, 0xffff, "ProDOS 8 code module" },
	/*   */ { 0x2e, 0x8001, 0x8001, "Davex 8 Command" },
	/*PTP*/ { 0x2e, 0x8002, 0x8002, "Point-to-Point drivers" },
	/*PTP*/ { 0x2e, 0x8003, 0x8003, "Point-to-Point code" },
	/*   */ { 0x2e, 0x8004, 0x8004, "Softdisk printer driver" },
	/*DIC*/ { 0x40, 0x0000, 0xffff, "Dictionary file" },
	/*???*/ { 0x41, 0x0000, 0xffff, "OCR data" },
	/*   */ { 0x41, 0x8001, 0x8001, "InWords OCR font table" },
	/*FTD*/ { 0x42, 0x0000, 0xffff, "File type names" },
	/*???*/ { 0x43, 0x0000, 0xffff, "Peripheral data" },
	/*   */ { 0x43, 0x8001, 0x8001, "Express document" },
	/*???*/ { 0x44, 0x0000, 0xffff, "Personal information" },
	/*   */ { 0x44, 0x8001, 0x8001, "ResuMaker personal information" },
	/*   */ { 0x44, 0x8002, 0x8002, "ResuMaker resume" },
	/*   */ { 0x44, 0x8003, 0x8003, "II Notes document" },
	/*   */ { 0x44, 0x8004, 0x8004, "Softdisk scrapbook document" },
	/*   */ { 0x44, 0x8005, 0x8005, "Don't Forget document" },
	/*   */ { 0x44, 0x80ff, 0x80ff, "What To Do data" },
	/*   */ { 0x44, 0xbeef, 0xbeef, "Table Scraps scrapbook" },
	/*???*/ { 0x45, 0x0000, 0xffff, "Mathematical document" },
	/*   */ { 0x45, 0x8001, 0x8001, "GSymbolix 3D graph document" },
	/*   */ { 0x45, 0x8002, 0x8002, "GSymbolix formula document" },
	/*???*/ { 0x46, 0x0000, 0xffff, "AutoSave profiles" },
	/*   */ { 0x46, 0x8001, 0x8001, "AutoSave profiles" },
	/*GWP*/ { 0x50, 0x0000, 0xffff, "Apple IIgs Word Processor" },
	/*   */ { 0x50, 0x8001, 0x8001, "DeluxeWrite document" },
	/*   */ { 0x50, 0x8003, 0x8003, "Personal Journal document" },
	/*   */ { 0x50, 0x8010, 0x8010, "AppleWorks GS word processor" },
	/*   */ { 0x50, 0x8011, 0x8011, "Softdisk issue text" },
	/*   */ { 0x50, 0x5445, 0x5445, "Teach document" },
	/*GSS*/ { 0x51, 0x0000, 0xffff, "Apple IIgs spreadsheet" },
	/*   */ { 0x51, 0x8010, 0x8010, "AppleWorks GS spreadsheet" },
	/*   */ { 0x51, 0x2358, 0x2358, "QC Calc spreadsheet " },
	/*GDB*/ { 0x52, 0x0000, 0xffff, "Apple IIgs data base" },
	/*   */ { 0x52, 0x8001, 0x8001, "GTv database" },
	/*   */ { 0x52, 0x8010, 0x8010, "AppleWorks GS data base" },
	/*   */ { 0x52, 0x8011, 0x8011, "AppleWorks GS DB template" },
	/*   */ { 0x52, 0x8013, 0x8013, "GSAS database" },
	/*   */ { 0x52, 0x8014, 0x8014, "GSAS accounting journals" },
	/*   */ { 0x52, 0x8015, 0x8015, "Address Manager document" },
	/*   */ { 0x52, 0x8016, 0x8016, "Address Manager defaults" },
	/*   */ { 0x52, 0x8017, 0x8017, "Address Manager index" },
	/*DRW*/ { 0x53, 0x0000, 0xffff, "Drawing" },
	/*   */ { 0x53, 0x8002, 0x8002, "Graphic Disk Labeler document" },
	/*   */ { 0x53, 0x8010, 0x8010, "AppleWorks GS graphics" },
	/*GDP*/ { 0x54, 0x0000, 0xffff, "Desktop publishing" },
	/*   */ { 0x54, 0x8002, 0x8002, "GraphicWriter document" },
	/*   */ { 0x54, 0x8003, 0x8003, "Label It document" },
	/*   */ { 0x54, 0x8010, 0x8010, "AppleWorks GS Page Layout" },
	/*   */ { 0x54, 0xdd3e, 0xdd3e, "Medley document" },
	/*HMD*/ { 0x55, 0x0000, 0xffff, "Hypermedia" },
	/*   */ { 0x55, 0x0001, 0x0001, "HyperCard IIgs stack" },
	/*   */ { 0x55, 0x8001, 0x8001, "Tutor-Tech document" },
	/*   */ { 0x55, 0x8002, 0x8002, "HyperStudio document" },
	/*   */ { 0x55, 0x8003, 0x8003, "Nexus document" },
	/*   */ { 0x55, 0x8004, 0x8004, "HyperSoft stack" },
	/*   */ { 0x55, 0x8005, 0x8005, "HyperSoft card" },
	/*   */ { 0x55, 0x8006, 0x8006, "HyperSoft external command" },
	/*EDU*/ { 0x56, 0x0000, 0xffff, "Educational Data" },
	/*   */ { 0x56, 0x8001, 0x8001, "Tutor-Tech scores" },
	/*   */ { 0x56, 0x8007, 0x8007, "GradeBook data" },
	/*STN*/ { 0x57, 0x0000, 0xffff, "Stationery" },
	/*   */ { 0x57, 0x8003, 0x8003, "Music Writer format" },
	/*HLP*/ { 0x58, 0x0000, 0xffff, "Help file" },
	/*   */ { 0x58, 0x8002, 0x8002, "Davex 8 help file" },
	/*   */ { 0x58, 0x8005, 0x8005, "Micol Advanced Basic help file" },
	/*   */ { 0x58, 0x8006, 0x8006, "Locator help document" },
	/*   */ { 0x58, 0x8007, 0x8007, "Personal Journal help" },
	/*   */ { 0x58, 0x8008, 0x8008, "Home Refinancer help" },
	/*   */ { 0x58, 0x8009, 0x8009, "The Optimizer help" },
	/*   */ { 0x58, 0x800a, 0x800a, "Text Wizard help" },
	/*   */ { 0x58, 0x800b, 0x800b, "WordWorks Pro help system" },
	/*   */ { 0x58, 0x800c, 0x800c, "Sound Wizard help" },
	/*   */ { 0x58, 0x800d, 0x800d, "SeeHear help system" },
	/*   */ { 0x58, 0x800e, 0x800e, "QuickForms help system" },
	/*   */ { 0x58, 0x800f, 0x800f, "Don't Forget help system" },
	/*COM*/ { 0x59, 0x0000, 0xffff, "Communications file" },
	/*   */ { 0x59, 0x8002, 0x8002, "AppleWorks GS communications" },
	/*CFG*/ { 0x5a, 0x0000, 0xffff, "Configuration file" },
	/*   */ { 0x5a, 0x0000, 0x0000, "Sound settings files" },
	/*   */ { 0x5a, 0x0002, 0x0002, "Battery RAM configuration" },
	/*   */ { 0x5a, 0x0003, 0x0003, "AutoLaunch preferences" },
	/*   */ { 0x5a, 0x0004, 0x0004, "SetStart preferences" },
	/*   */ { 0x5a, 0x0005, 0x0005, "GSBug configuration" },
	/*   */ { 0x5a, 0x0006, 0x0006, "Archiver preferences" },
	/*   */ { 0x5a, 0x0007, 0x0007, "Archiver table of contents" },
	/*   */ { 0x5a, 0x0008, 0x0008, "Font Manager data" },
	/*   */ { 0x5a, 0x0009, 0x0009, "Print Manager data" },
	/*   */ { 0x5a, 0x000a, 0x000a, "IR preferences" },
	/*   */ { 0x5a, 0x8001, 0x8001, "Master Tracks Jr. preferences" },
	/*   */ { 0x5a, 0x8002, 0x8002, "GraphicWriter preferences" },
	/*   */ { 0x5a, 0x8003, 0x8003, "Z-Link configuration" },
	/*   */ { 0x5a, 0x8004, 0x8004, "JumpStart configuration" },
	/*   */ { 0x5a, 0x8005, 0x8005, "Davex 8 configuration" },
	/*   */ { 0x5a, 0x8006, 0x8006, "Nifty List configuration" },
	/*   */ { 0x5a, 0x8007, 0x8007, "GTv videodisc configuration" },
	/*   */ { 0x5a, 0x8008, 0x8008, "GTv Workshop configuration" },
	/*PTP*/ { 0x5a, 0x8009, 0x8009, "Point-to-Point preferences" },
	/*   */ { 0x5a, 0x800a, 0x800a, "ORCA/Disassembler preferences" },
	/*   */ { 0x5a, 0x800b, 0x800b, "SnowTerm preferences" },
	/*   */ { 0x5a, 0x800c, 0x800c, "My Word! preferences" },
	/*   */ { 0x5a, 0x800d, 0x800d, "Chipmunk configuration" },
	/*   */ { 0x5a, 0x8010, 0x8010, "AppleWorks GS configuration" },
	/*   */ { 0x5a, 0x8011, 0x8011, "SDE Shell preferences" },
	/*   */ { 0x5a, 0x8012, 0x8012, "SDE Editor preferences" },
	/*   */ { 0x5a, 0x8013, 0x8013, "SDE system tab ruler" },
	/*   */ { 0x5a, 0x8014, 0x8014, "Nexus configuration" },
	/*   */ { 0x5a, 0x8015, 0x8015, "DesignMaster preferences" },
	/*   */ { 0x5a, 0x801a, 0x801a, "MAX/Edit keyboard template" },
	/*   */ { 0x5a, 0x801b, 0x801b, "MAX/Edit tab ruler set" },
	/*   */ { 0x5a, 0x801c, 0x801c, "Platinum Paint preferences" },
	/*   */ { 0x5a, 0x801d, 0x801d, "Sea Scan 1000" },
	/*   */ { 0x5a, 0x801e, 0x801e, "Allison preferences" },
	/*   */ { 0x5a, 0x801f, 0x801f, "Gold of the Americas options" },
	/*   */ { 0x5a, 0x8021, 0x8021, "GSAS accounting setup" },
	/*   */ { 0x5a, 0x8022, 0x8022, "GSAS accounting document" },
	/*   */ { 0x5a, 0x8023, 0x8023, "UtilityLaunch preferences" },
	/*   */ { 0x5a, 0x8024, 0x8024, "Softdisk configuration" },
	/*   */ { 0x5a, 0x8025, 0x8025, "Quit-To configuration" },
	/*   */ { 0x5a, 0x8026, 0x8026, "Big Edit Thing" },
	/*   */ { 0x5a, 0x8027, 0x8027, "ZMaker preferences" },
	/*   */ { 0x5a, 0x8028, 0x8028, "Minstrel configuration" },
	/*   */ { 0x5a, 0x8029, 0x8029, "WordWorks Pro preferences" },
	/*   */ { 0x5a, 0x802b, 0x802b, "Pointless preferences" },
	/*   */ { 0x5a, 0x802c, 0x802c, "Micol Advanced Basic config" },
	/*   */ { 0x5a, 0x802e, 0x802e, "Label It configuration" },
	/*   */ { 0x5a, 0x802f, 0x802f, "Cool Cursor document" },
	/*   */ { 0x5a, 0x8030, 0x8030, "Locator preferences" },
	/*   */ { 0x5a, 0x8031, 0x8031, "Replicator preferences" },
	/*   */ { 0x5a, 0x8032, 0x8032, "Kangaroo configuration" },
	/*   */ { 0x5a, 0x8033, 0x8033, "Kangaroo data" },
	/*   */ { 0x5a, 0x8034, 0x8034, "TransProg III configuration" },
	/*   */ { 0x5a, 0x8035, 0x8035, "Home Refinancer preferences" },
	/*   */ { 0x5a, 0x8036, 0x8036, "Easy Eyes settings" },
	/*   */ { 0x5a, 0x8037, 0x8037, "The Optimizer settings" },
	/*   */ { 0x5a, 0x8038, 0x8038, "Text Wizard settings" },
	/*   */ { 0x5a, 0x803b, 0x803b, "Disk Access II preferences" },
	/*   */ { 0x5a, 0x803d, 0x803d, "Quick DA configuration" },
	/*   */ { 0x5a, 0x803e, 0x803e, "Crazy 8s preferences" },
	/*   */ { 0x5a, 0x803f, 0x803f, "Sound Wizard settings" },
	/*   */ { 0x5a, 0x8041, 0x8041, "Quick Window configuration" },
	/*   */ { 0x5a, 0x8044, 0x8044, "Universe Master disk map" },
	/*   */ { 0x5a, 0x8046, 0x8046, "Autopilot configuration" },
	/*   */ { 0x5a, 0x8047, 0x8047, "EGOed preferences" },
	/*   */ { 0x5a, 0x8049, 0x8049, "Quick DA preferences" },
	/*   */ { 0x5a, 0x804b, 0x804b, "HardPressed volume preferences" },
	/*   */ { 0x5a, 0x804c, 0x804c, "HardPressed global preferences" },
	/*   */ { 0x5a, 0x804d, 0x804d, "HardPressed profile" },
	/*   */ { 0x5a, 0x8050, 0x8050, "Don't Forget settings" },
	/*   */ { 0x5a, 0x8052, 0x8052, "ProBOOT preferences" },
	/*   */ { 0x5a, 0x8054, 0x8054, "Battery Brain preferences" },
	/*   */ { 0x5a, 0x8055, 0x8055, "Rainbow configuration" },
	/*   */ { 0x5a, 0x8061, 0x8061, "TypeSet preferences" },
	/*   */ { 0x5a, 0x8063, 0x8063, "Cool Cursor preferences" },
	/*   */ { 0x5a, 0x806e, 0x806e, "Balloon preferences" },
	/*   */ { 0x5a, 0x80fe, 0x80fe, "Special Edition configuration" },
	/*   */ { 0x5a, 0x80ff, 0x80ff, "Sun Dial preferences" },
	/*ANM*/ { 0x5b, 0x0000, 0xffff, "Animation file" },
	/*   */ { 0x5b, 0x8001, 0x8001, "Cartooners movie" },
	/*   */ { 0x5b, 0x8002, 0x8002, "Cartooners actors" },
	/*   */ { 0x5b, 0x8005, 0x8005, "Arcade King Super document" },
	/*   */ { 0x5b, 0x8006, 0x8006, "Arcade King DHRG document" },
	/*   */ { 0x5b, 0x8007, 0x8007, "DreamVision movie" },
	/*MUM*/ { 0x5c, 0x0000, 0xffff, "Multimedia document" },
	/*   */ { 0x5c, 0x8001, 0x8001, "GTv multimedia playlist" },
	/*ENT*/ { 0x5d, 0x0000, 0xffff, "Game/Entertainment document" },
	/*   */ { 0x5d, 0x8001, 0x8001, "Solitaire Royale document" },
	/*   */ { 0x5d, 0x8002, 0x8002, "BattleFront scenario" },
	/*   */ { 0x5d, 0x8003, 0x8003, "BattleFront saved game" },
	/*   */ { 0x5d, 0x8004, 0x8004, "Gold of the Americas game" },
	/*   */ { 0x5d, 0x8006, 0x8006, "Blackjack Tutor document" },
	/*   */ { 0x5d, 0x8008, 0x8008, "Canasta document" },
	/*   */ { 0x5d, 0x800b, 0x800b, "Word Search document" },
	/*   */ { 0x5d, 0x800c, 0x800c, "Tarot deal" },
	/*   */ { 0x5d, 0x800d, 0x800d, "Tarot tournament" },
	/*   */ { 0x5d, 0x800e, 0x800e, "Full Metal Planet game" },
	/*   */ { 0x5d, 0x800f, 0x800f, "Full Metal Planet player" },
	/*   */ { 0x5d, 0x8010, 0x8010, "Quizzical high scores" },
	/*   */ { 0x5d, 0x8011, 0x8011, "Meltdown high scores" },
	/*   */ { 0x5d, 0x8012, 0x8012, "BlockWords high scores" },
	/*   */ { 0x5d, 0x8013, 0x8013, "Lift-A-Gon scores" },
	/*   */ { 0x5d, 0x8014, 0x8014, "Softdisk Adventure" },
	/*   */ { 0x5d, 0x8015, 0x8015, "Blankety Blank document" },
	/*   */ { 0x5d, 0x8016, 0x8016, "Son of Star Axe champion" },
	/*   */ { 0x5d, 0x8017, 0x8017, "Digit Fidget high scores" },
	/*   */ { 0x5d, 0x8018, 0x8018, "Eddie map" },
	/*   */ { 0x5d, 0x8019, 0x8019, "Eddie tile set" },
	/*   */ { 0x5d, 0x8122, 0x8122, "Wolfenstein 3D scenario" },
	/*   */ { 0x5d, 0x8123, 0x8123, "Wolfenstein 3D saved game" },
	/*DVU*/ { 0x5e, 0x0000, 0xffff, "Development utility document" },
	/*   */ { 0x5e, 0x0001, 0x0001, "Resource file" },
	/*   */ { 0x5e, 0x8001, 0x8001, "ORCA/Disassembler template" },
	/*   */ { 0x5e, 0x8003, 0x8003, "DesignMaster document" },
	/*   */ { 0x5e, 0x8008, 0x8008, "ORCA/C symbol file" },
	/*FIN*/ { 0x5f, 0x0000, 0xffff, "Financial document" },
	/*   */ { 0x5f, 0x8001, 0x8001, "Your Money Matters document" },
	/*   */ { 0x5f, 0x8002, 0x8002, "Home Refinancer document" },
	/*BIO*/ { 0x6b, 0x0000, 0xffff, "PC Transporter BIOS" },
	/*TDR*/ { 0x6d, 0x0000, 0xffff, "PC Transporter driver" },
	/*PRE*/ { 0x6e, 0x0000, 0xffff, "PC Transporter pre-boot" },
	/*HDV*/ { 0x6f, 0x0000, 0xffff, "PC Transporter volume" },
	/*WP */ { 0xa0, 0x0000, 0xffff, "WordPerfect document" },
	/*GSB*/ { 0xab, 0x0000, 0xffff, "Apple IIgs BASIC program" },
	/*TDF*/ { 0xac, 0x0000, 0xffff, "Apple IIgs BASIC TDF" },
	/*BDF*/ { 0xad, 0x0000, 0xffff, "Apple IIgs BASIC data" },
	/*SRC*/ { 0xb0, 0x0000, 0xffff, "Apple IIgs source code" },
	/*   */ { 0xb0, 0x0001, 0x0001, "APW Text file" },
	/*   */ { 0xb0, 0x0003, 0x0003, "APW 65816 Assembly source code" },
	/*   */ { 0xb0, 0x0005, 0x0005, "ORCA/Pascal source code" },
	/*   */ { 0xb0, 0x0006, 0x0006, "APW command file" },
	/*   */ { 0xb0, 0x0008, 0x0008, "ORCA/C source code" },
	/*   */ { 0xb0, 0x0009, 0x0009, "APW Linker command file" },
	/*   */ { 0xb0, 0x000a, 0x000a, "APW C source code" },
	/*   */ { 0xb0, 0x000c, 0x000c, "ORCA/Desktop command file" },
	/*   */ { 0xb0, 0x0015, 0x0015, "APW Rez source file" },
	/*   */ { 0xb0, 0x0017, 0x0017, "Installer script" },
	/*   */ { 0xb0, 0x001e, 0x001e, "TML Pascal source code" },
	/*   */ { 0xb0, 0x0116, 0x0116, "ORCA/Disassembler script" },
	/*   */ { 0xb0, 0x0503, 0x0503, "SDE Assembler source code" },
	/*   */ { 0xb0, 0x0506, 0x0506, "SDE command script" },
	/*   */ { 0xb0, 0x0601, 0x0601, "Nifty List data" },
	/*   */ { 0xb0, 0x0719, 0x0719, "PostScript file" },
	/*OBJ*/ { 0xb1, 0x0000, 0xffff, "Apple IIgs object code" },
	/*LIB*/ { 0xb2, 0x0000, 0xffff, "Apple IIgs Library file" },
	/*S16*/ { 0xb3, 0x0000, 0xffff, "GS/OS application" },
	/*RTL*/ { 0xb4, 0x0000, 0xffff, "GS/OS run-time library" },
	/*EXE*/ { 0xb5, 0x0000, 0xffff, "GS/OS shell application" },
	/*PIF*/ { 0xb6, 0x0000, 0xffff, "Permanent initialization file" },
	/*TIF*/ { 0xb7, 0x0000, 0xffff, "Temporary initialization file" },
	/*NDA*/ { 0xb8, 0x0000, 0xffff, "New desk accessory" },
	/*CDA*/ { 0xb9, 0x0000, 0xffff, "Classic desk accessory" },
	/*TOL*/ { 0xba, 0x0000, 0xffff, "Tool" },
	/*DVR*/ { 0xbb, 0x0000, 0xffff, "Apple IIgs device driver file" },
	/*   */ { 0xbb, 0x7e01, 0x7e01, "GNO/ME terminal device driver" },
	/*   */ { 0xbb, 0x7f01, 0x7f01, "GTv videodisc serial driver" },
	/*   */ { 0xbb, 0x7f02, 0x7f02, "GTv videodisc game port driver" },
	/*LDF*/ { 0xbc, 0x0000, 0xffff, "Load file (generic)" },
	/*   */ { 0xbc, 0x4001, 0x4001, "Nifty List module" },
	/*   */ { 0xbc, 0xc001, 0xc001, "Nifty List module" },
	/*   */ { 0xbc, 0x4002, 0x4002, "Super Info module" },
	/*   */ { 0xbc, 0xc002, 0xc002, "Super Info module" },
	/*   */ { 0xbc, 0x4004, 0x4004, "Twilight document" },
	/*   */ { 0xbc, 0xc004, 0xc004, "Twilight document" },
	/*   */ { 0xbc, 0x4006, 0x4006, "Foundation resource editor" },
	/*   */ { 0xbc, 0xc006, 0xc006, "Foundation resource editor" },
	/*   */ { 0xbc, 0x4007, 0x4007, "HyperStudio new button action" },
	/*   */ { 0xbc, 0xc007, 0xc007, "HyperStudio new button action" },
	/*   */ { 0xbc, 0x4008, 0x4008, "HyperStudio screen transition" },
	/*   */ { 0xbc, 0xc008, 0xc008, "HyperStudio screen transition" },
	/*   */ { 0xbc, 0x4009, 0x4009, "DreamGrafix module" },
	/*   */ { 0xbc, 0xc009, 0xc009, "DreamGrafix module" },
	/*   */ { 0xbc, 0x400a, 0x400a, "HyperStudio Extra utility" },
	/*   */ { 0xbc, 0xc00a, 0xc00a, "HyperStudio Extra utility" },
	/*   */ { 0xbc, 0x400f, 0x400f, "HardPressed module" },
	/*   */ { 0xbc, 0xc00f, 0xc00f, "HardPressed module" },
	/*   */ { 0xbc, 0x4010, 0x4010, "Graphic Exchange translator" },
	/*   */ { 0xbc, 0xc010, 0xc010, "Graphic Exchange translator" },
	/*   */ { 0xbc, 0x4011, 0x4011, "Desktop Enhancer blanker" },
	/*   */ { 0xbc, 0xc011, 0xc011, "Desktop Enhancer blanker" },
	/*   */ { 0xbc, 0x4083, 0x4083, "Marinetti link layer module" },
	/*   */ { 0xbc, 0xc083, 0xc083, "Marinetti link layer module" },
	/*FST*/ { 0xbd, 0x0000, 0xffff, "GS/OS File System Translator" },
	/*DOC*/ { 0xbf, 0x0000, 0xffff, "GS/OS document" },
	/*PNT*/ { 0xc0, 0x0000, 0xffff, "Packed super hi-res picture" },
	/*   */ { 0xc0, 0x0000, 0x0000, "Paintworks packed picture" },
	/*   */ { 0xc0, 0x0001, 0x0001, "Packed super hi-res image" },
	/*   */ { 0xc0, 0x0002, 0x0002, "Apple Preferred Format picture" },
	/*   */ { 0xc0, 0x0003, 0x0003, "Packed QuickDraw II PICT file" },
	/*   */ { 0xc0, 0x0080, 0x0080, "TIFF document" },
	/*   */ { 0xc0, 0x0081, 0x0081, "JFIF (JPEG) document" },
	/*   */ { 0xc0, 0x8001, 0x8001, "GTv background image" },
	/*   */ { 0xc0, 0x8005, 0x8005, "DreamGrafix document" },
	/*   */ { 0xc0, 0x8006, 0x8006, "GIF document" },
	/*PIC*/ { 0xc1, 0x0000, 0xffff, "Super hi-res picture" },
	/*   */ { 0xc1, 0x0000, 0x0000, "Super hi-res screen image" },
	/*   */ { 0xc1, 0x0001, 0x0001, "QuickDraw PICT file" },
	/*   */ { 0xc1, 0x0002, 0x0002, "Super hi-res 3200-color screen image" },
	/*   */ { 0xc1, 0x8001, 0x8001, "Allison raw image doc" },
	/*   */ { 0xc1, 0x8002, 0x8002, "ThunderScan image doc" },
	/*   */ { 0xc1, 0x8003, 0x8003, "DreamGrafix document" },
	/*ANI*/ { 0xc2, 0x0000, 0xffff, "Paintworks animation" },
	/*PAL*/ { 0xc3, 0x0000, 0xffff, "Paintworks palette" },
	/*OOG*/ { 0xc5, 0x0000, 0xffff, "Object-oriented graphics" },
	/*   */ { 0xc5, 0x8000, 0x8000, "Draw Plus document" },
	/*   */ { 0xc5, 0xc000, 0xc000, "DYOH architecture doc" },
	/*   */ { 0xc5, 0xc001, 0xc001, "DYOH predrawn objects" },
	/*   */ { 0xc5, 0xc002, 0xc002, "DYOH custom objects" },
	/*   */ { 0xc5, 0xc003, 0xc003, "DYOH clipboard" },
	/*   */ { 0xc5, 0xc004, 0xc004, "DYOH interiors document" },
	/*   */ { 0xc5, 0xc005, 0xc005, "DYOH patterns" },
	/*   */ { 0xc5, 0xc006, 0xc006, "DYOH landscape document" },
	/*   */ { 0xc5, 0xc007, 0xc007, "PyWare Document" },
	/*SCR*/ { 0xc6, 0x0000, 0xffff, "Script" },
	/*   */ { 0xc6, 0x8001, 0x8001, "Davex 8 script" },
	/*   */ { 0xc6, 0x8002, 0x8002, "Universe Master backup script" },
	/*   */ { 0xc6, 0x8003, 0x8003, "Universe Master Chain script" },
	/*CDV*/ { 0xc7, 0x0000, 0xffff, "Control Panel document" },
	/*FON*/ { 0xc8, 0x0000, 0xffff, "Font" },
	/*   */ { 0xc8, 0x0000, 0x0000, "Font (Standard Apple IIgs QuickDraw II Font)" },
	/*   */ { 0xc8, 0x0001, 0x0001, "TrueType font resource" },
	/*   */ { 0xc8, 0x0008, 0x0008, "Postscript font resource" },
	/*   */ { 0xc8, 0x0081, 0x0081, "TrueType font file" },
	/*   */ { 0xc8, 0x0088, 0x0088, "Postscript font file" },
	/*FND*/ { 0xc9, 0x0000, 0xffff, "Finder data" },
	/*ICN*/ { 0xca, 0x0000, 0xffff, "Icons" },
	/*MUS*/ { 0xd5, 0x0000, 0xffff, "Music sequence" },
	/*   */ { 0xd5, 0x0000, 0x0000, "Music Construction Set song" },
	/*   */ { 0xd5, 0x0001, 0x0001, "MIDI Synth sequence" },
	/*   */ { 0xd5, 0x0007, 0x0007, "SoundSmith document" },
	/*   */ { 0xd5, 0x8002, 0x8002, "Diversi-Tune sequence" },
	/*   */ { 0xd5, 0x8003, 0x8003, "Master Tracks Jr. sequence" },
	/*   */ { 0xd5, 0x8004, 0x8004, "Music Writer document" },
	/*   */ { 0xd5, 0x8005, 0x8005, "Arcade King Super music" },
	/*   */ { 0xd5, 0x8006, 0x8006, "Music Composer file" },
	/*INS*/ { 0xd6, 0x0000, 0xffff, "Instrument" },
	/*   */ { 0xd6, 0x0000, 0x0000, "Music Construction Set instrument" },
	/*   */ { 0xd6, 0x0001, 0x0001, "MIDI Synth instrument" },
	/*   */ { 0xd6, 0x8002, 0x8002, "Diversi-Tune instrument" },
	/*MDI*/ { 0xd7, 0x0000, 0xffff, "MIDI data" },
	/*   */ { 0xd7, 0x0000, 0x0000, "MIDI standard data" },
	/*   */ { 0xd7, 0x0080, 0x0080, "MIDI System Exclusive data" },
	/*   */ { 0xd7, 0x8001, 0x8001, "MasterTracks Pro Sysex file" },
	/*SND*/ { 0xd8, 0x0000, 0xffff, "Sampled sound" },
	/*   */ { 0xd8, 0x0000, 0x0000, "Audio IFF document" },
	/*   */ { 0xd8, 0x0001, 0x0001, "AIFF-C document" },
	/*   */ { 0xd8, 0x0002, 0x0002, "ASIF instrument" },
	/*   */ { 0xd8, 0x0003, 0x0003, "Sound resource file" },
	/*   */ { 0xd8, 0x0004, 0x0004, "MIDI Synth wave data" },
	/*   */ { 0xd8, 0x8001, 0x8001, "HyperStudio sound" },
	/*   */ { 0xd8, 0x8002, 0x8002, "Arcade King Super sound" },
	/*   */ { 0xd8, 0x8003, 0x8003, "SoundOff! sound bank" },
	/*DBM*/ { 0xdb, 0x0000, 0xffff, "DB Master document" },
	/*   */ { 0xdb, 0x0001, 0x0001, "DB Master document" },
	/*???*/ { 0xdd, 0x0000, 0xffff, "DDD Deluxe archive" },	// unofficial
	/*LBR*/ { 0xe0, 0x0000, 0xffff, "Archival library" },
	/*   */ { 0xe0, 0x0000, 0x0000, "ALU library" },
	/*   */ { 0xe0, 0x0001, 0x0001, "AppleSingle file" },
	/*   */ { 0xe0, 0x0002, 0x0002, "AppleDouble header file" },
	/*   */ { 0xe0, 0x0003, 0x0003, "AppleDouble data file" },
	/*   */ { 0xe0, 0x0004, 0x0004, "Archiver archive" },
	/*   */ { 0xe0, 0x0005, 0x0005, "DiskCopy 4.2 disk image" },
	/*   */ { 0xe0, 0x0100, 0x0100, "Apple 5.25 disk image" },
	/*   */ { 0xe0, 0x0101, 0x0101, "Profile 5MB disk image" },
	/*   */ { 0xe0, 0x0102, 0x0102, "Profile 10MB disk image" },
	/*   */ { 0xe0, 0x0103, 0x0103, "Apple 3.5 disk image" },
	/*   */ { 0xe0, 0x0104, 0x0104, "SCSI device image" },
	/*   */ { 0xe0, 0x0105, 0x0105, "SCSI hard disk image" },
	/*   */ { 0xe0, 0x0106, 0x0106, "SCSI tape image" },
	/*   */ { 0xe0, 0x0107, 0x0107, "SCSI CD-ROM image" },
	/*   */ { 0xe0, 0x010e, 0x010e, "RAM disk image" },
	/*   */ { 0xe0, 0x010f, 0x010f, "ROM disk image" },
	/*   */ { 0xe0, 0x0110, 0x0110, "File server image" },
	/*   */ { 0xe0, 0x0113, 0x0113, "Hard disk image" },
	/*   */ { 0xe0, 0x0114, 0x0114, "Floppy disk image" },
	/*   */ { 0xe0, 0x0115, 0x0115, "Tape image" },
	/*   */ { 0xe0, 0x011e, 0x011e, "AppleTalk file server image" },
	/*   */ { 0xe0, 0x0120, 0x0120, "DiskCopy 6 disk image" },
	/*   */ { 0xe0, 0x0130, 0x0130, "Universal Disk Image file" },
	/*   */ { 0xe0, 0x8000, 0x8000, "Binary II file" },
	/*   */ { 0xe0, 0x8001, 0x8001, "AppleLink ACU document" },
	/*   */ { 0xe0, 0x8002, 0x8002, "ShrinkIt (NuFX) document" },
	/*   */ { 0xe0, 0x8003, 0x8003, "Universal Disk Image file" },
	/*   */ { 0xe0, 0x8004, 0x8004, "Davex archived volume" },
	/*   */ { 0xe0, 0x8006, 0x8006, "EZ Backup Saveset doc" },
	/*   */ { 0xe0, 0x8007, 0x8007, "ELS DOS 3.3 volume" },
	/*   */ { 0xe0, 0x8008, 0x8008, "UtilityWorks document" },
	/*   */ { 0xe0, 0x800a, 0x800a, "Replicator document" },
	/*   */ { 0xe0, 0x800b, 0x800b, "AutoArk compressed document" },
	/*   */ { 0xe0, 0x800d, 0x800d, "HardPressed compressed data (data fork)" },
	/*   */ { 0xe0, 0x800e, 0x800e, "HardPressed compressed data (rsrc fork)" },
	/*   */ { 0xe0, 0x800f, 0x800f, "HardPressed compressed data (both forks)" },
	/*   */ { 0xe0, 0x8010, 0x8010, "LHA archive" },
	/*ATK*/ { 0xe2, 0x0000, 0xffff, "AppleTalk data" },
	/*   */ { 0xe2, 0xffff, 0xffff, "EasyMount document" },
	/*R16*/ { 0xee, 0x0000, 0xffff, "EDASM 816 relocatable file" },
	/*PAS*/ { 0xef, 0x0000, 0xffff, "Pascal area" },
	/*CMD*/ { 0xf0, 0x0000, 0xffff, "BASIC command" },
	/*???*/ { 0xf1, 0x0000, 0xffff, "User type #1" },
	/*???*/ { 0xf2, 0x0000, 0xffff, "User type #2" },
	/*???*/ { 0xf3, 0x0000, 0xffff, "User type #3" },
	/*???*/ { 0xf4, 0x0000, 0xffff, "User type #4" },
	/*???*/ { 0xf5, 0x0000, 0xffff, "User type #5" },
	/*???*/ { 0xf6, 0x0000, 0xffff, "User type #6" },
	/*???*/ { 0xf7, 0x0000, 0xffff, "User type #7" },
	/*???*/ { 0xf8, 0x0000, 0xffff, "User type #8" },
	/*OS */ { 0xf9, 0x0000, 0xffff, "GS/OS system file" },
	/*OS */ { 0xfa, 0x0000, 0xffff, "Integer BASIC program" },
	/*OS */ { 0xfb, 0x0000, 0xffff, "Integer BASIC variables" },
	/*OS */ { 0xfc, 0x0000, 0xffff, "AppleSoft BASIC program" },
	/*OS */ { 0xfd, 0x0000, 0xffff, "AppleSoft BASIC variables" },
	/*OS */ { 0xfe, 0x0000, 0xffff, "Relocatable code" },
	/*OS */ { 0xff, 0x0000, 0xffff, "ProDOS 8 application" },
};

/*
 * Find an entry in the type description table that matches both file type and
 * aux type.  If no match is found, nil is returned.
 */
/*static*/ const char*
PathProposal::FileTypeDescription(long fileType, long auxType)
{
	int i;

	for (i = NELEM(gTypeDescriptions)-1; i >= 0; i--) {
		if (fileType == gTypeDescriptions[i].fileType &&
			auxType >= gTypeDescriptions[i].minAuxType &&
			auxType <= gTypeDescriptions[i].maxAuxType)
		{
			return gTypeDescriptions[i].descr;
		}
	}

	return nil;
}


/*
 * ===========================================================================
 *		Filename/filetype conversion
 * ===========================================================================
 */

/*
 * Convert a pathname pulled out of an archive to something suitable for the
 * local filesystem.
 *
 * The new pathname may be shorter (because characters were removed) or
 * longer (if we add a "#XXYYYYZ" extension or replace chars with '%' codes).
 */
void
PathProposal::ArchiveToLocal(void)
{
	char* pathBuf;
	const char* startp;
	const char* endp;
	char* dstp;
	int newBufLen;

	/* init output fields */
	fLocalFssep = kLocalFssep;
	fLocalPathName = "";

	/*
	 * Set up temporary buffer space.  The maximum possible expansion
	 * requires converting all chars to '%' codes and adding the longest
	 * possible preservation string.
	 */
	newBufLen = fStoredPathName.GetLength()*3 + kMaxPathGrowth +1;
	pathBuf = fLocalPathName.GetBuffer(newBufLen);
	ASSERT(pathBuf != nil);

	startp = fStoredPathName;
	dstp = pathBuf;
	while (*startp == fStoredFssep) {
		/* ignore leading path sep; always extract to current dir */
		startp++;
	}

	/* normalize all directory components and the filename component */
	while (startp != nil) {
		endp = nil;
		if (fStoredFssep != '\0')
			endp = strchr(startp, fStoredFssep);
		if (endp != nil && endp == startp) {
			/* zero-length subdir component */
			WMSG1("WARNING: zero-length subdir component in '%s'\n", startp);
			startp++;
			continue;
		}
		if (endp != nil) {
			/* normalize directory component */
			NormalizeDirectoryName(startp, endp - startp,
					fStoredFssep, &dstp, newBufLen);

			*dstp++ = fLocalFssep;

			startp = endp +1;
		} else {
			/* normalize filename component */
			NormalizeFileName(startp, strlen(startp),
					fStoredFssep, &dstp, newBufLen);
			*dstp++ = '\0';

			/* add/replace extension if necessary */
			char extBuf[kMaxPathGrowth +1] = "";
			if (fPreservation) {
				AddPreservationString(pathBuf, extBuf);
			} else if (fThreadKind == GenericEntry::kRsrcThread) {
				/* add this in lieu of the preservation extension */
				strcat(pathBuf, kResourceStr);
			}
			if (fAddExtension) {
				AddTypeExtension(pathBuf, extBuf);
			}
			ASSERT(strlen(extBuf) <= kMaxPathGrowth);
			strcat(pathBuf, extBuf);

			startp = nil;	/* we're done, break out of loop */
		}
	}

	/* check for overflow */
	ASSERT(dstp - pathBuf <= newBufLen);

	/*
	 * If "junk paths" is set, drop everything but the last component.
	 */
	if (fJunkPaths) {
		char* lastFssep;
		lastFssep = strrchr(pathBuf, fLocalFssep);
		if (lastFssep != nil) {
			ASSERT(*(lastFssep+1) != '\0'); /* should already have been caught*/
			memmove(pathBuf, lastFssep+1, strlen(lastFssep+1)+1);
		}
	}

	fLocalPathName.ReleaseBuffer();
}

#if defined(WINDOWS_LIKE)
/*
 * You can't create files or directories with these names on a FAT filesystem,
 * because they're MS-DOS "device special files".
 *
 * The list comes from the Linux kernel's fs/msdos/namei.c.
 *
 * The trick is that the name can't start with any of these.  That could mean
 * that the name is just "aux", or it could be "aux.this.txt".
 */
static const char* gFatReservedNames3[] = {
	"CON", "PRN", "NUL", "AUX", nil
};
static const char* gFatReservedNames4[] = {
	"LPT1", "LPT2", "LPT3", "LPT4", "COM1", "COM2", "COM3", "COM4", nil
};

/*
 * Filename normalization for Win32 filesystems.  You can't use [ \/:*?"<>| ]
 * or control characters, and it's probably unwise to use high-ASCII stuff.
 */
void
PathProposal::Win32NormalizeFileName(const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	char* dstp = *pDstp;
	const char* startp = srcp;
	static const char* kInvalid = "\\/:*?\"<>|";

	/* match on "aux" or "aux.blah" */
	if (srcLen >= 3) {
		const char** ppcch;

		for (ppcch = gFatReservedNames3; *ppcch != nil; ppcch++) {
			if (strncasecmp(srcp, *ppcch, 3) == 0 &&
				(srcp[3] == '.' || srcLen == 3))
			{
				WMSG1("--- fixing '%s'\n", *ppcch);
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
		const char** ppcch;

		for (ppcch = gFatReservedNames4; *ppcch != nil; ppcch++) {
			if (strncasecmp(srcp, *ppcch, 4) == 0 &&
				(srcp[4] == '.' || srcLen == 4))
			{
				WMSG1("--- fixing '%s'\n", *ppcch);
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


	while (srcLen--) {		/* don't go until null found! */
		ASSERT(*srcp != '\0');

		if (*srcp == kForeignIndic) {
			/* change '%' to "%%" */
			if (fPreservation)
				*dstp++ = *srcp;
			*dstp++ = *srcp++;
		} else if (strchr(kInvalid, *srcp) != nil ||
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

	*dstp = '\0';		/* end the string, but don't advance past the null */
	ASSERT(*pDstp - dstp <= dstLen);	/* make sure we didn't overflow */
	*pDstp = dstp;
}
#endif


/*
 * Normalize a file name to local filesystem conventions.  The input
 * is quite possibly *NOT* null-terminated, since it may represent a
 * substring of a full pathname.  Use "srcLen".
 *
 * The output filename is copied to *pDstp, which is advanced forward.
 *
 * The output buffer must be able to hold 3x the original string length.
 */
void
PathProposal::NormalizeFileName(const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	ASSERT(srcp != nil);
	ASSERT(srcLen > 0);
	ASSERT(dstLen > srcLen);
	ASSERT(pDstp != nil);
	ASSERT(*pDstp != nil);

#if defined(UNIX_LIKE)
	UNIXNormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
#elif defined(WINDOWS_LIKE)
	Win32NormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
#else
	#error "port this"
#endif
}


/*
 * Normalize a directory name to local filesystem conventions.
 */
void
PathProposal::NormalizeDirectoryName(const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	/* in general, directories and filenames are the same */
	ASSERT(fssep > ' ' && fssep < 0x7f);
	NormalizeFileName(srcp, srcLen, fssep, pDstp, dstLen);
}


/*
 * Add a preservation string.
 *
 * "pathBuf" is assumed to have enough space to hold the current path
 * plus kMaxPathGrowth more.  It will be modified in place.
 */
void
PathProposal::AddPreservationString(const char* pathBuf, char* extBuf)
{
	char* cp;

	ASSERT(pathBuf != nil);
	ASSERT(extBuf != nil);
	ASSERT(fPreservation);

	cp = extBuf + strlen(extBuf);

	/*
	 * Cons up a preservation string.  On some platforms "sprintf" doesn't
	 * return the #of characters written, so we add it up manually.
	 */
	if (fFileType < 0x100 && fAuxType < 0x10000) {
		sprintf(cp, "%c%02lx%04lx", kPreserveIndic, fFileType, fAuxType);
		cp += 7;
	} else {
		sprintf(cp, "%c%08lx%08lx", kPreserveIndic, fFileType, fAuxType);
		cp += 17;
	}

	if (fThreadKind == GenericEntry::kRsrcThread)
		*cp++ = kResourceFlag;
	else if (fThreadKind == GenericEntry::kDiskImageThread)
		*cp++ = kDiskImageFlag;


	/* make sure it's terminated */
	*cp = '\0';
}

/*
 * Add a ".foo" extension to the filename.
 *
 * We either need to retain the existing extension (possibly obscured by file
 * type preservation) or append an extension based on the ProDOS file type.
 */
void
PathProposal::AddTypeExtension(const char* pathBuf, char* extBuf)
{
	const char* pPathExt = nil;
	const char* pWantedExt = nil;
	const char* pTypeExt = nil;
	char* end;
	char* cp;

	cp = extBuf + strlen(extBuf);

	/*
	 * Find extension in the local filename that we've prepared so far.
	 * Note FindExtension guarantees there's at least one char after '.'.
	 */
	pPathExt = FindExtension(pathBuf, fLocalFssep);
	if (pPathExt == nil) {
		/*
		 * There's no extension on the filename.  Use the standard
		 * ProDOS type, if one exists for this entry.  We don't use
		 * the table if it's NON, "???", or a hex value.
		 */
		if (fFileType) {
			pTypeExt = FileTypeString(fFileType);
			if (pTypeExt[0] == '?' || pTypeExt[0] == '$')
				pTypeExt = nil;
		}
	} else {
		pPathExt++;		// skip leading '.'
	}

	/*
	 * Figure out what extension we want this file to have.  Files of type
	 * text are *always* ".TXT", and our extracted disk images are always
	 * ".PO".  If it's not one of these two, we either retain the file's
	 * original extension, or generate one for it from the ProDOS file type.
	 */
	if (fFileType == 0x04)
		pWantedExt = "TXT";
	else if (fThreadKind == GenericEntry::kDiskImageThread)
		pWantedExt = "PO";
	else {
		/*
		 * We want to use the extension currently on the file, if it has one.
		 * If not, use the one from the file type.
		 */
		if (pPathExt != nil) {
			pWantedExt = pPathExt;
		} else {
			pWantedExt = pTypeExt;
		}
	}
	/* pWantedExt != nil unless we failed to find a pTypeExt */


	/*
	 * Now we know which one we want.  Figure out if we want to add it.
	 */
	if (pWantedExt != nil) {
		if (extBuf[0] == '\0' && pPathExt != nil &&
			strcasecmp(pPathExt, pWantedExt) == 0)
		{
			/* don't add an extension that's already there */
			pWantedExt = nil;
			goto know_ext;
		}

		if (strlen(pWantedExt) >= kMaxExtLen) {
			/* too long, forget it */
			pWantedExt = nil;
			goto know_ext;
		}

		/* if it's strictly decimal-numeric, don't use it (.1, .2, etc) */
		(void) strtoul(pWantedExt, &end, 10);
		if (*end == '\0') {
			pWantedExt = nil;
			goto know_ext;
		}

		/* if '#' appears in it, don't use it -- it'll confuse us */
		//WMSG2("LOOKING FOR '%c' in '%s'\n", kPreserveIndic, ccp);
		const char* ccp = pWantedExt;
		while (*ccp != '\0') {
			if (*ccp == kPreserveIndic) {
				pWantedExt = nil;
				goto know_ext;
			}
			ccp++;
		}
	}
know_ext:

	/*
	 * If pWantedExt is non-nil, it points to a filename extension without
	 * the leading '.'.
	 */
	if (pWantedExt != nil) {
		*cp++ = kFilenameExtDelim;
		strcpy(cp, pWantedExt);
		//cp += strlen(pWantedExt);
	}
}


/*
 * ===========================================================================
 *		File type restoration
 * ===========================================================================
 */

typedef bool Boolean;

/*
 * Convert a local path into something suitable for storage in an archive.
 * Type preservation strings are interpreted and stripped as appropriate.
 *
 * This does *not* do filesystem-specific normalization here.  (It could, but
 * it's better to leave that for later so we can do uniqueification.)
 *
 * In the current implementation, fStoredPathName will always get smaller,
 * but it would be unwise to rely on that.
 */
void
PathProposal::LocalToArchive(const AddFilesDialog* pAddOpts)
{
	Boolean wasPreserved;
	Boolean doJunk = false;
	Boolean adjusted;
	char slashDotDotSlash[5] = "_.._";

	fStoredPathName = fLocalPathName;
	char* livePathStr = fStoredPathName.GetBuffer(0);

	fStoredFssep = kDefaultStoredFssep;

	/* convert '/' to '\' */
	ReplaceFssep(livePathStr,
		kAltLocalFssep,	//NState_GetAltSystemPathSeparator(pState),
		kLocalFssep,	//NState_GetSystemPathSeparator(pState),
		kLocalFssep);	//NState_GetSystemPathSeparator(pState));

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
			memmove(livePathStr, livePathStr+1, strlen(livePathStr));
			adjusted = true;
		}

		/*
		 * Remove leading "./".
		 */
		while (livePathStr[0] == '.' && livePathStr[1] == kLocalFssep)
		{
			/* slide it down, len is (strlen +1) -2 (dropping two chars) */
			memmove(livePathStr, livePathStr+2, strlen(livePathStr)-1);
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
		(strstr(livePathStr, slashDotDotSlash) != nil))
	{
		WMSG1("Found dot dot in '%s', keeping only filename\n", livePathStr);
		doJunk = true;
	}

	/*
	 * Scan for and remove "/./" and trailing "/.".  They're filesystem
	 * no-ops that work just fine under Win32 and UNIX but could confuse
	 * a IIgs.	(Of course, the user could just omit them from the pathname.)
	 */
	/* TO DO 20030208 */

	/*
	 * If "junk paths" is set, drop everything before the last fssep char.
	 */
	if (pAddOpts->fStripFolderNames || doJunk) {
		char* lastFssep;
		lastFssep = strrchr(livePathStr, kLocalFssep);
		if (lastFssep != nil) {
			ASSERT(*(lastFssep+1) != '\0'); /* should already have been caught*/
			memmove(livePathStr, lastFssep+1, strlen(lastFssep+1)+1);
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

/*
 * Replace "oldc" with "newc".  If we find an instance of "newc" already
 * in the string, replace it with "newSubst".
 */
void
PathProposal::ReplaceFssep(char* str, char oldc, char newc, char newSubst)
{
    while (*str != '\0') {
        if (*str == oldc)
            *str = newc;
        else if (*str == newc)
            *str = newSubst;
        str++;
    }
}


/*
 * Try to figure out what file type is associated with a filename extension.
 *
 * This checks the standard list of ProDOS types (which should catch things
 * like "TXT" and "BIN") and the separate list of recognized extensions.
 */
void
PathProposal::LookupExtension(const char* ext)
{
	char uext3[4];
	int i, extLen;

	extLen = strlen(ext);
	ASSERT(extLen > 0);

	/*
	 * First step is to try to find it in the recognized types list.
	 */
	for (i = 0; i < NELEM(gRecognizedExtensions); i++) {
		if (strcasecmp(ext, gRecognizedExtensions[i].label) == 0) {
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

		/*printf("### converted '%s' to '%s'\n", ext, uext3);*/

		for (i = 0; i < NELEM(gFileTypeNames); i++) {
			if (strcmp(uext3, gFileTypeNames[i]) == 0) {
				fFileType = i;
				goto bail;
			}
		}
	}

bail:
	return;
}

/*
 * Try to associate some meaning with the file extension.
 */
void
PathProposal::InterpretExtension(const char* pathName)
{
	const char* pExt;

	ASSERT(pathName != nil);

	pExt = FindExtension(pathName, fLocalFssep);
	if (pExt != nil)
		LookupExtension(pExt+1);
}


/*
 * Check to see if there's a preservation string on the filename.  If so,
 * set the filetype and auxtype information, and trim the preservation
 * string off.
 *
 * We have to be careful not to trip on false-positive occurrences of '#'
 * in the filename.
 */
Boolean
PathProposal::ExtractPreservationString(char* pathname)
{
	char numBuf[9];
	unsigned long fileType, auxType;
	int threadMask;
	char* pPreserve;
	char* cp;
	int digitCount;

	ASSERT(pathname != nil);

	pPreserve = strrchr(pathname, kPreserveIndic);
	if (pPreserve == nil)
		return false;

	/* count up the #of hex digits */
	digitCount = 0;
	for (cp = pPreserve+1; *cp != '\0' && isxdigit((int)*cp); cp++)
		digitCount++;

	/* extract the file and aux type */
	switch (digitCount) {
	case 6:
		/* ProDOS 1-byte type and 2-byte aux */
		memcpy(numBuf, pPreserve+1, 2);
		numBuf[2] = 0;
		fileType = strtoul(numBuf, &cp, 16);
		ASSERT(cp == numBuf + 2);

		auxType = strtoul(pPreserve+3, &cp, 16);
		ASSERT(cp == pPreserve + 7);
		break;
	case 16:
		/* HFS 4-byte type and 4-byte creator */
		memcpy(numBuf, pPreserve+1, 8);
		numBuf[8] = 0;
		fileType = strtoul(numBuf, &cp, 16);
		ASSERT(cp == numBuf + 8);

		auxType = strtoul(pPreserve+9, &cp, 16);
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
	case kFilenameExtDelim: 	/* redundant "-ee" extension */
	case '\0':					/* end of string! */
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


/*
 * Remove NuLib2's normalization magic (e.g. "%2f" for '/').
 *
 * This always results in the filename staying the same length or getting
 * smaller, so we can do it in place in the buffer.
 */
void
PathProposal::DenormalizePath(char* pathBuf)
{
	const char* srcp;
	char* dstp;
	char ch;

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
					/* valid, output char */
					ch += HexDigit(*srcp);
					*dstp++ = ch;
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

/*
 * Remove a disk image suffix.
 *
 * Useful when adding disk images directly from a .SDK or .2MG file.  We
 * don't want them to retain their original suffix.
 */
void
PathProposal::StripDiskImageSuffix(char* pathName)
{
	static const char diskExt[][4] = {
		"SHK", "SDK", "IMG", "PO", "DO", "2MG", "DSK"
	};
	char* pExt;
	int i;

	pExt = (char*)FindExtension(pathName, fLocalFssep);
	if (pExt == nil || pExt == pathName)
		return;

	for (i = 0; i < NELEM(diskExt); i++) {
		if (strcasecmp(pExt+1, diskExt[i]) == 0) {
			WMSG2("Dropping '%s' from '%s'\n", pExt, pathName);
			*pExt = '\0';
			return;
		}
	}
}

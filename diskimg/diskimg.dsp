# Microsoft Developer Studio Project File - Name="diskimg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=diskimg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "diskimg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "diskimg.mak" CFG="diskimg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "diskimg - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "diskimg - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "diskimg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DISKIMG_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUGX" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DISKIMG_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ..\prebuilt\nufxlib2.lib ..\prebuilt\zdll.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"Release/diskimg4.dll"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying DLL to app directory
PostBuild_Cmds=copy Release\diskimg4.dll ..\app	copy Release\diskimg4.dll ..\mdc
# End Special Build Tool

!ELSEIF  "$(CFG)" == "diskimg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DISKIMG_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "DISKIMG_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\prebuilt\nufxlib2D.lib ..\prebuilt\zdll.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"Debug/diskimg4.dll" /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying debug DLL to app directory
PostBuild_Cmds=copy Debug\diskimg4.dll ..\app	copy Debug\diskimg4.dll ..\mdc
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "diskimg - Win32 Release"
# Name "diskimg - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ASPI.cpp
# End Source File
# Begin Source File

SOURCE=.\CFFA.cpp
# End Source File
# Begin Source File

SOURCE=.\Container.cpp
# End Source File
# Begin Source File

SOURCE=.\CPM.cpp
# End Source File
# Begin Source File

SOURCE=.\DDD.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskFS.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskImg.cpp
# End Source File
# Begin Source File

SOURCE=.\DIUtil.cpp
# End Source File
# Begin Source File

SOURCE=.\DOS33.cpp
# End Source File
# Begin Source File

SOURCE=.\DOSImage.cpp
# End Source File
# Begin Source File

SOURCE=.\FAT.CPP
# End Source File
# Begin Source File

SOURCE=.\FDI.cpp
# End Source File
# Begin Source File

SOURCE=.\FocusDrive.cpp
# End Source File
# Begin Source File

SOURCE=.\GenericFD.cpp
# End Source File
# Begin Source File

SOURCE=.\Global.cpp
# End Source File
# Begin Source File

SOURCE=.\Gutenberg.cpp
# End Source File
# Begin Source File

SOURCE=.\HFS.cpp
# End Source File
# Begin Source File

SOURCE=.\ImageWrapper.cpp
# End Source File
# Begin Source File

SOURCE=.\MacPart.cpp
# End Source File
# Begin Source File

SOURCE=.\MicroDrive.cpp
# End Source File
# Begin Source File

SOURCE=.\Nibble.cpp
# End Source File
# Begin Source File

SOURCE=.\Nibble35.cpp
# End Source File
# Begin Source File

SOURCE=.\OuterWrapper.cpp
# End Source File
# Begin Source File

SOURCE=.\OzDOS.cpp
# End Source File
# Begin Source File

SOURCE=.\Pascal.cpp
# End Source File
# Begin Source File

SOURCE=.\ProDOS.cpp
# End Source File
# Begin Source File

SOURCE=.\RDOS.cpp
# End Source File
# Begin Source File

SOURCE=.\SPTI.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\TwoImg.cpp
# End Source File
# Begin Source File

SOURCE=.\UNIDOS.cpp
# End Source File
# Begin Source File

SOURCE=.\VolumeUsage.cpp
# End Source File
# Begin Source File

SOURCE=.\Win32BlockIO.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ASPI.h
# End Source File
# Begin Source File

SOURCE=.\CP_ntddscsi.h
# End Source File
# Begin Source File

SOURCE=.\CP_WNASPI32.H
# End Source File
# Begin Source File

SOURCE=.\DiskImg.h
# End Source File
# Begin Source File

SOURCE=.\DiskImgDetail.h
# End Source File
# Begin Source File

SOURCE=.\DiskImgPriv.h
# End Source File
# Begin Source File

SOURCE=.\GenericFD.h
# End Source File
# Begin Source File

SOURCE=.\SCSIDefs.h
# End Source File
# Begin Source File

SOURCE=.\SPTI.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TwoImg.h
# End Source File
# Begin Source File

SOURCE=.\Win32BlockIO.h
# End Source File
# Begin Source File

SOURCE=.\Win32Extra.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project

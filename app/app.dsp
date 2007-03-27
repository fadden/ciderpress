# Microsoft Developer Studio Project File - Name="app" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=app - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "app.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "app.mak" CFG="app - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "app - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "app - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "app - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUGX" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FR /YX"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 ..\prebuilt\nufxlib2.lib ..\prebuilt\zdll.lib /nologo /subsystem:windows /map /machine:I386 /out:"Release/CiderPress.exe"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying DLLs and help
PostBuild_Cmds=copy                                        Help\CiderPress.hlp                                        Release	copy                                        Help\CiderPress.cnt                                        Release	copy ..\prebuilt\nufxlib2.dll .	copy ..\prebuilt\zlib1.dll .
# End Special Build Tool

!ELSEIF  "$(CFG)" == "app - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FR /YX"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\prebuilt\nufxlib2D.lib ..\prebuilt\zdll.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Debug/CiderPress.exe" /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying debug DLLs and help
PostBuild_Cmds=copy                                        Help\CiderPress.hlp                                        Debug	copy                                        Help\CiderPress.cnt                                        Debug	copy ..\prebuilt\nufxlib2D.dll .	copy ..\prebuilt\zlib1.dll .
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "app - Win32 Release"
# Name "app - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AboutDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ActionProgressDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Actions.cpp
# End Source File
# Begin Source File

SOURCE=.\ACUArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\AddClashDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\AddFilesDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ArchiveInfoDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\BasicImport.cpp
# End Source File
# Begin Source File

SOURCE=.\BNYArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\CassetteDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\CassImpTargetDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ChooseAddTargetDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ChooseDirDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\CiderPress.rc
# End Source File
# Begin Source File

SOURCE=.\Clipboard.cpp
# End Source File
# Begin Source File

SOURCE=.\ConfirmOverwriteDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ContentList.cpp
# End Source File
# Begin Source File

SOURCE=.\ConvDiskOptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ConvFileOptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\CreateImageDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\CreateSubdirDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\DEFileDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskConvertDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskEditDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskEditOpenDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\DiskFSTree.cpp
# End Source File
# Begin Source File

SOURCE=.\EditAssocDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EditCommentDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EditPropsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EnterRegDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\EOLScanDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ExtractOptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\FileNameConv.cpp
# End Source File
# Begin Source File

SOURCE=.\GenericArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\ImageFormatDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Main.cpp
# End Source File
# Begin Source File

SOURCE=.\MyApp.cpp
# End Source File
# Begin Source File

SOURCE=.\NewDiskSize.cpp
# End Source File
# Begin Source File

SOURCE=.\NewFolderDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\NufxArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenVolumeDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\PasteSpecialDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Preferences.cpp
# End Source File
# Begin Source File

SOURCE=.\PrefsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Print.cpp
# End Source File
# Begin Source File

SOURCE=.\RecompressOptionsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Registry.cpp
# End Source File
# Begin Source File

SOURCE=.\RenameEntryDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\RenameVolumeDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Squeeze.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\SubVolumeDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools.cpp
# End Source File
# Begin Source File

SOURCE=.\TwoImgPropsDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\UseSelectionDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ViewFilesDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\VolumeCopyDialog.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\AboutDialog.h
# End Source File
# Begin Source File

SOURCE=.\ActionProgressDialog.h
# End Source File
# Begin Source File

SOURCE=.\ACUArchive.h
# End Source File
# Begin Source File

SOURCE=.\AddClashDialog.h
# End Source File
# Begin Source File

SOURCE=.\AddFilesDialog.h
# End Source File
# Begin Source File

SOURCE=.\ArchiveInfoDialog.h
# End Source File
# Begin Source File

SOURCE=.\BasicImport.h
# End Source File
# Begin Source File

SOURCE=.\BNYArchive.h
# End Source File
# Begin Source File

SOURCE=.\CassetteDialog.h
# End Source File
# Begin Source File

SOURCE=.\CassImpTargetDialog.h
# End Source File
# Begin Source File

SOURCE=.\ChooseAddTargetDialog.h
# End Source File
# Begin Source File

SOURCE=.\ChooseDirDialog.h
# End Source File
# Begin Source File

SOURCE=.\ConfirmOverwriteDialog.h
# End Source File
# Begin Source File

SOURCE=.\ContentList.h
# End Source File
# Begin Source File

SOURCE=.\ConvDiskOptionsDialog.h
# End Source File
# Begin Source File

SOURCE=.\ConvFileOptionsDialog.h
# End Source File
# Begin Source File

SOURCE=.\CreateImageDialog.h
# End Source File
# Begin Source File

SOURCE=.\CreateSubdirDialog.h
# End Source File
# Begin Source File

SOURCE=.\DEFileDialog.h
# End Source File
# Begin Source File

SOURCE=.\DiskArchive.h
# End Source File
# Begin Source File

SOURCE=.\DiskConvertDialog.h
# End Source File
# Begin Source File

SOURCE=.\DiskEditDialog.h
# End Source File
# Begin Source File

SOURCE=.\DiskEditOpenDialog.h
# End Source File
# Begin Source File

SOURCE=.\DiskFSTree.h
# End Source File
# Begin Source File

SOURCE=.\DoneOpenDialog.h
# End Source File
# Begin Source File

SOURCE=.\EditAssocDialog.h
# End Source File
# Begin Source File

SOURCE=.\EditCommentDialog.h
# End Source File
# Begin Source File

SOURCE=.\EditPropsDialog.h
# End Source File
# Begin Source File

SOURCE=.\EnterRegDialog.h
# End Source File
# Begin Source File

SOURCE=.\EOLScanDialog.h
# End Source File
# Begin Source File

SOURCE=.\ExtractOptionsDialog.h
# End Source File
# Begin Source File

SOURCE=.\FileNameConv.h
# End Source File
# Begin Source File

SOURCE=.\GenericArchive.h
# End Source File
# Begin Source File

SOURCE=.\HelpTopics.h
# End Source File
# Begin Source File

SOURCE=.\ImageFormatDialog.h
# End Source File
# Begin Source File

SOURCE=.\Main.h
# End Source File
# Begin Source File

SOURCE=.\MyApp.h
# End Source File
# Begin Source File

SOURCE=.\NewDiskSize.h
# End Source File
# Begin Source File

SOURCE=.\NewFolderDialog.h
# End Source File
# Begin Source File

SOURCE=.\NufxArchive.h
# End Source File
# Begin Source File

SOURCE=.\OpenVolumeDialog.h
# End Source File
# Begin Source File

SOURCE=.\PasteSpecialDialog.h
# End Source File
# Begin Source File

SOURCE=.\Preferences.h
# End Source File
# Begin Source File

SOURCE=.\PrefsDialog.h
# End Source File
# Begin Source File

SOURCE=.\Print.h
# End Source File
# Begin Source File

SOURCE=.\ProgressCounterDialog.h
# End Source File
# Begin Source File

SOURCE=.\RecompressOptionsDialog.h
# End Source File
# Begin Source File

SOURCE=.\Registry.h
# End Source File
# Begin Source File

SOURCE=.\RenameEntryDialog.h
# End Source File
# Begin Source File

SOURCE=.\RenameVolumeDialog.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\Squeeze.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\SubVolumeDialog.h
# End Source File
# Begin Source File

SOURCE=.\TwoImgPropsDialog.h
# End Source File
# Begin Source File

SOURCE=.\UseSelectionDialog.h
# End Source File
# Begin Source File

SOURCE=.\ViewFilesDialog.h
# End Source File
# Begin Source File

SOURCE=.\VolumeCopyDialog.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\Graphics\binary2.ico
# End Source File
# Begin Source File

SOURCE=.\Graphics\ChooseFolder.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\CiderPress.ico
# End Source File
# Begin Source File

SOURCE=.\Graphics\diskimage.ico
# End Source File
# Begin Source File

SOURCE=.\Graphics\FileViewer.ico
# End Source File
# Begin Source File

SOURCE=.\Graphics\fslogo.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\hdrbar.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\icon2.ico
# End Source File
# Begin Source File

SOURCE=".\Graphics\list-pics.bmp"
# End Source File
# Begin Source File

SOURCE=.\Graphics\NewFolder.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\nufx.ico
# End Source File
# Begin Source File

SOURCE=.\Graphics\toolbar1.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\tree_pics.bmp
# End Source File
# Begin Source File

SOURCE=.\Graphics\vol_pics.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\Help\CIDERPRESS.HLP
# End Source File
# Begin Source File

SOURCE=..\LICENSE.txt
# End Source File
# End Target
# End Project

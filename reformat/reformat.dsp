# Microsoft Developer Studio Project File - Name="reformat" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=reformat - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "reformat.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "reformat.mak" CFG="reformat - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "reformat - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "reformat - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "reformat - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "reformat - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "reformat - Win32 Release"
# Name "reformat - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AppleWorks.cpp
# End Source File
# Begin Source File

SOURCE=.\Asm.cpp
# End Source File
# Begin Source File

SOURCE=.\AWGS.cpp
# End Source File
# Begin Source File

SOURCE=.\BASIC.cpp
# End Source File
# Begin Source File

SOURCE=.\CPMFiles.cpp
# End Source File
# Begin Source File

SOURCE=.\Directory.cpp
# End Source File
# Begin Source File

SOURCE=.\Disasm.cpp
# End Source File
# Begin Source File

SOURCE=.\DisasmTable.cpp
# End Source File
# Begin Source File

SOURCE=.\DoubleHiRes.cpp
# End Source File
# Begin Source File

SOURCE=.\DreamGrafix.cpp
# End Source File
# Begin Source File

SOURCE=.\HiRes.cpp
# End Source File
# Begin Source File

SOURCE=.\MacPaint.cpp
# End Source File
# Begin Source File

SOURCE=.\NiftyList.cpp
# End Source File
# Begin Source File

SOURCE=.\PascalFiles.cpp
# End Source File
# Begin Source File

SOURCE=.\PrintShop.cpp
# End Source File
# Begin Source File

SOURCE=.\Reformat.cpp
# End Source File
# Begin Source File

SOURCE=.\ReformatBase.cpp
# End Source File
# Begin Source File

SOURCE=.\ResourceFork.cpp
# End Source File
# Begin Source File

SOURCE=.\Simple.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\SuperHiRes.cpp
# End Source File
# Begin Source File

SOURCE=.\Teach.cpp
# End Source File
# Begin Source File

SOURCE=.\Text8.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\AppleWorks.h
# End Source File
# Begin Source File

SOURCE=.\Asm.h
# End Source File
# Begin Source File

SOURCE=.\AWGS.h
# End Source File
# Begin Source File

SOURCE=.\BASIC.h
# End Source File
# Begin Source File

SOURCE=.\CPMFiles.h
# End Source File
# Begin Source File

SOURCE=.\Directory.h
# End Source File
# Begin Source File

SOURCE=.\Disasm.h
# End Source File
# Begin Source File

SOURCE=.\DoubleHiRes.h
# End Source File
# Begin Source File

SOURCE=.\HiRes.h
# End Source File
# Begin Source File

SOURCE=.\MacPaint.h
# End Source File
# Begin Source File

SOURCE=.\PascalFiles.h
# End Source File
# Begin Source File

SOURCE=.\PrintShop.h
# End Source File
# Begin Source File

SOURCE=.\Reformat.h
# End Source File
# Begin Source File

SOURCE=.\ReformatBase.h
# End Source File
# Begin Source File

SOURCE=.\ResourceFork.h
# End Source File
# Begin Source File

SOURCE=.\Simple.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\SuperHiRes.h
# End Source File
# Begin Source File

SOURCE=.\Teach.h
# End Source File
# Begin Source File

SOURCE=.\Text8.h
# End Source File
# End Group
# End Target
# End Project

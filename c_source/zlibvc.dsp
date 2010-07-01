# Microsoft Developer Studio Project File - Name="zlibvc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102
# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=zlibvc - Win32 DLL Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "zlibvc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "zlibvc.mak" CFG="zlibvc - Win32 DLL Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "zlibvc - Win32 DLL Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "zlibvc - Win32 DLL Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "zlibvc - Win32 DLL ASM Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "zlibvc - Win32 DLL ASM Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "zlibvc - Win32 DLL ASM Obj Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "zlibvc - Win32 static ASM Release" (based on "Win32 (x86) Static Library")
!MESSAGE "zlibvc - Win32 static Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "zlibvc - Win32 DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_DLL_Release"
# PROP BASE Intermediate_Dir "zlibvc___Win32_DLL_Release"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zlibvc___Win32_DLL_Release"
# PROP Intermediate_Dir "zlibvc___Win32_DLL_Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
LIB32=link.exe -lib
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /FD /c
# ADD CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /D "ZLIB_DLL" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x40c /d "WIN32" /d "NDEBUG"
# ADD RSC /l 0x409 /fo"zlibvc___Win32_DLL_Release\lvzlib.res" /d "WIN32" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /pdb:none /machine:I386
# ADD LINK32 user32.lib kernel32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /out:"zlibvc___Win32_DLL_Release\lvzlib.dll"

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "zlibvc___Win32_DLL_Debug"
# PROP BASE Intermediate_Dir "zlibvc___Win32_DLL_Debug"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "zlibvc___Win32_DLL_Debug"
# PROP Intermediate_Dir "zlibvc___Win32_DLL_Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
LIB32=link.exe -lib
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W3 /Od /D "WIN32" /D "_DEBUG" /D "NO_vsnprintf" /FD /c
# ADD CPP /nologo /MDd /W3 /ZI /Od /D "WIN32" /D "_DEBUG" /D "NO_vsnprintf" /D "ZLIB_DLL" /FR /FD /c
# SUBTRACT CPP /YX /Yc /Yu
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "_DEBUG"
# ADD RSC /l 0x409 /fo"zlibvc___Win32_DLL_Debug\lvzlib.res" /d "WIN32" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 user32.lib kernel32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrt" /out:"zlibvc___Win32_DLL_Debug\lvzlib.dll"
# SUBTRACT LINK32 /incremental:no

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_DLL_ASM_Release"
# PROP BASE Intermediate_Dir "zlibvc___Win32_DLL_ASM_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zlibvc___Win32_DLL_ASM_Release"
# PROP Intermediate_Dir "zlibvc___Win32_DLL_ASM_Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
LIB32=link.exe -lib
CPP=cl.exe
# ADD BASE CPP /nologo /MTd /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /D "ASMV" /D "ASMINF" /D "ZLIB_DLL" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "NDEBUG" /d "NO_vsnprintf"
# ADD RSC /l 0x409 /fo"zlibvc___Win32_DLL_ASM_Release\lvzlib.res" /d "WIN32" /d "NDEBUG" /d "NO_vsnprintf"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /pdb:none /machine:I386
# ADD LINK32 user32.lib kernel32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /out:"zlibvc___Win32_DLL_ASM_Release\lvzlib.dll"

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_DLL_ASM_Debug"
# PROP BASE Intermediate_Dir "zlibvc___Win32_DLL_ASM_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "zlibvc___Win32_DLL_ASM_Debug"
# PROP Intermediate_Dir "zlibvc___Win32_DLL_ASM_Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
LIB32=link.exe -lib
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W3 /Od /D "WIN32" /D "_DEBUG" /D "NO_vsnprintf" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MDd /W3 /ZI /Od /D "WIN32" /D "_DEBUG" /D "NO_vsnprintf" /D "ASMV" /D "ASMINF" /D "ZLIB_DLL" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "_DEBUG"
# ADD RSC /l 0x409 /fo"zlibvc___Win32_DLL_ASM_Debug\lvzlib.res" /d "WIN32" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 user32.lib kernel32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrt" /out:"zlibvc___Win32_DLL_ASM_Debug\lvzlib.dll"
# SUBTRACT LINK32 /incremental:no

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Obj Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_DLL_ASM_Obj_Release"
# PROP BASE Intermediate_Dir "zlibvc___Win32_DLL_ASM_Obj_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zlibvc___Win32_DLL_ASM_Obj_Release"
# PROP Intermediate_Dir "zlibvc___Win32_DLL_ASM_Obj_Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /D "ASMV" /D "ASMINF" /D "ZLIB_DLL" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "NDEBUG"
# ADD RSC /l 0x409 /d "WIN32" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 /machine:IX86

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static ASM Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_static_ASM_Release"
# PROP BASE Intermediate_Dir "zlibvc___Win32_static_ASM_Release"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zlibvc___Win32_static_ASM_Release"
# PROP Intermediate_Dir "zlibvc___Win32_static_ASM_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /D "ASMV" /D "ASMINF" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MT /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /D "ASMV" /D "ASMINF" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "NDEBUG"
# ADD RSC /l 0x409 /d "WIN32" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zlibvc___Win32_static_Release"
# PROP BASE Intermediate_Dir "zlibvc___Win32_static_Release"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zlibvc___Win32_static_Release"
# PROP Intermediate_Dir "zlibvc___Win32_static_Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MT /W3 /O2 /D "WIN32" /D "NDEBUG" /D "NO_vsnprintf" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "WIN32" /d "NDEBUG"
# ADD RSC /l 0x409 /d "WIN32" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "zlibvc - Win32 DLL Release"
# Name "zlibvc - Win32 DLL Debug"
# Name "zlibvc - Win32 DLL ASM Release"
# Name "zlibvc - Win32 DLL ASM Debug"
# Name "zlibvc - Win32 DLL ASM Obj Release"
# Name "zlibvc - Win32 static ASM Release"
# Name "zlibvc - Win32 static Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\adler32.c
# End Source File
# Begin Source File

SOURCE=.\compress.c
# End Source File
# Begin Source File

SOURCE=.\crc32.c
# End Source File
# Begin Source File

SOURCE=.\deflate.c
# End Source File
# Begin Source File

SOURCE=.\infback.c
# End Source File
# Begin Source File

SOURCE=.\inffast.c
# End Source File
# Begin Source File

SOURCE=.\inflate.c
# End Source File
# Begin Source File

SOURCE=.\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\ioapi.c
# End Source File
# Begin Source File

SOURCE=.\iomem.c
# End Source File
# Begin Source File

SOURCE=.\iowin.c
# End Source File
# Begin Source File

SOURCE=.\lvutil.c
# End Source File
# Begin Source File

SOURCE=.\lvzlib.def
# End Source File
# Begin Source File

SOURCE=.\MacBinaryIII.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\mztools.c
# End Source File
# Begin Source File

SOURCE=.\trees.c
# End Source File
# Begin Source File

SOURCE=.\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\unzip.c
# End Source File
# Begin Source File

SOURCE=.\zip.c
# End Source File
# Begin Source File

SOURCE=.\zutil.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\crc32.h
# End Source File
# Begin Source File

SOURCE=.\crypt.h
# End Source File
# Begin Source File

SOURCE=.\deflate.h
# End Source File
# Begin Source File

SOURCE=.\inffast.h
# End Source File
# Begin Source File

SOURCE=.\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\inflate.h
# End Source File
# Begin Source File

SOURCE=.\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\ioapi.h
# End Source File
# Begin Source File

SOURCE=.\iomem.h
# End Source File
# Begin Source File

SOURCE=.\iowin.h
# End Source File
# Begin Source File

SOURCE=.\lvutil.h
# End Source File
# Begin Source File

SOURCE=.\MacBinaryIII.h
# End Source File
# Begin Source File

SOURCE=.\mztools.h
# End Source File
# Begin Source File

SOURCE=.\trees.h
# End Source File
# Begin Source File

SOURCE=.\unzip.h
# End Source File
# Begin Source File

SOURCE=.\zalias.h
# End Source File
# Begin Source File

SOURCE=.\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zip.h
# End Source File
# Begin Source File

SOURCE=.\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zutil.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\lvzlib.ico
# End Source File
# Begin Source File

SOURCE=.\zlib.rc
# End Source File
# End Group
# Begin Group "Assembler Files"

# PROP Default_Filter "*.asm;*.c"
# Begin Source File

SOURCE=.\gvmat32.asm

!IF  "$(CFG)" == "zlibvc - Win32 DLL Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Release"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_DLL_ASM_Release
InputPath=.\gvmat32.asm
InputName=gvmat32

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /Cx /coff /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Debug"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_DLL_ASM_Debug
InputPath=.\gvmat32.asm
InputName=gvmat32

BuildCmds= \
	ml /nologo /c /Cx /coff /Zi /Fl$(IntDir)\$(InputName).lst /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).lst" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Obj Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static ASM Release"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_static_ASM_Release
InputPath=.\gvmat32.asm
InputName=gvmat32

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /Cx /coff /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static Release"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gvmat32c.c

!IF  "$(CFG)" == "zlibvc - Win32 DLL Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Release"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Debug"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Obj Release"

# PROP BASE Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static ASM Release"

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static Release"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\inffas32.asm

!IF  "$(CFG)" == "zlibvc - Win32 DLL Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Release"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_DLL_ASM_Release
InputPath=.\inffas32.asm
InputName=inffas32

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /Cx /coff /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Debug"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_DLL_ASM_Debug
InputPath=.\inffas32.asm
InputName=inffas32

BuildCmds= \
	ml /nologo /c /Cx /coff /Zi /Fl$(IntDir)\$(InputName).lst /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"$(IntDir)\$(InputName).lst" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 DLL ASM Obj Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static ASM Release"

# Begin Custom Build - Assembling...
IntDir=.\zlibvc___Win32_static_ASM_Release
InputPath=.\inffas32.asm
InputName=inffas32

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	ml /nologo /c /Cx /coff /Zi /Fo"$(IntDir)\$(InputName).obj" "$(InputPath)"

# End Custom Build

!ELSEIF  "$(CFG)" == "zlibvc - Win32 static Release"

# PROP BASE Exclude_From_Build 1
# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\..\cintools\labview.lib
# End Source File
# End Target
# End Project

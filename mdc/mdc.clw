; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=AboutDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "mdc.h"
LastPage=0

ClassCount=2

ResourceCount=4
Resource1=IDD_ABOUTBOX
Class1=AboutDlg
Resource2="IDD_CHOOSE_FILES"
Resource3=IDC_MDC
Class2=ProgressDlg
Resource4=IDD_PROGRESS

[DLG:IDD_ABOUTBOX]
Type=1
Class=AboutDlg
ControlCount=6
Control1=IDC_MYICON,static,1342177283
Control2=IDC_ABOUT_VERS,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342242817
Control5=IDC_STATIC,static,1342308352
Control6=IDC_STATIC,static,1342308352

[MNU:IDC_MDC]
Type=1
Class=?
Command1=IDM_FILE_SCAN
Command2=IDM_FILE_EXIT
Command3=IDM_HELP_WEBSITE
Command4=IDM_HELP_ABOUT
CommandCount=4

[ACL:IDC_MDC]
Type=1
Class=?
Command1=IDM_ABOUT
Command2=IDM_ABOUT
CommandCount=2

[CLS:AboutDlg]
Type=0
HeaderFile=AboutDlg.h
ImplementationFile=AboutDlg.cpp
BaseClass=CDialog
Filter=D
VirtualFilter=dWC
LastObject=AboutDlg

[DLG:"IDD_CHOOSE_FILES"]
Type=1
Class=?
ControlCount=4
Control1=1119,static,1342177280
Control2=IDC_SELECT_ACCEPT,button,1342242816
Control3=IDCANCEL,button,1342242816
Control4=IDC_CHOOSEFILES_STATIC1,static,1342308352

[DLG:IDD_PROGRESS]
Type=1
Class=ProgressDlg
ControlCount=3
Control1=IDCANCEL,button,1342242816
Control2=IDC_STATIC,static,1342308352
Control3=IDC_PROGRESS_FILENAME,static,1342308352

[CLS:ProgressDlg]
Type=0
HeaderFile=ProgressDlg.h
ImplementationFile=ProgressDlg.cpp
BaseClass=CDialog
Filter=C
LastObject=IDCANCEL
VirtualFilter=dWC


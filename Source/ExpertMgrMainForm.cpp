#include <vcl.h>
#pragma hdrstop

#include "ExpertMgrMainForm.h"
#include "sstream"
#include <ExpertEditorForm.h>
#include <ExpertManagerGlobals.h>
#include <regex>
#include "ExpertManagerTypes.h"
#ifdef ENABLE_CODESITE
#include "CodeSiteLogging.hpp"
#endif

#pragma package(smart_init)
#pragma resource "*.dfm"

/** An IDE managed global variable for the application to create the main form. **/
TfrmExpertManager *frmExpertManager;

/**

  The constructor for the TfrmExpertManager class.

  @precon  None.
  @postcon None

  @param   Owner as a TComponent

**/
__fastcall TfrmExpertManager::TfrmExpertManager(TComponent* Owner) : TForm(Owner) {}

/**

  This method loads the applications position and size information from the registry.

  @precon  None.
  @postcon The applications position and size information is loaded from the registry.

**/
void __fastcall TfrmExpertManager::LoadSettings() {
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strRegSettings) );
  Left = iniFile->ReadInteger(L"Setup", L"Left", 100);
  Top = iniFile->ReadInteger("Setup", "Top", 100);
  Width = iniFile->ReadInteger("Setup", "Width", Width);
  Height = iniFile->ReadInteger("Setup", "Height", Height);
  tvExpertInstallations->Width = iniFile->ReadInteger("Setup", "DividerWidth",
    tvExpertInstallations->Width);
  lvInstalledExperts->Columns->Items[0]->Width =
    iniFile->ReadInteger("Setup", "ExpertsListWidth",
      lvInstalledExperts->Columns->Items[0]->Width);
  lvKnownIDEPackages->Columns->Items[0]->Width =
    iniFile->ReadInteger("Setup", "KnownIDEPackagesListWidth",
      lvKnownIDEPackages->Columns->Items[0]->Width);
  lvKnownPackages->Columns->Items[0]->Width =
    iniFile->ReadInteger("Setup", "KnownPackagesListWidth",
      lvKnownPackages->Columns->Items[0]->Width);
  pagPages->ActivePageIndex = iniFile->ReadInteger("Setup", "FocusedPage", pagPages->ActivePageIndex);
  FSelectedNodePath = iniFile->ReadString("Setup", "SelectedNode", "");
}

void __fastcall TfrmExpertManager::SelectTreeViewNode(const String strSelectedPath) {
  for (int iNode = 0; iNode < tvExpertInstallations->Items->Count; iNode++) {
    String strNodePath = FExpandedNodeManager->ConvertNodeToPath(
      tvExpertInstallations->Items->Item[iNode]);
    if (strSelectedPath.Compare(strNodePath) == 0) {
      tvExpertInstallations->Selected = tvExpertInstallations->Items->Item[iNode];
      return;
    }
  }
};

/**

  This method saves the applications position and size information to the registry.

  @precon  None.
  @postcon The applications position and size information is saved to the registry.

**/
void __fastcall TfrmExpertManager::SaveSettings() {
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strRegSettings) );
  iniFile->WriteInteger("Setup", "Left", Left);
  iniFile->WriteInteger("Setup", "Top", Top);
  iniFile->WriteInteger("Setup", "Width", Width);
  iniFile->WriteInteger("Setup", "Height", Height);
  iniFile->WriteInteger("Setup", "DividerWidth", tvExpertInstallations->Width);
  iniFile->WriteInteger("Setup", "ExpertsListWidth", lvInstalledExperts->Columns->Items[0]->Width);
  iniFile->WriteInteger("Setup", "KnownIDEPackagesListWidth", lvKnownIDEPackages->Columns->Items[0]->Width);
  iniFile->WriteInteger("Setup", "KnownPackagesListWidth", lvKnownPackages->Columns->Items[0]->Width);
  iniFile->WriteInteger("Setup", "FocusedPage", pagPages->ActivePageIndex);
  iniFile->WriteString("Setup", "SelectedNode",
    FExpandedNodeManager->ConvertNodeToPath(tvExpertInstallations->Selected));
}

/**

  This method starts the process of searching for registry installations of RAD Studio
  starting with Borland, Codegear and finally Embarcadero.

  @precon  None.
  @postcon Each of the three regsitry nodes is searched for expert installations.

**/
void __fastcall TfrmExpertManager::IterateExpertInstallations() {
  tvExpertInstallations->Items->BeginUpdate();
  try {
    const String strInstallationRoots[3] = { L"Borland", L"CodeGear", L"Embarcadero"};
    tvExpertInstallations->Items->Clear();
    for (auto strInstallation : strInstallationRoots) {
      TTreeNode* N = tvExpertInstallations->Items->AddChild(NULL, strInstallation.c_str());
      IterateSubInstallations(N, strInstallation);
      SetExpandedNodes(N);
    }
  } __finally {
    tvExpertInstallations->Items->EndUpdate();
  }
}

/**

  This method iterates the sub-keys of the company names searching for versions numbers. The default
  sub-key is BDS but other can be created using the -rXxxxxx command line option to the IDE which
  will create the Xxxxxx sub-key.

  @precon   Node must be a valid instance.
  @postcon  The sub keys of the company nodes are searched for versions numbers.

  @param    Node                as a TTreeNode
  @param    strRootInstallation as a String

**/
void __fastcall TfrmExpertManager::IterateSubInstallations(TTreeNode *Node,
  String strRootInstallation) {
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls("Software\\" + strRootInstallation + "\\") );
  std::unique_ptr<TStringList> sl( new TStringList() );
  iniFile->ReadSections(sl.get());
  TTreeNode *N = NULL;
  for (int i = 0; i < sl->Count; i++) {
    N = tvExpertInstallations->Items->AddChild(Node, sl->Strings[i]);
    IterateVersions(N, "Software\\" + strRootInstallation + "\\" + sl->Strings[i] + "\\");
    if (N->Count == 0)
      N->Delete();
  }
}

/**

  This method iterates the next level down looking for a decimal number denoting the
  version of RAD Studio and if found searches for expert installations.

  @precon  Node must be a valid instance.
  @postcon If a decimal number is found at the next level down the expert installations
           are iterated and checked and the nodes data value asigned an enumerate
           according to their state.

  @param   Node as a TTreeNode
  @param   strSubSection as a String

**/
void __fastcall TfrmExpertManager::IterateVersions(TTreeNode *Node, String strSubSection) {
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strSubSection) );
  std::unique_ptr<TStringList> sl( new TStringList() );
  iniFile->ReadSections(sl.get());
  TTreeNode *N = NULL;
  for (int i = 0; i < sl->Count; i++) {
    String strVersion = sl->Strings[i];
    std::wregex VersionNumPattern(L"\\d+.\\d");
    if (std::regex_match(strVersion.c_str(), VersionNumPattern)) {
      N = tvExpertInstallations->Items->AddChild(Node, strVersion);
      UpdateTreeViewStatus(N, false);
    }
  }
}

/**

  This method checks the given node for expert installations and returns an enumerate
  depending on their state. If any experts have invalid paths then evInvalidPaths. If any
  two or more experts have the same filename (not path) then the function returns
  evDuplicates.

  @precon  Node must be a valid instance.
  @postcon Returns the collective state of the experts.

  @param   Node as a TTreeNode
  @param   strSubSection as a String
  @return  a TExpertValidation

**/
TExpertValidation __fastcall TfrmExpertManager::CheckExperts(TTreeNode* Node, String strSubSection) {
  TExpertValidation iExpertValidation = evOkay;
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strSubSection) );
  std::unique_ptr<TStringList> sl( new TStringList() );
  iniFile->ReadSection(strKnownIDEPackages, sl.get());
  if (sl->Count > 0)
    iExpertValidation = evOkay;
  std::unique_ptr<TStringList> slDuplicates( new TStringList() );
  GetCurrentRADStudioMacros(strSubSection);
  //: @refactor Enabled Experts
  sl->Clear();
  iniFile->ReadSection(strExperts, sl.get());
  for (int i = 0; i < sl->Count; i++) {
    String strFullFileName = iniFile->ReadString(strExperts, sl->Strings[i], "");
    String strFileName = ExtractFileName(strFullFileName);
    if (!FileExists(ExpandRADStudioMacros(strFullFileName)) && iExpertValidation == evOkay)
      iExpertValidation = evInvalidPaths;
    if (slDuplicates->IndexOf(strFileName) != -1) {
      iExpertValidation = evDuplication;
      break;
    } else
      slDuplicates->Add(strFileName);
  }
  //: @refactor Disabled experts
  sl->Clear();
  iniFile->ReadSection(strDisabledExperts, sl.get());
  for (int i = 0; i < sl->Count; i++) {
    String strFullFileName = iniFile->ReadString(strDisabledExperts, sl->Strings[i], "");
    String strFileName = ExtractFileName(strFullFileName);
  }
  return iExpertValidation;
}

TExpertValidation __fastcall TfrmExpertManager::CheckPackages(TTreeNode* Node, String strSubSection,
  String strPackage) {
  TExpertValidation iPackageValidation = evOkay;
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strSubSection) );
  std::unique_ptr<TStringList> sl( new TStringList() );
  iniFile->ReadSection(strPackage, sl.get());
  if (sl->Count > 0)
    iPackageValidation = evOkay;
  std::unique_ptr<TStringList> slDuplicates( new TStringList() );
  GetCurrentRADStudioMacros(strSubSection);
  //: @refactor Enabled Packages
  sl->Clear();
  iniFile->ReadSection(strPackage, sl.get());
  for (int i = 0; i < sl->Count; i++) {
    String strFullFileName = sl->Strings[i];
    String strFileName = ExtractFileName(strFullFileName);
    String strName = iniFile->ReadString(strPackage, sl->Strings[i], "");
    if (strName.SubString(1, 2) != "__") {
      if (!FileExists(ExpandRADStudioMacros(strFullFileName)) && iPackageValidation == evOkay)
        iPackageValidation = evInvalidPaths;
      if (slDuplicates->IndexOf(strFileName) != -1) {
        iPackageValidation = evDuplication;
        break;
      } else
        slDuplicates->Add(strFileName);
    }
  }
  return iPackageValidation;
}

/**

  This is an on create event handler for the form.

  @precon  None.
  @postcon Loads the applications settings and intialises the expanded nodes manager.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::FormCreate(TObject *Sender) {
  pagPages->ActivePageIndex = 0;
  GetVersionAndBuild();
  FCurrentRADStudioMacros = std::unique_ptr<TStringList>( new TStringList() );
  LoadSettings();
  FExpandedNodeManager = std::unique_ptr<TExpandedNodeManager>( new TExpandedNodeManager() );
}

/**

  This is an on destroy event handler for the form.

  @precon  None.
  @postcon Gets all the expanded nodes and saves them in the expanded nodes manager, then
           frees the expanded node manager (it saves the settings to the registry) and
           saves the applications settings.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::FormDestroy(TObject *Sender) {
  TTreeNode* N = tvExpertInstallations->Items->GetFirstNode();
  while (N != NULL) {
    GetExpandedNodes(N);
    N = N->getNextSibling();
  }
  SaveSettings();
}

/**

  This is an on show event handler for the form.

  @precon  None.
  @postcon This is used in preference to OnFormCreate as the treeview renders quicker.
           Starts the iterations of all installations of RAD Studio.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::FormShow(TObject *Sender) {
  IterateExpertInstallations();
  SelectTreeViewNode(FSelectedNodePath);
}

/**

  This is an on Custom Draw Item event handler for the treeview.

  @precon  None.
  @postcon For the given node in the tree a search is undertaken to find the highest
           validation enumerate for all child nodes recursively and set the colour of the
           node accordingly. RED = Duplicates, GRAY = Invalid paths & BLUE is Okay.

  @param   Sender      as a TCustomTreeView
  @param   Node        as a TTreeNode
  @param   State       as a TCustomDrawState
  @param   Stage       as a TCustomDrawStage
  @param   PaintImage  as a bool as a Reference
  @param   DefaultDraw as a bool as a Reference

**/
void __fastcall TfrmExpertManager::tvExpertInstallationsAdvancedCustomDrawItem(TCustomTreeView *Sender,
          TTreeNode *Node, TCustomDrawState State, TCustomDrawStage Stage, bool &PaintImages,
          bool &DefaultDraw) {
  DefaultDraw = true;
  TExpertValidation iExpertValidation = GetHighestValidation(Node);
  switch (iExpertValidation) {
    case evNone:
      Sender->Canvas->Font->Color = clWindowText;
      break;
    case evOkay:
      Sender->Canvas->Font->Color = clBlue;
      break;
    case evInvalidPaths:
      Sender->Canvas->Font->Color = clGrayText;
      break;
    case evDuplication:
      Sender->Canvas->Font->Color = clRed;
      break;
  default:
    Sender->Canvas->Font->Color = clWindowText;
  }
}

/**

  This method iterates over all child nodes of the given node looking for the highest
  enumerate assigned to the nodes and returns the highest enumerate to the calling
  code.

  @precon  Node must be a valid instance.
  @postcon Iterates over all child nodes returning the highesy assigned enumerate.

  @param   Node as a TTreeNode
  @return  a TExpertValidation

**/
TExpertValidation  __fastcall TfrmExpertManager::GetHighestValidation(TTreeNode* Node) {
  int i = (int)Node->Data;
  TExpertValidation iReturn = (TExpertValidation)i;
  TTreeNode* N = Node->getFirstChild();
  while (N != NULL) {
    TExpertValidation iResult = GetHighestValidation(N);
    if (iResult > iReturn)
      iReturn = iResult;
    N = N->getNextSibling();
  }
  return iReturn;
}

/**

  This is an on change event handler for the tvExpertInstallations tree view.

  @precon  None.
  @postcon For a tree node with a TExpertValidation of evOkay, evInvalidPaths,
           evDuplication the list of installed experts for the installation are rendered
           in the listview with duplicates coloured in red.

  @param   Sender as a TObject
  @param   Node   as a TTreeNode

**/
void __fastcall TfrmExpertManager::tvExpertInstallationsChange(TObject *Sender,
  TTreeNode *Node) {
  FUpdatingListView = true;
  __try {
    ShowExperts(Node);
  } __finally {
    FUpdatingListView = false;
  }
}

void __fastcall TfrmExpertManager::ShowExperts(TTreeNode *Node) {
  lvInstalledExperts->Clear();
  if (Node) {
    int i = (int)Node->Data;
    TExpertValidation iExpertValidation = (TExpertValidation)i;
    TExpertValidations setExpertValidations =
      TExpertValidations() << evOkay << evInvalidPaths << evDuplication;
    if (setExpertValidations.Contains(iExpertValidation)) {
      String strSubSection = GetRegPathToNode(Node);
      GetCurrentRADStudioMacros(strSubSection);
      std::unique_ptr<TStringList> slDups( new TStringList() );
      AddExpertsToList(lvInstalledExperts, strSubSection, strExperts, true, slDups.get());
      AddExpertsToList(lvInstalledExperts, strSubSection, strDisabledExperts, false, slDups.get());
      AddPackagesToList(lvKnownIDEPackages, strSubSection, strKnownIDEPackages);
      AddPackagesToList(lvKnownPackages, strSubSection, strKnownPackages);
    }
  }
}

/**

  This function adds the experts found in the passed sub section into the listview.

  @precon  None.
  @postcon The experts i the passed subs ection are added to the list view.

  @param   strSubSection as a String
  @param   strKey        as a String
  @param   boolEnabled   as a bool

**/
void __fastcall TfrmExpertManager::AddExpertsToList(TListView* lvList, String strSubSection,
  String strKey, bool boolEnabled, TStringList* slDups) {
  // Get expert list
  std::unique_ptr<TRegistryINIFileCls> iniFile( new TRegistryINIFileCls(strSubSection) );
  std::unique_ptr<TStringList> slKeys( new TStringList() );
  iniFile->ReadSection(strKey, slKeys.get());
  std::unique_ptr<TStringList> slExperts( new TStringList() );
  for (int iKey = 0; iKey < slKeys->Count; iKey++) {
    String S = iniFile->ReadString(strKey, slKeys->Strings[iKey], "");
    slExperts->AddPair(slKeys->Strings[iKey], S);
  }
  // Render List
  lvList->Items->BeginUpdate();
  try {
    GetCurrentRADStudioMacros(strSubSection);
    for (int i = 0; i < slExperts->Count; i++) {
      TListItem* Item = lvList->Items->Add();
      String strFullFileName = slExperts->ValueFromIndex[i];
      String strFileName = ExtractFileName(strFullFileName);
      Item->Caption = slExperts->Names[i];
      Item->SubItems->Add(strFullFileName);
      Item->Checked = boolEnabled;
      int iIndex = slDups->IndexOf(strFileName);
      if (iIndex != -1) {
        Item->Data = (void*)evDuplication;
        int iOtherIndex = (int)slDups->Objects[iIndex]; //: @bug CANNOT GET INDEX WHEN SORTED
        if ((int)lvList->Items->Item[iOtherIndex]->Data == 0)
          lvList->Items->Item[iOtherIndex]->Data = (void*)evDuplication;
      }
      if (!FileExists(ExpandRADStudioMacros(strFullFileName)))
        Item->Data = (void*)evInvalidPaths;
      slDups->AddObject(strFileName, (TObject*)Item->Index); //: @bug CANNOT STORE INDEX ACROSS 2 SETS
    }
  } __finally {
    lvList->Items->EndUpdate();
  }
}

/**

  This function adds the packages found in the passed sub section into the listview.

  @precon  None.
  @postcon The experts i the passed subs ection are added to the list view.

  @param   strSubSection as a String
  @param   strKey        as a String
  @param   boolEnabled   as a bool

**/
void __fastcall TfrmExpertManager::AddPackagesToList(TListView* lvList, String strSubSection,
  String strKey) {
  TUPStrList slPackages( new TStringList() );
  GetPackageList(slPackages.get(), strSubSection, strKey);
  String strViewName = strSubSection + strKey;
  if (strKey == strKnownIDEPackages)
    RenderPackageList(lvList, slPackages.get(), FLastKnownIDEPackagesViewName, strViewName);
  else
    RenderPackageList(lvList, slPackages.get(), FLastKnownPackagesViewName, strViewName);
}

void __fastcall TfrmExpertManager::RenderPackageList(TListView* lvList, TStringList* slPackages,
  String &strLastViewName, const String strViewName) {
  TUPStrList slDups( new TStringList() );
  lvList->Items->BeginUpdate();
  try {
    int iSelected = -1;
    GetCurrentPosition(lvList, strLastViewName, strViewName, iSelected);
    // Render List
    lvList->Clear();
    for (int i = 0; i < slPackages->Count; i++) {
      TListItem* Item = lvList->Items->Add();
      String strFullFileName = slPackages->ValueFromIndex[i];
      String strFileName = ExtractFileName(strFullFileName);
      String strName = slPackages->Names[i];
      if (strName.SubString(1, 2) == "__")
        strName.Delete(1, 2);
      Item->Caption = strName;
      Item->SubItems->Add(strFullFileName);
      Item->Checked = (slPackages->Names[i].SubString(1, 2) != "__");
      // Update list item state
      int iIndex = slDups->IndexOf(strFileName);
      if (iIndex != -1) {
        Item->Data = (void*)evDuplication;
        int iOtherIndex = (int)slDups->Objects[iIndex]; //: @bug CANNOT GET INDEX WHEN SORTED
        if ((int)lvList->Items->Item[iOtherIndex]->Data == 0)
          lvList->Items->Item[iOtherIndex]->Data = (void*)evDuplication;
      }
      if (!FileExists(ExpandRADStudioMacros(strFullFileName)))
        Item->Data = (void*)evInvalidPaths;
      slDups->AddObject(strFileName, (TObject*)Item->Index); //: @bug CANNOT STORE INDEX ACROSS 2 SETS
    }
    SetCurrentPosition(lvList, iSelected);
  } __finally {
    lvList->Items->EndUpdate();
  }
}

/** ***************************** GOT HERE EDITING (GOING UP) ***************************** **/

/**

  This method gets the currently selected item.

  @precon  lvList must be a valid instance.
  @postcon The selected item retrieved.

  @param   lvList as a TListView
  @param   iSelected as an int as a reference

**/
void __fastcall TfrmExpertManager::GetCurrentPosition(TListView* lvList, String &strLastViewName,
  const String strViewName, int &iSelected) {
  if (strLastViewName.Compare(strViewName) == 0) {
    iSelected = lvList->ItemIndex;
  }
  strLastViewName = strViewName;
}

/**

  This method sets the current selected item.

  @precon  lvList must be a valid instance.
  @postcon The selected item is set and brought into view.

  @param   lvList as a TListView
  @param   iSelected as an int as a reference

**/
void __fastcall TfrmExpertManager::SetCurrentPosition(TListView* lvList, int &iSelected) {
  if (iSelected >= lvList->Items->Count)
    iSelected = lvList->Items->Count;
  lvList->ItemIndex = iSelected;
  if (iSelected > -1)
    lvList->Items->Item[iSelected]->MakeVisible(false);
}

/**

  This method extracts the packages from the given registry keys and puts them into the given string
  list with the filename as the key and the package name as the value.

  @precon  slPackages must be a valid instance.
  @postcon The slPackages string list is populated with the pacakges.

  @param   slPackages as a TStringList
  @param   strSubSection as a String as a constant
  @param   strKey as a String as a constant

**/
void __fastcall TfrmExpertManager::GetPackageList(TStringList* slPackages, const String strSubSection,
  const String strKey) {
  TUPIniFile iniFile( new TRegistryINIFileCls(strSubSection) );
  TUPStrList slKeys( new TStringList() );
  iniFile->ReadSection(strKey, slKeys.get());
  for (int iKey = 0; iKey < slKeys->Count - 1; iKey++) {
    String S = iniFile->ReadString(strKey, slKeys->Strings[iKey], "");
    slPackages->AddPair(S, slKeys->Strings[iKey]);
  }
}

/**

  This method returns the registry path to the given nodes installation (without the
  Expert bit).

  @precon  Node must be a valid instance.
  @postcon Returns the regsitry path to the RAD Studio installation.

  @param   Node as a TTreeNode.
  @return  a String

**/
String __fastcall TfrmExpertManager::GetRegPathToNode(TTreeNode* Node) {
  return
    "Software\\" +
    Node->Parent->Parent->Text + "\\" +
    Node->Parent->Text + "\\" +
    Node->Text + "\\";
}

/**

  This is an on custom draw item event handler for the tvInstalledExperts list view
  control.

  @precon  None.
  @postcon Any item with a data value > 0 (NUL) will be draw in red font.

  @param   Sender      as a TCustomListView
  @param   Item        as a TListItem
  @param   State       as a TCustomDrawState
  @param   Stage       as a TCustomDrawStage
  @param   DefaultDraw as a bool as a reference

**/
void __fastcall TfrmExpertManager::lvInstalledExpertsAdvancedCustomDrawItem(TCustomListView *Sender,
          TListItem *Item, TCustomDrawState State, TCustomDrawStage Stage, bool &DefaultDraw) {
  DefaultDraw = true;
  int i = (int)Item->Data;
  switch ((TExpertValidation)i) {
    case evInvalidPaths:
      Sender->Canvas->Font->Color = clGrayText;
      break;
    case evDuplication:
      Sender->Canvas->Font->Color = clRed;
      break;
  default:
    Sender->Canvas->Font->Color = clWindowText;
  }
}

/**

  This is an on double click event handler for the installations list view control.

  @precon  None.
  @postcon Invokes the editor in edit mode if an item is double click else invokes the
           editor in add mode if there is no selected item and the tree node is on a valid
           installation.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::lvInstalledExpertsDblClick(TObject *Sender) {
  if (IsViewableNode(tvExpertInstallations->Selected)) {
    if (lvInstalledExperts->Selected == NULL)
      actAddExpertExecute(Sender);
    else
      actEditExpertExecute(Sender);
  }
}

/**

  This method force the tree to update the currently selected node based on any changes
  to the experts listed (checks the registry).

  @precon  tvExpertInstallations->Selected must be a valid node.
  @postcon Updates the current treenodes enumerate valid and asks the tre view to repaint.

**/
void __fastcall TfrmExpertManager::UpdateTreeViewStatus(TTreeNode* Node, const bool boolShow) {
  TExpertValidation iExpertValidation = CheckExperts(Node, GetRegPathToNode(Node));
  TExpertValidation iKnownIDEPackageValidation = CheckPackages(Node, GetRegPathToNode(Node),
    strKnownIDEPackages);
  if (iKnownIDEPackageValidation > iExpertValidation)
    iExpertValidation = iKnownIDEPackageValidation;
  TExpertValidation iKnownPackageValidation = CheckPackages(Node, GetRegPathToNode(Node),
    strKnownPackages);
  if (iKnownPackageValidation > iExpertValidation)
    iExpertValidation = iKnownPackageValidation;
  Node->Data = (void*)iExpertValidation;
  if (boolShow) {
    tvExpertInstallations->Invalidate();
    tvExpertInstallationsChange(tvExpertInstallations, tvExpertInstallations->Selected);
  }
}

/**

  This is an on execute event handle for the AddExpert action.

  @precon  None.
  @postcon This method invokes the editor with null strings and if confirmed adds the new
           expert to the selected expert installation and asks the treeview to update.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::actAddExpertExecute(TObject *Sender) {
  FUpdatingListView = true;
  __try {
    String strExpertName = "";
    String strExpertFileName = "";
    if (TfrmExpertEditor::Execute(strExpertName, strExpertFileName, ExpandRADStudioMacros)) {
      TListItem* Item = lvInstalledExperts->Items->Add();
      Item->Caption = strExpertName;
      Item->SubItems->Add(strExpertFileName);
      String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
      TUPIniFile iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection) );
      iniFile->WriteString(strExperts, strExpertName, strExpertFileName);
      UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
    }
  } __finally {
    FUpdatingListView = false;
  }
}

/**

  This is an on execute event handler for the EditExpert action.

  @precon  None.
  @postcon This method gets the currently selected expert information and passes it to the
           editor for changes and if confirmed updates the expert installation and asks
           treeview and listview to update.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::actEditExpertExecute(TObject *Sender) {
  String strExpertName = lvInstalledExperts->Selected->Caption;
  String strOldExpertName = strExpertName;
  String strExpertFileName = lvInstalledExperts->Selected->SubItems->Strings[0];
  if (TfrmExpertEditor::Execute(strExpertName, strExpertFileName, ExpandRADStudioMacros)) {
    lvInstalledExperts->Selected->Caption = strExpertName;
    lvInstalledExperts->Selected->SubItems->Strings[0] = strExpertFileName;
    String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
    String strKey = lvInstalledExperts->Selected->Checked ? strExperts : strDisabledExperts ;
    TUPIniFile iniFile( new TRegistryINIFileCls(strRegSection + strKey) );
    iniFile->DeleteValue(strOldExpertName);
    iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection) );
    strKey = lvInstalledExperts->Selected->Checked ? strExperts : strDisabledExperts ;
    iniFile->WriteString(strKey, strExpertName, strExpertFileName);
    UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
  }
}

/**

  This is an on execute event handler for the DeleteExpert action.

  @precon  None.
  @postcon Deletes the selected expert from the registry and asks the treeview and
           listviews to update.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::actDeleteExpertExecute(TObject *Sender) {
  FUpdatingListView = true;
  __try {
    if (tvExpertInstallations->Selected != NULL && lvInstalledExperts->Selected != NULL) {
      String strExpertName = lvInstalledExperts->Selected->Caption;
      String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
      String strKey = lvInstalledExperts->Selected->Checked ? L"Experts\\" : L"Experts\\Disabled\\" ;
      TUPIniFile iniFile( new TRegistryINIFileCls(strRegSection + strKey) );
      iniFile->DeleteValue(strExpertName);
      UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
    }
  } __finally {
    FUpdatingListView = false;
  }
}

/**

  This is an on execute update event handler for the Edit and Delete Expert actions.

  @precon  None.
  @postcon if the sender is a TAction then enable or disable the action depending upon the
           whether the list view has a selected item.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::actActionExpertUpdate(TObject *Sender) {
  TAction* Action = dynamic_cast<TAction*>(Sender);
  if (Action != NULL)
    Action->Enabled = (lvInstalledExperts->Selected != NULL);
}

/**

  This is an on execute update event handler for the Add Expert action.

  @precon  None.
  @postcon Updates the enable / disable status of the add action based on whether the
           node is a node for a RAD Studio installation.

  @param   Sender as a TObject

**/
void __fastcall TfrmExpertManager::actAddExpertUpdate(TObject *Sender) {
  bool boolEnabled = false;
  TAction* Action = dynamic_cast<TAction*>(Sender);
  if (Action != NULL) {
    TTreeNode* Node = tvExpertInstallations->Selected;
    boolEnabled = IsViewableNode(Node);
  }
  Action->Enabled = boolEnabled;
}

/**

  This method returns return if the given node belongs to a RAD Studio installation else
  returns false.

  @precon  Node must be a valid instance.
  @postcon Returns return if the given node belongs to a RAD Studio installation else
           returns false.

  @param   Node as a TTreeNode
  @return  a bool

**/
bool __fastcall TfrmExpertManager::IsViewableNode(TTreeNode* Node) {
  if (Node != NULL) {
    int i = (int)Node->Data;
    TExpertValidation iExpertValidation = (TExpertValidation)i;
    TExpertValidations setExpertValidations = TExpertValidations() << evOkay << evInvalidPaths <<
      evDuplication;
    return setExpertValidations.Contains(iExpertValidation);
  } else
    return false;
}

/**

  This method iterates over all child nodes of the given node looking for them being
  expanded and adds or remove the node from the expanded node mamager accordingly.

  @precon  Node must be a valid instance.
  @postcon The status of all the node under the given node are stored in the expanded node
           maanger.

  @param   Node as a TTreeNode

**/
void __fastcall TfrmExpertManager::GetExpandedNodes(TTreeNode* Node) {
  if (Node->Expanded)
    FExpandedNodeManager->AddNode(Node);
  else
    FExpandedNodeManager->RemoveNode(Node);
  TTreeNode* N = Node->getFirstChild();
  while (N != NULL) {
    GetExpandedNodes(N);
    N = N->getNextSibling();
  }
}

/**

  This method iterates over all child nodes of the given node looking as to whether they
  are in expanded node mamager and if so expanding the node accordingly.

  @precon  Node must be a valid instance.
  @postcon The status of all the node expanded status under the given node are restored
           from the informatioh in the expanded node maanger.

  @param   Node as a TTreeNode

**/
void __fastcall TfrmExpertManager::SetExpandedNodes(TTreeNode* Node) {
  if (FExpandedNodeManager->IsExpanded(Node))
    Node->Expand(false);
  TTreeNode* N = Node->getFirstChild();
  while (N != NULL) {
    SetExpandedNodes(N);
    N = N->getNextSibling();
  }
}

/**

  This method loads the FCurrentRADStudioMacros string list with the environemnt variables
  in the given RAD Studio installation so that they can be used for expanded macros.

  @precon  None.
  @postcon The internal string list for environment variables is cleared and repopulated
           with enviroment variables from the given RAD Studio installation and the users
           additional variables.

  @param   strRegPathToRADStudioRoot as a String

**/
void __fastcall TfrmExpertManager::GetCurrentRADStudioMacros(String strRegPathToRADStudioRoot) {
  FCurrentRADStudioMacros->Clear();
  TUPIniFile iniFile( new TRegistryINIFileCls(strRegPathToRADStudioRoot) );
  // Create system wide enviroment variables
  String strRootDir = iniFile->ReadString("", "RootDir", "");
  AddRADStudioMacro("$(BDS)", strRootDir);
  AddRADStudioMacro("$(BCB)", strRootDir);
  AddRADStudioMacro("$(BDSBIN)", strRootDir + "\\Bin");
  AddRADStudioMacro("$(BDSINCLUDE)", strRootDir + "\\Include");
  AddRADStudioMacro("$(BDSLIB)", strRootDir + "\\Lib");
  AddRADStudioMacro("$(DELPHI)", strRootDir);
  // Create user wide environment variables
  TUPStrList slUserEn( new TStringList() );
  iniFile->ReadSection("Environment Variables", slUserEn.get());
  for (int i = 0; i < slUserEn->Count; i++)
    AddRADStudioMacro(
      slUserEn->Strings[i],
      iniFile->ReadString("Environment Variables", slUserEn->Strings[i], "")
    );
}

/**

  This method searches through the given filename for text matching any environment
  variables found the FCurrentRADStudioMacros string list and replaces them with
  the actual path.

  @precon  None.
  @postcon An macros which match environment variables are expanded.

  @param   strFullFileName as a String
  @return  a String

**/
String __fastcall TfrmExpertManager::ExpandRADStudioMacros(String strFullFileName) {
  String strExpandedFileName = strFullFileName;
  for (int i = 0; i < FCurrentRADStudioMacros->Count; i++) {
    if (strExpandedFileName.Pos(FCurrentRADStudioMacros->Names[i]) > 0) {
      strExpandedFileName = StringReplace(
        strExpandedFileName,
        FCurrentRADStudioMacros->Names[i],
        FCurrentRADStudioMacros->ValueFromIndex[i],
        TReplaceFlags() << rfReplaceAll
      );
    }
  }
  return strExpandedFileName;
}

/**

  This method attempts to add an enviroment variable to the string list.

  @precon  None.
  @postcon The enviroment variable is added if it does not exist else the existing
           variable is updated to the new value.

  @param   strMacro as a String
  @param   strValue as a String

**/
void __fastcall TfrmExpertManager::AddRADStudioMacro(String strMacro, String strValue) {
  int iIndex = FCurrentRADStudioMacros->IndexOf(strMacro);
  if (iIndex == -1)
    FCurrentRADStudioMacros->AddPair(strMacro, strValue);
  else
    FCurrentRADStudioMacros->ValueFromIndex[iIndex] = strValue;
}

/**

  This method updates the application caption to include the applications version number
  and build information.

  @precon  None.
  @postcon The applications caption is updated with version and build information.

  @refactor Refactor the proces of getting the version information from a filename into
            a library unit so that it can be used in other CPP project rather than the
            Pascal implementation in DGHLibrary.

**/
void __fastcall TfrmExpertManager::GetVersionAndBuild() {
  DWORD iVerHandle = NULL;
  String strFileName = ParamStr(0);
  DWORD iVerInfoSize = GetFileVersionInfoSize(strFileName.c_str(), &iVerHandle);
  if (iVerInfoSize) {
    LPSTR VerInfo = new char [iVerInfoSize];
    try {
      if (GetFileVersionInfo(strFileName.c_str(), iVerHandle, iVerInfoSize, VerInfo)) {
        unsigned int iVerValueSize = 0;
        LPBYTE lpBuffer = NULL;
        if (VerQueryValue(VerInfo, TEXT("\\"), (VOID FAR* FAR*)&lpBuffer, &iVerValueSize)) {
          if (iVerValueSize) {
            VS_FIXEDFILEINFO *VerValue = (VS_FIXEDFILEINFO *)lpBuffer;
            int iMajor = VerValue->dwFileVersionMS >> 16;
            int iMinor = VerValue->dwFileVersionMS & 0xFFFF;
            int iBugFix = VerValue->dwFileVersionLS >> 16;
            int iBuild = VerValue->dwFileVersionLS & 0xFFFF;
            String strBugFix = " abcedfghijklmnopqrstuvwxyz";
            Caption = Caption + Format(" %d.%d%s (Build %d.%d.%d.%d)",
              ARRAYOFCONST((iMajor, iMinor, strBugFix[iBugFix + 1], iMajor, iMinor, iBugFix, iBuild)));
          }
        }
      }
    } __finally {
      delete[] VerInfo;
    }
  } else
    ShowMessage("The executable """ + strFileName +
      """ does not contain any version information.");
}

/**

  This is an on execute event handler for the Exit action.

  @precon  None.
  @postcon Cloes the application.

  @param   Sender as a TObject.

**/
void __fastcall TfrmExpertManager::actFileExitExecute(TObject *Sender) {
  Close();
}

/**

  This is an item checked event handler for the experts listview control.

  @precon  None.
  @postcon If an item is checked the expert is copied to the Experts key and deleted from the
           Disabled or the erverse if unchecked.

  @param   Sender as a TObject
  @param   Item   as a TListItem

**/
void __fastcall TfrmExpertManager::lvInstalledExpertsItemChecked(TObject *Sender, TListItem *Item) {
  if (!FUpdatingListView) {
    String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
    String strExpertName = Item->Caption;
    String strExpertFileName = Item->SubItems->Strings[0];
    TUPIniFile iniFile;
    if (Item->Checked) {
      iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection) );
      iniFile->WriteString(strExperts, strExpertName, strExpertFileName);
      iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection +
        strDisabledExperts) );
      iniFile->DeleteValue(strExpertName);
    } else {
      iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection) );
      iniFile->WriteString(strDisabledExperts, strExpertName, strExpertFileName);
      iniFile = TUPIniFile( new TRegistryINIFileCls(strRegSection +
        strExperts) );
      iniFile->DeleteValue(strExpertName);
    }
    UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
  }
}

/**

  This is an item checked event handler for the Known IDE Packages listview control.

  @precon  None.
  @postcon If an item is checked the package has any double underscores removed from the package name
           else double underscore are added to the package name to disable the package.

  @param   Sender as a TObject
  @param   Item   as a TListItem

**/
void __fastcall TfrmExpertManager::lvKnownIDEPackagesItemChecked(TObject *Sender, TListItem *Item) {
  if (!FUpdatingListView) {
    lvKnownIDEPackages->ItemIndex =  Item->Index;
    String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
    String strExpertName = Item->Caption;
    String strExpertFileName = Item->SubItems->Strings[0];
    TUPIniFile iniFile( new TRegistryINIFileCls(strRegSection) );
    if (!Item->Checked)
      strExpertName = "__" + strExpertName;
    iniFile->WriteString(strKnownIDEPackages, strExpertFileName, strExpertName);
    UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
  }
}

/**

  This is an item checked event handler for the Known Packages listview control.

  @precon  None.
  @postcon If an item is checked the package has any double underscores removed from the package name
           else double underscore are added to the package name to disable the package.

  @param   Sender as a TObject
  @param   Item   as a TListItem

**/
void __fastcall TfrmExpertManager::lvKnownPackagesItemChecked(TObject *Sender, TListItem *Item) {
  if (!FUpdatingListView) {
    lvKnownPackages->ItemIndex =  Item->Index;
    String strRegSection = GetRegPathToNode(tvExpertInstallations->Selected);
    String strExpertName = Item->Caption;
    String strExpertFileName = Item->SubItems->Strings[0];
    TUPIniFile iniFile( new TRegistryINIFileCls(strRegSection) );
    if (!Item->Checked)
      strExpertName = "__" + strExpertName;
    iniFile->WriteString(strKnownPackages, strExpertFileName, strExpertName);
    UpdateTreeViewStatus(tvExpertInstallations->Selected, true);
  }
}

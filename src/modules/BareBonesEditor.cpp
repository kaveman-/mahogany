///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   modules/BareBonesEditor.cpp: implements minimal MessageEditor
// Purpose:     This editor will be there until some better native editor
//              is developed
// Author:      Robert Vazan
// Modified by:
// Created:     12.07.2003
// CVS-ID:      $Id$
// Copyright:   (c) 2003 Mahogany team
// Licence:     M license
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "Mpch.h"

#ifndef USE_PCH
   #include "Mcommon.h"
   
   #include <wx/textctrl.h>
   #include <wx/panel.h>
   #include <wx/button.h>
   #include <wx/sizer.h>
#endif // USE_PCH

#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>

#include "Composer.h"
#include "MessageEditor.h"

// ----------------------------------------------------------------------------
// options we use here
// ----------------------------------------------------------------------------

extern const MOption MP_WRAPMARGIN;
extern const MOption MP_REPLY_MSGPREFIX;

// ----------------------------------------------------------------------------
// persistent msgboxes we use here
// ----------------------------------------------------------------------------

extern const MPersMsgBox *M_MSGBOX_FORMAT_PARAGRAPH_BEFORE_EXIT;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// control ids
enum
{
   Button_FormatParagraph = 100,
   Button_FormatAll,
   Button_UnformatParagraph,
   Button_UnformatAll,
   ListCtrl_Attachments
};

// ----------------------------------------------------------------------------
// BareBonesEditor: minimalistic MessageEditor implementation
// ----------------------------------------------------------------------------

class FormattedParagraph;
class wxBareBonesEditorNotebook;

class BareBonesEditor : public MessageEditor
{
public:
   // default ctor
   BareBonesEditor();
   virtual ~BareBonesEditor();

   // accessors
   virtual wxWindow *GetWindow() const;
   virtual bool IsModified() const;
   virtual bool IsEmpty() const;
   virtual unsigned long ComputeHash() const;

   // creation
   virtual void Create(Composer *composer, wxWindow *parent);
   virtual void UpdateOptions();
   virtual bool FinishWork();

   // operations
   virtual void Clear();
   virtual void Enable(bool enable);
   virtual void ResetDirty();
   virtual void SetEncoding(wxFontEncoding encoding);
   virtual void Copy();
   virtual void Cut();
   virtual void Paste();

   virtual bool Print();
   virtual void PrintPreview();

   virtual void MoveCursorTo(unsigned long x, unsigned long y);
   virtual void MoveCursorBy(long x, long y);
   virtual void SetFocus();

   // content
   virtual void InsertAttachment(const wxBitmap& icon, EditorContentPart *mc);
   virtual void InsertText(const String& text, InsertMode insMode);
   virtual EditorContentPart *GetFirstPart();
   virtual EditorContentPart *GetNextPart();

   // for wxBareBonesTextControl only: we have to use
   bool OnFirstTimeFocus() { return MessageEditor::OnFirstTimeFocus(); }
   void OnFirstTimeModify() { MessageEditor::OnFirstTimeModify(); }

   friend class FormattedParagraph;
   friend class wxBareBonesEditorNotebook;
   
private:
   wxBareBonesEditorNotebook *m_notebook;
   wxTextCtrl *m_textControl;
   wxListCtrl *m_attachments;
   
   wxFont m_originalFont;
   bool m_originalFontValid;
   
   int m_getNextAttachement;
};

class FormattedParagraph
{
public:
   FormattedParagraph(wxTextCtrl *control,BareBonesEditor *editor);

   void FromCursor();
   void First();
   void Next();
   bool Empty() { return m_from == m_to; }
   void Format();
   void Unformat();
   bool NeedsFormat();
   void FormatAll();
   void UnformatAll();
   
private:
   bool IsWhiteLine(int line);
   int FindLineByWhite(int start,bool white);
   String Get();
   void Set(const String &modified);
   int LineToPosition(int line);
   bool IsQuoted();
   String UnformatCommon();
   String FormatCommon();
   int SizeWithoutNewline(const String &paragraph);
   int FindLineLength(
      const String &paragraph,int lineStart,int paragraphEnd) const;
   size_t FindLastSpace(const String &paragraph,size_t start) const;
   
   wxTextCtrl *m_control;
   int m_from;
   int m_to;
   int m_margin;
   String m_prefix;
};

class wxBareBonesEditorNotebook : public wxNotebook
{
public:
   wxBareBonesEditorNotebook(BareBonesEditor *editor,wxWindow *parent);
   
   wxTextCtrl *GetTextControl() const { return m_textControl; }
   wxListCtrl *GetList() const { return m_attachments; }

protected:
   void OnFormatParagraph(wxCommandEvent& event);
   void OnFormatAll(wxCommandEvent& event);
   void OnUnformatParagraph(wxCommandEvent& event);
   void OnUnformatAll(wxCommandEvent& event);
   void OnItemActivate(wxListEvent& event);
   
private:
   wxSizer *CreateButtonRow(wxWindow *parent);
   void CreateAttachmentPage();
   
   BareBonesEditor *m_editor;
   wxTextCtrl *m_textControl;
   wxListCtrl *m_attachments;

   DECLARE_EVENT_TABLE()
};

class wxBareBonesTextControl : public wxTextCtrl
{
public:
   wxBareBonesTextControl(BareBonesEditor *editor,wxWindow *parent);

protected:
   // event handlers
   void OnKeyDown(wxKeyEvent& event);
   void OnFocus(wxFocusEvent& event);

private:
   BareBonesEditor *m_editor;

   bool m_firstTimeModify;
   bool m_firstTimeFocus;

   DECLARE_EVENT_TABLE()
};

class wxBareBonesAttachments : public wxListView
{
public:
   wxBareBonesAttachments(wxWindow *parent)
      : wxListView(parent,ListCtrl_Attachments,wxDefaultPosition,
         wxDefaultSize,wxLC_ICON | wxLC_SINGLE_SEL | wxSUNKEN_BORDER)
      {}
   
protected:
   void OnKeyDown(wxKeyEvent& event);

private:
   DECLARE_EVENT_TABLE()
};

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_MESSAGE_EDITOR(BareBonesEditor,
                         _("Minimal message editor"),
                         "(c) 2003 Mahogany Team");

// ----------------------------------------------------------------------------
// FormattedParagraph
// ----------------------------------------------------------------------------

FormattedParagraph::FormattedParagraph(wxTextCtrl *control,
   BareBonesEditor *editor)
   : m_control(control), m_from(0), m_to(0)
{
   m_margin = READ_CONFIG(editor->GetProfile(),MP_WRAPMARGIN);
   if(m_margin <= 0)
      m_margin = 1;
   m_prefix = READ_CONFIG_TEXT(editor->GetProfile(),MP_REPLY_MSGPREFIX);
}

void FormattedParagraph::FromCursor()
{
   long throwAwayX;
   long currentLine;
   m_control->PositionToXY(m_control->GetInsertionPoint(),
      &throwAwayX,&currentLine);
   
   if(IsWhiteLine(currentLine) && !(currentLine > 0
      && !IsWhiteLine(currentLine-1)))
   {
      m_from = m_to = currentLine;
   }
   else
   {
      int upNonWhite;
      for(upNonWhite = currentLine;
         upNonWhite > 0 && !IsWhiteLine(upNonWhite-1); --upNonWhite)
      {
      }
      m_from = upNonWhite;
      m_to = FindLineByWhite(m_from+1,true);
   }
}

bool FormattedParagraph::IsWhiteLine(int line)
{
   wxString contentWx(m_control->GetLineText(line));
   String content(contentWx.c_str());
   return content.find_first_not_of(" \n") == content.npos;
}

int FormattedParagraph::FindLineByWhite(int start,bool white)
{
   int lineCount = m_control->GetNumberOfLines();
   int result;
   for(result = start; result < lineCount && IsWhiteLine(result) != white;
      ++result)
   {
   }
   return result;
}

void FormattedParagraph::First()
{
   m_from = FindLineByWhite(0,false);
   m_to = FindLineByWhite(m_from,true);
}

void FormattedParagraph::Next()
{
   m_from = FindLineByWhite(m_to,false);
   m_to = FindLineByWhite(m_from,true);
}

void FormattedParagraph::Format()
{
   if(!Empty() && !IsQuoted())
      Set(FormatCommon());
}

void FormattedParagraph::Unformat()
{
   if(!Empty() && !IsQuoted())
      Set(UnformatCommon());
}

String FormattedParagraph::Get()
{
   return m_control->GetRange(LineToPosition(m_from),LineToPosition(m_to));
}

void FormattedParagraph::Set(const String &modified)
{
   m_control->Replace(LineToPosition(m_from),LineToPosition(m_to),modified);
   int lineCount = 0;
   size_t newline;
   for(newline = modified.find_first_of('\n'); newline != modified.npos;
      newline = modified.find_first_of('\n',newline+1))
   {
      ++lineCount;
   }
   if(!modified.empty() && modified[modified.size()-1] != '\n')
      ++lineCount;
   m_to = m_from+lineCount;
}

int FormattedParagraph::LineToPosition(int line)
{
   if(line >= m_control->GetNumberOfLines())
      return m_control->GetLastPosition();
   return m_control->XYToPosition(0,line);
}

bool FormattedParagraph::IsQuoted()
{
   int line;
   for(line = m_from; line < m_to; ++line)
   {
      String content(m_control->GetLineText(line));
      int offset;
      for(offset = 0; content.size()-offset >= m_prefix.size()
         && (!offset || isupper(content[(size_t)(offset-1)])); ++offset)
      {
         if(content.compare(offset,m_prefix.size(),m_prefix) == 0)
            return true;
      }
   }
   return false;
}

String FormattedParagraph::UnformatCommon()
{
   String modified(Get());
   size_t newline;
   for(newline = modified.find_first_of('\n');
      newline != modified.npos && newline != modified.size()-1;
      newline = modified.find_first_of('\n',newline+1))
   {
      modified[newline] = ' ';
   }
   return modified;
}

bool FormattedParagraph::NeedsFormat()
{
   return !Empty() && !IsQuoted() && Get() != FormatCommon();
}

void FormattedParagraph::FormatAll()
{
   for(First(); !Empty(); Next())
      Format();
}

void FormattedParagraph::UnformatAll()
{
   for(First(); !Empty(); Next())
      Unformat();
}

String FormattedParagraph::FormatCommon()
{
   String modified;
   int lineStart;
   int lineLength;
   
   const String old(UnformatCommon());
   const int contentSize = SizeWithoutNewline(old);
   
   for(lineStart = 0; contentSize-lineStart > m_margin;
      lineStart += lineLength+1)
   {
      lineLength = FindLineLength(old,lineStart,contentSize);
      modified.append(old,lineStart,lineLength);
      modified.append(1,'\n');
   }
   
   if(lineStart < contentSize)
      modified.append(old,lineStart,old.size()-lineStart);
   
   return modified;
}

int FormattedParagraph::SizeWithoutNewline(const String &paragraph)
{
   int contentSize;
   
   if(!paragraph.empty() && paragraph[paragraph.size()-1] == '\n')
      contentSize = paragraph.size()-1;
   else
      contentSize = paragraph.size();
      
   return contentSize;
}

int FormattedParagraph::FindLineLength(
   const String &paragraph,int lineStart,int paragraphEnd) const
{
   int lineLength;
   
   size_t findSpace = FindLastSpace(paragraph,lineStart+m_margin);
   if(findSpace != paragraph.npos && (int)findSpace >= lineStart)
   {
      size_t findNonSpace = paragraph.find_last_not_of(' ',findSpace);
      if(findNonSpace != paragraph.npos && (int)findNonSpace >= lineStart)
         lineLength = findNonSpace-lineStart+1;
      else
         lineLength = m_margin;
   }
   else
   {
      size_t wordEnd = paragraph.find_first_of(' ',lineStart+m_margin);
      if(wordEnd != paragraph.npos)
         lineLength = wordEnd-lineStart;
      else
         lineLength = paragraphEnd-lineStart;
   }
   
   return lineLength;
}

size_t FormattedParagraph::FindLastSpace(
   const String &paragraph,size_t start) const
{
#if wxCHECK_VERSION(2,5,0) // Bug in wxWindows implementation of find_last_of
   size_t findSpace = paragraph.find_last_of(' ',start);
#else
   size_t findSpace = paragraph.npos;
   int tryFindSpace;
   
   for( tryFindSpace = start-1; tryFindSpace >= 0; --tryFindSpace )
   {
      if( paragraph[tryFindSpace] == ' ' )
      {
         findSpace = (size_t)tryFindSpace;
         break;
      }
   }
#endif

   return findSpace;
}

// ----------------------------------------------------------------------------
// wxBareBonesEditorNotebook
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxBareBonesEditorNotebook, wxNotebook)
   EVT_BUTTON(Button_FormatParagraph,
      wxBareBonesEditorNotebook::OnFormatParagraph)
   EVT_BUTTON(Button_FormatAll,
      wxBareBonesEditorNotebook::OnFormatAll)
   EVT_BUTTON(Button_UnformatParagraph,
      wxBareBonesEditorNotebook::OnUnformatParagraph)
   EVT_BUTTON(Button_UnformatAll,
      wxBareBonesEditorNotebook::OnUnformatAll)
   EVT_LIST_ITEM_ACTIVATED(ListCtrl_Attachments,
      wxBareBonesEditorNotebook::OnItemActivate)
END_EVENT_TABLE()

wxBareBonesEditorNotebook::wxBareBonesEditorNotebook(
   BareBonesEditor *editor,wxWindow *parent)
   : wxNotebook(parent,-1), m_editor(editor)
{
   wxPanel *body = new wxPanel(this);
   AddPage(body,_T("Message Body"));
   
   wxSizer *textColumn = new wxBoxSizer(wxVERTICAL);
   body->SetSizer(textColumn);
   
   m_textControl = new wxBareBonesTextControl(editor,body);
   textColumn->Add(m_textControl,1,wxEXPAND);
   
   textColumn->Add(CreateButtonRow(body),0,wxALL | wxALIGN_LEFT,10);

   CreateAttachmentPage();
}

wxSizer *wxBareBonesEditorNotebook::CreateButtonRow(wxWindow *parent)
{
   wxSizer *buttonRow = new wxGridSizer(1,4,0,10);
   
   buttonRow->Add(new wxButton(parent,Button_FormatParagraph,
      _T("Format Paragraph")),0,wxEXPAND);
   buttonRow->Add(new wxButton(parent,Button_FormatAll,
      _T("Format All")),0,wxEXPAND);
   buttonRow->Add(new wxButton(parent,Button_UnformatParagraph,
      _T("Unformat Paragraph")),0,wxEXPAND);
   buttonRow->Add(new wxButton(parent,Button_UnformatAll,
      _T("Unformat All")),0,wxEXPAND);
      
   return buttonRow;
}

void wxBareBonesEditorNotebook::CreateAttachmentPage()
{
   wxPanel *files = new wxPanel(this);
   AddPage(files,_T("Attachments"));
   
   wxBoxSizer *fileSizer = new wxBoxSizer(wxVERTICAL);
   files->SetSizer(fileSizer);
   
   m_attachments = new wxBareBonesAttachments(files);
   fileSizer->Add(m_attachments,1,wxEXPAND);
   
   m_attachments->AssignImageList(
      new wxImageList(32,32),wxIMAGE_LIST_NORMAL);
}

void wxBareBonesEditorNotebook::OnFormatParagraph(wxCommandEvent& event)
{
   FormattedParagraph paragraph(m_textControl,m_editor);
   
   paragraph.FromCursor();
   paragraph.Format();
   
   event.Skip();
}

void wxBareBonesEditorNotebook::OnFormatAll(wxCommandEvent& event)
{
   FormattedParagraph paragraph(m_textControl,m_editor);
   paragraph.FormatAll();
   event.Skip();
}

void wxBareBonesEditorNotebook::OnUnformatParagraph(wxCommandEvent& event)
{
   FormattedParagraph paragraph(m_textControl,m_editor);
   
   paragraph.FromCursor();
   paragraph.Unformat();
   
   event.Skip();
}

void wxBareBonesEditorNotebook::OnUnformatAll(wxCommandEvent& event)
{
   FormattedParagraph paragraph(m_textControl,m_editor);
   paragraph.UnformatAll();
   event.Skip();
}

void wxBareBonesEditorNotebook::OnItemActivate(wxListEvent& event)
{
   m_editor->EditAttachmentProperties((EditorContentPart *)event.GetData());
}

// ----------------------------------------------------------------------------
// wxBareBonesAttachments
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxBareBonesAttachments, wxListView)
   EVT_KEY_DOWN(wxBareBonesAttachments::OnKeyDown)
END_EVENT_TABLE()

void wxBareBonesAttachments::OnKeyDown(wxKeyEvent& event)
{
   if( event.GetKeyCode() == WXK_DELETE )
   {
      int selected = GetFirstSelected();
      if( selected >= 0 )
      {
         wxListItem item;
         item.SetId(selected);
         GetItem(item);
         
         EditorContentPart *part = (EditorContentPart *)item.GetData();
         
         DeleteItem(selected);
         
         part->DecRef();
      }
   }
   
   event.Skip();
}

// ----------------------------------------------------------------------------
// wxBareBonesTextControl
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxBareBonesTextControl, wxTextCtrl)
   EVT_KEY_DOWN(wxBareBonesTextControl::OnKeyDown)
   EVT_SET_FOCUS(wxBareBonesTextControl::OnFocus)
END_EVENT_TABLE()

wxBareBonesTextControl::wxBareBonesTextControl(BareBonesEditor *editor,
                                               wxWindow *parent)
                      : wxTextCtrl(parent,-1,"",
                        wxDefaultPosition,wxDefaultSize,wxTE_MULTILINE)
{
   m_editor = editor;

   m_firstTimeModify = TRUE;
   m_firstTimeFocus = TRUE;
}

void wxBareBonesTextControl::OnKeyDown(wxKeyEvent& event)
{
   if ( m_firstTimeModify )
   {
      m_firstTimeModify = FALSE;

      m_editor->OnFirstTimeModify();
   }

   event.Skip();
}

void wxBareBonesTextControl::OnFocus(wxFocusEvent& event)
{
   if ( m_firstTimeFocus )
   {
      m_firstTimeFocus = FALSE;

      if ( m_editor->OnFirstTimeFocus() )
      {
         // composer doesn't need first modification notification any more
         // because it modified the text itself
         m_firstTimeModify = FALSE;
      }
   }

   event.Skip();
}

// ----------------------------------------------------------------------------
// BareBonesEditor ctor/dtor
// ----------------------------------------------------------------------------

BareBonesEditor::BareBonesEditor()
{
   m_textControl = NULL;
   m_originalFontValid = false;
   m_getNextAttachement = -1;
}

BareBonesEditor::~BareBonesEditor()
{
   int itemIndex;
   
   // DecRef all EditorContentPart objects in wxListCtrl, that's all
   for(itemIndex = 0; itemIndex < m_attachments->GetItemCount(); ++itemIndex)
   {
      wxListItem itemProperties;
      
      itemProperties.SetId(itemIndex);
      m_attachments->GetItem(itemProperties);
      
      EditorContentPart *file
         = (EditorContentPart *)itemProperties.GetData();
      
      itemProperties.SetData((void *)NULL);
#if !wxCHECK_VERSION(2,5,0)
      itemProperties.SetMask(itemProperties.GetMask() & ~wxLIST_MASK_DATA);
#endif
      m_attachments->SetItem(itemProperties);
      
      file->DecRef();
   }
}

void BareBonesEditor::Create(Composer *composer, wxWindow *parent)
{
   // save it to be able to call GetProfile() and GetOptions()
   m_composer = composer;

   m_notebook = new wxBareBonesEditorNotebook(this,parent);
   
   m_textControl = m_notebook->GetTextControl();
   m_attachments = m_notebook->GetList();
   
   Enable(true);
   Clear();
}

void BareBonesEditor::UpdateOptions()
{
   // nothing to do here so far as it's not called anyway
}

bool BareBonesEditor::FinishWork()
{
   FormattedParagraph paragraph(m_textControl,this);
   
   bool needsFormat = false;
   for(paragraph.First(); !paragraph.Empty(); paragraph.Next())
      needsFormat = needsFormat || paragraph.NeedsFormat();
   
   if(needsFormat)
   {
      switch
      (
         MDialog_YesNoCancel
         (
            _("Would you like to format all paragraphs first?"),
            m_notebook,
            MDIALOG_YESNOTITLE,
            M_DLG_YES_DEFAULT,
            M_MSGBOX_FORMAT_PARAGRAPH_BEFORE_EXIT
         )
      )
      {
         case MDlg_Yes:
            paragraph.FormatAll();
            return false;
         case MDlg_No:
            break;
         case MDlg_Cancel:
            return false;
      }
   }
   
   return true;
}

// ----------------------------------------------------------------------------
// BareBonesEditor accessors
// ----------------------------------------------------------------------------

wxWindow *BareBonesEditor::GetWindow() const
{
   return m_notebook;
}

bool BareBonesEditor::IsModified() const
{
   return m_textControl->IsModified();
}

bool BareBonesEditor::IsEmpty() const
{
   return m_textControl->GetLastPosition() == 0;
}

unsigned long BareBonesEditor::ComputeHash() const
{
   // TODO: our hash is quite lame actually - it is just the text length!
   return m_textControl->GetLastPosition();
}

// ----------------------------------------------------------------------------
// BareBonesEditor misc operations
// ----------------------------------------------------------------------------

void BareBonesEditor::Clear()
{
   m_textControl->Clear();
   m_textControl->DiscardEdits();
}

void BareBonesEditor::Enable(bool enable)
{
   m_textControl->SetEditable(enable);
}

void BareBonesEditor::ResetDirty()
{
   m_textControl->DiscardEdits();
}

void BareBonesEditor::SetEncoding(wxFontEncoding encoding)
{
   if(!m_originalFontValid)
   {
      m_originalFont = m_textControl->GetFont();
      m_originalFontValid = true;
   }
   
   wxFont next(
      m_originalFont.GetPointSize(),
      m_originalFont.GetFamily(),
      wxNORMAL, // Italic off
      wxNORMAL, // Bold off
      false, // Underline off
      m_originalFont.GetFaceName(),
      encoding
   );
   
   m_textControl->SetFont(next);
}

void BareBonesEditor::MoveCursorTo(unsigned long x, unsigned long y)
{
   long correctedY = y;
   if(correctedY < 0)
      correctedY = 0;
   long countY = m_textControl->GetNumberOfLines();
   if(countY > 0 && correctedY >= countY)
      correctedY = countY - 1;
   
   long correctedX = x;
   if(correctedX < 0)
      correctedX = 0;
   long countX = m_textControl->GetLineLength(correctedY);
   if(countX >= 0 && correctedX > countX)
      correctedX = countX;
   
   long position = m_textControl->XYToPosition(correctedX,correctedY);
   m_textControl->SetInsertionPoint(position);
   m_textControl->ShowPosition(position);
}

void BareBonesEditor::MoveCursorBy(long x, long y)
{
   long oldX;
   long oldY;
   m_textControl->PositionToXY(m_textControl->GetInsertionPoint(),
      &oldX,&oldY);
   MoveCursorTo(oldX+x,oldY+y);
}

void BareBonesEditor::SetFocus()
{
   m_textControl->SetFocus();
}

// ----------------------------------------------------------------------------
// BareBonesEditor clipboard operations
// ----------------------------------------------------------------------------

void BareBonesEditor::Copy()
{
   m_textControl->Copy();
}

void BareBonesEditor::Cut()
{
   m_textControl->Cut();
}

void BareBonesEditor::Paste()
{
   m_textControl->Paste();
}

// ----------------------------------------------------------------------------
// BareBonesEditor printing
// ----------------------------------------------------------------------------

bool BareBonesEditor::Print()
{
   return false;
}

void BareBonesEditor::PrintPreview()
{
}

// ----------------------------------------------------------------------------
// BareBonesEditor contents: adding data/text
// ----------------------------------------------------------------------------

void BareBonesEditor::InsertAttachment(
   const wxBitmap& icon, EditorContentPart *mc)
{
   wxListItem item;
   
   item.SetData(mc);
   item.SetText(mc->GetName());
   item.SetId(m_attachments->GetItemCount());
   item.SetImage(
      m_attachments->GetImageList(wxIMAGE_LIST_NORMAL)->Add(icon));
#if !wxCHECK_VERSION(2,5,0)
   item.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE | wxLIST_MASK_DATA);
#endif
   
   m_attachments->InsertItem(item);
}

void BareBonesEditor::InsertText(const String& text, InsertMode insMode)
{
   if(insMode == Insert_Replace)
      m_textControl->Clear();
   m_textControl->Freeze();
   m_textControl->WriteText(text);
   m_textControl->Thaw();
}

// ----------------------------------------------------------------------------
// BareBonesEditor contents: enumerating the different parts
// ----------------------------------------------------------------------------

EditorContentPart *BareBonesEditor::GetFirstPart()
{
   m_getNextAttachement = 0;
   return new EditorContentPart(m_textControl->GetValue());
}

EditorContentPart *BareBonesEditor::GetNextPart()
{
   if(m_getNextAttachement < 0)
      return NULL;

   if(m_getNextAttachement >= m_attachments->GetItemCount())
   {
      m_getNextAttachement = -1;
      return NULL;
   }
   
   wxListItem item;
   item.SetId(m_getNextAttachement);
   m_attachments->GetItem(item);
   
   EditorContentPart *file = (EditorContentPart *)item.GetData();
   file->IncRef();
   
   ++m_getNextAttachement;
   
   return file;
}
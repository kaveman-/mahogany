///////////////////////////////////////////////////////////////////////////////
// Project:     M - cross platform e-mail GUI client
// File name:   modules/TextViewer.cpp: implements text-only MessageViewer
// Purpose:     this is a wxTextCtrl-based implementation of MessageViewer
// Author:      Vadim Zeitlin
// Modified by:
// Created:     26.07.01
// CVS-ID:      $Id$
// Copyright:   (c) 2001 Vadim Zeitlin
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
#   include "Mcommon.h"
#   include "guidef.h"                  // for GetFrame
#   include "gui/wxMApp.h"              // for wxMApp

#   include <wx/textctrl.h>
#endif // USE_PCH

#include "gui/wxMenuDefs.h"
#include "MessageViewer.h"
#include "ClickURL.h"
#include "MTextStyle.h"

#include <wx/textbuf.h>

#include <wx/html/htmprint.h>   // for wxHtmlEasyPrinting

// only Win32 supports URLs in the text control natively so far, define this to
// use this possibility
//
// don't define it any longer because we do it now better than the native
// control
//#define USE_AUTO_URL_DETECTION

class TextViewerWindow;

#if wxUSE_PRINTING_ARCHITECTURE

// ----------------------------------------------------------------------------
// wxTextEasyPrinting: easy way to print text (TODO: move to wxWindows)
// ----------------------------------------------------------------------------

class wxTextEasyPrinting : public wxHtmlEasyPrinting
{
public:
   wxTextEasyPrinting(const wxString& name, wxWindow *parent = NULL)
      : wxHtmlEasyPrinting(name, GetFrame(parent)) { }

   bool Print(wxTextCtrl *text) { return PrintText(ControlToHtml(text)); }
   bool Preview(wxTextCtrl *text) { return PreviewText(ControlToHtml(text)); }

private:
   static wxString ControlToHtml(wxTextCtrl *text);
};

#endif // wxUSE_PRINTING_ARCHITECTURE

// ----------------------------------------------------------------------------
// TextViewer: a wxTextCtrl-based MessageViewer implementation
// ----------------------------------------------------------------------------

class TextViewer : public MessageViewer
{
public:
   // default ctor
   TextViewer();

   // creation &c
   virtual void Create(MessageView *msgView, wxWindow *parent);
   virtual void Clear();
   virtual void Update();
   virtual void UpdateOptions();
   virtual wxWindow *GetWindow() const;

   // operations
   virtual bool Find(const String& text);
   virtual bool FindAgain();
   virtual void SelectAll();
   virtual String GetSelection() const;
   virtual void Copy();
   virtual bool Print();
   virtual void PrintPreview();

   // header showing
   virtual void StartHeaders();
   virtual void ShowRawHeaders(const String& header);
   virtual void ShowHeaderName(const String& name);
   virtual void ShowHeaderValue(const String& value,
                                wxFontEncoding encoding);
   virtual void ShowHeaderURL(const String& text,
                              const String& url);
   virtual void EndHeader();
   virtual void ShowXFace(const wxBitmap& bitmap);
   virtual void EndHeaders();

   // body showing
   virtual void StartBody();
   virtual void StartPart();
   virtual void InsertAttachment(const wxBitmap& icon, ClickableInfo *ci);
   virtual void InsertClickable(const wxBitmap& icon,
                                ClickableInfo *ci,
                                const wxColour& col);
   virtual void InsertImage(const wxImage& image, ClickableInfo *ci);
   virtual void InsertRawContents(const String& data);
   virtual void InsertText(const String& text, const MTextStyle& style);
   virtual void InsertURL(const String& text, const String& url);
   virtual void EndPart();
   virtual void EndBody();

   // scrolling
   virtual bool LineDown();
   virtual bool LineUp();
   virtual bool PageDown();
   virtual bool PageUp();

   // capabilities querying
   virtual bool CanInlineImages() const;
   virtual bool CanProcess(const String& mimetype) const;

private:
   // create m_printText if necessary
   void InitPrinting();

   // flush the contents of m_textToAppend if it is not empty
   void FlushText();


   // the viewer window
   TextViewerWindow *m_window;

   // the position of the last match used by Find() and FindAgain()
   long m_posFind;

   // the text we're searched for the last time
   wxString m_textFind;

   // calling AppendText() each time is too slow, so we collect all text with
   // the same style in this variable and then FlushText() it all at once
   wxString m_textToAppend;

#if wxUSE_PRINTING_ARCHITECTURE
   // the object which does the printing
   wxTextEasyPrinting *m_printText;
#endif // wxUSE_PRINTING_ARCHITECTURE

   DECLARE_MESSAGE_VIEWER()
};

// ----------------------------------------------------------------------------
// ClickableFace: a clickable object representing a face picture
// ----------------------------------------------------------------------------

class ClickableFace : public ClickableInfo
{
public:
   ClickableFace(MessageView *msgView, wxBitmap face)
      : ClickableInfo(msgView),
        m_face(face)
   {
   }

   virtual String GetLabel() const { return "Face picture"; }

   virtual void OnLeftClick() const { DoShow(); }
   virtual void OnRightClick(const wxPoint& /* pt */) const { }

private:
   class FaceWindow : public wxDialog
   {
   public:
      FaceWindow(wxWindow *parent, const wxBitmap& bmp)
         : wxDialog(parent, wxID_ANY, _("Face picture"),
                    wxDefaultPosition, wxDefaultSize,
                    wxCAPTION | wxCLOSE_BOX),
           m_bmp(bmp)
      {
         Connect(wxEVT_PAINT, wxPaintEventHandler(FaceWindow::OnPaint));

         SetClientSize(bmp.GetWidth(), bmp.GetHeight());
      }

   private:
      void OnPaint(wxPaintEvent& /* event */)
      {
         wxPaintDC dc(this);
         dc.DrawBitmap(m_bmp, 0, 0);
      }

      wxBitmap m_bmp;
   };

   void DoShow() const
   {
      FaceWindow dlg(GetMessageView()->GetWindow(), m_face);
      dlg.ShowModal();
   }

   wxBitmap m_face;
};

// ----------------------------------------------------------------------------
// TextViewerClickable: an object we can click on in the text control
// ----------------------------------------------------------------------------

class TextViewerClickable
{
public:
   TextViewerClickable(ClickableInfo *ci, long pos, size_t len)
   {
      m_start = pos;
      m_len = len;
      m_ci = ci;
   }

   ~TextViewerClickable() { delete m_ci; }

   // accessors

   bool Inside(long pos) const
      { return pos >= m_start && pos - m_start < m_len; }

   const ClickableInfo *GetClickableInfo() const { return m_ci; }

private:
   // the range of the text we correspond to
   long m_start,
        m_len;

   ClickableInfo *m_ci;
};

WX_DEFINE_ARRAY(TextViewerClickable *, ArrayClickables);

// ----------------------------------------------------------------------------
// TextViewerWindow: the viewer window used by TextViewer
// ----------------------------------------------------------------------------

class TextViewerWindow : public wxTextCtrl
{
public:
   TextViewerWindow(TextViewer *viewer, wxWindow *parent);
   virtual ~TextViewerWindow();

   void InsertClickable(const wxString& text,
                        ClickableInfo *ci,
                        const wxColour& col = wxNullColour);

   // override some base class virtuals
   virtual void Clear();
   virtual bool AcceptsFocusFromKeyboard() const { return FALSE; }

private:
#ifdef USE_AUTO_URL_DETECTION
   void OnLinkEvent(wxTextUrlEvent& event);
#endif // USE_AUTO_URL_DETECTION

   // the generic mouse event handler for right/left/double clicks
   void OnMouseEvent(wxMouseEvent& event);

   // process the mouse click at the given text position
   bool ProcessMouseEvent(const wxMouseEvent& event, long pos);

   TextViewer *m_viewer;

   // all "active" objects
   ArrayClickables m_clickables;

   DECLARE_EVENT_TABLE()
   DECLARE_NO_COPY_CLASS(TextViewerWindow)
};

#if wxUSE_PRINTING_ARCHITECTURE

// ============================================================================
// wxTextEasyPrinting implementation
// ============================================================================

/* static */
wxString wxTextEasyPrinting::ControlToHtml(wxTextCtrl *text)
{
   wxCHECK_MSG( text, wxEmptyString, _T("NULL control in wxTextEasyPrinting") );

   const long posEnd = text->GetLastPosition();

   wxString s;
   s.reserve(posEnd + 100);

   s << _T("<html><body><tt>");

   wxString ch;
   wxTextAttr attr, attrNew;
   for ( long pos = 0; pos < posEnd; pos++ )
   {
      ch = text->GetRange(pos, pos + 1);

      // this is not implemented in wxGTK and doesn't want to work under MSW
      // for some reason (TODO: debug it)
#if 0
      if ( text->GetStyle(pos, attrNew) )
      {
         bool changed = false;
         if ( attrNew.GetTextColour() != attr.GetTextColour() )
         {
            if ( attrNew.HasTextColour() )
            {
               s << _T("<font color=\"#")
                 << Col2Html(attrNew.GetTextColour())
                 << _T("\">");
            }
            else
            {
               s << _T("</font>");
            }

            changed = true;
         }

         if ( changed )
         {
            attr = attrNew;
         }
      }
#endif // 0

      switch ( (wxChar)ch[0u] )
      {
         case '<':
            s += _T("&lt;");
            break;

         case '>':
            s += _T("&gt;");
            break;

         case '&':
            s += _T("&amp;");
            break;

         case '"':
            s += _T("&quot;");
            break;

         case '\t':
            // we hardcode the tab width to 8 spaces
            s += _T("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
            break;

         case '\r':
            // ignore, we process '\n' below
            break;

         case '\n':
            s += _T("<br>\n");
            break;

         case ' ':
            s += _T("&nbsp;");
            break;

         default:
            s += ch[0u];
      }
   }

   s << _T("</tt></body></html>");

   return s;
}

#endif // wxUSE_PRINTING_ARCHITECTURE

// ============================================================================
// TextViewerWindow implementation
// ============================================================================

BEGIN_EVENT_TABLE(TextViewerWindow, wxTextCtrl)
#ifdef USE_AUTO_URL_DETECTION
   EVT_TEXT_URL(-1, TextViewerWindow::OnLinkEvent)
#endif // USE_AUTO_URL_DETECTION

#ifdef __WXGTK20__
   // we need to catch right down under GTK 2 as right up is eaten by the
   // control itself (it shows its own menu if we don't handle right down)
   EVT_RIGHT_DOWN(TextViewerWindow::OnMouseEvent)
#else
   EVT_RIGHT_UP(TextViewerWindow::OnMouseEvent)
#endif
   EVT_LEFT_UP(TextViewerWindow::OnMouseEvent)
END_EVENT_TABLE()

TextViewerWindow::TextViewerWindow(TextViewer *viewer, wxWindow *parent)
                : wxTextCtrl(parent, -1, wxEmptyString,
                             wxDefaultPosition,
                             parent->GetClientSize(),
                             wxTE_RICH2 |
#ifdef USE_AUTO_URL_DETECTION
                             wxTE_AUTO_URL |
#endif // USE_AUTO_URL_DETECTION
                             wxTE_MULTILINE)
{
   m_viewer = viewer;

   SetEditable(false);
}

TextViewerWindow::~TextViewerWindow()
{
   WX_CLEAR_ARRAY(m_clickables);
}

void TextViewerWindow::InsertClickable(const wxString& text,
                                       ClickableInfo *ci,
                                       const wxColour& col)
{
   if ( col.Ok() )
   {
      SetDefaultStyle(wxTextAttr(col));
   }

   TextViewerClickable *clickable =
      new TextViewerClickable(ci, GetLastPosition(), text.length());
   m_clickables.Add(clickable);

   AppendText(text);

   if ( col.Ok() )
   {
      // reset the style back to the previous one
      SetDefaultStyle(wxTextAttr());
   }
}

void TextViewerWindow::Clear()
{
   wxTextCtrl::Clear();

   // reset the default style because it could have font with an encoding which
   // we shouldn't use for the next message we'll show
   SetDefaultStyle(wxTextAttr());

   WX_CLEAR_ARRAY(m_clickables);
}

#ifdef USE_AUTO_URL_DETECTION

void TextViewerWindow::OnLinkEvent(wxTextUrlEvent& event)
{
   wxMouseEvent eventMouse = event.GetMouseEvent();
   wxEventType type = eventMouse.GetEventType();
   if ( type == wxEVT_RIGHT_UP || type == wxEVT_LEFT_UP )
   {
      if ( ProcessMouseEvent(eventMouse, event.GetURLStart()) )
      {
         // skip event.Skip() below
         return;
      }
   }

   event.Skip();
}

#endif // USE_AUTO_URL_DETECTION

void TextViewerWindow::OnMouseEvent(wxMouseEvent& event)
{
   long pos;
#if defined(__WXMSW__) || defined(__WXGTK20__)
   wxTextCtrlHitTestResult rc = HitTest(event.GetPosition(), &pos);
#else
   wxTextCtrlHitTestResult rc = wxTE_HT_ON_TEXT;
   pos = GetInsertionPoint();
#endif

   if ( rc != wxTE_HT_ON_TEXT || !ProcessMouseEvent(event, pos) )
   {
      event.Skip();
   }
}

bool TextViewerWindow::ProcessMouseEvent(const wxMouseEvent& event, long pos)
{
   size_t count = m_clickables.GetCount();
   for ( size_t n = 0; n < count; n++ )
   {
      TextViewerClickable *clickable = m_clickables[n];
      if ( clickable->Inside(pos) )
      {
#ifdef __WXGTK20__
         if ( event.RightDown() )
#else
         if ( event.RightUp() )
#endif
         {
            clickable->GetClickableInfo()->OnRightClick(event.GetPosition());
         }
         else if ( event.LeftUp() )
         {
            // we don't want to count releasing the mouse to finish selecting
            // something as a click as otherwise we wouldn't be able to select
            // a link without activating it
            long from,
                 to;

            GetSelection(&from, &to);
            if ( from != to )
               return false;


#ifdef __WXMSW__
            // the mouse cursor is captured by the text control when we're here
            // and this results in very strange behaviour if we open a window
            // without releasing mouse capture first
            if ( HasCapture() )
            {
               // don't use wx ReleaseMouse() method because the mouse is not
               // captured at wx level, go down to MSW function instead
               ::ReleaseCapture();
            }
#endif // __WXMSW__

            clickable->GetClickableInfo()->OnLeftClick();
         }
         else
         {
            FAIL_MSG( wxS("unexpected mouse event") );

            return false;
         }

         return true;
      }
   }

   return false;
}

// ============================================================================
// TextViewer implementation
// ============================================================================

// ----------------------------------------------------------------------------
// ctor
// ----------------------------------------------------------------------------

IMPLEMENT_MESSAGE_VIEWER
(
   TextViewer,
   _("Text only message viewer"),
   gettext_noop("(c) 2001 Vadim Zeitlin <vadim@wxwindows.org>")
);

TextViewer::TextViewer()
{
   m_window = NULL;
   m_posFind = -1;

#if wxUSE_PRINTING_ARCHITECTURE
   m_printText = NULL;
#endif // wxUSE_PRINTING_ARCHITECTURE
}

// ----------------------------------------------------------------------------
// TextViewer creation &c
// ----------------------------------------------------------------------------

void TextViewer::Create(MessageView *msgView, wxWindow *parent)
{
   m_msgView = msgView;
   m_window = new TextViewerWindow(this, parent);
}

void TextViewer::Clear()
{
   // we shouldn't have anything left over from the last message we showed
   ASSERT_MSG( m_textToAppend.empty(), _T("forgot to call FlushText()?") );


   m_window->Clear();

   m_window->Freeze();

   const ProfileValues& profileValues = GetOptions();

   wxFont font(profileValues.GetFont());
   if ( font.Ok() )
      m_window->SetFont(font);
   m_window->SetForegroundColour(profileValues.FgCol);
   m_window->SetBackgroundColour(profileValues.BgCol);
}

void TextViewer::Update()
{
   m_window->Thaw();
}

void TextViewer::UpdateOptions()
{
   // no special options

   // TODO: support for line wrap
}

void TextViewer::FlushText()
{
   if ( !m_textToAppend.empty() )
   {
      m_window->AppendText(m_textToAppend);
      m_textToAppend.clear();
   }
}

// ----------------------------------------------------------------------------
// TextViewer operations
// ----------------------------------------------------------------------------

bool TextViewer::Find(const String& text)
{
   m_posFind = -1;
   m_textFind = text;

   return FindAgain();
}

bool TextViewer::FindAgain()
{
   const wxChar *pStart = m_window->GetValue();

   const wxChar *p = pStart;
   if ( m_posFind != -1 )
   {
      // start looking at the next position after the last match
      p += m_posFind + 1;
   }

   p = *p != '\0' ? wxStrstr(p, m_textFind) : NULL;

   if ( p )
   {
      m_posFind = p - pStart;

      m_window->SetSelection(m_posFind, m_posFind + m_textFind.length());
   }
   else // not found
   {
      m_window->SetSelection(0, 0);
   }

   return p != NULL;
}

void TextViewer::Copy()
{
   m_window->Copy();
}

void TextViewer::SelectAll()
{
   m_window->SelectAll();
}

String TextViewer::GetSelection() const
{
   return m_window->GetStringSelection();
}

// ----------------------------------------------------------------------------
// TextViewer printing
// ----------------------------------------------------------------------------

void TextViewer::InitPrinting()
{
#if wxUSE_PRINTING_ARCHITECTURE
   if ( !m_printText )
   {
      m_printText = new wxTextEasyPrinting(_("Mahogany Printing"),
                                           GetFrame(m_window));
   }

   wxMApp *app = (wxMApp *)mApplication;

   *m_printText->GetPrintData() = *app->GetPrintData();
   *m_printText->GetPageSetupData() = *app->GetPageSetupData();
#endif // wxUSE_PRINTING_ARCHITECTURE
}

bool TextViewer::Print()
{
#if wxUSE_PRINTING_ARCHITECTURE
   InitPrinting();

   return m_printText->Print(m_window);
#else // !wxUSE_PRINTING_ARCHITECTURE
   return false;
#endif // wxUSE_PRINTING_ARCHITECTURE/!wxUSE_PRINTING_ARCHITECTURE
}

void TextViewer::PrintPreview()
{
#if wxUSE_PRINTING_ARCHITECTURE
   InitPrinting();

   (void)m_printText->Preview(m_window);
#endif // wxUSE_PRINTING_ARCHITECTURE
}

wxWindow *TextViewer::GetWindow() const
{
   return m_window;
}

// ----------------------------------------------------------------------------
// header showing
// ----------------------------------------------------------------------------

void TextViewer::StartHeaders()
{
}

void TextViewer::ShowRawHeaders(const String& header)
{
   m_window->AppendText(header);
}

void TextViewer::ShowHeaderName(const String& name)
{
   // flush previous text before the style change
   FlushText();

   const ProfileValues& profileValues = GetOptions();

   // change the style for the header name
   MTextStyle attr(profileValues.HeaderNameCol);
   wxFont font = m_window->GetFont();
   font.SetWeight(wxFONTWEIGHT_BOLD);
   attr.SetFont(font);
   m_window->SetDefaultStyle(attr);

   // do show it
   m_window->AppendText(name + _T(": "));

   // and restore the non bold font for the header value which will follow
   attr.SetFont(m_window->GetFont());
   m_window->SetDefaultStyle(attr);
}

void TextViewer::ShowHeaderValue(const String& value,
                                 wxFontEncoding encoding)
{
   const ProfileValues& profileValues = GetOptions();

   wxColour col = profileValues.HeaderValueCol;
   if ( !col.Ok() )
      col = profileValues.FgCol;

   MTextStyle attr(col);
   if ( encoding != wxFONTENCODING_SYSTEM )
   {
      wxFont font = profileValues.GetFont(encoding);
      attr.SetFont(font);
   }

   InsertText(value, attr);
}

void TextViewer::ShowHeaderURL(const String& text,
                               const String& url)
{
   InsertURL(text, url);
}

void TextViewer::EndHeader()
{
   InsertText(_T("\n"), MTextStyle());
}

void TextViewer::ShowXFace(const wxBitmap& bitmap)
{
   FlushText();

   // we can't show faces in the control itself so insert a clickable object
   // which shows it
   m_window->InsertClickable(_("[Click here to see the face picture]"),
                             new ClickableFace(m_msgView, bitmap),
                             GetOptions().HeaderValueCol);
   EndHeader();
}

void TextViewer::EndHeaders()
{
}

// ----------------------------------------------------------------------------
// body showing
// ----------------------------------------------------------------------------

void TextViewer::StartBody()
{
}

void TextViewer::StartPart()
{
   // put a blank line before each part start - including the very first one to
   // separate it from the headers
   InsertText(_T("\n"), MTextStyle());
}

void TextViewer::InsertAttachment(const wxBitmap& /* icon */, ClickableInfo *ci)
{
   FlushText();

   String str;
   str << _("[Attachment: ") << ci->GetLabel() << _T(']');

   m_window->InsertClickable(str, ci, GetOptions().AttCol);
}

void TextViewer::InsertClickable(const wxBitmap& /* icon */,
                                 ClickableInfo *ci,
                                 const wxColour& col)
{
   FlushText();

   String str;
   str << _T('[') << ci->GetLabel() << _T(']');

   m_window->InsertClickable(str, ci, col);
}

void
TextViewer::InsertImage(const wxImage& /* image */, ClickableInfo * /* ci */)
{
   // as we return false from CanInlineImages() this is not supposed to be
   // called
   FAIL_MSG( _T("unexpected call to TextViewer::InsertImage") );
}

void TextViewer::InsertRawContents(const String& /* data */)
{
   // as we return false from our CanProcess(), MessageView is not supposed to
   // ask us to process any raw data
   FAIL_MSG( _T("unexpected call to TextViewer::InsertRawContents()") );
}

void TextViewer::InsertText(const String& text, const MTextStyle& style)
{
   // check if we need to change style
   wxTextAttr old = m_window->GetDefaultStyle();
   if ( (style.HasTextColour() &&
            style.GetTextColour() != old.GetTextColour()) ||
        (style.HasBackgroundColour() &&
            style.GetBackgroundColour() != old.GetBackgroundColour()) ||
        (style.HasFont() && style.GetFont() != old.GetFont()) )
   {
      // we do, flush the text in old style
      FlushText();

      // and set the new one
      m_window->SetDefaultStyle(style);
   }

   m_textToAppend += text;
}

void TextViewer::InsertURL(const String& text, const String& url)
{
   FlushText();

   m_window->InsertClickable(text,
                             new ClickableURL(m_msgView, url),
                             GetOptions().UrlCol);
}

void TextViewer::EndPart()
{
   FlushText();
}

void TextViewer::EndBody()
{
   FlushText();

   m_window->SetInsertionPoint(0);

   Update();
}

// ----------------------------------------------------------------------------
// scrolling
// ----------------------------------------------------------------------------

bool TextViewer::LineDown()
{
   return m_window->LineDown();
}

bool TextViewer::LineUp()
{
   return m_window->LineUp();
}

bool TextViewer::PageDown()
{
   return m_window->PageDown();
}

bool TextViewer::PageUp()
{
   return m_window->PageUp();
}

// ----------------------------------------------------------------------------
// capabilities querying
// ----------------------------------------------------------------------------

bool TextViewer::CanInlineImages() const
{
   // we can't show any images
   return false;
}

bool TextViewer::CanProcess(const String& /* mimetype */) const
{
   // we don't have any special processing for any MIME types
   return false;
}


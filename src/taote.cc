// Copyright 2020 The Taote Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------

#include <gtk/gtk.h>
#include <inttypes.h>
#include <vte/vte.h>

#include "./config.h"

// --------

typedef enum {
  ACTIVATE_FALSE = 0,
  ACTIVATE_TRUE = 1,
} Activate;

typedef enum {
  DETACH_TEMPORARILY = 0,
  DETACH_PERMANENTLY = 1,
} Detach;

typedef enum {
  DIR_PREV = 0,
  DIR_NEXT = 1,
  NUM_DIRS = 2,
} Dir;

typedef enum {
  NUDGE_FALSE = 0,
  NUDGE_TRUE = 1,
} Nudge;

// --------

void  //
onActivate(GtkApplication* app, gpointer context);

void  //
onChildExited(VteTerminal* terminal, int status, gpointer context);

gboolean  //
onKeyPressEvent(GtkWidget* widget, GdkEvent* event, gpointer context);

void  //
onSpawn(VteTerminal* terminal, GPid pid, GError* error, gpointer context);

void  //
onWindowTitleChanged(VteTerminal* terminal, gpointer context);

// --------

class Tab;
class Window;

template <class T>
gboolean  //
deleteTheArg(gpointer arg) {
  delete static_cast<T*>(arg);
  return FALSE;  // g_idle_add semantics: don't run again.
}

template <class T>
void  //
onDestroyDeleteTheArg(GtkWidget* widget, gpointer arg) {
  g_idle_add(deleteTheArg<T>, arg);
}

// --------

class Tab {
 public:
  explicit Tab();
  ~Tab();

  // Delete the copy and assign constructors.
  Tab(const Tab&) = delete;
  Tab& operator=(const Tab&) = delete;

  // ----

  bool isClosed() const;
  bool isSelected() const;

  void clipboardCopy();
  void clipboardPaste();

  void zoomMore(int sign);
  void zoomReset();

  void close();
  void ensureTerminalWidget();
  void setIwdFrom(Tab* t);
  void toggleSelected();
  Tab* walkToOpenTab(Dir dir);

  // ----

  uint64_t mSeqNum;

  // mWinTabs are the links in the double-linked list of a Window's tabs.
  Tab* mWinTabs[NUM_DIRS];
  // mSelTabs are the links in the double-linked list of selected tabs.
  Tab* mSelTabs[NUM_DIRS];

  Window* mWindow;
  GtkWidget* mTerminal;
  char* mInitialWorkingDirectory;
  int mPid;
};

// --------

class Window {
 public:
  explicit Window(GtkApplication* app, uint32_t titleColor, Tab* cwdTab);
  ~Window() = default;

  // Delete the copy and assign constructors.
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  // ----

  void updateTitleColor(uint32_t delta);
  void updateTitleText();

  bool adoptSelectedTabs();
  void attachTab(Tab* t, Activate activate);
  void detachTab(Tab* t, Detach detach);
  Tab* mruOpenTab();
  void walk(Dir dir, Nudge nudge);

  // ----

  GtkApplication* mApp;
  uint32_t mTitleColor;

  GtkWidget* mWindow;
  GtkWidget* mLabel;
  GtkWidget* mStack;

  Tab* mTopTab;

  bool mInvincible;

  // mTabs is the dummy element of a circular double-linked list.
  Tab mTabs;
};

// --------

// gSelectedTabs is the dummy element of a circular double-linked list.
Tab gSelectedTabs;

PangoFontDescription* gFontDescription = nullptr;

uint64_t gSeqNum = 0;

const char* gShellCommand = nullptr;

// --------

Tab::Tab()
    : mSeqNum(0),
      mWinTabs{nullptr, nullptr},
      mSelTabs{nullptr, nullptr},
      mWindow(nullptr),
      mTerminal(nullptr),
      mInitialWorkingDirectory(nullptr),
      mPid(0) {}

Tab::~Tab() {
  if (mWinTabs[DIR_PREV] != nullptr) {
    mWinTabs[DIR_PREV]->mWinTabs[DIR_NEXT] = mWinTabs[DIR_NEXT];
    mWinTabs[DIR_NEXT]->mWinTabs[DIR_PREV] = mWinTabs[DIR_PREV];
    mWinTabs[DIR_PREV] = nullptr;
    mWinTabs[DIR_NEXT] = nullptr;
  }
  if (mSelTabs[DIR_PREV] != nullptr) {
    mSelTabs[DIR_PREV]->mSelTabs[DIR_NEXT] = mSelTabs[DIR_NEXT];
    mSelTabs[DIR_NEXT]->mSelTabs[DIR_PREV] = mSelTabs[DIR_PREV];
    mSelTabs[DIR_PREV] = nullptr;
    mSelTabs[DIR_NEXT] = nullptr;
  }
  if (mTerminal != nullptr) {
    g_object_unref(mTerminal);
  }
  g_free(mInitialWorkingDirectory);
}

bool  //
Tab::isClosed() const {
  return mSeqNum == 0;
}

bool  //
Tab::isSelected() const {
  return mSelTabs[DIR_PREV] != nullptr;
}

void  //
Tab::clipboardCopy() {
  if (mTerminal != nullptr) {
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(mTerminal),
                                       VTE_FORMAT_TEXT);
  }
}

void  //
Tab::clipboardPaste() {
  if (mTerminal != nullptr) {
    vte_terminal_paste_clipboard(VTE_TERMINAL(mTerminal));
  }
}

void  //
Tab::zoomMore(int sign) {
  if (mTerminal != nullptr) {
    double x = vte_terminal_get_font_scale(VTE_TERMINAL(mTerminal));
    x = (sign > 0) ? (x * 1.125) : (x / 1.125);
    if (x < 0.0625) {
      x = 0.0625;
    } else if (x > 16.0) {
      x = 16.0;
    } else if ((0.9 < x) && (x < 1.1)) {
      x = 1.0;
    }
    vte_terminal_set_font_scale(VTE_TERMINAL(mTerminal), x);
  }
}

void  //
Tab::zoomReset() {
  if (mTerminal != nullptr) {
    vte_terminal_set_font_scale(VTE_TERMINAL(mTerminal), 1.0);
  }
}

void  //
Tab::close() {
  mSeqNum = 0;
  if (mWindow) {
    mWindow->detachTab(this, DETACH_PERMANENTLY);
  }
}

void  //
Tab::ensureTerminalWidget() {
  if (mTerminal != nullptr) {
    return;
  }
  mSeqNum = ++gSeqNum;

  mTerminal = vte_terminal_new();
  g_object_ref_sink(mTerminal);

  if (gFontDescription == nullptr) {
    gFontDescription = pango_font_description_from_string(FONT);
  }
  vte_terminal_set_font(VTE_TERMINAL(mTerminal), gFontDescription);

  vte_terminal_set_colors(VTE_TERMINAL(mTerminal), nullptr, nullptr,
                          g_palette_colors, NUM_G_PALETTE_COLORS);
  vte_terminal_set_mouse_autohide(VTE_TERMINAL(mTerminal), TRUE);
  vte_terminal_set_scrollback_lines(VTE_TERMINAL(mTerminal), SCROLLBACK_LINES);
  vte_terminal_set_word_char_exceptions(VTE_TERMINAL(mTerminal),
                                        WORD_CHAR_EXCEPTIONS);

  const char* argv[2] = {gShellCommand, nullptr};
  vte_terminal_spawn_async(VTE_TERMINAL(mTerminal),   // terminal
                           VTE_PTY_DEFAULT,           // pty_flags
                           mInitialWorkingDirectory,  // working_directory
                           const_cast<char**>(argv),  // argv
                           NULL,                      // envv
                           G_SPAWN_DEFAULT,           // spawn_flags
                           NULL,                      // child_setup
                           NULL,                      // child_setup_data
                           NULL,      // child_setup_data_destroy
                           -1,        // timeout
                           NULL,      // cancellable
                           &onSpawn,  // calback
                           this);     // user_data
  g_free(mInitialWorkingDirectory);
  mInitialWorkingDirectory = nullptr;

  g_signal_connect(mTerminal, "child-exited", G_CALLBACK(onChildExited), this);
  g_signal_connect(mTerminal, "destroy", G_CALLBACK(onDestroyDeleteTheArg<Tab>),
                   this);
  g_signal_connect(mTerminal, "window-title-changed",
                   G_CALLBACK(onWindowTitleChanged), this);

  gtk_widget_show_all(mTerminal);
}

void  //
Tab::setIwdFrom(Tab* t) {
  if ((t != nullptr) && (t->mPid > 0)) {
    char* cwdFile = g_strdup_printf("/proc/%d/cwd", t->mPid);
    mInitialWorkingDirectory = g_file_read_link(cwdFile, nullptr);
    g_free(cwdFile);
  }
}

void  //
Tab::toggleSelected() {
  if (mSelTabs[DIR_PREV]) {
    mSelTabs[DIR_PREV]->mSelTabs[DIR_NEXT] = mSelTabs[DIR_NEXT];
    mSelTabs[DIR_NEXT]->mSelTabs[DIR_PREV] = mSelTabs[DIR_PREV];
    mSelTabs[DIR_PREV] = nullptr;
    mSelTabs[DIR_NEXT] = nullptr;
  } else if (!isClosed()) {
    mSelTabs[DIR_PREV] = gSelectedTabs.mSelTabs[DIR_PREV];
    mSelTabs[DIR_NEXT] = &gSelectedTabs;
    mSelTabs[DIR_PREV]->mSelTabs[DIR_NEXT] = this;
    mSelTabs[DIR_NEXT]->mSelTabs[DIR_PREV] = this;
  }
}

Tab*  //
Tab::walkToOpenTab(Dir dir) {
  Tab* t = mWinTabs[dir];
  if (t == nullptr) {
    return nullptr;
  }
  while (t != this) {
    if (!t->isClosed()) {
      return t;
    }
    t = t->mWinTabs[dir];
  }
  return !t->isClosed() ? t : nullptr;
}

// --------

Window::Window(GtkApplication* app, uint32_t titleColor, Tab* cwdTab)
    : mApp(app),
      mTitleColor(titleColor),
      mWindow(nullptr),
      mLabel(nullptr),
      mTopTab(nullptr),
      mInvincible(false) {
  mTabs.mWindow = this;
  mTabs.mWinTabs[DIR_PREV] = &mTabs;
  mTabs.mWinTabs[DIR_NEXT] = &mTabs;

  GdkRGBA white = {1.0, 1.0, 1.0, 1.0};

  mLabel = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(mLabel), 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  gtk_widget_override_color(mLabel, GTK_STATE_FLAG_NORMAL, &white);
#pragma GCC diagnostic pop

  mStack = gtk_stack_new();

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(box), mLabel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), mStack, TRUE, TRUE, 0);

  mWindow = gtk_application_window_new(app);
  gtk_container_add(GTK_CONTAINER(mWindow), box);
  gtk_window_set_title(GTK_WINDOW(mWindow), "Terminal");
  gtk_window_set_default_size(GTK_WINDOW(mWindow), 640, 480);

  g_signal_connect(mWindow, "destroy",
                   G_CALLBACK(onDestroyDeleteTheArg<Window>), this);
  g_signal_connect(mWindow, "key-press-event", G_CALLBACK(onKeyPressEvent),
                   this);

  if (!adoptSelectedTabs()) {
    Tab* t = new Tab();
    t->setIwdFrom(cwdTab);
    attachTab(t, ACTIVATE_TRUE);
  }
  updateTitleColor(0);
  gtk_widget_show_all(mWindow);
}

void  //
Window::updateTitleColor(uint32_t delta) {
  mTitleColor += delta;
  mTitleColor %= NUM_G_TITLE_COLORS;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  gtk_widget_override_background_color(mWindow, GTK_STATE_FLAG_NORMAL,
                                       &g_title_colors[mTitleColor]);
#pragma GCC diagnostic pop
}

void  //
Window::updateTitleText() {
  size_t i = 0;
  size_t n = 0;
  for (Tab* t = mTabs.mWinTabs[DIR_NEXT]; t != &mTabs;
       t = t->mWinTabs[DIR_NEXT]) {
    if (t->isClosed()) {
      continue;
    }
    n++;
    if (t == mTopTab) {
      i = n;
    }
  }

  const char* title = nullptr;
  if ((mTopTab != nullptr) && (mTopTab->mTerminal != nullptr)) {
    title = vte_terminal_get_window_title(VTE_TERMINAL(mTopTab->mTerminal));
  }
  if (title == nullptr) {
    title = "";
  }

  gchar* s = g_strdup_printf("%s  %zu/%zu  %s",
                             ((mTopTab && (mTopTab->isSelected()))
                                  ? "☑"    // U+2611 BALLOT BOX WITH CHECK
                                  : "☐"),  // U+2610 BALLOT BOX
                             i, n, title);
  gtk_label_set_text(GTK_LABEL(mLabel), s);
  g_free(s);
}

bool  //
Window::adoptSelectedTabs() {
  bool result = false;
  mInvincible = true;

  Tab* t = gSelectedTabs.mSelTabs[DIR_NEXT];
  while (t != &gSelectedTabs) {
    Tab* next = t->mSelTabs[DIR_NEXT];
    t->mSelTabs[DIR_PREV] = nullptr;
    t->mSelTabs[DIR_NEXT] = nullptr;
    attachTab(t, (next == &gSelectedTabs) ? ACTIVATE_TRUE : ACTIVATE_FALSE);
    t = next;
    result = true;
  }

  mInvincible = false;
  gSelectedTabs.mSelTabs[DIR_PREV] = &gSelectedTabs;
  gSelectedTabs.mSelTabs[DIR_NEXT] = &gSelectedTabs;
  return result;
}

void  //
Window::attachTab(Tab* t, Activate activate) {
  t->mSeqNum = ++gSeqNum;
  if (t->mWindow != nullptr) {
    t->mWindow->detachTab(t, DETACH_TEMPORARILY);
  }

  t->ensureTerminalWidget();
  if (t->isClosed()) {
    return;
  }

  t->mWindow = this;
  Tab* prev = mTopTab ? mTopTab : &mTabs;
  Tab* next = prev->mWinTabs[DIR_NEXT];
  t->mWinTabs[DIR_PREV] = prev;
  t->mWinTabs[DIR_NEXT] = next;
  next->mWinTabs[DIR_PREV] = t;
  prev->mWinTabs[DIR_NEXT] = t;

  gtk_container_add(GTK_CONTAINER(mStack), t->mTerminal);
  if (activate == ACTIVATE_TRUE) {
    mTopTab = t;
    gtk_stack_set_visible_child(GTK_STACK(mStack), mTopTab->mTerminal);
    gtk_widget_grab_focus(mTopTab->mTerminal);
    updateTitleText();
  }
}

void  //
Window::detachTab(Tab* t, Detach detach) {
  if (t->mTerminal == nullptr) {
    // No-op.
  } else if (detach == DETACH_TEMPORARILY) {
    gtk_container_remove(GTK_CONTAINER(mStack), t->mTerminal);
  } else {
    gtk_widget_destroy(t->mTerminal);
  }

  t->mWindow = nullptr;
  if (t->mWinTabs[DIR_PREV] != nullptr) {
    t->mWinTabs[DIR_NEXT]->mWinTabs[DIR_PREV] = t->mWinTabs[DIR_PREV];
    t->mWinTabs[DIR_PREV]->mWinTabs[DIR_NEXT] = t->mWinTabs[DIR_NEXT];
    t->mWinTabs[DIR_PREV] = nullptr;
    t->mWinTabs[DIR_NEXT] = nullptr;
  }

  if (mTopTab == t) {
    mTopTab = mruOpenTab();
  }

  if (mTopTab != nullptr) {
    mTopTab->ensureTerminalWidget();
    gtk_stack_set_visible_child(GTK_STACK(mStack), mTopTab->mTerminal);
    gtk_widget_grab_focus(mTopTab->mTerminal);
    updateTitleText();
  } else if (!mInvincible) {
    gtk_widget_destroy(mWindow);
  }
}

Tab*  //
Window::mruOpenTab() {
  uint64_t bestSeqNum = 0;
  Tab* bestTab = nullptr;
  for (Tab* t = mTabs.mWinTabs[DIR_NEXT]; t != &mTabs;
       t = t->mWinTabs[DIR_NEXT]) {
    if (bestSeqNum < t->mSeqNum) {
      bestSeqNum = t->mSeqNum;
      bestTab = t;
    }
  }
  return bestTab;
}

void  //
Window::walk(Dir dir, Nudge nudge) {
  if (mTopTab == nullptr) {
    return;
  }
  Tab* t = mTopTab->walkToOpenTab(dir);
  if (t == nullptr) {
    return;
  }
  t->mSeqNum = ++gSeqNum;
  if (mTopTab != t) {
    if (nudge == NUDGE_FALSE) {
      mTopTab = t;
      gtk_stack_set_visible_child(GTK_STACK(mStack), mTopTab->mTerminal);
      gtk_widget_grab_focus(mTopTab->mTerminal);

    } else {
      if (mTopTab->mWinTabs[DIR_PREV] != nullptr) {
        mTopTab->mWinTabs[DIR_NEXT]->mWinTabs[DIR_PREV] =
            mTopTab->mWinTabs[DIR_PREV];
        mTopTab->mWinTabs[DIR_PREV]->mWinTabs[DIR_NEXT] =
            mTopTab->mWinTabs[DIR_NEXT];
        mTopTab->mWinTabs[DIR_PREV] = nullptr;
        mTopTab->mWinTabs[DIR_NEXT] = nullptr;
      }

      Tab* prev = t;
      Tab* u = t->mWinTabs[dir ^ DIR_PREV];
      mTopTab->mWinTabs[dir ^ DIR_NEXT] = t;
      mTopTab->mWinTabs[dir ^ DIR_PREV] = u;
      u->mWinTabs[dir ^ DIR_NEXT] = mTopTab;
      t->mWinTabs[dir ^ DIR_PREV] = mTopTab;
    }

    updateTitleText();
  }
}

// --------

void  //
onActivate(GtkApplication* app, gpointer context) {
  new Window(app, 0, nullptr);
}

void  //
onChildExited(VteTerminal* terminal, int status, gpointer context) {
  Tab* t = static_cast<Tab*>(context);
  t->close();
}

gboolean  //
onKeyPressEvent(GtkWidget* widget, GdkEvent* event, gpointer context) {
  Window* w = static_cast<Window*>(context);
  if ((event->type != GDK_KEY_PRESS) ||
      ((event->key.state & gtk_accelerator_get_default_mod_mask()) !=
       (GDK_CONTROL_MASK | GDK_SHIFT_MASK))) {
    return FALSE;
  }

  switch (event->key.keyval) {
    case 'C':
      if (w->mTopTab) {
        w->mTopTab->clipboardCopy();
      }
      return TRUE;

    case 'V':
      if (w->mTopTab) {
        w->mTopTab->clipboardPaste();
      }
      return TRUE;

    case 'N':
      new Window(w->mApp, w->mTitleColor, w->mTopTab);
      return TRUE;

    case 'T': {
      Tab* t = new Tab();
      t->setIwdFrom(w->mTopTab);
      w->attachTab(t, ACTIVATE_TRUE);
      return TRUE;
    }

    case 'Y':
      if (w->mTopTab) {
        w->mTopTab->close();
      }
      return TRUE;

    case 'S':
      if (w->mTopTab) {
        w->mTopTab->toggleSelected();
        w->updateTitleText();
      }
      return TRUE;

    case 'W':
      w->adoptSelectedTabs();
      return TRUE;

    case 'K':
    case GDK_KEY_Page_Up:
      w->walk(DIR_PREV, NUDGE_FALSE);
      return TRUE;

    case 'J':
    case GDK_KEY_Page_Down:
      w->walk(DIR_NEXT, NUDGE_FALSE);
      return TRUE;

    case 'H':
    case GDK_KEY_Home:
      w->walk(DIR_PREV, NUDGE_TRUE);
      return TRUE;

    case 'L':
    case GDK_KEY_End:
      w->walk(DIR_NEXT, NUDGE_TRUE);
      return TRUE;

    case '<':
      w->updateTitleColor(NUM_G_TITLE_COLORS - 1);
      return TRUE;

    case '>':
      w->updateTitleColor(NUM_G_TITLE_COLORS + 1);
      return TRUE;

    case '_':
      if (w->mTopTab) {
        w->mTopTab->zoomMore(-1);
      }
      return TRUE;

    case '+':
      if (w->mTopTab) {
        w->mTopTab->zoomMore(+1);
      }
      return TRUE;

    case ')':
      if (w->mTopTab) {
        w->mTopTab->zoomReset();
      }
      return TRUE;
  }
  return FALSE;
}

void  //
onSpawn(VteTerminal* terminal, GPid pid, GError* error, gpointer context) {
  if (terminal == nullptr) {
    // No-op. As per the VTE docs, nullptr means that the terminal was
    // destroyed before the async spawn finished.
  } else if (error != nullptr) {
    g_error_free(error);
  } else if (pid >= 0) {
    Tab* t = static_cast<Tab*>(context);
    t->mPid = static_cast<int>(pid);
  }
}

void  //
onWindowTitleChanged(VteTerminal* terminal, gpointer context) {
  Tab* t = static_cast<Tab*>(context);
  if (t->mWindow != nullptr) {
    t->mWindow->updateTitleText();
  }
}

// --------

int  //
main(int argc, char** argv) {
  char* shellCommand = g_strdup(g_getenv("SHELL"));
  gShellCommand = shellCommand ? shellCommand : "/bin/sh";
  gSelectedTabs.mSelTabs[DIR_PREV] = &gSelectedTabs;
  gSelectedTabs.mSelTabs[DIR_NEXT] = &gSelectedTabs;

  GtkApplication* app = gtk_application_new("com.github.nigeltao.taote",
                                            G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(onActivate), nullptr);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  if (gFontDescription) {
    pango_font_description_free(gFontDescription);
  }
  g_free(shellCommand);
  return status;
}

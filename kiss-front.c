#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "kisslib.h" // read_pdf, read_mobi, read_chm, read_epub

// GTK3
#include <gtk/gtk.h>
#include <gio/gio.h>

//SQLite3
#include <sqlite3.h>

//GNU
#include <dirent.h>

//------------------------------------------------------------------------------
enum {
  STARTUP_COLUMN,
  FORMAT_COLUMN,
  AUTHOR_COLUMN,
  TITLE_COLUMN,
  N_COLUMNS
};

typedef struct dbHandlingData {
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreeIter *iter;
  gchar **data;
} dbHandlingData;

typedef struct dbAnswer {
  unsigned int count;
  char **data;
  char **columnNames;
} dbAnswer;

//------------------------------------------------------------------------------

void run(GtkApplication*, gpointer);
void handle_launchCommand(GtkWidget*);

gboolean handle_drag_data(GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData*, guint, gpointer);
gboolean handle_key_press(GtkWidget*, GdkEventKey*, gpointer);
gboolean handle_editing_author(GtkCellRendererText*, gchar*, gchar*, gpointer);
gboolean handle_editing_title(GtkCellRendererText*, gchar*, gchar*, gpointer);
void handle_row_activated(GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer);


bool read_and_add_file_to_model(char*, bool, GtkWidget*, unsigned int, bool, GtkWidget*, unsigned int, unsigned int, bool, GtkTreeModel*, sqlite3*);
int read_out_path(char*, GtkWidget*, unsigned int, GtkWidget*, unsigned int, unsigned int, GtkTreeModel*, sqlite3*);

void menuhandle_meQuit(GtkMenuItem*, gpointer);
void menuhandle_meImportFiles(GtkMenuItem*, gpointer);
void menuhandle_meSetLauncher(GtkMenuItem*, gpointer);
void launcherWindow_save_data(GtkButton*, gpointer);
void launcherWindow_close(GtkButton*, gpointer);

void menuhandle_meEditEntry(GtkMenuItem*, gpointer);
void open_edit_window(GObject*);
void edit_entry_close(GtkButton*, gpointer);
void edit_entry_save_data(GtkButton*, gpointer);

void fileChooser_close(GtkButton*, gpointer);
void fileChooser_importFiles(GtkButton*, gpointer);

// Db related
int add_db_data_to_store(void*, int, char**, char**);

int fill_db_answer(void*, int, char**, char**);
void free_db_answer(struct dbAnswer*);
bool get_db_answer_value(struct dbAnswer*, const char[], char**);

//------------------------------------------------------------------------------
int main (int argc, char *argv[]) {
  GtkApplication *app;
  int status;

  // SQLite3 code
  sqlite3 *db;
  char *dbErrorMsg = NULL;

  int rc = sqlite3_open("kisslib.db", &db);

  if (rc) {
    sqlite3_close(db);
    printf("Error opening sqlite3 database \"kisslib.db\" file. Exiting.");
    return 1;
  } else {
    rc = sqlite3_exec(db, " \
      CREATE TABLE IF NOT EXISTS ebook_collection ( \
        'id' INTEGER PRIMARY KEY ASC, \
        'format' INTEGER, \
        'author' TEXT, \
        'title' TEXT, \
        'path' TEXT, \
        'filename' TEXT \
      ); \
      CREATE TABLE IF NOT EXISTS launcher_applications ( \
        'id' INTEGER PRIMARY KEY ASC, \
        'format' INTEGER, \
        'program' TEXT DEFAULT NULL, \
        'args' TEXT DEFAULT NULL \
      )", NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      printf("SQL error: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
      sqlite3_close(db);

      if (rc == SQLITE_READONLY) {
        printf("Error opening sqlite3 database for writing. Exiting.");
      }

      return 1;
    }
  }

  //----------------------------------------------------------------------------

  app = gtk_application_new("net.dwrox.kiss", G_APPLICATION_HANDLES_OPEN);
  g_signal_connect(app, "activate", G_CALLBACK (run), NULL);
  g_object_set_data(G_OBJECT(app), "db", db);
  status = g_application_run(G_APPLICATION(app), argc, argv);

  g_object_unref(g_object_get_data(G_OBJECT(app), "dataStore"));
  g_object_unref(app);

  // Close db

  sqlite3_exec(db, "VACUUM", NULL, NULL, &dbErrorMsg);
  sqlite3_close(db);

  return status;
}


//------------------------------------------------------------------------------
int fill_db_answer(void* dbAnswerData, int argc, char **argv, char **columnNames) {
  /*
  typedef struct dbAnswer {
    unsigned int count;
    char **data;
    char **columnNames;
  } dbAnswer;
  */

  dbAnswer *answerData = (dbAnswer*) dbAnswerData;

  for (int i = 0; i < argc; ++i) {
    answerData->data = (char**) realloc(answerData->data, sizeof(char*) * (answerData->count + 1));
    answerData->data[i] = malloc(sizeof(char) * (strlen(argv[i]) + 1));
    strcpy(answerData->data[i], argv[i]);

    answerData->columnNames = (char**) realloc(answerData->columnNames, sizeof(char*) * (answerData->count + 1));
    answerData->columnNames[i] = malloc(sizeof(char) * (strlen(argv[i]) + 1));
    strcpy(answerData->columnNames[i], columnNames[i]);

    ++answerData->count;
  }

  return 0;
}

//------------------------------------------------------------------------------
bool get_db_answer_value(struct dbAnswer* answerData, const char columnName[], char** data) {
  for (int i = 0; i < answerData->count; ++i) {
    if (strcmp(answerData->columnNames[i], columnName) == 0) {
      *data = malloc(sizeof(char) * (strlen(answerData->data[i]) + 1));
      strcpy(*data, answerData->data[i]);
      return true;
    }
  }

  return false;
}

//------------------------------------------------------------------------------
void free_db_answer(struct dbAnswer *answerData) {
  /*
  typedef struct dbAnswer {
    unsigned int count;
    char **data;
    char **columnNames;
  } dbAnswer;
  */

  for (int i = 0; i < answerData->count; ++i) {
    free(answerData->data[i]);
    free(answerData->columnNames[i]);
  }

  free(answerData->data);
  free(answerData->columnNames);

  answerData->count = 0;
  answerData->data = NULL;
  answerData->columnNames = NULL;
}


//------------------------------------------------------------------------------

int add_db_data_to_store(void* dataStore, int argc, char **argv, char **columnNames) {
  GtkListStore *store = GTK_LIST_STORE(dataStore);
  GtkTreeIter iter;

  char formatString[5];
  char *author = NULL;
  char *title = NULL;

  for(int i = 0; i < argc; i++) {
    if (strcmp(columnNames[i], "format") == 0) {
      switch(atoi(argv[i])) {
        case 1:
          strcpy(formatString, "pdf");
          break;
        case 2:
          strcpy(formatString, "epub");
          break;
        case 3:
          strcpy(formatString, "mobi");
          break;
        case 4:
          strcpy(formatString, "chm");
          break;
        default:
          break;
      }
    } else if (strcmp(columnNames[i], "author") == 0) {
      author = calloc((strlen(argv[i]) + 1), sizeof(char));
      strcpy(author, argv[i]);

    } else if (strcmp(columnNames[i], "title") == 0) {
      title = calloc((strlen(argv[i]) + 1), sizeof(char));
      strcpy(title, argv[i]);
    }
  }

  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter,
    FORMAT_COLUMN, formatString,
    AUTHOR_COLUMN, author == NULL ? "Unknown" : author,
    TITLE_COLUMN, title,
    -1
  );

  free(author);
  free(title);

  return 0;
}

void run(GtkApplication *app, gpointer user_data) {
  GtkWidget *window;
  GtkWidget *grid;

  GtkWidget *ebookList;

  GtkCellRenderer *ebookListRender;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "KISS Ebook Starter");
  gtk_window_set_default_size(GTK_WINDOW(window), 640, 400);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(window), grid);


  //----------------------------------------------------------------------------

  GtkListStore *dataStore = gtk_list_store_new(
    N_COLUMNS,
    GDK_TYPE_PIXBUF,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING
  );

  gtk_list_store_append(dataStore, &iter);
  gtk_list_store_set(dataStore, &iter,
    FORMAT_COLUMN, "pdf",
    AUTHOR_COLUMN, "theSplit",
    TITLE_COLUMN, "PDFs from day one, more or less ;)",
    -1
  );


  gtk_list_store_append(dataStore, &iter);
  gtk_list_store_set(dataStore, &iter,
    FORMAT_COLUMN, "chm",
    AUTHOR_COLUMN, "theSplit",
    TITLE_COLUMN, "No Windows Help here, but available",
    -1
  );

  gtk_list_store_append(dataStore, &iter);
  gtk_list_store_set(dataStore, &iter,
    FORMAT_COLUMN, "mobi",
    AUTHOR_COLUMN, "theSplit",
    TITLE_COLUMN, "First chars of the file",
    -1
  );

  gtk_list_store_append(dataStore, &iter);
  gtk_list_store_set(dataStore, &iter,
    FORMAT_COLUMN, "epub",
    AUTHOR_COLUMN, "theSplit",
    TITLE_COLUMN, "libzip is the way to go!",
    -1
  );


  //----------------------------------------------------------------------------
  // Read entries from db into list store
  //----------------------------------------------------------------------------
  sqlite3* db = g_object_get_data(G_OBJECT(app), "db");

  char *dbErrorMsg = NULL;

  int rc = sqlite3_exec(db, "SELECT format, author, title, path FROM ebook_collection;", add_db_data_to_store, (void*) dataStore, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    printf("SQL error: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  //----------------------------------------------------------------------------

  GtkWidget *menuBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_grid_attach(GTK_GRID(grid), menuBox, 0, 0, 10, 1);

  ebookList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dataStore));

  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(ebookList), true);
  gtk_widget_set_hexpand(ebookList, true);
  gtk_widget_set_vexpand(ebookList, true);

  // NOTE: Add reorder option?
  //gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ebookList), true);

  gtk_tree_view_set_search_column(GTK_TREE_VIEW(ebookList), TITLE_COLUMN);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(ebookList), GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_drag_dest_set(ebookList, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(ebookList);

  GtkAdjustment *vadjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(ebookList));
  GtkAdjustment *hadjustment = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(ebookList));
  GtkWidget *scrollWin = gtk_scrolled_window_new(hadjustment, vadjustment);
  gtk_grid_attach(GTK_GRID(grid), scrollWin, 0, 1, 10, 1);
  gtk_container_add(GTK_CONTAINER(scrollWin), ebookList);

  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrollWin), 800);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrollWin), 400);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scrollWin), false);

  GtkWidget* progressRevealer = gtk_revealer_new();
  gtk_revealer_set_transition_duration(GTK_REVEALER(progressRevealer), 2250);
  gtk_revealer_set_transition_type(GTK_REVEALER(progressRevealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_grid_attach(GTK_GRID(grid), progressRevealer, 0, 2, 10, 1);

  GtkWidget *progressBar = gtk_progress_bar_new();
  g_object_set(G_OBJECT(progressBar), "margin-top", 8, "margin-bottom", 5, "margin-left", 15, "margin-right", 15, NULL);
  gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progressBar), true);
  gtk_container_add(GTK_CONTAINER(progressRevealer), progressBar);

  GtkWidget *statusBar = gtk_statusbar_new();
  gtk_grid_attach(GTK_GRID(grid), statusBar, 0, 3, 10, 1);
  guint contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
  gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, "Welcome to KISS Ebook");

  g_signal_connect(G_OBJECT(ebookList), "row-activated", G_CALLBACK (handle_row_activated), NULL);
  g_signal_connect(G_OBJECT(ebookList), "key_press_event", G_CALLBACK (handle_key_press), NULL);
  g_signal_connect(G_OBJECT(ebookList), "drag_data_received", G_CALLBACK(handle_drag_data), NULL);
  g_object_set_data(G_OBJECT(ebookList), "app", app);
  g_object_set_data(G_OBJECT(ebookList), "appWindow", window);
  g_object_set_data(G_OBJECT(ebookList), "db", g_object_get_data(G_OBJECT(app), "db"));
  g_object_set_data(G_OBJECT(ebookList), "dataStore", dataStore);
  g_object_set_data(G_OBJECT(ebookList), "status", statusBar);
  g_object_set_data(G_OBJECT(ebookList), "progress", progressBar);
  g_object_set_data(G_OBJECT(ebookList), "progressRevealer", progressRevealer);
  g_object_set_data(G_OBJECT(ebookList), "treeview", ebookList);

  //----------------------------------------------------------------------------

  GtkIconTheme *iconTheme = gtk_icon_theme_get_default();
  GError *iconError = NULL;
  GtkIconInfo *infoOpenIcon = gtk_icon_theme_lookup_icon(iconTheme, "document-open", 24, GTK_ICON_LOOKUP_NO_SVG);
  GdkPixbuf *infoIcon = gtk_icon_info_load_icon(infoOpenIcon, &iconError);

  if (iconError != NULL) {
    printf("Icon loading error: %u - %d -%s\n", iconError->domain, iconError->code, iconError->message);
    g_error_free(iconError);
  } else {
    GtkCellRenderer *imageRenderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(G_OBJECT(imageRenderer), "pixbuf", infoIcon, NULL);
    gtk_cell_renderer_set_padding(imageRenderer, 5, 8);

    column = gtk_tree_view_column_new_with_attributes("Open", imageRenderer, NULL, STARTUP_COLUMN, NULL);
    gtk_tree_view_column_set_resizable(column, false);
    gtk_tree_view_column_set_min_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), column);
  }


  ebookListRender = gtk_cell_renderer_text_new();
  gtk_cell_renderer_set_padding(ebookListRender, 5, 8);

  column = gtk_tree_view_column_new_with_attributes("Format", ebookListRender, "text", FORMAT_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(column, false);
  gtk_tree_view_column_set_min_width(column, 80);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), column);

  ebookListRender = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListRender), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListRender), "edited", G_CALLBACK(handle_editing_author), ebookList);

  column = gtk_tree_view_column_new_with_attributes("Author", ebookListRender, "text", AUTHOR_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(column, 110);
  gtk_tree_view_column_set_resizable(column, true);
  gtk_cell_renderer_set_padding(ebookListRender, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), column);

  ebookListRender = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListRender), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListRender), "edited", G_CALLBACK(handle_editing_title), ebookList);

  column = gtk_tree_view_column_new_with_attributes("Title", ebookListRender, "text", TITLE_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(column, 240);
  gtk_tree_view_column_set_resizable(column, true);
  gtk_cell_renderer_set_padding(ebookListRender, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), column);



  //TODO: Add a sort function
  // gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(ebookList), true);



  // The main menu -------------------------------------------------------------
  // TODO: Should the main menu use images?
  // NOTE: https://developer.gnome.org/gtk3/stable/GtkImageMenuItem.html#GtkImageMenuItem--image

  GtkWidget *menu = gtk_menu_bar_new();

  GtkWidget *mainMenu = gtk_menu_new();
  GtkWidget *mMain = gtk_menu_item_new_with_label("Main");
  GtkWidget *meQuit = gtk_menu_item_new_with_label("Quit");

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mMain), mainMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(mainMenu), meQuit);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mMain);

  g_object_set_data(G_OBJECT(meQuit), "app", app);
  g_signal_connect(G_OBJECT(meQuit), "activate", G_CALLBACK(menuhandle_meQuit), NULL);

  GtkWidget *opMenu = gtk_menu_new();
  GtkWidget *mOp = gtk_menu_item_new_with_label("Operations");
  GtkWidget *seperator = gtk_separator_menu_item_new();
  GtkWidget *meSetLauncher = gtk_menu_item_new_with_label("Set launcher applications");
  GtkWidget *meImportFiles = gtk_menu_item_new_with_label("Import files and folders");
  GtkWidget *meEditEntry = gtk_menu_item_new_with_label("Edit ebook details");

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mOp), opMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meSetLauncher);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), seperator);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meImportFiles);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meEditEntry);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mOp);

  g_object_set_data(G_OBJECT(meSetLauncher), "appWindow", window);
  g_object_set_data(G_OBJECT(meSetLauncher), "db", g_object_get_data(G_OBJECT(app), "db"));
  g_signal_connect(G_OBJECT(meSetLauncher), "activate", G_CALLBACK(menuhandle_meSetLauncher), NULL);

  g_object_set_data(G_OBJECT(meEditEntry), "appWindow", window);
  g_object_set_data(G_OBJECT(meEditEntry), "treeview", ebookList);
  g_object_set_data(G_OBJECT(meEditEntry), "db", g_object_get_data(G_OBJECT(app), "db"));
  g_signal_connect(G_OBJECT(meEditEntry), "activate", G_CALLBACK(menuhandle_meEditEntry), NULL);

  g_object_set_data(G_OBJECT(meImportFiles), "appWindow", window);
  g_object_set_data(G_OBJECT(meImportFiles), "statusBar", statusBar);
  g_object_set_data(G_OBJECT(meImportFiles), "progressRevealer", progressRevealer);
  g_object_set_data(G_OBJECT(meImportFiles), "progressBar", progressBar);
  g_object_set_data(G_OBJECT(meImportFiles), "treeview", ebookList);
  g_object_set_data(G_OBJECT(meImportFiles), "db", g_object_get_data(G_OBJECT(app), "db"));
  g_signal_connect(G_OBJECT(meImportFiles), "activate", G_CALLBACK(menuhandle_meImportFiles), NULL);

  g_object_set(G_OBJECT(menu), "margin-bottom", 12, NULL);
  gtk_container_add(GTK_CONTAINER(menuBox), menu);

  // Menu code end -------------------------------------------------------------
  g_object_set_data(G_OBJECT(app), "dataStore", dataStore);

  gtk_widget_show_all(window);
}

//------------------------------------------------------------------------------
// Menu handlers
void menuhandle_meQuit(GtkMenuItem *menuitem, gpointer user_data) {
  g_application_quit(G_APPLICATION(g_object_get_data(G_OBJECT(menuitem), "app")));
}


//------------------------------------------------------------------------------
void menuhandle_meSetLauncher(GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *launcherWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(launcherWindow), true);

  gtk_window_set_title(GTK_WINDOW(launcherWindow), "KISS Ebook Starter - Set launcher applications");
  gtk_window_set_default_size(GTK_WINDOW(launcherWindow), 640, 330);

  //----------------------------------------------------------------------------
  sqlite3* db = g_object_get_data(G_OBJECT(menuitem), "db");

  char *pdfHandler = NULL;
  char *epubHandler = NULL;
  char *mobiHandler = NULL;
  char *chmHandler = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  char **pointer = NULL;

  for (int i = 1; i < 5; ++i) {
    char getHandlerStmt[67];
    sprintf(getHandlerStmt, "SELECT program FROM launcher_applications WHERE format=%d LIMIT 0,1", i);


    rc = sqlite3_exec(db, getHandlerStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      printf("SQL error during selection: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      switch(i) {
        case 1:
          pointer = &pdfHandler;
          break;
        case 2:
          pointer = &epubHandler;
          break;
        case 3:
          pointer = &mobiHandler;
          break;
        case 4:
          pointer = &chmHandler;
        default:
          break;
      }

      get_db_answer_value(&receiveFromDb, "program", pointer);
      free_db_answer(&receiveFromDb);
    }
  }


  //----------------------------------------------------------------------------

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(G_OBJECT(box), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(launcherWindow), box);

  //----------------------------------------------------------------------------

  GtkWidget *labelPDF = gtk_label_new("PDF file handler:");
  gtk_label_set_xalign(GTK_LABEL(labelPDF), 0);
  g_object_set(G_OBJECT(labelPDF), "margin-top", 6, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelPDF);

  GtkWidget *entryPDF = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryPDF), 127);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryPDF), "Name of the application to launch .pdf files.");
  gtk_widget_set_tooltip_text(entryPDF, "Application to open .pdf files.");
  gtk_container_add(GTK_CONTAINER(box), entryPDF);


  GtkWidget *labelEPUB = gtk_label_new("EPUB file handler:");
  gtk_label_set_xalign(GTK_LABEL(labelEPUB), 0);
  g_object_set(G_OBJECT(labelEPUB), "margin-top", 12, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelEPUB);

  GtkWidget *entryEPUB = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryEPUB), 127);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryEPUB), "Name of the application to launch .epub files.");
  gtk_widget_set_tooltip_text(entryEPUB, "Application to open .epub files.");
  gtk_container_add(GTK_CONTAINER(box), entryEPUB);

  GtkWidget *labelMOBI = gtk_label_new("MOBI file handler:");
  gtk_label_set_xalign(GTK_LABEL(labelMOBI), 0);
  g_object_set(G_OBJECT(labelMOBI), "margin-top", 12, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelMOBI);

  GtkWidget *entryMOBI = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryMOBI), 127);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryMOBI), "Name of the application to launch .mobi files.");
  gtk_widget_set_tooltip_text(entryMOBI, "Application to open .mobi files.");
  gtk_container_add(GTK_CONTAINER(box), entryMOBI);


  GtkWidget *labelCHM= gtk_label_new("CHM file handler:");
  gtk_label_set_xalign(GTK_LABEL(labelCHM), 0);
  g_object_set(G_OBJECT(labelCHM), "margin-top", 12, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelCHM);

  GtkWidget *entryCHM = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryCHM), 127);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryCHM), "Name of the application to launch .chm files.");
  gtk_widget_set_tooltip_text(entryCHM, "Application to open .chm files.");
  gtk_container_add(GTK_CONTAINER(box), entryCHM);

  if (pdfHandler != NULL) {
    gtk_entry_set_text(GTK_ENTRY(entryPDF), pdfHandler);
    free(pdfHandler);
  }

  if (epubHandler != NULL) {
    gtk_entry_set_text(GTK_ENTRY(entryEPUB), epubHandler);
    free(epubHandler);
  }

  if (mobiHandler != NULL) {
    gtk_entry_set_text(GTK_ENTRY(entryMOBI), mobiHandler);
    free(mobiHandler);
  }

  if (chmHandler != NULL) {
    gtk_entry_set_text(GTK_ENTRY(entryCHM), chmHandler);
    free(chmHandler);
  }
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin-top", 12, NULL);
  gtk_container_add(GTK_CONTAINER(box), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label("Cancel");
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", launcherWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(launcherWindow_close), NULL);

  GtkWidget *saveButton = gtk_button_new_with_label("Save");
  gtk_container_add(GTK_CONTAINER(buttonBox), saveButton);
  g_object_set_data(G_OBJECT(saveButton), "entryPDF", entryPDF);
  g_object_set_data(G_OBJECT(saveButton), "entryEPUB", entryEPUB);
  g_object_set_data(G_OBJECT(saveButton), "entryMOBI", entryMOBI);
  g_object_set_data(G_OBJECT(saveButton), "entryCHM", entryCHM);
  g_object_set_data(G_OBJECT(saveButton), "rootWindow", launcherWindow);
  g_object_set_data(G_OBJECT(saveButton), "db", g_object_get_data(G_OBJECT(menuitem), "db"));
  g_signal_connect(G_OBJECT(saveButton), "clicked", G_CALLBACK(launcherWindow_save_data), NULL);
  //----------------------------------------------------------------------------

  gtk_window_set_type_hint(GTK_WINDOW(launcherWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(launcherWindow));
  gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entryPDF));
  gtk_widget_show_all(launcherWindow);

  gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(launcherWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(launcherWindow), true);
  gtk_window_set_position(GTK_WINDOW(launcherWindow), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(launcherWindow), true);
}

void launcherWindow_save_data(GtkButton* button, gpointer user_data) {
  GtkEntry *entryPDF = g_object_get_data(G_OBJECT(button), "entryPDF");
  GtkEntry *entryEPUB = g_object_get_data(G_OBJECT(button), "entryEPUB");
  GtkEntry *entryMOBI = g_object_get_data(G_OBJECT(button), "entryMOBI");
  GtkEntry *entryCHM = g_object_get_data(G_OBJECT(button), "entryCHM");
  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");

  const gchar *pdfHandler = gtk_entry_get_text(entryPDF);
  const gchar *epubHandler  = gtk_entry_get_text(entryEPUB);
  const gchar *mobiHandler = gtk_entry_get_text(entryMOBI);
  const gchar *chmHandler = gtk_entry_get_text(entryCHM);

  char key = '\0';
  unsigned int readPos = 0;
  unsigned int writePos = 0;

  char pdfHandle[128];
  char epubHandle[128];
  char mobiHandle[128];
  char chmHandle[128];

  bool pdfHasData = false;
  bool epubHasData = false;
  bool mobiHasData = false;
  bool chmHasData = false;

  for (int i = 0; i < 4; ++i) {
    readPos = 0;
    writePos = 0;
    bool hasData = false;

    while(true) {
      switch (i) {
        case 0:
          key = pdfHandler[readPos++];
          break;
        case 1:
          key = epubHandler[readPos++];
          break;
        case 2:
          key = mobiHandler[readPos++];
          break;
        case 3:
          key = chmHandler[readPos++];
          break;
        default:
          break;
      }

      if (key == '\0') {
        break;
      } else if (!hasData && key == ' ') {
        continue;
      } else {
        hasData = true;
      }

      switch (i) {
        case 0:
          pdfHandle[writePos++] = key;
          pdfHandle[writePos] = '\0';
          pdfHasData = true;
          break;
        case 1:
          epubHandle[writePos++] = key;
          epubHandle[writePos] = '\0';
          epubHasData = true;
          break;
        case 2:
          mobiHandle[writePos++] = key;
          mobiHandle[writePos] = '\0';
          mobiHasData = true;
          break;
        case 3:
          chmHandle[writePos++] = key;
          chmHandle[writePos] = '\0';
          chmHasData = true;
          break;
        default:
          break;
      }
    }
  }

  //----------------------------------------------------------------------------
  char *dbErrorMsg = NULL;
  char clearLauncherStmt[34];
  sprintf(clearLauncherStmt, "DELETE FROM launcher_applications");

  int rc = sqlite3_exec(db, clearLauncherStmt, NULL, NULL, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    printf("Unknown SQL error while clearing launcher applications:%s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  //----------------------------------------------------------------------------

  char *dbStmt = NULL;

  // TODO: Add parameter parsing here which are stored in 'args'

  for (int i = 1; i < 5; ++i) {
    if (i == 1 && pdfHasData) {
      dbStmt = calloc(sizeof(char), 77 + strlen(pdfHandle));
      sprintf(dbStmt, "INSERT OR REPLACE INTO launcher_applications VALUES(NULL, %d, \"%s\", NULL)", i, pdfHandle);
    } else if (i == 2 && epubHasData) {
      dbStmt = calloc(sizeof(char), 77 + strlen(epubHandle));
      sprintf(dbStmt, "INSERT OR REPLACE INTO launcher_applications VALUES(NULL, %d, \"%s\", NULL)", i, epubHandle);
    } else if (i == 3 && mobiHasData) {
      dbStmt = calloc(sizeof(char), 77 + strlen(mobiHandle));
      sprintf(dbStmt, "INSERT OR REPLACE INTO launcher_applications VALUES(NULL, %d, \"%s\", NULL)", i, mobiHandle);
    } else if (i == 4 && chmHasData) {
      dbStmt = calloc(sizeof(char), 77 + strlen(chmHandle));
      sprintf(dbStmt, "INSERT OR REPLACE INTO launcher_applications VALUES(NULL, %d, \"%s\", NULL)", i, chmHandle);
    } else {
      continue;
    }

    if (dbStmt != NULL) {
      int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      if (rc != SQLITE_OK) {
        if (rc == SQLITE_FULL) {
          printf("Cannot add data to the database, the (temporary) disk is full.");
        } else {
          printf("Unknown SQL error: %s\n", dbErrorMsg);
        }

        sqlite3_free(dbErrorMsg);
      }

      free(dbStmt);
    }

  }


  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}

void launcherWindow_close(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}


//------------------------------------------------------------------------------

void menuhandle_meEditEntry(GtkMenuItem *menuitem, gpointer user_data) {
  open_edit_window(G_OBJECT(menuitem));
}

void open_edit_window(GObject *dataItem) {
  GtkWidget *editWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(editWindow), true);

  gtk_window_set_title(GTK_WINDOW(editWindow), "KISS Ebook Starter - Edit ebook details");
  gtk_window_set_default_size(GTK_WINDOW(editWindow), 640, 400);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(G_OBJECT(box), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(editWindow), box);

  GtkWidget *labelPath = gtk_label_new("Path:");
  GtkWidget *labelFileName = gtk_label_new("Filename:");
  GtkWidget *labelFormat = gtk_label_new("Format:");
  GtkWidget *labelAuthor = gtk_label_new("Author:");
  GtkWidget *labelTitle = gtk_label_new("Title:");

  gtk_label_set_xalign(GTK_LABEL(labelPath), 0);
  gtk_label_set_xalign(GTK_LABEL(labelFileName), 0);
  gtk_label_set_xalign(GTK_LABEL(labelFormat), 0);
  gtk_label_set_xalign(GTK_LABEL(labelAuthor), 0);
  gtk_label_set_xalign(GTK_LABEL(labelTitle), 0);

  g_object_set(G_OBJECT(labelPath), "margin-top", 0, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelFileName), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelFormat), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelAuthor), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelTitle), "margin-top", 12, "margin-left", 6, NULL);

  GtkWidget *entryPath = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryPath), 1024);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryPath), "Complete path");
  gtk_widget_set_tooltip_text(entryPath, "Complete path, including the filename. This is not changeable.");
  g_object_set(G_OBJECT(entryPath), "editable", false, NULL);

  GtkWidget *entryFileName = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryFileName), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryFileName), "Filename");
  gtk_widget_set_tooltip_text(entryFileName, "The file name which is not changeable.");
  g_object_set(G_OBJECT(entryFileName), "editable", false, NULL);

  GtkWidget *entryFormat = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryFormat), 5);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryFormat), "Format");
  gtk_widget_set_tooltip_text(entryFormat, "The ebook format which is not changeable.");
  g_object_set(G_OBJECT(entryFormat), "editable", false, NULL);

  GtkWidget *entryAuthor = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryAuthor), 255);
  gtk_widget_set_tooltip_text(entryAuthor, "The ebook author(s) displayed in KISS Ebook.");
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryAuthor), "Author(s)");

  GtkWidget *entryTitle = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryTitle), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryTitle), "Title");
  gtk_widget_set_tooltip_text(entryTitle, "The title of the ebook displayed in KISS Ebook.");

  gtk_container_add(GTK_CONTAINER(box), labelTitle);
  gtk_container_add(GTK_CONTAINER(box), entryTitle);
  gtk_container_add(GTK_CONTAINER(box), labelAuthor);
  gtk_container_add(GTK_CONTAINER(box), entryAuthor);
  gtk_container_add(GTK_CONTAINER(box), labelFileName);
  gtk_container_add(GTK_CONTAINER(box), entryFileName);
  gtk_container_add(GTK_CONTAINER(box), labelPath);
  gtk_container_add(GTK_CONTAINER(box), entryPath);
  gtk_container_add(GTK_CONTAINER(box), labelFormat);
  gtk_container_add(GTK_CONTAINER(box), entryFormat);

  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin-top", 12, NULL);
  gtk_container_add(GTK_CONTAINER(box), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label("Cancel");
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", editWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(edit_entry_close), NULL);

  GtkWidget *saveButton = gtk_button_new_with_label("Save data");
  gtk_container_add(GTK_CONTAINER(buttonBox), saveButton);

  g_object_set_data(G_OBJECT(saveButton), "rootWindow", editWindow);
  g_object_set_data(G_OBJECT(saveButton), "treeview", g_object_get_data(G_OBJECT(dataItem), "treeview"));
  g_object_set_data(G_OBJECT(saveButton), "db", g_object_get_data(G_OBJECT(dataItem), "db"));
  g_object_set_data(G_OBJECT(saveButton), "entryPath", entryPath);
  g_object_set_data(G_OBJECT(saveButton), "entryFileName", entryFileName);
  g_object_set_data(G_OBJECT(saveButton), "entryFormat", entryFormat);
  g_object_set_data(G_OBJECT(saveButton), "entryAuthor", entryAuthor);
  g_object_set_data(G_OBJECT(saveButton), "entryTitle", entryTitle);
  g_signal_connect(G_OBJECT(saveButton), "clicked", G_CALLBACK(edit_entry_save_data), NULL);

  //----------------------------------------------------------------------------
  GtkWidget *treeView = g_object_get_data(G_OBJECT(dataItem), "treeview");
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
  GtkTreeIter iter;
  gchar *formatStr;
  gchar *authorStr;
  gchar *titleStr;

  gtk_tree_selection_get_selected(selection, &model, &iter);
  gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, AUTHOR_COLUMN, &authorStr, TITLE_COLUMN, &titleStr, -1);

  int format = 0;
  if (strcmp(formatStr, "pdf") == 0) {
    format = 1;
  } else if (strcmp(formatStr, "epub") == 0) {
    format = 2;
  } else if (strcmp(formatStr, "mobi") == 0) {
    format = 3;
  } else if (strcmp(formatStr, "chm") == 0) {
    format = 4;
  } else {
    return;
  }

  sqlite3* db = g_object_get_data(G_OBJECT(dataItem), "db");

  char *filePath = NULL;
  char *fileName = NULL;
  char *fileFormat = NULL;
  char *fileAuthor = NULL;
  char *fileTitle = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  char dbStmt[84 + strlen(authorStr) + strlen(titleStr)];
  sprintf(dbStmt, "SELECT * FROM ebook_collection WHERE format=%d AND author=\"%s\" AND title=\"%s\" LIMIT 0,1", format, authorStr, titleStr);

  rc = sqlite3_exec(db, dbStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    printf("SQL error during select: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (receiveFromDb.count == 0) {
      return;
    }

    get_db_answer_value(&receiveFromDb, "path", &filePath);
    get_db_answer_value(&receiveFromDb, "filename", &fileName);
    get_db_answer_value(&receiveFromDb, "format", &fileFormat);
    get_db_answer_value(&receiveFromDb, "author", &fileAuthor);
    get_db_answer_value(&receiveFromDb, "title", &fileTitle);
    free_db_answer(&receiveFromDb);

    char fileType[5];
    if (fileFormat != NULL) {
      switch (atoi(fileFormat)) {
        case 1:
          strcpy(fileType, "pdf");
          break;
        case 2:
          strcpy(fileType, "epub");
          break;
        case 3:
          strcpy(fileType, "mobi");
          break;
        case 4:
          strcpy(fileType, "chm");
          break;
        default:
          break;
      }
    }

    if (filePath != NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryPath), filePath);
    }

    if (fileName != NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryFileName), fileName);
    }

    if (fileFormat != NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryFormat), fileType);
    }

    if (fileAuthor != NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryAuthor), fileAuthor);
    }

    if (fileTitle != NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryTitle), fileTitle);
    }
  }

  g_free(formatStr);
  g_free(authorStr);
  g_free(titleStr);

  //----------------------------------------------------------------------------
  gtk_window_set_type_hint(GTK_WINDOW(editWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(editWindow));
  gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entryTitle));

  gtk_widget_show_all(editWindow);

  gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(dataItem), "appWindow")), GTK_WINDOW(editWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(editWindow), true);
  gtk_window_set_position(GTK_WINDOW(editWindow), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(editWindow), true);
}

void edit_entry_close(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}

void edit_entry_save_data(GtkButton *button, gpointer user_data) {
  GtkTreeView *treeview = g_object_get_data(G_OBJECT(button), "treeview");
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(treeview), "dataStore");

  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");

  GtkEntry *entryPath = g_object_get_data(G_OBJECT(button), "entryPath");
  //GtkEntry *entryFileName = g_object_get_data(G_OBJECT(button), "entryFileName");
  //GtkEntry *entryFormat = g_object_get_data(G_OBJECT(button), "entryFormat");
  GtkEntry *entryAuthor = g_object_get_data(G_OBJECT(button), "entryAuthor");
  GtkEntry *entryTitle = g_object_get_data(G_OBJECT(button), "entryTitle");

  int rc = 0;
  char *dbErrorMsg = NULL;

  const gchar *path = gtk_entry_get_text(entryPath);
  const gchar *author = gtk_entry_get_text(entryAuthor);
  const gchar *title = gtk_entry_get_text(entryTitle);

  //----------------------------------------------------------------------------

  char *authorStripped = malloc((256 + 1) * sizeof(char));
  char *titleStripped = malloc((256 + 1) * sizeof(char));

  char key = '\0';
  int readPos = 0;
  int writePos = 0;
  bool hasValidChar = false;

  while((key = author[readPos++]) != '\0') {
      if (key == '"') {
        if (hasValidChar) {
          key = ' ';
        } else {
          continue;
        }
      } else if (!hasValidChar && key == ' ') {
        continue;
      }

      if (key != ' ') {
        hasValidChar = true;
      }

      authorStripped[writePos++] = key;
  }

  authorStripped[writePos] = '\0';

  //----------------------------------------------------------------------------
  key = '\0';
  readPos = 0;
  writePos = 0;
  hasValidChar = false;

  while((key = title[readPos++]) != '\0') {
      if (key == '"') {
        if (hasValidChar) {
          key = ' ';
        } else {
          continue;
        }
      } else if (!hasValidChar && key == ' ') {
        continue;
      }

      if (key != ' ') {
        hasValidChar = true;
      }

      titleStripped[writePos++] = key;
  }

  titleStripped[writePos] = '\0';


  if (strlen(titleStripped) == 0) {
    // NOTE: Should we add a popup that a title has to be entered in order to save?
    return;
  }

  if (strlen(authorStripped) == 0) {
    sprintf(authorStripped, "Unknown");
  }

  char dbStmt[91 + strlen(authorStripped) + strlen(titleStripped) + strlen(path)];
  sprintf(dbStmt, "UPDATE ebook_collection SET author = trim(\"%s\"), title = trim(\"%s\") WHERE path == \"%s\" LIMIT 0,1", authorStripped, titleStripped, path);

  rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    printf("SQL error during update: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeIter iter;

    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_list_store_set(dataStore, &iter, AUTHOR_COLUMN, authorStripped, TITLE_COLUMN, titleStripped, -1);
  }

  free(authorStripped);
  free(titleStripped);

  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));

  return;
}

//------------------------------------------------------------------------------
void menuhandle_meImportFiles(GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *fileChooserWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(fileChooserWindow), true);

  gtk_window_set_title(GTK_WINDOW(fileChooserWindow), "KISS Ebook Starter - Import files and folders");
  gtk_window_set_default_size(GTK_WINDOW(fileChooserWindow), 640, 400);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(fileChooserWindow), box);

  GtkWidget *chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);

  GtkFileFilter *inputFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(inputFilter, "All supported ebook formats");
  gtk_file_filter_add_pattern(inputFilter, "*.epub");
  gtk_file_filter_add_pattern(inputFilter, "*.pdf");
  gtk_file_filter_add_pattern(inputFilter, "*.mobi");
  gtk_file_filter_add_pattern(inputFilter, "*.chm");

  GtkFileFilter *pdfFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(pdfFilter, "*.pdf");
  gtk_file_filter_add_pattern(pdfFilter, "*.pdf");

  GtkFileFilter *epubFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(epubFilter, "*.epub");
  gtk_file_filter_add_pattern(epubFilter, "*.epub");

  GtkFileFilter *mobiFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(mobiFilter, "*.mobi");
  gtk_file_filter_add_pattern(mobiFilter, "*.mobi");

  GtkFileFilter *chmFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(chmFilter, "*.chm");
  gtk_file_filter_add_pattern(chmFilter, "*.chm");

  gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), true);
  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), true);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), inputFilter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), pdfFilter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), epubFilter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), mobiFilter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), chmFilter);

  gtk_container_add(GTK_CONTAINER(box), chooser);
  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label("Cancel");
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", fileChooserWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(fileChooser_close), NULL);

  GtkWidget *openButton = gtk_button_new_with_label("Open");
  gtk_container_add(GTK_CONTAINER(buttonBox), openButton);
  g_object_set_data(G_OBJECT(openButton), "rootWindow", fileChooserWindow);
  g_object_set_data(G_OBJECT(openButton), "fileChooser", chooser);
  g_object_set_data(G_OBJECT(openButton), "progressRevealer", g_object_get_data(G_OBJECT(menuitem), "progressRevealer"));
  g_object_set_data(G_OBJECT(openButton), "progressBar", g_object_get_data(G_OBJECT(menuitem), "progressBar"));
  g_object_set_data(G_OBJECT(openButton), "statusBar", g_object_get_data(G_OBJECT(menuitem), "statusBar"));
  g_object_set_data(G_OBJECT(openButton), "treeview", g_object_get_data(G_OBJECT(menuitem), "treeview"));
  g_object_set_data(G_OBJECT(openButton), "db", g_object_get_data(G_OBJECT(menuitem), "db"));

  g_signal_connect(G_OBJECT(openButton), "clicked", G_CALLBACK(fileChooser_importFiles), NULL);

  //gtk_window_set_keep_above(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_type_hint(GTK_WINDOW(fileChooserWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(fileChooserWindow));
  gtk_widget_show_all(fileChooserWindow);

  gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(fileChooserWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_modal(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_position(GTK_WINDOW(fileChooserWindow), GTK_WIN_POS_CENTER_ON_PARENT);
}


//------------------------------------------------------------------------------

void fileChooser_close(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}


//------------------------------------------------------------------------------

void fileChooser_importFiles(GtkButton *button, gpointer user_data) {
  // SQLite3 code
  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");
  GtkWidget *rootWindow = g_object_get_data(G_OBJECT(button), "rootWindow");

  GtkWidget *fileChooser = g_object_get_data(G_OBJECT(button), "fileChooser");
  GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(fileChooser));

  if (filenames == NULL) {
    return;
  }

  gtk_widget_destroy(rootWindow);

  GtkWidget *progressRevealer = g_object_get_data(G_OBJECT(button), "progressRevealer");
  gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), true);

  GtkWidget *progressBar = g_object_get_data(G_OBJECT(button), "progressBar");
  GtkWidget *statusBar = g_object_get_data(G_OBJECT(button), "statusBar");
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_object_get_data(G_OBJECT(button), "treeview")));
  guint contextId = 0;

  GSList *item = NULL;

  char message[512];
  unsigned int filesToAdd = g_slist_length(filenames);
  unsigned int filesAdded = 0;
  unsigned int filesError = 0;

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);


  for (unsigned int i = 0; (item = g_slist_nth(filenames, i)) != NULL; ++i) {
    char *fileType = strrchr(item->data, '.');

    DIR *dp = opendir(item->data);

    if (dp != NULL) {
      // Its a folder most likely
      int retVal = read_out_path(item->data, statusBar, contextId, progressBar, i, filesToAdd, model, db);

      if (retVal != -1) {
        filesAdded += retVal;
        filesToAdd += retVal-1;
      }

      closedir(dp);
    } else if (fileType != NULL && strlen(fileType) < 6) {
      if (read_and_add_file_to_model(item->data, true, statusBar, contextId, true, progressBar, i, filesToAdd, true, model, db)) {
        ++filesAdded;
      } else {
        ++filesError;
      }
    } else {
      ++filesError;
    }
  }

  for (unsigned int i = 0; (item = g_slist_nth(filenames, i)) != NULL; ++i) {
    g_free(item->data);
  }

  g_slist_free(filenames);

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), 1.0);

  char errorString[64];
  sprintf(errorString, "%u %s", filesError, filesError == 1 ? "error." : "errors.");

  sprintf(message, "[STATUS] Done importing %u %s%s%s",
    filesAdded, filesAdded == 1 ? "file" : "files", filesError != 0 ? ", " : ".", filesError != 0 ? errorString : "");

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
  gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);

  gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), false);
}


//------------------------------------------------------------------------------

gboolean handle_drag_data(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selData, guint time, gpointer user_data) {
  sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");
  GtkWidget *statusBar = g_object_get_data(G_OBJECT(widget), "status");
  GtkWidget *progressBar = g_object_get_data(G_OBJECT(widget), "progress");
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

  guint contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  //gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");

  gint format = gtk_selection_data_get_format(selData);
  GtkWidget *progressRevealer = g_object_get_data(G_OBJECT(widget), "progressRevealer");

  if (format == 8) {
    gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), true);

    char message[512];

    gtk_drag_dest_unset(widget);
    gchar **uriList = gtk_selection_data_get_uris(selData);

    unsigned int filesToAdd = 0;
    unsigned int filesAdded = 0;
    unsigned int filesError = 0;
    for (int i = 0; uriList && uriList[i]; ++i) {
      ++filesToAdd;
    }

    for (int i = 0; uriList && uriList[i]; ++i) {
      char *cleanedPath = (char*) calloc(strlen(&uriList[i][7]) + 1, sizeof(char));
      char *pathPart = strstr(&uriList[i][7], "%20");
      if (pathPart == NULL) {
        strcpy(cleanedPath, &uriList[i][7]);
      } else {
        int pathSize = 0;
        long int length = 0;
        while (pathPart != NULL) {
          length = pathPart - &uriList[i][7];
          strncat(cleanedPath, &uriList[i][7+pathSize], (pathPart - &uriList[i][7]) - pathSize);
          strcat(cleanedPath, " ");
          pathSize = length+3;
          pathPart = strstr(&pathPart[3], "%20");
        }

        strncat(cleanedPath, &uriList[i][7+pathSize], strlen(uriList[i])-pathSize);
      }

      DIR *dp = opendir(cleanedPath);
      if (dp != NULL) {
        // Its a folder most likely
        int retVal = read_out_path((char*) cleanedPath, statusBar, contextId, progressBar, i, filesToAdd, model, db);

        if (retVal != -1) {
          filesAdded += retVal;
          filesToAdd += retVal-1;
        }

        free(cleanedPath);
        closedir(dp);
      } else if (read_and_add_file_to_model(uriList[i], true, statusBar, contextId, true, progressBar, i, filesToAdd, true, model, db)) {
        ++filesAdded;
      } else {
        ++filesError;
      }
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), 1.0);
    char errorString[64];
    sprintf(errorString, "%u %s", filesError, filesError == 1 ? "error." : "errors.");

    sprintf(message, "[STATUS] Done importing %u %s%s%s",
      filesAdded, filesAdded == 1 ? "file" : "files", filesError != 0 ? ", " : ".", filesError != 0 ? errorString : "");

    gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
    contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
    gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);

    g_strfreev(uriList);

    gtk_drag_finish(context, true, true, time);
    gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(widget);
    gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), false);
  } else {
    gtk_drag_finish(context, true, true, time);
    gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
    gtk_drag_dest_add_uri_targets(widget);
    gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), false);
    return false;
  }

  return true;
}

//------------------------------------------------------------------------------
void handle_launchCommand(GtkWidget* widget) {
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
  GtkTreeIter iter;
  gchar *formatStr;
  gchar *authorStr;
  gchar *titleStr;

  sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");

  gtk_tree_selection_get_selected(selection, &model, &iter);
  gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, AUTHOR_COLUMN, &authorStr, TITLE_COLUMN, &titleStr, -1);

  int format = 0;
  if (strcmp(formatStr, "pdf") == 0) {
    format = 1;
  } else if (strcmp(formatStr, "epub") == 0) {
    format = 2;
  } else if (strcmp(formatStr, "mobi") == 0) {
    format = 3;
  } else if (strcmp(formatStr, "chm") == 0) {
    format = 4;
  } else {
    return;
  }

  //----------------------------------------------------------------------------
  int rc = 0;
  char *dbErrorMsg = 0;
  struct dbAnswer receiveFromDb = {0, NULL, NULL};

  char *filePath = NULL;
  char *launcher = NULL;

  //----------------------------------------------------------------------------
  char dbGetPathStmt[92 + strlen(authorStr) + strlen(titleStr)];
  sprintf(dbGetPathStmt, "SELECT path FROM ebook_collection WHERE format=%d AND author=\"%s\" AND title=\"%s\" LIMIT 0,1", format, authorStr, titleStr);

  g_free(formatStr);
  g_free(authorStr);
  g_free(titleStr);

  rc = sqlite3_exec(db, dbGetPathStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    printf("SQL error during selection: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (!get_db_answer_value(&receiveFromDb, "path", &filePath)) {
      free_db_answer(&receiveFromDb);
      return;
    }

    free_db_answer(&receiveFromDb);
  }

  //----------------------------------------------------------------------------
  char dbGetLauncherStmt[67];
  sprintf(dbGetLauncherStmt, "SELECT program FROM launcher_applications WHERE format=%d LIMIT 0,1", format);

  rc = sqlite3_exec(db, dbGetLauncherStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    printf("SQL error during selection: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (!get_db_answer_value(&receiveFromDb, "program", &launcher)) {
      free(filePath);
      free_db_answer(&receiveFromDb);
      return;
    }

    free_db_answer(&receiveFromDb);
  }

  //----------------------------------------------------------------------------

  char launchString[strlen(launcher) + strlen(filePath) + 2];
  sprintf(launchString, "%s %s", launcher, filePath);

  pid_t child_pid = fork();
  if (child_pid >= 0) {
    if (child_pid == 0) {
      free(filePath);
      free(launcher);

      GtkApplication *app = g_object_get_data(G_OBJECT(widget), "app");
      g_object_unref(g_object_get_data(G_OBJECT(widget), "dataStore"));
      g_object_unref(G_OBJECT(app));
      g_application_quit(G_APPLICATION(app));

      execlp("/bin/sh", "/bin/sh", "-c", launchString, NULL);
      return;
    } else {
      free(filePath);
      free(launcher);
      return;
    }
  }

  free(filePath);
  free(launcher);

  //system(launchString);
  return;
}

void handle_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data) {
  if (gtk_tree_view_column_get_x_offset(column) != 0) {
    return;
  }

  handle_launchCommand(GTK_WIDGET(tree_view));
  return;
}

gboolean handle_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
  bool pressedDelete = false;

  switch (event->keyval) {
    case 65535:
      // Delete key
      pressedDelete = true;
      break;
    case 115:
      // S key
      if (event->state == 20) { // Strg Pressed
        handle_launchCommand(widget);
        return true;
      }
      break;
    case 101:
      // E key
      if (event->state == 20) {
        open_edit_window(G_OBJECT(widget));
        return true;
      }
    default:
      //printf("key pressed: %u - %d\n", event->keyval, event->state);
      return false;
      break;
  }

  if (pressedDelete) {
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
      GtkListStore *dataStore = g_object_get_data(G_OBJECT(widget), "dataStore");
      gchar *formatStr;
      gchar *authorStr;
      gchar *titleStr;

      gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, AUTHOR_COLUMN, &authorStr, TITLE_COLUMN, &titleStr, -1);

      int format = 0;
      if (strcmp(formatStr, "pdf") == 0) {
        format = 1;
      } else if (strcmp(formatStr, "epub") == 0) {
        format = 2;
      } else if (strcmp(formatStr, "mobi") == 0) {
        format = 3;
      } else if (strcmp(formatStr, "chm") == 0) {
        format = 4;
      }

      char *dbErrorMsg = 0;

      sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");
      char stmt[90+strlen(authorStr)+strlen(titleStr)];
      sprintf(stmt, "DELETE FROM ebook_collection WHERE format == %d AND author == \"%s\" AND title == \"%s\" LIMIT 0,1;", format, authorStr, titleStr);
      int rc = sqlite3_exec(db, stmt, NULL, NULL, &dbErrorMsg);

      if (rc != SQLITE_OK) {
        printf("SQL error while deleting: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
      } else {
        gtk_list_store_remove(dataStore, &iter);
      }

      g_free(formatStr);
      g_free(authorStr);
      g_free(titleStr);
    }
  }

  return true;
}


//------------------------------------------------------------------------------
gboolean handle_editing_author(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;

  gtk_tree_model_get_iter_from_string(model, &iter, path);

  gchar *formatStr;
  gchar *authorStr;
  gchar *titleStr;

  gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, AUTHOR_COLUMN, &authorStr, TITLE_COLUMN, &titleStr, -1);

  int format = 0;
  if (strcmp(formatStr, "pdf") == 0) {
    format = 1;
  } else if (strcmp(formatStr, "epub") == 0) {
    format = 2;
  } else if (strcmp(formatStr, "mobi") == 0) {
    format = 3;
  } else if (strcmp(formatStr, "chm") == 0) {
    format = 4;
  }

  int dataLength = strlen(new_text);

  char author[dataLength == 0 ? 8 : dataLength+1];
  if (dataLength == 0) {
    strcpy(author, "Unknown");
  } else {
    strcpy(author, new_text);
  }

  char *dbErrorMsg = 0;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  char stmt[103+strlen(authorStr)+strlen(titleStr)+strlen(author)];
  sprintf(stmt, "UPDATE ebook_collection SET author = \"%s\" WHERE format == %d AND author == \"%s\" AND title == \"%s\" LIMIT 0,1;", author, format, authorStr, titleStr);
  int rc = sqlite3_exec(db, stmt, NULL, NULL, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    printf("SQL error while updating: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    gtk_list_store_set(dataStore, &iter, AUTHOR_COLUMN, author, -1);
  }

  g_free(formatStr);
  g_free(authorStr);
  g_free(titleStr);

  return true;
}


//------------------------------------------------------------------------------
gboolean handle_editing_title(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;

  gtk_tree_model_get_iter_from_string(model, &iter, path);

  gchar *formatStr;
  gchar *authorStr;
  gchar *titleStr;

  gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, AUTHOR_COLUMN, &authorStr, TITLE_COLUMN, &titleStr, -1);

  int format = 0;
  if (strcmp(formatStr, "pdf") == 0) {
    format = 1;
  } else if (strcmp(formatStr, "epub") == 0) {
    format = 2;
  } else if (strcmp(formatStr, "mobi") == 0) {
    format = 3;
  } else if (strcmp(formatStr, "chm") == 0) {
    format = 4;
  } else {
    return false;
  }

  char *dbErrorMsg = 0;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");
  if (strlen(new_text) != 0) {
    char stmt[102+strlen(authorStr)+strlen(titleStr)+strlen(new_text)];
    sprintf(stmt, "UPDATE ebook_collection SET title = \"%s\" WHERE format == %d AND author == \"%s\" AND title == \"%s\" LIMIT 0,1", new_text, format, authorStr, titleStr);
    int rc = sqlite3_exec(db, stmt, NULL, NULL, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      printf("SQL error while updating: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      gtk_list_store_set(dataStore, &iter, TITLE_COLUMN, new_text, -1);
    }
  } else {
    struct dbAnswer dbAnswerData = { 0, NULL, NULL };
    char stmt[101 + strlen(authorStr) + strlen(titleStr)];
    sprintf(stmt, "SELECT filename FROM ebook_collection WHERE format == %d AND author == \"%s\" AND title == \"%s\" LIMIT 0,1", format, authorStr, titleStr);
    int rc = sqlite3_exec(db, stmt, fill_db_answer, (void*) &dbAnswerData, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      printf("SQL error: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      char *fileName = NULL;
      if (get_db_answer_value(&dbAnswerData, "filename", &fileName)) {
        char updateStmt[70+(strlen(fileName)*2)];
        sprintf(updateStmt, "UPDATE ebook_collection SET title = \"%s\" WHERE filename == \"%s\" LIMIT 0,1", fileName, fileName);
        int rc = sqlite3_exec(db, updateStmt, NULL, NULL, &dbErrorMsg);

        if (rc != SQLITE_OK) {
          printf("SQL error while updating: %s\n", dbErrorMsg);
          sqlite3_free(dbErrorMsg);
        } else {
          gtk_list_store_set(dataStore, &iter, TITLE_COLUMN, fileName, -1);
        }

        free(fileName);
      }
    }
  }

  g_free(formatStr);
  g_free(authorStr);
  g_free(titleStr);

  return true;
}

//------------------------------------------------------------------------------

bool read_and_add_file_to_model(char* inputFileName, bool showStatus, GtkWidget* statusBar, unsigned int contextId, bool showProgress, GtkWidget* progressBar, unsigned int index, unsigned int filesToAdd, bool addFileToModel, GtkTreeModel *model, sqlite3* db) {
  GtkTreeIter iter;

  char* fileType = strrchr(inputFileName, '.');
  char lowerFiletype[strlen(fileType)];
  char message[512];

  if (fileType != NULL) {
    char lowercase = '\0';
    int pos = 0;

    while ((lowercase = fileType[pos]) != '\0') {
      lowerFiletype[pos++] = tolower(lowercase);
    }

    lowerFiletype[pos] = '\0';
  }

  char* fileName = &strrchr(inputFileName, '/')[1];
  char* author = NULL;
  char* title = NULL;

  //----------------------------------------------------------------------------

  char *cleanedPath = (char*) calloc(strlen(inputFileName) + 1, sizeof(char));
  char *pathPart = strstr(inputFileName, "%20");
  if (pathPart == NULL) {
    strcpy(cleanedPath, inputFileName);
  } else {
    int pathSize = 0;
    long int length = 0;
    while (pathPart != NULL) {
      length = pathPart - (char*) inputFileName;
      strncat(cleanedPath, &inputFileName[pathSize], (pathPart - (char*) inputFileName)-pathSize);
      strcat(cleanedPath, " ");
      pathSize = length+3;
      pathPart = strstr(&pathPart[3], "%20");
    }

    strncat(cleanedPath, &inputFileName[pathSize], strlen(inputFileName)-pathSize);
  }

  //----------------------------------------------------------------------------

  char* cleanedFileName = (char*) calloc(strlen(fileName) + 1, sizeof(char));
  if (fileName != NULL && fileName[0] != '\0') {
    pathPart = strstr(fileName, "%20");
    if (pathPart == NULL) {
      strcpy(cleanedFileName, fileName);
    } else {
      int pathSize = 0;
      long int length = 0;
      while (pathPart != NULL) {
        length = pathPart - fileName;
        strncat(cleanedFileName, &fileName[pathSize], (pathPart - fileName)-pathSize);
        strcat(cleanedFileName, " ");
        pathSize = length+3;
        pathPart = strstr(&pathPart[3], "%20");
      }

      strncat(cleanedFileName, &fileName[pathSize], strlen(fileName)-pathSize-strlen(fileType));
    }
  }

  // Update the UI ---------------------------------------------------------
  if (showStatus && statusBar != NULL) {
    sprintf(message, "[PARSING] %s", cleanedFileName);
    gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
    gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);
  }

  //------------------------------------------------------------------------

  fileInfo fileData = {NULL, NULL, 0};

  int format = 0;

  if (fileType != NULL) {
    bool retVal = false;

    if (strcmp(lowerFiletype, ".pdf") == 0) {
      format = 1;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_pdf(&cleanedPath[7], &fileData);
      } else {
        retVal = read_pdf(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".epub") == 0) {
      format = 2;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_epub(&cleanedPath[7], &fileData);
      } else {
        retVal = read_epub(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".mobi") == 0) {
      format = 3;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_mobi(&cleanedPath[7], &fileData);
      } else {
        retVal = read_mobi(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".chm") == 0) {
      format = 4;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_chm(&cleanedPath[7], &fileData);
      } else {
        retVal = read_chm(cleanedPath, &fileData);
      }
    }

    if (!retVal) {
      printf("Error reading out file \"%s\"", cleanedPath);
      free(cleanedFileName);
      free(cleanedPath);
      return false;
    }
  }

  for (int i = 0; i < fileData.count; ++i) {
    switch(fileData.types[i]) {
      case 1:
        if (title == NULL) {
          title = malloc(sizeof(char) * (strlen(fileData.entries[i]) + 1));
          strncpy(title, &fileData.entries[i][6], strlen(fileData.entries[i]) - 7);
          title[strlen(fileData.entries[i]) - 7] = '\0';
        }
        break;
      case 2:
        if (author == NULL) {
          author = malloc(sizeof(char) * (strlen(fileData.entries[i]) + 1));
          strncpy(author, &fileData.entries[i][7], strlen(fileData.entries[i]) - 8);
          author[strlen(fileData.entries[i]) - 8] = '\0';
        }
        break;
      default:
        break;
    }
  }

  if (format != 0) {

    //--------------------------------------------------------------------------
    bool dbOkay = true;
    char *dbErrorMsg = NULL;

    int additionSize = 1 + (title == NULL ? strlen(cleanedFileName) : strlen(title))
                       + (author == NULL ? 7 : strlen(author))
                       + strlen(cleanedPath) + strlen(cleanedFileName) + 13;

    char *dbStm = (char*) calloc((54 + additionSize), sizeof(char));
    sprintf(dbStm, "INSERT INTO ebook_collection VALUES (NULL,%d,\"%s\",\"%s\",\"%s\",\"%s\");",
      format,
      author == NULL ? "Unknown" : author,
      title == NULL ? cleanedFileName : title,
      cleanedPath,
      cleanedFileName
    );


    int rc = sqlite3_exec(db, dbStm, NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      dbOkay = false;
      if (rc == SQLITE_FULL) {
        printf("Cannot add data to the database, the (temporary) disk is full.");
      } else {
        printf("Unknown SQL error: %s\n", dbErrorMsg);
      }

      sqlite3_free(dbErrorMsg);
    }

    free(dbStm);

    //--------------------------------------------------------------------------
    if (dbOkay) {
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      gtk_list_store_set(GTK_LIST_STORE(model), &iter,
        FORMAT_COLUMN, &lowerFiletype[1],
        AUTHOR_COLUMN, author == NULL ? "Unknown" : author,
        TITLE_COLUMN, title == NULL ? cleanedFileName : title,
        -1
      );

      if (showStatus && statusBar != NULL) {
        sprintf(message, "[ADDED] %s", cleanedFileName);
        gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
        gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);
      }
    } else if (showStatus && statusBar != NULL) {
      sprintf(message, "[ERROR IMPORTING INTO DB] %s", cleanedFileName);
      gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
      gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);
    }

    if (progressBar != NULL) {
      if (showProgress) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), (index * 1.0) / filesToAdd);
      } else if (!showProgress) {
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progressBar));
      }
    }
    //--------------------------------------------------------------------------
    // Cleanup
    free(fileData.types);
    for (int i = 0; i < fileData.count; ++i) {
      free(fileData.entries[i]);
    }

    free(fileData.entries);

    free(title);
    free(author);
    free(cleanedFileName);
    free(cleanedPath);

    //--------------------------------------------------------------------------
    gtk_main_iteration_do(true);
    return true;
  }

  free(title);
  free(author);
  free(cleanedFileName);
  free(cleanedPath);

  return false;
}


int read_out_path(char* pathRootData, GtkWidget *statusBar, guint contextId, GtkWidget *progressBar, unsigned int i, unsigned int filesToAdd, GtkTreeModel *model, sqlite3* db) {
  char pathRoot[strlen(pathRootData)+1];
  strcpy(pathRoot, pathRootData);

  DIR *dp = opendir(pathRoot);
  struct dirent *ep;
  int filesAdded = 0;

  if (dp != NULL) {
    while((ep = readdir(dp))) {
      if (ep->d_type == 4) {
        // its a subfolder

        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) {
          continue;
        }

        int pathSize = strlen(pathRoot)+strlen(ep->d_name)+2;
        char *completePath = malloc((pathSize) * sizeof(char));
        sprintf(completePath, "%s/%s", pathRoot, ep->d_name);

        filesAdded += read_out_path(completePath, statusBar, contextId, progressBar, i, filesToAdd, model, db);
        free(completePath);
      } else if (ep->d_type == 8) {
        char *folderFileType = strrchr(ep->d_name, '.');
        if (folderFileType != NULL) {
          char folderLowerFileType[strlen(folderFileType)+1];

          char key = '\0';
          unsigned int pos = 0;

          while ((key = folderFileType[pos]) != '\0') {
            folderLowerFileType[pos++] = tolower(key);
          }

          folderLowerFileType[pos] = '\0';

          int pathSize = strlen(pathRoot)+strlen(ep->d_name)+2;
          char *completePath = malloc((pathSize) * sizeof(char));
          sprintf(completePath, "%s/%s", pathRoot, ep->d_name);

          if (strcmp(folderLowerFileType, ".pdf") == 0 || strcmp(folderLowerFileType, ".epub") == 0 || strcmp(folderLowerFileType, ".mobi") == 0 || strcmp(folderLowerFileType, ".chm") == 0) {
            if (read_and_add_file_to_model(completePath, true, statusBar, contextId, false, progressBar, i, filesToAdd, true, model, db)) {
              gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progressBar));
              ++filesAdded;
            }
          }

          free(completePath);
        }
      }

    }

    closedir(dp);
  }

  return filesAdded;
}

//------------------------------------------------------------------------------

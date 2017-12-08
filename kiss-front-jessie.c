#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "kisslib.h" // read_pdf, read_mobi, read_chm, read_epub

// GTK3
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <glib.h>

//SQLite3
#include <sqlite3.h>

//GNU
#include <dirent.h> // readdir, closedir
#include <unistd.h> // access, fork, execlp
#include <sys/stat.h>

// gettext
#include <locale.h>
#include <libintl.h>

//------------------------------------------------------------------------------
enum {
  STARTUP_COLUMN,
  FORMAT_COLUMN,
  AUTHOR_COLUMN,
  TITLE_COLUMN,
  CATEGORY_COLUMN,
  TAGS_COLUMN,
  FILENAME_COLUMN,
  PRIORITY_COLUMN,
  READ_COLUMN,
  N_COLUMNS
};

enum {
   SEARCH_AUTHOR_TITLE,
   SEARCH_FORMAT,
   SEARCH_AUTHORS,
   SEARCH_TITLE,
   SEARCH_CATEGORY,
   SEARCH_TAGS,
   SEARCH_FILENAME
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

typedef struct argumentStore {
  unsigned int count;
  char **arguments;
} argumentStore;

typedef struct radioGroup {
  int key;
  int count;
  GtkWidget **items;
} radioGroup;

typedef struct dimensions {
  int width;
  int height;
} dimensions;

//------------------------------------------------------------------------------

void run(GtkApplication*, gpointer);
void handle_resize(GtkWidget*, GdkRectangle*, gpointer);

void ask_setup_window(sqlite3*);
void ask_setup_window_open(GtkButton*, gpointer);
void ask_setup_window_close(GtkButton*, gpointer);
void ask_reader_window_close(GtkButton*, gpointer);
void ask_reader_window_save(GtkButton*, gpointer);
void menuhandle_setup_dialog(GtkMenuItem*, gpointer);
void open_setup_reader_dialog(GObject*);

void handle_launchCommand(GtkWidget*);
void handle_column_change(GtkTreeView*, gpointer);
gboolean handle_drag_data(GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData*, guint, gpointer);
gboolean handle_key_press(GtkWidget*, GdkEventKey*, gpointer);
gboolean handle_editing_author(GtkCellRendererText*, gchar*, gchar*, gpointer);
gboolean handle_editing_title(GtkCellRendererText*, gchar*, gchar*, gpointer);
gboolean handle_editing_category(GtkCellRendererText*, gchar*, gchar*, gpointer);
gboolean handle_editing_tags(GtkCellRendererText*, gchar*, gchar*, gpointer);
void handle_editing_priority_value(GtkAdjustment*, gpointer);
void handle_editing_priority(GtkAdjustment*, gpointer);
void handle_toggle_read(GtkCellRendererToggle*, gchar*, gpointer);

void delete_selected_entry_from_db_and_store(GtkWidget*);
void handle_row_activated(GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer);
void handle_sort_column(GtkTreeViewColumn*, gpointer);

void search_icon_click(GtkEntry*, GtkEntryIconPosition, GdkEvent*, gpointer);
void search_handle_search(GtkEntry*, gpointer);

bool read_and_add_file_to_model(char*, bool, GtkWidget*, unsigned int, bool, GtkWidget*, unsigned int, unsigned int, bool, GtkTreeModel*, sqlite3*, bool);
int read_out_path(char*, GtkWidget*, unsigned int, GtkWidget*, unsigned int, unsigned int, GtkTreeModel*, sqlite3*, bool);
bool retrieve_handler_arguments(struct argumentStore*, const char *);
void free_handler_arguments(struct argumentStore*);

void menuhandle_meQuit(GtkMenuItem*, gpointer);
void menuhandle_meImportFiles(GtkMenuItem*, gpointer);
void open_importFiles_window(GObject*);

void menuhandle_meSetLauncher(GtkMenuItem*, gpointer);
void open_launcher_window(GObject*);

void menuhandle_meEditEntry(GtkMenuItem*, gpointer);
void open_edit_window(GObject*);
void edit_entry_close(GtkButton*, gpointer);
void edit_entry_save_data(GtkButton*, gpointer);

void menuhandle_meToggleColumn(GtkCheckMenuItem*, gpointer);
void menuhandle_meResetColumns(GtkMenuItem*, gpointer);

void menuhandle_mOptions(GtkMenuItem*, gpointer);
void open_options_window(GObject*, gpointer);
void options_toggle_column(GtkToggleButton*, gpointer);
void optionsWindow_close(GtkButton*, gpointer);
void optionsWindow_save(GtkButton*, gpointer);

void launcherWindow_save_data(GtkButton*, gpointer);
void launcherWindow_close(GtkButton*, gpointer);

void fileChooser_close(GtkButton*, gpointer);
void fileChooser_importFiles(GtkButton*, gpointer);

// Db related
int trim(const char*, char**, bool);
int add_db_data_to_store(void*, int, char**, char**);
void clearAndUpdateDataStore(GtkListStore*, sqlite3*);

int fill_db_answer(void*, int, char**, char**);
void free_db_answer(struct dbAnswer*);
bool get_db_answer_value(struct dbAnswer*, const char[], char**);

//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  setlocale(LC_ALL, "");
  // NOTE: Translations have to be placed in + "translations/de_DE/LC_MESSAGES/KISSebook.mo" for german de_DE translation
  char *localPath = malloc(2560 * sizeof(char));
  getcwd(localPath, 2048);
  sprintf(localPath, "%s/translations/", localPath);

  bindtextdomain("KISSebook", localPath);
  textdomain("KISSebook");
  free(localPath);

  char homePath[512];
  sprintf(homePath, "%s/.kissebook", getenv("HOME"));
  mkdir(homePath, S_IRWXU);
  if (chdir(homePath) != 0) {
    fprintf(stderr, "Cannot set application working directory to \"%s\", quitting.\n\n", homePath);
    return -1;
  }

  GtkApplication *app;
  int status;

  // SQLite3 code
  sqlite3 *db;
  char *dbErrorMsg = NULL;
  char *dbStmt = NULL;

  int rc = sqlite3_open("kisslib.db", &db);

  if (rc) {
    sqlite3_close(db);
    fprintf(stderr, "Error opening sqlite3 database \"kisslib.db\" file. Exiting.");
    return 1;
  } else {
    char columnOrder[64];
    char indexString[3];
    columnOrder[0] = '\0';

    for (int i = 0; i < N_COLUMNS; ++i) {
      sprintf(indexString, "%d,", i);
      strcat(columnOrder, indexString);
    }

    dbStmt = sqlite3_mprintf("CREATE TABLE IF NOT EXISTS ebook_collection ( \
        'id' INTEGER PRIMARY KEY ASC, \
        'format' INTEGER, \
        'author' TEXT, \
        'title' TEXT, \
        'path' TEXT, \
        'filename' TEXT, \
        'read' INTEGER DEFAULT 0, \
        'priority' INTEGER DEFAULT 0, \
        'category' TEXT DEFAULT NULL, \
        'tags' TEXT DEFAULT NULL \
      ); \
      CREATE TABLE IF NOT EXISTS launcher_applications ( \
        'id' INTEGER PRIMARY KEY ASC, \
        'format' INTEGER, \
        'program' TEXT DEFAULT NULL, \
        'args' TEXT DEFAULT NULL \
      ); \
      CREATE TABLE IF NOT EXISTS options ( \
        'id' INTEGER PRIMARY KEY ASC, \
        'option' TEXT, \
        'type' TEXT, \
        'value' TEXT \
      ); \
      INSERT OR IGNORE INTO options VALUES(1, 'visible_columns', 'text', 'format,author'); \
      INSERT OR IGNORE INTO options VALUES(2, 'overwrite_on_import', 'bool', 'true'); \
      INSERT OR IGNORE INTO options VALUES(3, 'first_startup', 'bool', 'true'); \
      INSERT OR IGNORE INTO options VALUES(4, 'sort_on_startup', 'int', -1); \
      INSERT OR IGNORE INTO options VALUES(5, 'column_order', 'text', '%s'); \
      INSERT OR IGNORE INTO options VALUES(6, 'window_dimensions', 'text', '640;400;'); \
    ", columnOrder);

    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
        sqlite3_close(db);

        if (rc == SQLITE_READONLY) {
          fprintf(stderr, "Error opening sqlite3 database for writing. Exiting.\n");
        }

        return 1;
      }
    } else {
      fprintf(stderr, "SQL out of memory error, cant setup the database. Exiting.\n");
      return 1;
    }
  }

  //----------------------------------------------------------------------------
  // Read out window sizes;
  struct dimensions *window_dimensions = malloc(sizeof(dimensions));

  //----------------------------------------------------------------------------
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option == 'window_dimensions' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, could not save 'window_dimensions', error: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    char *sizeString = NULL;
    if (get_db_answer_value(&receiveFromDb, "value", &sizeString)) {
      char *width = strtok(sizeString, ";");
      char *height = strtok(NULL, ";");

      window_dimensions->width = atoi(width);
      window_dimensions->height = atoi(height);
      free(sizeString);
    }

    free_db_answer(&receiveFromDb);
  }

  //----------------------------------------------------------------------------

  app = gtk_application_new("net.dwrox.kiss", G_APPLICATION_HANDLES_OPEN);
  g_signal_connect(app, "activate", G_CALLBACK(run), NULL);

  g_object_set_data(G_OBJECT(app), "db", db);
  g_object_set_data(G_OBJECT(app), "window_dimensions", window_dimensions);
  status = g_application_run(G_APPLICATION(app), argc, argv);

  int *columnIds = g_object_get_data(G_OBJECT(app), "columnIds");
  free(columnIds);

  g_object_unref(g_object_get_data(G_OBJECT(app), "dataStore"));
  g_object_unref(app);

  // Save window sizes;
  dbStmt = sqlite3_mprintf("UPDATE options SET value='%d;%d;' WHERE option == 'window_dimensions'", window_dimensions->width, window_dimensions->height);
  if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error, could not save 'window_dimensions', error: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
      }
  }

  sqlite3_free(dbStmt);


  // Close db
  rc = sqlite3_exec(db, "VACUUM", NULL, NULL, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, could not run 'vacuum', error: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  sqlite3_close(db);

  free(window_dimensions);

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
    if (argv[i] != NULL) {
      answerData->data = (char**) realloc(answerData->data, sizeof(char*) * (answerData->count + 1));
      answerData->data[i] = malloc(sizeof(char) * (strlen(argv[i]) + 1));
      strcpy(answerData->data[i], argv[i]);

      answerData->columnNames = (char**) realloc(answerData->columnNames, sizeof(char*) * (answerData->count + 1));
      answerData->columnNames[i] = malloc(sizeof(char) * (strlen(columnNames[i]) + 1));
      strcpy(answerData->columnNames[i], columnNames[i]);

      ++answerData->count;
    }
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
  char *category = NULL;
  char *tags = NULL;
  char *filename = NULL;
  bool read = false;
  int priority = 0;

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
    } else if (strcmp(columnNames[i], "category") == 0) {
      if (argv[i] != NULL) {
        category = calloc((strlen(argv[i]) + 1), sizeof(char));
        strcpy(category, argv[i]);
      }
    } else if (strcmp(columnNames[i], "tags") == 0) {
      if (argv[i] != NULL) {
        tags = calloc((strlen(argv[i]) + 1), sizeof(char));
        strcpy(tags, argv[i]);
      }
    } else if (strcmp(columnNames[i], "filename") == 0) {
      filename = calloc((strlen(argv[i]) + 1), sizeof(char));
      strcpy(filename, argv[i]);
    } else if (strcmp(columnNames[i], "priority") == 0) {
      priority = atoi(argv[i]);
    } else if (strcmp(columnNames[i], "read") == 0) {
      read = (gboolean) atoi(argv[i]);
    }
  }

  gtk_list_store_insert_with_values(store, &iter, -1,
    FORMAT_COLUMN, formatString,
    AUTHOR_COLUMN, author == NULL ? "Unknown" : author,
    TITLE_COLUMN, title,
    CATEGORY_COLUMN, category == NULL ? "" : category,
    TAGS_COLUMN, tags == NULL ? "" : tags,
    FILENAME_COLUMN, filename,
    PRIORITY_COLUMN, priority,
    READ_COLUMN, read,
    -1
  );

  free(author);
  free(title);
  free(category);
  free(tags);
  free(filename);

  return 0;
}

void run(GtkApplication *app, gpointer user_data) {

  GtkWidget *window = gtk_application_window_new(app);
  g_signal_connect(G_OBJECT(window), "size-allocate", G_CALLBACK(handle_resize), app);
  gtk_window_set_title(GTK_WINDOW(window), gettext("KISS Ebook Starter"));

  struct dimensions *window_dimensions = g_object_get_data(G_OBJECT(app), "window_dimensions");

  gtk_window_set_default_size(GTK_WINDOW(window), window_dimensions->width, window_dimensions->height);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  GtkWidget *grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(window), grid);


  //----------------------------------------------------------------------------

  GtkListStore *dataStore = gtk_list_store_new(
    N_COLUMNS,
    GDK_TYPE_PIXBUF,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_INT,
    G_TYPE_BOOLEAN
  );

  //----------------------------------------------------------------------------
  // Read entries from db into list store
  //----------------------------------------------------------------------------
  sqlite3* db = g_object_get_data(G_OBJECT(app), "db");

  char *dbErrorMsg = NULL;
  int rc = sqlite3_exec(db, "SELECT format, author, title, category, tags, priority, filename, read FROM ebook_collection;", add_db_data_to_store, (void*) dataStore, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  //----------------------------------------------------------------------------

  GtkWidget *menuBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(G_OBJECT(menuBox), "margin", 10, "margin-top", 5, "margin-bottom", 5, NULL);
  gtk_grid_attach(GTK_GRID(grid), menuBox, 0, 0, 10, 1);

  //----------------------------------------------------------------------------

  GtkWidget *ebookList = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dataStore));

  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(ebookList), true);
  gtk_widget_set_hexpand(ebookList, true);
  gtk_widget_set_vexpand(ebookList, true);
  gtk_window_set_focus(GTK_WINDOW(window), ebookList);

  gtk_tree_view_set_search_column(GTK_TREE_VIEW(ebookList), TITLE_COLUMN);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(ebookList), GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_drag_dest_set(ebookList, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets(ebookList);

  GtkAdjustment *vadjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(ebookList));
  GtkAdjustment *hadjustment = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(ebookList));
  GtkWidget *scrollWin = gtk_scrolled_window_new(hadjustment, vadjustment);
  g_object_set(G_OBJECT(scrollWin), "margin-left", 10, "margin-right", 10, NULL);
  gtk_grid_attach(GTK_GRID(grid), scrollWin, 0, 2, 10, 1);
  gtk_container_add(GTK_CONTAINER(scrollWin), ebookList);

  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrollWin), 800);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrollWin), 400);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  //gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scrollWin), false);

  GtkWidget* progressRevealer = gtk_revealer_new();
  gtk_revealer_set_transition_duration(GTK_REVEALER(progressRevealer), 2250);
  gtk_revealer_set_transition_type(GTK_REVEALER(progressRevealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_grid_attach(GTK_GRID(grid), progressRevealer, 0, 3, 10, 1);

  GtkWidget *progressBar = gtk_progress_bar_new();
  g_object_set(G_OBJECT(progressBar), "margin-top", 8, "margin-bottom", 5, "margin-left", 15, "margin-right", 15, NULL);
  gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progressBar), true);
  gtk_container_add(GTK_CONTAINER(progressRevealer), progressBar);

  GtkWidget *statusBar = gtk_statusbar_new();
  gtk_grid_attach(GTK_GRID(grid), statusBar, 0, 4, 10, 1);
  guint contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
  gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, gettext("Welcome to KISS Ebook"));

  g_signal_connect(G_OBJECT(ebookList), "row-activated", G_CALLBACK (handle_row_activated), NULL);
  g_signal_connect(G_OBJECT(ebookList), "key_press_event", G_CALLBACK (handle_key_press), NULL);
  g_signal_connect(G_OBJECT(ebookList), "drag_data_received", G_CALLBACK(handle_drag_data), NULL);
  g_object_set_data(G_OBJECT(ebookList), "app", app);
  g_object_set_data(G_OBJECT(ebookList), "appWindow", window);
  g_object_set_data(G_OBJECT(ebookList), "db", db);
  g_object_set_data(G_OBJECT(ebookList), "dataStore", dataStore);
  g_object_set_data(G_OBJECT(ebookList), "status", statusBar);
  g_object_set_data(G_OBJECT(ebookList), "progress", progressBar);
  g_object_set_data(G_OBJECT(ebookList), "progressRevealer", progressRevealer);
  g_object_set_data(G_OBJECT(ebookList), "treeview", ebookList);

  //----------------------------------------------------------------------------
  GtkWidget *searchSelection = gtk_combo_box_text_new();
  g_object_set(G_OBJECT(searchSelection), "margin", 10, "margin-top", 0, NULL);
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Author and title"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Format"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Authors"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Title"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Category"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Tags"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(searchSelection), gettext("Filename"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(searchSelection), 0);
  gtk_grid_attach(GTK_GRID(grid), searchSelection, 0, 1, 1, 1);
  g_object_set_data(G_OBJECT(ebookList), "searchSelection", searchSelection);

  GtkWidget *searchEntry = gtk_entry_new();
  g_object_set(G_OBJECT(searchEntry), "margin", 10, "margin-top", 0, NULL);
  gtk_entry_set_placeholder_text(GTK_ENTRY(searchEntry), gettext("Search inside ebook list..."));
  gtk_entry_set_max_length(GTK_ENTRY(searchEntry), 64);
  gtk_entry_set_icon_from_icon_name(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_PRIMARY, "system-search");
  gtk_entry_set_icon_from_icon_name(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_SECONDARY, "edit-clear");
  gtk_entry_set_icon_activatable(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_PRIMARY, true);
  gtk_entry_set_icon_activatable(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_SECONDARY, true);
  gtk_entry_set_icon_tooltip_text(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_PRIMARY, gettext("Start search using input"));
  gtk_entry_set_icon_tooltip_text(GTK_ENTRY(searchEntry), GTK_ENTRY_ICON_SECONDARY, gettext("Clear search and restore entries to default."));
  gtk_grid_attach(GTK_GRID(grid), searchEntry, 1, 1, 9, 1);
  g_signal_connect(G_OBJECT(searchEntry), "icon-press", G_CALLBACK(search_icon_click), ebookList);
  g_signal_connect(G_OBJECT(searchEntry), "activate", G_CALLBACK(search_handle_search), ebookList);


  //----------------------------------------------------------------------------

  GtkIconTheme *iconTheme = gtk_icon_theme_get_default();
  GError *iconError = NULL;
  GtkIconInfo *infoOpenIcon = gtk_icon_theme_lookup_icon(iconTheme, "document-open", 24, GTK_ICON_LOOKUP_NO_SVG);
  GdkPixbuf *infoIcon = NULL;
  if (infoOpenIcon != NULL) {
    infoIcon = gtk_icon_info_load_icon(infoOpenIcon, &iconError);
  }

  //----------------------------------------------------------------------------

  int *columnIds = malloc(sizeof(int) * N_COLUMNS);

  for (int i = 0; i < N_COLUMNS; ++i) {
    columnIds[i] = i;
  }

  g_object_set_data(G_OBJECT(window), "columnIds", columnIds);
  g_object_set_data(G_OBJECT(app), "treeview", ebookList);
  g_object_set_data(G_OBJECT(app), "columnIds", columnIds);
  g_object_set_data(G_OBJECT(app), "dataStore", dataStore);

  //----------------------------------------------------------------------------

  GtkCellRenderer *imageRenderer = NULL;
  if (infoIcon != NULL) {
    imageRenderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(G_OBJECT(imageRenderer), "pixbuf", infoIcon, NULL);
    gtk_cell_renderer_set_padding(imageRenderer, 5, 8);
  } else {
    imageRenderer = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_set_padding(imageRenderer, 5, 8);
  }

  GtkTreeViewColumn *columnOpen = gtk_tree_view_column_new_with_attributes(gettext("Open"), imageRenderer, NULL, STARTUP_COLUMN, NULL);
  g_object_set_data(G_OBJECT(columnOpen), "id", &columnIds[0]);
  gtk_tree_view_column_set_min_width(columnOpen, 40);
  gtk_tree_view_column_set_resizable(columnOpen, false);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnOpen);


  GtkCellRenderer *ebookListFormat = gtk_cell_renderer_text_new();
  gtk_cell_renderer_set_padding(ebookListFormat, 5, 8);

  GtkTreeViewColumn *columnFormat = gtk_tree_view_column_new_with_attributes(gettext("Format"), ebookListFormat, "text", FORMAT_COLUMN, NULL);
  gtk_tree_view_column_set_resizable(columnFormat, false);
  gtk_tree_view_column_set_min_width(columnFormat, 80);
  g_object_set_data(G_OBJECT(columnFormat), "id", &columnIds[1]);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnFormat);

  GtkCellRenderer *ebookListAuthor = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListAuthor), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListAuthor), "edited", G_CALLBACK(handle_editing_author), ebookList);

  GtkTreeViewColumn *columnAuthor = gtk_tree_view_column_new_with_attributes(gettext("Author"), ebookListAuthor, "text", AUTHOR_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnAuthor, 110);
  gtk_tree_view_column_set_resizable(columnAuthor, true);
  g_object_set_data(G_OBJECT(columnAuthor), "id", &columnIds[2]);
  gtk_cell_renderer_set_padding(ebookListAuthor, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnAuthor);

  GtkCellRenderer *ebookListTitle = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListTitle), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListTitle), "edited", G_CALLBACK(handle_editing_title), ebookList);

  GtkTreeViewColumn *columnTitle = gtk_tree_view_column_new_with_attributes(gettext("Title"), ebookListTitle, "text", TITLE_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnTitle, 240);
  gtk_tree_view_column_set_expand(columnTitle, true);
  gtk_tree_view_column_set_resizable(columnTitle, true);
  g_object_set_data(G_OBJECT(columnTitle), "id", &columnIds[3]);
  gtk_cell_renderer_set_padding(ebookListTitle, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnTitle);


  //----------------------------------------------------------------------------

  GtkCellRenderer *ebookListCategory = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListCategory), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListCategory), "edited", G_CALLBACK(handle_editing_category), ebookList);
  GtkTreeViewColumn *columnCategory = gtk_tree_view_column_new_with_attributes(gettext("Category"), ebookListCategory, "text", CATEGORY_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnCategory, 180);
  gtk_tree_view_column_set_resizable(columnCategory, true);
  g_object_set_data(G_OBJECT(columnCategory), "id", &columnIds[4]);
  gtk_cell_renderer_set_padding(ebookListCategory, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnCategory);

  GtkCellRenderer *ebookListTags = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(ebookListTags), "editable", true, NULL);
  g_signal_connect(G_OBJECT(ebookListTags), "edited", G_CALLBACK(handle_editing_tags), ebookList);
  GtkTreeViewColumn *columnTags = gtk_tree_view_column_new_with_attributes(gettext("Tags"), ebookListTags, "text", TAGS_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnTags, 180);
  gtk_tree_view_column_set_resizable(columnTags, true);
  g_object_set_data(G_OBJECT(columnTags), "id", &columnIds[5]);
  gtk_cell_renderer_set_padding(ebookListTags, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnTags);

  GtkCellRenderer *ebookListFilename = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *columnFilename = gtk_tree_view_column_new_with_attributes(gettext("Filename"), ebookListFilename, "text", FILENAME_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnFilename, 180);
  gtk_tree_view_column_set_resizable(columnFilename, true);
  g_object_set_data(G_OBJECT(columnFilename), "id", &columnIds[6]);
  gtk_cell_renderer_set_padding(ebookListFilename, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnFilename);

  GtkAdjustment *priorityAdjustment = gtk_adjustment_new(0, -10, 10, 1, 5, 0);
  GtkCellRenderer *ebookListPriority = gtk_cell_renderer_spin_new();
  g_object_set(G_OBJECT(ebookListPriority), "editable", true, NULL);
  g_object_set(G_OBJECT(ebookListPriority), "digits", 0, NULL);
  g_object_set(G_OBJECT(ebookListPriority), "adjustment", priorityAdjustment, NULL);

  GtkTreeViewColumn *columnPriority = gtk_tree_view_column_new_with_attributes(gettext("Priority"), ebookListPriority, "text", PRIORITY_COLUMN, NULL);
  gtk_tree_view_column_set_min_width(columnPriority, 100);
  g_object_set_data(G_OBJECT(columnPriority), "id", &columnIds[7]);
  gtk_cell_renderer_set_padding(ebookListPriority, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnPriority);
  g_signal_connect(G_OBJECT(priorityAdjustment), "value-changed", G_CALLBACK(handle_editing_priority_value), ebookList);
  g_signal_connect(G_OBJECT(priorityAdjustment), "changed", G_CALLBACK(handle_editing_priority), ebookList);

  GtkCellRenderer *ebookListRead = gtk_cell_renderer_toggle_new();
  GtkTreeViewColumn *columnRead = gtk_tree_view_column_new_with_attributes(gettext("Read"), ebookListRead, "active", READ_COLUMN, NULL);
  gtk_cell_renderer_toggle_set_activatable(GTK_CELL_RENDERER_TOGGLE(ebookListRead), true);
  g_signal_connect(G_OBJECT(ebookListRead), "toggled", G_CALLBACK(handle_toggle_read), ebookList);
  gtk_tree_view_column_set_fixed_width(columnRead, 60);
  g_object_set_data(G_OBJECT(columnRead), "id", &columnIds[8]);
  gtk_cell_renderer_set_padding(ebookListRead, 5, 8);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ebookList), columnRead);

  //----------------------------------------------------------------------------
  // Sorting of columns or highlight enabled
  gtk_tree_view_column_set_clickable(columnOpen, true);
  gtk_tree_view_column_set_clickable(columnFormat, true);
  gtk_tree_view_column_set_clickable(columnAuthor, true);
  gtk_tree_view_column_set_clickable(columnTitle, true);
  gtk_tree_view_column_set_clickable(columnCategory, true);
  gtk_tree_view_column_set_clickable(columnTags, true);
  gtk_tree_view_column_set_clickable(columnFilename, true);
  gtk_tree_view_column_set_clickable(columnPriority, true);
  gtk_tree_view_column_set_clickable(columnRead, true);

  // Reorderable
  gtk_tree_view_column_set_reorderable(columnOpen, true);
  gtk_tree_view_column_set_reorderable(columnFormat, true);
  gtk_tree_view_column_set_reorderable(columnAuthor, true);
  gtk_tree_view_column_set_reorderable(columnTitle, true);
  gtk_tree_view_column_set_reorderable(columnCategory, true);
  gtk_tree_view_column_set_reorderable(columnTags, true);
  gtk_tree_view_column_set_reorderable(columnFilename, true);
  gtk_tree_view_column_set_reorderable(columnPriority, true);
  gtk_tree_view_column_set_reorderable(columnRead, true);

  // Column and table signals
  g_signal_connect(G_OBJECT(columnFormat), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnAuthor), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnTitle), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnCategory), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnTags), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnFilename), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnPriority), "clicked", G_CALLBACK(handle_sort_column), ebookList);
  g_signal_connect(G_OBJECT(columnRead), "clicked", G_CALLBACK(handle_sort_column), ebookList);

  g_signal_connect(G_OBJECT(ebookList), "columns-changed", G_CALLBACK(handle_column_change), NULL);

  // The main menu -------------------------------------------------------------
  // TODO: Should the main menu use images?
  // NOTE: https://developer.gnome.org/gtk3/stable/GtkImageMenuItem.html#GtkImageMenuItem--image

  GtkWidget *menu = gtk_menu_bar_new();

  GtkWidget *mainMenu = gtk_menu_new();
  GtkWidget *mMain = gtk_menu_item_new_with_label(gettext("Main"));
  GtkWidget *meQuit = gtk_menu_item_new_with_label(gettext("Quit"));

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mMain), mainMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(mainMenu), meQuit);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mMain);

  g_object_set_data(G_OBJECT(meQuit), "app", app);
  g_object_set_data(G_OBJECT(meQuit), "appWindow", window);
  g_signal_connect(G_OBJECT(meQuit), "activate", G_CALLBACK(menuhandle_meQuit), NULL);

  GtkWidget *opMenu = gtk_menu_new();
  GtkWidget *mOp = gtk_menu_item_new_with_label(gettext("Operations"));
  GtkWidget *seperator = gtk_separator_menu_item_new();
  GtkWidget *meSetupLauncher = gtk_menu_item_new_with_label(gettext("Setup launcher applications"));
  GtkWidget *meSetLauncher = gtk_menu_item_new_with_label(gettext("Set launcher applications and parameters"));
  GtkWidget *meImportFiles = gtk_menu_item_new_with_label(gettext("Import files and folders"));
  GtkWidget *meEditEntry = gtk_menu_item_new_with_label(gettext("Edit ebook details"));

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mOp), opMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meSetupLauncher);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meSetLauncher);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), seperator);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meImportFiles);
  gtk_menu_shell_append(GTK_MENU_SHELL(opMenu), meEditEntry);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mOp);

  g_object_set_data(G_OBJECT(meSetupLauncher), "db", db);
  g_signal_connect(G_OBJECT(meSetupLauncher), "activate", G_CALLBACK(menuhandle_setup_dialog), NULL);

  g_object_set_data(G_OBJECT(meSetLauncher), "appWindow", window);
  g_object_set_data(G_OBJECT(meSetLauncher), "db", db);
  g_signal_connect(G_OBJECT(meSetLauncher), "activate", G_CALLBACK(menuhandle_meSetLauncher), NULL);

  g_object_set_data(G_OBJECT(meEditEntry), "appWindow", window);
  g_object_set_data(G_OBJECT(meEditEntry), "treeview", ebookList);
  g_object_set_data(G_OBJECT(meEditEntry), "db", db);
  g_signal_connect(G_OBJECT(meEditEntry), "activate", G_CALLBACK(menuhandle_meEditEntry), NULL);

  g_object_set_data(G_OBJECT(meImportFiles), "appWindow", window);
  g_object_set_data(G_OBJECT(meImportFiles), "status", statusBar);
  g_object_set_data(G_OBJECT(meImportFiles), "progressRevealer", progressRevealer);
  g_object_set_data(G_OBJECT(meImportFiles), "progress", progressBar);
  g_object_set_data(G_OBJECT(meImportFiles), "treeview", ebookList);
  g_object_set_data(G_OBJECT(meImportFiles), "db", db);
  g_signal_connect(G_OBJECT(meImportFiles), "activate", G_CALLBACK(menuhandle_meImportFiles), NULL);

  g_object_set_data(G_OBJECT(window), "menuSetLauncher", GTK_MENU_ITEM(meSetLauncher));
  g_object_set_data(G_OBJECT(window), "menuImportFiles", GTK_MENU_ITEM(meImportFiles));

  GtkWidget *viewMenu = gtk_menu_new();
  GtkWidget *mView = gtk_menu_item_new_with_label(gettext("View"));
  GtkWidget *viewColumns = gtk_menu_new();
  GtkWidget *meSubColumns = gtk_menu_item_new_with_label(gettext("Show or hide columns..."));
  GtkWidget *meShowFormat = gtk_check_menu_item_new_with_label(gettext("Format"));
  GtkWidget *meShowAuthor = gtk_check_menu_item_new_with_label(gettext("Author"));
  GtkWidget *meShowCategory = gtk_check_menu_item_new_with_label(gettext("Category"));
  GtkWidget *meShowTags = gtk_check_menu_item_new_with_label(gettext("Tags"));
  GtkWidget *meShowFilename = gtk_check_menu_item_new_with_label(gettext("Filename"));
  GtkWidget *meShowPriority = gtk_check_menu_item_new_with_label(gettext("Priority"));
  GtkWidget *meShowRead = gtk_check_menu_item_new_with_label(gettext("Read state"));
  GtkWidget *seperator_two = gtk_separator_menu_item_new();
  GtkWidget *meResetColumns = gtk_menu_item_new_with_label(gettext("Reset column order"));

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mView), viewMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), meSubColumns);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(meSubColumns), viewColumns);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowFormat);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowAuthor);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowCategory);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowTags);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowFilename);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowPriority);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewColumns), meShowRead);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), seperator_two);
  gtk_menu_shell_append(GTK_MENU_SHELL(viewMenu), meResetColumns);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mView);

  g_object_set_data(G_OBJECT(meShowFormat), "db", db);
  g_object_set_data(G_OBJECT(meShowAuthor), "db", db);
  g_object_set_data(G_OBJECT(meShowCategory), "db", db);
  g_object_set_data(G_OBJECT(meShowTags), "db", db);
  g_object_set_data(G_OBJECT(meShowFilename), "db", db);
  g_object_set_data(G_OBJECT(meShowPriority), "db", db);
  g_object_set_data(G_OBJECT(meShowRead), "db", db);

  g_signal_connect(G_OBJECT(meShowFormat), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnFormat);
  g_signal_connect(G_OBJECT(meShowAuthor), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnAuthor);
  g_signal_connect(G_OBJECT(meShowCategory), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnCategory);
  g_signal_connect(G_OBJECT(meShowFilename), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnFilename);
  g_signal_connect(G_OBJECT(meShowPriority), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnPriority);
  g_signal_connect(G_OBJECT(meShowTags), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnTags);
  g_signal_connect(G_OBJECT(meShowRead), "toggled", G_CALLBACK(menuhandle_meToggleColumn), columnRead);

  g_signal_connect(G_OBJECT(meResetColumns), "activate", G_CALLBACK(menuhandle_meResetColumns), ebookList);


  GtkWidget *optionsMenu = gtk_menu_new();
  GtkWidget *mOptions = gtk_menu_item_new_with_label(gettext("Options"));
  GtkWidget *mEditOptions = gtk_menu_item_new_with_label(gettext("Edit options"));
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(mOptions), optionsMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(optionsMenu), mEditOptions);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), mOptions);

  g_object_set_data(G_OBJECT(mEditOptions), "meShowFormat", meShowFormat);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowAuthor", meShowAuthor);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowCategory", meShowCategory);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowTags", meShowTags);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowFilename", meShowFilename);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowPriority", meShowPriority);
  g_object_set_data(G_OBJECT(mEditOptions), "meShowRead", meShowRead);

  g_signal_connect(G_OBJECT(mEditOptions), "activate", G_CALLBACK(menuhandle_mOptions), db);

  g_object_set(G_OBJECT(menu), "margin-bottom", 12, NULL);
  gtk_container_add(GTK_CONTAINER(menuBox), menu);
  // Menu code end -------------------------------------------------------------

  //----------------------------------------------------------------------------
  // Retrieve which columns are shown

  gtk_tree_view_column_set_visible(columnFormat, false);
  gtk_tree_view_column_set_visible(columnAuthor, false);
  gtk_tree_view_column_set_visible(columnCategory, false);
  gtk_tree_view_column_set_visible(columnTags, false);
  gtk_tree_view_column_set_visible(columnPriority, false);
  gtk_tree_view_column_set_visible(columnFilename, false);
  gtk_tree_view_column_set_visible(columnRead, false);

  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='visible_columns' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot read out column setting: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    char *visibleColumnsString = NULL;

    char *columnNames[] = {
      "format",
      "author",
      "category",
      "tags",
      "filename",
      "priority",
      "read"
    };

    if (get_db_answer_value(&receiveFromDb, "value", &visibleColumnsString)) {
      char *stringPart = NULL;
      stringPart = strtok(visibleColumnsString, ",");
      while (stringPart != NULL) {

        for (int i = 0; i < N_COLUMNS; ++i) {
          if (strncmp(stringPart, columnNames[i], strlen(columnNames[i])) == 0) {
            switch (i) {
              case 0:
                gtk_tree_view_column_set_visible(columnFormat, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowFormat), true);
                break;
              case 1:
                gtk_tree_view_column_set_visible(columnAuthor, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowAuthor), true);
                break;
              case 2:
                gtk_tree_view_column_set_visible(columnCategory, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowCategory), true);
                break;
              case 3:
                gtk_tree_view_column_set_visible(columnTags, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowTags), true);
                break;
              case 4:
                gtk_tree_view_column_set_visible(columnFilename, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowFilename), true);
                break;
              case 5:
                gtk_tree_view_column_set_visible(columnPriority, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowPriority), true);
                break;
              case 6:
                gtk_tree_view_column_set_visible(columnRead, true);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(meShowRead), true);
                break;
              default:
                break;
            }
            break;
          }
        }

        stringPart = strtok(NULL, ",");
      }

      free(visibleColumnsString);
    }
  }

  free_db_answer(&receiveFromDb);

  //----------------------------------------------------------------------------

  gtk_widget_show_all(window);

  //"first_startup" check
  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='first_startup' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot read out startup setting: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    char *startupValue = NULL;

    if (get_db_answer_value(&receiveFromDb, "value", &startupValue)) {
      if (strcmp(startupValue, "true") == 0) {
        ask_setup_window(db);
      }
    }

    free(startupValue);
  }

  free_db_answer(&receiveFromDb);

  //----------------------------------------------------------------------------
  //"sort_on_startup" check
  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='sort_on_startup' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot read out sort on startup setting: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    char *sortOnStartup = NULL;

    if (get_db_answer_value(&receiveFromDb, "value", &sortOnStartup)) {
      int sortOnColumn = atoi(sortOnStartup);
      if (sortOnColumn && sortOnColumn != -1 && sortOnColumn > 0 && sortOnColumn < N_COLUMNS) {
        GtkTreeViewColumn *sortColumn = gtk_tree_view_get_column(GTK_TREE_VIEW(ebookList), sortOnColumn);
        GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(ebookList));
        int index = g_list_index(columns, (gpointer) sortColumn);
        g_list_free(columns);

        gtk_tree_view_column_set_sort_order(sortColumn, GTK_SORT_ASCENDING);
        gtk_tree_view_column_set_sort_column_id(sortColumn, index-1);
        gtk_tree_view_column_set_sort_indicator(sortColumn, true);
        g_signal_emit_by_name(sortColumn, "clicked", NULL);
      }
    }

    free(sortOnStartup);
    free_db_answer(&receiveFromDb);
  }


  //----------------------------------------------------------------------------
  // Restore column order from database
  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option == 'column_order' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot read out sort on startup setting: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    char *columnOrderString = NULL;

    if (get_db_answer_value(&receiveFromDb, "value", &columnOrderString)) {
      int orderId = 0;
      int orderIndex = 0;

      GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(ebookList));
      int columnsSize = g_list_length(columns)-1;
      GtkTreeViewColumn *previousColumn;

      char *orderPart = strtok(columnOrderString, ",");
      while (orderPart != NULL) {
        orderId = atoi(orderPart);

        if (orderIndex == 0) {
          gtk_tree_view_move_column_after(GTK_TREE_VIEW(ebookList), GTK_TREE_VIEW_COLUMN(g_list_nth_data(columns, orderId)),  NULL);
        } else if (orderId < N_COLUMNS && orderId < columnsSize) {
          gtk_tree_view_move_column_after(GTK_TREE_VIEW(ebookList), GTK_TREE_VIEW_COLUMN(g_list_nth_data(columns, orderId)),  previousColumn);
        }

        orderPart = strtok(NULL, ",");
        ++orderIndex;
        previousColumn = GTK_TREE_VIEW_COLUMN(g_list_nth_data(columns, orderId));
      }

       g_list_free(columns);
    }

    free(columnOrderString);
  }

  free_db_answer(&receiveFromDb);
}

//------------------------------------------------------------------------------

void ask_setup_window(sqlite3 *db) {
  GtkWidget *askSetupWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(askSetupWindow), true);

  gtk_window_set_title(GTK_WINDOW(askSetupWindow), gettext("KISS Ebook Starter - Setup"));
  gtk_window_set_default_size(GTK_WINDOW(askSetupWindow), 640, 260);

  GtkWidget *askBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(G_OBJECT(askBox), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(askSetupWindow), askBox);

  GtkWidget *labelQuestion= gtk_label_new(gettext("It's the first time you start KISS Ebook.\n\nDo you need help to setup launcher applications in order to open ebooks from inside KISS Ebook?\n\nKISS Ebook will now search for suitable applications already installed on your system.\nThen you can choose your favorite application for each filetype.\n\nIf you want to change your selections, you can, at any time, customize these applications in the menu\n\"Operations\" > \"Setup launcher applications\"."));
  g_object_set(G_OBJECT(labelQuestion), "margin", 20, NULL);
  gtk_label_set_justify(GTK_LABEL(labelQuestion), GTK_JUSTIFY_CENTER);
  gtk_container_add(GTK_CONTAINER(askBox), labelQuestion);

  //----------------------------------------------------------------------------

  GtkWidget *buttonAskBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonAskBox), "margin", 0, "margin-top", 24, NULL);
  gtk_container_add(GTK_CONTAINER(askBox), buttonAskBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonAskBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *noButton = gtk_button_new_with_label(gettext("No, thank you."));
  gtk_container_add(GTK_CONTAINER(buttonAskBox), noButton);
  g_object_set_data(G_OBJECT(noButton), "rootWindow", askSetupWindow);
  g_object_set_data(G_OBJECT(noButton), "db", db);
  g_signal_connect(G_OBJECT(noButton), "clicked", G_CALLBACK(ask_setup_window_close), NULL);

  GtkWidget *okButton = gtk_button_new_with_label(gettext("Please help me!"));
  gtk_container_add(GTK_CONTAINER(buttonAskBox), okButton);
  g_object_set_data(G_OBJECT(okButton), "rootWindow", askSetupWindow);
  g_object_set_data(G_OBJECT(okButton), "db", db);
  g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(ask_setup_window_open), NULL);

  //----------------------------------------------------------------------------
  gtk_window_set_type_hint(GTK_WINDOW(askSetupWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(askSetupWindow));
  gtk_widget_show_all(askSetupWindow);

  gtk_window_set_destroy_with_parent(GTK_WINDOW(askSetupWindow), true);
  gtk_window_set_modal(GTK_WINDOW(askSetupWindow), true);
  gtk_window_set_position(GTK_WINDOW(askSetupWindow), GTK_WIN_POS_CENTER_ON_PARENT);
}

//------------------------------------------------------------------------------

void handle_resize(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data) {
  int w, h;
  gtk_window_get_size(GTK_WINDOW(widget), &w, &h);

  struct dimensions *window_dimensions = g_object_get_data(G_OBJECT(user_data), "window_dimensions");
  window_dimensions->width = w;
  window_dimensions->height = h;
}

//------------------------------------------------------------------------------

void open_setup_reader_dialog(GObject* dataItem) {
  gtk_widget_destroy(GTK_WIDGET(g_object_get_data(dataItem, "rootWindow")));

  char calibreStr[64];
  strcpy(calibreStr, gettext("Calibre"));
  char coolreaderStr[64];
  strcpy(coolreaderStr, gettext("Cool Reader 3"));
  char evinceStr[64];
  strcpy(evinceStr, gettext("Evince - Document Viewer"));
  char easyebookStr[64];
  strcpy(easyebookStr, gettext("Easy eBook Viewer"));
  char fbreaderStr[64];
  strcpy(fbreaderStr, gettext("FBReader ebook reader"));
  char lucidorStr[64];
  strcpy(lucidorStr, gettext("LUCIDOR ebook reader"));
  char xchmStr[64];
  strcpy(xchmStr, gettext("xCHM (X chm viewer)"));

  char ebookReaderList[21][64] = {
    "ebook-viewer", "1;2;3;4", "Calibre",
    "cr3", "1;2;3;4;", "Cool Reader 3",
    "evince", "1;", "Evince - Document Viewer",
    "easy-ebook-viewer", "2;", "Easy eBook Viewer",
    "fbreader", "2;3;4;", "FBReader ebook reader",
    "lucidor", "2;", "LUCIDOR ebook reader",
    "xchm", "4;", "xCHM (X chm viewer)"
  };

  strcpy(ebookReaderList[2], calibreStr);
  strcpy(ebookReaderList[5], coolreaderStr);
  strcpy(ebookReaderList[8], evinceStr);
  strcpy(ebookReaderList[11], easyebookStr);
  strcpy(ebookReaderList[14], fbreaderStr);
  strcpy(ebookReaderList[17], lucidorStr);
  strcpy(ebookReaderList[20], xchmStr);

  char pathString[256];

  GtkWidget *askReaderWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(askReaderWindow), true);

  gtk_window_set_title(GTK_WINDOW(askReaderWindow), gettext("KISS Ebook Starter - Setup launcher applications"));
  gtk_window_set_default_size(GTK_WINDOW(askReaderWindow), 640, 300);

  GtkWidget *askBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(G_OBJECT(askBox), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(askReaderWindow), askBox);

  GtkWidget *labelNote = gtk_label_new(gettext("All available ebook file types in KISS Ebook are listed below.\nAdditionally, a list of detected applications which are suitable for at least one file type is presented.\n\nYou can select one application for each ebook type. The application will then be used to open ebooks with this file format.\n\nAfter this initial setup, it's possible to change your selection at any time.\n\nA more specific configuration, like a special command for a viewer in\n\"Operations\" > \"Set launcher applications\"\nand provide parameters."));
  g_object_set(G_OBJECT(labelNote), "margin", 20, NULL);
  gtk_label_set_justify(GTK_LABEL(labelNote), GTK_JUSTIFY_CENTER);
  gtk_container_add(GTK_CONTAINER(askBox), labelNote);

  //----------------------------------------------------------------------------

  GtkWidget *grid = gtk_grid_new();
  g_object_set(G_OBJECT(grid), "margin", 25, NULL);
  gtk_container_add(GTK_CONTAINER(askBox), grid);

  char noneStr[32];
  strcpy(noneStr, gettext("None"));

  //----------------------------------------------------------------------------

GtkWidget *labelPDF = gtk_label_new(gettext("PDF file handler:"));
  gtk_widget_set_halign(GTK_WIDGET(labelPDF), GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), labelPDF, 0, 0, 5, 1);

  GtkWidget *boxPDF = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  g_object_set(G_OBJECT(boxPDF), "margin", 14, "margin-right", 120, NULL);
  gtk_grid_attach(GTK_GRID(grid), boxPDF, 0, 1, 5, 1);
  GtkWidget *radioPDF = gtk_radio_button_new_with_label(NULL, noneStr);
  gtk_container_add(GTK_CONTAINER(boxPDF), radioPDF);


  GtkWidget *labelEPUB = gtk_label_new(gettext("EPUB file handler:"));
  gtk_widget_set_halign(GTK_WIDGET(labelEPUB), GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), labelEPUB, 6, 0, 5, 1);

  GtkWidget *boxEPUB = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  g_object_set(G_OBJECT(boxEPUB), "margin", 14, "margin-right", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), boxEPUB, 6, 1, 5, 1);

  GtkWidget *radioEPUB = gtk_radio_button_new_with_label(NULL, noneStr);
  gtk_container_add(GTK_CONTAINER(boxEPUB), radioEPUB);


  GtkWidget *labelMOBI = gtk_label_new(gettext("MOBI file handler:"));
  g_object_set(G_OBJECT(labelMOBI), "margin-top", 30, NULL);
  gtk_widget_set_halign(GTK_WIDGET(labelMOBI), GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), labelMOBI, 0, 2, 5, 1);

  GtkWidget *boxMOBI = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  g_object_set(G_OBJECT(boxMOBI), "margin", 14, "margin-right", 120, NULL);
  gtk_grid_attach(GTK_GRID(grid), boxMOBI, 0, 3, 5, 1);

  GtkWidget *radioMOBI = gtk_radio_button_new_with_label(NULL, noneStr);
  gtk_container_add(GTK_CONTAINER(boxMOBI), radioMOBI);


  GtkWidget *labelCHM = gtk_label_new(gettext("CHM file handler:"));
  g_object_set(G_OBJECT(labelCHM), "margin-top", 30, NULL);
  gtk_widget_set_halign(GTK_WIDGET(labelCHM), GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), labelCHM, 6, 2, 3, 1);

  GtkWidget *boxCHM = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  g_object_set(G_OBJECT(boxCHM), "margin", 14, "margin-right", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), boxCHM, 6, 3, 5, 1);

  GtkWidget *radioCHM = gtk_radio_button_new_with_label(NULL, noneStr);
  gtk_container_add(GTK_CONTAINER(boxCHM), radioCHM);

  //----------------------------------------------------------------------------

  int intVal = 0;

  GtkWidget **radioButtons = NULL;
  int radioCount = 0;
  struct radioGroup **radioGroups = malloc(sizeof(radioGroup*) * 4);

  for (int i = 0; i < 4; ++i) {
    radioGroups[i] = malloc(sizeof(radioGroup));
    radioGroups[i]->count = 0;
    radioGroups[i]->items = NULL;
    radioGroups[i]->key = i + 1;
  }

  for (int i = 0; i < 7; ++i) {
    sprintf(pathString, "/usr/bin/%s", ebookReaderList[i*3]);
    fprintf(stdout, "Checking for: %s ", ebookReaderList[(i*3)+2]);
    if (access(pathString, X_OK) == 0) {
      fprintf(stdout, "- available.\n");

      char *strKey = strtok(ebookReaderList[(i*3)+1], ";");
      intVal = -1;
      while (strKey != NULL) {
        intVal = atoi(strKey);
        switch(intVal) {
          case 1:
            radioButtons = (GtkWidget**) realloc(radioButtons, sizeof(GtkWidget*) * (radioCount + 1));
            radioButtons[radioCount] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radioPDF), ebookReaderList[(i*3)+2]);
            gtk_container_add(GTK_CONTAINER(boxPDF), radioButtons[radioCount]);
            break;
          case 2:
            radioButtons = (GtkWidget**) realloc(radioButtons, sizeof(GtkWidget*) * (radioCount + 1));
            radioButtons[radioCount] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radioEPUB), ebookReaderList[(i*3)+2]);
            gtk_container_add(GTK_CONTAINER(boxEPUB), radioButtons[radioCount]);
            break;
          case 3:
            radioButtons = (GtkWidget**) realloc(radioButtons, sizeof(GtkWidget*) * (radioCount + 1));
            radioButtons[radioCount] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radioMOBI), ebookReaderList[(i*3)+2]);
            gtk_container_add(GTK_CONTAINER(boxMOBI), radioButtons[radioCount]);
            break;
          case 4:
            radioButtons = (GtkWidget**) realloc(radioButtons, sizeof(GtkWidget*) * (radioCount + 1));
            radioButtons[radioCount] = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radioCHM), ebookReaderList[(i*3)+2]);
            gtk_container_add(GTK_CONTAINER(boxCHM), radioButtons[radioCount]);
            break;
          default:
            strKey = strtok(NULL, ";");
            continue;
            break;
        }


        radioGroups[intVal-1]->items = (GtkWidget**) realloc(radioGroups[intVal-1]->items, sizeof(GtkWidget*) * (radioGroups[intVal-1]->count + 1));
        radioGroups[intVal-1]->items[radioGroups[intVal-1]->count] = radioButtons[radioCount];
        g_object_set_data(G_OBJECT(radioButtons[radioCount]), "group", (void*) radioGroups[intVal-1]);
        ++radioGroups[intVal-1]->count;
        ++radioCount;

        strKey = strtok(NULL, ";");
      }
    } else {
      fprintf(stdout, "- not present.\n");
    }
  }

  //----------------------------------------------------------------------------

  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin", 0, "margin-top", 24, NULL);
  gtk_container_add(GTK_CONTAINER(askBox), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label(gettext("Cancel setup"));
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "radioButtons", radioButtons);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", askReaderWindow);
  g_object_set_data(G_OBJECT(cancelButton), "db", g_object_get_data(G_OBJECT(dataItem), "db"));
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(ask_reader_window_close), NULL);

  GtkWidget *saveButton = gtk_button_new_with_label(gettext("Save selections"));
  gtk_container_add(GTK_CONTAINER(buttonBox), saveButton);
  g_object_set_data(G_OBJECT(saveButton), "radioButtons", radioButtons);
  g_object_set_data(G_OBJECT(saveButton), "rootWindow", askReaderWindow);
  g_object_set_data(G_OBJECT(saveButton), "db", g_object_get_data(G_OBJECT(dataItem), "db"));
  g_signal_connect(G_OBJECT(saveButton), "clicked", G_CALLBACK(ask_reader_window_save), radioGroups);

  //----------------------------------------------------------------------------
  gtk_window_set_type_hint(GTK_WINDOW(askReaderWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(askReaderWindow));
  gtk_widget_show_all(askReaderWindow);

  //gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(fileChooserWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(askReaderWindow), true);
  gtk_window_set_modal(GTK_WINDOW(askReaderWindow), true);
  gtk_window_set_position(GTK_WINDOW(askReaderWindow), GTK_WIN_POS_CENTER_ON_PARENT);
}

//------------------------------------------------------------------------------

void ask_setup_window_open(GtkButton *button, gpointer user_data) {
  open_setup_reader_dialog(G_OBJECT(button));

}

void ask_setup_window_close(GtkButton *button, gpointer user_data) {
  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");
  gtk_widget_destroy(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "rootWindow")));

  char *dbErrorMsg;
  //int rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup' LIMIT 0,1;", NULL, NULL, &dbErrorMsg);
  int rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup';", NULL, NULL, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot set first startup done: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }
}

void menuhandle_setup_dialog(GtkMenuItem *menuitem, gpointer user_data) {
  open_setup_reader_dialog(G_OBJECT(menuitem));
}

//------------------------------------------------------------------------------

void ask_reader_window_close(GtkButton *button, gpointer user_data) {
  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");

  char *dbErrorMsg;
  //int rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup' LIMIT 0,1;", NULL, NULL, &dbErrorMsg);
  int rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup';", NULL, NULL, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot set first startup done: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  GtkWidget **radioButtons = (GtkWidget**) (g_object_get_data(G_OBJECT(button), "radioButtons"));

  gtk_widget_destroy(GTK_WIDGET(g_object_get_data(G_OBJECT(button), "rootWindow")));
  free(radioButtons);
}

void ask_reader_window_save(GtkButton *button, gpointer user_data) {
  GtkWidget **radioButtons = (GtkWidget**) (g_object_get_data(G_OBJECT(button), "radioButtons"));
  struct radioGroup **radioGroups = (radioGroup**) user_data;

  char pdfViewerStr[128];
  char epubViewerStr[128];
  char mobiViewerStr[128];
  char chmViewerStr[128];

  char calibreStr[64];
  strcpy(calibreStr, gettext("Calibre"));
  char coolreaderStr[64];
  strcpy(coolreaderStr, gettext("Cool Reader 3"));
  char evinceStr[64];
  strcpy(evinceStr, gettext("Evince - Document Viewer"));
  char easyebookStr[64];
  strcpy(easyebookStr, gettext("Easy eBook Viewer"));
  char fbreaderStr[64];
  strcpy(fbreaderStr, gettext("FBReader ebook reader"));
  char lucidorStr[64];
  strcpy(lucidorStr, gettext("LUCIDOR ebook reader"));
  char xchmStr[64];
  strcpy(xchmStr, gettext("xCHM (X chm viewer)"));

  char ebookReaderList[21][64] = {
    "ebook-viewer", "1;2;3;4", "Calibre",
    "cr3", "1;2;3;4;", "Cool Reader 3",
    "evince", "1;", "Evince - Document Viewer",
    "easy-ebook-viewer", "2;", "Easy eBook Viewer",
    "fbreader", "2;3;4;", "FBReader ebook reader",
    "lucidor", "2;", "LUCIDOR ebook reader",
    "xchm", "4;", "xCHM (X chm viewer)"
  };

  strcpy(ebookReaderList[2], calibreStr);
  strcpy(ebookReaderList[5], coolreaderStr);
  strcpy(ebookReaderList[8], evinceStr);
  strcpy(ebookReaderList[11], easyebookStr);
  strcpy(ebookReaderList[14], fbreaderStr);
  strcpy(ebookReaderList[17], lucidorStr);
  strcpy(ebookReaderList[20], xchmStr);

  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");

  char clearLauncherStmt[34];
  sprintf(clearLauncherStmt, "DELETE FROM launcher_applications");

  int rc = sqlite3_exec(db, clearLauncherStmt, NULL, NULL, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Unknown SQL error while clearing launcher applications: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  char *pointer = NULL;

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < radioGroups[i]->count; ++j) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radioGroups[i]->items[j]))) {
        pointer = NULL;

        switch (radioGroups[i]->key) {
          case 1:
            pointer = pdfViewerStr;
            break;
          case 2:
            pointer = epubViewerStr;
            break;
          case 3:
            pointer = mobiViewerStr;
            break;
          case 4:
            pointer = chmViewerStr;
            break;
          default:
            continue;
            break;
        }

        if (pointer != NULL) {
          strcpy(pointer, gtk_button_get_label(GTK_BUTTON(radioGroups[i]->items[j])));

          bool hasReader = false;

          for (int k = 0; k < 7; ++k) {
            if (strcmp(pointer, ebookReaderList[(k * 3) + 2]) == 0) {
              strcpy(pointer, ebookReaderList[k * 3]);
              hasReader = true;
              break;
            }
          }

          if (hasReader) {
            char *dbStmt = NULL;

            dbStmt = sqlite3_mprintf("INSERT OR REPLACE INTO launcher_applications VALUES(%d, %d, '%q', NULL);", radioGroups[i]->key, radioGroups[i]->key, pointer);

            if (dbStmt != NULL) {
              int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
              if (rc != SQLITE_OK) {
                if (rc == SQLITE_FULL) {
                  fprintf(stderr, "Cannot add data to the database, the (temporary) disk is full.");
                } else {
                  fprintf(stderr, "Unknown SQL error while adding launcher application: %s\n", dbErrorMsg);
                }

                sqlite3_free(dbErrorMsg);
              }

              sqlite3_free(dbStmt);
            }
          }
        }

      }

    }

    free(radioGroups[i]->items);
  }

  //rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup' LIMIT 0,1;", NULL, NULL, &dbErrorMsg);
  rc = sqlite3_exec(db, "UPDATE options SET value='false' WHERE option == 'first_startup';", NULL, NULL, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error, cannot set first startup done: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
  free(radioButtons);
}


//------------------------------------------------------------------------------
// Menu handlers
void menuhandle_meQuit(GtkMenuItem *menuitem, gpointer user_data) {
  gtk_widget_destroy(GTK_WIDGET(g_object_get_data(G_OBJECT(menuitem), "appWindow")));
  g_application_quit(G_APPLICATION(g_object_get_data(G_OBJECT(menuitem), "app")));
}


//------------------------------------------------------------------------------
void menuhandle_meSetLauncher(GtkMenuItem *menuitem, gpointer user_data) {
  open_launcher_window(G_OBJECT(menuitem));
}

void open_launcher_window(GObject* menuitem) {
  GtkWidget *launcherWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(launcherWindow), true);

  gtk_window_set_title(GTK_WINDOW(launcherWindow), gettext("KISS Ebook Starter - Set launcher applications"));
  gtk_window_set_default_size(GTK_WINDOW(launcherWindow), 640, 330);

  //----------------------------------------------------------------------------
  sqlite3* db = g_object_get_data(G_OBJECT(menuitem), "db");

  char *pdfHandler = NULL;
  char *pdfArgs = NULL;

  char *epubHandler = NULL;
  char *epubArgs = NULL;

  char *mobiHandler = NULL;
  char *mobiArgs = NULL;

  char *chmHandler = NULL;
  char *chmArgs = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  char **pointer = NULL;
  char **pointerArgs = NULL;

  for (int i = 1; i < 5; ++i) {
    char getHandlerStmt[74];
    sprintf(getHandlerStmt, "SELECT program, args FROM launcher_applications WHERE format=%d LIMIT 0,1;", i);

    rc = sqlite3_exec(db, getHandlerStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during selection: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      switch(i) {
        case 1:
          pointer = &pdfHandler;
          pointerArgs = &pdfArgs;
          break;
        case 2:
          pointer = &epubHandler;
          pointerArgs = &epubArgs;
          break;
        case 3:
          pointer = &mobiHandler;
          pointerArgs = &mobiArgs;
          break;
        case 4:
          pointer = &chmHandler;
          pointerArgs = &chmArgs;
        default:
          break;
      }

      if (pointer != NULL) {
        get_db_answer_value(&receiveFromDb, "program", pointer);
        get_db_answer_value(&receiveFromDb, "args", pointerArgs);
      }

      free_db_answer(&receiveFromDb);
    }
  }


  //----------------------------------------------------------------------------

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(G_OBJECT(box), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(launcherWindow), box);

  //----------------------------------------------------------------------------

  GtkWidget *labelPDF = gtk_label_new(gettext("PDF file handler:"));
  gtk_widget_set_halign(GTK_WIDGET(labelPDF), GTK_ALIGN_START);
  g_object_set(G_OBJECT(labelPDF), "margin-top", 6, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelPDF);

  GtkWidget *entryPDF = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryPDF), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryPDF), gettext("Name of the application to launch .pdf files."));
  gtk_widget_set_tooltip_text(entryPDF, gettext("Application and/or optional parameters to open .pdf files. This could be: \"evince\" and as parameter \"-f\" to run evince in fullscreen-mode."));
  gtk_container_add(GTK_CONTAINER(box), entryPDF);

  GtkWidget *labelEPUB = gtk_label_new(gettext("EPUB file handler:"));
  g_object_set(G_OBJECT(labelEPUB), "margin-top", 12, "margin-left", 6, NULL);
  gtk_widget_set_halign(GTK_WIDGET(labelEPUB), GTK_ALIGN_START);
  gtk_container_add(GTK_CONTAINER(box), labelEPUB);

  GtkWidget *entryEPUB = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryEPUB), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryEPUB), gettext("Name of the application to launch .epub files."));
  gtk_widget_set_tooltip_text(entryEPUB, gettext("Application and/or optional parameters to open .epub files. This could be: \"fbreader\""));
  gtk_container_add(GTK_CONTAINER(box), entryEPUB);

  GtkWidget *labelMOBI = gtk_label_new(gettext("MOBI file handler:"));
  gtk_widget_set_halign(GTK_WIDGET(labelMOBI), GTK_ALIGN_START);
  g_object_set(G_OBJECT(labelMOBI), "margin-top", 12, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelMOBI);

  GtkWidget *entryMOBI = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryMOBI), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryMOBI), gettext("Name of the application to launch .mobi files."));
  gtk_widget_set_tooltip_text(entryMOBI, gettext("Application and/or optional parameters to open .mobi files. This could be: \"cr3\" for Cool Reader 3"));
  gtk_container_add(GTK_CONTAINER(box), entryMOBI);


  GtkWidget *labelCHM= gtk_label_new(gettext("CHM file handler:"));
  gtk_widget_set_halign(GTK_WIDGET(labelCHM), GTK_ALIGN_START);
  g_object_set(G_OBJECT(labelCHM), "margin-top", 12, "margin-left", 6, NULL);
  gtk_container_add(GTK_CONTAINER(box), labelCHM);

  GtkWidget *entryCHM = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryCHM), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryCHM), gettext("Name of the application to launch .chm files."));
  gtk_widget_set_tooltip_text(entryCHM, gettext("Application and/or optional parameters to open .chm files. This could be: \"xchm\""));
  gtk_container_add(GTK_CONTAINER(box), entryCHM);

  if (pdfHandler != NULL) {
    if (pdfArgs == NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryPDF), pdfHandler);
    } else {
      char commandString[strlen(pdfHandler) + strlen(pdfArgs) + 2];
      strcpy(commandString, pdfHandler);
      strncat(commandString, " ", 1);
      strcat(commandString, pdfArgs);
      gtk_entry_set_text(GTK_ENTRY(entryPDF), commandString);

      free(pdfArgs);
    }

    free(pdfHandler);
  }

  if (epubHandler != NULL) {
    if (epubArgs == NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryEPUB), epubHandler);
    } else {
      char commandString[strlen(epubHandler) + strlen(epubArgs) + 2];
      strcpy(commandString, epubHandler);
      strncat(commandString, " ", 1);
      strcat(commandString, epubArgs);
      gtk_entry_set_text(GTK_ENTRY(entryEPUB), commandString);

      free(epubArgs);
    }

    free(epubHandler);
  }

  if (mobiHandler != NULL) {
    if (mobiArgs == NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryMOBI), mobiHandler);
    } else {
      char commandString[strlen(mobiHandler) + strlen(mobiArgs) + 2];
      strcpy(commandString, mobiHandler);
      strncat(commandString, " ", 1);
      strcat(commandString, mobiArgs);
      gtk_entry_set_text(GTK_ENTRY(entryMOBI), commandString);

      free(mobiArgs);
    }

    free(mobiHandler);
  }

  if (chmHandler != NULL) {
    if (chmArgs == NULL) {
      gtk_entry_set_text(GTK_ENTRY(entryCHM), chmHandler);
    } else {
      char commandString[strlen(chmHandler) + strlen(chmArgs) + 2];
      strcpy(commandString, chmHandler);
      strncat(commandString, " ", 1);
      strcat(commandString, chmArgs);
      gtk_entry_set_text(GTK_ENTRY(entryCHM), commandString);

      free(chmArgs);
    }


    free(chmHandler);
  }
  //----------------------------------------------------------------------------

  //----------------------------------------------------------------------------
  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin-top", 12, NULL);
  gtk_container_add(GTK_CONTAINER(box), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label(gettext("Cancel"));
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", launcherWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(launcherWindow_close), launcherWindow);

  GtkWidget *saveButton = gtk_button_new_with_label(gettext("Save"));
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
  //gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entryPDF));

  //gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(launcherWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(launcherWindow), true);
  gtk_window_set_position(GTK_WINDOW(launcherWindow), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(launcherWindow), true);
  gtk_widget_show_all(launcherWindow);
  gtk_window_present(GTK_WINDOW(launcherWindow));
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

  argumentStore handlerDataPDF = {0, NULL};
  argumentStore handlerDataEPUB = {0, NULL};
  argumentStore handlerDataMOBI = {0, NULL};
  argumentStore handlerDataCHM = {0, NULL};

  retrieve_handler_arguments(&handlerDataPDF, pdfHandler);
  retrieve_handler_arguments(&handlerDataEPUB, epubHandler);
  retrieve_handler_arguments(&handlerDataMOBI, mobiHandler);
  retrieve_handler_arguments(&handlerDataCHM, chmHandler);

  //----------------------------------------------------------------------------
  char *dbErrorMsg = NULL;
  char clearLauncherStmt[35];
  sprintf(clearLauncherStmt, "DELETE FROM launcher_applications;");

  int rc = sqlite3_exec(db, clearLauncherStmt, NULL, NULL, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Unknown SQL error while clearing launcher applications:%s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  //----------------------------------------------------------------------------

  char *dbStmt = NULL;

  // TODO: Add parameter parsing here which are stored in 'args'

  for (int i = 1; i < 5; ++i) {
    argumentStore *handler = NULL;

    switch(i) {
      case 1:
        handler = &handlerDataPDF;
        break;
      case 2:
        handler = &handlerDataEPUB;
        break;
      case 3:
        handler = &handlerDataMOBI;
        break;
      case 4:
        handler = &handlerDataCHM;
        break;
      default:
        break;
    }

    if (handler->count == 0) {
      continue;
    }

    char *args = (char*) calloc(1, sizeof(char));

    for (int j = 1; j < handler->count; ++j) {
      args = (char*) realloc(args, sizeof(char) * (strlen(handler->arguments[j]) + 2));
      strcat(args, handler->arguments[j]);
      if ((j+1) < handler->count) {
        strncat(args, " ", 1);
      }
    }

    dbStmt = sqlite3_mprintf("INSERT OR REPLACE INTO launcher_applications VALUES(NULL, %d, '%q', '%q');", i, handler->arguments[0], (args == NULL ? "" : args));
    free(args);

    if (dbStmt != NULL) {
      int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      if (rc != SQLITE_OK) {
        if (rc == SQLITE_FULL) {
          fprintf(stderr, "Cannot add data to the database, the (temporary) disk is full.");
        } else {
          fprintf(stderr, "Unknown SQL error: %s\n", dbErrorMsg);
        }

        sqlite3_free(dbErrorMsg);
      }

      sqlite3_free(dbStmt);
    }
  }

  free_handler_arguments(&handlerDataPDF);
  free_handler_arguments(&handlerDataEPUB);
  free_handler_arguments(&handlerDataMOBI);
  free_handler_arguments(&handlerDataCHM);


  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}

int trim(const char *input, char **trimmed, bool trimInside) {
  *trimmed = (char*) calloc(strlen(input)+1, sizeof(char));

  char *pointer = *trimmed;

  char key = '\0';
  unsigned int readPos = 0;
  unsigned int writePos = 0;
  bool hasValidData = false;

  while ((key = input[readPos++]) != '\0') {
    if (key != ' ' || (key == ' ' && hasValidData)) {
      if (key == ' ') {
        unsigned int subPos = 0;
        bool hasValidChar = false;
        char subKey = '\0';

        while((subKey = input[readPos + subPos++]) != '\0') {
          if (subKey != ' ') {
            hasValidChar = true;
            if (trimInside) {
              readPos = readPos + (subPos - 1);
            }
            break;
          }
        }

        if (!hasValidChar) {
          pointer[writePos] = '\0';
          break;
        }
      }

      pointer[writePos++] = key;
      hasValidData = true;
    }
  }

  return writePos;
}

bool retrieve_handler_arguments(struct argumentStore *store, const char *stringData) {
  char *data = NULL;

  int retVal = trim(stringData, &data, true);

  if (retVal == 0) {
    return false;
  }

  char *dataPointer = strchr(data, ' ');
  if (dataPointer == NULL) {
    // only one argument
    store->arguments = (char**) realloc(store->arguments, sizeof(char*) * (store->count + 1));
    store->arguments[store->count] = malloc(sizeof(char) * (strlen(data)+1));
    strcpy(store->arguments[store->count], data);
    ++store->count;
  } else {
    long unsigned int count = (dataPointer - data);
    char mainArg[count+1];
    strncpy(mainArg, data, count);
    mainArg[count] = '\0';

    store->arguments = (char**) realloc(store->arguments, sizeof(char*) * (store->count + 1));
    store->arguments[store->count] = malloc(sizeof(char) * (strlen(mainArg)+1));
    strcpy(store->arguments[store->count], mainArg);
    ++store->count;

    char *argumentData = strtok(&dataPointer[1], " ");
    while (argumentData != NULL) {
      store->arguments = (char**) realloc(store->arguments, sizeof(char*) * (store->count + 1));
      store->arguments[store->count] = malloc(sizeof(char) * (strlen(argumentData)+1));
      strcpy(store->arguments[store->count], argumentData);
      ++store->count;
      argumentData = strtok(NULL, " ");
    }
  }

  free(data);

  return true;
}

void free_handler_arguments(struct argumentStore* store) {
  for (int i = 0; i < store->count; ++i) {
    free(store->arguments[i]);
  }

  store->count = 0;
  store->arguments = NULL;
}

void launcherWindow_close(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}


//------------------------------------------------------------------------------

void menuhandle_meEditEntry(GtkMenuItem *menuitem, gpointer user_data) {
  open_edit_window(G_OBJECT(menuitem));
}

void open_edit_window(GObject *dataItem) {
  //----------------------------------------------------------------------------
  GtkWidget *treeView = g_object_get_data(G_OBJECT(dataItem), "treeview");
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

  if (gtk_tree_selection_count_selected_rows(selection) == 0) {
    return;
  }

  GtkTreeIter iter;
  gchar *formatStr;
  gchar *authorStr;
  gchar *titleStr;

  GtkWidget *editWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(editWindow), true);

  gtk_window_set_title(GTK_WINDOW(editWindow), gettext("KISS Ebook Starter - Edit ebook details"));
  gtk_window_set_default_size(GTK_WINDOW(editWindow), 640, 400);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(G_OBJECT(box), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(editWindow), box);

  GtkWidget *labelTitle = gtk_label_new(gettext("Title:"));
  GtkWidget *labelAuthor = gtk_label_new(gettext("Author:"));
  GtkWidget *labelFileName = gtk_label_new(gettext("Filename:"));
  GtkWidget *labelPath = gtk_label_new(gettext("Path:"));
  GtkWidget *labelFormat = gtk_label_new(gettext("Format:"));

  gtk_widget_set_halign(GTK_WIDGET(labelTitle), GTK_ALIGN_START);
  gtk_widget_set_halign(GTK_WIDGET(labelAuthor), GTK_ALIGN_START);
  gtk_widget_set_halign(GTK_WIDGET(labelFileName), GTK_ALIGN_START);
  gtk_widget_set_halign(GTK_WIDGET(labelPath), GTK_ALIGN_START);
  gtk_widget_set_halign(GTK_WIDGET(labelFormat), GTK_ALIGN_START);

  g_object_set(G_OBJECT(labelPath), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelFileName), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelFormat), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelAuthor), "margin-top", 12, "margin-left", 6, NULL);
  g_object_set(G_OBJECT(labelTitle), "margin-top", 0, "margin-left", 6, NULL);

  GtkWidget *entryTitle = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryTitle), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryTitle), "Title");
  gtk_widget_set_tooltip_text(entryTitle, gettext("The title of the ebook displayed in KISS Ebook."));
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryTitle), gettext("Title"));

  GtkWidget *entryAuthor = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryAuthor), 255);
  gtk_widget_set_tooltip_text(entryAuthor, gettext("The ebook author(s) displayed in KISS Ebook."));
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryAuthor), gettext("Author(s)"));

  GtkWidget *entryFileName = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryFileName), 255);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryFileName), gettext("Filename"));
  gtk_widget_set_tooltip_text(entryFileName, gettext("The file name which is not changeable."));
  g_object_set(G_OBJECT(entryFileName), "editable", false, NULL);

  GtkWidget *entryPath = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryPath), 1024);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryPath), gettext("Complete path"));
  gtk_widget_set_tooltip_text(entryPath, gettext("Complete path, including the filename. This is not changeable."));
  g_object_set(G_OBJECT(entryPath), "editable", false, NULL);

  GtkWidget *entryFormat = gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(entryFormat), 5);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entryFormat), gettext("Format"));
  gtk_widget_set_tooltip_text(entryFormat, gettext("The ebook format which is not changeable."));
  g_object_set(G_OBJECT(entryFormat), "editable", false, NULL);

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

  GtkWidget *cancelButton = gtk_button_new_with_label(gettext("Cancel"));
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", editWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(edit_entry_close), NULL);

  GtkWidget *saveButton = gtk_button_new_with_label(gettext("Save data"));
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

  char *dbStmt = sqlite3_mprintf("SELECT * FROM ebook_collection WHERE format=%d AND author='%q' AND title='%q' LIMIT 0,1;", format, authorStr, titleStr);

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during select: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      if (receiveFromDb.count != 0) {
        get_db_answer_value(&receiveFromDb, "path", &filePath);
        get_db_answer_value(&receiveFromDb, "filename", &fileName);
        get_db_answer_value(&receiveFromDb, "format", &fileFormat);
        get_db_answer_value(&receiveFromDb, "author", &fileAuthor);
        get_db_answer_value(&receiveFromDb, "title", &fileTitle);
        free_db_answer(&receiveFromDb);
      }

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

    sqlite3_free(dbStmt);
  }

  g_free(formatStr);
  g_free(authorStr);
  g_free(titleStr);

  //----------------------------------------------------------------------------
  gtk_window_set_type_hint(GTK_WINDOW(editWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(editWindow));
  //gtk_entry_grab_focus_without_selecting(GTK_ENTRY(entryTitle));

  //gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(dataItem), "appWindow")), GTK_WINDOW(editWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(editWindow), true);
  gtk_window_set_position(GTK_WINDOW(editWindow), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal(GTK_WINDOW(editWindow), true);
  gtk_widget_show_all(editWindow);
  gtk_window_present(GTK_WINDOW(editWindow));
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
    return;
  }

  if (strlen(authorStripped) == 0) {
    sprintf(authorStripped, "Unknown");
  }

  //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author = trim('%q'), title = trim('%q') WHERE path == '%s' LIMIT 0,1;", authorStripped, titleStripped, path);
  char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author = trim('%q'), title = trim('%q') WHERE path == '%s';", authorStripped, titleStripped, path);

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during update: %s\n", dbErrorMsg);
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
    sqlite3_free(dbStmt);
  }

  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));

  return;
}

//------------------------------------------------------------------------------
void menuhandle_meToggleColumn(GtkCheckMenuItem *checkmenuitem, gpointer user_data) {
  gtk_tree_view_column_set_visible(GTK_TREE_VIEW_COLUMN(user_data), gtk_check_menu_item_get_active(checkmenuitem));
}

//------------------------------------------------------------------------------

void menuhandle_meResetColumns(GtkMenuItem *menuitem, gpointer user_data) {
  GtkTreeView *ebookList = GTK_TREE_VIEW(user_data);
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  //----------------------------------------------------------------------------
  char columnOrder[64];
  char indexString[3];
  columnOrder[0] = '\0';

  for (int i = 0; i < N_COLUMNS; ++i) {
    sprintf(indexString, "%d,", i);
    strcat(columnOrder, indexString);
  }

  GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(ebookList));
  int orderIndex = 0;

  GtkTreeViewColumn *previousColumn = NULL;
  GtkTreeViewColumn *currentColumn = NULL;
  bool hasValidColumn = false;

  while(true) {
    hasValidColumn = false;

    for (GList *list = columns; list != NULL; list = list->next) {
      if (*(int*) g_object_get_data(G_OBJECT(list->data), "id") == orderIndex) {
        previousColumn = currentColumn;
        hasValidColumn = true;
        currentColumn = GTK_TREE_VIEW_COLUMN(list->data);
        break;
      }
    }

    if (hasValidColumn) {
      if (previousColumn == NULL) {
        gtk_tree_view_move_column_after(GTK_TREE_VIEW(ebookList), currentColumn,  NULL);
      } else {
        gtk_tree_view_move_column_after(GTK_TREE_VIEW(ebookList), currentColumn,  previousColumn);
      }
    }

    if (orderIndex++ == N_COLUMNS) {
      break;
    }
  }

  g_list_free(columns);

  //----------------------------------------------------------------------------

  char *dbErrorMsg = NULL;
  //char *dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option == 'column_order' LIMIT 0,1;", columnOrder);
  char *dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option == 'column_order';", columnOrder);

  if (dbStmt != NULL) {
    int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error, cannot update column order option: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    }
  }

  return;
}


//------------------------------------------------------------------------------
void menuhandle_mOptions(GtkMenuItem *menuitem, gpointer user_data) {
  open_options_window(G_OBJECT(menuitem), user_data);
}

void open_options_window(GObject *menuitem, gpointer user_data) {

  sqlite3 *db = user_data;

  GtkCheckMenuItem *meShowFormat = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowFormat"));
  GtkCheckMenuItem *meShowAuthor = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowAuthor"));
  GtkCheckMenuItem *meShowCategory = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowCategory"));
  GtkCheckMenuItem *meShowTags = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowTags"));
  GtkCheckMenuItem *meShowFilename = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowFilename"));
  GtkCheckMenuItem *meShowPriority = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowPriority"));
  GtkCheckMenuItem *meShowRead = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(menuitem), "meShowRead"));

  GtkWidget *optionsWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(optionsWindow), true);

  gtk_window_set_title(GTK_WINDOW(optionsWindow), gettext("KISS Ebook Starter - Options"));
  gtk_window_set_default_size(GTK_WINDOW(optionsWindow), 640, 300);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(G_OBJECT(box), "margin", 10, NULL);
  gtk_container_add(GTK_CONTAINER(optionsWindow), box);

  GtkWidget *grid = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(box), grid);

  GtkWidget *labelColumns= gtk_label_new(gettext("Visible columns:"));
  gtk_widget_set_halign(GTK_WIDGET(labelColumns), GTK_ALIGN_START);
  g_object_set(G_OBJECT(labelColumns), "margin-top", 0, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), labelColumns, 0, 0, 5, 1);

  GtkWidget *colFormat = gtk_check_button_new_with_label(gettext("Format"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colFormat), gtk_check_menu_item_get_active(meShowFormat));
  g_object_set(G_OBJECT(colFormat), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colFormat, 0, 1, 5, 1);

  GtkWidget *colAuthor = gtk_check_button_new_with_label(gettext("Author"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colAuthor), gtk_check_menu_item_get_active(meShowAuthor));
  g_object_set(G_OBJECT(colAuthor), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colAuthor, 0, 2, 5, 1);

  GtkWidget *colCategory = gtk_check_button_new_with_label(gettext("Category"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colCategory), gtk_check_menu_item_get_active(meShowCategory));
  g_object_set(G_OBJECT(colCategory), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colCategory, 0, 3, 5, 1);

  GtkWidget *colTags = gtk_check_button_new_with_label(gettext("Tags"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colTags), gtk_check_menu_item_get_active(meShowTags));
  g_object_set(G_OBJECT(colTags), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colTags, 0, 4, 5, 1);

  GtkWidget *colFilename = gtk_check_button_new_with_label(gettext("Filename"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colFilename), gtk_check_menu_item_get_active(meShowFilename));
  g_object_set(G_OBJECT(colFilename), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colFilename, 0, 5, 5, 1);

  GtkWidget *colPriority = gtk_check_button_new_with_label(gettext("Priority"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colPriority), gtk_check_menu_item_get_active(meShowPriority));
  g_object_set(G_OBJECT(colPriority), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colPriority, 0, 6, 5, 1);

  GtkWidget *colRead = gtk_check_button_new_with_label(gettext("Read status"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(colRead), gtk_check_menu_item_get_active(meShowRead));
  g_object_set(G_OBJECT(colRead), "margin-top", 10, "margin-left", 0, NULL);
  gtk_grid_attach(GTK_GRID(grid), colRead, 0, 7, 5, 1);

  GtkWidget *importOverwrite = gtk_check_button_new_with_label(gettext("Overwrite existing on import"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(importOverwrite), false);
  g_object_set(G_OBJECT(importOverwrite), "margin-top", 0, "margin-left", 150, NULL);
  gtk_grid_attach(GTK_GRID(grid), importOverwrite, 7, 1, 3, 1);

  GtkWidget *labelSortSelection = gtk_label_new(gettext("Sort column on startup:"));
  gtk_widget_set_halign(GTK_WIDGET(labelSortSelection), GTK_ALIGN_START);
  g_object_set(G_OBJECT(labelSortSelection), "margin-top", 0, "margin-left", 150, NULL);
  gtk_grid_attach(GTK_GRID(grid), labelSortSelection, 7, 3, 3, 1);

  GtkWidget *sortSelection = gtk_combo_box_text_new();
  g_object_set(G_OBJECT(sortSelection), "margin", 0, "margin-left", 150, NULL);
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("No sorting"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Format"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Author"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Title"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Category"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Tags"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Filename"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Priority"));
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sortSelection), gettext("Read"));
  gtk_combo_box_set_active(GTK_COMBO_BOX(sortSelection), 0);
  gtk_grid_attach(GTK_GRID(grid), sortSelection, 7, 4, 3, 1);

  g_signal_connect(G_OBJECT(colFormat), "toggled", G_CALLBACK(options_toggle_column), meShowFormat);
  g_signal_connect(G_OBJECT(colAuthor), "toggled", G_CALLBACK(options_toggle_column), meShowAuthor);
  g_signal_connect(G_OBJECT(colCategory), "toggled", G_CALLBACK(options_toggle_column), meShowCategory);
  g_signal_connect(G_OBJECT(colTags), "toggled", G_CALLBACK(options_toggle_column), meShowTags);
  g_signal_connect(G_OBJECT(colFilename), "toggled", G_CALLBACK(options_toggle_column), meShowFilename);
  g_signal_connect(G_OBJECT(colPriority), "toggled", G_CALLBACK(options_toggle_column), meShowPriority);
  g_signal_connect(G_OBJECT(colRead), "toggled", G_CALLBACK(options_toggle_column), meShowRead);

  //----------------------------------------------------------------------------

  GtkWidget *buttonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(G_OBJECT(buttonBox), "margin", 0, "margin-top", 24, NULL);
  gtk_container_add(GTK_CONTAINER(box), buttonBox);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_EDGE);

  GtkWidget *cancelButton = gtk_button_new_with_label(gettext("Cancel"));
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", optionsWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(optionsWindow_close), NULL);

  GtkWidget *saveButton = gtk_button_new_with_label(gettext("Save"));
  gtk_container_add(GTK_CONTAINER(buttonBox), saveButton);
  g_object_set_data(G_OBJECT(saveButton), "rootWindow", optionsWindow);
  g_object_set_data(G_OBJECT(saveButton), "meShowFormat", meShowFormat);
  g_object_set_data(G_OBJECT(saveButton), "meShowAuthor", meShowAuthor);
  g_object_set_data(G_OBJECT(saveButton), "meShowCategory", meShowCategory);
  g_object_set_data(G_OBJECT(saveButton), "meShowTags", meShowTags);
  g_object_set_data(G_OBJECT(saveButton), "meShowFilename", meShowFilename);
  g_object_set_data(G_OBJECT(saveButton), "meShowPriority", meShowPriority);
  g_object_set_data(G_OBJECT(saveButton), "meShowRead", meShowRead);
  g_object_set_data(G_OBJECT(saveButton), "importOverwrite", importOverwrite);
  g_object_set_data(G_OBJECT(saveButton), "sortSelection", sortSelection);
  g_object_set_data(G_OBJECT(saveButton), "db", user_data);

  g_signal_connect(G_OBJECT(saveButton), "clicked", G_CALLBACK(optionsWindow_save), NULL);

  //----------------------------------------------------------------------------
  char *overwriteOnImport = NULL;

  char *dbErrorMsg = NULL;
  int rc = 0;
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='overwrite_on_import' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error during option retrieval: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (get_db_answer_value(&receiveFromDb, "value", &overwriteOnImport)) {
      if (strcmp(overwriteOnImport, "true") == 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(importOverwrite), true);
      }
    }

    free(overwriteOnImport);
    free_db_answer(&receiveFromDb);
  }

  //----------------------------------------------------------------------------
  // Sort on startup
  char *sortOnStartup = NULL;
  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='sort_on_startup' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error during option retrieval for sorting: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (get_db_answer_value(&receiveFromDb, "value", &sortOnStartup)) {
      int sortColumn = atoi(sortOnStartup);

      if (sortColumn != -1) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(sortSelection), sortColumn);
      }
    }

    free(sortOnStartup);
    free_db_answer(&receiveFromDb);
  }

  //----------------------------------------------------------------------------
  //gtk_window_set_keep_above(GTK_WINDOW(optionsWindow), true);
  gtk_window_set_type_hint(GTK_WINDOW(optionsWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(optionsWindow));
  gtk_widget_show_all(optionsWindow);

  //gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(fileChooserWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(optionsWindow), true);
  gtk_window_set_modal(GTK_WINDOW(optionsWindow), true);
  gtk_window_set_position(GTK_WINDOW(optionsWindow), GTK_WIN_POS_CENTER_ON_PARENT);
}


//------------------------------------------------------------------------------
void optionsWindow_close(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}

void optionsWindow_save(GtkButton *button, gpointer user_data) {
  GtkCheckMenuItem *meShowFormat = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowFormat"));
  GtkCheckMenuItem *meShowAuthor = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowAuthor"));
  GtkCheckMenuItem *meShowCategory = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowCategory"));
  GtkCheckMenuItem *meShowTags = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowTags"));
  GtkCheckMenuItem *meShowFilename = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowFilename"));
  GtkCheckMenuItem *meShowPriority = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowPriority"));
  GtkCheckMenuItem *meShowRead = GTK_CHECK_MENU_ITEM(g_object_get_data(G_OBJECT(button), "meShowRead"));
  GtkComboBox *sortSelection = GTK_COMBO_BOX(g_object_get_data(G_OBJECT(button), "sortSelection"));
  GtkToggleButton *importOverwrite = GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(button), "importOverwrite"));

  sqlite3 *db = g_object_get_data(G_OBJECT(button), "db");
  char *dbStmt = NULL;
  char *dbErrorMsg = NULL;
  int rc = 0;

  //----------------------------------------------------------------------------

  // Create the column selection string
  char *visibleColumnsStmt = (char*) calloc(1, sizeof(char));
  int addedColumns = 0;
  int size = 1;

  char *columnName = calloc(256, sizeof(char));
  GtkCheckMenuItem **activeColumn = NULL;

  for (int i = 0; i < 7; ++i) {
    switch(i) {
      case 0:
        activeColumn = &meShowFormat;
        sprintf(columnName, "format");
        break;
      case 1:
        activeColumn = &meShowAuthor;
        sprintf(columnName, "author");
        break;
      case 2:
        activeColumn = &meShowCategory;
        sprintf(columnName, "category");
        break;
      case 3:
        activeColumn = &meShowTags;
        sprintf(columnName, "tags");
        break;
      case 4:
        activeColumn = &meShowFilename;
        sprintf(columnName, "filename");
        break;
      case 5:
        activeColumn = &meShowPriority;
        sprintf(columnName, "priority");
        break;
      case 6:
        activeColumn = &meShowRead;
        sprintf(columnName, "read");
        break;
      default:
        break;
    }

    if (activeColumn != NULL) {
      if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(*activeColumn))) {
        size += strlen(columnName);

        if (addedColumns != 0) {
          size += 1;
          visibleColumnsStmt = (char*) realloc(visibleColumnsStmt, size);
          strcat(visibleColumnsStmt, ",");
        } else {
          visibleColumnsStmt = (char*) realloc(visibleColumnsStmt, size);
        }

        strcat(visibleColumnsStmt, columnName);

        ++addedColumns;
      }

      activeColumn = NULL;
    }
  }

  //----------------------------------------------------------------------------

  //dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option='visible_columns' LIMIT 0,1;", visibleColumnsStmt);
  dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option='visible_columns';", visibleColumnsStmt);

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during save of visible column options: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    }

    sqlite3_free(dbStmt);
  }

  free(visibleColumnsStmt);
  free(columnName);

  //----------------------------------------------------------------------------
  //dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option='overwrite_on_import' LIMIT 0,1;", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(importOverwrite)) == true ? "true" : "false"));
  dbStmt = sqlite3_mprintf("UPDATE options SET value='%s' WHERE option='overwrite_on_import';", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(importOverwrite)) == true ? "true" : "false"));

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during save of overwrite on import option: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    }

    sqlite3_free(dbStmt);
  }

  //----------------------------------------------------------------------------
  int sortOnColumn = gtk_combo_box_get_active(GTK_COMBO_BOX(sortSelection));
  if (sortOnColumn == 0) {
    sortOnColumn = -1;
  }

  //dbStmt = sqlite3_mprintf("UPDATE options SET value=%d WHERE option='sort_on_startup' LIMIT 0,1;", sortOnColumn);
  dbStmt = sqlite3_mprintf("UPDATE options SET value=%d WHERE option='sort_on_startup';", sortOnColumn);
  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error during save of sort on startup option: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    }

    sqlite3_free(dbStmt);
  }

  //----------------------------------------------------------------------------
  gtk_widget_destroy(g_object_get_data(G_OBJECT(button), "rootWindow"));
}


void options_toggle_column(GtkToggleButton *toggleButton, gpointer user_data) {
  GtkCheckMenuItem *menuItem = GTK_CHECK_MENU_ITEM(user_data);
  gtk_check_menu_item_set_active(menuItem, gtk_toggle_button_get_active(toggleButton));
}


//------------------------------------------------------------------------------
void menuhandle_meImportFiles(GtkMenuItem *menuitem, gpointer user_data) {
  open_importFiles_window(G_OBJECT(menuitem));
}

void open_importFiles_window(GObject *menuitem) {
  GtkWidget *fileChooserWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(fileChooserWindow), true);

  gtk_window_set_title(GTK_WINDOW(fileChooserWindow), gettext("KISS Ebook Starter - Import files and folders"));
  gtk_window_set_default_size(GTK_WINDOW(fileChooserWindow), 640, 400);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(fileChooserWindow), box);

  GtkWidget *chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);

  GtkFileFilter *inputFilter = gtk_file_filter_new();
  gtk_file_filter_set_name(inputFilter, gettext("All supported ebook formats"));
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

  GtkWidget *cancelButton = gtk_button_new_with_label(gettext("Cancel"));
  gtk_container_add(GTK_CONTAINER(buttonBox), cancelButton);
  g_object_set_data(G_OBJECT(cancelButton), "rootWindow", fileChooserWindow);
  g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(fileChooser_close), NULL);

  GtkWidget *openButton = gtk_button_new_with_label(gettext("Open"));
  gtk_container_add(GTK_CONTAINER(buttonBox), openButton);
  g_object_set_data(G_OBJECT(openButton), "rootWindow", fileChooserWindow);
  g_object_set_data(G_OBJECT(openButton), "fileChooser", chooser);
  g_object_set_data(G_OBJECT(openButton), "progressRevealer", g_object_get_data(G_OBJECT(menuitem), "progressRevealer"));
  g_object_set_data(G_OBJECT(openButton), "progress", g_object_get_data(G_OBJECT(menuitem), "progress"));
  g_object_set_data(G_OBJECT(openButton), "status", g_object_get_data(G_OBJECT(menuitem), "status"));
  g_object_set_data(G_OBJECT(openButton), "treeview", g_object_get_data(G_OBJECT(menuitem), "treeview"));
  g_object_set_data(G_OBJECT(openButton), "db", g_object_get_data(G_OBJECT(menuitem), "db"));

  g_signal_connect(G_OBJECT(openButton), "clicked", G_CALLBACK(fileChooser_importFiles), NULL);

  //gtk_window_set_keep_above(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_type_hint(GTK_WINDOW(fileChooserWindow), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_activate_focus(GTK_WINDOW(fileChooserWindow));

  //gtk_window_set_transient_for(GTK_WINDOW(g_object_get_data(G_OBJECT(menuitem), "appWindow")), GTK_WINDOW(fileChooserWindow));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_modal(GTK_WINDOW(fileChooserWindow), true);
  gtk_window_set_position(GTK_WINDOW(fileChooserWindow), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_widget_show_all(fileChooserWindow);
  gtk_window_present(GTK_WINDOW(fileChooserWindow));
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
  GtkTreeView *treeview = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(button), "treeview"));

  GtkWidget *fileChooser = g_object_get_data(G_OBJECT(button), "fileChooser");
  GSList *filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(fileChooser));

  if (filenames == NULL) {
    return;
  }

  //--------------------------------------------------------------------------
  bool doOverwriteOnImport = false;

  // Get overwrite setting
  char *overwriteOnImport = NULL;
  char *dbErrorMsg = NULL;
  int rc = 0;
  struct dbAnswer receiveFromDb = { 0, NULL, NULL };

  rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='overwrite_on_import' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error reading out overwrite on import option: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  } else {
    if (get_db_answer_value(&receiveFromDb, "value", &overwriteOnImport)) {
      if (strcmp(overwriteOnImport, "true") == 0) {
        doOverwriteOnImport = true;
      }
    }
    free(overwriteOnImport);
    free_db_answer(&receiveFromDb);
  }

  //--------------------------------------------------------------------------

  gtk_widget_destroy(rootWindow);

  GtkWidget *progressRevealer = g_object_get_data(G_OBJECT(button), "progressRevealer");
  gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), true);

  GtkWidget *progressBar = g_object_get_data(G_OBJECT(button), "progress");
  GtkWidget *statusBar = g_object_get_data(G_OBJECT(button), "status");
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

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Info");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);


  for (unsigned int i = 0; (item = g_slist_nth(filenames, i)) != NULL; ++i) {
    char *fileType = strrchr(item->data, '.');

    DIR *dp = opendir(item->data);

    if (dp != NULL) {
      // Its a folder most likely
      int retVal = read_out_path(item->data, statusBar, contextId, progressBar, i, filesToAdd, model, db, doOverwriteOnImport);

      if (retVal != -1) {
        filesAdded += retVal;
        filesToAdd += retVal-1;
      }

      closedir(dp);
    } else if (fileType != NULL && strlen(fileType) < 6) {
      if (read_and_add_file_to_model(item->data, true, statusBar, contextId, true, progressBar, i, filesToAdd, true, model, db, doOverwriteOnImport)) {
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

  if (doOverwriteOnImport) {
    clearAndUpdateDataStore(GTK_LIST_STORE(gtk_tree_view_get_model(treeview)), db);
  }

  char errorString[128];
  sprintf(errorString, "%u %s", filesError, filesError == 1 ? gettext("error.") : gettext("errors."));

  char importDoneString[128];
  strcpy(importDoneString, gettext("[STATUS] Done importing"));
  sprintf(message, "%s %u %s%s%s",
    importDoneString, filesAdded, filesAdded == 1 ? gettext("file") : gettext("files"), filesError != 0 ? ", " : ".", filesError != 0 ? errorString : "");

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
  gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);

  gtk_revealer_set_reveal_child(GTK_REVEALER(progressRevealer), false);
}

void clearAndUpdateDataStore(GtkListStore *dataStore, sqlite3 *db) {
  gtk_list_store_clear(dataStore);

  char *dbErrorMsg = NULL;

  int rc = sqlite3_exec(db, "SELECT format, author, title, filename, category, tags, priority, read FROM ebook_collection;", add_db_data_to_store, (void*) dataStore, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error while restoring ebook list in clean and update functon: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }
}


//------------------------------------------------------------------------------

void handle_column_change(GtkTreeView *treeview, gpointer user_data) {
  g_signal_stop_emission_by_name(treeview, "columns-changed");

  GList *columns = gtk_tree_view_get_columns(GTK_TREE_VIEW(treeview));

  char columnString[64];
  char stringIndex[3];
  columnString[0] = '\0';

  if (g_list_length(columns) != N_COLUMNS) {
    return;
  }

  for (GList *list = columns; list != NULL; list = list->next) {
    int index = *(int*) g_object_get_data(G_OBJECT(GTK_TREE_VIEW_COLUMN(list->data)), "id");
    sprintf(stringIndex, "%d,", index);
    strcat(columnString, stringIndex);
  }

  g_list_free(columns);

  // Update database
  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(treeview), "db");

  //char *dbStmt = sqlite3_mprintf("UPDATE options SET value = '%s' WHERE option == 'column_order' LIMIT 0,1;", columnString);
  char *dbStmt = sqlite3_mprintf("UPDATE options SET value = '%s' WHERE option == 'column_order';", columnString);

  if (dbStmt != NULL) {
    int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
    sqlite3_free(dbStmt);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error while updating column order: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
      sqlite3_close(db);
    }
  }
}

gboolean handle_drag_data(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selData, guint time, gpointer user_data) {
  sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");
  GtkWidget *statusBar = g_object_get_data(G_OBJECT(widget), "status");
  GtkWidget *progressBar = g_object_get_data(G_OBJECT(widget), "progress");
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

  guint contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Info");
  gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

  //gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");

  gint format = gtk_selection_data_get_format(selData);
  GtkWidget *progressRevealer = g_object_get_data(G_OBJECT(widget), "progressRevealer");

  if (format == 8) {

    //--------------------------------------------------------------------------
    bool doOverwriteOnImport = false;

    // Get overwrite setting
    char *overwriteOnImport = NULL;
    char *dbErrorMsg = NULL;
    int rc = 0;
    struct dbAnswer receiveFromDb = { 0, NULL, NULL };

    rc = sqlite3_exec(db, "SELECT value FROM options WHERE option='overwrite_on_import' LIMIT 0,1;", fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error reading out overwrite on import option: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      if (get_db_answer_value(&receiveFromDb, "value", &overwriteOnImport)) {
        if (strcmp(overwriteOnImport, "true") == 0) {
          doOverwriteOnImport = true;
        }
      }
      free(overwriteOnImport);
      free_db_answer(&receiveFromDb);
    }

    //--------------------------------------------------------------------------

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
      char *cleanedPath = g_uri_unescape_string(&uriList[i][7], NULL);

      DIR *dp = opendir(cleanedPath);
      if (dp != NULL) {
        // Its a folder most likely
        int retVal = read_out_path(cleanedPath, statusBar, contextId, progressBar, i, filesToAdd, model, db, doOverwriteOnImport);

        if (retVal != -1) {
          filesAdded += retVal;
          filesToAdd += retVal-1;
        }

        closedir(dp);
      } else if (read_and_add_file_to_model(cleanedPath, true, statusBar, contextId, true, progressBar, i, filesToAdd, true, model, db, doOverwriteOnImport)) {
        ++filesAdded;
      } else {
        ++filesError;
      }

      free(cleanedPath);
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar), 1.0);

    if (doOverwriteOnImport) {
      clearAndUpdateDataStore(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(widget))), db);
    }


    char errorString[128];
    sprintf(errorString, "%u %s", filesError, filesError == 1 ? gettext("error.") : gettext("errors."));

    char importDoneString[128];
    strcpy(importDoneString, gettext("[STATUS] Done importing"));
    sprintf(message, "%s %u %s%s%s",
      importDoneString, filesAdded, filesAdded == 1 ? gettext("file") : gettext("files"), filesError != 0 ? ", " : ".", filesError != 0 ? errorString : "");

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

  if (gtk_tree_selection_count_selected_rows(selection) == 0) {
    return;
  }

  GtkTreeIter iter;
  gchar *filenameStr;
  gchar *formatStr;

  sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");

  gtk_tree_selection_get_selected(selection, &model, &iter);
  gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, FILENAME_COLUMN, &filenameStr, -1);

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
    g_free(formatStr);
    return;
  }

  g_free(formatStr);

  //----------------------------------------------------------------------------
  int rc = 0;
  char *dbErrorMsg = NULL;
  struct dbAnswer receiveFromDb = {0, NULL, NULL};
  char *dbStmt = NULL;

  char *filePath = NULL;
  char *launcher = NULL;
  char *args = NULL;

  //----------------------------------------------------------------------------
  dbStmt = sqlite3_mprintf("SELECT path FROM ebook_collection WHERE filename=='%s' LIMIT 0,1;", filenameStr);
  g_free(filenameStr);

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during selection: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      if (!get_db_answer_value(&receiveFromDb, "path", &filePath)) {
        sqlite3_free(dbStmt);
        free_db_answer(&receiveFromDb);
        return;
      }

      free_db_answer(&receiveFromDb);
    }

    sqlite3_free(dbStmt);
  }

  //----------------------------------------------------------------------------
  dbStmt = sqlite3_mprintf("SELECT program, args FROM launcher_applications WHERE format=%d LIMIT 0,1;", format);

  if (dbStmt != NULL) {
    rc = sqlite3_exec(db, dbStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);

    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error during selection: %s\n", dbErrorMsg);
      sqlite3_free(dbErrorMsg);
    } else {
      if (!get_db_answer_value(&receiveFromDb, "program", &launcher)) {

        GtkWidget *statusBar = g_object_get_data(G_OBJECT(widget), "status");
        guint contextId;

        contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Welcome");
        gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

        contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Update");
        gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

        contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Info");
        gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);

        contextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusBar), "Info");
        gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, gettext("Please set a laucher application first to open a file."));

        free(filePath);
        free_db_answer(&receiveFromDb);
        sqlite3_free(dbStmt);
        return;
      }

      get_db_answer_value(&receiveFromDb, "args", &args);
      free_db_answer(&receiveFromDb);
      sqlite3_free(dbStmt);
    }
  }

  //----------------------------------------------------------------------------

  char launchString[strlen(launcher) + (args != NULL ? strlen(args) : 0) + (strlen(filePath) * 3) + 2];
  int readPos = 0;

  if (args != NULL) {
    sprintf(launchString, "%s %s ", launcher, args);
  } else {
    sprintf(launchString, "%s ", launcher);
  }

  char escapeChars[13] = " $.*/()[\\]^";

  int writePos = strlen(launchString);
  bool isEscapingChar = false;
  char key = '\0';
  char subkey = '\0';
  int readSub = 0;

  while((key = filePath[readPos++]) != '\0') {
    readSub = 0;
    isEscapingChar = false;
    while ((subkey = escapeChars[readSub++]) != '\0') {
      if (subkey == key) {
        isEscapingChar = true;
        break;
      }
    }

    if (isEscapingChar) {
      launchString[writePos++] = '\\';
      launchString[writePos++] = key;
    } else {
      launchString[writePos++] = key;
    }

  }

  launchString[writePos] = '\0';

  pid_t child_pid = fork();
  if (child_pid >= 0) {
    if (child_pid == 0) {
      free(args);
      free(filePath);
      free(launcher);
      GtkApplication *app = g_object_get_data(G_OBJECT(widget), "app");
      g_object_unref(g_object_get_data(G_OBJECT(widget), "dataStore"));
      g_object_unref(G_OBJECT(app));
      g_application_quit(G_APPLICATION(app));

      execlp("/bin/sh", "/bin/sh", "-c", launchString, NULL);
      return;
    } else {
      free(args);
      free(filePath);
      free(launcher);
      return;
    }
  }

  free(args);
  free(filePath);
  free(launcher);

  //system(launchString);
  return;
}

void handle_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* column, gpointer user_data) {
  int *id = g_object_get_data(G_OBJECT(column), "id");
  if (*id != 0) {
    return;
  }

  handle_launchCommand(GTK_WIDGET(tree_view));
  return;
}

gboolean handle_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
  bool pressedDelete = false;

  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();

  switch (event->keyval) {
    case GDK_KEY_Delete:
      pressedDelete = true;
      break;
    case GDK_KEY_s:
      if ((event->state & modifiers) == GDK_CONTROL_MASK) { // Strg Pressed
        handle_launchCommand(widget);
        return true;
      }
      break;
    case GDK_KEY_e:
      if ((event->state & modifiers) == GDK_CONTROL_MASK) {
        open_edit_window(G_OBJECT(widget));
        return true;
      }
      break;
    case GDK_KEY_w:
      if ((event->state & modifiers) == GDK_CONTROL_MASK) {
        open_launcher_window(G_OBJECT(widget));
      }
      break;
    case GDK_KEY_a:
      if ((event->state & modifiers) == GDK_CONTROL_MASK) {
        open_importFiles_window(G_OBJECT(widget));
      }
      break;
    default:
      return false;
      break;
  }

  if (pressedDelete) {
    delete_selected_entry_from_db_and_store(widget);
  }

  return true;
}


//------------------------------------------------------------------------------
void delete_selected_entry_from_db_and_store(GtkWidget *widget) {
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
  GtkTreeIter iter;

  if (gtk_tree_selection_count_selected_rows(selection) == 0) {
    return;
  }

  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    GtkListStore *dataStore = g_object_get_data(G_OBJECT(widget), "dataStore");
    gchar *formatStr;
    gchar *filenameStr;

    gtk_tree_model_get(model, &iter, FORMAT_COLUMN, &formatStr, FILENAME_COLUMN, &filenameStr, -1);

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

    char *dbErrorMsg = NULL;
    char *dbStmt = NULL;
    int rc = 0;

    sqlite3 *db = g_object_get_data(G_OBJECT(widget), "db");
    //dbStmt = sqlite3_mprintf("DELETE FROM ebook_collection WHERE format == %d AND filename == '%q' LIMIT 0,1;", format, filenameStr);
    dbStmt = sqlite3_mprintf("DELETE FROM ebook_collection WHERE format == %d AND filename == '%q';", format, filenameStr);
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while deleting: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
      } else {
        gtk_list_store_remove(dataStore, &iter);
      }
    }

    g_free(formatStr);
    g_free(filenameStr);
  }
}


//------------------------------------------------------------------------------
void search_icon_click(GtkEntry* entry, GtkEntryIconPosition icon_pos, GdkEvent* event, gpointer user_data) {
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY) {
    GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
    sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");
    clearAndUpdateDataStore(dataStore, db);
    gtk_entry_set_text(entry, "");
  } else {
    search_handle_search(entry, user_data);
  }
}

void search_handle_search(GtkEntry* entry, gpointer user_data) {
  const gchar *text = gtk_entry_get_text(entry);
  char *trimmedText = NULL;

  int retVal = trim(text, &trimmedText, false);
  char *dbErrorMsg = NULL;
  int rc = 0;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  GtkWidget *searchSelection = g_object_get_data(G_OBJECT(user_data), "searchSelection");
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  gtk_list_store_clear(dataStore);

  if (retVal != 0) {

    gint searchItem = gtk_combo_box_get_active(GTK_COMBO_BOX(searchSelection));
    char searchStmt[99] = "SELECT format, author, title, filename, category, tags, priority, read FROM ebook_collection WHERE";
    char *dbStmt = NULL;
    int format = 0;

    switch (searchItem) {
      default:
      case SEARCH_AUTHOR_TITLE:
        dbStmt = sqlite3_mprintf("%s author LIKE '%%%q%%' OR title LIKE '%%%q%%' ORDER BY author, title ASC;", searchStmt, trimmedText, trimmedText);
        break;
      case SEARCH_FORMAT:

        if (strncmp(trimmedText, "pdf", 3) == 0) {
          format = 1;
        } else if (strncmp(trimmedText, "epub", 4) == 0) {
          format = 2;
        } else if (strncmp(trimmedText, "mobi", 4) == 0) {
          format = 3;
        } else if (strncmp(trimmedText, "chm", 3) == 0) {
          format = 4;
        }

        if (format != 0) {
          dbStmt = sqlite3_mprintf("%s format == %d ORDER BY title ASC;", searchStmt, format);
        }
        break;
      case SEARCH_AUTHORS:
        dbStmt = sqlite3_mprintf("%s author LIKE '%%%q%%' ORDER BY author ASC;", searchStmt, trimmedText);
        break;
      case SEARCH_TITLE:
        dbStmt = sqlite3_mprintf("%s title LIKE '%%%q%%' ORDER BY title ASC;", searchStmt, trimmedText);
        break;
      case SEARCH_CATEGORY:
        dbStmt = sqlite3_mprintf("%s category LIKE '%%%q%%' ORDER BY category ASC;", searchStmt, trimmedText);
        break;
      case SEARCH_TAGS:
        dbStmt = sqlite3_mprintf("%s tags LIKE '%%%q%%' ORDER BY tags ASC;", searchStmt, trimmedText);
        break;
      case SEARCH_FILENAME:
        dbStmt = sqlite3_mprintf("%s filename LIKE '%%%q%%' ORDER BY filename ASC;", searchStmt, trimmedText);
        break;
    }

    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, add_db_data_to_store, (void*) dataStore, &dbErrorMsg);
      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while filtering ebook list: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
          free(trimmedText);
      } else {
        free(trimmedText);
        return;
      }
    }
  }

  rc = sqlite3_exec(db, "SELECT format, author, title, filename, category, tags, priority, read FROM ebook_collection;", add_db_data_to_store, (void*) dataStore, &dbErrorMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error while restoring ebook list: %s\n", dbErrorMsg);
    sqlite3_free(dbErrorMsg);
  }

  free(trimmedText);
  return;
}


//------------------------------------------------------------------------------
void handle_sort_column(GtkTreeViewColumn* column, gpointer user_data) {
  GtkTreeView *treeview = GTK_TREE_VIEW(user_data);
  GList *columns = gtk_tree_view_get_columns(treeview);
  int index = g_list_index(columns, (gpointer) column);
  g_list_free(columns);

  gtk_tree_view_column_set_sort_column_id(column, index);
}


//------------------------------------------------------------------------------
gboolean handle_editing_author(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {

  unsigned int inputLength = strlen(new_text);
  if (inputLength > 512) {
    return false;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;
  gchar *filenameStr = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, -1);

    char *stringValue = NULL;
    char defaultValue[] = "Unknown";

    if (inputLength != 0) {
      stringValue = new_text;
    } else {
      stringValue = defaultValue;
    }

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author = '%q' WHERE filename == '%q' LIMIT 0,1;", stringValue, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author = '%q' WHERE filename == '%q';", stringValue, filenameStr);
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
        sqlite3_free(dbStmt);
        g_free(filenameStr);
        return false;
      } else {
        gtk_list_store_set(dataStore, &iter, AUTHOR_COLUMN, stringValue, -1);
      }

      sqlite3_free(dbStmt);
    } else {
      g_free(filenameStr);
      return false;
    }

    g_free(filenameStr);
  }

  return true;
}


//------------------------------------------------------------------------------
gboolean handle_editing_title(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {

  unsigned int inputLength = strlen(new_text);
  if (inputLength > 512) {
    return false;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;
  gchar *filenameStr = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, -1);

    char *stringValue = NULL;
    char defaultValue[strlen(filenameStr)+1];
    strcpy(defaultValue, filenameStr);

    if (inputLength != 0) {
      stringValue = new_text;
    } else {
      stringValue = defaultValue;
    }

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET title = '%q' WHERE filename == '%q' LIMIT 0,1;", stringValue, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET title = '%q' WHERE filename == '%q';", stringValue, filenameStr);
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
        g_free(filenameStr);
        return false;
      } else {
        gtk_list_store_set(dataStore, &iter, TITLE_COLUMN, stringValue, -1);
      }

      sqlite3_free(dbStmt);
    } else {
      g_free(filenameStr);
      return false;
    }

    g_free(filenameStr);
  }

  return true;
}


//------------------------------------------------------------------------------
gboolean handle_editing_category(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {

  unsigned int inputLength = strlen(new_text);
  if (inputLength > 512) {
    return false;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;
  gchar *filenameStr = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, -1);

    char *stringValue = NULL;
    char defaultValue[1] = "";

    if (inputLength != 0) {
      stringValue = new_text;
    } else {
      stringValue = defaultValue;
    }

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET category = '%q' WHERE filename == '%q' LIMIT 0,1;", stringValue, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET category = '%q' WHERE filename == '%q';", stringValue, filenameStr);
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
        g_free(filenameStr);
        return false;
      } else {
        gtk_list_store_set(dataStore, &iter, CATEGORY_COLUMN, stringValue, -1);
      }

      sqlite3_free(dbStmt);
    } else {
      g_free(filenameStr);
      return false;
    }

    g_free(filenameStr);
  }

  return true;
}

//------------------------------------------------------------------------------
gboolean handle_editing_tags(GtkCellRendererText *renderer, gchar *path, gchar *new_text, gpointer user_data) {

  unsigned int inputLength = strlen(new_text);
  if (inputLength > 1024) {
    return false;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;
  gchar *filenameStr = NULL;

  int rc = 0;
  char *dbErrorMsg = NULL;
  sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

  if (gtk_tree_model_get_iter_from_string(model, &iter, path)) {
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, -1);

    char *stringValue = NULL;
    char defaultValue[1] = "";

    if (inputLength != 0) {
      stringValue = new_text;
    } else {
      stringValue = defaultValue;
    }

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET tags = '%q' WHERE filename == '%q' LIMIT 0,1;", stringValue, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET tags = '%q' WHERE filename == '%q';", stringValue, filenameStr);
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
        g_free(filenameStr);
        return false;
      } else {
        gtk_list_store_set(dataStore, &iter, TAGS_COLUMN, stringValue, -1);
      }

      sqlite3_free(dbStmt);
    } else {
      g_free(filenameStr);
      return false;
    }

    g_free(filenameStr);
  }

  return true;
}

//------------------------------------------------------------------------------
void handle_editing_priority_value(GtkAdjustment *adjustment, gpointer user_data) {
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkTreeIter iter;

  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    gdouble value = gtk_adjustment_get_value(adjustment);
    //gtk_list_store_set(dataStore, &iter, PRIORITY_COLUMN, (int) value, -1);
    gtk_adjustment_configure(adjustment, (int) value, -10, 10, 1, 5, 0);
  }
}


void handle_editing_priority(GtkAdjustment *adjustment, gpointer user_data) {
  gdouble value = gtk_adjustment_get_value(adjustment);

  if (value > 10) {
    value = 10;
  } else if (value < -10) {
    value = -10;
  }

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;

  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    gchar *filenameStr = NULL;
    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, -1);

    char *dbErrorMsg = NULL;
    sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET priority=%d WHERE filename == '%s' LIMIT 0,1;", (int) value, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET priority=%d WHERE filename == '%s';", (int) value, filenameStr);
    if (dbStmt != NULL) {
      int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating priority: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
      } else {
        gtk_list_store_set(dataStore, &iter, PRIORITY_COLUMN, (int) value, -1);
        gtk_adjustment_set_value(adjustment, value);
      }

      g_free(filenameStr);
    }

  }
}

//------------------------------------------------------------------------------

void handle_toggle_read(GtkCellRendererToggle *cell_renderer, gchar *path, gpointer user_data) {

  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(user_data));
  GtkListStore *dataStore = g_object_get_data(G_OBJECT(user_data), "dataStore");
  GtkTreeIter iter;

  GtkTreePath *treePath = gtk_tree_path_new_from_string(path);
  if (gtk_tree_model_get_iter(model, &iter, treePath)) {
    gchar *filenameStr = NULL;
    gboolean toggleState = false;

    gtk_tree_model_get(model, &iter, FILENAME_COLUMN, &filenameStr, READ_COLUMN, &toggleState, -1);
    toggleState = !toggleState;

    char *dbErrorMsg = NULL;
    sqlite3 *db = g_object_get_data(G_OBJECT(user_data), "db");

    //char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET read=%d WHERE filename == '%s' LIMIT 0,1;", (bool) toggleState, filenameStr);
    char *dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET read=%d WHERE filename == '%s';", (bool) toggleState, filenameStr);
    if (dbStmt != NULL) {
      int rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);

      sqlite3_free(dbStmt);

      if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error while updating read: %s\n", dbErrorMsg);
        sqlite3_free(dbErrorMsg);
      } else {
        gtk_cell_renderer_toggle_set_active(cell_renderer, toggleState);
        gtk_list_store_set(dataStore, &iter, READ_COLUMN, toggleState, -1);
      }

      g_free(filenameStr);
    }

  }
}


//------------------------------------------------------------------------------

bool read_and_add_file_to_model(char* inputFileName, bool showStatus, GtkWidget* statusBar, unsigned int contextId, bool showProgress, GtkWidget* progressBar, unsigned int index, unsigned int filesToAdd, bool addFileToModel, GtkTreeModel *model, sqlite3* db, bool doOverwriteOnImport) {
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
    char parsingStr[128];
    strcpy(parsingStr, gettext("[PARSING]"));

    sprintf(message, "%s %s", parsingStr, cleanedFileName);
    gtk_statusbar_remove_all(GTK_STATUSBAR(statusBar), contextId);
    gtk_statusbar_push(GTK_STATUSBAR(statusBar), contextId, message);
  }

  //------------------------------------------------------------------------

  fileInfo fileData = {NULL, NULL, 0};

  int format = 0;
  bool hasCleanPath = false;

  if (fileType != NULL) {
    bool retVal = false;

    if (strcmp(lowerFiletype, ".pdf") == 0) {
      format = 1;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_pdf(&cleanedPath[7], &fileData);
      } else {
        hasCleanPath = true;
        retVal = read_pdf(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".epub") == 0) {
      format = 2;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_epub(&cleanedPath[7], &fileData);
      } else {
        hasCleanPath = true;
        retVal = read_epub(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".mobi") == 0) {
      format = 3;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_mobi(&cleanedPath[7], &fileData);
      } else {
        hasCleanPath = true;
        retVal = read_mobi(cleanedPath, &fileData);
      }
    } else if (strcmp(lowerFiletype, ".chm") == 0) {
      format = 4;

      if (strncmp(cleanedPath, "file:", 5) == 0) {
        retVal = read_chm(&cleanedPath[7], &fileData);
      } else {
        hasCleanPath = true;
        retVal = read_chm(cleanedPath, &fileData);
      }
    }

    if (!retVal) {
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
    bool dbOkay = true;
    char *dbErrorMsg = NULL;
    int rc = 0;

    char *dbStmt = NULL;

    bool hasOverwrite = false;
    char *existingId = NULL;

    if (doOverwriteOnImport) {
      struct dbAnswer receiveFromDb = { 0, NULL, NULL };

      dbStmt = sqlite3_mprintf("SELECT id FROM ebook_collection WHERE filename='%q' LIMIT 0,1;", cleanedFileName);

      if (dbStmt != NULL) {
        rc = sqlite3_exec(db, dbStmt, fill_db_answer, (void*) &receiveFromDb, &dbErrorMsg);
        if (rc != SQLITE_OK) {
          sqlite3_free(dbErrorMsg);
        } else {
          if (get_db_answer_value(&receiveFromDb, "id", &existingId)) {
            hasOverwrite = true;
          }
        }

        sqlite3_free(dbErrorMsg);
      }
    }

    if (!hasOverwrite) {
      dbStmt = sqlite3_mprintf("INSERT INTO ebook_collection VALUES (NULL,%d,'%q','%q','%q','%q',0,0,NULL,NULL);",
      format,
      author == NULL ? "Unknown" : author,
      title == NULL ? cleanedFileName : title,
      hasCleanPath ? cleanedPath : &cleanedPath[7],
      cleanedFileName
      );
    } else {
      //dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author='%q', title='%q', path='%q' WHERE id==%s LIMIT 0,1;",
      dbStmt = sqlite3_mprintf("UPDATE ebook_collection SET author='%q', title='%q', path='%q' WHERE id==%s;",
        author == NULL ? "Unknown" : author,
        title == NULL ? cleanedFileName : title,
        hasCleanPath ? cleanedPath : &cleanedPath[7],
        existingId
      );

      free(existingId);
    }

    //--------------------------------------------------------------------------
    if (dbStmt != NULL) {
      rc = sqlite3_exec(db, dbStmt, NULL, NULL, &dbErrorMsg);
      if (rc != SQLITE_OK) {
        dbOkay = false;
        if (rc == SQLITE_FULL) {
          fprintf(stderr, "Cannot add data to the database, the (temporary) disk is full.");
        } else {
          fprintf(stderr, "Unknown SQL error while inserting ebooks: %s\n", dbErrorMsg);
          fprintf(stderr, "dbStmt: %s\n", dbStmt);
        }

        sqlite3_free(dbErrorMsg);
      }

      sqlite3_free(dbStmt);
    }

    //--------------------------------------------------------------------------
    if (dbOkay) {
      if (!hasOverwrite) {
        gtk_list_store_insert_with_values(GTK_LIST_STORE(model), &iter, -1,
          FORMAT_COLUMN, &lowerFiletype[1],
          AUTHOR_COLUMN, author == NULL ? "Unknown" : author,
          TITLE_COLUMN, title == NULL ? cleanedFileName : title,
          CATEGORY_COLUMN, "",
          TAGS_COLUMN, "",
          FILENAME_COLUMN, cleanedFileName,
          PRIORITY_COLUMN, 0,
          READ_COLUMN, false,
          -1
        );
      }

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


int read_out_path(char* pathRootData, GtkWidget *statusBar, guint contextId, GtkWidget *progressBar, unsigned int i, unsigned int filesToAdd, GtkTreeModel *model, sqlite3* db, bool doOverwriteOnImport) {
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

        filesAdded += read_out_path(completePath, statusBar, contextId, progressBar, i, filesToAdd, model, db, doOverwriteOnImport);
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
            if (read_and_add_file_to_model(completePath, true, statusBar, contextId, false, progressBar, i, filesToAdd, true, model, db, doOverwriteOnImport)) {
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

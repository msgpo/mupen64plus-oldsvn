/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - main_gtk.c                                              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
 *   Copyright (C) 2002 Blight                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* main_gtk.c - Handles the main window and 'glues' it with other windows */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <SDL_thread.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "main_gtk.h"
#include "aboutdialog.h"
#include "cheatdialog.h"
#include "configdialog.h"
#include "rombrowser.h"
#include "romproperties.h"
#include "icontheme.h"

#include "../version.h"
#include "../main.h"
#include "../config.h"
#include "../util.h"
#include "../translate.h"
#include "../savestates.h"
#include "../plugin.h"
#include "../rom.h"

#ifdef DBG
#include "debugger/registers.h"     //temporary includes for the debugger menu
#include "debugger/breakpoints.h"   //these can be removed when the main gui
#include "debugger/regTLB.h"        //window no longer needs to know if each
#include "debugger/memedit.h"       //debugger window is open
#include "debugger/varlist.h"

#include "../../debugger/debugger.h"
#endif

/* Necessary function prototypes. */
static void callback_start_emulation(GtkWidget *widget, gpointer data);
static void callback_stop_emulation(GtkWidget *widget, gpointer data);
static void callback_open_rom(GtkWidget *widget, gpointer data);
static void callback_theme_changed(GtkWidget *widget, gpointer data);
static void create_mainWindow(void);

/* Globals. */
SMainWindow g_MainWindow;
static Uint32 g_GuiThreadID = 0; /* Main gui thread. */

/*********************************************************************************************************
* GUI interfaces.
*/

/* Parse gui-specific arguments, remove them from argument list,
 * and create gtk gui in thread-safe manner, but do not display.
 */
void gui_init(int *argc, char ***argv)
{
    /* Initialize multi-threading support. */
    g_thread_init(NULL);
    gdk_threads_init();

    /* Save main gui thread handle. */
    g_GuiThreadID = SDL_ThreadID();

    /* Call gtk to parse arguments. */
    gtk_init(argc, argv);

    /* Set application name. */
    g_set_application_name(MUPEN_NAME);

    /* Setup gtk theme. */
    g_MainWindow.iconTheme = gtk_icon_theme_get_default();
    g_MainWindow.fallbackIcons = check_icon_theme();
    g_signal_connect(g_MainWindow.iconTheme, "changed", G_CALLBACK(callback_theme_changed), NULL);
    g_MainWindow.dialogErrorImage = g_MainWindow.dialogQuestionImage = NULL;

    create_mainWindow();
    create_configDialog();
    create_romPropDialog();
    callback_theme_changed(NULL, NULL);
}

/* Display GUI components to the screen. */
void gui_display(void)
{
    gtk_widget_show_all(g_MainWindow.toplevelVBox);
    if(!(config_get_bool("ToolbarVisible",TRUE)))
        gtk_widget_hide(g_MainWindow.toolBar);
    if(!(config_get_bool("FilterVisible",TRUE)))
        gtk_widget_hide(g_MainWindow.filterBar);
    if(!(config_get_bool("StatusbarVisible",TRUE)))
        gtk_widget_hide(g_MainWindow.statusBarHBox);
    gtk_widget_show(g_MainWindow.window);
}

/* Save GUI window properties and destroy widgets. */
void gui_destroy(void)
{
    gint width, height, xposition, yposition;

    gtk_window_get_size(GTK_WINDOW(g_MainWindow.window), &width, &height);
    gtk_window_get_position(GTK_WINDOW(g_MainWindow.window), &xposition, &yposition); 

    config_put_number("MainWindowWidth",width);
    config_put_number("MainWindowHeight",height);
    config_put_number("MainWindowXPosition",xposition);
    config_put_number("MainWindowYPosition",yposition);

    config_put_bool("ToolbarVisible", GTK_WIDGET_VISIBLE(g_MainWindow.toolBar));
    config_put_bool("FilterVisible", GTK_WIDGET_VISIBLE(g_MainWindow.filterBar));
    config_put_bool("StatusbarVisible", GTK_WIDGET_VISIBLE(g_MainWindow.statusBarHBox));

    gtk_widget_destroy(g_MainWindow.window);
    gtk_widget_destroy(g_ConfigDialog.dialog);
    gtk_widget_destroy(g_RomPropDialog.dialog);
}

/* Give control of thread to gtk. */
void gui_main_loop(void)
{
    gtk_main();
    gui_destroy();
    gdk_threads_leave();
}

/* updaterombrowser() accesses g_romcahce.length and adds upto roms to the rombrowser.
 * The clear flag tells the GUI to clear the rombrowser first.
 */
void updaterombrowser( unsigned int roms, unsigned short clear )
{
    Uint32 self = SDL_ThreadID();

    /* If we're calling from a thread other than the main gtk thread, take gdk lock. */
    if (self != g_GuiThreadID)
        gdk_threads_enter();

    rombrowser_refresh(roms, clear);

    gdk_threads_leave();
    while(g_main_context_iteration(NULL, FALSE));
    gdk_threads_enter();

    if (self != g_GuiThreadID)
        gdk_threads_leave();

    return;
}

/* Display either an informational message to the status bar, a yes / no confirmation 
 * dialog, or an error dialog.
 */
int gui_message(unsigned char messagetype, const char *format, ...)
{
    if(!gui_enabled())
        return 0;

    va_list ap;
    char buffer[2049];
    Uint32 self = SDL_ThreadID();
    gint response = 0;

    va_start(ap, format);
    vsnprintf(buffer, 2048, format, ap);
    buffer[2048] = '\0';
    va_end(ap);

    /* If we're calling from a thread other than the main gtk thread, take gdk lock. */
    if (self != g_GuiThreadID)
        gdk_threads_enter();

    if (messagetype == 0)
        {
        int counter;
        for( counter = 0; counter < strlen(buffer); ++counter )
            {
            if(buffer[counter]=='\n')
                {
                buffer[counter]='\0';
                break;
                }
            }

        gtk_statusbar_pop(GTK_STATUSBAR(g_MainWindow.statusBar), gtk_statusbar_get_context_id( GTK_STATUSBAR(g_MainWindow.statusBar), "status"));
        gtk_statusbar_push(GTK_STATUSBAR(g_MainWindow.statusBar), gtk_statusbar_get_context_id( GTK_STATUSBAR(g_MainWindow.statusBar), "status"), buffer);
        }
    else if(messagetype>0)
        {
        GtkWidget *dialog, *hbox, *label, *icon;

        hbox = gtk_hbox_new(FALSE, 10);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

        if(messagetype==1)
            {
            dialog = gtk_dialog_new_with_buttons(tr("Error"), GTK_WINDOW(g_MainWindow.window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK, GTK_RESPONSE_NONE,NULL);

            icon = g_MainWindow.dialogErrorImage = gtk_image_new();
            set_icon(g_MainWindow.dialogErrorImage, "dialog-error", 32, FALSE);
            }
        else if(messagetype==2)
            {
            dialog = gtk_dialog_new_with_buttons(tr("Confirm"), GTK_WINDOW(g_MainWindow.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_YES, GTK_RESPONSE_ACCEPT, GTK_STOCK_NO, GTK_RESPONSE_REJECT, NULL);

            icon = g_MainWindow.dialogErrorImage = gtk_image_new();
            set_icon(g_MainWindow.dialogErrorImage, "dialog-question", 32, FALSE);
            }

        gtk_misc_set_alignment(GTK_MISC(icon), 0, 0);
        gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);

        label = gtk_label_new(buffer);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 0);

        gtk_widget_show_all(dialog);

        if(messagetype==1)
            g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
        else if(messagetype==2)
            {
            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            g_MainWindow.dialogErrorImage = NULL;
            }
        }

    gdk_threads_leave();
    g_main_context_iteration(NULL, FALSE);
    gdk_threads_enter();

   if (self != g_GuiThreadID)
       gdk_threads_leave();

    g_MainWindow.dialogErrorImage = g_MainWindow.dialogQuestionImage = NULL;
    return response == GTK_RESPONSE_ACCEPT;
}

/*********************************************************************************************************
* Callbacks.
*/

static gint callback_mainWindowDeleteEvent(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_main_quit();
    return TRUE; /* Don't delete main window here. */
}

/* If theme changes, update application with images from new theme, or fallbacks. */
static void callback_theme_changed(GtkWidget *widget, gpointer data)
{
    g_MainWindow.fallbackIcons = check_icon_theme();
    short size = config_get_number("ToolbarSize", 22);

    set_icon(g_MainWindow.openButtonImage, "mupen64cart", size, TRUE);
    set_icon(g_MainWindow.playButtonImage, "media-playback-start", size, FALSE);
    set_icon(g_MainWindow.pauseButtonImage, "media-playback-pause", size, FALSE);
    set_icon(g_MainWindow.stopButtonImage, "media-playback-stop", size, FALSE);
    set_icon(g_MainWindow.saveStateButtonImage, "document-save", size, FALSE);
    set_icon(g_MainWindow.loadStateButtonImage, "document-revert", size, FALSE);
    set_icon(g_MainWindow.configureButtonImage, "preferences-system", size, FALSE);
    set_icon(g_MainWindow.fullscreenButtonImage, "view-fullscreen", size, FALSE);

    if(g_MainWindow.dialogErrorImage)
        set_icon(g_MainWindow.dialogErrorImage, "dialog-error", size, FALSE);
    if(g_MainWindow.dialogQuestionImage)
        set_icon(g_MainWindow.dialogQuestionImage, "dialog-question", size, FALSE);

    set_icon(g_ConfigDialog.graphicsImage, "video-display", 32, FALSE);
    set_icon(g_ConfigDialog.audioImage, "audio-card", 32, FALSE);
    set_icon(g_ConfigDialog.inputImage, "input-gaming", 32, FALSE);

    set_icon(g_MainWindow.playMenuImage, "media-playback-start", 16, FALSE);
    set_icon(g_MainWindow.pauseMenuImage, "media-playback-pause", 16, FALSE);
    set_icon(g_MainWindow.stopMenuImage, "media-playback-stop", 16, FALSE);

    set_icon(g_MainWindow.openRomMenuImage, "mupen64cart", 16, TRUE);
    set_icon(g_MainWindow.closeRomMenuImage, "window-close", 16, FALSE);
    set_icon(g_MainWindow.quitMenuImage, "application-exit", 16, FALSE);

    set_icon(g_MainWindow.configureMenuImage, "preferences-system", 16, FALSE);
    set_icon(g_MainWindow.graphicsMenuImage, "video-display", 16, FALSE);
    set_icon(g_MainWindow.audioMenuImage, "audio-card", 16, FALSE);
    set_icon(g_MainWindow.inputMenuImage, "input-gaming", 16, FALSE);
    set_icon(g_MainWindow.rspMenuImage, "cpu", 16, TRUE);
    set_icon(g_MainWindow.fullscreenMenuImage, "view-fullscreen", 16, FALSE);

    set_icon(g_MainWindow.saveStateMenuImage, "document-save", 16, FALSE);
    set_icon(g_MainWindow.saveStateAsMenuImage, "document-save-as", 16, FALSE);
    set_icon(g_MainWindow.loadStateMenuImage, "document-revert", 16, FALSE);
    set_icon(g_MainWindow.loadStateAsMenuImage, "document-open", 16, FALSE);

    set_icon(g_MainWindow.aboutMenuImage, "gtk-about", 16, FALSE);
}

/* Ask user to load a rom. If emulation is running, give warning. */
static void callback_open_rom(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        {
        if(!gui_message(2, tr("Emulation is running. Do you want to\nstop it and load a rom?")))
            return;
        callback_stop_emulation(NULL, NULL);
        }

    /* Get rom file from user. */
    GtkWidget* file_chooser = gtk_file_chooser_dialog_new(tr("Open Rom..."), GTK_WINDOW(g_MainWindow.window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

    /* Add filter for rom file types. */
    GtkFileFilter* file_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(file_filter, "N64 ROM (*.z64, *.v64, *.n64, *.gz, *.zip. *.bz2, *.lzma *.7z)");
    gtk_file_filter_add_mime_type(file_filter, "application/x-gzip");
    gtk_file_filter_add_mime_type(file_filter, "application/zip");
    gtk_file_filter_add_mime_type(file_filter, "application/x-bzip2");
    gtk_file_filter_add_mime_type(file_filter, "application/x-7z");
    gtk_file_filter_add_pattern(file_filter, "*.[zZ]64");
    gtk_file_filter_add_pattern(file_filter, "*.lzma");
    gtk_file_filter_add_pattern(file_filter, "*.7z");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), file_filter);

    /* Add filter for "all files." */
    file_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(file_filter, "All files (*.*)");
    gtk_file_filter_add_pattern(file_filter, "*");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), file_filter);

    if(gtk_dialog_run(GTK_DIALOG(file_chooser))==GTK_RESPONSE_ACCEPT)
        {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (file_chooser));
        gtk_widget_hide(file_chooser); /* Hide dialog while rom is loading. */
        open_rom(filename, 0);
        g_free(filename);
        }

    gtk_widget_destroy(file_chooser);
}

static void callback_close_rom(GtkWidget *widget, gpointer data)
{
    close_rom();
}

/* If a rom is loaded, start emulation. Else attempt to load rom, first from rombrowser
 * then, if none selected, ask the user.
 */
static void callback_start_emulation(GtkWidget *widget, gpointer data)
{
    if(!rom)
        {
        GList *list = NULL, *llist = NULL;
        cache_entry *entry;
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romDisplay));
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_MainWindow.romDisplay));

        list = gtk_tree_selection_get_selected_rows (selection, &model);

        if(!list) 
            {
            if(gui_message(2, tr("There is no Rom loaded. Do you want to load one?")))
                callback_open_rom(NULL, NULL);
            return;
            }
        else
            {
            llist = g_list_first(list);

            gtk_tree_model_get_iter(model, &iter,(GtkTreePath *) llist->data);
            gtk_tree_model_get(model, &iter, 22, &entry, -1);

            g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
            g_list_free(list);

            if(open_rom(entry->filename, entry->archivefile)==0)
                startEmulation();
            else
                return;
            }
        }

    startEmulation();
}

static void callback_pause_emulation(GtkWidget *widget, gpointer data)
{
    pauseContinueEmulation();
}

static void callback_stop_emulation(GtkWidget *widget, gpointer data)
{
    stopEmulation();
}

static void callback_save_state(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        savestates_job |= SAVESTATE;
    else
        error_message(tr("Emulation is not running."));
}

/* Save state as. Launch a file chooser so user can specify file. */
static void callback_save_state_as(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        {
        GtkWidget *file_chooser;

        file_chooser = gtk_file_chooser_dialog_new(tr("Save as..."), GTK_WINDOW(g_MainWindow.window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

        /* Set default filename. */
        char* filename = savestates_get_filename();
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(file_chooser), filename);
        free(filename);

        if(gtk_dialog_run(GTK_DIALOG(file_chooser))==GTK_RESPONSE_ACCEPT)
            {
            gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));

            savestates_select_filename(filename);
            savestates_job |= SAVESTATE;

            g_free(filename);
            }

        gtk_widget_destroy (file_chooser);
        }
    else
        error_message(tr("Emulation is not running."));
}

static void callback_load_state(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        savestates_job |= LOADSTATE;
    else
        error_message(tr("Emulation is not running."));
}

/* Load state from. Open a file chooser so user can specify file. */
static void callback_load_state_from(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        {
        GtkWidget *file_chooser;

        file_chooser = gtk_file_chooser_dialog_new(tr("Load..."), GTK_WINDOW(g_MainWindow.window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

        if(gtk_dialog_run(GTK_DIALOG(file_chooser))==GTK_RESPONSE_ACCEPT)
            {
            gchar *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_chooser));

            savestates_select_filename(filename);
            savestates_job |= LOADSTATE;

            g_free(filename);
            }

        gtk_widget_destroy (file_chooser);
        }
    else
        error_message(tr("Emulation is not running."));
}

/* User changed savestate slot. */
static void callback_change_slot(GtkMenuItem *item, int slot)
{
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
        {
        if(slot!=savestates_get_slot()) /* Only actually change slot when not a GUI update. */
            savestates_select_slot(slot);
        }
}

/* User opened save slot menu. Make sure current save slot is selected. */
static void callback_update_slot(GtkMenuItem *item, GSList *slots)
{
    unsigned int i, slot;
    GtkWidget* slotItem;

    for(i = 0; i < g_slist_length(slots); i++)
        {
        slotItem = GTK_WIDGET(g_slist_nth_data(slots, i));
        slot = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(slotItem), "slot_num"));

        if(slot==savestates_get_slot())
            {
            if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(slotItem)))
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(slotItem), TRUE);
            break;
            }
        }
}

static void callback_configure(GtkWidget *widget, gpointer data)
{
    gtk_widget_show_all(g_ConfigDialog.dialog);
}

static void callback_configure_graphics(GtkWidget *widget, gpointer data)
{
    char *name = plugin_name_by_filename(config_get_string("Gfx Plugin", ""));
    if(name)
        plugin_exec_config(name);
    else
        {
        if(gui_message(2, tr("No graphics plugin selected! Do you\nwant to select one?")))
            {
            gtk_notebook_set_page(GTK_NOTEBOOK(g_ConfigDialog.notebook), gtk_notebook_page_num(GTK_NOTEBOOK(g_ConfigDialog.notebook), g_ConfigDialog.configPlugins));
            gtk_widget_show_all(g_ConfigDialog.dialog);
            }
        }
}

static void callback_configure_audio(GtkWidget *widget, gpointer data)
{
    char *name = plugin_name_by_filename(config_get_string("Audio Plugin", ""));
    if(name)
        plugin_exec_config(name);
    else
        {
        if(gui_message(2, tr("No audio plugin selected! Do you\nwant to select one?")))
            {
            gtk_notebook_set_page(GTK_NOTEBOOK(g_ConfigDialog.notebook), gtk_notebook_page_num(GTK_NOTEBOOK(g_ConfigDialog.notebook), g_ConfigDialog.configPlugins));
            gtk_widget_show_all(g_ConfigDialog.dialog);
            }
        }
}

static void callback_configure_input( GtkWidget *widget, gpointer data )
{
    char *name = plugin_name_by_filename(config_get_string("Input Plugin", ""));
    if(name)
        plugin_exec_config(name);
    else
        {
        if(gui_message(2, tr("No input plugin selected! Do you\nwant to select one?")))
            {
            gtk_notebook_set_page(GTK_NOTEBOOK(g_ConfigDialog.notebook), gtk_notebook_page_num(GTK_NOTEBOOK(g_ConfigDialog.notebook), g_ConfigDialog.configPlugins));
            gtk_widget_show_all(g_ConfigDialog.dialog);
            }
        }
}

static void callback_configure_rsp(GtkWidget *widget, gpointer data)
{
    char *name = plugin_name_by_filename(config_get_string( "RSP Plugin", ""));
    if(name)
        plugin_exec_config(name);
    else
        {
        if(gui_message(2, tr("No RSP plugin selected! Do you\nwant to select one?")))
            {
            gtk_notebook_set_page(GTK_NOTEBOOK(g_ConfigDialog.notebook), gtk_notebook_page_num(GTK_NOTEBOOK(g_ConfigDialog.notebook), g_ConfigDialog.configPlugins));
            gtk_widget_show_all(g_ConfigDialog.dialog);
            }
        }
}

/* Enter full screen mode. */
static void callback_fullscreen(GtkWidget *widget, gpointer data)
{
    if(g_EmulatorRunning)
        changeWindow();
}

static void callback_toggle_view(GtkWidget *widget, gpointer data)
{
    GtkWidget *toggle_object = NULL;
    switch(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "toggle_object")))
        {
        case TOOLBAR:
            toggle_object = g_MainWindow.toolBar;
            break;
        case FILTER:
            toggle_object = g_MainWindow.filterBar;
            gtk_entry_set_text(GTK_ENTRY(g_MainWindow.filter),"");
            break;
        case STATUSBAR:
            toggle_object = g_MainWindow.statusBarHBox;
            break;
        }

    if(toggle_object==NULL)
        return;

    if(GTK_WIDGET_VISIBLE(toggle_object))
        gtk_widget_hide(toggle_object);
    else
        gtk_widget_show(toggle_object);
}

static void callback_update_check_item(GtkWidget *widget, gpointer data)
{
    GtkWidget *toggle_object = NULL;
    switch(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(widget), "toggle_object")))
        {
        case TOOLBAR:
            toggle_object = g_MainWindow.toolBar;
            break;
        case FILTER:
            toggle_object = g_MainWindow.filterBar;
            break;
        case STATUSBAR:
            toggle_object = g_MainWindow.statusBarHBox;
            break;
        }

    if(toggle_object==NULL)
        return;

     g_signal_handlers_block_by_func(widget, callback_toggle_view, NULL);
     gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget),GTK_WIDGET_VISIBLE(toggle_object));
     g_signal_handlers_unblock_by_func(widget, callback_toggle_view, NULL);
}

static void callback_update_view(GtkWidget *widget, gpointer data)
{
    gtk_container_foreach(GTK_CONTAINER(widget), callback_update_check_item, NULL);
}

#ifdef DBG
static GtkWidget   *debuggerRegistersShow;
static GtkWidget   *debuggerBreakpointsShow;
static GtkWidget   *debuggerTLBShow;
static GtkWidget   *debuggerMemoryShow;
static GtkWidget   *debuggerVariablesShow;

static void callback_debuggerEnableToggled(GtkWidget *widget, gpointer data)
{
    int emuRestart = 0;

    if(g_EmulatorRunning)
        {
        if(gui_message(2, tr("Emulation needs to be restarted in order\nto activate the debugger. Do you want\nthis to happen?")))
            {
            callback_stop_emulation( NULL, NULL );
            emuRestart = 1;
            }
        }

    g_DebuggerEnabled = gtk_check_menu_item_get_active((GtkCheckMenuItem*)widget);

    if(g_DebuggerEnabled==TRUE)
        {
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerRegistersShow), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerBreakpointsShow), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerTLBShow), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerMemoryShow), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerVariablesShow), TRUE);
        }
    else
        {
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerRegistersShow), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerBreakpointsShow), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerTLBShow), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerMemoryShow), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(debuggerVariablesShow), FALSE);
        }

    if(emuRestart==1)
        callback_start_emulation(NULL,NULL);
}

static void callback_debuggerWindowShow(GtkWidget *widget, gpointer window)
{
    switch(GPOINTER_TO_UINT(window))
    {
    case 1:
        if(registers_opened==0)
        init_registers();
        break;
    case 2:
        if(breakpoints_opened==0)
        init_breakpoints();
        break;
    case 3:
        if(regTLB_opened==0)
        init_TLBwindow();
        break;
    case 4:
        if(memedit_opened==0)
        init_memedit();
        break;
    case 5:
        if(varlist_opened==0)
        init_varlist();
        break;
    }
}
#endif // DBG

/*********************************************************************************************************
* GUI creation.
*/

static int create_menubar(void)
{
    GtkWidget* menu;
    GtkWidget* menuitem;
    GtkWidget* item;
    GtkWidget* submenu;
    GtkWidget* submenuitem;
    int i;

    /* Create toggable accelerator groups. */
    g_MainWindow.accelGroup = gtk_accel_group_new();
    g_MainWindow.accelUnsafe = gtk_accel_group_new();
    g_MainWindow.accelUnsafeActive = TRUE;
    gtk_window_add_accel_group(GTK_WINDOW(g_MainWindow.window), g_MainWindow.accelGroup);
    gtk_window_add_accel_group(GTK_WINDOW(g_MainWindow.window), g_MainWindow.accelUnsafe);

    g_MainWindow.menuBar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(g_MainWindow.toplevelVBox), g_MainWindow.menuBar, FALSE, FALSE, 0);

    /* File menu. */
    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_mnemonic(tr("_File"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_MainWindow.menuBar), menuitem);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Open Rom..."));
    g_MainWindow.openRomMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.openRomMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_open_rom), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Close Rom"));
    g_MainWindow.closeRomMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.closeRomMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_w, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_close_rom), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Quit"));
    g_MainWindow.quitMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.quitMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_mainWindowDeleteEvent), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    /* Emulation menu. */
    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_mnemonic(tr("_Emulation"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_MainWindow.menuBar), menuitem);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Start"));
    g_MainWindow.playMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.playMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_start_emulation), NULL );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Pause"));
    g_MainWindow.pauseMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.pauseMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_Pause, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_pause_emulation), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("S_top"));
    g_MainWindow.stopMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.stopMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_Escape, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_stop_emulation), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_image_menu_item_new_with_mnemonic(tr("Sa_ve State"));
    g_MainWindow.saveStateMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.saveStateMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_F5, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_save_state), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("Save State _as..."));
    g_MainWindow.saveStateAsMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.saveStateAsMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_save_state_as), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Load State"));
    g_MainWindow.loadStateMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.loadStateMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_F7, 0, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_load_state), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("L_oad State from..."));
    g_MainWindow.loadStateAsMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.loadStateAsMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_load_state_from), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    /* Slot submenu. */
    submenuitem = gtk_menu_item_new_with_mnemonic(tr("_Current Save State Slot"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), submenuitem);
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenuitem), submenu);

    item = gtk_radio_menu_item_new(NULL);
    char buffer[128];
    for (i = 0; i < 10; ++i)
        {
        snprintf(buffer, 128, tr(" Slot _%d"), i); /* Mnemonic needed even with accelerator. */

        item = gtk_radio_menu_item_new_with_mnemonic_from_widget(GTK_RADIO_MENU_ITEM(item), buffer); 
        g_object_set_data(G_OBJECT(item), "slot_num", GUINT_TO_POINTER(i));
        gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelUnsafe, GDK_0+i, 0, GTK_ACCEL_VISIBLE);
        g_signal_connect(item, "activate", G_CALLBACK(callback_change_slot), GUINT_TO_POINTER(i));
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        }

    /* Set callback so save state slot menu is updated everytime it's opened. */
    g_signal_connect(submenuitem, "activate", G_CALLBACK(callback_update_slot), gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item)));

    /* Options menu. */
    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_mnemonic(tr("_Options"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_MainWindow.menuBar), menuitem);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Configure..."));
    g_MainWindow.configureMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.configureMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_configure), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Video Settings..."));
    g_MainWindow.graphicsMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.graphicsMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_configure_graphics), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Audio Settings..."));
    g_MainWindow.audioMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.audioMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_configure_audio), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Input Settings..."));
    g_MainWindow.inputMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.inputMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_configure_input), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_RSP Settings..."));
    g_MainWindow.rspMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.rspMenuImage);
    g_signal_connect(item, "activate", G_CALLBACK(callback_configure_rsp), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    item = gtk_menu_item_new_with_mnemonic(tr("C_heats..."));
    g_signal_connect(item, "activate", G_CALLBACK(cb_cheatDialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_Full Screen"));
    g_MainWindow.fullscreenMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.fullscreenMenuImage);
    gtk_widget_add_accelerator(item, "activate", g_MainWindow.accelGroup, GDK_Return, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(item, "activate", G_CALLBACK(callback_fullscreen), NULL );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    /* View menu. */
    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_mnemonic(tr("_View"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    g_signal_connect_object(menuitem, "activate", G_CALLBACK(callback_update_view), menu, G_CONNECT_SWAPPED);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_MainWindow.menuBar), menuitem);

    item = gtk_check_menu_item_new_with_mnemonic(tr(" _Toolbar"));
    g_object_set_data(G_OBJECT(item), "toggle_object", GUINT_TO_POINTER(TOOLBAR));
    g_signal_connect(item, "toggled", G_CALLBACK(callback_toggle_view), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_check_menu_item_new_with_mnemonic(tr(" _Filter"));
    g_object_set_data(G_OBJECT(item), "toggle_object", GUINT_TO_POINTER(FILTER));
    g_signal_connect(item, "toggled", G_CALLBACK(callback_toggle_view), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_check_menu_item_new_with_mnemonic(tr(" _Statusbar"));
    g_object_set_data(G_OBJECT(item), "toggle_object", GUINT_TO_POINTER(STATUSBAR));
    g_signal_connect(item, "toggled", G_CALLBACK(callback_toggle_view), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    // debugger menu
#ifdef DBG
    GtkWidget   *debuggerMenu;
    GtkWidget   *debuggerMenuItem;
    GtkWidget   *debuggerEnableItem;
    GtkWidget   *debuggerSeparator;

    debuggerMenu = gtk_menu_new();
    debuggerMenuItem = gtk_menu_item_new_with_mnemonic(tr("_Debugger"));
    gtk_menu_item_set_submenu( GTK_MENU_ITEM(debuggerMenuItem), debuggerMenu );
    debuggerEnableItem = gtk_check_menu_item_new_with_mnemonic(tr(" _Enable"));
    debuggerSeparator = gtk_menu_item_new();
    debuggerRegistersShow = gtk_menu_item_new_with_mnemonic(tr("_Registers"));
    debuggerBreakpointsShow = gtk_menu_item_new_with_mnemonic(tr("_Breakpoints"));
    debuggerTLBShow = gtk_menu_item_new_with_mnemonic(tr("_TLB"));
    debuggerMemoryShow = gtk_menu_item_new_with_mnemonic(tr("_Memory"));
    debuggerVariablesShow = gtk_menu_item_new_with_mnemonic(tr("_Variables"));

    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerEnableItem );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerSeparator );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerRegistersShow );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerBreakpointsShow );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerTLBShow );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerMemoryShow );
    gtk_menu_append( GTK_MENU(debuggerMenu), debuggerVariablesShow );

    if(g_DebuggerEnabled)
      gtk_check_menu_item_set_active( (GtkCheckMenuItem *) debuggerEnableItem, TRUE );
    else 
      {
    gtk_widget_set_sensitive( GTK_WIDGET(debuggerRegistersShow), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(debuggerBreakpointsShow), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(debuggerTLBShow), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(debuggerMemoryShow), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(debuggerVariablesShow), FALSE);
      }

    g_signal_connect(debuggerEnableItem, "toggled", G_CALLBACK(callback_debuggerEnableToggled), NULL);
    g_signal_connect(debuggerRegistersShow, "activate", G_CALLBACK(callback_debuggerWindowShow), (gpointer)1);
    g_signal_connect(debuggerBreakpointsShow, "activate", G_CALLBACK(callback_debuggerWindowShow), (gpointer)2);
    g_signal_connect(debuggerTLBShow, "activate", G_CALLBACK(callback_debuggerWindowShow), (gpointer)3);
    g_signal_connect(debuggerMemoryShow, "activate", G_CALLBACK(callback_debuggerWindowShow), (gpointer)4);
    g_signal_connect(debuggerVariablesShow, "activate", G_CALLBACK(callback_debuggerWindowShow), (gpointer)5);
    gtk_menu_bar_append( GTK_MENU_BAR(g_MainWindow.menuBar), debuggerMenuItem );
#endif // DBG

    /* Help menu. */
    menu = gtk_menu_new();
    menuitem = gtk_menu_item_new_with_mnemonic(tr("_Help"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_MainWindow.menuBar), menuitem);

    item = gtk_image_menu_item_new_with_mnemonic(tr("_About..."));
    g_MainWindow.aboutMenuImage = gtk_image_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), g_MainWindow.aboutMenuImage);
    gtk_signal_connect_object(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(callback_aboutMupen), (gpointer)NULL );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
}

static void create_toolbar(void)
{
    g_MainWindow.toolBar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(g_MainWindow.toolBar), GTK_ORIENTATION_HORIZONTAL);
    switch(config_get_number("ToolbarStyle", 0))
        {
        case 1:
            gtk_toolbar_set_style(GTK_TOOLBAR(g_MainWindow.toolBar), GTK_TOOLBAR_TEXT);
            break;
        case 2:
            gtk_toolbar_set_style(GTK_TOOLBAR(g_MainWindow.toolBar), GTK_TOOLBAR_BOTH);
            break;
        default:
            gtk_toolbar_set_style(GTK_TOOLBAR(g_MainWindow.toolBar), GTK_TOOLBAR_ICONS);
        }
    gtk_toolbar_set_tooltips(GTK_TOOLBAR(g_MainWindow.toolBar), TRUE);

    g_MainWindow.openButtonImage = gtk_image_new();
    g_MainWindow.playButtonImage = gtk_image_new();
    g_MainWindow.pauseButtonImage = gtk_image_new();
    g_MainWindow.stopButtonImage = gtk_image_new();
    g_MainWindow.saveStateButtonImage = gtk_image_new();
    g_MainWindow.loadStateButtonImage = gtk_image_new();
    g_MainWindow.fullscreenButtonImage = gtk_image_new();
    g_MainWindow.configureButtonImage = gtk_image_new();

    /* Add icons to toolbar. */
    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Open Rom"), tr("Open Rom"), "", g_MainWindow.openButtonImage, G_CALLBACK(callback_open_rom), NULL);

    gtk_toolbar_append_space(GTK_TOOLBAR(g_MainWindow.toolBar));

    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Start"), tr("Start Emulation"), "", g_MainWindow.playButtonImage, G_CALLBACK(callback_start_emulation), NULL);
    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Pause"), tr("Pause/ Continue Emulation"), "", g_MainWindow.pauseButtonImage, G_CALLBACK(callback_pause_emulation), NULL );
    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Stop"), tr("Stop Emulation"), "", g_MainWindow.stopButtonImage, G_CALLBACK(callback_stop_emulation), NULL);

    gtk_toolbar_append_space(GTK_TOOLBAR(g_MainWindow.toolBar));

    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Save State"), tr("Save State"), "", g_MainWindow.saveStateButtonImage, G_CALLBACK(callback_save_state), NULL );
    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Load State"), tr("Load State"), "", g_MainWindow.loadStateButtonImage, G_CALLBACK(callback_load_state), NULL);

    gtk_toolbar_append_space(GTK_TOOLBAR(g_MainWindow.toolBar));

    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Configure"), tr("Configure") ,"", g_MainWindow.configureButtonImage, G_CALLBACK(callback_configure), NULL);
    gtk_toolbar_append_item(GTK_TOOLBAR(g_MainWindow.toolBar), tr("Fullscreen"), tr("Fullscreen"), "", g_MainWindow.fullscreenButtonImage, G_CALLBACK(callback_fullscreen), NULL);

    gtk_box_pack_start(GTK_BOX(g_MainWindow.toplevelVBox), g_MainWindow.toolBar, FALSE, FALSE, 0);
}

static void create_statusbar()
{
    g_MainWindow.statusBarHBox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_end(GTK_BOX(g_MainWindow.toplevelVBox), g_MainWindow.statusBarHBox, FALSE, FALSE, 0);

    g_MainWindow.statusBar = gtk_statusbar_new();
    gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(g_MainWindow.statusBar), FALSE);
    gtk_box_pack_start(GTK_BOX(g_MainWindow.statusBarHBox), g_MainWindow.statusBar, TRUE , TRUE, 0);
}

static void create_mainWindow()
{
    /* Setup main window. */
    gint width, height, xposition, yposition;

    width = config_get_number("MainWindowWidth", 600);
    height = config_get_number("MainWindowHeight", 400);
    xposition = config_get_number("MainWindowXPosition", 0);
    yposition = config_get_number("MainWindowYPosition", 0);

    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gint screenwidth = gdk_screen_get_width(screen);
    gint screenheight = gdk_screen_get_height(screen);

    if(xposition>screenwidth)
        xposition = 0;
    if(yposition>screenheight)
        yposition = 0;

    if(width>screenwidth)
        width = 600;
    if(height>screenheight)
        height = 400;

    if((xposition+width)>screenwidth)
        xposition = screenwidth - width;
    if((yposition+height)>screenheight)
        yposition = screenheight - height;

    g_MainWindow.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_MainWindow.window), MUPEN_NAME " v" MUPEN_VERSION);
    gtk_window_set_default_size(GTK_WINDOW(g_MainWindow.window), width, height);
    gtk_window_move(GTK_WINDOW(g_MainWindow.window), xposition, yposition);

    GdkPixbuf *mupen64plus16, *mupen64plus32;
    mupen64plus16 = gdk_pixbuf_new_from_file(get_iconpath("16x16/mupen64plus.png"), NULL);
    mupen64plus32 = gdk_pixbuf_new_from_file(get_iconpath("32x32/mupen64plus.png"), NULL);

    GList *iconlist = NULL;
    iconlist = g_list_append(iconlist, mupen64plus16);
    iconlist = g_list_append(iconlist, mupen64plus16);

    gtk_window_set_icon_list(GTK_WINDOW(g_MainWindow.window), iconlist);
    /* Edit gtk on quit. */
    gtk_signal_connect(GTK_OBJECT(g_MainWindow.window), "delete_event", GTK_SIGNAL_FUNC(callback_mainWindowDeleteEvent), (gpointer)NULL);

    /* Toplevel vbox, parent to all GUI widgets. */
    g_MainWindow.toplevelVBox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER(g_MainWindow.window), g_MainWindow.toplevelVBox);

    create_menubar();
    create_toolbar();
    create_filter();

    /* Setup rombrowser. */
    int value = config_get_number("RomSortType",16);
    if(value!=GTK_SORT_ASCENDING&&value!=GTK_SORT_DESCENDING)
        {
        g_MainWindow.romSortType = GTK_SORT_ASCENDING;
        config_put_number("RomSortType",GTK_SORT_ASCENDING);
        }
    else
        g_MainWindow.romSortType = value;
    value = config_get_number("RomSortColumn",17);
    if(value<0||value>16)
        {
        g_MainWindow.romSortColumn = 1;
        config_put_number("RomSortColumn",1);
        }
    else
        g_MainWindow.romSortColumn = value;

    create_romBrowser();
    create_statusbar();
}


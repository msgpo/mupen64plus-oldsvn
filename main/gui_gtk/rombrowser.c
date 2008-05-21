/***************************************************************************
 vcrcomp_dialog.h - Handles VCR mode GUI elements
----------------------------------------------------------------------------
Began                : Sat Nov 9 2002
Copyright            : (C) 2002 by blight
Email                : blight@Ashitaka
****************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "rombrowser.h"
#include "romproperties.h"
#include "../main.h"
#include "main_gtk.h"
#include "../config.h"
#include "../translate.h"

#include "../rom.h"
#include "../mupenIniApi.h"
#include "../../memory/memory.h"    // sl()

#include <gtk/gtk.h>
#include <zlib.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

// rom file extensions
static const char *g_romFileExtensions[] = 
{
    ".rom", ".v64", ".z64", ".gz", ".zip", ".n64", NULL
};

static void setup_view (GtkWidget *view);

/*********************************************************************************************************
* globals
*/
GList *g_RomList = NULL;
static GList *g_RomListCache = NULL; // roms in cache
static GdkPixbuf *australia, *europe, *france, *germany, *italy, *japan, *spain, *usa, *japanusa, *n64cart;
static GtkWidget *playRomItem;
static GtkWidget *romPropertiesItem;
static GtkWidget *refreshRomBrowserItem;
static int g_iNumRoms = 0;
static int g_iSortColumn = 0; // sort column
static GtkSortType g_SortType = GTK_SORT_ASCENDING; // sort type (ascending/descending)

// function to fill a SRomEntry structure
static void romentry_fill( SRomEntry *entry )
{
    // fill in defaults
    if( strlen( entry->cName ) == 0 )
    {
        if( strlen( entry->info.cGoodName ) == 0 )
            strcpy( entry->cName, entry->info.cName );
        else
            strcpy( entry->cName, entry->info.cGoodName );
    }

    entry->flag = n64cart;
    sprintf( entry->cSize, "%.1f MBits", (float)(entry->info.iSize / (float)0x20000) );
    switch( entry->info.cCountry )
    {
    case 0:    /* Demo */
        strcpy( entry->cCountry, tr("Demo") );
        break;

    case '7':  /* Beta */
        strcpy( entry->cCountry, tr("Beta") );
        break;

    case 0x41: /* Japan / USA */
        strcpy( entry->cCountry, tr("USA/Japan") );
        entry->flag = japanusa; 
        break;

    case 0x44: /* Germany */
        strcpy( entry->cCountry, tr("Germany") );
        entry->flag = germany;
        break;

    case 0x45: /* USA */
        strcpy( entry->cCountry, tr("USA") );
        entry->flag = usa;
        break;

    case 0x46: /* France */
        strcpy( entry->cCountry, tr("France") );
        entry->flag = france;
        break;

    case 'I':  /* Italy */
        strcpy( entry->cCountry, tr("Italy") );
        entry->flag = italy;
        break;

    case 0x4A: /* Japan */
        strcpy( entry->cCountry, tr("Japan") );
        entry->flag = japan;
        break;

    case 'S':  /* Spain */
        strcpy( entry->cCountry, tr("Spain") );
        entry->flag = spain;
        break;

    case 0x55: case 0x59:  /* Australia */
        sprintf( entry->cCountry, tr("Australia (0x%2.2X)"), entry->info.cCountry );
        entry->flag = australia;
        break;

    case 0x50: case 0x58: case 0x20:
    case 0x21: case 0x38: case 0x70:
        sprintf( entry->cCountry, tr("Europe (0x%02X)"), entry->info.cCountry );
        entry->flag = europe;
        break;

    default:
        sprintf( entry->cCountry, tr("Unknown (0x%02X)"), entry->info.cCountry );
        break;
    }
}

/*********************************************************************************************************
* cache functions
*/
void rombrowser_readCache( void )
{
    char filename[PATH_MAX];
    gzFile *f;
    int i;
    SRomEntry *entry;

    snprintf( filename, PATH_MAX, "%srombrowser.cache", get_configpath() );
    f = gzopen( filename, "rb" );
    if( !f )
        return;

    // free old list
    for( i = 0; i < g_list_length( g_RomListCache ); i++ )
    {
        entry = (SRomEntry *)g_list_nth_data( g_RomListCache, i );
        free( entry );
    }
    g_list_free( g_RomListCache );
    g_RomListCache = NULL;

    // number of entries
    gzread( f, &g_iNumRoms, sizeof( g_iNumRoms ) );

    // entries
    for( i = 0; i < g_iNumRoms; i++ )
    {
        entry = malloc( sizeof( SRomEntry ) );
        if( !entry )
        {
            printf( "%s, %c: Out of memory!\n", __FILE__, __LINE__ );
            continue;
        }
        gzread( f, entry->cFilename, sizeof( char ) * PATH_MAX );
        gzread( f, &entry->info, sizeof( entry->info ) );

        if( access( entry->cFilename, F_OK ) < 0 )
            continue;

        romentry_fill( entry );

        // append to list
        g_RomListCache = g_list_append( g_RomListCache, entry );
    }

    gzclose( f );
}

void rombrowser_writeCache( void )
{
    char filename[PATH_MAX];
    gzFile *f;
    SRomEntry *entry = 0;
    GtkTreeIter iter;
    int i=0;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romDisplay));

    snprintf( filename, PATH_MAX, "%srombrowser.cache", get_configpath() );
    f = gzopen( filename, "wb" );
    if( !f )
        return;
    // number of entries
    g_iNumRoms = gtk_tree_model_iter_n_children(model, NULL);
    gzwrite( f, &g_iNumRoms, sizeof( g_iNumRoms ) );

    gtk_tree_model_get_iter_first(model, &iter);

    // entries
    for(i=0; i< g_iNumRoms; i++)
    {
        gtk_tree_model_get (model, &iter, 5, &entry, -1);

        gzwrite( f, entry->cFilename, sizeof( char ) * PATH_MAX );
        gzwrite( f, &entry->info, sizeof( entry->info ) );

        if(!gtk_tree_model_iter_next (model, &iter)) { break; }
    }

    gzclose( f );

    // update cache list
    rombrowser_readCache();
}

/*********************************************************************************************************
* rombrowser functions
*/
// compare function
gint rombrowser_compare( GtkTreeModel *model, GtkTreeIter *ptr1, GtkTreeIter *ptr2, gpointer user_data )
{
    GtkSortType SortType;
    short int returnvalue, column;
    char *buffer1, *buffer2;
    float size1, size2, difference;

    switch( g_iSortColumn )
        {
        case 0: //Flag, so sort by country name.
           column = 1;
            break;
        case 1: //ROM Name
            column = 0;
            break;
        case 2: //Country
            column = 1;
            break;
        case 3: //Size
            column = 2;
            break;
        case 4: //Comments
            column = 3;
            break;
        case 5: //Filename
            column = 4;
            break;
        default:
            return 0;
        };

    gtk_tree_model_get ( model, ptr1, column, &buffer1, -1 );
    gtk_tree_model_get ( model, ptr2, column, &buffer2, -1 );

    if(column==2)
        {
        sscanf ( buffer1, "%f", &size1 );
        sscanf ( buffer2, "%f", &size2 );

        difference = size2 - size1;

        if( difference < 0 )
            { return 1; }
        else if( difference > 0 )
            { return -1; }
        else
            { return  0; }
        }
    else
        { return strcasecmp(buffer1, buffer2); }
}

char *sub_string(const char *string, int start, int end)
{
    int length = end-start;
    if(length <= 0)
    {
        return NULL;
    }
    char *alloc = (char *)malloc(length+1);
    int i;
    for(i = 0; i < length; i++)
    {
        alloc[i] = string[i+start];
    }
    alloc[length] = '\0';
    
    return alloc;
}

// scan dir for roms
static void scan_dir( const char *dirname )
{
    DIR *dir;
    int rom_size, i;
    char filename[PATH_MAX];
    char real_path[PATH_MAX];
    char crc_code[200];
    const char *p;
    struct dirent *de;
    struct stat sb;
    GtkTreeIter *iter1 = (GtkTreeIter *)malloc(sizeof(GtkTreeIter));
    GtkTreeIter *iter2 = (GtkTreeIter *)malloc(sizeof(GtkTreeIter));
    gchar *line[5];
    SRomEntry *entry, *cacheEntry;
    int found;

    dir = opendir( dirname );
    if( !dir )
    {
        printf( "Couldn't open directory '%s': %s\n", dirname, strerror( errno ) );
        return;
    }

    while( (de = readdir( dir )) )
    {
        if( de->d_name[0] == '.' ) // .., . or hidden file
            continue;
        snprintf( filename, PATH_MAX, "%s%s", dirname, de->d_name );
    
        if( config_get_bool( "RomDirsScanRecursive", FALSE ) )
        {
            // use real path (maybe it's a link)
            if( !realpath( filename, real_path ) )
                strcpy( real_path, filename );

            if( stat( real_path, &sb ) == -1 )
            {
                // Display error?
                continue;
            }

            if( S_ISDIR(sb.st_mode) )
            {
                strncat( filename, "/", PATH_MAX );
                scan_dir( filename );
                continue;
            }
        }
    
        // check file extension
        p = strrchr( filename, '.' );
        if( !p )
            continue;
        for( i = 0; g_romFileExtensions[i]; i++ )
        {
            if( !strcasecmp( p, g_romFileExtensions[i] ) )
                break;
        }
        if( !g_romFileExtensions[i] )
            continue;
    
        entry = malloc( sizeof( SRomEntry ) );
        if( !entry )
        {
            fprintf( stderr, "%s, %c: Out of memory!\n", __FILE__, __LINE__ );
            continue;
        }
    
        memset( entry, 0, sizeof( SRomEntry ) );
        strcpy( entry->cFilename, filename );
    
        // search cache
        found = 0;
        for( i = 0; i < g_list_length( g_RomListCache ); i++ )
        {
            cacheEntry = (SRomEntry *)g_list_nth_data( g_RomListCache, i );
            if( !strcmp( entry->cFilename, cacheEntry->cFilename ) )
            {
                memcpy( &entry->info, &cacheEntry->info, sizeof( cacheEntry->info ) );
                found = 1;
                break;
            }
        }
    
        if( !found )
        {
            // load rom header
            rom_size = fill_header( entry->cFilename );
            if( !rom_size )
            {
                free( entry );
                continue;
            }

            // fill entry info struct
            entry->info.iSize = rom_size;
            entry->info.iManufacturer = ROM_HEADER->Manufacturer_ID;
            entry->info.sCartID = ROM_HEADER->Cartridge_ID;
            entry->info.cCountry = ROM_HEADER->Country_code;
            entry->info.iCRC1 = ROM_HEADER->CRC1;
            entry->info.iCRC2 = ROM_HEADER->CRC2;

            sprintf( crc_code, "%08X-%08X-C%02X", sl(entry->info.iCRC1), sl(entry->info.iCRC2), entry->info.cCountry );
            entry->iniEntry = ini_search_by_CRC( crc_code );
        
            if( entry->iniEntry )
            {
                strcpy( entry->info.cComments, entry->iniEntry->comments );
                strcpy( entry->info.cGoodName, entry->iniEntry->goodname );
                strcpy( entry->cName, entry->iniEntry->goodname );
            }
        }
        romentry_fill( entry );

        line[0] = entry->cName;
        line[1] = entry->cCountry;
        line[2] = entry->cSize;
        line[3] = entry->info.cComments;
        if(config_get_bool( "RomBrowserShowFullPaths", FALSE ))
        {
            line[4] = entry->cFilename;
        }
        else
        {
            int fnlen = strlen(entry->cFilename);
            char *newfn= NULL;
            int i;
            for(i=fnlen; i > 0; i--)
            {
                if(entry->cFilename[i] == '/')
                {
                    newfn = sub_string(entry->cFilename, i+1, fnlen);
                    break;
                }
            }
            line[4] = newfn;
        }

        //Add entries to TreeModel
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romFullList));

        gtk_list_store_append ( GTK_LIST_STORE(model), iter1);
        gtk_list_store_set ( GTK_LIST_STORE(model), iter1, 0, line[0], 1, line[1], 2, line[2], 3, line[3], 4, line[4], 5, entry, 6, entry->flag, -1);

        model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romDisplay));
        gtk_list_store_append ( GTK_LIST_STORE(model), iter2);
        gtk_list_store_set ( GTK_LIST_STORE(model), iter2, 0, line[0], 1, line[1], 2, line[2], 3, line[3], 4, line[4], 5, entry, 6, entry->flag, -1);

        g_iNumRoms ++;
    }
    closedir( dir );
}

//Load a fresh TreeView by re-scanning directories.
void rombrowser_refresh( void )
{
    int i;

    GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(g_MainWindow.romFullList) );
    gtk_tree_selection_select_all(selection);
    GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW(g_MainWindow.romFullList) );
    gtk_list_store_clear( GTK_LIST_STORE(model) );

    selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    gtk_tree_selection_select_all(selection);
    model = gtk_tree_view_get_model ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    gtk_list_store_clear( GTK_LIST_STORE(model) );

    g_iNumRoms = 0;
    g_list_free( g_RomList );
    g_RomList = NULL;

    for ( i = 0; i < config_get_number( "NumRomDirs", 0 ); ++i )
        {
        char buf[30];
        const char *dir;
        sprintf( buf, "RomDirectory[%d]", i );
        dir = config_get_string( buf, "" );
        statusbar_message( "status", tr("Scanning directory '%s'..."), dir );
        scan_dir( dir );
        }

    statusbar_message( "status", tr("%d Rom Directories scanned, %d Roms found"), i, g_iNumRoms );

    // save cache
    rombrowser_writeCache();

    // update status bar
    statusbar_message( "num_roms", tr("Total Roms: %d"), g_iNumRoms );

   //Do an initial sort.
   GtkTreeSortable *sortable = GTK_TREE_SORTABLE(model);
   gtk_tree_sortable_set_sort_func ( sortable, g_iSortColumn, rombrowser_compare, (gpointer)NULL, (gpointer)NULL );
   gtk_tree_sortable_set_sort_column_id ( sortable, g_iSortColumn, g_SortType );
   gtk_tree_view_set_model ( GTK_TREE_VIEW(g_MainWindow.romDisplay), GTK_TREE_MODEL(sortable) );
}

//We're going our own filtering, but this is a GtkTreeModelFilter function.
//We do filtering manually since GtkTreeModelFilter can not implement automatic 
//sorting and GtkTreeModelSort can't implement automatic filtering.
gboolean filter_function( GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    const gchar *filter;
    char *buffer1, *buffer2;
    short int column;

    filter = gtk_entry_get_text ( GTK_ENTRY(g_MainWindow.filter) );
    //printf("Filter: %s\n", filter);

    switch( g_iSortColumn )
        {
        case 0: //Flag, so sort by country name.
            column = 1;
            break;
        case 1: //ROM Name
            column = 0;
            break;
        case 2: //Country
            column = 1;
            break;
        case 3: //Size
            column = 2;
            break;
        case 4: //Comments
            column = 3;
            break;
        case 5: //Filename
            column = 4;
            break;
        };

    gtk_tree_model_get (model, iter, column, &buffer1, -1);
    //printf("Value %s\n", buffer1);
    if( buffer1 == NULL || filter == NULL)
        { return TRUE; }
    else
        {
        buffer2 = strcasestr( buffer1, filter );
        if ( buffer2 != NULL )
            { return TRUE; }
       else
            { return FALSE; }
       }
}


/*********************************************************************************************************
* callbacks
*/
// column clicked (title) -> sort
static void callback_columnClicked(GtkTreeViewColumn *treeviewcolumn, gpointer user_data)
{
    if( g_iSortColumn == gtk_tree_view_column_get_sort_column_id(treeviewcolumn) )
        { g_SortType = ( g_SortType == GTK_SORT_ASCENDING ) ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING; }
    else
        {
        g_iSortColumn = gtk_tree_view_column_get_sort_column_id(treeviewcolumn); 
        g_SortType = GTK_SORT_ASCENDING;
        }

    apply_filter();
}

// row double clicked -> play rom
static gboolean callback_rowSelected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    SRomEntry *entry;
    GtkTreeIter iter;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get (model, &iter, 5, &entry, -1);

    if(open_rom( entry->cFilename ) == 0)
        { startEmulation(); }

    return FALSE;
}

// right click -> show menu
gboolean callback_buttonPressed( GtkWidget *widget, GdkEventButton *event, gpointer data )
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_MainWindow.romDisplay));

    if( event->type == GDK_BUTTON_PRESS )
        {
        if( event->button == 3 ) // right click
            {
            if( gtk_tree_selection_count_selected_rows (selection) > 0 )
                {
                gtk_widget_set_sensitive( playRomItem, TRUE );
                gtk_widget_set_sensitive( romPropertiesItem, TRUE );
                }
            else
                {
                gtk_widget_set_sensitive( playRomItem, FALSE );
                gtk_widget_set_sensitive( romPropertiesItem, FALSE );
                }
            gtk_menu_popup( GTK_MENU(data), NULL, NULL, NULL, NULL,
            event->button, event->time );

            return TRUE;
            }
        }

    return FALSE;
}

// activate filter widget -> filter and resort.
static void callback_apply_filter( GtkWidget *widget, gpointer data )
{
    apply_filter();
}

void apply_filter( void )
{
    GtkTreeModel *model, *source, *destination;
    GtkTreeIter sourceiter;
    gboolean validiter;

    //Clear the Display Tree View.
    GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    gtk_tree_selection_select_all(selection);
    model = gtk_tree_view_get_model ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    gtk_list_store_clear( GTK_LIST_STORE(model) );

    source = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romFullList) );
    g_iNumRoms = gtk_tree_model_iter_n_children(source, NULL);
    validiter = gtk_tree_model_get_iter_first(source, &sourceiter);
    destination = gtk_tree_view_get_model ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );

    if(validiter)
        {
        GdkPixbuf *flag;
        SRomEntry *entry;
        short int counter;
        gchar *line[5];

        for ( counter = 0; counter < g_iNumRoms; ++counter )
            {
            if ( filter_function( source, &sourceiter, (gpointer)NULL) )
                 {
                 gtk_tree_model_get ( GTK_TREE_MODEL(source), &sourceiter, 0, &line[0], 1, &line[1], 2, &line[2], 3, &line[3], 4, &line[4], 5, &entry, 6, &flag, -1 );

                 GtkTreeIter destinationiter;
                 gtk_list_store_append ( GTK_LIST_STORE(destination), &destinationiter );
                 gtk_list_store_set ( GTK_LIST_STORE(model), &destinationiter, 0, line[0], 1, line[1], 2, line[2], 3, line[3], 4, line[4], 5, entry, 6, flag, -1 );
                 }
            if(!gtk_tree_model_iter_next(source, &sourceiter))
                 { break; }
            }
        }

    destination = gtk_tree_view_get_model ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    g_iNumRoms = gtk_tree_model_iter_n_children(destination, NULL);

    GtkTreeSortable *sortable = GTK_TREE_SORTABLE(destination);
    gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE(sortable), g_iSortColumn,  rombrowser_compare, (gpointer)NULL, (gpointer)NULL );
    gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE(sortable), g_iSortColumn, g_SortType );
}

static gboolean callback_filter_selected( GtkWidget *widget, gpointer data )
{
    if(g_MainWindow.accelUnsafeActive)
        { gtk_window_remove_accel_group(GTK_WINDOW(g_MainWindow.window), g_MainWindow.accelUnsafe); }
    return FALSE;
}


static gboolean callback_filter_unselected( GtkWidget *widget, gpointer data )
{
    if(g_MainWindow.accelUnsafeActive)
        { gtk_window_add_accel_group(GTK_WINDOW(g_MainWindow.window), g_MainWindow.accelUnsafe); }
    return FALSE;
}

static gboolean callback_filter_grab_unselected( GtkWidget *widget, gpointer data )
{
    if(!g_MainWindow.accelUnsafeActive)
        { gtk_window_add_accel_group(GTK_WINDOW(g_MainWindow.window), g_MainWindow.accelUnsafe); }
    gtk_window_set_focus(GTK_WINDOW(g_MainWindow.window), NULL);
    return FALSE;
}

// create GUI filter widgets.
int create_filter( void )
{
    GtkWidget *filter;
    GtkToolItem *toolitem;
    GtkWidget *label;
    GtkWidget *Hbox;

    Hbox = gtk_hbox_new(FALSE, 0);
    toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, TRUE);

    filter = gtk_toolbar_new();
    gtk_toolbar_set_orientation( GTK_TOOLBAR(filter), GTK_ORIENTATION_HORIZONTAL );

    label = gtk_label_new_with_mnemonic ( tr("F_ilter:") );
    g_MainWindow.filter = gtk_entry_new();
    gtk_entry_set_text ( GTK_ENTRY(g_MainWindow.filter), "" );
    gtk_signal_connect ( GTK_OBJECT(g_MainWindow.filter), "changed",
                       GTK_SIGNAL_FUNC(callback_apply_filter), (gpointer)NULL );
    gtk_signal_connect ( GTK_OBJECT(g_MainWindow.filter), "focus-in-event",
                        GTK_SIGNAL_FUNC(callback_filter_selected), (gpointer)NULL );
    gtk_signal_connect ( GTK_OBJECT(g_MainWindow.filter), "focus-out-event",
                      GTK_SIGNAL_FUNC(callback_filter_unselected), (gpointer)NULL );
    gtk_signal_connect ( GTK_OBJECT(g_MainWindow.filter), "grab-notify",
                     GTK_SIGNAL_FUNC(callback_filter_grab_unselected), (gpointer)NULL );

    gtk_label_set_mnemonic_widget ( GTK_LABEL(label), g_MainWindow.filter );

    gtk_box_pack_start ( GTK_BOX(Hbox), label, FALSE, FALSE, 5);
    gtk_box_pack_start ( GTK_BOX(Hbox), g_MainWindow.filter, TRUE, TRUE, 5);

    gtk_container_add(GTK_CONTAINER(toolitem), Hbox);
    gtk_toolbar_insert ( GTK_TOOLBAR(filter), toolitem, 0);

    gtk_box_pack_start ( GTK_BOX(g_MainWindow.toplevelVBox), filter, FALSE, FALSE, 0 );
}

// play rom menu item
static void callback_playRom( GtkWidget *widget, gpointer data )
{
    GList *list = NULL, *llist = NULL;
    SRomEntry *entry;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romDisplay));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_MainWindow.romDisplay));

    list = gtk_tree_selection_get_selected_rows (selection, &model);

    if( !list ) // should never happen since the item is only active when a row is selected
        { return; }

    llist = g_list_first (list);

    gtk_tree_model_get_iter (model, &iter,(GtkTreePath *) llist->data);
    gtk_tree_model_get(model, &iter, 5, &entry, -1);

    g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (list);

    if(open_rom( entry->cFilename ) == 0)
        { startEmulation(); }
}

// rom properties
static void callback_romProperties( GtkWidget *widget, gpointer data )
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_MainWindow.romDisplay));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_MainWindow.romDisplay));

    if (gtk_tree_selection_count_selected_rows (selection) > 0)
        {
        SRomEntry *entry;

        GList *list = NULL, *llist = NULL;
        list = gtk_tree_selection_get_selected_rows (selection, &model);
        llist = g_list_first (list);

        GtkTreeIter iter;
        gtk_tree_model_get_iter (model, &iter,(GtkTreePath *) llist->data);
        gtk_tree_model_get (model, &iter, 5, &entry, -1);

        g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (list);

        show_romPropDialog( entry );
        }
}

// refresh rom browser
static void callback_refreshRomBrowser( GtkWidget *widget, gpointer data )
{
    rombrowser_refresh();
}



static void setup_view (GtkWidget *view)
{
    gchar *titles[] = 
        {
        (gchar *)tr("Good Name"),
        (gchar *)tr("Country"),
        (gchar *)tr("Size"),
        (gchar *)tr("Comments"),
        (gchar *)tr("File Name")
        };

    GtkCellRenderer     *renderer;
    GtkTreeModel        *model;
    GtkTreeViewColumn   *column;

    //Create an empty ListStore as we never use tree childs and format for ROM data.
    GtkListStore *store = gtk_list_store_new (7, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_PIXBUF, -1);

    model = GTK_TREE_MODEL (store);

    renderer = gtk_cell_renderer_pixbuf_new ();
    column = gtk_tree_view_column_new_with_attributes ("Flag", renderer, "pixbuf", 6, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 0);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 0);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (titles[0], renderer, "text", 0, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 1);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 1);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (titles[1], renderer, "text", 1, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 2);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 2);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (titles[2], renderer, "text", 2, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 3);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 3);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (titles[3], renderer, "text", 3, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 4);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 4);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (titles[4], renderer, "text", 4, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 5);
    g_signal_connect (G_OBJECT (column), "clicked", G_CALLBACK (callback_columnClicked), model);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (view), column, 5);

    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model );
    g_object_unref (model);
}

/*********************************************************************************************************
* gui functions
*/
//Create rom browser.
int create_romBrowser( void )
{
    GtkWidget *rightClickMenu;
    GtkWidget *separatorItem;

    g_iSortColumn = 5;
    g_SortType = GTK_SORT_ASCENDING;

    GError* err = NULL;
    //Load flags.
    australia = gdk_pixbuf_new_from_file( get_iconpath("australia.png"), &err);
    europe = gdk_pixbuf_new_from_file( get_iconpath("europe.png"), &err );
    france = gdk_pixbuf_new_from_file( get_iconpath("france.png"), &err );
    germany = gdk_pixbuf_new_from_file( get_iconpath("germany.png"), &err );
    italy = gdk_pixbuf_new_from_file( get_iconpath("italy.png"), &err );
    japan = gdk_pixbuf_new_from_file( get_iconpath("japan.png"), &err );
    spain = gdk_pixbuf_new_from_file( get_iconpath("spain.png"), &err );
    usa = gdk_pixbuf_new_from_file( get_iconpath("usa.png"), &err );
    japanusa = gdk_pixbuf_new_from_file( get_iconpath("japanusa.png"), &err );
    n64cart = gdk_pixbuf_new_from_file( get_iconpath("n64cart.png"), &err );

    //Setup right-click menu.
    rightClickMenu = gtk_menu_new();
    playRomItem = gtk_menu_item_new_with_label( tr("Play Rom") );
    romPropertiesItem = gtk_menu_item_new_with_label( tr("Rom Properties") );
    separatorItem = gtk_menu_item_new();
    refreshRomBrowserItem = gtk_menu_item_new_with_label( tr("Refresh") );

    gtk_menu_append ( GTK_MENU(rightClickMenu), playRomItem );
    gtk_menu_append ( GTK_MENU(rightClickMenu), romPropertiesItem );
    gtk_menu_append ( GTK_MENU(rightClickMenu), separatorItem );
    gtk_menu_append ( GTK_MENU(rightClickMenu), refreshRomBrowserItem );

    gtk_widget_show ( rightClickMenu );
    gtk_widget_show ( playRomItem );
    gtk_widget_show ( romPropertiesItem );
    gtk_widget_show ( separatorItem );
    gtk_widget_show ( refreshRomBrowserItem );

    gtk_signal_connect ( GTK_OBJECT(playRomItem), "activate", GTK_SIGNAL_FUNC(callback_playRom), (gpointer)NULL );
    gtk_signal_connect ( GTK_OBJECT(romPropertiesItem), "activate", GTK_SIGNAL_FUNC(callback_romProperties), (gpointer)NULL );
    gtk_signal_connect ( GTK_OBJECT(refreshRomBrowserItem), "activate", GTK_SIGNAL_FUNC(callback_refreshRomBrowser), (gpointer)NULL );

    g_MainWindow.romFullList = gtk_tree_view_new();
    g_MainWindow.romDisplay = gtk_tree_view_new();

    //This is needed due to the way we clear the TreeViews.
    GtkTreeSelection *selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(g_MainWindow.romFullList) );
    gtk_tree_selection_set_mode ( selection, GTK_SELECTION_MULTIPLE );
    selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(g_MainWindow.romDisplay) );
    gtk_tree_selection_set_mode ( selection, GTK_SELECTION_MULTIPLE );

    //Initialize TreeViews - i.e. setup column info.
    setup_view(g_MainWindow.romFullList);
    setup_view(g_MainWindow.romDisplay);

    //Create a scrolled window to contain the rom list, make scrollbar visibility automatic.
    g_MainWindow.romScrolledWindow = gtk_scrolled_window_new ( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(g_MainWindow.romScrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

    //Add the Display TreeView into our scrolled window.
    gtk_container_add (GTK_CONTAINER (g_MainWindow.romScrolledWindow), g_MainWindow.romDisplay );
    gtk_container_add (GTK_CONTAINER (g_MainWindow.toplevelVBox), g_MainWindow.romScrolledWindow );

    //Instruct the widget to become visible.
    gtk_widget_show(g_MainWindow.romScrolledWindow);

    gtk_tree_view_set_headers_clickable ( GTK_TREE_VIEW(g_MainWindow.romDisplay), TRUE );
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW(g_MainWindow.romDisplay), TRUE );
    gtk_tree_view_set_reorderable   (GTK_TREE_VIEW(g_MainWindow.romDisplay), TRUE );
 
    gtk_signal_connect( GTK_OBJECT(g_MainWindow.romDisplay), "row-activated", GTK_SIGNAL_FUNC(callback_rowSelected), (gpointer)NULL );
    gtk_signal_connect( GTK_OBJECT(g_MainWindow.romDisplay), "button-press-event", GTK_SIGNAL_FUNC(callback_buttonPressed), (gpointer)rightClickMenu );

    return 0;
}

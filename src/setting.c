/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "setting.h"
#include "application_window.h"
#include "cddb_dialog.h"
#include "load_files_dialog.h"
#include "playlist_dialog.h"
#include "preferences_dialog.h"
#include "search_dialog.h"
#include "bar.h"
#include "easytag.h"
#include "charset.h"
#include "scan_dialog.h"
#include "log.h"
#include "misc.h"
#include "browser.h"
#include "et_core.h"

#include "win32/win32dep.h"


/***************
 * Declaration *
 ***************/

/*
 * Nota :
 *  - no trailing slashes on directory name to avoid problem with
 *    NetBSD's mkdir(2).
 */

// File for configuration
static const gchar CONFIG_FILE[] = "easytagrc";
// File of masks for tag scanner
static const gchar SCAN_TAG_MASKS_FILE[] = "scan_tag.mask";
// File of masks for rename file scanner
static const gchar RENAME_FILE_MASKS_FILE[] = "rename_file.mask";
// File for history of BrowserEntry combobox
static const gchar PATH_ENTRY_HISTORY_FILE[] = "browser_path.history";
// File for history of run program combobox for directories
static const gchar RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE[] = "run_program_with_directory.history";
// File for history of run program combobox for files
static const gchar RUN_PROGRAM_WITH_FILE_HISTORY_FILE[] = "run_program_with_file.history";
// File for history of run player combobox
static const gchar AUDIO_FILE_PLAYER_HISTORY_FILE[] = "audio_file_player.history";
// File for history of search string combobox
static const gchar SEARCH_FILE_HISTORY_FILE[] = "search_file.history";
// File for history of FileToLoad combobox
static const gchar FILE_TO_LOAD_HISTORY_FILE[] = "file_to_load.history";
// File for history of CddbSearchStringEntry combobox
static const gchar CDDB_SEARCH_STRING_HISTORY_FILE[] = "cddb_search_string.history";
// File for history of CddbSearchStringInResultEntry combobox
static const gchar CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE[] = "cddb_search_string_in_result.history";



/**************
 * Prototypes *
 **************/

static void Save_Config_To_File (void);
static gboolean Create_Easytag_Directory (void);



/********************
 * Config Variables *
 ********************/
static const tConfigVariable Config_Variables[] =
{

    {"audio_file_player",                       CV_TYPE_STRING,&AUDIO_FILE_PLAYER                        }
};




/*************
 * Functions *
 *************/

static void
check_default_path (void)
{
    GVariant *default_path;
    const gchar *path;

    default_path = g_settings_get_value (MainSettings, "default-path");
    path = g_variant_get_bytestring (default_path);

    if (!*path)
    {
        path = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
        g_settings_set_value (MainSettings, "default-path",
                              g_variant_new_bytestring (path ? path
                                                             : g_get_home_dir ()));
    }

    g_variant_unref (default_path);
}

/*
 * Define and Load default values into config variables
 */
void Init_Config_Variables (void)
{
    MainSettings = g_settings_new ("org.gnome.EasyTAG");

    /*
     * Common
     */
    check_default_path ();

    /*
     * Misc
     */
#ifdef G_OS_WIN32
    AUDIO_FILE_PLAYER                       = ET_Win32_Get_Audio_File_Player();
#else /* !G_OS_WIN32 */
    AUDIO_FILE_PLAYER                       = g_strdup("xdg-open");
#endif /* !G_OS_WIN32 */
}



/*
 * Function called when pressing the "Save" button of the preferences window.
 * Save into the config variables the settings of each tab of the Preferences window...
 * If settings needs to be "shown/applied" to the corresponding window, we do it
 */
static void
Apply_Changes_Of_Preferences_Window (void)
{
    EtApplicationWindow *window;
    GtkWidget *dialog;

    window = ET_APPLICATION_WINDOW (MainWindow);
    dialog = et_application_window_get_preferences_dialog (window);

    if (dialog)
    {
        /* Misc */
        if (AUDIO_FILE_PLAYER) g_free(AUDIO_FILE_PLAYER);
        AUDIO_FILE_PLAYER                       = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(FilePlayerCombo)))));

        /* Tag Settings */
#ifdef ENABLE_ID3LIB
        g_settings_set_boolean (MainSettings, "id3v2-version-4", TRUE);
#endif

        /* Parameters and variables of Scanner Window are in "scan.c" file */
        /* Parameters and variables of Cddb Window are in "cddb.c" file */
    }

    /*
     * Changes to apply to :
     */
}

/*
 * Save into the config variables the settings of each window
 *  - Position/size of the window
 *  - Specific options in the window
 */
static void
Apply_Changes_Of_UI (void)
{
    /*
     * Changes in user interface
     */

    /* Configuration of the preference window (see preferences_dialog.c).
     * Function also called when destroying the window. */
    et_preferences_dialog_apply_changes (ET_PREFERENCES_DIALOG (et_application_window_get_preferences_dialog (ET_APPLICATION_WINDOW (MainWindow))));

    /* Configuration of the scanner window (see scan_dialog.c).
     * Function also called when destroying the window. */
    et_scan_dialog_apply_changes (ET_SCAN_DIALOG (et_application_window_get_scan_dialog (ET_APPLICATION_WINDOW (MainWindow))));

    /* Configuration of the cddb window (see cddb_dialog.c).
     * Function also called when destroying the window. */
    et_cddb_dialog_apply_changes (ET_CDDB_DIALOG (et_application_window_get_cddb_dialog (ET_APPLICATION_WINDOW (MainWindow))));

    /* Configuration of the search_file window (see search_dialog.c).
     * Function also called when destroying the window. */
    et_search_dialog_apply_changes (ET_SEARCH_DIALOG (et_application_window_get_search_dialog (ET_APPLICATION_WINDOW (MainWindow))));
}

void Save_Changes_Of_UI (void)
{
    Apply_Changes_Of_UI();
    Save_Config_To_File();
}

void Save_Changes_Of_Preferences_Window (void)
{
    Apply_Changes_Of_Preferences_Window();
    Save_Config_To_File();

    Statusbar_Message(_("Configuration saved"),TRUE);
}



/*
 * Write the config file
 */
static void
Save_Config_To_File (void)
{
    gchar *file_path = NULL;
    FILE *file;

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  CONFIG_FILE, NULL);

    if (!Create_Easytag_Directory () || (file = fopen (file_path, "w+")) == 0)
    {
        Log_Print (LOG_ERROR,
                   _("Error: Cannot write configuration file: %s (%s)"),
                   file_path, g_strerror(errno));
    }
    else
    {
        gint ConfigVarListLen = sizeof(Config_Variables)/sizeof(tConfigVariable);
        gint i;
        gchar *data = NULL;

        for (i=0; i<ConfigVarListLen; i++)
        {
            switch (Config_Variables[i].type)
            {
                case CV_TYPE_INT:
                {
                    data = g_strdup_printf("%s=%i\n",Config_Variables[i].name,
                                                     *(int *)Config_Variables[i].pointer);
                    if (fwrite (data, strlen (data), 1, file) != 1)
                    {
                        Log_Print (LOG_ERROR,
                                   _("Error while writing configuration file: %s"),
                                   file_path);
                        fclose (file);
                        g_free (file_path);
                        g_free (data);
                        return;
                    }
                    g_free(data);
                    break;
                }
                case CV_TYPE_BOOL:
                {
                    data = g_strdup_printf("%s=%i\n",Config_Variables[i].name,
                                                     ( *(int *)Config_Variables[i].pointer ? 1 : 0 ));
                    if (fwrite (data, strlen (data), 1, file) != 1)
                    {
                        Log_Print (LOG_ERROR,
                                   _("Error while writing configuration file: %s"),
                                   file_path);
                        fclose (file);
                        g_free (file_path);
                        g_free (data);
                        return;
                    }
                    g_free(data);
                    break;
                }
                case CV_TYPE_STRING:
                {
                    /* Doesn't write datum if empty */
                    if ( (*(char **)Config_Variables[i].pointer)==NULL ) break;

                    data = g_strdup_printf("%s=%s\n",Config_Variables[i].name,
                                                     *(char **)Config_Variables[i].pointer);
                    if (fwrite (data, strlen (data), 1, file) != 1)
                    {
                        Log_Print (LOG_ERROR,
                                   _("Error while writing configuration file: %s"),
                                   file_path);
                        fclose (file);
                        g_free (file_path);
                        g_free (data);
                        return;
                    }
                    g_free(data);
                    break;
                }
                default:
                {
                    Log_Print(LOG_ERROR,"ERROR: Can't save: type of config variable not supported "
                              "for '%s'!",Config_Variables[i].name);
                    break;
                }
            }
        }
        fclose(file);
    }
    g_free(file_path);
}


/*
 * Parse lines read (line as <var_description>=<value>) and load the values
 * into the corresponding config variables.
 */
static void
Set_Config (gchar *line)
{
    const gchar *var_descriptor;
    const gchar *var_value;
    gint ConfigVarListLen;
    gint i;

    g_return_if_fail (line != NULL);

    if (*line=='\n' || *line=='#') return;

    /* Cut string */
    var_descriptor = strtok (line, "=");

    if (!var_descriptor)
    {
        g_message ("Invalid configuration file line ‘%s’", line);
        return;
    }

    var_value = strtok (NULL, "=");

    if (!var_value)
    {
        g_message ("No value for configuration key ‘%s’", var_descriptor);
        return;
    }

    ConfigVarListLen = sizeof(Config_Variables)/sizeof(tConfigVariable);
    for (i=0; i<ConfigVarListLen; i++)
    {
        if (Config_Variables[i].name!=NULL && var_descriptor
        && !strcmp(Config_Variables[i].name,var_descriptor))
        {
            switch (Config_Variables[i].type)
            {
                case CV_TYPE_INT:
                {
                    *(int *)Config_Variables[i].pointer = strtol(var_value, NULL, 10);
                    break;
                }

                case CV_TYPE_BOOL:
                {
                    if (strtol(var_value, NULL, 10))
                        *(int *)Config_Variables[i].pointer = 1;
                    else
                        *(int *)Config_Variables[i].pointer = 0;
                    break;
                }

                case CV_TYPE_STRING:
                {

                    if (*(char **)Config_Variables[i].pointer != NULL)
                    {
                        g_free (*(char **)Config_Variables[i].pointer);
                    }

                    *(char **)Config_Variables[i].pointer = g_strdup (var_value);
                    break;
                }

                default:
                {
                    Log_Print(LOG_ERROR,"ERROR: Can't read: type of config variable not supported "
                              "for '%s'!",Config_Variables[i].name);
                    break;
                }
            }
        }
    }
}


/*
 * Read config from config file
 */
void Read_Config (void)
{
    gchar *file_path = NULL;
    FILE *file;
    gchar buffer[MAX_STRING_LEN];

    /* The file to read */
    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  CONFIG_FILE, NULL);

    if ((file = fopen (file_path,"r")) == 0)
    {
        Log_Print (LOG_ERROR, _("Cannot open configuration file '%s' (%s)"),
                   file_path, g_strerror (errno));
        Log_Print (LOG_OK, _("Loading default configuration"));
    }else
    {
        while (fgets(buffer,sizeof(buffer),file))
        {
            if (buffer[strlen(buffer)-1]=='\n')
                buffer[strlen(buffer)-1]='\0';
            Set_Config(buffer);
        }
        fclose(file);

        // Force this configuration! - Disabled as it is boring for russian people
        //USE_ISO_8859_1_CHARACTER_SET_TRANSLATION = 1;
        //USE_CHARACTER_SET_TRANSLATION            = 0;
    }
    g_free(file_path);
}


/*
 * check_or_create_file:
 * @filename: (type filename): the filename to create
 *
 * Check that the provided @filename exists, and if not, create it.
 */
static void check_or_create_file (const gchar *filename)
{
    FILE  *file;
    gchar *file_path = NULL;

    g_return_if_fail (filename != NULL);

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if ((file = fopen (file_path, "a+")) != NULL )
    {
        fclose (file);
    }
    else
    {
        Log_Print (LOG_ERROR, _("Cannot create or open file '%s' (%s)"),
                   CONFIG_FILE, g_strerror (errno));
    }

    g_free (file_path);
}

/*
 * Create the main directory with empty history files
 */
gboolean Setting_Create_Files (void)
{
    /* The file to write */
    if (!Create_Easytag_Directory ())
    {
        return FALSE;
    }

    check_or_create_file (SCAN_TAG_MASKS_FILE);
    check_or_create_file (RENAME_FILE_MASKS_FILE);
    check_or_create_file (PATH_ENTRY_HISTORY_FILE);
    check_or_create_file (RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE);
    check_or_create_file (RUN_PROGRAM_WITH_FILE_HISTORY_FILE);
    check_or_create_file (AUDIO_FILE_PLAYER_HISTORY_FILE);
    check_or_create_file (SEARCH_FILE_HISTORY_FILE);
    check_or_create_file (FILE_TO_LOAD_HISTORY_FILE);
    check_or_create_file (CDDB_SEARCH_STRING_HISTORY_FILE);
    check_or_create_file (CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE);

    return TRUE;
}



/*
 * Save the contents of a list store to a file
 */
static void
Save_List_Store_To_File (const gchar *filename, GtkListStore *liststore, gint colnum)
{
    gchar *file_path = NULL;
    FILE *file;
    gchar *text;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter))
        return;

    /* The file to write */
    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if (!Create_Easytag_Directory () || (file = fopen (file_path, "w+")) == NULL)
    {
        Log_Print (LOG_ERROR, _("Error: Cannot write list to file: %s (%s)"),
                   file_path, g_strerror (errno));
    }else
    {
        do
        {
            GString *data;

            gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, colnum,
                                &text, -1);
            data = g_string_new (text);
            g_free (text);
            g_string_append_c (data, '\n');

            if (*data->str != '\n')
            {
                if (fwrite (data->str, data->len, 1, file) != 1)
                {
                    Log_Print (LOG_ERROR, _("Error while writing list file: %s"),
                               file_path);
                    fclose (file);
                    g_string_free (data, TRUE);
                    g_free (file_path);
                    return;
                }
            }
            g_string_free (data, TRUE);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(liststore), &iter));
        fclose(file);
    }
    g_free(file_path);
}

/*
 * Populate a list store with data from a file passed in as first parameter
 */
static gboolean
Populate_List_Store_From_File (const gchar *filename, GtkListStore *liststore, gint text_column)
{

    gchar *file_path = NULL;
    FILE *file;
    gchar buffer[MAX_STRING_LEN];
    gboolean entries_set = FALSE;

    /* The file to write */
    g_return_val_if_fail (filename != NULL, FALSE);

    file_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                  filename, NULL);

    if ((file = fopen (file_path, "r")) == NULL)
    {
        Log_Print (LOG_ERROR, _("Cannot open file '%s' (%s)"), file_path,
                   g_strerror (errno));
    }else
    {
        gchar *data = NULL;

        while(fgets(buffer,sizeof(buffer),file))
        {
            if (buffer[strlen(buffer)-1]=='\n')
                buffer[strlen(buffer)-1]='\0';

            /*if (g_utf8_validate(buffer, -1, NULL))
                data = g_strdup(buffer);
            else
                data = convert_to_utf8(buffer);*/
            data = Try_To_Validate_Utf8_String(buffer);

            if (data && g_utf8_strlen(data, -1) > 0)
            {
                gtk_list_store_insert_with_values (liststore, NULL, G_MAXINT,
                                                   text_column, data, -1);
                entries_set = TRUE;
            }
            g_free(data);
        }
        fclose(file);
    }
    g_free(file_path);
    return entries_set;
}


/*
 * Functions for writing and reading list of 'Fill Tag' masks
 */
void
Load_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum,
                          const gchar * const *fallback)
{
    gsize i = 0;
    GtkTreeIter iter;

    if (!Populate_List_Store_From_File(SCAN_TAG_MASKS_FILE, liststore, colnum))
    {
        // Fall back to defaults
        Log_Print(LOG_OK,_("Loading default 'Fill Tag' masks…"));

        while(fallback[i])
        {
            gtk_list_store_insert_with_values (liststore, &iter, G_MAXINT,
                                               colnum, fallback[i], -1);
            i++;
        }
    }
}

void Save_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(SCAN_TAG_MASKS_FILE, liststore, colnum);
}


/*
 * Functions for writing and reading list of 'Rename File' masks
 */
void
Load_Rename_File_Masks_List (GtkListStore *liststore, gint colnum,
                             const gchar * const *fallback)
{
    gsize i = 0;
    GtkTreeIter iter;

    if (!Populate_List_Store_From_File(RENAME_FILE_MASKS_FILE, liststore, colnum))
    {
        // Fall back to defaults
        Log_Print(LOG_OK,_("Loading default 'Rename File' masks…"));

        while(fallback[i])
        {
            gtk_list_store_insert_with_values (liststore, &iter, G_MAXINT,
                                               colnum, fallback[i], -1);
            i++;
        }
    }
}

void Save_Rename_File_Masks_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RENAME_FILE_MASKS_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of 'BrowserEntry' combobox
 */
void Load_Path_Entry_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(PATH_ENTRY_HISTORY_FILE, liststore, colnum);
}
void Save_Path_Entry_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(PATH_ENTRY_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run program (tree browser)
 */
void Load_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE, liststore, colnum);
}
void Save_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run program (file browser)
 */
void Load_Run_Program_With_File_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(RUN_PROGRAM_WITH_FILE_HISTORY_FILE, liststore, colnum);
}
void Save_Run_Program_With_File_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(RUN_PROGRAM_WITH_FILE_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to run file audio player
 */
void Load_Audio_File_Player_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(AUDIO_FILE_PLAYER_HISTORY_FILE, liststore, colnum);
}
void Save_Audio_File_Player_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(AUDIO_FILE_PLAYER_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox to search a string into file (tag or filename)
 */
void Load_Search_File_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(SEARCH_FILE_HISTORY_FILE, liststore, colnum);
}
void Save_Search_File_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(SEARCH_FILE_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of path of file to load to rename files
 */
void Load_File_To_Load_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(FILE_TO_LOAD_HISTORY_FILE, liststore, colnum);
}
void Save_File_To_Load_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(FILE_TO_LOAD_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of cddb search string
 */
void Load_Cddb_Search_String_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(CDDB_SEARCH_STRING_HISTORY_FILE, liststore, colnum);
}
void Save_Cddb_Search_String_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(CDDB_SEARCH_STRING_HISTORY_FILE, liststore, colnum);
}

/*
 * Functions for writing and reading list of combobox of cddb search string in result list
 */
void Load_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum)
{
    Populate_List_Store_From_File(CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE, liststore, colnum);
}
void Save_Cddb_Search_String_In_Result_List (GtkListStore *liststore, gint colnum)
{
    Save_List_Store_To_File(CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE, liststore, colnum);
}

/*
 * migrate_config_to_xdg_dir:
 * @old_path: (type filename): the path to migrate from
 * @new_path: (type filename): the path to migrate to
 *
 * Migrate the EasyTAG configuration files contained in the old path to the new
 * one.
 */
static void
migrate_config_file_dir (const gchar *old_path, const gchar *new_path)
{
    gsize i;
    static const gchar *filenames[] = { CONFIG_FILE,
                                        SCAN_TAG_MASKS_FILE,
                                        RENAME_FILE_MASKS_FILE,
                                        PATH_ENTRY_HISTORY_FILE,
                                        RUN_PROGRAM_WITH_DIRECTORY_HISTORY_FILE,
                                        RUN_PROGRAM_WITH_FILE_HISTORY_FILE,
                                        AUDIO_FILE_PLAYER_HISTORY_FILE,
                                        SEARCH_FILE_HISTORY_FILE,
                                        FILE_TO_LOAD_HISTORY_FILE,
                                        CDDB_SEARCH_STRING_HISTORY_FILE,
                                        CDDB_SEARCH_STRING_IN_RESULT_HISTORY_FILE,
                                        NULL
    };

    Log_Print (LOG_OK, _("Migrating configuration from directory '%s' to '%s'"),
               old_path, new_path);

    for (i = 0; filenames[i]; i++)
    {
        gchar *old_filename, *new_filename;
        GFile *old_file, *new_file;

        old_filename = g_build_filename (old_path, filenames[i], NULL);

        if (!g_file_test (old_filename, G_FILE_TEST_EXISTS))
        {
            g_free (old_filename);
            continue;
        }

        new_filename = g_build_filename (new_path, filenames[i], NULL);
        old_file = g_file_new_for_path (old_filename);
        new_file = g_file_new_for_path (new_filename);

        if (!g_file_move (old_file, new_file, G_FILE_COPY_NONE, NULL, NULL,
                          NULL, NULL))
        {
            Log_Print (LOG_ERROR,
                       _("Failed to migrate configuration file '%s'"),
                       filenames[i]);
        }

        g_free (old_filename);
        g_free (new_filename);
        g_object_unref (old_file);
        g_object_unref (new_file);
    }
}

/**
 * Create the directory used by EasyTAG to store user configuration files.
 *
 * Returns: %TRUE if the directory was created, or already exists. %FALSE if
 * the directory could not be created.
 */
static gboolean
Create_Easytag_Directory (void)
{
    gchar *easytag_path = NULL;
    gint result;

    /* Directory to create (if it does not exist) with absolute path. */
    easytag_path = g_build_filename (g_get_user_config_dir (), PACKAGE_TARNAME,
                                     NULL);

    if (g_file_test (easytag_path, G_FILE_TEST_IS_DIR))
    {
        g_free (easytag_path);
        return TRUE;
    }

    result = g_mkdir_with_parents (easytag_path, S_IRWXU);

    if (result == -1)
    {
        Log_Print (LOG_ERROR,_("Error: Cannot create directory '%s' (%s)"),
                  easytag_path, g_strerror (errno));
        g_free (easytag_path);
        return FALSE;
    }
    else
    {
        gchar *old_path = g_build_filename (g_get_home_dir (),
                                            "." PACKAGE_TARNAME, NULL);

        if (g_file_test (old_path, G_FILE_TEST_IS_DIR))
        {
            migrate_config_file_dir (old_path, easytag_path);
        }

        g_free (old_path);
        g_free (easytag_path);

        return TRUE;
    }
}

/*
 * et_settings_enum_get:
 * @value: the property value to be set (active item on combo box)
 * @variant: the variant to set the @value from
 * @user_data: the #GType of the #GSettings enum
 *
 * Wrapper function to convert an enum-type GSettings key into an integer
 * value.
 *
 * Returns: %TRUE if the mapping was successful, %FALSE otherwise
 */
gboolean
et_settings_enum_get (GValue *value, GVariant *variant, gpointer user_data)
{
    GType enum_type;
    GEnumClass *enum_class;
    GEnumValue *enum_value;
    const gchar *nick;

    g_return_val_if_fail (user_data != NULL, FALSE);

    enum_type = (GType)GPOINTER_TO_SIZE (user_data);
    enum_class = g_type_class_ref (enum_type);
    nick = g_variant_get_string (variant, NULL);
    enum_value = g_enum_get_value_by_nick (enum_class, nick);
    g_type_class_unref (enum_class);

    if (!enum_value)
    {
        g_warning ("Unable to lookup %s enum nick '%s' from GType",
                   g_type_name (enum_type),
                   nick);
        return FALSE;
    }

    g_value_set_int (value, enum_value->value);
    return TRUE;
}

/*
 * et_settings_enum_set:
 * @value: the property value to set the @variant from
 * @expected_type: the expected type of the returned variant
 * @user_data: the #GType of the #GSettings enum
 *
 * Wrapper function to convert an integer value into a string suitable for
 * storing into an enum-type GSettings key.
 *
 * Returns: a new GVariant containing the mapped value, or %NULL upon failure
 */
GVariant *
et_settings_enum_set (const GValue *value, const GVariantType *expected_type,
                      gpointer user_data)
{
    GType enum_type;
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    g_return_val_if_fail (user_data != NULL, NULL);

    enum_type = (GType)GPOINTER_TO_SIZE (user_data);
    enum_class = g_type_class_ref (enum_type);
    enum_value = g_enum_get_value (enum_class, g_value_get_int (value));
    g_type_class_unref (enum_class);

    if (!enum_value)
    {
        g_warning ("Unable to lookup %s enum value '%d' from GType",
                   g_type_name (enum_type), g_value_get_int (value));
        return NULL;
    }

    return g_variant_new (g_variant_type_peek_string (expected_type),
                          enum_value->value_nick);
}

/*
 * et_settings_enum_radio_get:
 * @value: the property value to be set
 * @variant: the variant to set the @value from
 * @user_data: the widget on which the setting should be applied
 *
 * Wrapper function to convert an enum-type GSettings key state to the active
 * radio button.
 *
 * Returns: %TRUE
 */
gboolean
et_settings_enum_radio_get (GValue *value, GVariant *variant,
                            gpointer user_data)
{
    const gchar *name;
    const gchar *setting;

    name = gtk_widget_get_name (GTK_WIDGET (user_data));
    setting = g_variant_get_string (variant, NULL);

    /* Only set the radio button which matches the setting to active. */
    if (g_strcmp0 (name, setting) == 0)
    {
        g_value_set_boolean (value, TRUE);
    }

    return TRUE;
}

/*
 * et_settings_enum_radio_set:
 * @value: the property value to set the @variant from
 * @expected_type: the expected type of the returned variant
 * @user_data: the widget which the setting should be taken from
 *
 * Wrapper function to convert the active radiobutton to the value of an
 * enum-type GSettings key.
 *
 * Returns: a new GVariant containing the mapped value, or %NULL upon failure
 */
GVariant *
et_settings_enum_radio_set (const GValue *value,
                            const GVariantType *expected_type,
                            gpointer user_data)
{
    GVariant *variant = NULL;
    const gchar *name;

    /* Ignore buttons that are not active. */
    if (!g_value_get_boolean (value))
    {
        return variant;
    }

    name = gtk_widget_get_name (GTK_WIDGET (user_data));
    variant = g_variant_new_string (name);

    return variant;
}

/*
 * et_settings_flags_toggle_get:
 * @value: the property value to be set (active item on combo box)
 * @variant: the variant to set the @value from
 * @user_data: the #GType of the #GSettings flags
 *
 * Wrapper function to convert a flags-type GSettings key state into the active
 * toggle button.
 *
 * Returns: %TRUE if the mapping was successful, %FALSE otherwise
 */
gboolean
et_settings_flags_toggle_get (GValue *value, GVariant *variant, gpointer user_data)
{
    const gchar *name;
    GType flags_type;
    GFlagsClass *flags_class;
    GVariantIter iter;
    GFlagsValue *flags_value;
    const gchar *nick;
    guint flags = 0;

    g_return_val_if_fail (user_data != NULL, FALSE);

    name = gtk_widget_get_name (GTK_WIDGET (user_data));
    flags_type = (GType)GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (user_data),
                                                                       "flags-type"));
    flags_class = g_type_class_ref (flags_type);

    g_variant_iter_init (&iter, variant);

    while (g_variant_iter_next (&iter, "&s", &nick))
    {
        flags_value = g_flags_get_value_by_nick (flags_class, nick);

        if (flags_value)
        {
            flags |= flags_value->value;
        }
        else
        {
            g_warning ("Unable to lookup %s flags nick '%s' from GType",
                       g_type_name (flags_type), nick);
            g_type_class_unref (flags_class);
            return FALSE;
        }
    }

    flags_value = g_flags_get_value_by_nick (flags_class, name);
    g_type_class_unref (flags_class);

    /* TRUE if settings flag is set for this widget, which will make the widget
     * active. */
    g_value_set_boolean (value, flags & flags_value->value);
    return TRUE;
}

/*
 * et_settings_flags_toggle_set:
 * @value: the property value to set the @variant from
 * @expected_type: the expected type of the returned variant
 * @user_data: the widget associated with the changed setting
 *
 * Wrapper function to convert a boolean value into a string suitable for
 * storing into a flags-type GSettings key.
 *
 * Returns: a new GVariant containing the mapped value, or %NULL upon failure
 */
GVariant *
et_settings_flags_toggle_set (const GValue *value,
                              const GVariantType *expected_type,
                              gpointer user_data)
{
    const gchar *name;
    GType flags_type;
    GFlagsClass *flags_class;
    GFlagsValue *flags_value;
    guint mask;
    GVariantBuilder builder;
    guint flags = g_settings_get_flags (MainSettings, "process-fields");

    g_return_val_if_fail (user_data != NULL, NULL);

    name = gtk_widget_get_name (GTK_WIDGET (user_data));
    flags_type = (GType)GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (user_data),
                                                                       "flags-type"));
    flags_class = g_type_class_ref (flags_type);
    flags_value = g_flags_get_value_by_nick (flags_class, name);
    mask = flags_class->mask;

    if (!flags_value)
    {
        g_warning ("Unable to lookup %s flags value '%d' from GType",
                   g_type_name (flags_type), g_value_get_boolean (value));
        g_type_class_unref (flags_class);
        return NULL;
    }

    if (g_value_get_boolean (value))
    {
        flags |= flags_value->value;
    }
    else
    {
        flags &= (flags_value->value ^ mask);
    }

    g_variant_builder_init (&builder, expected_type);

    while (flags)
    {
        flags_value = g_flags_get_first_value (flags_class, flags);

        if (flags_value == NULL)
        {
            g_variant_builder_clear (&builder);
            g_type_class_unref (flags_class);
            return NULL;
        }

        g_variant_builder_add (&builder, "s", flags_value->value_nick);
        flags &= ~flags_value->value;
    }

    g_type_class_unref (flags_class);

    return g_variant_builder_end (&builder);
}

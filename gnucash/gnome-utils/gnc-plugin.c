/*
 * gnc-plugin.c --
 *
 * Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
 * Copyright (C) 2003,2005 David Hampton <hampton@employees.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

/** @addtogroup MenuPlugins
    @{ */
/** @addtogroup MenuPluginBase Common object and functions
    @{ */
/** @file gnc-plugin.c
    @brief Functions for adding plugins to a Gnucash window.
    @author Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
    @author Copyright (C) 2003,2005 David Hampton <hampton@employees.org>
*/

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gnc-plugin.h"
#include "gnc-engine.h"
#include "gnc-filepath-utils.h"
#include "gnc-gnome-utils.h"
#include "gnc-gobject-utils.h"
#include "gnc-gtk-utils.h"

/** The debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;
/** A pointer to the parent class of a plugin. */
static gpointer parent_class = NULL;

static void gnc_plugin_class_init (GncPluginClass *klass);
static void gnc_plugin_init       (GncPlugin *plugin_page,
                                   void *data);
static void gnc_plugin_finalize   (GObject *object);


/** The instance private data for a menu-only plugin.  This data
 *  structure is unused. */
typedef struct GncPluginPrivate
{
    gpointer dummy;
} GncPluginPrivate;

GNC_DEFINE_TYPE_WITH_CODE(GncPlugin, gnc_plugin, G_TYPE_OBJECT,
		        G_ADD_PRIVATE(GncPlugin))

#define GNC_PLUGIN_GET_PRIVATE(o)  \
   ((GncPluginPrivate*)gnc_plugin_get_instance_private((GncPlugin*)o))

/** Initialize the class for the new gnucash plugin object.  This will
 *  set up any function pointers that override functions in the parent
 *  class, and also installs the proprieties that are unique to this
 *  class.
 *
 *  @param klass The new class structure created by the object system.
 */
static void
gnc_plugin_class_init (GncPluginClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    parent_class = g_type_class_peek_parent (klass);
    gobject_class->finalize = gnc_plugin_finalize;
}


/** Initialize a new instance of a gnucash menu-only plugin.  This
 *  function adds the object to the tracking system.
 *
 *  @param plugin_page The new object instance created by the object
 *  system.
 *
 *  @param klass A pointer to the class data structure for this
 *  object. */
static void
gnc_plugin_init (GncPlugin *plugin_page, void *data)
{
    GncPluginClass *klass = (GncPluginClass*)data;

    gnc_gobject_tracking_remember(G_OBJECT(plugin_page), \
                                  G_OBJECT_CLASS(klass));
}


/** Finalize the gnucash plugin object.  This function is called from
 *  the G_Object level to complete the destruction of the object.  It
 *  should release any memory not previously released by the destroy
 *  function (i.e. the private data structure), then chain up to the
 *  parent's destroy function.
 *
 *  @param object The object being destroyed. */
static void
gnc_plugin_finalize (GObject *object)
{
    g_return_if_fail (GNC_IS_PLUGIN (object));

    gnc_gobject_tracking_forget(object);
    G_OBJECT_CLASS (parent_class)->finalize (object);
}


/** Add the specified plugin from the specified window.  This function
 *  will add the page's user interface from the window and call the
 *  plugin to perform any plugin specific actions.
 *
 *  See gnc-plugin.h for documentation on the function arguments. */
void
gnc_plugin_add_to_window (GncPlugin *plugin,
                          GncMainWindow *window,
                          GQuark type)
{
    GncPluginClass *klass;
    GSimpleActionGroup *simple_action_group;

    g_return_if_fail (GNC_IS_PLUGIN (plugin));
    klass = GNC_PLUGIN_GET_CLASS (plugin);
    ENTER (": plugin %s(%p), window %p", gnc_plugin_get_name(plugin),
           plugin, window);

    /*
     * Update window with additional UI items
     */
    if (klass->actions_name)
    {
        DEBUG ("%s: %d actions to merge with gui from %s",
               klass->actions_name, klass->n_actions, klass->ui_filename);
        gnc_main_window_merge_actions (window, klass->actions_name,
                                       klass->actions, klass->n_actions,
                                       klass->display_items, klass->n_display_items,
                                       klass->ui_updates,
                                       klass->ui_filename, plugin);
    }

    /*
     * Do plugin specific actions.
     */
    if (GNC_PLUGIN_GET_CLASS (plugin)->add_to_window)
    {
        DEBUG ("Calling child class function %p", GNC_PLUGIN_GET_CLASS (plugin)->add_to_window);
        GNC_PLUGIN_GET_CLASS (plugin)->add_to_window (plugin, window, type);
    }
    LEAVE ("");
}


/*  Remove the specified plugin from the specified window.  This
 *  function will call the plugin to perform any plugin specific
 *  actions and remove the page's user interface from the window.
 *
 *  See gnc-plugin.h for documentation on the function arguments. */
void
gnc_plugin_remove_from_window (GncPlugin *plugin,
                               GncMainWindow *window,
                               GQuark type)
{
    GncPluginClass *klass;

    g_return_if_fail (GNC_IS_PLUGIN (plugin));
    klass = GNC_PLUGIN_GET_CLASS (plugin);
    ENTER (": plugin %s(%p), window %p", gnc_plugin_get_name(plugin),
           plugin, window);

    /*
     * Do plugin specific actions.
     */
    if (GNC_PLUGIN_GET_CLASS (plugin)->remove_from_window)
    {
        DEBUG ("Calling child class function %p",
               GNC_PLUGIN_GET_CLASS (plugin)->remove_from_window);
        GNC_PLUGIN_GET_CLASS (plugin)->remove_from_window (plugin, window, type);
    }

    /*
     * Update window to remove UI items
     */
    if (klass->actions_name && !window->just_plugin_prefs)
    {
        DEBUG ("%s: %d actions to unmerge",
               klass->actions_name, (klass->n_actions));
        gnc_main_window_unmerge_actions (window, klass->actions_name);
    }
    LEAVE ("");
}


/** Retrieve the textual name of a plugin.
 */
const gchar *
gnc_plugin_get_name (GncPlugin *plugin)
{
    g_return_val_if_fail (GNC_IS_PLUGIN (plugin), NULL);
    return (GNC_PLUGIN_GET_CLASS(plugin)->plugin_name);
}


/************************************************************
 *                    Utility Functions                     *
 ************************************************************/


/** Add "short" labels to existing actions.  The "short" label is the
 *  string used on toolbar buttons when the action is visible.
 *
 *  See gnc-plugin.h for documentation on the function arguments. */
void
gnc_plugin_init_short_names (GtkWidget *toolbar,
                             GncToolBarShortNames *toolbar_labels)
{
    g_return_if_fail (toolbar != NULL);
    g_return_if_fail (toolbar_labels != NULL);

    for (gint i = 0; (toolbar_labels[i].action_name); i++)
    {
        GtkWidget *tool_item = gnc_find_toolbar_item (toolbar, toolbar_labels[i].action_name);

        if (!tool_item)
            continue;

        gtk_tool_button_set_label (GTK_TOOL_BUTTON(tool_item), _(toolbar_labels[i].short_label));
        gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON(tool_item), TRUE);
    }
}


/*  Update the sensitivity of an action */
void
gnc_plugin_set_actions_enabled (GSimpleActionGroup *simple_action_group,
                                const gchar **action_names, gboolean enable)
{
    g_return_if_fail (simple_action_group != NULL);

    for (gint i = 0; action_names[i]; i++)
    {
        GAction *action = g_action_map_lookup_action (G_ACTION_MAP(simple_action_group),
                                                      action_names[i]);
        if (action)
             g_simple_action_set_enabled (G_SIMPLE_ACTION(action), enable);
        else
            PERR("No such action with name '%s' in action group %p)",
                  action_names[i], simple_action_group);
    }
}

/** Load a new set of actions into an existing UI.
 *
 *  See gnc-plugin.h for documentation on the function arguments. */
gint
gnc_plugin_add_actions (GtkUIManager *ui_merge,
                        GtkActionGroup *action_group,
                        const gchar *filename)
{
    GError *error = NULL;
    gchar *pathname;
    gint merge_id;

    g_return_val_if_fail (ui_merge, 0);
    g_return_val_if_fail (action_group, 0);
    g_return_val_if_fail (filename, 0);

    ENTER("ui_merge %p, action_group %p, filename %s",
          ui_merge, action_group, filename);
    gtk_ui_manager_insert_action_group (ui_merge, action_group, 0);

    pathname = gnc_filepath_locate_ui_file (filename);
    if (pathname == NULL)
    {
        LEAVE("fail");
        return 0;
    }

    merge_id = gtk_ui_manager_add_ui_from_file (ui_merge, pathname, &error);
    DEBUG("merge_id is %d", merge_id);

    g_assert(merge_id || error);
    if (merge_id)
    {
        gtk_ui_manager_ensure_update (ui_merge);
    }
    else
    {
        g_critical("Failed to load ui file.\n  Filename %s\n  Error %s",
                   filename, error->message);
        g_error_free(error);
    }

    g_free(pathname);
    LEAVE(" ");
    return merge_id;
}

#if 0
static void
gnc_plugin_base_init (gpointer klass)
{
    static gboolean initialized = FALSE;

    if (!initialized)
    {
        initialized = TRUE;

        signals[MERGE_ACTIONS] = g_signal_new ("merge-actions",
                                               G_OBJECT_CLASS_TYPE (klass),
                                               G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET (GncPluginClass, merge_actions),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__POINTER,
                                               G_TYPE_NONE,
                                               1,
                                               GTK_TYPE_MENU_MERGE);
        signals[UNMERGE_ACTIONS] = g_signal_new ("unmerge-actions",
                                   G_OBJECT_CLASS_TYPE (klass),
                                   G_SIGNAL_RUN_FIRST,
                                   G_STRUCT_OFFSET (GncPluginClass, unmerge_actions),
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__POINTER,
                                   G_TYPE_NONE,
                                   1,
                                   GTK_TYPE_MENU_MERGE);
    }
}
#endif

/** @} */
/** @} */

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <config.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include "misc.h"
#include "trayicon.h"
#include "applet-common.h"

#ifdef HAVE_APP_INDICATOR
    #include <libappindicator/app-indicator.h>
#endif


struct SeafileTrayIconPriv {
#ifdef HAVE_APP_INDICATOR
    AppIndicator        *icon;
#else
    GtkStatusIcon         *icon;
#endif
    GtkWidget           *popup_menu;
    GtkAction           *start_action;
    GtkAction           *quit_action;
    GtkAction           *restart_action;
};

#define GET_PRIV(o)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAFILE_TYPE_TRAY_ICON, SeafileTrayIconPriv))

G_DEFINE_TYPE (SeafileTrayIcon, seafile_tray_icon, G_TYPE_OBJECT);

static void
tray_icon_popup_menu_cb (GtkStatusIcon     *tray_icon,
                         guint              button,
                         guint              activate_time,
                         SeafileTrayIcon   *icon);

#ifdef HAVE_APP_INDICATOR
static void _tray_set_icon (AppIndicator *icon, const char *icon_name)
{
    app_indicator_set_icon_full(icon, icon_name, NULL);
}

static void _update_icon_tootip(AppIndicator *icon, const char *tooltip)
{
    app_indicator_set_label (icon, tooltip, NULL);
}

static AppIndicator* _create_icon ()
{
    AppIndicator *app_icon = app_indicator_new("seafile",
                                               ICON_STATUS_UP,
                                               APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status (app_icon, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_icon_theme_path (app_icon, PKGDATADIR );
    return app_icon;
}

static void _setup_menu (SeafileTrayIcon *icon, GtkWidget *menu)
{
    app_indicator_set_menu(APP_INDICATOR(icon->priv->icon), GTK_MENU(menu));
}

#else  /* IFDEF HAVE_APP_INDICATOR */
static void _tray_set_icon (GtkStatusIcon *icon, const char *icon_name)
{
    gtk_status_icon_set_from_icon_name (icon, icon_name);
}

static GtkStatusIcon* _create_icon ()
{
    return  gtk_status_icon_new ();
}

static void _setup_menu (SeafileTrayIcon *icon, GtkWidget *menu)
{
    g_signal_connect (icon->priv->icon, "popup-menu",
                      G_CALLBACK (tray_icon_popup_menu_cb),
                      icon);
}
#endif

void
seafile_trayicon_set_icon (SeafileTrayIcon *icon, const char *name)
{
    SeafileTrayIconPriv *priv = GET_PRIV (icon);
    
    _tray_set_icon (priv->icon, name);
}

void open_web_browser();

extern int on_quit ();

static void
tray_icon_quit_cb (GtkAction *action, SeafileTrayIcon *icon)
{
    on_quit ();
}

static void
restart_menu_cb (GtkAction       *action,
                 SeafileTrayIcon *icon)
{
    restart_all();
}

static void
open_browser_cb (GtkAction *action, SeafileTrayIcon *icon)
{
    open_web_browser(SEAF_HTTP_ADDR);
}

static void
tray_icon_popup_menu_cb (GtkStatusIcon     *tray_icon,
                         guint              button,
                         guint              activate_time,
                         SeafileTrayIcon   *icon)
{
    SeafileTrayIconPriv *priv = GET_PRIV (icon);


    gtk_menu_popup (GTK_MENU (priv->popup_menu),
                    NULL, NULL,
                    gtk_status_icon_position_menu,
                    priv->icon,
                    button,
                    activate_time);
}

static void
tray_icon_create_menu (SeafileTrayIcon *icon)
{
    SeafileTrayIconPriv *priv = GET_PRIV (icon);
    GtkBuilder *builder;
    gchar *filename;

    filename = ccnet_file_lookup ("seafile-trayicon.ui", "gtk");
    builder = gtk_builder_get_all_widgets (filename,
                       "popup_menu", &priv->popup_menu,
                       "quit_action", &priv->quit_action,
                       "start_action", &priv->start_action,
                       "restart_network_action", &priv->restart_action,
                       NULL);
    g_free (filename);
    g_object_ref (priv->popup_menu);
    g_object_unref (builder);
    
    g_signal_connect (priv->quit_action, "activate",
                      G_CALLBACK (tray_icon_quit_cb), icon);
    g_signal_connect (priv->restart_action, "activate",
                      G_CALLBACK (restart_menu_cb), icon);
    g_signal_connect (priv->start_action, "activate",
                      G_CALLBACK (open_browser_cb), icon);
}

static void
seafile_trayicon_finalize (GObject *object)
{
    SeafileTrayIconPriv *priv = GET_PRIV (object);

    g_object_unref (priv->icon);
}

static void
seafile_tray_icon_class_init (SeafileTrayIconClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = seafile_trayicon_finalize;

    g_type_class_add_private (object_class, sizeof (SeafileTrayIconPriv));
}


static void
seafile_tray_icon_init (SeafileTrayIcon *icon)
{
    SeafileTrayIconPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (icon,
        SEAFILE_TYPE_TRAY_ICON, SeafileTrayIconPriv);

    icon->priv = priv;
    priv->icon = _create_icon();

    tray_icon_create_menu (icon);
    _setup_menu (icon, priv->popup_menu);
#ifndef HAVE_APP_INDICATOR
    g_signal_connect (priv->icon, "activate",
                      G_CALLBACK (open_browser_cb),
                      icon);
#endif

    notify_init("Seafile");
}

SeafileTrayIcon *
seafile_trayicon_new (GtkWindow *window)
{
    SeafileTrayIcon *icon;

    icon = g_object_new (SEAFILE_TYPE_TRAY_ICON, NULL);

    seafile_trayicon_set_icon (icon, ICON_STATUS_UP);

    return icon;
}

static GtkStatusIcon *
seafile_trayicon_get_gtk_icon (SeafileTrayIcon *icon)
{
#ifndef HAVE_APP_INDICATOR
    return (GtkStatusIcon *)(icon->priv->icon);
#else
    return NULL;
#endif
}

void seafile_trayicon_notify (SeafileTrayIcon *icon, char *title, char *buf)
{
    NotifyNotification *n;
    GtkStatusIcon *gicon = seafile_trayicon_get_gtk_icon(icon);

    /* notify_notification_new_with_status_icon doesn't exists in
       libnotify version > 0.7 */
#ifdef LIBNOTIFY_GREAT_THAN_7
    const char *name = gtk_status_icon_get_icon_name (gicon);
    n = notify_notification_new (title, buf, name);
#else
    n = notify_notification_new_with_status_icon (title, buf,
                                                  "Seafile", gicon);
#endif

    notify_notification_set_timeout (n, 2000);
    notify_notification_show (n, NULL);
    g_object_unref (n);
}


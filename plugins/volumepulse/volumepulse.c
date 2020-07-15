/*
Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
/**
 * Copyright (c) 2008-2014 LxDE Developers, see the file AUTHORS for details.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "plugin.h"
#include "volumepulse.h"
#include "pulse.h"

#define BLUEALSA_DEV (-99)

#define BT_SERV_AUDIO_SOURCE    "0000110A"
#define BT_SERV_AUDIO_SINK      "0000110B"
#define BT_SERV_HSP             "00001108"
#define BT_SERV_HFP             "0000111E"

/* Helpers */
static char *get_string (const char *fmt, ...);
static int get_value (const char *fmt, ...);
static int vsystem (const char *fmt, ...);
static gboolean find_in_section (char *file, char *sec, char *seek);
static int hdmi_monitors (VolumeALSAPlugin *vol);

/* Bluetooth */
static void bt_cb_object_added (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
static void bt_cb_object_removed (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data);
static void bt_cb_name_owned (GDBusConnection *connection, const gchar *name, const gchar *owner, gpointer user_data);
static void bt_cb_name_unowned (GDBusConnection *connection, const gchar *name, gpointer user_data);
static void bt_connect_device (VolumeALSAPlugin *vol);
static void bt_cb_connected (GObject *source, GAsyncResult *res, gpointer user_data);
static void bt_cb_trusted (GObject *source, GAsyncResult *res, gpointer user_data);
static void bt_reconnect_devices (VolumeALSAPlugin *vol);
static void bt_cb_reconnected (GObject *source, GAsyncResult *res, gpointer user_data);
static void bt_disconnect_device (VolumeALSAPlugin *vol, char *device);
static void bt_cb_disconnected (GObject *source, GAsyncResult *res, gpointer user_data);
static gboolean bt_has_service (VolumeALSAPlugin *vol, const gchar *path, const gchar *service);
static gboolean bt_is_connected (VolumeALSAPlugin *vol, const gchar *path);

/* Handlers and graphics */
static void volumealsa_update_display (VolumeALSAPlugin *vol);
static void volumealsa_theme_change (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_open_config_dialog (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_open_input_config_dialog (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_open_profile_dialog (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_show_connect_dialog (VolumeALSAPlugin *vol, gboolean failed, const gchar *param);
static void volumealsa_close_connect_dialog (GtkButton *button, gpointer user_data);
static gint volumealsa_delete_connect_dialog (GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean volumealsa_button_press_event (GtkWidget *widget, GdkEventButton *event, LXPanel *panel);

/* Menu popup */
GtkWidget *volumealsa_add_item_to_menu (VolumeALSAPlugin *vol, GtkWidget *menu, const char *label, const char *name, gboolean enabled, gboolean input, GCallback cb);
static void volumealsa_menu_show_default_sink (GtkWidget *widget, gpointer data);
static void volumealsa_menu_show_default_source (GtkWidget *widget, gpointer data);
const char *volumealsa_device_display_name (VolumeALSAPlugin *vol, const char *name);
static void volumealsa_build_device_menu (VolumeALSAPlugin *vol);
void volumealsa_set_external_output (GtkWidget *widget, VolumeALSAPlugin *vol);
void volumealsa_set_external_input (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_set_bluetooth_output (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_set_bluetooth_input (GtkWidget *widget, VolumeALSAPlugin *vol);

/* Volume popup */
static void volumealsa_build_popup_window (GtkWidget *p);
static void volumealsa_popup_scale_changed (GtkRange *range, VolumeALSAPlugin *vol);
static void volumealsa_popup_scale_scrolled (GtkScale *scale, GdkEventScroll *evt, VolumeALSAPlugin *vol);
static void volumealsa_popup_mute_toggled (GtkWidget *widget, VolumeALSAPlugin *vol);
static void volumealsa_popup_set_position (GtkWidget *menu, gint *px, gint *py, gboolean *push_in, gpointer data);
static gboolean volumealsa_mouse_out (GtkWidget *widget, GdkEventButton *event, VolumeALSAPlugin *vol);

/* Profiles dialog */
static void show_profiles (VolumeALSAPlugin *vol);
static void close_profiles (VolumeALSAPlugin *vol);
static void profiles_ok_handler (GtkButton *button, gpointer *user_data);
static gboolean profiles_wd_close_handler (GtkWidget *wid, GdkEvent *event, gpointer user_data);
static void profile_changed (GtkComboBox *combo, gpointer *userdata);
static void relocate_last_item (GtkWidget *box);
void volumealsa_add_combo_to_profiles (VolumeALSAPlugin *vol, GtkListStore *ls, GtkWidget *dest, int sel, const char *name, const char *label);

static char *bluez_to_pa_sink_name (char *bluez_name, char *profile);
static char *bluez_to_pa_source_name (char *bluez_name);
static char *bluez_to_pa_card_name (char *bluez_name);
static char *bluez_from_pa_name (char *pa_name);
static int pa_bt_sink_source_compare (char *sink, char *source);
static int pa_bluez_device_same (const char *padev, const char *btdev);

/* Plugin */
static GtkWidget *volumealsa_configure (LXPanel *panel, GtkWidget *plugin);
static void volumealsa_panel_configuration_changed (LXPanel *panel, GtkWidget *plugin);
static gboolean volumealsa_control_msg (GtkWidget *plugin, const char *cmd);
static GtkWidget *volumealsa_constructor (LXPanel *panel, config_setting_t *settings);
static void volumealsa_destructor (gpointer user_data);

/*----------------------------------------------------------------------------*/
/* Generic helper functions                                                   */
/*----------------------------------------------------------------------------*/

static char *get_string (const char *fmt, ...)
{
    char *cmdline, *line = NULL, *res = NULL;
    size_t len = 0;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);

    FILE *fp = popen (cmdline, "r");
    if (fp)
    {
        if (getline (&line, &len, fp) > 0)
        {
            res = line;
            while (*res++) if (g_ascii_isspace (*res)) *res = 0;
            res = g_strdup (line);
        }
        pclose (fp);
        g_free (line);
    }
    g_free (cmdline);
    return res ? res : g_strdup ("");
}

static int get_value (const char *fmt, ...)
{
    char *res;
    int n, m;

    res = get_string (fmt);
    n = sscanf (res, "%d", &m);
    g_free (res);

    if (n != 1) return -1;
    else return m;
}

static int vsystem (const char *fmt, ...)
{
    char *cmdline;
    int res;

    va_list arg;
    va_start (arg, fmt);
    g_vasprintf (&cmdline, fmt, arg);
    va_end (arg);
    res = system (cmdline);
    g_free (cmdline);
    return res;
}

static gboolean find_in_section (char *file, char *sec, char *seek)
{
    char *cmd = g_strdup_printf ("sed -n '/%s/,/}/p' %s 2>/dev/null | grep -q %s", sec, file, seek);
    int res = system (cmd);
    g_free (cmd);
    if (res == 0) return TRUE;
    else return FALSE;
}

/* Multiple HDMI support */

static int hdmi_monitors (VolumeALSAPlugin *vol)
{
    int i, m;

    /* check xrandr for connected monitors */
    m = get_value ("xrandr -q | grep -c connected");
    if (m < 0) m = 1; /* couldn't read, so assume 1... */
    if (m > 2) m = 2;

    /* get the names */
    if (m == 2)
    {
        for (i = 0; i < m; i++)
        {
            vol->mon_names[i] = get_string ("xrandr --listmonitors | grep %d: | cut -d ' ' -f 6", i);
        }

        /* check both devices are HDMI */
        if ((vol->mon_names[0] && strncmp (vol->mon_names[0], "HDMI", 4) != 0)
            || (vol->mon_names[1] && strncmp (vol->mon_names[1], "HDMI", 4) != 0))
                m = 1;
    }

    return m;
}


/*----------------------------------------------------------------------------*/
/* Bluetooth D-Bus interface                                                  */
/*----------------------------------------------------------------------------*/

static void bt_cb_object_added (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    const char *obj = g_dbus_object_get_object_path (object);
    pulse_get_default_sink_source (vol);
    char *device = bluez_from_pa_name (vol->pa_default_sink);
    char *idevice = bluez_from_pa_name (vol->pa_default_source);
    if (g_strcmp0 (obj, device) || g_strcmp0 (obj, idevice))
    {
        DEBUG ("Selected Bluetooth audio device has connected");
        volumealsa_update_display (vol);
    }
    if (device) g_free (device);
    if (idevice) g_free (idevice);
}

static void bt_cb_object_removed (GDBusObjectManager *manager, GDBusObject *object, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    const char *obj = g_dbus_object_get_object_path (object);
    pulse_get_default_sink_source (vol);
    char *device = bluez_from_pa_name (vol->pa_default_sink);
    char *idevice = bluez_from_pa_name (vol->pa_default_source);
    if (g_strcmp0 (obj, device) || g_strcmp0 (obj, idevice))
    {
        DEBUG ("Selected Bluetooth audio device has disconnected");
        volumealsa_update_display (vol);
    }
    if (device) g_free (device);
    if (idevice) g_free (idevice);
}

static void bt_cb_name_owned (GDBusConnection *connection, const gchar *name, const gchar *owner, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    DEBUG ("Name %s owned on DBus", name);

    /* BlueZ exists - get an object manager for it */
    GError *error = NULL;
    vol->objmanager = g_dbus_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SYSTEM, 0, "org.bluez", "/", NULL, NULL, NULL, NULL, &error);
    if (error)
    {
        DEBUG ("Error getting object manager - %s", error->message);
        vol->objmanager = NULL;
        g_error_free (error);
    }
    else
    {
        /* register callbacks for devices being added or removed */
        g_signal_connect (vol->objmanager, "object-added", G_CALLBACK (bt_cb_object_added), vol);
        g_signal_connect (vol->objmanager, "object-removed", G_CALLBACK (bt_cb_object_removed), vol);

        /* Check whether a Bluetooth audio device is the current default output or input - connect to one or both if so */
        pulse_get_default_sink_source (vol);
        char *device = bluez_from_pa_name (vol->pa_default_sink);
        char *idevice = bluez_from_pa_name (vol->pa_default_source);
        if (device || idevice)
        {
            /* Reconnect the current Bluetooth audio device */
            if (vol->bt_conname) g_free (vol->bt_conname);
            if (vol->bt_reconname) g_free (vol->bt_reconname);
            if (device) vol->bt_conname = device;
            else if (idevice) vol->bt_conname = idevice;

            if (device && idevice && g_strcmp0 (device, idevice)) vol->bt_reconname = idevice;
            else vol->bt_reconname = NULL;

            DEBUG ("Reconnecting devices");
            bt_reconnect_devices (vol);
        }
        if (device) g_free (device);
        if (idevice) g_free (idevice);
    }
}

static void bt_cb_name_unowned (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    DEBUG ("Name %s unowned on DBus", name);

    if (vol->objmanager) g_object_unref (vol->objmanager);
    if (vol->bt_conname) g_free (vol->bt_conname);
    if (vol->bt_reconname) g_free (vol->bt_reconname);
    vol->objmanager = NULL;
    vol->bt_conname = NULL;
    vol->bt_reconname = NULL;
}

static void bt_connect_device (VolumeALSAPlugin *vol)
{
    GDBusInterface *interface = g_dbus_object_manager_get_interface (vol->objmanager, vol->bt_conname, "org.bluez.Device1");
    DEBUG ("Connecting device %s...", vol->bt_conname);
    if (interface)
    {
        // trust and connect
        g_dbus_proxy_call (G_DBUS_PROXY (interface), "org.freedesktop.DBus.Properties.Set", 
            g_variant_new ("(ssv)", g_dbus_proxy_get_interface_name (G_DBUS_PROXY (interface)), "Trusted", g_variant_new_boolean (TRUE)),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, bt_cb_trusted, vol);
        g_dbus_proxy_call (G_DBUS_PROXY (interface), "Connect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, bt_cb_connected, vol);
        g_object_unref (interface);
    }
    else
    {
        DEBUG ("Couldn't get device interface from object manager");
        if (vol->conn_dialog) volumealsa_show_connect_dialog (vol, TRUE, _("Could not get BlueZ interface"));
        if (vol->bt_conname) g_free (vol->bt_conname);
        vol->bt_conname = NULL;
    }
}

static void bt_cb_connected (GObject *source, GAsyncResult *res, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    GError *error = NULL;
    char *paname, *pacard;

    GVariant *var = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
    if (var) g_variant_unref (var);

    if (error)
    {
        DEBUG ("Connect error %s", error->message);

        // update dialog to show a warning
        if (vol->conn_dialog) volumealsa_show_connect_dialog (vol, TRUE, error->message);
        g_error_free (error);
    }
    else
    {
        DEBUG ("Connected OK");

        // some devices take a very long time to be valid PulseAudio cards after connection
        pacard = bluez_to_pa_card_name (vol->bt_conname);
        do pulse_get_profile (vol, pacard);
        while (vol->pa_profile == NULL);

        // set connected device as PulseAudio default
        if (vol->bt_input)
        {
            paname = bluez_to_pa_source_name (vol->bt_conname);
            pulse_set_profile (vol, pacard, "headset_head_unit");
            pulse_change_source (vol, paname);
        }
        else
        {
            paname = bluez_to_pa_sink_name (vol->bt_conname, vol->pa_profile);
            pulse_change_sink (vol, paname);
        }
        g_free (paname);
        g_free (pacard);

        // close the connection dialog
        volumealsa_close_connect_dialog (NULL, vol);
    }

    // delete the connection information
    g_free (vol->bt_conname);
    vol->bt_conname = NULL;

    // reinit alsa to configure mixer
    volumealsa_update_display (vol);
}

static void bt_cb_trusted (GObject *source, GAsyncResult *res, gpointer user_data)
{
    GError *error = NULL;
    GVariant *var = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
    if (var) g_variant_unref (var);

    if (error)
    {
        DEBUG ("Trusting error %s", error->message);
        g_error_free (error);
    }
    else DEBUG ("Trusted OK");
}

static void bt_reconnect_devices (VolumeALSAPlugin *vol)
{
    while (vol->bt_conname)
    {
        GDBusInterface *interface = g_dbus_object_manager_get_interface (vol->objmanager, vol->bt_conname, "org.bluez.Device1");
        DEBUG ("Reconnecting %s...", vol->bt_conname);
        if (interface)
        {
            // trust and connect
            g_dbus_proxy_call (G_DBUS_PROXY (interface), "org.freedesktop.DBus.Properties.Set",
                g_variant_new ("(ssv)", g_dbus_proxy_get_interface_name (G_DBUS_PROXY (interface)), "Trusted", g_variant_new_boolean (TRUE)),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, bt_cb_trusted, vol);
            g_dbus_proxy_call (G_DBUS_PROXY (interface), "Connect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, bt_cb_reconnected, vol);
            g_object_unref (interface);
            break;
        }

        DEBUG ("Couldn't get device interface from object manager - device not available to reconnect");
        g_free (vol->bt_conname);

        if (vol->bt_reconname)
        {
            vol->bt_conname = vol->bt_reconname;
            vol->bt_reconname = NULL;
        }
        else vol->bt_conname = NULL;
    }
}

static void bt_cb_reconnected (GObject *source, GAsyncResult *res, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    GError *error = NULL;

    GVariant *var = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
    if (var) g_variant_unref (var);

    if (error) DEBUG ("Connect error %s", error->message);
    else DEBUG ("Connected OK");

    // delete the connection information
    g_free (vol->bt_conname);
    vol->bt_conname = NULL;

    // connect to second device if there is one...
    if (vol->bt_reconname)
    {
        vol->bt_conname = vol->bt_reconname;
        vol->bt_reconname = NULL;
        DEBUG ("Connecting to second device %s...", vol->bt_conname);
        bt_reconnect_devices (vol);
    }
    else
    {
        // reinit alsa to configure mixer
        volumealsa_update_display (vol);
    }
}

static void bt_disconnect_device (VolumeALSAPlugin *vol, char *device)
{
    GDBusInterface *interface = g_dbus_object_manager_get_interface (vol->objmanager, device, "org.bluez.Device1");
    DEBUG ("Disconnecting device %s...", device);
    if (interface)
    {
        // call the disconnect method on BlueZ
        g_dbus_proxy_call (G_DBUS_PROXY (interface), "Disconnect", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, bt_cb_disconnected, vol);
        g_object_unref (interface);
    }
    else
    {
        DEBUG ("Couldn't get device interface from object manager - device probably already disconnected");
        if (vol->bt_conname)
        {
            DEBUG ("Connecting to %s...", vol->bt_conname);
            bt_connect_device (vol);
        }
    }
}

static void bt_cb_disconnected (GObject *source, GAsyncResult *res, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    GError *error = NULL;
    GVariant *var = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
    if (var) g_variant_unref (var);

    if (error)
    {
        DEBUG ("Disconnect error %s", error->message);
        g_error_free (error);
    }
    else DEBUG ("Disconnected OK");

    // call BlueZ over DBus to connect to the device
    if (vol->bt_conname)
    {
        DEBUG ("Connecting to %s...", vol->bt_conname);
        bt_connect_device (vol);
    }
}

static gboolean bt_has_service (VolumeALSAPlugin *vol, const gchar *path, const gchar *service)
{
    GDBusInterface *interface = g_dbus_object_manager_get_interface (vol->objmanager, path, "org.bluez.Device1");
    GVariant *elem, *var = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "UUIDs");
    GVariantIter iter;
    g_variant_iter_init (&iter, var);
    while ((elem = g_variant_iter_next_value (&iter)))
    {
        const char *uuid = g_variant_get_string (elem, NULL);
        if (!strncasecmp (uuid, service, 8)) return TRUE;
        g_variant_unref (elem);
    }
    g_variant_unref (var);
    g_object_unref (interface);
    return FALSE;
}

static gboolean bt_is_connected (VolumeALSAPlugin *vol, const gchar *path)
{
    GDBusInterface *interface = g_dbus_object_manager_get_interface (vol->objmanager, path, "org.bluez.Device1");
    GVariant *var = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Connected");
    gboolean res = g_variant_get_boolean (var);
    g_variant_unref (var);
    g_object_unref (interface);
    return res;
}


/*----------------------------------------------------------------------------*/
/* Plugin handlers and graphics                                               */
/*----------------------------------------------------------------------------*/

/* Do a full redraw of the display. */
static void volumealsa_update_display (VolumeALSAPlugin *vol)
{
    gboolean mute;
    int level;
#ifdef ENABLE_NLS
    // need to rebind here for tooltip update
    textdomain (GETTEXT_PACKAGE);
#endif

#ifdef OPTIONS
    if (vol->options_dlg) update_options (vol);
#endif

    /* read current mute and volume status */
    mute = pulse_get_mute (vol);
    level = pulse_get_volume (vol);
    if (mute) level = 0;

    /* update icon */
    const char *icon = "audio-volume-muted";
    if (!mute)
    {
        if (level >= 66) icon = "audio-volume-high";
        else if (level >= 33) icon = "audio-volume-medium";
        else if (level > 0) icon = "audio-volume-low";
    }
    lxpanel_plugin_set_taskbar_icon (vol->panel, vol->tray_icon, icon);

    /* update popup window controls */
    if (vol->mute_check)
    {
        g_signal_handler_block (vol->mute_check, vol->mute_check_handler);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vol->mute_check), mute);
        g_signal_handler_unblock (vol->mute_check, vol->mute_check_handler);
    }

    if (vol->volume_scale)
    {
        g_signal_handler_block (vol->volume_scale, vol->volume_scale_handler);
        gtk_range_set_value (GTK_RANGE (vol->volume_scale), level);
        g_signal_handler_unblock (vol->volume_scale, vol->volume_scale_handler);
    }

    /* update tooltip */
    char *tooltip = g_strdup_printf ("%s %d", _("Volume control"), level);
    gtk_widget_set_tooltip_text (vol->plugin, tooltip);
    g_free (tooltip);
}

gboolean volumealsa_update_disp_cb (gpointer userdata)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) userdata;
    volumealsa_update_display (vol);
    return FALSE;
}

static void volumealsa_theme_change (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    volumealsa_update_display (vol);
}

static void volumealsa_open_profile_dialog (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    gtk_menu_popdown (GTK_MENU (vol->menu_popup));
    show_profiles (vol);
}

static void volumealsa_show_connect_dialog (VolumeALSAPlugin *vol, gboolean failed, const gchar *param)
{
    char buffer[256];

    if (!failed)
    {
        vol->conn_dialog = gtk_dialog_new_with_buttons (_("Connecting Audio Device"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL);
        gtk_window_set_icon_name (GTK_WINDOW (vol->conn_dialog), "preferences-system-bluetooth");
        gtk_window_set_position (GTK_WINDOW (vol->conn_dialog), GTK_WIN_POS_CENTER);
        gtk_container_set_border_width (GTK_CONTAINER (vol->conn_dialog), 10);
        sprintf (buffer, _("Connecting to Bluetooth audio device '%s'..."), param);
        vol->conn_label = gtk_label_new (buffer);
        gtk_label_set_line_wrap (GTK_LABEL (vol->conn_label), TRUE);
        gtk_label_set_justify (GTK_LABEL (vol->conn_label), GTK_JUSTIFY_LEFT);
        gtk_misc_set_alignment (GTK_MISC (vol->conn_label), 0.0, 0.0);
        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (vol->conn_dialog))), vol->conn_label, TRUE, TRUE, 0);
        g_signal_connect (GTK_OBJECT (vol->conn_dialog), "delete_event", G_CALLBACK (volumealsa_delete_connect_dialog), vol);
        gtk_widget_show_all (vol->conn_dialog);
    }
    else
    {
        sprintf (buffer, _("Failed to connect to device - %s. Try to connect again."), param);
        gtk_label_set_text (GTK_LABEL (vol->conn_label), buffer);
        vol->conn_ok = gtk_dialog_add_button (GTK_DIALOG (vol->conn_dialog), _("_OK"), 1);
        g_signal_connect (vol->conn_ok, "clicked", G_CALLBACK (volumealsa_close_connect_dialog), vol);
        gtk_widget_show (vol->conn_ok);
    }
}

static void volumealsa_close_connect_dialog (GtkButton *button, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    if (vol->conn_dialog)
    {
        gtk_widget_destroy (vol->conn_dialog);
        vol->conn_dialog = NULL;
    }
}

static gint volumealsa_delete_connect_dialog (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;
    if (vol->conn_dialog)
    {
        gtk_widget_destroy (vol->conn_dialog);
        vol->conn_dialog = NULL;
    }
    return TRUE;
}

/* Handler for "button-press-event" signal on main widget. */

static gboolean volumealsa_button_press_event (GtkWidget *widget, GdkEventButton *event, LXPanel *panel)
{
    VolumeALSAPlugin *vol = lxpanel_plugin_get_data (widget);

#ifdef ENABLE_NLS
    textdomain (GETTEXT_PACKAGE);
#endif

    if (event->button == 1)
    {
        /* left-click - show or hide volume popup */
        if (vol->show_popup)
        {
            gtk_widget_hide (vol->popup_window);
            vol->show_popup = FALSE;
        }
        else
        {
            volumealsa_build_popup_window (vol->plugin);
            volumealsa_update_display (vol);

            gint x, y;
            gtk_window_set_position (GTK_WINDOW (vol->popup_window), GTK_WIN_POS_MOUSE);
            // need to draw the window in order to allow the plugin position helper to get its size
            gtk_widget_show_all (vol->popup_window);
            gtk_widget_hide (vol->popup_window);
            lxpanel_plugin_popup_set_position_helper (panel, widget, vol->popup_window, &x, &y);
            gdk_window_move (gtk_widget_get_window (vol->popup_window), x, y);
            gtk_window_present (GTK_WINDOW (vol->popup_window));
            gdk_pointer_grab (gtk_widget_get_window (vol->popup_window), TRUE, GDK_BUTTON_PRESS_MASK, NULL, NULL, GDK_CURRENT_TIME);
            g_signal_connect (G_OBJECT (vol->popup_window), "focus-out-event", G_CALLBACK (volumealsa_mouse_out), vol);
            vol->show_popup = TRUE;
        }
    }
    else if (event->button == 2)
    {
        /* middle-click - toggle mute */
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vol->mute_check), ! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (vol->mute_check)));
    }
    else if (event->button == 3)
    {
        /* right-click - show device list */
        volumealsa_build_device_menu (vol);
        gtk_widget_show_all (vol->menu_popup);
        gtk_menu_popup (GTK_MENU (vol->menu_popup), NULL, NULL, (GtkMenuPositionFunc) volumealsa_popup_set_position, (gpointer) vol,
            event->button, event->time);
    }
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/* Device select menu                                                         */
/*----------------------------------------------------------------------------*/

static void menu_add_separator (GtkWidget *menu)
{
    if (menu == NULL) return;

    // find the end of the menu
    GList *l = g_list_last (gtk_container_get_children (GTK_CONTAINER (menu)));
    if (l == NULL) return;
    if (G_OBJECT_TYPE (l->data) == GTK_TYPE_SEPARATOR_MENU_ITEM) return;
    GtkWidget *mi = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
}

GtkWidget *volumealsa_add_item_to_menu (VolumeALSAPlugin *vol, GtkWidget *menu, const char *label, const char *name, gboolean enabled, gboolean input, GCallback cb)
{
    GtkWidget *mi = gtk_image_menu_item_new_with_label (label);
    gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (mi), TRUE);
    gtk_widget_set_name (mi, name);
    g_signal_connect (mi, "activate", cb, (gpointer) vol);

    if (!enabled)
    {
        gtk_widget_set_sensitive (mi, FALSE);
        if (input)
            gtk_widget_set_tooltip_text (mi, _("Input from this device not available in the current profile"));
        else
            gtk_widget_set_tooltip_text (mi, _("Output to this device not available in the current profile"));
    }

    // insert alphabetically in current section - count the list first
    int count = 0;
    GList *l = g_list_first (gtk_container_get_children (GTK_CONTAINER (menu)));
    while (l)
    {
        count++;
        l = l->next;
    }

    // find the start point of the last section - either a separator or the beginning of the list
    l = g_list_last (gtk_container_get_children (GTK_CONTAINER (menu)));
    while (l)
    {
        if (G_OBJECT_TYPE (l->data) == GTK_TYPE_SEPARATOR_MENU_ITEM) break;
        count--;
        l = l->prev;
    }

    // if l is NULL, init to element after start; if l is non-NULL, init to element after separator
    if (!l) l = gtk_container_get_children (GTK_CONTAINER (menu));
    else l = l->next;

    // loop forward from the first element, comparing against the new label
    while (l)
    {
        if (g_strcmp0 (label, gtk_menu_item_get_label (GTK_MENU_ITEM (l->data))) < 0) break;
        count++;
        l = l->next;
    }

    gtk_menu_shell_insert (GTK_MENU_SHELL (menu), mi, count);
    return mi;
}

static void volumealsa_menu_show_default_sink (GtkWidget *widget, gpointer data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) data;

    if (!g_strcmp0 (gtk_widget_get_name (widget), vol->pa_default_sink) || pa_bluez_device_same (vol->pa_default_sink, gtk_widget_get_name (widget)))
    {
        GtkWidget *image = gtk_image_new ();
        lxpanel_plugin_set_menu_icon (vol->panel, image, "dialog-ok-apply");
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (widget), image);
    }
}

static void volumealsa_menu_show_default_source (GtkWidget *widget, gpointer data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) data;

    if (!g_strcmp0 (gtk_widget_get_name (widget), vol->pa_default_source) || pa_bluez_device_same (vol->pa_default_source, gtk_widget_get_name (widget)))
    {
        GtkWidget *image = gtk_image_new ();
        lxpanel_plugin_set_menu_icon (vol->panel, image, "dialog-ok-apply");
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (widget), image);
    }
}

static void volumealsa_menu_add_bluetooth (VolumeALSAPlugin *vol, gboolean input)
{
    if (vol->objmanager)
    {
        // iterate all the objects the manager knows about
        GList *objects = g_dbus_object_manager_get_objects (vol->objmanager);
        while (objects != NULL)
        {
            GDBusObject *object = (GDBusObject *) objects->data;
            const char *objpath = g_dbus_object_get_object_path (object);
            GList *interfaces = g_dbus_object_get_interfaces (object);
            while (interfaces != NULL)
            {
                // if an object has a Device1 interface, it is a Bluetooth device - add it to the list
                GDBusInterface *interface = G_DBUS_INTERFACE (interfaces->data);
                if (g_strcmp0 (g_dbus_proxy_get_interface_name (G_DBUS_PROXY (interface)), "org.bluez.Device1") == 0)
                {
                    if (bt_has_service (vol, g_dbus_proxy_get_object_path (G_DBUS_PROXY (interface)), input ? BT_SERV_HSP : BT_SERV_AUDIO_SINK))
                    {
                        GVariant *name = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Alias");
                        GVariant *icon = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Icon");
                        GVariant *paired = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Paired");
                        GVariant *trusted = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Trusted");
                        if (name && icon && paired && trusted && g_variant_get_boolean (paired) && g_variant_get_boolean (trusted))
                        {
                            if (input)
                            {
                                // create a menu if there isn't one already
                                if (!vol->inputs) vol->inputs = gtk_menu_new ();
                                volumealsa_add_item_to_menu (vol, vol->inputs, g_variant_get_string (name, NULL), objpath, TRUE, TRUE, G_CALLBACK (volumealsa_set_bluetooth_input));
                            }
                            else
                                volumealsa_add_item_to_menu (vol, vol->outputs, g_variant_get_string (name, NULL), objpath, TRUE, FALSE, G_CALLBACK (volumealsa_set_bluetooth_output));
                        }
                        g_variant_unref (name);
                        g_variant_unref (icon);
                        g_variant_unref (paired);
                        g_variant_unref (trusted);
                    }
                    break;
                }
                interfaces = interfaces->next;
            }
            objects = objects->next;
        }
    }
}

const char *volumealsa_device_display_name (VolumeALSAPlugin *vol, const char *name)
{
    if (!g_strcmp0 (name, "bcm2835 HDMI 1"))
        return vol->hdmis == 1 ? _("HDMI") : vol->mon_names[0];
    else if (!g_strcmp0 (name, "bcm2835 HDMI 2"))
        return vol->hdmis == 1 ? _("HDMI") : vol->mon_names[1];
    else if (!g_strcmp0 (name, "bcm2835 Headphones"))
        return _("Analog");
    else return name;
}

static void volumealsa_build_device_menu (VolumeALSAPlugin *vol)
{
    GtkWidget *mi;

    vol->menu_popup = gtk_menu_new ();

    // create input selector
    vol->inputs = NULL;

    // add ALSA inputs
    pulse_add_devices_to_menu (vol, TRUE, FALSE);
    menu_add_separator (vol->inputs);

    // add Bluetooth inputs
    volumealsa_menu_add_bluetooth (vol, TRUE);

    if (vol->inputs)
    {
        menu_add_separator (vol->inputs);

#ifdef OPTIONS
        mi = gtk_image_menu_item_new_with_label (_("Input Device Settings..."));
        g_signal_connect (mi, "activate", G_CALLBACK (volumealsa_open_input_config_dialog), (gpointer) vol);
        gtk_menu_shell_append (GTK_MENU_SHELL (vol->inputs), mi);
#endif

        mi = gtk_image_menu_item_new_with_label (_("Device Profiles..."));
        g_signal_connect (mi, "activate", G_CALLBACK (volumealsa_open_profile_dialog), (gpointer) vol);
        gtk_menu_shell_append (GTK_MENU_SHELL (vol->inputs), mi);
    }

    // create a submenu for the outputs if there is an input submenu
    if (vol->inputs) vol->outputs = gtk_menu_new ();
    else vol->outputs = vol->menu_popup;

    // add internal outputs
    pulse_add_devices_to_menu (vol, FALSE, TRUE);
    menu_add_separator (vol->outputs);

    // add external outputs
    pulse_add_devices_to_menu (vol, FALSE, FALSE);
    menu_add_separator (vol->outputs);

    // add Bluetooth devices
    volumealsa_menu_add_bluetooth (vol, FALSE);

    // did we find any output devices? if not, the menu will be empty...
    if (gtk_container_get_children (GTK_CONTAINER (vol->outputs)) != NULL)
    {
        // add the output options menu item to the output menu
        menu_add_separator (vol->outputs);

#ifdef OPTIONS
        mi = gtk_image_menu_item_new_with_label (_("Output Device Settings..."));
        g_signal_connect (mi, "activate", G_CALLBACK (volumealsa_open_config_dialog), (gpointer) vol);
        gtk_menu_shell_append (GTK_MENU_SHELL (vol->outputs), mi);
#endif

        mi = gtk_image_menu_item_new_with_label (_("Device Profiles..."));
        g_signal_connect (mi, "activate", G_CALLBACK (volumealsa_open_profile_dialog), (gpointer) vol);
        gtk_menu_shell_append (GTK_MENU_SHELL (vol->outputs), mi);

        if (vol->inputs)
        {
            // insert submenus
            mi = gtk_menu_item_new_with_label (_("Audio Outputs"));
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), vol->outputs);
            gtk_menu_shell_append (GTK_MENU_SHELL (vol->menu_popup), mi);

            mi = gtk_separator_menu_item_new ();
            gtk_menu_shell_append (GTK_MENU_SHELL (vol->menu_popup), mi);

            mi = gtk_menu_item_new_with_label (_("Audio Inputs"));
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), vol->inputs);
            gtk_menu_shell_append (GTK_MENU_SHELL (vol->menu_popup), mi);
        }
    }
    else
    {
        mi = gtk_image_menu_item_new_with_label (_("No audio devices found"));
        gtk_widget_set_sensitive (GTK_WIDGET (mi), FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (vol->menu_popup), mi);
    }

    // update the menu item names, which are currently ALSA device names, to PulseAudio sink/source names
    pulse_update_devices (vol);

    // show the fallback sink and source in the menu
    pulse_get_default_sink_source (vol);
    gtk_container_foreach (GTK_CONTAINER (vol->outputs), volumealsa_menu_show_default_sink, vol);
    gtk_container_foreach (GTK_CONTAINER (vol->inputs), volumealsa_menu_show_default_source, vol);

    // lock menu if a dialog is open
    if (vol->conn_dialog || vol->options_dlg)
    {
        GList *items = gtk_container_get_children (GTK_CONTAINER (vol->menu_popup));
        while (items)
        {
            gtk_widget_set_sensitive (GTK_WIDGET (items->data), FALSE);
            items = items->next;
        }
        g_list_free (items);
    }
}

void volumealsa_set_external_output (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    if (strstr (vol->pa_default_sink, "bluez") && pa_bt_sink_source_compare (vol->pa_default_sink, vol->pa_default_source))
    {
        // if the current default sink is Bluetooth and not also the default source, disconnect it
        char *bt_name = bluez_from_pa_name (vol->pa_default_sink);
        bt_disconnect_device (vol, bt_name);
        g_free (bt_name);
    }

    pulse_change_sink (vol, gtk_widget_get_name (widget));
    volumealsa_update_display (vol);
}

void volumealsa_set_external_input (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    if (strstr (vol->pa_default_source, "bluez") && pa_bt_sink_source_compare (vol->pa_default_sink, vol->pa_default_source))
    {
        // if the current default source is Bluetooth and not also the default sink, disconnect it
        char *bt_name = bluez_from_pa_name (vol->pa_default_source);
        bt_disconnect_device (vol, bt_name);
        g_free (bt_name);
    }

    pulse_change_source (vol, gtk_widget_get_name (widget));
    volumealsa_update_display (vol);
}

static void volumealsa_set_bluetooth_output (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    volumealsa_update_display (vol);

    char *odevice = bluez_from_pa_name (vol->pa_default_sink);

    // is this device already connected and attached - might want to force reconnect here?
    if (!g_strcmp0 (widget->name, odevice))
    {
        DEBUG ("Reconnect device %s", widget->name);
        // store the name of the BlueZ device to connect to
        if (vol->bt_conname) g_free (vol->bt_conname);
        vol->bt_conname = g_strdup (widget->name);
        vol->bt_input = FALSE;

        // show the connection dialog
        volumealsa_show_connect_dialog (vol, FALSE, gtk_menu_item_get_label (GTK_MENU_ITEM (widget)));

        // disconnect the device prior to reconnect
        bt_disconnect_device (vol, odevice);

        g_free (odevice);
        return;
    }

    char *idevice = bluez_from_pa_name (vol->pa_default_source);

    // check to see if this device is already connected
    if (!g_strcmp0 (widget->name, idevice))
    {
        DEBUG ("Device %s is already connected", widget->name);
        char *pacard = bluez_to_pa_card_name (widget->name);
        pulse_get_profile (vol, pacard);
        char *paname = bluez_to_pa_sink_name (widget->name, vol->pa_profile);
        pulse_change_sink (vol, paname);
        g_free (paname);
        g_free (pacard);
        volumealsa_update_display (vol);

        /* disconnect old Bluetooth output device */
        if (odevice) bt_disconnect_device (vol, odevice);
    }
    else
    {
        DEBUG ("Need to connect device %s", widget->name);
        // store the name of the BlueZ device to connect to
        if (vol->bt_conname) g_free (vol->bt_conname);
        vol->bt_conname = g_strdup (widget->name);
        vol->bt_input = FALSE;

        // show the connection dialog
        volumealsa_show_connect_dialog (vol, FALSE, gtk_menu_item_get_label (GTK_MENU_ITEM (widget)));

        // disconnect the current output device unless it is also the input device; otherwise just connect the new device
        if (odevice && g_strcmp0 (idevice, odevice)) bt_disconnect_device (vol, odevice);
        else bt_connect_device (vol);
    }

    if (idevice) g_free (idevice);
    if (odevice) g_free (odevice);
}

static void volumealsa_set_bluetooth_input (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    char *idevice = bluez_from_pa_name (vol->pa_default_source);

    // is this device already connected and attached - might want to force reconnect here?
    if (!g_strcmp0 (widget->name, idevice))
    {
        DEBUG ("Reconnect device %s", widget->name);
        // store the name of the BlueZ device to connect to
        if (vol->bt_conname) g_free (vol->bt_conname);
        vol->bt_conname = g_strdup (widget->name);
        vol->bt_input = TRUE;

        // show the connection dialog
        volumealsa_show_connect_dialog (vol, FALSE, gtk_menu_item_get_label (GTK_MENU_ITEM (widget)));

        // disconnect the current input device unless it is also the output device; otherwise just connect the new device
        bt_disconnect_device (vol, idevice);

        g_free (idevice);
        return;
    }

    char *odevice = bluez_from_pa_name (vol->pa_default_sink);

    // check to see if this device is already connected
    if (!g_strcmp0 (widget->name, odevice))
    {
        DEBUG ("Device %s is already connected\n", widget->name);
        char *paname = bluez_to_pa_source_name (widget->name);
        char *pacard = bluez_to_pa_card_name (widget->name);
        pulse_set_profile (vol, pacard, "headset_head_unit");
        pulse_change_source (vol, paname);
        g_free (paname);
        g_free (pacard);

        /* disconnect old Bluetooth input device */
        if (idevice) bt_disconnect_device (vol, idevice);
    }
    else
    {
        DEBUG ("Need to connect device %s", widget->name);
        // store the name of the BlueZ device to connect to
        if (vol->bt_conname) g_free (vol->bt_conname);
        vol->bt_conname = g_strdup (widget->name);
        vol->bt_input = TRUE;

        // show the connection dialog
        volumealsa_show_connect_dialog (vol, FALSE, gtk_menu_item_get_label (GTK_MENU_ITEM (widget)));

        // disconnect the current input device unless it is also the output device; otherwise just connect the new device
        if (idevice && g_strcmp0 (idevice, odevice)) bt_disconnect_device (vol, idevice);
        else bt_connect_device (vol);
    }

    if (idevice) g_free (idevice);
    if (odevice) g_free (odevice);
}


/*----------------------------------------------------------------------------*/
/* Volume scale popup window                                                  */
/*----------------------------------------------------------------------------*/

/* Build the window that appears when the top level widget is clicked. */
static void volumealsa_build_popup_window (GtkWidget *p)
{
    VolumeALSAPlugin *vol = lxpanel_plugin_get_data (p);

    if (vol->popup_window) gtk_widget_destroy (vol->popup_window);

    /* Create a new window. */
    vol->popup_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name (vol->popup_window, "volals");
    gtk_window_set_decorated (GTK_WINDOW (vol->popup_window), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (vol->popup_window), 5);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (vol->popup_window), TRUE);
    gtk_window_set_skip_pager_hint (GTK_WINDOW (vol->popup_window), TRUE);
    gtk_window_set_type_hint (GTK_WINDOW (vol->popup_window), GDK_WINDOW_TYPE_HINT_DIALOG);

    /* Create a scrolled window as the child of the top level window. */
    GtkWidget *scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_set_name (scrolledwindow, "whitewd");
    gtk_container_set_border_width (GTK_CONTAINER (scrolledwindow), 0);
    gtk_widget_show (scrolledwindow);
    gtk_container_add (GTK_CONTAINER (vol->popup_window), scrolledwindow);
    gtk_widget_set_can_focus (scrolledwindow, FALSE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_NONE);

    /* Create a viewport as the child of the scrolled window. */
    GtkWidget *viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scrolledwindow), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_show (viewport);

    gtk_container_set_border_width (GTK_CONTAINER (vol->popup_window), 0);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_SHADOW_IN);
    /* Create a vertical box as the child of the viewport. */
    GtkWidget *box = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (viewport), box);

    /* Create a vertical scale as the child of the vertical box. */
    vol->volume_scale = gtk_vscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (100, 0, 100, 0, 0, 0)));
    gtk_widget_set_name (vol->volume_scale, "volscale");
    g_object_set (vol->volume_scale, "height-request", 120, NULL);
    gtk_scale_set_draw_value (GTK_SCALE (vol->volume_scale), FALSE);
    gtk_range_set_inverted (GTK_RANGE (vol->volume_scale), TRUE);
    gtk_box_pack_start (GTK_BOX (box), vol->volume_scale, TRUE, TRUE, 0);
    gtk_widget_set_can_focus (vol->volume_scale, FALSE);

    /* Value-changed and scroll-event signals. */
    vol->volume_scale_handler = g_signal_connect (vol->volume_scale, "value-changed", G_CALLBACK (volumealsa_popup_scale_changed), vol);
    g_signal_connect (vol->volume_scale, "scroll-event", G_CALLBACK (volumealsa_popup_scale_scrolled), vol);

    /* Create a check button as the child of the vertical box. */
    vol->mute_check = gtk_check_button_new_with_label (_("Mute"));
    gtk_box_pack_end (GTK_BOX (box), vol->mute_check, FALSE, FALSE, 0);
    vol->mute_check_handler = g_signal_connect (vol->mute_check, "toggled", G_CALLBACK (volumealsa_popup_mute_toggled), vol);
    gtk_widget_set_can_focus (vol->mute_check, FALSE);
}

/* Handler for "value_changed" signal on popup window vertical scale. */
static void volumealsa_popup_scale_changed (GtkRange *range, VolumeALSAPlugin *vol)
{
    /* Reflect the value of the control to the sound system. */
    if (!pulse_get_mute (vol))
        pulse_set_volume (vol, gtk_range_get_value (range));

    /* Redraw the controls. */
    volumealsa_update_display (vol);
}

/* Handler for "scroll-event" signal on popup window vertical scale. */
static void volumealsa_popup_scale_scrolled (GtkScale *scale, GdkEventScroll *evt, VolumeALSAPlugin *vol)
{
    /* Get the state of the vertical scale. */
    gdouble val = gtk_range_get_value (GTK_RANGE (vol->volume_scale));

    /* Dispatch on scroll direction to update the value. */
    if ((evt->direction == GDK_SCROLL_UP) || (evt->direction == GDK_SCROLL_LEFT))
        val += 2;
    else
        val -= 2;

    /* Reset the state of the vertical scale.  This provokes a "value_changed" event. */
    gtk_range_set_value (GTK_RANGE (vol->volume_scale), CLAMP((int) val, 0, 100));
}

/* Handler for "toggled" signal on popup window mute checkbox. */
static void volumealsa_popup_mute_toggled (GtkWidget *widget, VolumeALSAPlugin *vol)
{
    /* Reflect the mute toggle to the sound system. */
    pulse_set_mute (vol, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));

    /* Redraw the controls. */
    volumealsa_update_display (vol);
}

static void volumealsa_popup_set_position (GtkWidget *menu, gint *px, gint *py, gboolean *push_in, gpointer data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) data;

    /* Determine the coordinates. */
    lxpanel_plugin_popup_set_position_helper (vol->panel, vol->plugin, menu, px, py);
    *push_in = TRUE;
}

/* Handler for "focus-out" signal on popup window. */
static gboolean volumealsa_mouse_out (GtkWidget *widget, GdkEventButton *event, VolumeALSAPlugin *vol)
{
    /* Hide the widget. */
    gtk_widget_hide (vol->popup_window);
    vol->show_popup = FALSE;
    gdk_pointer_ungrab (GDK_CURRENT_TIME);
    return FALSE;
}

/*----------------------------------------------------------------------------*/
/* Profiles dialog                                                            */
/*----------------------------------------------------------------------------*/

static void show_profiles (VolumeALSAPlugin *vol)
{
    GtkWidget *btn, *wid, *box;
    char *lbl;

    // create the window itself
    vol->options_dlg = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (vol->options_dlg), _("Device Profiles"));
    gtk_window_set_position (GTK_WINDOW (vol->options_dlg), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size (GTK_WINDOW (vol->options_dlg), 400, 300);
    gtk_container_set_border_width (GTK_CONTAINER (vol->options_dlg), 10);
    gtk_window_set_icon_name (GTK_WINDOW (vol->options_dlg), "multimedia-volume-control");
    g_signal_connect (vol->options_dlg, "delete-event", G_CALLBACK (profiles_wd_close_handler), vol);

    box = gtk_vbox_new (FALSE, 5);
    vol->intprofiles = gtk_vbox_new (FALSE, 5);
    vol->alsaprofiles = gtk_vbox_new (FALSE, 5);
    vol->btprofiles = gtk_vbox_new (FALSE, 5);
    gtk_container_add (GTK_CONTAINER (vol->options_dlg), box);
    gtk_box_pack_start (GTK_BOX (box), vol->intprofiles, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), vol->alsaprofiles, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), vol->btprofiles, FALSE, FALSE, 0);

    // first loop through cards
    pulse_add_devices_to_profile_dialog (vol);

    // then loop through Bluetooth devices
    if (vol->objmanager)
    {
        // iterate all the objects the manager knows about
        GList *objects = g_dbus_object_manager_get_objects (vol->objmanager);
        while (objects != NULL)
        {
            GDBusObject *object = (GDBusObject *) objects->data;
            const char *objpath = g_dbus_object_get_object_path (object);
            GList *interfaces = g_dbus_object_get_interfaces (object);
            while (interfaces != NULL)
            {
                // if an object has a Device1 interface, it is a Bluetooth device - add it to the list
                GDBusInterface *interface = G_DBUS_INTERFACE (interfaces->data);
                if (g_strcmp0 (g_dbus_proxy_get_interface_name (G_DBUS_PROXY (interface)), "org.bluez.Device1") == 0)
                {
                    if (bt_has_service (vol, g_dbus_proxy_get_object_path (G_DBUS_PROXY (interface)), BT_SERV_HSP)
                        || bt_has_service (vol, g_dbus_proxy_get_object_path (G_DBUS_PROXY (interface)), BT_SERV_AUDIO_SINK))
                    {
                        GVariant *name = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Alias");
                        GVariant *icon = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Icon");
                        GVariant *paired = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Paired");
                        GVariant *trusted = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (interface), "Trusted");
                        if (name && icon && paired && trusted && g_variant_get_boolean (paired) && g_variant_get_boolean (trusted))
                        {
                            // only disconnected devices here...
                            char *pacard = bluez_to_pa_card_name ((char *) objpath);
                            pulse_get_profile (vol, pacard);
                            if (vol->pa_profile == NULL)
                                volumealsa_add_combo_to_profiles (vol, NULL, vol->btprofiles, 0, NULL, g_variant_get_string (name, NULL));
                        }
                        g_variant_unref (name);
                        g_variant_unref (icon);
                        g_variant_unref (paired);
                        g_variant_unref (trusted);
                    }
                    break;
                }
                interfaces = interfaces->next;
            }
            objects = objects->next;
        }
    }

    wid = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (wid), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (box), wid, FALSE, FALSE, 5);

    btn = gtk_button_new_from_stock (GTK_STOCK_OK);
    g_signal_connect (btn, "clicked", G_CALLBACK (profiles_ok_handler), vol);
    gtk_box_pack_end (GTK_BOX (wid), btn, FALSE, FALSE, 5);

    gtk_widget_show_all (vol->options_dlg);
}

static void close_profiles (VolumeALSAPlugin *vol)
{
    gtk_widget_destroy (vol->options_dlg);
    vol->options_dlg = NULL;
}

static void profiles_ok_handler (GtkButton *button, gpointer *user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;

    close_profiles (vol);
}

static gboolean profiles_wd_close_handler (GtkWidget *wid, GdkEvent *event, gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;

    close_profiles (vol);
    return TRUE;
}

static void relocate_last_item (GtkWidget *box)
{
    GtkWidget *elem;
    GList *children = gtk_container_get_children (GTK_CONTAINER (box));
    int n = g_list_length (children);
    GtkWidget *newcomb = g_list_nth_data (children, n - 1);
    GtkWidget *newlab = g_list_nth_data (children, n - 2);
    const char *new_item = gtk_label_get_text (GTK_LABEL (newlab));
    n -= 2;
    while (n > 0)
    {
        elem = g_list_nth_data (children, n - 2);
        if (g_strcmp0 (new_item, gtk_label_get_text (GTK_LABEL (elem))) >= 0) break;
        n -= 2;
    }
    gtk_box_reorder_child (GTK_BOX (box), newlab, n);
    gtk_box_reorder_child (GTK_BOX (box), newcomb, n + 1);
}

static void profile_changed (GtkComboBox *combo, gpointer *userdata)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) userdata;

    const char *option;
    GtkTreeIter iter;

    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter, 0, &option, -1);
    pulse_set_profile (vol, gtk_widget_get_name (GTK_WIDGET (combo)), option);
}

void volumealsa_add_combo_to_profiles (VolumeALSAPlugin *vol, GtkListStore *ls, GtkWidget *dest, int sel, const char *name, const char *label)
{
    GtkWidget *lbl, *comb;
    GtkCellRenderer *rend;

    lbl = gtk_label_new (label);
    gtk_box_pack_start (GTK_BOX (dest), lbl, FALSE, FALSE, 5);

    if (ls)
    {
        comb = gtk_combo_box_new_with_model (GTK_TREE_MODEL (ls));
        gtk_widget_set_name (comb, name);
        rend = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (comb), rend, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (comb), rend, "text", 1);
    }
    else
    {
        comb = gtk_combo_box_text_new ();
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (comb), _("Device not connected"));
        gtk_widget_set_sensitive (comb, FALSE);
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (comb), sel);
    gtk_box_pack_start (GTK_BOX (dest), comb, FALSE, FALSE, 5);

    relocate_last_item (dest);

    if (ls) g_signal_connect (comb, "changed", G_CALLBACK (profile_changed), vol);
}

/* Bluetooth name remapping
 * ------------------------
 *
 * Helper functions to remap PulseAudio sink and source names to and from
 * Bluez device names.
 */

static char *bluez_to_pa_sink_name (char *bluez_name, char *profile)
{
    unsigned int b1, b2, b3, b4, b5, b6;

    if (bluez_name == NULL) return NULL;
    if (sscanf (bluez_name, "/org/bluez/hci0/dev_%x_%x_%x_%x_%x_%x", &b1, &b2, &b3, &b4, &b5, &b6) != 6)
    {
        DEBUG ("Bluez name invalid : %s", bluez_name);
        return NULL;
    }
    return g_strdup_printf ("bluez_sink.%02X_%02X_%02X_%02X_%02X_%02X.%s", b1, b2, b3, b4, b5, b6, profile);
}

static char *bluez_to_pa_source_name (char *bluez_name)
{
    unsigned int b1, b2, b3, b4, b5, b6;

    if (bluez_name == NULL) return NULL;
    if (sscanf (bluez_name, "/org/bluez/hci0/dev_%x_%x_%x_%x_%x_%x", &b1, &b2, &b3, &b4, &b5, &b6) != 6)
    {
        DEBUG ("Bluez name invalid : %s", bluez_name);
        return NULL;
    }
    return g_strdup_printf ("bluez_source.%02X_%02X_%02X_%02X_%02X_%02X.headset_head_unit", b1, b2, b3, b4, b5, b6);
}

static char *bluez_to_pa_card_name (char *bluez_name)
{
    unsigned int b1, b2, b3, b4, b5, b6;

    if (bluez_name == NULL) return NULL;
    if (sscanf (bluez_name, "/org/bluez/hci0/dev_%x_%x_%x_%x_%x_%x", &b1, &b2, &b3, &b4, &b5, &b6) != 6)
    {
        DEBUG ("Bluez name invalid : %s", bluez_name);
        return NULL;
    }
    return g_strdup_printf ("bluez_card.%02X_%02X_%02X_%02X_%02X_%02X", b1, b2, b3, b4, b5, b6);
}

static char *bluez_from_pa_name (char *pa_name)
{
    unsigned int b1, b2, b3, b4, b5, b6;

    if (pa_name == NULL) return NULL;
    if (strstr (pa_name, "bluez") == NULL) return NULL;
    if (sscanf (strstr (pa_name, ".") + 1, "%x_%x_%x_%x_%x_%x", &b1, &b2, &b3, &b4, &b5, &b6) != 6) return NULL;
    return g_strdup_printf ("/org/bluez/hci0/dev_%02X_%02X_%02X_%02X_%02X_%02X", b1, b2, b3, b4, b5, b6);
}

static int pa_bt_sink_source_compare (char *sink, char *source)
{
    if (sink == NULL || source == NULL) return 1;
    if (strstr (sink, "bluez") == NULL) return 1;
    if (strstr (source, "bluez") == NULL) return 1;
    return strncmp (sink + 11, source + 13, 17);
}

static int pa_bluez_device_same (const char *padev, const char *btdev)
{
    if (strstr (btdev, "bluez") && strstr (padev, btdev + 20)) return 1;
    return 0;
}

/*----------------------------------------------------------------------------*/
/* Plugin structure                                                           */
/*----------------------------------------------------------------------------*/

/* Callback when panel configuration changes */

static void volumealsa_panel_configuration_changed (LXPanel *panel, GtkWidget *plugin)
{
    VolumeALSAPlugin *vol = lxpanel_plugin_get_data (plugin);

    volumealsa_build_popup_window (vol->plugin);
    volumealsa_update_display (vol);
    if (vol->show_popup) gtk_widget_show_all (vol->popup_window);
}

/* Callback when control message arrives */

static gboolean volumealsa_control_msg (GtkWidget *plugin, const char *cmd)
{
    VolumeALSAPlugin *vol = lxpanel_plugin_get_data (plugin);

    if (!strncmp (cmd, "mute", 4))
    {
        pulse_set_mute (vol, pulse_get_mute (vol) ? 0 : 1);
        volumealsa_update_display (vol);
        return TRUE;
    }

    if (!strncmp (cmd, "volu", 4))
    {
        if (pulse_get_mute (vol)) pulse_set_mute (vol, 0);
        else
        {
            int volume = pulse_get_volume (vol);
            if (volume < 100)
            {
                volume += 5;
                volume /= 5;
                volume *= 5;
            }
            pulse_set_volume (vol, volume);
        }
        volumealsa_update_display (vol);
        return TRUE;
    }

    if (!strncmp (cmd, "vold", 4))
    {
        if (pulse_get_mute (vol)) pulse_set_mute (vol, 0);
        else
        {
            int volume = pulse_get_volume (vol);
            if (volume > 0)
            {
                volume -= 1; // effectively -5 + 4 for rounding...
                volume /= 5;
                volume *= 5;
            }
            pulse_set_volume (vol, volume);
        }
        volumealsa_update_display (vol);
        return TRUE;
    }

    return FALSE;
}

/* Plugin constructor */

static GtkWidget *volumealsa_constructor (LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    VolumeALSAPlugin *vol = g_new0 (VolumeALSAPlugin, 1);
    GtkWidget *p;

#ifdef ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    vol->bt_conname = NULL;
    vol->bt_reconname = NULL;
    vol->options_dlg = NULL;

    /* Allocate top level widget and set into Plugin widget pointer. */
    vol->panel = panel;
    vol->plugin = p = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (vol->plugin), GTK_RELIEF_NONE);
    g_signal_connect (vol->plugin, "button-press-event", G_CALLBACK (volumealsa_button_press_event), vol->panel);
    vol->settings = settings;
    lxpanel_plugin_set_data (p, vol, volumealsa_destructor);
    gtk_widget_add_events (p, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_tooltip_text (p, _("Volume control"));

    /* Allocate icon as a child of top level. */
    vol->tray_icon = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (p), vol->tray_icon);

    /* Set up callbacks to see if BlueZ is on DBus */
    g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.bluez", 0, bt_cb_name_owned, bt_cb_name_unowned, vol, NULL);

    /* Initialize volume scale */
    volumealsa_build_popup_window (p);

    /* Connect signals. */
    g_signal_connect (G_OBJECT (p), "scroll-event", G_CALLBACK (volumealsa_popup_scale_scrolled), vol);
    g_signal_connect (panel_get_icon_theme (panel), "changed", G_CALLBACK (volumealsa_theme_change), vol);

    /* Set up for multiple HDMIs */
    vol->hdmis = hdmi_monitors (vol);

    /* set up PulseAudio context */
    pulse_init (vol);
    pulse_get_default_sink_source (vol);

    /* Update the display, show the widget, and return. */
    volumealsa_update_display (vol);
    gtk_widget_show_all (p);

    return p;
}

/* Plugin destructor */

static void volumealsa_destructor (gpointer user_data)
{
    VolumeALSAPlugin *vol = (VolumeALSAPlugin *) user_data;

    pulse_terminate (vol);

    /* If the dialog box is open, dismiss it. */
    if (vol->popup_window != NULL) gtk_widget_destroy (vol->popup_window);
    if (vol->menu_popup != NULL) gtk_widget_destroy (vol->menu_popup);

    if (vol->panel) g_signal_handlers_disconnect_by_func (panel_get_icon_theme (vol->panel), volumealsa_theme_change, vol);

    /* Deallocate all memory. */
    g_free (vol);
}

FM_DEFINE_MODULE (lxpanel_gtk, volumepulse)

/* Plugin descriptor */

LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = N_("Volume Control (PulseAudio)"),
    .description = N_("Display and control volume for PulseAudio"),
    .new_instance = volumealsa_constructor,
    .reconfigure = volumealsa_panel_configuration_changed,
    .control = volumealsa_control_msg,
    .gettext_package = GETTEXT_PACKAGE
};

/* End of file */
/*----------------------------------------------------------------------------*/
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

#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include "plugin.h"

#define DEBUG_ON
#ifdef DEBUG_ON
#define DEBUG(fmt,args...) if(getenv("DEBUG_VP"))g_message("vp: " fmt,##args)
#else
#define DEBUG
#endif

typedef struct {
    /* plugin */
    GtkWidget *plugin;                  /* Back pointer to widget */
    LXPanel *panel;                     /* Back pointer to panel */
    config_setting_t *settings;         /* Plugin settings */

    /* graphics */
    GtkWidget *tray_icon;               /* Displayed icon */
    GtkWidget *popup_window;            /* Top level window for popup */
    GtkWidget *volume_scale;            /* Scale for volume */
    GtkWidget *mute_check;              /* Checkbox for mute state */
    GtkWidget *menu_popup;              /* Right-click menu */
    GtkWidget *options_dlg;             /* Device options dialog */
    GtkWidget *outputs;                 /* Output select menu */
    GtkWidget *inputs;                  /* Input select menu */
    GtkWidget *intprofiles;             /* Vbox for profile combos */
    GtkWidget *alsaprofiles;            /* Vbox for profile combos */
    GtkWidget *btprofiles;              /* Vbox for profile combos */
    gboolean show_popup;                /* Toggle to show and hide the popup on left click */
    guint volume_scale_handler;         /* Handler for vscale widget */
    guint mute_check_handler;           /* Handler for mute_check widget */

    /* Bluetooth interface */
    GDBusObjectManager *objmanager;     /* BlueZ object manager */
    char *bt_conname;                   /* BlueZ name of device - just used during connection */
    char *bt_reconname;                 /* BlueZ name of second device - used during reconnection */
    gboolean bt_input;                  /* Is the device being connected as an input or an output? */
    GtkWidget *conn_dialog;             /* Connection dialog box */
    GtkWidget *conn_label;              /* Dialog box text field */
    GtkWidget *conn_ok;                 /* Dialog box button */

    /* HDMI devices */
    guint hdmis;                        /* Number of HDMI devices */
    char *mon_names[2];                 /* Names of HDMI devices */

    /* PulseAudio interface */
    pa_threaded_mainloop *pa_mainloop;  /* Controller loop variable */
    pa_context *pa_context;             /* Controller context */
    pa_context_state_t pa_state;        /* Current controller state */
    char *pa_default_sink;              /* Current default sink name */
    char *pa_default_source;            /* Current default source name */
    int pa_channels;                    /* Number of channels on default sink */
    int pa_volume;                      /* Volume setting on default sink */
    int pa_mute;                        /* Mute setting on default sink */
    char *pa_profile;                   /* Current profile for card */
    GList *pa_indices;                  /* Indices for current streams */

} VolumeALSAPlugin;

/* End of file */
/*----------------------------------------------------------------------------*/
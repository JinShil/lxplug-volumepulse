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

extern char *get_string (const char *fmt, ...);
extern int vsystem (const char *fmt, ...);
extern void close_widget (GtkWidget **wid);

extern void menu_create (VolumePulsePlugin *vol);
extern void menu_add_separator (VolumePulsePlugin *vol, GtkWidget *menu);
extern void menu_mark_default (GtkWidget *widget, gpointer data);
extern void menu_set_alsa_device (GtkWidget *widget, VolumePulsePlugin *vol);
extern void menu_set_bluetooth_device (GtkWidget *widget, VolumePulsePlugin *vol);

extern gboolean volumepulse_button_press_event (GtkWidget *widget, GdkEventButton *event, VolumePulsePlugin *vol);
extern void volumepulse_mouse_scrolled (GtkScale *scale, GdkEventScroll *evt, VolumePulsePlugin *vol);
extern void volumepulse_configuration_changed (LXPanel *panel, GtkWidget *plugin);
extern gboolean volumepulse_control_msg (GtkWidget *plugin, const char *cmd);
extern void volumepulse_destructor (gpointer user_data);

/* End of file */
/*----------------------------------------------------------------------------*/

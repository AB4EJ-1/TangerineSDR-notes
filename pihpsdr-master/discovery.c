/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "discovered.h"
#include "old_discovery.h"
#include "new_discovery.h"
#include "main.h"
#include "radio.h"
#ifdef USBOZY
#include "ozyio.h"
#endif
#ifdef REMOTE
#include "remote_radio.h"
#endif
#ifdef STEMLAB_DISCOVERY
#include "stemlab_discovery.h"
#endif
#include "ext.h"
#include "configure.h"

static GtkWidget *discovery_dialog;
static DISCOVERED *d;

#ifdef STEMLAB_DISCOVERY
static GtkWidget *apps_combobox[MAX_DEVICES];
#endif

GtkWidget *tcpaddr;
#define IPADDR_LEN 20
static char ipaddr_tcp_buf[IPADDR_LEN] = "10.10.10.10";
char *ipaddr_tcp = &ipaddr_tcp_buf[0];

static gboolean start_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  radio=(DISCOVERED *)data;
#ifdef STEMLAB_DISCOVERY
  // We need to start the STEMlab app before destroying the dialog, since
  // we otherwise lose the information about which app has been selected.
  if (radio->protocol == STEMLAB_PROTOCOL) {
    const int device_id = radio - discovered;
    int ret;
    ret=stemlab_start_app(gtk_combo_box_get_active_id(GTK_COMBO_BOX(apps_combobox[device_id])));
    //
    // We have started the SDR app on the RedPitaya, but may need to fill
    // in information necessary for starting the radio, including the
    // MAC address and the interface listening to. Even when using AVAHI,
    // we miss some information.
    // To get all required info, we do a "fake" discovery on the RedPitaya IP address.
    // Here we also try TCP if UDP does not work, such that we can work with STEMlabs
    // in remote subnets.
    //
    if (ret == 0) {
      ret=stemlab_get_info(device_id);
    }
    // At this point, if stemlab_start_app failed, we cannot recover
    if (ret != 0) exit(-1);
  }
  stemlab_cleanup();
#endif
  gtk_widget_destroy(discovery_dialog);
  start_radio();
  return TRUE;
}

#ifdef GPIO
static gboolean gpio_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  configure_gpio(discovery_dialog);
  return TRUE;
}
#endif

static gboolean discover_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  g_idle_add(ext_discovery,NULL);
  return TRUE;
}

static gboolean exit_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
  gtk_widget_destroy(discovery_dialog);
  _exit(0);
  return TRUE;
}

static gboolean tcp_cb (GtkWidget *widget, GdkEventButton *event, gpointer data) {
    strncpy(ipaddr_tcp, gtk_entry_get_text(GTK_ENTRY(tcpaddr)), IPADDR_LEN);
    ipaddr_tcp[IPADDR_LEN-1]=0;
    // remove possible trailing newline chars in ipaddr_tcp
	    int len=strnlen(ipaddr_tcp,IPADDR_LEN);
	    while (--len >= 0) {
	      if (ipaddr_tcp[len] != '\n') break;
	      ipaddr_tcp[len]=0;
	    }
	    //fprintf(stderr,"New TCP addr = %s.\n", ipaddr_tcp);
	    // save this value to config file
	    FILE *fp = fopen("ip.addr", "w");
	    if (fp) {
		fprintf(fp,"%s\n",ipaddr_tcp);
		fclose(fp);
	    }
	    gtk_widget_destroy(discovery_dialog);
	    g_idle_add(ext_discovery,NULL);
	    return TRUE;
	}

void discovery() {
//fprintf(stderr,"discovery\n");
  selected_device=0;
  devices=0;

  // Try to locate IP addr
  FILE *fp=fopen("ip.addr","r");
  if (fp) {
    fgets(ipaddr_tcp, IPADDR_LEN,fp);
    fclose(fp);
    ipaddr_tcp[IPADDR_LEN-1]=0;
    // remove possible trailing newline char in ipaddr_tcp
    int len=strnlen(ipaddr_tcp,IPADDR_LEN);
    while (--len >= 0) {
      if (ipaddr_tcp[len] != '\n') break;
      ipaddr_tcp[len]=0;
    }
  }
#ifdef USBOZY
//
// first: look on USB for an Ozy
//
  fprintf(stderr,"looking for USB based OZY devices\n");

  if (ozy_discover() != 0)
  {
    discovered[devices].protocol = ORIGINAL_PROTOCOL;
    discovered[devices].device = DEVICE_OZY;
    discovered[devices].software_version = 10;              // we can't know yet so this isn't a real response
    discovered[devices].status = STATE_AVAILABLE;
    strcpy(discovered[devices].name,"Ozy on USB");

    strcpy(discovered[devices].info.network.interface_name,"USB");
    devices++;
  }
#endif

#ifdef STEMLAB_DISCOVERY
#ifdef NO_AVAHI
  status_text("Looking for STEMlab WEB apps");
#else
  status_text("STEMlab (Avahi) ... Discovering Devices");
#endif
  stemlab_discovery();
#endif

  status_text("Protocol 1 ... Discovering Devices");
  old_discovery();

  status_text("Protocol 2 ... Discovering Devices");
  new_discovery();

#ifdef LIMESDR
  status_text("LimeSDR ... Discovering Devices");
  lime_discovery();
#endif

  status_text("Discovery");
  
  if(devices==0) {
    gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));
    discovery_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog),GTK_WINDOW(top_window));
    gtk_window_set_title(GTK_WINDOW(discovery_dialog),"piHPSDR - Discovery");
    //gtk_window_set_decorated(GTK_WINDOW(discovery_dialog),FALSE);

    //gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("FreeMono 16"));

    GdkRGBA color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = 1.0;
    gtk_widget_override_background_color(discovery_dialog,GTK_STATE_FLAG_NORMAL,&color);

    GtkWidget *content;

    content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);

    GtkWidget *label=gtk_label_new("No devices found!");
    gtk_grid_attach(GTK_GRID(grid),label,0,0,2,1);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,0,1,1,1);

    GtkWidget *discover_b=gtk_button_new_with_label("Retry Discovery");
    g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),discover_b,1,1,1,1);

    GtkWidget *tcp_b=gtk_button_new_with_label("Use new TCP Addr:");
    g_signal_connect (tcp_b, "button-press-event", G_CALLBACK(tcp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),tcp_b,0,2,1,1);

    tcpaddr=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(tcpaddr), 20);
    gtk_grid_attach(GTK_GRID(grid),tcpaddr,1,2,1,1);
    gtk_entry_set_text(GTK_ENTRY(tcpaddr), ipaddr_tcp);

    gtk_container_add (GTK_CONTAINER (content), grid);
    gtk_widget_show_all(discovery_dialog);
  } else {
    fprintf(stderr,"discovery: found %d devices\n", devices);
    gdk_window_set_cursor(gtk_widget_get_window(top_window),gdk_cursor_new(GDK_ARROW));
    discovery_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(discovery_dialog),GTK_WINDOW(top_window));
    gtk_window_set_title(GTK_WINDOW(discovery_dialog),"piHPSDR - Discovery");
    //gtk_window_set_decorated(GTK_WINDOW(discovery_dialog),FALSE);

    //gtk_widget_override_font(discovery_dialog, pango_font_description_from_string("FreeMono 16"));

    GdkRGBA color;
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 1.0;
    color.alpha = 1.0;
    gtk_widget_override_background_color(discovery_dialog,GTK_STATE_FLAG_NORMAL,&color);

    GtkWidget *content;

    content=gtk_dialog_get_content_area(GTK_DIALOG(discovery_dialog));

    GtkWidget *grid=gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);
    //gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
    gtk_grid_set_row_spacing (GTK_GRID(grid),10);

    int i;
    char version[16];
    char text[256];
    for(i=0;i<devices;i++) {
      d=&discovered[i];
fprintf(stderr,"%p Protocol=%d name=%s\n",d,d->protocol,d->name);
      sprintf(version,"v%d.%d",
                        d->software_version/10,
                        d->software_version%10);
      switch(d->protocol) {
        case ORIGINAL_PROTOCOL:
        case NEW_PROTOCOL:
#ifdef USBOZY
          if(d->device==DEVICE_OZY) {
            sprintf(text,"%s (%s) on USB /dev/ozy", d->name, d->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2");
          } else {
#endif
            sprintf(text,"%s (%s %s) %s (%02X:%02X:%02X:%02X:%02X:%02X) on %s: ",
                          d->name,
                          d->protocol==ORIGINAL_PROTOCOL?"Protocol 1":"Protocol 2",
                          version,
                          inet_ntoa(d->info.network.address.sin_addr),
                          d->info.network.mac_address[0],
                          d->info.network.mac_address[1],
                          d->info.network.mac_address[2],
                          d->info.network.mac_address[3],
                          d->info.network.mac_address[4],
                          d->info.network.mac_address[5],
                          d->info.network.interface_name);
#ifdef USBOZY
          }
#endif
          break;
#ifdef LIMESDR
        case LIMESDR_PROTOCOL:
          sprintf(text,"%s",
                        d->name);
          break;
#endif
#ifdef STEMLAB_DISCOVERY
        case STEMLAB_PROTOCOL:
#ifdef NO_AVAHI
	  sprintf(text,"Choose RedPitaya App from %s and start radio: ",inet_ntoa(d->info.network.address.sin_addr));
#else
          sprintf(text, "STEMlab (%02X:%02X:%02X:%02X:%02X:%02X) on %s",
                         d->info.network.mac_address[0],
                         d->info.network.mac_address[1],
                         d->info.network.mac_address[2],
                         d->info.network.mac_address[3],
                         d->info.network.mac_address[4],
                         d->info.network.mac_address[5],
                         d->info.network.interface_name);
#endif
#endif
      }

      GtkWidget *label=gtk_label_new(text);
      gtk_widget_override_font(label, pango_font_description_from_string("FreeMono 12"));
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_show(label);
      gtk_grid_attach(GTK_GRID(grid),label,0,i,3,1);

      GtkWidget *start_button=gtk_button_new_with_label("Start");
      gtk_widget_override_font(start_button, pango_font_description_from_string("FreeMono 18"));
      gtk_widget_show(start_button);
      gtk_grid_attach(GTK_GRID(grid),start_button,3,i,1,1);
      g_signal_connect(start_button,"button_press_event",G_CALLBACK(start_cb),(gpointer)d);

      // if not available then cannot start it
      if(d->status!=STATE_AVAILABLE) {
        gtk_button_set_label(GTK_BUTTON(start_button),"In Use");
        gtk_widget_set_sensitive(start_button, FALSE);
      }

      // if not on the same subnet then cannot start it
      if((d->info.network.interface_address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr) != (d->info.network.address.sin_addr.s_addr&d->info.network.interface_netmask.sin_addr.s_addr)) {
        gtk_button_set_label(GTK_BUTTON(start_button),"Subnet!");
        gtk_widget_set_sensitive(start_button, FALSE);
      }

#ifdef STEMLAB_DISCOVERY
      if (d->protocol == STEMLAB_PROTOCOL) {
        if (d->software_version == 0) {
          gtk_button_set_label(GTK_BUTTON(start_button), "Not installed");
          gtk_widget_set_sensitive(start_button, FALSE);
        } else {
          apps_combobox[i] = gtk_combo_box_text_new();
          gtk_widget_override_font(apps_combobox[i],
              pango_font_description_from_string("FreeMono 12"));
          // We want the default selection priority for the STEMlab app to be
          // RP-Trx > HAMlab-Trx > Pavel-Trx > Pavel-Rx, so we add in decreasing order and
          // always set the newly added entry to be active.
          if ((d->software_version & STEMLAB_PAVEL_RX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[i]),
                "sdr_receiver_hpsdr", "Pavel-Rx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[i]),
                "sdr_receiver_hpsdr");
          }
          if ((d->software_version & STEMLAB_PAVEL_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[i]),
                "sdr_transceiver_hpsdr", "Pavel-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[i]),
                "sdr_transceiver_hpsdr");
          }
          if ((d->software_version & HAMLAB_RP_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[i]),
                "hamlab_sdr_transceiver_hpsdr", "HAMlab-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[i]),
                "hamlab_sdr_transceiver_hpsdr");
          }
          if ((d->software_version & STEMLAB_RP_TRX) != 0) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(apps_combobox[i]),
                "stemlab_sdr_transceiver_hpsdr", "STEMlab-Trx");
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(apps_combobox[i]),
                "stemlab_sdr_transceiver_hpsdr");
          }
          gtk_widget_show(apps_combobox[i]);
          gtk_grid_attach(GTK_GRID(grid), apps_combobox[i], 4, i, 1, 1);
        }
      }
#endif

    }
#ifdef GPIO
    GtkWidget *gpio_b=gtk_button_new_with_label("Config GPIO");
    g_signal_connect (gpio_b, "button-press-event", G_CALLBACK(gpio_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),gpio_b,0,i,1,1);
#endif

    GtkWidget *discover_b=gtk_button_new_with_label("Discover");
    g_signal_connect (discover_b, "button-press-event", G_CALLBACK(discover_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),discover_b,1,i,1,1);

    GtkWidget *exit_b=gtk_button_new_with_label("Exit");
    g_signal_connect (exit_b, "button-press-event", G_CALLBACK(exit_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),exit_b,2,i,1,1);

    i++;
    GtkWidget *tcp_b=gtk_button_new_with_label("Use new TCP Addr:");
    g_signal_connect (tcp_b, "button-press-event", G_CALLBACK(tcp_cb), NULL);
    gtk_grid_attach(GTK_GRID(grid),tcp_b,1,i,1,1);

    tcpaddr=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(tcpaddr), 20);
    gtk_grid_attach(GTK_GRID(grid),tcpaddr,2,i,1,1);
    gtk_entry_set_text(GTK_ENTRY(tcpaddr), ipaddr_tcp);

    gtk_container_add (GTK_CONTAINER (content), grid);
    gtk_widget_show_all(discovery_dialog);
fprintf(stderr,"showing device dialog\n");
  }

}



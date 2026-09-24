// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Tap Zap tray helper (tapzap-tray) — a StatusNotifierItem via libayatana-appindicator that
// gives the menu-bar-style presence the macOS app has. It runs as its own process (GTK main
// loop) and controls the running GUI over the IPC socket (ipc.hpp). Built only when GTK3 +
// ayatana-appindicator3 are present (build.sh detects them); the GUI launches it on startup.
#include "ipc.hpp"
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

using namespace tapzap;

static void onShow(GtkMenuItem*, gpointer) { ipcSend("show"); }
static void onZap(GtkMenuItem*, gpointer) { ipcSend("toggle"); }
static void onQuit(GtkMenuItem*, gpointer) { ipcSend("quit"); gtk_main_quit(); }

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    AppIndicator* ind = app_indicator_new(
        "tapzap", "preferences-desktop-display", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(ind, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(ind, "Tap Zap");

    GtkWidget* menu = gtk_menu_new();
    struct { const char* label; GCallback cb; } items[] = {
        {"Show Tap Zap", G_CALLBACK(onShow)},
        {"ZAP (toggle filter)", G_CALLBACK(onZap)},
    };
    for (auto& it : items) {
        GtkWidget* mi = gtk_menu_item_new_with_label(it.label);
        g_signal_connect(mi, "activate", it.cb, nullptr);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget* mQuit = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(mQuit, "activate", G_CALLBACK(onQuit), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mQuit);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(ind, GTK_MENU(menu));

    gtk_main();
    return 0;
}

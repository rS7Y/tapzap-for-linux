// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Test compositor: exercises the real client protocol without touching a display.
#include <wayland-server.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "ctm-server.h"

static struct wl_resource* owner;
static double pending[9];
static const char* state_path;
static bool always_blocked;
static unsigned commits;
static void state(bool active) {
    char path[4096];snprintf(path,sizeof path,"%s.tmp",state_path);
    FILE* f=fopen(path,"w");if(!f)abort();
    fprintf(f,"{\"active\":%s,\"commits\":%u,\"matrix\":[",active?"true":"false",commits);
    for(int i=0;i<9;i++)fprintf(f,"%s%.8f",i?",":"",active?pending[i]:(i%4==0?1.:0.));
    fputs("]}\n",f);fclose(f);rename(path,state_path);
}
static void destroyed(struct wl_resource* r){if(r==owner){owner=NULL;state(false);}}
static void destroy(struct wl_client* c,struct wl_resource* r){(void)c;wl_resource_destroy(r);}
static void matrix(struct wl_client* c,struct wl_resource* r,struct wl_resource* out,
    wl_fixed_t a,wl_fixed_t b,wl_fixed_t d,wl_fixed_t e,wl_fixed_t f,wl_fixed_t g,wl_fixed_t h,wl_fixed_t i,wl_fixed_t j){
    (void)c;(void)out;if(r!=owner)return;
    const wl_fixed_t values[]={a,b,d,e,f,g,h,i,j};
    for(int n=0;n<9;n++)pending[n]=wl_fixed_to_double(values[n]);
}
static void commit(struct wl_client* c,struct wl_resource* r){(void)c;if(r==owner){commits++;state(true);}}
static const struct hyprland_ctm_control_manager_v1_interface implementation={matrix,commit,destroy};
static void bind_ctm(struct wl_client* client,void* data,uint32_t version,uint32_t id){
    (void)data;struct wl_resource* r=wl_resource_create(client,&hyprland_ctm_control_manager_v1_interface,version,id);
    wl_resource_set_implementation(r,&implementation,NULL,destroyed);
    if(owner||always_blocked)hyprland_ctm_control_manager_v1_send_blocked(r);else owner=r;
}
static const struct wl_output_interface output_implementation={destroy};
static void bind_output(struct wl_client* client,void* data,uint32_t version,uint32_t id){
    (void)data;struct wl_resource* r=wl_resource_create(client,&wl_output_interface,version,id);
    wl_resource_set_implementation(r,&output_implementation,NULL,NULL);
    wl_output_send_geometry(r,0,0,300,200,WL_OUTPUT_SUBPIXEL_UNKNOWN,"TEST","Virtual output",WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(r,WL_OUTPUT_MODE_CURRENT|WL_OUTPUT_MODE_PREFERRED,1280,1024,60000);
    wl_output_send_scale(r,getenv("TEST_OUTPUT_SCALE")?atoi(getenv("TEST_OUTPUT_SCALE")):1);wl_output_send_name(r,"TEST-1");wl_output_send_description(r,"Protocol test output");wl_output_send_done(r);
}
int main(int argc,char** argv){
    if(argc<3)return 2;
    state_path=argv[2];always_blocked=argc>3;
    struct wl_display* display=wl_display_create();
    wl_global_create(display,&wl_output_interface,4,NULL,bind_output);
    wl_global_create(display,&hyprland_ctm_control_manager_v1_interface,2,NULL,bind_ctm);
    if(wl_display_add_socket(display,argv[1]))return 1;
    state(false);wl_display_run(display);wl_display_destroy_clients(display);wl_display_destroy(display);return 0;
}

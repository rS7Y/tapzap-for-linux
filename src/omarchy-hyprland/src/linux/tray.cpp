// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// A small StatusNotifierItem for Waybar. Uses GLib/GIO, without a GTK UI.
#include "ipc.hpp"
#include <gio/gio.h>
#include <cstdio>
#include <string>

static GDBusConnection* bus;
static GMainLoop* loop;
static bool enabled=false;
static const char* itemPath="/StatusNotifierItem";
static const char* itemInterface="org.kde.StatusNotifierItem";
static const char* xml=R"XML(<node>
<interface name="org.kde.StatusNotifierItem">
 <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
 <property name="Category" type="s" access="read"/><property name="Id" type="s" access="read"/>
 <property name="Title" type="s" access="read"/><property name="Status" type="s" access="read"/>
 <property name="WindowId" type="i" access="read"/><property name="IconName" type="s" access="read"/>
 <property name="IconPixmap" type="a(iiay)" access="read"/><property name="IconThemePath" type="s" access="read"/>
 <property name="AttentionIconName" type="s" access="read"/><property name="AttentionIconPixmap" type="a(iiay)" access="read"/>
 <property name="OverlayIconName" type="s" access="read"/><property name="OverlayIconPixmap" type="a(iiay)" access="read"/>
 <property name="AttentionMovieName" type="s" access="read"/>
 <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
 <property name="ItemIsMenu" type="b" access="read"/><property name="Menu" type="o" access="read"/>
 <signal name="NewIcon"/><signal name="NewToolTip"/>
</interface>
<interface name="com.canonical.dbusmenu">
 <method name="GetLayout"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="as" direction="in"/><arg type="u" direction="out"/><arg type="(ia{sv}av)" direction="out"/></method>
 <method name="GetGroupProperties"><arg type="ai" direction="in"/><arg type="as" direction="in"/><arg type="a(ia{sv})" direction="out"/></method>
 <method name="Event"><arg type="i" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="in"/><arg type="u" direction="in"/></method>
 <method name="EventGroup"><arg type="a(isvu)" direction="in"/><arg type="ai" direction="out"/></method>
 <method name="AboutToShow"><arg type="i" direction="in"/><arg type="b" direction="out"/></method>
 <property name="Version" type="u" access="read"/><property name="TextDirection" type="s" access="read"/>
 <property name="Status" type="s" access="read"/><property name="IconThemePath" type="as" access="read"/>
</interface></node>)XML";

static GVariant* pixmaps(bool draw=true) {
    GVariantBuilder list;g_variant_builder_init(&list,G_VARIANT_TYPE("a(iiay)"));
    if(draw){
        unsigned char bytes[32*32*4]={};
        const double xs[]={10,3,7,5,13,9},ys[]={1,9,9,15,6,6};
        for(int y=0;y<32;y++)for(int x=0;x<32;x++){
            double px=(x+.5)/2,py=(y+.5)/2;bool inside=false;
            for(int i=0,j=5;i<6;j=i++)
                if((ys[i]>py)!=(ys[j]>py) && px<(xs[j]-xs[i])*(py-ys[i])/(ys[j]-ys[i])+xs[i])inside=!inside;
            if(inside){int p=(y*32+x)*4;bytes[p]=255;bytes[p+1]=255;bytes[p+2]=enabled?0:255;bytes[p+3]=enabled?0:255;}
        }
        GVariant* pixels=g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,bytes,sizeof bytes,1);
        g_variant_builder_add(&list,"(ii@ay)",32,32,pixels);
    }
    return g_variant_builder_end(&list);
}
static GVariant* menuProperties(int id){
    GVariantBuilder p;g_variant_builder_init(&p,G_VARIANT_TYPE("a{sv}"));
    if(id==0)g_variant_builder_add(&p,"{sv}","children-display",g_variant_new_string("submenu"));
    else{
        const char* labels[]={"","Show TAP ZAP","Toggle filter","Quit TAP ZAP"};
        g_variant_builder_add(&p,"{sv}","label",g_variant_new_string(labels[id]));
        g_variant_builder_add(&p,"{sv}","enabled",g_variant_new_boolean(true));
        g_variant_builder_add(&p,"{sv}","visible",g_variant_new_boolean(true));
    }
    return g_variant_builder_end(&p);
}
static GVariant* menuLayout(int id,int depth){
    GVariantBuilder children;g_variant_builder_init(&children,G_VARIANT_TYPE("av"));
    if(id==0 && depth!=0)for(int n=1;n<=3;n++)g_variant_builder_add(&children,"v",menuLayout(n,0));
    return g_variant_new("(i@a{sv}@av)",id,menuProperties(id),g_variant_builder_end(&children));
}
static void action(int id){
    if(id==1)tapzap::ipcSend("show");
    else if(id==2)tapzap::ipcSend("toggle");
    else if(id==3){tapzap::ipcSend("quit");g_main_loop_quit(loop);}
}
static void call(GDBusConnection*,const gchar*,const gchar*,const gchar* iface,const gchar* method,
                 GVariant* args,GDBusMethodInvocation* invocation,gpointer){
    if(std::string(iface)==itemInterface){
        if(std::string(method)=="Activate")tapzap::ipcSend("show");
        else if(std::string(method)=="SecondaryActivate")tapzap::ipcSend("toggle");
        else if(std::string(method)=="ContextMenu")tapzap::ipcSend("show");
        g_dbus_method_invocation_return_value(invocation,nullptr);return;
    }
    std::string name=method;
    if(name=="GetLayout"){
        gint32 parent,depth;g_variant_get_child(args,0,"i",&parent);g_variant_get_child(args,1,"i",&depth);
        if(parent<0 || parent>3){g_dbus_method_invocation_return_dbus_error(invocation,"com.canonical.dbusmenu.Error.InvalidMenu","Unknown item");return;}
        g_dbus_method_invocation_return_value(invocation,g_variant_new("(u@(ia{sv}av))",1u,menuLayout(parent,depth)));
    }else if(name=="GetGroupProperties"){
        GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a(ia{sv})"));
        GVariant* ids=g_variant_get_child_value(args,0);GVariantIter it;g_variant_iter_init(&it,ids);gint32 id;
        if(g_variant_n_children(ids)==0)for(int i=0;i<=3;i++)g_variant_builder_add(&values,"(i@a{sv})",i,menuProperties(i));
        while(g_variant_iter_next(&it,"i",&id))if(id>=0&&id<=3)g_variant_builder_add(&values,"(i@a{sv})",id,menuProperties(id));
        g_variant_unref(ids);g_dbus_method_invocation_return_value(invocation,g_variant_new("(@a(ia{sv}))",g_variant_builder_end(&values)));
    }else if(name=="AboutToShow")g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",false));
    else if(name=="Event"){
        gint32 id;const gchar* event;g_variant_get_child(args,0,"i",&id);g_variant_get_child(args,1,"&s",&event);
        if(std::string(event)=="clicked")action(id);
        g_dbus_method_invocation_return_value(invocation,nullptr);
    }else if(name=="EventGroup"){
        GVariant* events=g_variant_get_child_value(args,0);
        for(gsize i=0;i<g_variant_n_children(events);i++){
            GVariant* e=g_variant_get_child_value(events,i);gint32 id;const gchar* event;
            g_variant_get_child(e,0,"i",&id);g_variant_get_child(e,1,"&s",&event);
            if(std::string(event)=="clicked")action(id);
            g_variant_unref(e);
        }
        g_variant_unref(events);GVariantBuilder errors;g_variant_builder_init(&errors,G_VARIANT_TYPE("ai"));
        g_dbus_method_invocation_return_value(invocation,g_variant_new("(@ai)",g_variant_builder_end(&errors)));
    }else g_dbus_method_invocation_return_dbus_error(invocation,"org.freedesktop.DBus.Error.UnknownMethod","Unknown method");
}
static GVariant* property(GDBusConnection*,const gchar*,const gchar*,const gchar* iface,const gchar* key,GError**,gpointer){
    const std::string k=key;
    if(std::string(iface)=="com.canonical.dbusmenu"){
        if(k=="Version")return g_variant_new_uint32(3);
        if(k=="TextDirection")return g_variant_new_string("ltr");
        if(k=="Status")return g_variant_new_string("normal");
        if(k=="IconThemePath")return g_variant_new_strv(nullptr,0);
    }
    if(k=="Category")return g_variant_new_string("Hardware");
    if(k=="Id")return g_variant_new_string("tapzap-omarchy");
    if(k=="Title")return g_variant_new_string("TAP ZAP");
    if(k=="Status")return g_variant_new_string("Active");
    if(k=="WindowId")return g_variant_new_int32(0);
    if(k=="ItemIsMenu")return g_variant_new_boolean(false);
    if(k=="Menu")return g_variant_new_object_path("/MenuBar");
    if(k=="IconPixmap")return pixmaps();
    if(k=="AttentionIconPixmap" || k=="OverlayIconPixmap")return pixmaps(false);
    if(k=="ToolTip")return g_variant_new("(s@a(iiay)ss)","",pixmaps(false),"TAP ZAP",enabled?"Filter ON — click for controls":"Filter OFF — click for controls");
    return g_variant_new_string("");
}
static void appeared(GDBusConnection* connection,const gchar*,const gchar*,gpointer){
    g_dbus_connection_call(connection,"org.kde.StatusNotifierWatcher","/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher","RegisterStatusNotifierItem",g_variant_new("(s)",itemPath),
        nullptr,G_DBUS_CALL_FLAGS_NONE,2000,nullptr,nullptr,nullptr);
}
static gboolean update(gpointer){
    std::string reply;
    if(tapzap::ipcQuery("status",reply,150)){
        bool next=reply.find("\"enabled\":true")!=std::string::npos;
        if(next!=enabled){enabled=next;
            g_dbus_connection_emit_signal(bus,nullptr,itemPath,itemInterface,"NewIcon",nullptr,nullptr);
            g_dbus_connection_emit_signal(bus,nullptr,itemPath,itemInterface,"NewToolTip",nullptr,nullptr);
        }
    }
    return G_SOURCE_CONTINUE;
}
int main(){
    GError* error=nullptr;bus=g_bus_get_sync(G_BUS_TYPE_SESSION,nullptr,&error);
    if(!bus){std::fprintf(stderr,"Tray unavailable: %s\n",error->message);g_error_free(error);return 1;}
    GDBusNodeInfo* node=g_dbus_node_info_new_for_xml(xml,&error);
    if(!node){std::fprintf(stderr,"Tray interface error: %s\n",error->message);return 1;}
    const GDBusInterfaceVTable table={call,property,nullptr,{nullptr}};
    if(!g_dbus_connection_register_object(bus,itemPath,node->interfaces[0],&table,nullptr,nullptr,&error) ||
       !g_dbus_connection_register_object(bus,"/MenuBar",node->interfaces[1],&table,nullptr,nullptr,&error)){
        std::fprintf(stderr,"Tray registration error: %s\n",error->message);return 1;
    }
    g_bus_watch_name_on_connection(bus,"org.kde.StatusNotifierWatcher",G_BUS_NAME_WATCHER_FLAGS_NONE,appeared,nullptr,nullptr,nullptr);
    loop=g_main_loop_new(nullptr,false);g_timeout_add(1000,update,nullptr);
    g_main_loop_run(loop);g_main_loop_unref(loop);g_dbus_node_info_unref(node);g_object_unref(bus);return 0;
}

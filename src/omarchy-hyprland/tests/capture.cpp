// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Capture only the trial's own XWayland window, never the desktop.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
static Window find(Display* d,Window root){
    XClassHint hint{};
    if(XGetClassHint(d,root,&hint)){
        bool match=hint.res_class && !std::strcmp(hint.res_class,"TapZapOmarchy");
        if(hint.res_name)XFree(hint.res_name);if(hint.res_class)XFree(hint.res_class);
        if(match)return root;
    }
    Window top,parent,*children=nullptr;unsigned n=0;
    if(!XQueryTree(d,root,&top,&parent,&children,&n))return 0;
    Window result=0;for(unsigned i=0;i<n&&!result;i++)result=find(d,children[i]);
    if(children)XFree(children);return result;
}
int main(int argc,char** argv){
    if(argc!=2)return 2;Display* d=XOpenDisplay(nullptr);if(!d)return 3;
    Window win=find(d,DefaultRootWindow(d));if(!win)return 4;
    XWindowAttributes a{};XGetWindowAttributes(d,win,&a);
    XImage* image=XGetImage(d,win,0,0,a.width,a.height,AllPlanes,ZPixmap);if(!image)return 5;
    FILE* f=fopen(argv[1],"wb");if(!f)return 6;fprintf(f,"P6\n%d %d\n255\n",a.width,a.height);
    for(int y=0;y<a.height;y++)for(int x=0;x<a.width;x++){
        unsigned long pixel=XGetPixel(image,x,y);
        for(unsigned long mask:{image->red_mask,image->green_mask,image->blue_mask}){
            unsigned long value=pixel&mask;while(mask && !(mask&1)){mask>>=1;value>>=1;}
            fputc(mask?static_cast<int>(value*255/mask):0,f);
        }
    }
    fclose(f);XDestroyImage(image);XCloseDisplay(d);return 0;
}

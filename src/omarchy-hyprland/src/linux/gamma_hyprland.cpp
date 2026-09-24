// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamma_backend.hpp"
#include "ipc.hpp"
#include <cmath>
#include <cstdio>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>

namespace tapzap {
class HyprlandBackend final : public IGammaBackend {
    int socket_=-1;
    pid_t child_=-1;
    int outputs_=0;
    bool active_=false;
    double scale_=1;
    void disconnect() {
        if(socket_>=0){close(socket_);socket_=-1;}
        // Closing the owning Wayland connection releases its CTM, even if the
        // helper is blocked in a compositor roundtrip.
        if(child_>0){
            kill(child_,SIGKILL);
            while(waitpid(child_,nullptr,0)<0 && errno==EINTR){}
            child_=-1;
        }
        active_=false;
    }
    bool receive(std::string& err) {
        std::string response;
        if(!readLine(socket_,response,2000)) {
            err="Hyprland filter connection timed out or closed";disconnect();return false;
        }
        if(response.rfind("OK ",0)!=0){err=response;return false;}
        const auto at=response.find("outputs=");
        if(at!=std::string::npos)outputs_=std::atoi(response.c_str()+at+8);
        const auto scaleAt=response.find("scale=");
        if(scaleAt!=std::string::npos)scale_=clampd(std::atof(response.c_str()+scaleAt+6),1,4);
        active_=response.find("enabled=1")!=std::string::npos;
        return true;
    }
    bool request(const std::string& line,std::string& err){
        if(socket_<0){err="Restart TAP ZAP to reconnect to Hyprland";return false;}
        if(!sendLine(socket_,line)){err="Hyprland filter helper unavailable";disconnect();return false;}
        return receive(err);
    }
public:
    ~HyprlandBackend() override {disconnect();}
    bool init(std::string& err) override {
        const char* helper=std::getenv("TAPZAP_HYPRLAND_HELPER");
        if(!helper || *helper!='/' || access(helper,X_OK)) {err="Hyprland filter helper missing";return false;}
        int pair[2];
        if(socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,pair)){err=std::strerror(errno);return false;}
        const pid_t parent=getpid();
        child_=fork();
        if(child_==0){
            // A killed control window must not leave its filter helper alive.
            prctl(PR_SET_PDEATHSIG,SIGKILL);
            if(getppid()!=parent)_exit(1);
            dup2(pair[1],STDIN_FILENO);dup2(pair[1],STDOUT_FILENO);
            close(pair[0]);close(pair[1]);
            execl(helper,helper,"--allow-extreme-dim",static_cast<char*>(nullptr));
            _exit(127);
        }
        close(pair[1]);
        if(child_<0){close(pair[0]);err="Cannot start Hyprland filter helper";return false;}
        socket_=pair[0];
        if(!receive(err)){disconnect();return false;}
        return true;
    }
    bool setFilter(double intensity,double brightness,std::string& err) override {
        if(!std::isfinite(intensity)||!std::isfinite(brightness)||intensity<0||intensity>1||brightness<0||brightness>1){
            err="Invalid filter values";return false;
        }
        char line[128];std::snprintf(line,sizeof line,"set %.17g %.17g",intensity,brightness);
        if(!request(line,err))return false;
        return active_ || request("on",err);
    }
    bool restore(std::string& err) override {
        if(socket_<0)return true; // Child already killed; compositor owns cleanup.
        return request("off",err);
    }
    bool maintainFilter(double,double,std::string& err) override {
        if(!request("status",err))return false;
        if(!active_ || outputs_==0){err="No active Hyprland filter output";return false;}
        return true;
    }
    std::string name() const override{return "hyprland-ctm-v2";}
    int outputCount() const override{return outputs_;}
    double displayScale() const override{return scale_;}
};
std::unique_ptr<IGammaBackend> makeDefaultBackend(){return std::make_unique<HyprlandBackend>();}
}

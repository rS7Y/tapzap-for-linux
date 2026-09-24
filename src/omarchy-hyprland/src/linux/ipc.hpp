// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace tapzap {
inline std::string ipcDirectory() {
    const char* base = std::getenv("XDG_RUNTIME_DIR");
    if (!base || *base != '/') return {};
    struct stat s{};
    if (lstat(base, &s) || !S_ISDIR(s.st_mode) || s.st_uid != getuid()) return {};
    const std::string p = std::string(base) + "/tapzap-omarchy";
    if (mkdir(p.c_str(), 0700) && errno != EEXIST) return {};
    if (lstat(p.c_str(), &s) || !S_ISDIR(s.st_mode) || s.st_uid != getuid() || (s.st_mode & 077)) return {};
    return p;
}
inline std::string ipcSocketPath() {
    const auto dir = ipcDirectory();
    return dir.empty() ? std::string() : dir + "/control.sock";
}
inline bool socketAddress(sockaddr_un& addr) {
    const auto p = ipcSocketPath();
    if (p.empty() || p.size() >= sizeof(addr.sun_path)) return false;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, p.c_str(), p.size()+1);
    return true;
}
inline bool readyFd(int fd, short events, std::chrono::steady_clock::time_point deadline) {
    while (true) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if (ms <= 0) return false;
        pollfd p{fd, events, 0};
        int n = poll(&p, 1, static_cast<int>(ms));
        if (n < 0 && errno == EINTR) continue;
        return n > 0 && (p.revents & events);
    }
}
inline bool sendLine(int fd, const std::string& text, int timeout=200) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    const auto line=text+"\n";
    size_t offset=0;
    while(offset<line.size()) {
        if(!readyFd(fd,POLLOUT,deadline)) return false;
        ssize_t n=send(fd,line.data()+offset,line.size()-offset,MSG_NOSIGNAL|MSG_DONTWAIT);
        if(n<0 && (errno==EINTR || errno==EAGAIN)) continue;
        if(n<=0) return false;
        offset+=static_cast<size_t>(n);
    }
    return true;
}
inline bool readLine(int fd, std::string& text, int timeout, size_t limit=4096) {
    text.clear();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    while(text.size()<limit) {
        if(!readyFd(fd,POLLIN,deadline)) return false;
        char c;
        ssize_t n=recv(fd,&c,1,MSG_DONTWAIT);
        if(n<0 && (errno==EINTR || errno==EAGAIN)) continue;
        if(n!=1) return false;
        if(c=='\n') return true;
        if(c!='\r') text+=c;
    }
    return false;
}
inline bool ipcQuery(const std::string& cmd, std::string& reply, int timeout=5500) {
    sockaddr_un addr{};
    if(!socketAddress(addr)) return false;
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if(fd<0) return false;
    bool ok=connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof addr)==0;
    if(ok) ok=sendLine(fd,cmd) && readLine(fd,reply,timeout);
    close(fd);
    return ok;
}
inline bool ipcSend(const std::string& cmd) {
    std::string reply;
    return ipcQuery(cmd,reply) && reply.find("\"ok\":true")!=std::string::npos;
}
class InstanceLock {
    int fd_=-1;
public:
    bool acquire() {
        const auto dir=ipcDirectory();
        if(dir.empty()) return false;
        fd_=open((dir+"/instance.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
        return fd_>=0 && flock(fd_,LOCK_EX|LOCK_NB)==0;
    }
    ~InstanceLock(){ if(fd_>=0)close(fd_); }
};
inline int ipcListen() {
    sockaddr_un addr{};
    if(!socketAddress(addr))return -1;
    const auto p=ipcSocketPath();
    unlink(p.c_str()); // Only the holder of InstanceLock calls this.
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    if(fd<0)return -1;
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof addr) || listen(fd,8)) {close(fd);return -1;}
    chmod(p.c_str(),0600);
    return fd;
}
struct IpcRequest { int fd=-1; std::string command; };
inline IpcRequest ipcAccept(int listener) {
    IpcRequest r;
    r.fd=accept4(listener,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK);
    if(r.fd>=0 && !readLine(r.fd,r.command,150,128)){close(r.fd);r.fd=-1;}
    return r;
}
inline void ipcReply(IpcRequest& request,const std::string& reply) {
    if(request.fd>=0){sendLine(request.fd,reply);close(request.fd);request.fd=-1;}
}
inline void ipcClose(int fd){if(fd>=0){close(fd);unlink(ipcSocketPath().c_str());}}
inline std::string jsonString(const std::string& value) {
    std::string out="\"";
    for(unsigned char c:value){
        if(c=='"' || c=='\\'){out+='\\';out+=c;}
        else if(c=='\n')out+="\\n";
        else if(c>=32)out+=c;
    }
    return out+'"';
}
}

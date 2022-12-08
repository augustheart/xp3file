#include "cachedfile.h"

CachedFileHandle::~CachedFileHandle(){
    Reset();
}
bool CachedFileHandle::OpenFile(const std::filesystem::path& path,HANDLE& handle){
    auto itr =m_Cache.find(path);
    if(itr != m_Cache.end()){
        if(itr->second != INVALID_HANDLE_VALUE){
            handle = itr->second;
            return true;
        }
    }
    HANDLE hdl = CreateFileW(path.wstring().c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hdl == INVALID_HANDLE_VALUE){
        fmt::printf("creat file %s fail! error: %d\n",path.string().c_str(),GetLastError());
        return false;
    }
    m_Cache[path] = hdl;
    handle = hdl;
    return true;
}
HANDLE CachedFileHandle::OpenFile(const std::filesystem::path& path){
    HANDLE hdl = INVALID_HANDLE_VALUE;
    OpenFile(path,hdl);
    return hdl;
}
void CachedFileHandle::CloseFile(const std::filesystem::path& path){
    auto itr = m_Cache.find(path);
    if(itr != m_Cache.end()){
        HANDLE h = itr->second;
        m_Cache.erase(itr);
        CloseHandle(h);
    }
}
void CachedFileHandle::Reset(){
    if(m_Cache.size() == 0){
        return;
    }
    for(auto& node : m_Cache){
        if(node.second != INVALID_HANDLE_VALUE){
            HANDLE h = node.second;
            CloseHandle(h);
        }  
    }
    m_Cache.clear();
}
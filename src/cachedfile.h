#pragma once
#ifndef __CACHEFILE_H
#define __CACHEFILE_H
#include "stdafx.h"

class CachedFileHandle{
private:
    std::map<std::filesystem::path,HANDLE> m_Cache;
public:
    ~CachedFileHandle();
    bool OpenFile(const std::filesystem::path& path,HANDLE& handle);
    HANDLE OpenFile(const std::filesystem::path& path);
    void CloseFile(const std::filesystem::path& path);
    void Reset();
};

#endif

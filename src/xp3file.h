#ifndef __XP3FILE_H
#define __XP3FILE_H
#include "stdafx.h"
#include "cachedfile.h"
/*
1.xp3文件中的路径以x:/4545/4545.xp3|ppafe/werer/xxxx的形式存放
2.文件是否压缩以扩展名形式判断
3.将外部文件导入到特定的路径中
*/
class XP3File{
private:
    struct FileIndexInfo{
        bool UseCompress;
        std::wstring Source, Dest;
        uint64_t Size;
        uint64_t ComSize;
        uint64_t Offset;
        static bool sourceIsInFile(const std::wstring& path,std::wstring& file,std::wstring& inner);
        bool save(HANDLE handle,const std::filesystem::path& targetdir);
        bool read(HANDLE handle,void* dest,uint64_t& size);
        size_t indexSize()const;
        bool makeIndex(void* dest,size_t size,uint64_t offset,uint64_t comsize,uint64_t unsize,uint32_t adlr);
        bool isInFile()const;
    };
    std::vector< std::shared_ptr<FileIndexInfo>> m_Nodes;
    std::shared_ptr<CachedFileHandle> m_HandleCache;
    bool parse(void* buffer, size_t length);
    bool parseIndex(void* buffer,size_t length);
    bool parseFileInfo(void* buffer,size_t length);
    uint64_t calculateIndexSize()const;
    bool read(uint64_t index,void* dest,uint64_t& size)const;
    static uint32_t adler32(void* buffer,size_t length);
    std::shared_ptr<XP3File::FileIndexInfo> node(size_t index)const;
    static HANDLE CreateFileCached(const std::filesystem::path& path,std::map<std::filesystem::path,HANDLE>& cache);
public:
    XP3File();
    bool parse(const std::filesystem::path& file);
    bool dump(const std::filesystem::path& targetdir)const;//must parse first,no parse action in this function
    bool save(const std::filesystem::path& target, int compressLevel);
    bool addfile(const std::wstring& file,std::wstring inner, bool compress);
    uint32_t Count()const;
    std::wstring Source(size_t index)const;
    std::wstring Inner(size_t index)const;
    uint64_t Size(size_t index)const;
    bool Read(size_t index,void* dest,size_t cap)const;
};
#endif

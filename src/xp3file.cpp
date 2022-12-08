#include "xp3file.h"

using WC = char16_t;
enum XP3NodeData{
    INVALID_TYPE,
    FILE_HEADER,
    INFO_SEGMENT,
    SEGM_SEGMENT,
    ADLR_SEGMENT
};
#pragma push
#pragma pack(4)
struct XP3Header{
    uint8_t Identifier[4];
    uint32_t Unknown1,Unknown2,Unknown3,Unknown4,Unknown5,Unknown6,Unknown7;
    uint64_t IndexBegin;
};
struct Compressed{
    uint64_t ComSize,UncompSize;
};
struct FileSegment{
    uint8_t Signature[4];//0x46,0x69,0x6c,0x65
    uint64_t DataSize;
};
#pragma push
#pragma pack(2)
struct InfoNode : public FileSegment{///0x69,06e,0x66,0x6f
    uint32_t Unknown1;//0x80000000
    uint64_t Value1;
    uint64_t ComSize;
    uint16_t StrSize;
    WC Path[];
};
#pragma pop
struct SegmNode : public FileSegment{//0x73,0x65,0x67,0x6d
    uint32_t Unknow1;//1
    uint64_t Offset;
    uint64_t UncomSize;
    uint64_t ComSize;
};
struct AdlrNode : public FileSegment{//0x61,0x64,0x6c,0x72
    uint32_t CRC;//?
};
#pragma pop
XP3NodeData GetXp3IndexNodeType(const FileSegment& seg){
    const uint32_t tou32 = *reinterpret_cast<const uint32_t*>(seg.Signature);
    switch(tou32){
        case 0x656c6946:
            return FILE_HEADER;
        case 0x6f666e69:
            return INFO_SEGMENT;
        case 0x6d676573:
            return SEGM_SEGMENT;
        case 0x726c6461:
            return ADLR_SEGMENT;
        default:
        return INVALID_TYPE;
    }
}
bool CharConv(const std::string_view& s, int codepage,std::wstring& out){
    int len = s.length();
    if(len == 0){
        out.clear();
        return true;
    }
    const char* ptr = s.data();
    int req = MultiByteToWideChar(codepage,0,ptr,len,0,0);
    if(req < 0){
        return false;
    }
    out.resize(req);
    return 0 < MultiByteToWideChar(codepage,0,ptr,len,(LPWSTR)out.data(),req);
}
bool WriteFileTo(const std::filesystem::path& p,void* src,size_t size){
    std::filesystem::path obj = std::filesystem::absolute(p);
    std::filesystem::path dir = obj.parent_path();
    if(!std::filesystem::exists(dir)){
        std::filesystem::create_directories(dir);
    }
    if(obj.wstring().length() > MAX_PATH){
        std::wstring tmp = L"\\\\?\\";
        tmp += obj.wstring();
        obj = tmp;
    }
    HANDLE handle = CreateFileW(obj.wstring().c_str(),GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle == INVALID_HANDLE_VALUE){
        fmt::printf("create file %s fail!,err:%d\n",obj.string().c_str(),GetLastError());
        return false;
    }
    fmt::printf("write file %s from %p,size %x\n",p.string(),src,size);
    WriteFile(handle,src,size,NULL,NULL);
    CloseHandle(handle);
    return true;
}
XP3File::XP3File():m_HandleCache(std::make_shared<CachedFileHandle>()){}
uint32_t XP3File::adler32(void* buffer,size_t length){
    const uint32_t SEED = 65521;
    uint32_t flag1(1),flag2(0);
    uint8_t* data = reinterpret_cast<uint8_t*>(buffer);
    for (size_t idx = 0; idx < length; ++idx)
    {
        flag1 = (flag1 + data[idx]) % SEED;
        flag2 = (flag2 + flag1) % SEED;
    }
    return (flag2 << 16) | flag1;
}
bool XP3File::parse(const std::filesystem::path& file){
    HANDLE handle = CreateFileW(file.wstring().c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle == INVALID_HANDLE_VALUE){
        fmt::printf("open file fail!error: %d\n",GetLastError());
        return false;
    }
    bool result = false;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(handle,&size)){
        fmt::printf("get file size fail,error: %d\n",GetLastError());
    }
    fmt::printf("file size: %lld\n",size.QuadPart);
    HANDLE mapping = CreateFileMapping(handle,NULL,PAGE_READONLY,size.HighPart,size.LowPart,NULL);
    if(mapping){
        void* ptr = MapViewOfFile(mapping,FILE_MAP_READ,0,0,0);
        if(ptr){
            result = parse(ptr,size.QuadPart);
            UnmapViewOfFile(ptr);
            if(result){
                for(auto& node : m_Nodes){
                    node->Source = file.wstring();
                    node->Source += L"|";
                    node->Source += node->Dest;
                }
            }
        }else{
            int err = GetLastError();
            fmt::printf("mapping file fail,error: %d\n",err);
        }
    }else{
        int err = GetLastError();
        fmt::printf("create file mapping fail,error: %d\n",err);
    }
    if(mapping != NULL){
        CloseHandle(mapping);
    }
    if(handle != INVALID_HANDLE_VALUE){
        CloseHandle(handle);
    }
    return result;
}

bool XP3File::parse(void* buffer, size_t length){
    if(!buffer || length < sizeof(XP3Header)){
        fmt::printf("file size too small.1\n");
        return false;
    }
    uint8_t* ptr = reinterpret_cast<uint8_t*>(buffer);
    XP3Header* header = reinterpret_cast<XP3Header*>(ptr);
    uint32_t indexoffset = header->IndexBegin;
    if(length < indexoffset + 1){
        fmt::printf("file size too small.2\n");
        return false;
    }
    uint8_t count = *reinterpret_cast<uint8_t*>(ptr + indexoffset);
    if(count > 0){
        if(length < indexoffset + 1 + sizeof(Compressed) * count){
            fmt::printf("file size too small.3\n");
            return false;
        }
        for(int n(0); n < count; ++n){
            Compressed* comm = reinterpret_cast<Compressed*>(ptr + indexoffset + 1 + n * sizeof(Compressed));
            uint64_t comsize = comm->ComSize;
            uint64_t unsize = comm->UncompSize;
            uint8_t* comdata = ptr + indexoffset + 1 + sizeof(Compressed) * count + n * sizeof(Compressed);
            if(length < indexoffset + 1 + sizeof(Compressed) * count + comsize){
                fmt::printf("file size too small.4\n");
                return false;
            }
            std::unique_ptr<uint8_t> unbuff(new uint8_t[unsize]);
            if(!unbuff){
                fmt::printf("alloc decompress buffer fail\n");
                return false;
            }
            //int ZEXPORT uncompress OF((Bytef *dest,   uLongf *destLen,const Bytef *source, uLong sourceLen));
            if(Z_OK != uncompress(unbuff.get(),(uLongf*)&unsize,comdata,comsize)){
                fmt::printf("decompress fail\n");
                return false;
            }
            #ifndef NDEBUG
            if(!WriteFileTo("index.data",unbuff.get(),unsize)){
                fmt::printf("write temparary fail\n");
                return false;
            }
            #endif
            return parseIndex(unbuff.get(),unsize);
        }
    }
    return true;
}
bool XP3File::parseIndex(void* buffer,size_t length){
    uint8_t* base = reinterpret_cast<uint8_t*>(buffer);
    uint8_t* ptr = base;
    if(!buffer){
        return false;
    }
    if(length == 0){
        return true;
    }
    while(ptr < base + length){
        FileSegment* seg = reinterpret_cast<FileSegment*>(ptr);
        XP3NodeData type = GetXp3IndexNodeType(*seg);
        if(type != FILE_HEADER){
            fmt::printf("invalid type at offset: %x\n",ptr - base);
            return false;
        }
        uint64_t childsize = seg->DataSize;
        uint8_t* child = ptr + sizeof(FileSegment);
        if(!parseFileInfo(child,childsize)){
            fmt::printf("parse file info fail at offset: %x\n",child -base);
            return false;
        }
        ptr = child + childsize;
    }
    return true;
}
bool XP3File::parseFileInfo(void* buffer,size_t length){
    const uint8_t* base = reinterpret_cast<const uint8_t*>(buffer);
    const uint8_t* ptr = base;
    if(!buffer){
        return false;
    }
    const InfoNode* info = nullptr;
    const SegmNode* segm = nullptr;
    const AdlrNode* adlr = nullptr;
    while(ptr < base + length){
        const FileSegment* seg = reinterpret_cast<const FileSegment*>(ptr);
        XP3NodeData type = GetXp3IndexNodeType(*seg);
        switch(type){
            case INFO_SEGMENT:{
                if(info!=nullptr)
                    return false;
                info = reinterpret_cast<const InfoNode*>(seg);
                break;
            }
            case SEGM_SEGMENT:{
                if(segm != nullptr)
                    return false;
                segm = reinterpret_cast<const SegmNode*>(seg);
                break;
            }
            case ADLR_SEGMENT:{
                if(adlr != nullptr)
                    return false;
                adlr = reinterpret_cast<const AdlrNode*>(seg);
                break;
            }
            default:
                return false;
        }
        ptr += seg->DataSize + sizeof(FileSegment);
    }
    if(!info || !segm || !adlr){
        return false;
    }
    std::shared_ptr<FileIndexInfo> fileinfo = std::shared_ptr<FileIndexInfo>(new FileIndexInfo);
    if(!fileinfo){
        return false;
    }
    std::u16string s16 = std::u16string(info->Path,info->StrSize);
    std::wstring path(s16.begin(),s16.end());
    fileinfo->Dest = path;
    fileinfo->Size = segm->UncomSize;
    fileinfo->ComSize = segm->ComSize;
    fileinfo->Offset = segm->Offset;
    m_Nodes.push_back(fileinfo);
    return true;
}

bool XP3File::dump(const std::filesystem::path& targetdir)const{
    for(auto& node : m_Nodes){
        if(node->Dest.length() > MAX_PATH){
            fmt::printf(L"skip too long file %s\n",node->Dest);
            continue;
        }
        std::wstring packfile,innerpath;
        HANDLE handle = INVALID_HANDLE_VALUE;
        if(FileIndexInfo::sourceIsInFile(node->Source,packfile,innerpath)){
            handle = m_HandleCache->OpenFile(packfile);
        }else{
            handle = m_HandleCache->OpenFile(node->Source);
        }
        if(handle == INVALID_HANDLE_VALUE){
            fmt::printf("open package file fail!\n");
            return false;
        }
        std::filesystem::path outpath = targetdir;
        outpath.append(node->Dest);
        if(!node->save(handle,outpath)){
            fmt::printf("save node fail!\n");
            return false;
        }
    }
    return true;
}
uint64_t XP3File::calculateIndexSize()const{
    uint64_t ret = 0;
    for(auto& node : m_Nodes){
        ret += node->indexSize();
    }
    return ret;
}
bool XP3File::FileIndexInfo::sourceIsInFile(const std::wstring& path,std::wstring& file,std::wstring& innerpath){
    size_t pos = path.find('|');
    if(pos == std::wstring::npos){
        file = path;
        return false;
    }
    file = path.substr(0,pos);
    innerpath = path.substr(pos + 1);
    return true;
}
bool XP3File::FileIndexInfo::read(HANDLE handle,void* dest,uint64_t& size){
    if(!dest || size < Size){
        return false;
    }
    LARGE_INTEGER move;
    move.QuadPart = Offset;
    if(!SetFilePointerEx(handle,move,NULL,FILE_BEGIN)){
        fmt::printf("move file cursor fail,error: %d\n",GetLastError());
        return false;
    }
    if(isInFile()){
        if(Size != ComSize){
            std::unique_ptr<uint8_t> combuffer(new uint8_t[ComSize]);
            if(!ReadFile(handle,combuffer.get(),ComSize,NULL,NULL)){
                fmt::printf("read fail fail! error: %d\n",GetLastError());
                return false;
            }
            uint64_t comsize = ComSize;
            //fmt::printf("uncom: %x,%x,%x\n",node.Offset,comsize,uncomsize);
            if(Z_OK != uncompress((Bytef*)dest,(uLongf*)&Size,combuffer.get(),comsize)){
                fmt::printf("decompress fail\n");
                return false;
            }
        }else{
            if(!ReadFile(handle,dest,ComSize,NULL,NULL)){
                fmt::printf("read fail fail! error: %d\n",GetLastError());
                return false;
            }
        }
    }else{
        if(!ReadFile(handle,dest,Size,NULL,NULL)){
            fmt::printf("read external fail fail! error: %d\n",GetLastError());
            return false;
        }
    }
    return true;
}
bool XP3File::FileIndexInfo::save(HANDLE handle,const std::filesystem::path& target){
    std::unique_ptr<uint8_t> uncombuffer(new uint8_t[Size]);
    if(!uncombuffer){
        fmt::printf("alloc decompress buffer fail!\n");
        return false;
    }
    if(!read(handle,uncombuffer.get(),Size)){
        fmt::printf(L"read file %s fail!\n",Source.c_str());
        return false;
    }
    return WriteFileTo(target,uncombuffer.get(),Size);
}
size_t XP3File::FileIndexInfo::indexSize()const{
    return sizeof(FileSegment) + sizeof(InfoNode) + sizeof(SegmNode) + sizeof(AdlrNode) + Dest.length() * sizeof(char16_t);
}
bool XP3File::FileIndexInfo::isInFile()const{
    std::wstring t1,t2;
    return FileIndexInfo::sourceIsInFile(Source,t1,t2);
}
bool XP3File::FileIndexInfo::makeIndex(void* dest,size_t size,uint64_t offset, uint64_t comsize,uint64_t unsize,uint32_t adlrcrc){
    if(!dest){
        return false;
    }
    size_t req = indexSize();
    if(size < req){
        return false;
    }
    uint8_t* ptr = reinterpret_cast<uint8_t*>(dest);
    FileSegment* file = reinterpret_cast<FileSegment*>(ptr);
    InfoNode* info = reinterpret_cast<InfoNode*>(ptr + sizeof(FileSegment));
    SegmNode* segm = reinterpret_cast<SegmNode*>(reinterpret_cast<uint8_t*>(info) + sizeof(InfoNode) + Dest.length() * sizeof(char16_t));
    AdlrNode* adlr = reinterpret_cast<AdlrNode*>(reinterpret_cast<uint8_t*>(segm) + sizeof(SegmNode));
    file->Signature[0] = 0x46;
    file->Signature[1] = 0x69;
    file->Signature[2] = 0x6c;
    file->Signature[3] = 0x65;
    file->DataSize = req - sizeof(FileSegment);

    info->Signature[0] = 0x69;
    info->Signature[1] = 0x6e;
    info->Signature[2] = 0x66;
    info->Signature[3] = 0x6f;
    info->DataSize = sizeof(InfoNode) + Dest.length() * sizeof(char16_t) - sizeof(FileSegment);
    info->Unknown1 = 0x80000000;
    info->Value1 =  unsize;
    info->ComSize = comsize;
    info->StrSize = Dest.length();
    std::u16string s(Dest.begin(),Dest.end());
    for(size_t n(0); n < s.length(); ++n){
        info->Path[n] = s[n];
        if(info->Path[n] == '\\'){
            info->Path[n] = '/';
        }
    }

    segm->Signature[0] = 0x73;
    segm->Signature[1] = 0x65;
    segm->Signature[2] = 0x67;
    segm->Signature[3] = 0x6d;
    segm->DataSize = sizeof(SegmNode) - sizeof(FileSegment);
    segm->Unknow1 = UseCompress ? 1 : 0;
    segm->Offset = offset;
    segm->UncomSize = unsize;
    segm->ComSize = comsize;

    adlr->Signature[0] = 0x61;
    adlr->Signature[1] = 0x64;
    adlr->Signature[2] = 0x6c;
    adlr->Signature[3] = 0x72;
    adlr->DataSize = sizeof(AdlrNode) - sizeof(FileSegment);
    adlr->CRC = adlrcrc;
    return true;
}
bool XP3File::save(const std::filesystem::path& target, int compressLevel){
    uint64_t indexSize = calculateIndexSize();
    if(indexSize > 0){
        fmt::printf("index size: %u\n",indexSize);
        std::unique_ptr<uint8_t> buffer(new uint8_t[indexSize]);
        if(!buffer){
            return false;
        }
        fmt::printf("alloc index compress buffer\n");
        uLong needed = compressBound(indexSize);
        std::unique_ptr<uint8_t> combuffer(new uint8_t[needed]);
        if(!combuffer){
            return false;
        }
        uint8_t* ptr = buffer.get();
        uint8_t* end = ptr + indexSize;
        uint8_t* comptr = combuffer.get();
        std::map<std::filesystem::path,HANDLE> fileHandles;
        fmt::printf("create output file\n");
        HANDLE h = CreateFileW(target.wstring().c_str(),GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        if(h == INVALID_HANDLE_VALUE){
            fmt::printf("create output file %s fail,error:%d\n",target.string().c_str(),GetLastError());
            return false;
        }

        XP3Header header;
        header.Identifier[0] = 0x58;
        header.Identifier[1] = 0x50;
        header.Identifier[2] = 0x33;
        header.Identifier[3] = 0xd;
        header.Unknown1 = 0x1a0a200a;
        header.Unknown2 = 0x1701678b;
        header.Unknown3 = 0;
        header.Unknown4 = 0x1000000;
        header.Unknown5 = 0x80000000;
        header.Unknown6 = 0;
        header.Unknown7 = 0;
        WriteFile(h,&header,sizeof(header),NULL,NULL);
        for(auto& node : m_Nodes){
            std::unique_ptr<uint8_t> buffer(new uint8_t[node->Size]);
            if(!buffer){
                fmt::printf("alloc file buffer fail!\n");
                return false;
            }
            HANDLE child = INVALID_HANDLE_VALUE;
            std::wstring file,inner;
            FileIndexInfo::sourceIsInFile(node->Source,file,inner);
            child = m_HandleCache->OpenFile(file);
            if(child == INVALID_HANDLE_VALUE){
                fmt::printf(L"open file %s fail!\n",file);
                return false;
            }
            if(!node->read(child,buffer.get(),node->Size)){
                fmt::printf("read source fail!\n");
                return false;
            }
            m_HandleCache->CloseFile(node->Source);
            uint32_t adlr32 = XP3File::adler32(buffer.get(),node->Size);
            LARGE_INTEGER mov,cur;
            mov.QuadPart = 0;
            cur.QuadPart = 0;
            if(!SetFilePointerEx(h,mov,&cur,FILE_CURRENT)){
                fmt::printf("get binary offset fail,error: %d\n",GetLastError());
                return false;
            }
            if(node->UseCompress){
                fmt::printf("make compress\n");
                uLong needed = compressBound(node->Size);
                std::unique_ptr<uint8_t> combuffer(new uint8_t[needed]);
                if(!combuffer){
                    fmt::printf("alloc compress buffer fail!\n");
                    return false;
                }
                int zresult = compress2((Bytef*)combuffer.get(),(uLongf*)&needed,buffer.get(),node->Size,compressLevel);
                if(Z_OK != zresult){
                    fmt::printf("compress node fail\n");
                    return false;
                }
                node->ComSize = needed;
                WriteFile(h,combuffer.get(),needed,NULL,NULL);
            }else{
                fmt::printf("make raw\n");
                node->ComSize = node->Size;
                WriteFile(h,buffer.get(),node->Size,NULL,NULL);
            }
            node->Offset = cur.QuadPart;
            if(!node->makeIndex(ptr,end-ptr,node->Offset,node->ComSize,node->Size,adlr32)){
                fmt::printf("make index fail!\n");
                return false;
            }
            std::filesystem::path tp(file);
            fmt::printf("write index at %x,%s,offset:%x,com:%x,size:%x\n",(size_t)ptr,tp.string(),node->Offset,node->ComSize,node->Size);
            //system("pause");
            ptr += node->indexSize();
        }
        LARGE_INTEGER mov,cur;
        mov.QuadPart = 0;
        if(!SetFilePointerEx(h,mov,&cur,FILE_CURRENT)){
            fmt::printf("get index offset fail,error: %d\n",GetLastError());
            return false;
        }
        header.IndexBegin = cur.QuadPart;
        uint8_t num = 1;
        WriteFile(h,&num,sizeof(num),NULL,NULL);
        int zresult = compress2((Bytef*)comptr,(uLongf*)&needed,(Bytef*)buffer.get(),indexSize,compressLevel);
        if(Z_OK != zresult){
            fmt::printf("compress index fail!,error: %d\n",zresult);
            CloseHandle(h);
            return false;
        }
        Compressed indexCompress;
        indexCompress.UncompSize = indexSize;
        indexCompress.ComSize = needed;
        fmt::printf("write index at %x,size %x/%x\n",cur.QuadPart,indexCompress.UncompSize,indexCompress.ComSize);
        WriteFile(h,&indexCompress,sizeof(indexCompress),NULL,NULL);
        WriteFile(h,comptr,needed,NULL,NULL);
        SetFilePointerEx(h,mov,NULL,FILE_BEGIN);
        fmt::printf("write header\n");
        WriteFile(h,&header,sizeof(header),NULL,NULL);
        CloseHandle(h);
    }
    return true;
}
bool XP3File::read(uint64_t index,void* dest,uint64_t& size)const{
    if(index >= m_Nodes.size()){
        fmt::printf("index %ulld exceed!\n",index);
        return false;
    }
    auto& info = m_Nodes[index];
    if(!info){
        fmt::printf("invalid node at index %ulld\n",index);
        return false;
    }
    std::wstring xp3,inner;
    if(FileIndexInfo::sourceIsInFile(info->Source,xp3,inner)){//姑且认为这种情况下的信息都是正确的，否则需要再引入xp3的解析过程
        HANDLE h = m_HandleCache->OpenFile(xp3);
        if(h == INVALID_HANDLE_VALUE){
            fmt::printf(L"open file %s fail!\n",xp3.c_str());
            return false;
        }
        if(size < info->Size){
            fmt::printf(L"node %s require %ulld memory,given %ulld\n",info->Size,size);
            return false;
        }
        return info->read(h,dest,size);
    }else{//这种外部引入文件的情况下可以认为是独立的文件，可以用实际的文件数据返回大小
        HANDLE h = m_HandleCache->OpenFile(info->Source);
        if(h == INVALID_HANDLE_VALUE){
            fmt::printf(L"open file %s fail!\n",info->Source.c_str());
            return false;
        }
        LARGE_INTEGER insize;
        if(!GetFileSizeEx(h,&insize)){
            fmt::printf(L"get file %s size fail,error: %d",info->Source.c_str(),GetLastError());
            return false;
        }
        if(insize.QuadPart > size){
            fmt::printf(L"file %s require %ulld memory,given %ulld\n",info->Source.c_str(),insize.QuadPart,size);
            return false;
        }
        SetFilePointer(h,0,NULL,FILE_BEGIN);
        if(!ReadFile(h,dest,insize.QuadPart,NULL,NULL)){
            fmt::printf(L"read file %s fail!, error: %d\n",info->Source.c_str(),GetLastError());
            return false;
        }
        size = insize.QuadPart;
        return true;
    }
}
bool XP3File::addfile(const std::wstring& file,std::wstring inner, bool compress){
    /*
    struct FileIndexInfo{
        bool UseCompress;
        std::wstring Source, Dest;
        uint64_t Size;
        uint64_t ComSize;
        uint64_t Offset;
    */
    std::shared_ptr<FileIndexInfo> node(new FileIndexInfo);
    if(!node){
        fmt::printf("alloc index node fail!\n");
        return false;
    }
    if(!std::filesystem::is_regular_file(file)){
        fmt::printf(L"file %s not exist!\n",file);
        return false;
    }
    node->Offset = 0;
    node->UseCompress = compress;
    node->Source = file;
    node->Dest = inner;
    node->Size = std::filesystem::file_size(file);
    m_Nodes.push_back(node);
    return true;
}
uint32_t XP3File::Count()const{
    return m_Nodes.size();
}
std::wstring XP3File::Source(size_t index)const{
    auto n = node(index);
    if(n){
        return n->Source;
    }
    return std::wstring();
}
std::wstring XP3File::Inner(size_t index)const{
    auto n = node(index);
    if(n){
        return n->Dest;
    }
    return std::wstring();
}
std::shared_ptr<XP3File::FileIndexInfo> XP3File::node(size_t index)const{
    if(index < Count()){
        return m_Nodes[index];
    }
    return std::shared_ptr<FileIndexInfo>();
}
uint64_t XP3File::Size(size_t index)const{

}
bool XP3File::Read(size_t index,void* dest,size_t cap)const{
    
}
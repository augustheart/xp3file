#include "stdafx.h"
#include "xp3file.h"

int main(int argc,char** argv){
    CLI::App app{"XP3 file unpacker and packer"};
    CLI::App* unpacker = app.add_subcommand("unpack","unpack XP3 file");
    if(!unpacker){
        fmt::printf("initialize unpacker fail!");
        return -1;
    }
    std::string input,output;
    unpacker->add_option("-o",output,"output dir")->required();
    unpacker->add_option("-i",input,"input file")->required();
    CLI::App* packer = app.add_subcommand("pack","repack XP3 file");
    if(!packer){
        fmt::printf("initialize packer fail!");
        return -1;
    }
    packer->add_option("-o",output,"output file")->required();
    packer->add_option("-i",input,"input dir")->required();
    unpacker->callback([&input,&output](){
        fmt::printf("unpacker: input:%s,output:%s\n",input,output);
        std::filesystem::path infile(input);
        if(!std::filesystem::is_regular_file(infile)){
            fmt::printf("no input file: %s\n",infile.string().c_str());
            return;
        }
        XP3File xp3file;
        if(!xp3file.parse(infile)){
            fmt::printf("parse file fail!\n");
        }else{
            fmt::printf("parse file success!\n");
            fmt::printf("unpacka file %s!\n",xp3file.dump(output) ? "success" : "fail");
        }
    });
    packer->callback([&input,&output](){
        fmt::printf("packer: input:%s,output:%s\n",input,output);
        std::filesystem::path indir(input);
        if(!std::filesystem::is_directory(indir)){
            fmt::printf("no input directiry %s\n",indir.string().c_str());
            return;
        }
        XP3File xp3file;
        std::set<std::string> uncompressext = {".png",".ogg",".tft"};
        for(auto& f : std::filesystem::recursive_directory_iterator{indir}){
            if(std::filesystem::is_regular_file(f)){
                std::filesystem::path infile(f);
                std::filesystem::path innername = std::filesystem::relative(infile,indir);
                std::string ext = infile.extension().string();
                bool usecompress = uncompressext.find(ext) == uncompressext.end();
                xp3file.addfile(infile.wstring(),innername.wstring(),usecompress);
            }
        }
        xp3file.save(output,9);
    });
    //CLI::CallForHelp();
    CLI11_PARSE(app, argc, argv);
    return 0;
}
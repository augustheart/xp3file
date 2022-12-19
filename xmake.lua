set_policy("build.optimization.lto",true)
package("cli11")
    set_urls("https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.3.1.tar.gz")
    add_versions("v2.3.1","378DA73D2D1D9A7B82AD6ED2B5BDA3E7BC7093C4034A1D680A2E009EB067E7B2")
    add_configs("headonly",{description = "make a single headonly", default = false, type = "boolean"})
    on_install(function(package)
        os.mkdir("build")
        os.cd("build")
        local cfg = {"-DCLI11_BUILD_DOCS=OFF","-DCLI11_BUILD_TESTS=OFF","-DCLI11_BUILD_EXAMPLES=OFF","-G'Ninja'"}
        local headonly = package:config("headonly")
        if(headonly)then
            table.insert(cfg,"-DCLI11_SINGLE_FILE=ON")
        else
            table.insert(cfg,"-DCLI11_PRECOMPILED=ON")
        end
        table.insert(cfg,"../")
        os.vrunv("cmake",cfg)
        os.vrun("ninja")
		if(headonly)then
            os.cp("include/CLI11.hpp",package:installdir("include"))
        else
            os.cp("../include/CLI",package:installdir("include"))
            os.cp("src/libCLI11.a",package:installdir("lib"))
        end
    end)
package_end()
package("fmt")
    set_urls("https://github.com/fmtlib/fmt/releases/download/$(version)/fmt-$(version).zip")
    add_versions("9.1.0","CCEB4CB9366E18A5742128CB3524CE5F50E88B476F1E54737A47FFDF4DF4C996")
    add_defines("FMT_HEADER_ONLY")
    on_install(function(package)
        os.cp("include/fmt",package:installdir("include"))
    end)
package_end()
add_requires("zlib","cli11","fmt")
target("xp3file")
    set_languages("c99", "cxx20")
    set_kind("binary")
    add_packages("zlib","cli11","fmt")
    add_files("src/*.cpp")
    add_ldflags("-static")
    if(is_mode("release"))then
        add_ldflags("-s") 
    end
target_end()
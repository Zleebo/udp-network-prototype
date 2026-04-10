include "../../Premake/extensions.lua"

workspace "Game"
    location "../../"
    startproject "Game"
    architecture "x64"

    configurations {
        "Debug",
        "Release",
        "Retail"
    }

    -- include for common stuff 
    include "../../Premake/common.lua"

    include (dirs.external)
    include (dirs.engine)
    include (dirs.shared)
    include (dirs.server)  -- Assuming this includes the Server.lua Premake script

-------------------------------------------------------------
project "Game"
    location (dirs.projectfiles)
    dependson { "External", "Engine", "Shared", "Server" }  -- Make sure "Server" is listed as a dependency
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"

    debugdir "%{dirs.bin}"
    targetdir ("%{dirs.bin}")
    targetname("%{prj.name}_%{cfg.buildcfg}")
    objdir ("%{dirs.temp}/%{prj.name}/%{cfg.buildcfg}")

    links {"External", "Engine", "Shared", "Server"}  -- Include "Server" in the links directive

    includedirs { dirs.external, dirs.engine, dirs.server, dirs.shared }

    files {
        "source/**.h",
        "source/**.cpp",
    }

    libdirs { dirs.lib, dirs.dependencies }

    verify_or_create_settings("Game")

    filter "configurations:Debug"
        defines {"_DEBUG"}
        runtime "Debug"
        symbols "on"
        files {"tools/**"}
        includedirs {"tools/"}
    filter "configurations:Release"
        defines "_RELEASE"
        runtime "Release"
        optimize "on"
        files {"tools/**"}
        includedirs {"tools/"}
    filter "configurations:Retail"
        defines "_RETAIL"
        runtime "Release"
        optimize "on"

    filter "system:windows"
        staticruntime "off"
        symbols "On"
        systemversion "latest"
        warnings "Off"
        flags { 
            "FatalCompileWarnings",
            "MultiProcessorCompile",
		
        }

        defines {
            "WIN32",
            "_LIB", 
            "TGE_SYSTEM_WINDOWS" 
        }
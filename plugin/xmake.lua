-- include subprojects
includes("lib/commonlibsse-ng")

-- set project constants
set_project("NeedsWidget")
set_version("2.5.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules. mode.release because the build uses -m release: without it that
-- mode had no rule, so no optimising and no NDEBUG, and CommonLib's asserts crashed
-- players' games (gaodaxia1985, 2.3.0).
add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- define targets
target("NeedsWidget")
    add_rules("commonlibsse-ng.plugin", {
        name = "NeedsWidget",
        author = "Tom",
        description = "Needs Widget AIO - every needs mod's needs as icons on the HUD"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/SKSEMenuFramework")
    set_pcxxheader("src/pch.h")
    -- The SKSE Menu Framework header calls the wide Windows API through the generic macros.
    add_defines("UNICODE", "_UNICODE", "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")

-- premake5.lua
workspace "ase2json"
   configurations { "Debug", "Release" }
 
project "ase2json"
   kind "ConsoleApp"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
 
   files { "**.cpp" }
   
   cppdialect "C++11"
   
   if os.istarget( "Linux" ) then
      toolset "gcc"
      links { "curl" }
   end
 
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
 
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

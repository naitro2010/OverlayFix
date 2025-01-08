# ***Template Project for SKSE - CommonLibSSE-NG***

## ***Runtime requirements***

- [Skyrim Script Extender (SKSE)](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## ***Build requirements***

- [CMake](https://cmake.org/)
- [vcpkg](https://vcpkg.io/en/)
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/)
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG)

#### ***CommonLibSSE-NG***

To use CommonLibSSE-NG as a git-submodule instead of overlay-ports, clone it to extern/CommonLibSSE-NG and edit vcpkg.json removing "commonlibsse-ng" and adding its dependencies.

## ***Building***

In `Developer Command Prompt for VS 2022` or `Developer PowerShell for VS 2022`, run:

~~~
git clone https://github.com/epinter/skse-clibng-template.git
cd skse-clibng-template
~~~

then

~~~
.\cmake\build.ps1
~~~

or

~~~
.\cmake\build.ps1 -buildPreset relwithdebinfo
~~~

or

~~~
.\cmake\build.ps1 -buildPreset debug
~~~

or

~~~
cmake -B build -S . --preset default --fresh
cmake --build build --preset release
~~~

Then get the .dll in build/Release, or the .zip (ready to install using mod manager) in build.

## ***Clean up the template***

This template contains some examples that can be removed if not used:

- Boost dependencies in CMakeLists.txt and vcpkg.json
- Sample files inside dist/ directory (used to by cpack to generate .zip)
- GameEventHandler.cpp, GameEventHandler.h and SkseMessagingListener.h.
- .clang-tidy and .clang-format
- Log directory method in Plugin.cpp

## ***File local.cmake***

CMake will use a file named local.cmake (project root), in this file you can add something like:

~~~
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND xcopy /y /-I "bin\\$<CONFIG>\\${PROJECT_NAME}.dll" "C:\\games\\Skyrim\\Data\\SKSE\\Plugins\\${PROJECT_NAME}.dll"
    COMMAND xcopy /y /-I "bin\\$<CONFIG>\\${PROJECT_NAME}.pdb" "C:\\games\\Skyrim\\Data\\SKSE\\Plugins\\${PROJECT_NAME}.pdb"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "C:\\games\\Skyrim\\skse64_loader.exe" WORKING_DIRECTORY "C:\\games\\Skyrim"
)
~~~
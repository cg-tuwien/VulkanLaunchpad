# Vulkan Launchpad :rocket:

A framework by TU Wien targeted at Vulkan beginners. It abstracts some of the hard and overly verbose parts of the Vulkan C API and can help to boost your learning progress early on. 

## Setup Instructions

Vulkan Launchpad runs on Windows, MacOS, and Linux. For building you'll need [Git](https://git-scm.com/), the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/), a C++ compiler, [CMake](https://cmake.org/), and optimally an integrated development environment (IDE). In the following, we describe setup instructions for common operating systems and editors/IDEs (click the links in the table of contents to jump to the sections that are relevant to your chosen setup):
- [Operating Systems](#operating-systems)
    - [Windows](#windows)
    - [macOS](#macos)
    - [Linux](#linux)
        - [Dependencies](#dependencies)
        - [Ubuntu 22.04](#ubuntu-2204)
        - [Ubuntu 20.04](#ubuntu-2004)
        - [Linux Mint 21.1](#linux-mint-211)
        - [Debian Bullseye](#debian-bullseye)
        - [Automatic Git Clone and Build via Commandline](#automatic-git-clone-and-build-via-commandline)
- [Editors and IDEs](#editors-and-ides)
    - [Visual Studio Code (VS Code)](#visual-studio-code-vs-code)
    - [Visual Studio 2022 Community](#visual-studio-2022-community)

## Operating Systems

### Windows
- Download and install [Git for Windows](https://git-scm.com/download/win)!
    - Add Git to your PATH! This can be done through the installer, selecting the `Git from the command line and also from 3rd-party software` option. 
- Download and install one of the latest [Vulkan SDKs for Windows](https://vulkan.lunarg.com/sdk/home#windows)! (At time of writing, the most recent version is 1.3.236.0.)
    - _Note:_ It is not required to install any optional components, if you make only x64 builds.
- Download and install the Microsoft Visual C++ compiler (MSVC) by installing the [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/?q=build+tools) or a newer version!
    - Select the `Desktop development with C++` workload in the installer!
    - _Note:_ Should you decide to install Visual Studio Community 2022 (as described below), you don't have to install the Build Tools for Visual Studio 2022 separately. Also in this case of using the Visual Studio Community 2022 installer, ensure to select the `Desktop development with C++` workload! 
- Download and install CMake through its [Windows x64 Installer](https://cmake.org/download/)!
    - Select an option to `Add CMake to the system PATH ...` during installation!
    - _Important:_ Ensure to get CMake version `3.22` or newer!

### macOS

TODO

### Linux

#### Dependencies

We tested the compilation on the four different Linux distributions below.
Each of them has slightly different requirements, however the lines of bash code boil down to:

1. Installs necessary apt packages to be able to download and install and maybe also build further dependencies.
2. Adds required repositories to the list of package sources (`/etc/apt/sources.list`) via the command `apt-add-repository` or by directly downloading them to `/etc/apt/sources.list.d/`, to be able to install CMake and Vulkan and graphics drivers on Linux. `ppa:ubuntu-toolchain-r` contains gcc.
3. Updates the apt cache with the new sources, upgrade existing packages to the latest stable versions and install all necessary build-tools and drivers as well as the Vulkan SDK and CMake. This step also installs project library dependencies like glfw, which are usually installed system-wide on Linux distros.
4. Updates the default gcc and g++ version to the freshly installed version 11.
5. Invokes `vulkaninfo` to see if the Vulkan SDK and graphics drivers are installed correctly.

##### Ubuntu 22.04

```
#!/bin/bash
set -e -o pipefail

sudo apt update && sudo apt upgrade -y && sudo apt install -y wget gpg lsb-release software-properties-common

wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo rm /etc/apt/trusted.gpg.d/kitware.gpg && sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4
sudo apt-add-repository -y "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo add-apt-repository -y ppa:oibaf/graphics-drivers
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | tee /etc/apt/trusted.gpg.d/lunarg.asc
wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list

sudo apt update && sudo apt upgrade -y && sudo apt install -y  g++ gdb make ninja-build rsync zip kitware-archive-keyring cmake libassimp-dev g++-11 libvulkan-dev libvulkan1 mesa-vulkan-drivers vulkan-tools vulkan-sdk dpkg-dev libvulkan1-dbgsym vulkan-tools-dbgsym libglfw3 libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
sudo apt clean all

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 60 --slave /usr/bin/g++ g++ /usr/bin/g++-11

echo ""
echo "Now running \"vulkaninfo\" to see if vulkan has been installed successfully:"
vulkaninfo
```

##### Ubuntu 20.04

_Note:_ Only tested in a Docker environment.

```
sudo apt update && sudo apt upgrade -y && sudo apt install -y wget gpg git lsb-release software-properties-common && \
	wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
	sudo rm /etc/apt/trusted.gpg.d/kitware.gpg && sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4 && \
	sudo apt-add-repository -y "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" && \
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
	sudo add-apt-repository -y ppa:oibaf/graphics-drivers && \
	wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | tee /etc/apt/trusted.gpg.d/lunarg.asc && \
	wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list http://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list && \
	sudo apt update && sudo apt upgrade -y && sudo apt install -y g++ gdb make ninja-build rsync zip kitware-archive-keyring cmake libassimp-dev g++-11 libvulkan-dev libvulkan1 mesa-vulkan-drivers vulkan-tools vulkan-sdk dpkg-dev libvulkan1-dbgsym vulkan-tools-dbgsym libglfw3 libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev && \
	sudo apt clean all && \
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 60 --slave /usr/bin/g++ g++ /usr/bin/g++-11
```

##### Linux Mint 21.1

_Note:_ Only tested in a Docker environment.

```
sudo apt update && sudo apt upgrade -y && sudo apt install -y mint-dev-tools build-essential devscripts fakeroot quilt dh-make automake libdistro-info-perl less nano ubuntu-dev-tools python3 \
		wget git gpg lsb-release software-properties-common && \
	wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
	sudo rm /etc/apt/trusted.gpg.d/kitware.gpg && sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4 && \
	sudo apt-add-repository -y "deb https://apt.kitware.com/ubuntu/ jammy main" && \
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
	sudo add-apt-repository -y ppa:oibaf/graphics-drivers && \
	wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | tee /etc/apt/trusted.gpg.d/lunarg.asc && \
	wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list && \
	sudo apt update && sudo apt upgrade -y && sudo apt install -y  g++ gdb make ninja-build rsync zip kitware-archive-keyring cmake libassimp-dev g++-11 libvulkan-dev libvulkan1 mesa-vulkan-drivers vulkan-tools vulkan-sdk dpkg-dev libvulkan1-dbgsym vulkan-tools-dbgsym libglfw3 libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev && \
	sudo apt clean all && \
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 60 --slave /usr/bin/g++ g++ /usr/bin/g++-11
```

##### Debian Bullseye

_Note:_ Only tested in a Docker environment.

```
sudo apt update && sudo apt upgrade -y && sudo apt install -y wget sudo gpg git lsb-release software-properties-common build-essential checkinstall zlib1g-dev libssl-dev g++ gdb make ninja-build rsync zip bison libx11-xcb-dev libxkbcommon-dev libwayland-dev libxrandr-dev libxcb-randr0-dev autotools-dev libxcb-keysyms1 libxcb-keysyms1-dev libxcb-ewmh-dev pkg-config libglfw3 libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev libglew-dev libxinerama-dev libxcursor-dev libxcb-cursor-dev libxi-dev && \
	wget -q https://github.com/Kitware/CMake/releases/download/v3.25.1/cmake-3.25.1.tar.gz && \
	tar -zxvf cmake-3.25.1.tar.gz && cd cmake-3.25.1 && ./bootstrap && make -j12 && make install

	# Individual Vulkan repos below:
	# 1. https://github.com/KhronosGroup/Vulkan-Loader/blob/master/BUILD.md#building-on-linux
	#apt install -y git build-essential libx11-xcb-dev libxkbcommon-dev libwayland-dev libxrandr-dev
	#cd / && git clone https://github.com/KhronosGroup/Vulkan-Loader.git && cd Vulkan-Loader && mkdir build && cd build && cmake -DUPDATE_DEPS=ON .. && make

	# 2. https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/master/BUILD.md#building-on-linux
	#apt install -y pkg-config git build-essential libx11-xcb-dev libxkbcommon-dev libwayland-dev libxrandr-dev libegl1-mesa-dev python3-distutils
	#cd / && git clone https://github.com/KhronosGroup/Vulkan-ValidationLayers.git && cd Vulkan-ValidationLayers && mkdir build && cd build && cmake -DUPDATE_DEPS=ON .. && make

	# 3. https://github.com/LunarG/gfxreconstruct/blob/dev/BUILD.md#building-for-linux
	#apt install -y git build-essential libx11-xcb-dev libxcb-keysyms1-dev libwayland-dev libxrandr-dev zlib1g-dev liblz4-dev libzstd-dev
	#git clone https://github.com/LunarG/gfxreconstruct.git && cd gfxreconstruct && git submodule update --init --recursive && mkdir build && cd build && cmake .. && make

	# This one repo includes the ones above as submodules anyways:
	# 4. https://github.com/LunarG/VulkanTools/blob/master/BUILD.md
git clone https://github.com/LunarG/VulkanTools.git && cd VulkanTools && git submodule update --init --recursive && mkdir build && ./update_external_sources.sh && cd build && python3 ../scripts/update_deps.py && cmake -C helper.cmake .. && cmake --build . --parallel
```

Build:

```
git clone https://github.com/cg-tuwien/VulkanLaunchpadStarter.git && \
	cd VulkanLaunchpadStarter && \
	git checkout initialize-repo && \
	mkdir out && cd out && \
	cmake -DVulkan_INCLUDE_DIR=/VulkanTools/build/Vulkan-Headers/build/install/include/ -DVulkan_LIBRARY=/VulkanTools/build/Vulkan-Loader/build/install/lib/libvulkan.so -G Ninja .. && \
	cmake --build . --config Debug
```

##### Automatic Git Clone and Build via Commandline

```
git clone https://github.com/cg-tuwien/VulkanLaunchpadStarter.git && \
	cd VulkanLaunchpadStarter && \
	git checkout initialize-repo && \
	mkdir out && cd out && \
	cmake -G Ninja .. && \
	cmake --build . --config Debug
```

## Editors and IDEs

### Visual Studio Code (VS Code)
- Download and install [Visual Studio Code](https://code.visualstudio.com/download)!
    - Select the option `Add "Open with Code" action to Widows Explorer directory context menu` for more convenience.
- Install the following extensions (navigate to `View -> Extensions`):
    - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) (which will also install the [CMake](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension)
    - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
    - _Recommended:_ [Shader languages support for VS Code](https://marketplace.visualstudio.com/items?itemName=slevesque.shader)
    - Restart VS Code, or execute the comand `Developer: Reload Window`!
- Open the folder containing the `CMakeLists.txt` file (the root folder of this repository)!
    - This can be accomplished through `File -> Open Folder...`, you might also get the option to `Open with Code` from a folder's context menu in Windows Explorer.
- Execute the following commands (either through `Show All Commands`, which can be activated by default via `Ctrl+Shift+P` or `Cmd+Shift+P` (macOS), or you'll also find buttons for these actions at the bottom of the VS Code window):
    - `CMake: Select a Kit` then select, e.g., `Visual Studio Build Tools 2022 Release - amd64` (if you are using Windows and have installed the `Build Tools for Visual Studio 2022`).
    - `CMake: Select Variant` and select `Debug` for a build with debug information, or `Release` for one without. 
    - The above command should also trigger CMake's configuration step. If it doesn't, execute `CMake: Configure`!
    - `CMake: Build Target`, then select `VulkanLaunchpad STATIC_LIBRARY` to build Vulkan Launchpad as a static library. Alternatively, just build everything by selecting `ALL_BUILD`.

### Visual Studio 2022 Community
- Download and install [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/), or a newer version.
    - Select the `Desktop development with C++` workload in the installer!
    - Should you encounter CMake-related problems, install one of the latest versions of CMake _after_ installing Visual Studio Community 2022 using the [Windows x64 Installer](https://cmake.org/download/).
        - Ensure to select an option to `Add CMake to the system PATH ...` during installation!
        - _Important:_ Ensure to get CMake version `3.22` or newer!
- _Recommended:_ Install the [GLSL language integration](https://marketplace.visualstudio.com/items?itemName=DanielScherzer.GLSL) extension for syntax highlighting in shader files!
    - _Hint:_ Go to `Tools -> Options -> GLSL language integration`. For Vulkan shader development, either set `Live compiling` to `False` (syntax highlighting only), or set the `External compiler executable file` to, e.g., the path to `glslangValidator.exe`!
- Open the folder containing the `CMakeLists.txt` file (the root folder of this repository)!
    - This can be accomplished through `File -> Open -> Folder...`, you might also get the option to `Open with Visual Studio` from a folder's context menu in Windows Explorer.
    - You should be able to observer in the `Output` tab that CMake generation started.
        - If not, check if the `Show output from:` combobox is set to the option `CMake`!
        - Wait a bit until you see the message `CMake generation finished.`.
    - Execute `Build -> Build All` (default shortcut: `Ctrl+Shift+B`) to build Vulkan Launchpad as a static library.

# Documentation

TODO

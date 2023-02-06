# Vulkan Launchpad :rocket:

A framework by TU Wien targeted at Vulkan beginners. It abstracts some of the hard and overly verbose parts of the Vulkan C API and can help to boost your learning progress early on. 

# Setup Instructions

Vulkan Launchpad runs on Windows, macOS, and Linux. For building you'll need [Git](https://git-scm.com/), the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/), a C++ compiler, [CMake](https://cmake.org/), and optimally an integrated development environment (IDE). In the following, we describe setup instructions for common operating systems and editors/IDEs (click the links in the table of contents to jump to the sections that are relevant to your chosen setup):           
- [Operating Systems](#operating-systems)
    - [Windows](#windows)
    - [macOS](#macos)
    - [Linux](#linux)
        - [Ubuntu and Linux Mint](#ubuntu-and-linux-mint)
        - [Fedora Workstation](#fedora-workstation)
        - [Manjaro](#manjaro)
- [Editors and IDEs](#editors-and-ides)
    - [Visual Studio Code (VS Code)](#visual-studio-code-vs-code)
    - [Visual Studio 2022 Community](#visual-studio-2022-community)
    - [Xcode](#xcode)
    - [Other IDEs](#other-ides)
- [Troubleshooting](#troubleshooting)
    - [Submodule Updates Take a Long Time](#submodule-updates-take-a-long-time)
- [Documentation](#documentation)

**Starter Template:**       
For a quick project setup of an executable that links Vulkan Launchpad, we provide a starter template at [github.com/cg-tuwien/VulkanLaunchpadStarter](https://github.com/cg-tuwien/VulkanLaunchpadStarter).

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

- Download and install [Xcode](https://apps.apple.com/us/app/xcode/id497799835) from the Mac App Store!
  - Install the `Xcode Command Line Tools` by executing `xcode-select --install` from command line. This will install `Git` on your system.
- Download and install one of the latest [Vulkan SDKs for macOS](https://vulkan.lunarg.com/sdk/home#mac)! (At time of writing, the most recent version is 1.3.236.0.)
  - _Note:_ If you are using a Mac which runs on Apple silicon, it could happen that a popup asks you to install Rosetta. Please confirm, even though we are going to use native Apple silicon libraries.
  - _Important:_ Make sure to tick the box called `System Global Installation` during installation so the Vulkan SDK can be found by the build system.
- Download and install CMake through its [macOS universal Installer](https://cmake.org/download/) or through a package manager like [Homebrew](https://formulae.brew.sh/formula/cmake)!
  - _Note:_ The official website installer will not automatically add CMake to the system PATH. If you are planning to use CMake from the command line, you need to open the CMake app, go to `Tools -> How to Install For Command Line Use` and execute one of the three instructions listed.
  - _Important:_ Ensure to get CMake version `3.22` or newer!

### Linux

Requirements: C++ Compiler, [Git](https://git-scm.com/), [CMake](https://cmake.org/), [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#linux), [X.Org](https://www.x.org/wiki/) and Vulkan compatible driver.

In case you want to use [Ninja](https://ninja-build.org/) or other development tools please install them separately. The instructions below are the minimum dependencies to build Vulkan Launchpad.

#### Ubuntu and Linux Mint
```bash
# Add LunarG public key
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc

# Add Vulkan package
# Jammy Jellyfish (Ubuntu 22.04/22.10 and Linux Mint 21.0/21.1)
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.236-jammy.list https://packages.lunarg.com/vulkan/1.3.236/lunarg-vulkan-1.3.236-jammy.list
# Focal Fossa (Ubuntu 20.04/20.10 and Linux Mint 20.0/20.1/20.2/20.3)
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-1.3.236-focal.list https://packages.lunarg.com/vulkan/1.3.236/lunarg-vulkan-1.3.236-focal.list

# Update package manager
sudo apt update
# Install dependencies
sudo apt install git cmake build-essential xorg-dev libvulkan-dev vulkan-headers vulkan-validationlayers
```

#### Fedora Workstation
```bash
sudo dnf install cmake gcc-c++ libXinerama-devel vulkan-loader-devel vulkan-headers vulkan-validation-layers-devel
sudo dnf -y groupinstall "X Software Development"
```

#### Manjaro
```bash
sudo pacman -Sy cmake base-devel vulkan-validation-layers
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
    - You should be able to observe in the `Output` tab that CMake generation started.
        - If not, check if the `Show output from:` combobox is set to the option `CMake`!
        - Wait a bit until you see the message `CMake generation finished.`.
    - Execute `Build -> Build All` (default shortcut: `Ctrl+Shift+B`) to build Vulkan Launchpad as a static library.

### Xcode
- Download and install [Xcode](https://apps.apple.com/us/app/xcode/id497799835) from the Mac App Store!
- Generate the Xcode project files:
  - Command line option:
    - Open a terminal window at the workspace root directory. This can be done by right clicking the folder and selecting `New Terminal at Folder`.
    - Option 1: Execute `make` from the terminal. This uses the included `makefile` located in the workspace root directory. Project files can be found in `_project` afterwards.
    - Option 2: Execute `cmake -H. -B_project -G "Xcode" -DCMAKE_INSTALL_PREFIX="_install"` from the terminal.
  - CMake Gui option:
    - Open the CMake Gui and specify the workspace root directory as the source directory. Specify a folder into which the generated project files should be stored. Click `Configure`, select Xcode as the Generator and press `Done`. After completion, press `Generate`.
- Open `VulkanLaunchpad.xcodeproj` with Xcode. The file should be located in the folder into which the project files were generated.

### Other IDEs
Other IDEs (such as [CLion](https://www.jetbrains.com/clion/) or [Qt Creator](https://www.qt.io/product/development-tools)) are usable too as long as they support CMake. Please consider the following remarks for the setup process:
- Make sure to set the working directory to the workspace directory.

## Troubleshooting

#### Submodule Updates Take a Long Time

In case you experience problems concerning the submodule checkout, i.e. the cloning of the submodules (GLFW, GLM or glslang) takes a long time or seems to be stuck, please try the following approach:
* Please clone the repo manually in a terminal in a new location using the following git commands:     
    ```bash
    git clone --recurse-submodules https://github.com/cg-tuwien/VulkanLaunchpad.git
    ```

# Documentation

### Structure

- `VulkanLaunchpad.h`/`VulkanLaunchpad.cpp`: Vulkan Launchpad's main funnctionality.
- `Camera.h`/`Camera.cpp`: Implementation of a ready-to-use orbit/arcball-style camera.
- `external` directory: Contains the libraries GLFW, GLM and glslang as git submodules, as well as tinyobjloader and gli.

### Naming Conventions

The following naming conventions are used by Vulkan Launchpad:
- Function names: lowerCamelCase, prefixed `vkl`
- Struct names: UpperCamelCase, prefixed `Vkl`
- Struct member variables: mCamelCase 
- Function parameters: snake_case

### Functionality

#### Initialization and Destruction

Vulkan Launchpad needs to be initialized by calling `vklInitFramework`.      
Subsequently, it needs to be destroyed via `vklDestroyFramework`. 
    
Consistent with the Vulkan API, custom configuration structs should best be zero-initialized, although they do not require to expilictly set their instance's type:

    VklSwapchainConfig swapchain_config = {};

    
#### Extensions

Required extensions can be queried by `vklGetRequiredInstanceExtensions`.       

_Note:_ GLFW will require further extensions. These are not included yet in the array that is returned by `vklGetRequiredInstanceExtensions`. 

#### Render Loop

Vulkan Launchpad provides functionality needed during a typical render loop:

```cpp
    vklWaitForNextSwapchainImage();
    vklStartRecordingCommands();
		
    // Your commands here

    vklEndRecordingCommands();
    vklPresentCurrentSwapchainImage();
```

#### Graphics Pipelines

TODO: The basic graphics pipeline created by the framework can be retrieved using `vklGetBasicPipeline`. 

TODO: To set up a custom graphics pipeline you can use `vkl(Create/Destroy)GraphicsPipeline`.

TODO: Briefly mention what `VklGraphicsPipelineConfig` is about!


#### Buffers

Vulkan Launchpad can help with buffer management in different memory types, and provides some utility functions for transfering data into a buffer. 

The memory types are reflected in function naming:
- `vklCreateHostCoherentBufferWithBackingMemory`: Creates a new `VkBuffer` and allocates backing memory in the "host coherent" memory region, which is also accessible from the host. 
- `vklCreateDeviceLocalBufferWithBackingMemory`: Creates a new `VkBuffer` and allocates memory the "device-local" memory region, which is a region that might only be accessible from the device, but not from the host (if the device supports dedicated device-local memory).

The appropriate destruction functions are the following:       
- `vklDestroyHostCoherentBufferAndItsBackingMemory` for buffers previously created with `vklCreateHostCoherentBufferWithBackingMemory`.
- `vkDestroyDeviceLocalBufferAndItsBackingMemory` for buffers previsouly created with `vklCreateDeviceLocalBufferWithBackingMemory`. 

Utility functions:      
- `vklCopyDataIntoHostCoherentBuffer`: Copy data into a host-coherent buffer.
- `vklCreateHostCoherentBufferAndUploadData`: Create a new host-coherent buffer and fill it with data.

#### Images

Vulkan Launchpad provides some utility functions for images as well:
- `vklCreateDeviceLocalImageWithBackingMemory`: Creates a new `VkImage` and allocates backing memory in the "device-local" memory region. 
- `vklDestroyDeviceLocalImageAndItsBackingMemory`: Corresponding destruction function.

#### Resource Loading (3D Models, Textures)

For loading 3D Models, Vulkan Launchpad provides the following utility functions:
- `vklLoadModelGeometry`: Loads the (indexed) geometry of a 3D model (namely: positions, indices, normals, and texture coordinates) into host memory.       
    The following 3D model format is supported: OBJ.

For loading DDS images, Vulkan Launchpad provides the following utility functions:
- `vklGetDdsImageInfo`: Load meta data of a DDS image
- `vklLoadDdsImageIntoHostCoherentBuffer`: Load an image into a buffer, which has its backing memory in the "host-coherent" memory region.

#### Logging and Error Checking

The following macros are defined for logging purposes: 
- `VKL_LOG`: Prints a log message to the console.
- `VKL_WARNING`: Prints a warning message to the console.
- `VKL_EXIT_WITH_ERROR`: Prints an error message to the console and terminates the program.

Additionally, Vulkan Launchpad offers several possibilities to process a `VkResult`, which is returned by many Vulkan operations:

- `VKL_CHECK_VULKAN_RESULT` : Evaluates a `VkResult` and displays its status.
- `VKL_CHECK_VULKAN_ERROR` : Evaluates a `VkResult` and displays its status only if it represents an error.
- `VKL_RETURN_ON_ERROR` : Evaluates a `VkResult` and issues a return statement if it represents an error.


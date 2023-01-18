# Vulkan Launchpad :rocket:

A framework by TU Wien targeted at Vulkan beginners. It abstracts some of the hard and overly verbose parts of the Vulkan C API and can help to boost your learning progress early on. 

## Setup Instructions

Vulkan Launchpad runs on Windows, MacOS, and Linux. For building you'll need [Git](https://git-scm.com/), the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/), a C++ compiler, [CMake](https://cmake.org/) and optimally an integrated development environment (IDE). In the following, we describe setup instructions for common operating systems and editors/IDEs (click the links in the index to jump to the respective section):
- [Operating Systems](https://github.com/cg-tuwien/VulkanLaunchpad/blob/setup/README.md#operating-systems)
    - [Windows](https://github.com/cg-tuwien/VulkanLaunchpad/blob/setup/README.md#windows)
- [Editors and IDEs](https://github.com/cg-tuwien/VulkanLaunchpad/blob/setup/README.md#editors-and-ides)
    - [Visual Studio Code (VS Code)](https://github.com/cg-tuwien/VulkanLaunchpad/blob/setup/README.md#visual-studio-code-vs-code)
    - [Visual Studio 2022 Community](https://github.com/cg-tuwien/VulkanLaunchpad/blob/setup/README.md#visual-studio-2022-community)

### Operating Systems

#### Windows
- Download and install [Git for Windows](https://git-scm.com/download/win)!
    - Add Git to your PATH! This can be done through the installer, selecting the `Git from the command line and also from 3rd-party software` option. 
- Download and install one of the latest [Vulkan SDKs for Windows](https://vulkan.lunarg.com/sdk/home#windows)! (At time of writing, the most recent version is 1.3.236.0.)
    - _Note:_ It is not required to install any optional components, if you make only x64 builds.
- Download and install the Microsoft Visual C++ compiler (MSVC) by installing the [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/?q=build+tools) or a newer version!
    - Select the `Desktop development with C++` workload in the installer!
    - _Note:_ Should you decide to install Visual Studio Community 2022 (as described below), you can skip this step. But also in this case, ensure select the `Desktop development with C++` workload during installation of Visual Studio Community 2022! 
- Download and install CMake through its [Windows x64 Installer](https://cmake.org/download/)!
    - Select an option to `Add CMake to the system PATH ...` during installation!
	- _Important:_ Ensure to get CMake version `3.24` or newer!


### Editors and IDEs

#### Visual Studio Code (VS Code)
- Download and install [Visual Studio Code](https://code.visualstudio.com/download)!
    - Select the option `Add "Open with Code" action to Widows Explorer directory context menu` for more convenience.
- Install the following extensions (navigate to `View -> Extensions`):
    - [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) (which will also install the [CMake](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension).
    - [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
    - Restart VS Code, or execute the comand `Developer: Reload Window` (default shortcut on Windows: `Ctrl+R`)!
- Open the folder containing the `CMakeLists.txt` file (the root folder of this repository)!
    - This can be accomplished through `File -> Open Folder...`, you might also get the option to `Open with Code` from a folder's context menu in Windows Explorer.
- Execute the following commands (either through `Show All Commands`, which can be activated by default via `Ctrl+Shift+P`on Windows, or you'll also find buttons for these actions at the bottom of the VS Code window):
    - `CMake: Select a Kit` then select, e.g., `Visual Studio Build Tools 2022 Release - amd64` (if you are using Windows and have installed the `Build Tools for Visual Studio 2022`).
    - `CMake: Select Variant` and select `Debug` for a build with debug information, or `Release` for one without.
    - `CMake: Build Target`, then select `VulkanLaunchpad STATIC_LIBRARY` to build Vulkan Launchpad as a static library. Alternatively, just build everything by selecting `ALL_BUILD`.
    - TODO for VulkanLaunchpadStarter: `CMake: Debug` to start debugging the the selected target (default shortcut: `Ctrl+F5` on Windows).
    - TODO for VulkanLaunchpadStarter: `CMake: Run` to start debugging the the selected target (default shortcut: `Shift+F5` on Windows).

#### Visual Studio 2022 Community
- Download and install [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/community/), or a newer version.
    - Select the `Desktop development with C++` workload in the installer!
    - This should already install CMake. Should you encounter any problems in that regard, try installing CMake through its [Windows x64 Installer](https://cmake.org/download/)!
        - Ensure to select an option to `Add CMake to the system PATH ...` during installation!
- Open the folder containing the `CMakeLists.txt` file (the root folder of this repository)!
    - This can be accomplished through `File -> Open -> Folder...`, you might also get the option to `Open with Visual Studio` from a folder's context menu in Windows Explorer.
    - You should be able to observer in the `Output` tab that CMake generation started.
        - If not, check if the `Show output from:` combobox is set to the option `CMake`!
        - Wait a bit until you see the message `CMake generation finished.`.
    - Execute `Build -> Build All` (default shortcut: `Ctrl+Shift+B`) to build Vulkan Launchpad as a static library.

## Documentation

TODO

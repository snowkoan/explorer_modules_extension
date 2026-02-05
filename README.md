# Explorer Modules Namespace Extension

A Windows Explorer Namespace Extension that allows you to inspect and interact with the modules (DLLs) currently loaded within the Explorer process.

![Windows](https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows&logoColor=white)
![Language](https://img.shields.io/badge/Language-C++20-00599C?logo=c%2B%2B&logoColor=white)
![Build](https://img.shields.io/badge/Build-CMake-064F8C?logo=cmake&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)

## 📋 Overlay

This project implements a virtual folder that integrates directly into Windows Explorer under "This PC". It serves two primary purposes:

1.  **Visualization**: Lists all modules (EXEs and DLLs) loaded in the current `explorer.exe` process, showing details like base address and memory size.
2.  **Interaction**: Allows you to dynamically load new modules into the Explorer process simply by dragging and dropping them into the folder.

## ✨ Features

-   **Process Inspection**: View a real-time list of all loaded modules in the shell process.
-   **Detailed Columns**: Displays **Name**, **Base Address**, and **Size** for each module.
-   **Dynamic Injection**: Drag & drop or copy & paste any DLL file into the folder to call `LoadLibrary` on it immediately.
-   **User-Level Registration**: Registers in `HKCU`, so **no administrator privileges** are required to install or use it.

## 🚀 Getting Started

### Prerequisites

-   Windows 10/11
-   [Visual Studio 2022](https://visualstudio.microsoft.com/) (or newer) with C++ Desktop Development workload.
-   [CMake](https://cmake.org/) (3.22 or later)

### Installation

1.  **Clone the repository:**
    ```powershell
    git clone https://github.com/snowkoan/explorer_modules_extension.git
    cd explorer_modules_extension
    ```

2.  **Build the project:**
    ```powershell
    cmake -B build
    cmake --build build --config RelWithDebInfo
    ```

3.  **Register the extension:**
    Open a terminal (no admin required) and run:
    ```powershell
    regsvr32 .\build\RelWithDebInfo\ExplorerModulesNamespace.dll
    ```
    *Note: Adjust the path if you built a different configuration (e.g., Debug).*

### Usage

1.  Open **File Explorer** (`Win+E`).
2.  Navigate to **This PC**.
3.  You will see a new folder named **Explorer Modules** in the "Devices and drives" or "Network locations" section.
4.  Open it to see the list of loaded modules.
5.  **To load a new DLL**: Drag a DLL file from another folder and drop it into the **Explorer Modules** window.

### Uninstallation

To remove the extension, simply unregister the DLL:

```powershell
regsvr32 /u .\build\RelWithDebInfo\ExplorerModulesNamespace.dll
```

## 🛠️ Technical Details

This project is a great example of modern C++ COM programming for Windows Shell Extensions.

-   **Language**: C++20 standard.
-   **Frameworks**: Uses [WRL (Windows Runtime Library)](https://learn.microsoft.com/cpp/cppcx/wrl/windows-runtime-library-template-library-wrl) for lightweight COM implementation.
-   **APIs**:
    -   `IShellFolder` / `IShellView`: For the folder UI and navigation.
    -   `EnumProcessModules`: To enumerate loaded libraries.
    -   `IDropTarget`: To handle file drops for module loading.

## 🤝 Contributing

Contributions are welcome! Please check out our [CONTRIBUTING.md](docs/CONTRIBUTING.md) guide for details on how to submit pull requests, report issues, or request features.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🆘 Support

If you encounter issues or have questions:
-   Check the [Issues](https://github.com/snowkoan/explorer_modules_extension/issues) page.
-   Open a new issue if you find a bug.

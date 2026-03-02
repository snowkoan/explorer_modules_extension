# Copilot Instructions — Explorer Modules Namespace Extension

## Project Overview

Windows Explorer namespace extension (COM in-proc server DLL) that adds a virtual "Explorer Modules" folder under "This PC". It lists all DLLs/EXEs loaded in the `explorer.exe` process and supports drag-and-drop loading and right-click unloading of modules. Written in C++20, built with CMake, targets Windows 10/11.

## Architecture

| Component | Files | Role |
|---|---|---|
| **DLL entry & registration** | `src/dllmain.cpp`, `src/ClassFactory.cpp` | Standard COM entry points (`DllGetClassObject`, `DllRegisterServer`, etc.). Registers in `HKCU` — no admin required. |
| **Shell folder** | `src/ModuleFolder.{h,cpp}` | Core class implementing `IShellFolder2`, `IPersistFolder2`, `IDropTarget`, `IShellFolderViewCB`. Handles enumeration, columns, context menus, and drag-drop. |
| **Item enumeration** | `src/EnumIDList.{h,cpp}` | `IEnumIDList` implementation returned by `EnumObjects`. Deep-clones PIDLs. |
| **Context Menu** | `src/ItemContextMenu.{h,cpp}` | Implementation of `IContextMenu` family for right-click operations (Explore, Properties, Unload, Copy Path). |
| **Custom PIDLs** | `src/Pidl.{h,cpp}` | Packed binary format (`'MODL'` signature) embedding module path, base address, and size. LE layout is `[cb][signature][baseAddress][size][path]`. Structure definition is in `Pidl.h`. |
| **Module inspection** | `src/ModuleHelpers.{h,cpp}` | Business logic: `GetLoadedModules()`, `GetImageInfo()` (PE headers + version resources), `LoadModulesIf()`, `UnloadLibrary()`. |
| **DLL notifications** | `src/DllNotification.{h,cpp}`, `src/NtDll.h` | Uses undocumented `LdrRegisterDllNotification` (ntdll) to auto-refresh the folder view when modules load/unload. `NtDll.h` contains the NT type definitions. |
| **Diagnostics** | `src/Log.{h,cpp}`, `src/IidNames.{h,cpp}` | `OutputDebugStringW`-based logging with compile-time level filter (`kMaxLogLevel`). IID-to-name lookup for debug output. |


## Build

```powershell
# Debug (default):
.\build.bat
# Release:
.\build.bat release
# Or manually:
cmake -S . -B build
cmake --build build --config Debug
```

- **Toolchain:** Visual Studio 2022+, CMake 3.22+, MSVC with `/W4 /WX` (warnings-as-errors).
- **Runtime:** Static CRT (`/MT` / `/MTd`) — the DLL has no vcruntime dependency.
- **Output:** `build\<Config>\ExplorerModulesNamespace.dll`
- **Registration:** `regsvr32 .\build\Debug\ExplorerModulesNamespace.dll` (unregister with `/u`).

## Key Conventions

- **COM via WRL:** All COM classes use `Microsoft::WRL::RuntimeClass<ClassicCom, ...>` for ref-counting and `QueryInterface`. Never raw `AddRef`/`Release` implementations.
- **HRESULT everywhere:** Every COM method returns `HRESULT`. Log failures with `Log::Write` before returning.
- **RAII:** Use RAII wrappers for handles (see `ScopedKey` in `dllmain.cpp`). PIDLs are managed via `Pidl::Clone`/`Pidl::Free` pairs.
- **Anonymous namespaces:** Internal-linkage helpers and constants go in anonymous namespaces, not `static`.
- **PIDL format:** Items are identified by custom PIDLs with a `0x4C444F4D` (`MODL`) signature. Always validate with `Pidl::IsOurPidl()` before accessing data. Layout: `[cb][signature][baseAddress][size][path]`.
- **Logging:** Use `Log::Write(Log::Level::X, L"format", ...)`. Levels: `Critical`, `Error`, `Warn`, `Info`, `Trace`. Trace is compiled out by default.
- **String constants:** Use `constexpr wchar_t kName[]` with `k`-prefix naming for constants.

## CLSID & Registration

- **CLSID:** `{6B4E2E3B-3D6B-4D4E-9A1C-0F0C8D8E8F11}` — defined in `ModuleFolder.h`.
- **Namespace parent:** "My Computer" `{20D04FE0-3AEA-1069-A2D8-08002B30309D}`.
- Registration writes to `HKCU\Software\Classes\CLSID\{...}` and `HKCU\...\Explorer\MyComputer\NameSpace\{...}`.

## Adding a New Column

1. Add a `kColumn*` constant in `src/ModuleFolder.cpp`, increment `kColumnCount`.
2. Add a new entry to the `kColumns` table in `src/ModuleFolder.cpp` with the column title and a lambda to retrieve the formatted string value from a PIDL.
3. Everything else (`GetDetailsOf`, `MapColumnToSCID` etc) is handled automatically by the table-driven implementation.

## Testing & Debugging

- No automated test suite; test manually by registering the DLL and opening "This PC" in Explorer.
- Attach a debugger to `explorer.exe` or use **DebugView** to see `Log::Write` output (prefixed `[ExplorerModules]`).
- To iterate quickly: unregister → rebuild → re-register → restart Explorer or `SHChangeNotify`.

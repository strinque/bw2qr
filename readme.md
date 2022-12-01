# xxx

## Introduction


## Usage

``` console
```

## Requirements

This project uses **vcpkg**, a free C/C++ package manager for acquiring and managing libraries to build all the required libraries.  
It also needs the installation of the **winpp**, a private *header-only library*, inside **vcpkg**.

### Install vcpkg

The install procedure can be found in: https://vcpkg.io/en/getting-started.html.  
The following procedure installs `vcpkg` and integrates it in **Visual Studio**.

Open PowerShell: 

``` console
cd c:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install
```

Create a `x64-windows-static-md` triplet file used to build the program in *shared-mode* for **Windows CRT** libraries but *static-mode* for third-party libraries:

``` console
$VCPKG_DIR = Get-Content "$Env:LocalAppData/vcpkg/vcpkg.path.txt" -Raw 

Set-Content "$VCPKG_DIR/triplets/community/x64-windows-static-md.cmake" 'set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)'
```

### Install winpp ports-files

Copy the *vcpkg ports files* from **winpp** *header-only library* repository to the **vcpkg** directory.

``` console
$VCPKG_DIR = Get-Content "$Env:LocalAppData/vcpkg/vcpkg.path.txt" -Raw 

mkdir $VCPKG_DIR/ports/winpp
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/strinque/winpp/master/vcpkg/ports/winpp/portfile.cmake" -OutFile "$VCPKG_DIR/ports/winpp/portfile.cmake"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/strinque/winpp/master/vcpkg/ports/winpp/vcpkg.json" -OutFile "$VCPKG_DIR/ports/winpp/vcpkg.json"
```

## Build

### Build using cmake

To build the program with `vcpkg` and `cmake`, follow these steps:

``` console
$VCPKG_DIR = Get-Content "$Env:LocalAppData/vcpkg/vcpkg.path.txt" -Raw 

git clone ssh://git@git.fum-server.fr:1863/git/xxx.git
cd xxx
mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE="MinSizeRel" `
      -DVCPKG_TARGET_TRIPLET="x64-windows-static-md" `
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" `
      ../
cmake --build . --config MinSizeRel
```

The program executable should be compiled in: `xxx\build\src\MinSizeRel\xxx.exe`.

### Build with Visual Studio

**Microsoft Visual Studio** can automatically install required **vcpkg** libraries and build the program thanks to the pre-configured files: 

- `CMakeSettings.json`: debug and release settings
- `vcpkg.json`: libraries dependencies

The following steps needs to be executed in order to build/debug the program:

``` console
File => Open => Folder...
  Choose xxx root directory
Solution Explorer => Switch between solutions and available views => CMake Targets View
Select x64-release or x64-debug
Select the src\xxx.exe (not bin\xxx.exe)
```

To add command-line arguments for debugging the program:

```
Solution Explorer => Project => (executable) => Debug and Launch Settings => src\program.exe
```

``` json
  "args": []
```
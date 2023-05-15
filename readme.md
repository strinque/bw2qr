# bw2qr

## Introduction
This project is a c++ program that allows you to convert an exported `json` vault file from **bitwarden** or **vaultwarden** into a printable `pdf` file. This can be useful for securely storing all of your passwords in one place. In addition, the program uses **qrcode** with high redundancy mode to print each entry, which helps to recover damaged entries.

The program reads a **bitwarden** or **vaultwarden** `json` file and export specific entries that are tagged as `favorite` as **qrcode** exported in a `pdf` format. The exported entries include important login information such as the *username*, *password*, *authenticator key*, and *added custom fields*.

Once the **qrcode** is scanned, it can be read as text and includes all of the important login information in a readable format.  
For example:
``` json
{
  "login": {
    "username": "myusername",
    "password": "mypassword",
    "totp": "authenticator key",
  },
  "fields": [
    { "seed": "this is a test" }
  ]
}
```

!!! tip
    All **qrcode** are using the version: `33` with ecc: `high` (up to 30% of redondancy).  
    Thus, the maximum size of data that can be embedded in each **qrcode**: `511` bytes.

## Usage

``` console
bw2qr.exe --json bitwarden.json \
          --pdf file.pdf
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

git clone ssh://git@git.fum-server.fr:1863/git/bw2qr.git
cd bw2qr
mkdir build; cd build
cmake -DCMAKE_BUILD_TYPE="MinSizeRel" `
      -DVCPKG_TARGET_TRIPLET="x64-windows-static-md" `
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" `
      ../
cmake --build . --config MinSizeRel
```

The program executable should be compiled in: `bw2qr\build\src\MinSizeRel\bw2qr.exe`.

### Build with Visual Studio

**Microsoft Visual Studio** can automatically install required **vcpkg** libraries and build the program thanks to the pre-configured files: 

- `CMakeSettings.json`: debug and release settings
- `vcpkg.json`: libraries dependencies

The following steps needs to be executed in order to build/debug the program:

``` console
File => Open => Folder...
  Choose bw2qr root directory
Solution Explorer => Switch between solutions and available views => CMake Targets View
Select x64-release or x64-debug
Select the src\bw2qr.exe (not bin\bw2qr.exe)
```

To add command-line arguments for debugging the program:

```
Solution Explorer => Project => (executable) => Debug and Launch Settings => src\program.exe
```

``` json
  "args": [
    "--json \"${ProjectDir}\\model\\bitwarden.json\"",
    "--pdf \"${ProjectDir}\\model\\file.pdf\""
    "--qrcode-module-px-size 3",
    "--qrcode-border-px-size 3",
    "--qrcode-module-color \"black\"",
    "--qrcode-background-color \"white\"",
    "--frame-border-color \"#485778\"",
    "--frame-border-width-size 12",
    "--frame-border-height-size 65",
    "--frame-border-radius 15",
    "--frame-logo-size 48",
    "--frame-font-family \"Arial-Black\"",
    "--frame-font-color \"white\"",
    "--frame-font-size 28.0"
  ]
```
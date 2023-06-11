# 🔒 bw2qr 🔒

## Introduction

The **bw2qr** application, written in **C++** and hosted on GitHub, enables users to convert a JSON vault file exported from **Bitwarden** or **Vaultwarden** into a printable PDF file. With its offline functionality, this **C++** desktop application ensures enhanced security and data isolation, making it a reliable solution for securely storing passwords. The program utilizes **qrcode** with high redundancy mode for printing entries, offering the ability to recover damaged data, and verifies the validity of generated qrcode using the `opencv` library.  

This application incorporates the **AES-256-CBC** algorithm to encrypt the data of the qrcode when the `--password` command-line option is set. This encryption enhances the security of the generated qrcode by providing robust data protection.  

Implemented in c++17 and use `vcpkg`/`cmake` for the build-system.  
It uses the `winpp` header-only library from: https://github.com/strinque/winpp.  

## Features

- [x] use `winpp` to handle command-line argument variables
- [x] use `nlohmann/json` header-only library for `json` parsing
- [x] use `cpp-httplib` to get the favicon of websites to add them in the QR Code
- [x] use `nayuki-qr-code-generator` to generate QR Code
- [x] use `graphicsmagick` to create QR Code png image and frame
- [x] use `PoDoFo` to create the `pdf` file
- [x] use `opencv` to check the validity of the QR Code
- [x] use `openssl` to encrypt QR Code with **AES 256 CBC** algorithm

!!! note
    When it comes to handling passwords and vaults, it is crucial to prioritize security and avoid trusting any applications or websites that may compromise your credentials. The **bw2qr** application prioritizes security by operating in full offline mode by default, eliminating the risk of interception or unauthorized storage. However, if you specify the command-line option `--frame-logo-size 64`, it will retrieve the `favicon` and position it at the center of the qrcode, providing an additional visual element.

## Description
The program reads a **bitwarden** or **vaultwarden** `json` file and export specific entries that are tagged as `favorite` as **qrcode** exported in a `pdf` file. The exported entries include important login information such as *username*, *password*, *authenticator key*, and *custom fields*.

Once the **qrcode** is scanned, it can be read as text and includes all of the important login information in a `json` readable format.  
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
    All **qrcode** are using the algorithm version: `24` with ecc: `high` (up to 30% of redondancy).  
    Thus, the maximum size of data that can be embedded in each **qrcode**: `511` bytes (or `511 - 16 = 495` bytes with encryption).

!!! note To Decrypt QR Code
    To decrypt an encrypted QR Code with **AES-256-CBC** algorithm (when a password has been set), prefer using an offline application such as **Cryptography Tool [AES256/CBC]** on **Android**. Otherwise, use the following websites [https://encode-decode.com/aes-256-cbc-encrypt-online](https://encode-decode.com/aes-256-cbc-encrypt-online) or [https://www.toolnb.com/tools-lang-en/aesEnDe.html](https://www.toolnb.com/tools-lang-en/aesEnDe.html).

## Usage

Arguments:

- `--json`:                       path to the bitwarden json file                                       [mandatory]
- `--pdf`:                        path to the pdf output file                                           [mandatory]
- `--password`:                   set a password to encrypt QR Code data using AES-256-CBC
- `--qrcode-module-px-size`:      size in pixels of each QR Code module        (default: 3)
- `--qrcode-border-px-size`:      size in pixels of the QR Code border         (default: 3)
- `--qrcode-module-color`:        QR Code module color                         (default: black)
- `--qrcode-background-color`:    QR Code background color                     (default: white)
- `--frame-border-color`:         color of the frame                           (default: #054080)
- `--frame-border-width-size`:    size in pixels of the frame border width     (default: 12)
- `--frame-border-height-size`:   size in pixels of the frame border height    (default: 65)
- `--frame-border-radius`:        size in pixels of the frame border radius    (default: 15)
- `--frame-logo-size`:            size in pixels of the logo                   (default: 0)
- `--frame-font-family`:          font family of the QR Code name              (default: Arial-Black)
- `--frame-font-color`:           font color of the QR Code name               (default: white)
- `--frame-font-size`:            size in pixels of the QR Code name font      (default: 28)
- `--pdf-cols`:                   number of columns of QR Codes in pdf         (default: 4)
- `--pdf-rows`:                   number of rows of QR Codes in pdf            (default: 5)

T
``` console
bw2qr.exe --json bitwarden.json \
          --pdf file.pdf \
          --frame-logo-size 64
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
    "--pdf \"${ProjectDir}\\model\\file.pdf\"",
    "--qrcode-module-px-size 3",
    "--qrcode-border-px-size 3",
    "--qrcode-module-color \"black\"",
    "--qrcode-background-color \"white\"",
    "--frame-border-color \"#054080\"",
    "--frame-border-width-size 12",
    "--frame-border-height-size 65",
    "--frame-border-radius 15",
    "--frame-logo-size 64",
    "--frame-font-family \"Arial-Black\"",
    "--frame-font-color \"white\"",
    "--frame-font-size 28.0",
    "--pdf-cols 4",
    "--pdf-rows 5"
  ]
```
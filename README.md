# GlassBar

Lightweight Windows utility written in C++ that allows you to adjust the opacity of the entire Windows taskbar.



<p>
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=flat-square" alt="C++">
  <img src="https://img.shields.io/github/license/w1spo/glassbar?style=flat-square" alt="License">
</p>

<img width="800" height="450" alt="ezgif-7aaffcf627e86dc5" src="https://github.com/user-attachments/assets/a8655405-3aad-4d9c-a00d-6b707c2cc842" />

## Overview

GlassBar is a small native Windows application designed to provide a simple way to control the opacity of the Windows taskbar.

The application focuses on being lightweight, fast and unobtrusive while running in the background.

## Features

* Adjustable taskbar opacity
* Native C++ implementation
* Minimal memory and CPU usage
* No external runtime dependencies
* Lightweight background process
* Designed for Windows 10 and Windows 11

## Download

Download the latest version from the [Releases](../../releases) page.

## Building

### Requirements

* Windows 10 or Windows 11
* Visual Studio 2022
* C++ Desktop Development workload
* Windows SDK

### Build

Clone the repository:

```bash
git clone https://github.com/w1spo/glassbar.git
cd glassbar
```

Open the project in Visual Studio and build it using the desired configuration.

## Technical Details

GlassBar is written entirely in native C++ and uses the Windows API to interact with the desktop environment and taskbar.

The application is designed to keep its footprint as small as possible and avoid unnecessary dependencies.

## License

GlassBar is released under the MIT License.

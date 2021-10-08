<!-- ![Icon](RadTerminal.ico) RadTerminal -->
<img src="RadTerminal.ico" width=32/> [RadTerminal](../..)
==========

Simple Terminal Emulator for Windows using [libtsm](https://github.com/RadAd/libtsm). Implemented using a [Pseudo Console](https://docs.microsoft.com/en-us/windows/console/pseudoconsoles).

![Windows](https://img.shields.io/badge/platform-Windows-blue.svg)
[![Releases](https://img.shields.io/github/release/RadAd/RadTerminal.svg)](../../releases/latest)
[![Build](https://img.shields.io/appveyor/ci/RadAd/RadTerminal.svg)](https://ci.appveyor.com/project/RadAd/RadTerminal)
[![License](https://img.shields.io/github/license/RadAd/RadTerminal)](LICENSE.txt)

![Screenshot](docs/Screenshot.png)

Dependencies
=======
[libtsm](https://github.com/RadAd/libtsm) - Terminal-emulator State Machine

Build
=======
```bat
msbuild RadTerminal.vcxproj -p:Configuration=Release -p:Platform=x64
```

Execute
=======
```bat
msbuild RadTerminal.vcxproj -p:Configuration=Release -p:Platform=x64 /t:Build,Run
```

License
=======
[MIT License](LICENSE.txt)

![image](https://github.com/ConcealNetwork/conceal-assets/blob/master/splash.png)

#### Master Build Status
[![Build Status](https://travis-ci.org/ConcealNetwork/conceal-core.svg?branch=master)](https://travis-ci.org/ConcealNetwork/conceal-core) [![Build status](https://ci.appveyor.com/api/projects/status/github/concealnetwork/conceal-core?branch=master&svg=true)](https://ci.appveyor.com/project/cryptokatz/conceal-core)

#### Development Build Status
[![Build Status](https://travis-ci.org/ConcealNetwork/conceal-core.svg?branch=dev)](https://travis-ci.org/ConcealNetwork/conceal-core) [![Build status](https://ci.appveyor.com/api/projects/status/github/concealnetwork/conceal-core?branch=dev&svg=true)](https://ci.appveyor.com/project/cryptokatz/conceal-core)


# Conceal Core (CLI)
Latest Release: v5.1.9
Maintained by The Circle Team.

## Information
Conceal is a decentralized banking platform with encrypted messages and own privacy protected cryptocurrency.

Conceal is based on the Cryptonote protocol and runs on a secure peer-to-peer network technology to operate with no central authority. You control your private keys to your funds, you control your destiny. Conceal Network is accessible by anyone in the world regardless of his/her geographic location or status. Our blockchain is resistant to any kind of analysis. All your CCX transactions and messages are anonymous. Conceal Network provides an instant secure, untraceable and unlinkable way of encrypted communication - crypto messages.

Conceal is open-source, community driven and truly decentralized. No one owns it, everyone can take part.

## Resources
- Web: [conceal.network](https://conceal.network/)
- GitHub: [https://github.com/ConcealNetwork/conceal-core](https://github.com/ConcealNetwork/conceal-core)
- Discord: [https://discord.gg/YbpHVSd](https://discord.gg/YbpHVSd)
- Twitter: [https://twitter.com/ConcealNetwork](https://twitter.com/ConcealNetwork)
- Medium: [https://medium.com/@ConcealNetwork](https://medium.com/@ConcealNetwork)
- Reddit: [https://www.reddit.com/r/ConcealNetwork/](https://www.reddit.com/r/ConcealNetwork/)
- Bitcoin Talk: [https://bitcointalk.org/index.php?topic=4515873](https://bitcointalk.org/index.php?topic=4515873)
- Paperwallet: [https://paperwallet.conceal.network/](https://paperwallet.conceal.network/)

## Compiling Conceal from source

### Linux / Ubuntu

##### Prerequisites

- You will need the following dependencies to build the Conceal CLI: boost, cmake, git, gcc, g++, python, and make.
- On Ubuntu: `sudo apt-get install -y build-essential python-dev gcc g++ git cmake libboost-all-dev`

#### Building

- `git clone https://github.com/ConcealNetwork/conceal-core`
- `cd conceal-core`
- `mkdir build && cd $_`
- `cmake ..`
- `make`

If the build is successful the binaries will be in the src folder.

### Windows 10

##### Prerequisites

- Install [Visual Studio 2017 Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15&page=inlineinstall)
- Install [CMake](https://cmake.org/download/)
- When installing Visual Studio, you need to install **Desktop development with C++** and the **VC++ v140 toolchain** components. The option to install the v140 toolchain can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Install [Boost 1.67.0](https://boost.teeks99.com/bin/1.67.0/), ensuring you download the installer for MSVC 14.1.

##### Building

- From the start menu, open 'x64 Native Tools Command Prompt for vs2017' or run "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsMSBuildCmd.bat" from any command prompt.
- `git clone https://github.com/ConcealNetwork/conceal-core`
- `cd conceal-core`
- `mkdir build`
- `cd build`
- `cmake -G "Visual Studio 15 2017 Win64" -DBOOST_LIBRARYDIR:PATH=c:/local/boost_1_67_0 ..` (Or your boost installed dir.)
- `msbuild concealX.sln /p:Configuration=Release /m`

If the build is successful the binaries will be in the src/Release folder.

#### Special Thanks
Special thanks goes out to the developers from Cryptonote, Bytecoin, Monero, Forknote, TurtleCoin, and Masari.

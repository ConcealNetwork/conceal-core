![image](https://github.com/TheCircleFoundation/conceal-assets/blob/master/splash.png)

# Conceal Core (CLI)
Latest Release: v5.0.0
Maintained by The Circle Team.

## Information
Conceal is a free open source privacy protected peer-to-peer digital cash system that is completely decentralized, without the need for a central server or trusted parties. Users hold the crypto keys to their own money and transact directly with each other, with the help of a P2P network to check for double-spending. Conceal has a decentralized blockchain banking in its core and instant untraceable crypto messages that can be decrypted with recipient`s private key.

## Resources
- Web: [conceal.network](https://conceal.network/)
- GitHub: [https://github.com/TheCircleFoundation/conceal-core](https://github.com/TheCircleFoundation/conceal-core)
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

- `git clone https://github.com/TheCircleFoundation/conceal-core`
- `cd conceal-core`
- `make build-release`

If the build is successful the binaries will be in the src folder.

### Windows 10

##### Prerequisites

- Install [Visual Studio 2017 Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15&page=inlineinstall)
- Install [CMake](https://cmake.org/download/)
- When installing Visual Studio, you need to install **Desktop development with C++** and the **VC++ v140 toolchain** components. The option to install the v140 toolchain can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Install [Boost 1.67.0](https://boost.teeks99.com/bin/1.67.0/), ensuring you download the installer for MSVC 14.1.

##### Building

- From the start menu, open 'x64 Native Tools Command Prompt for vs2017' or run "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\VsMSBuildCmd.bat" from any command prompt.
- `git clone https://github.com/TheCircleFoundation/conceal-core`
- `cd conceal-core`
- `mkdir build`
- `cd build`
- `cmake -G "Visual Studio 15 2017 Win64" -DBOOST_LIBRARYDIR:PATH=c:/local/boost_1_67_0 ..` (Or your boost installed dir.)
- `msbuild concealX.sln /p:Configuration=Release /m`

If the build is successful the binaries will be in the src/Release folder.

#### Special Thanks
Special thanks goes out to the developers from Cryptonote, Bytecoin, Monero, Forknote, TurtleCoin, and Masari.

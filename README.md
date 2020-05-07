![image](https://github.com/ConcealNetwork/conceal-assets/blob/master/splash.png)

![](https://github.com/bomb-on/conceal-core/workflows/Ubuntu%2016.04/badge.svg)  
![](https://github.com/bomb-on/conceal-core/workflows/Ubuntu%2018.04/badge.svg)  
![](https://github.com/bomb-on/conceal-core/workflows/Windows/badge.svg)  
![](https://github.com/bomb-on/conceal-core/workflows/macOS/badge.svg)

# Conceal Core (CLI)
Latest Release: v6.1.6
Maintained by Conceal Developers.

## Information
Conceal is a decentralized banking platform with encrypted messages and own privacy protected cryptocurrency.

Conceal is based on the Cryptonote protocol and runs on a secure peer-to-peer network technology to operate with no central authority. You control your private keys to your funds, you control your destiny. Conceal Network is accessible by anyone in the world regardless of his/her geographic location or status. Our blockchain is resistant to any kind of analysis. All your CCX transactions and messages are anonymous. Conceal Network provides an instant secure, untraceable and unlinkable way of encrypted communication - crypto messages.

Conceal is open-source, community driven and truly decentralized. No one owns it, everyone can take part.

## Resources
- Web: [http://conceal.network](https://conceal.network/)
- GitHub: [https://github.com/ConcealNetwork/conceal-core](https://github.com/ConcealNetwork/conceal-core)
- Wiki: [https://conceal.network/wiki](https://conceal.network/wiki/doku.php)
- Explorer: [https://explorer.conceal.network](https://explorer.conceal.network)
- Paperwallet: [https://conceal.network/paperwallet](https://conceal.network/paperwallet/)
- Discord: [https://discord.gg/YbpHVSd](https://discord.gg/YbpHVSd)
- Twitter: [https://twitter.com/ConcealNetwork](https://twitter.com/ConcealNetwork)
- Medium: [https://medium.com/@ConcealNetwork](https://medium.com/@ConcealNetwork)
- Reddit: [https://www.reddit.com/r/ConcealNetwork/](https://www.reddit.com/r/ConcealNetwork/)
- Bitcoin Talk: [https://bitcointalk.org/index.php?topic=4515873](https://bitcointalk.org/index.php?topic=4515873)


## Compiling Conceal from source

### Linux / Ubuntu

##### Prerequisites

- You will need the following dependencies to build the Conceal CLI: boost, cmake, git, gcc, g++, python, and make.
- On Ubuntu:

```bash
$ sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
$ sudo apt-get update
$ sudo apt-get install aptitude -y
$ sudo aptitude install -y build-essential g++-8 gcc-8 git libboost-all-dev python-pip
$ sudo pip install cmake
$ sudo apt-get install -y build-essential python-dev g++-8 gcc-8 git cmake libboost-all-dev
$ export CC=gcc-8
$ export CXX=g++-8
```

#### Building

```bash
$ git clone https://github.com/ConcealNetwork/conceal-core
$ cd conceal-core
$ mkdir build && cd $_
$ cmake ..
$ make
```

If the build is successful the binaries will be in the src folder.

### Windows 10

##### Prerequisites

- Install [Visual Studio 2019 Community Edition](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=16)
- Install [CMake](https://cmake.org/download/)
- When installing Visual Studio, you need to install **Desktop development with C++** and the **MSVC v142 - VS 2019 C++ x64/x86 build tools** components. The option to install the v142 build tools can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Use the premade binaries for [MSVC 14.2](https://github.com/ConcealNetwork)
- Install [Boost 1.70.0](https://sourceforge.net/projects/boost/files/boost-binaries/1.70.0/boost_1_70_0-msvc-14.1-64.exe/download), ensuring you download the installer for MSVC 14.2.

##### Building

- From the start menu, open `x64 Native Tools Command Prompt for vs2019` 
    - OR run `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsMSBuildCmd.bat` from any command prompt.

```bash
$ git clone https://github.com/ConcealNetwork/conceal-core
$ cd conceal-core
$ mkdir build
$ cd build
$ cmake -G "Visual Studio 16 2017" -A x64 -DBOOST_LIBRARYDIR="c:\local\boost_1_70_0\lib64-msvc-14.2" ..
$ msbuild concealX.sln /p:Configuration=Release /m
```
*  (Or your boost installed dir with `-DBOOST_LIBRARYDIR=""`.)

If the build is successful the binaries will be in the src/Release folder.

### macOS

#### Prerequisites

- Install [cmake](https://cmake.org/). See [here](https://stackoverflow.com/questions/23849962/cmake-installer-for-mac-fails-to-create-usr-bin-symlinks) if you are unable to call cmake from the terminal after installing.
- Install the [boost](http://www.boost.org/) libraries. Either compile boost manually or run brew install boost.
- Install [XCode](https://developer.apple.com/xcode/) and [Homebrew](https://brew.sh/).

Once both are ready, open terminal app, run following command:

```bash
$ xcode-select --install
```

and install all tools. On newer macOS versions (v10.14 and higher) this step is done through Software Update app.

After that, proceed with installing dependencies:

```bash
$ brew install git python cmake gcc boost@1.60
```

Ensure that Boost is properly linked:

```bash
$ brew link --force boost@1.60
```

When all dependencies are installed, build Conceal Core binaries:

```bash
$ git clone https://github.com/ConcealNetwork/conceal-core
$ cd conceal-core
$ mkdir build && cd $_
$ cmake ..
$ make
```

OR `cmake -DBOOST_ROOT=<path_to_boost_install> ..` when building from a specific boost install. If you used brew to install boost, your path is most likely `/usr/local/include/boost`.

If the build is successful the binaries will be located in `src` directory.

- If your version of gcc is too old, you may need to run:

```bash
brew install gcc@8
export CC=gcc-8
export CXX=g++-8
```

#### Special Thanks
Special thanks goes out to the developers from Cryptonote, Bytecoin, Monero, Forknote, TurtleCoin, Karbo and Masari.

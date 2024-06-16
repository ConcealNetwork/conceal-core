![image](https://github.com/ConcealNetwork/conceal-imagery/blob/master/logos/splash.png)

# Conceal Core (CLI)

[![GitHub release (latest by date)](https://img.shields.io/github/v/release/ConcealNetwork/conceal-core)](https://github.com/ConcealNetwork/conceal-core/releases/latest)
[![Ubuntu 20.04](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu20.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu20.yml)
[![Ubuntu 22.04](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu22.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu22.yml)
[![Windows](https://github.com/ConcealNetwork/conceal-core/actions/workflows/windows.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/windows.yml)
[![macOS](https://github.com/ConcealNetwork/conceal-core/actions/workflows/macOS.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/macOS.yml)

Maintained by Conceal Developers, overseen by Conceal Team and driven by Conceal Community.

## What's Conceal Network?

Conceal Network is a peer-to-peer privacy-preserving network made for Private DeFi and Encrypted Communications. Conceal offers protocol-level private transactions, blockchain deposits, and on-chain encrypted messages. These unique functionalities make it effortless for everyday users to manage their finances and communicate securely and privately.

Conceal provides the ability for individuals to communicate and financially interact with each other in a totally anonymous manner. Two persons may exchange messages, conduct business, and transact funds without ever knowing the true name, or legal identity, of the other. Interactions over networks are untraceable, via cryptographic protocols with nearly perfect assurance against any tampering.

₡CCX mimics the properties of cash. Nobody knows how or where you spend your folding cash. You go to the ATM, take out money and you can buy an ice-cream or that pretty old chair at the neighborhood yard sale and there is no record of it anywhere, unlike with a credit card. These powerful privacy-preserving abilities will act as a critical bulwark against the inevitable rise of Panopticon digital money. They may be our only hope. The end of cash means the end of free choice. That’s where Conceal comes into the picture.

A lot of people say there is no killer app for crypto. They’re wrong. We’ve figured out how to mimic cash in an environment that is entirely focused on privacy: The biggest feature of cash is that it’s really hard to track. In other words, anonymity is the main feature of cash. Only the two parties that conducted business know it happened. Conceal (₡CCX) is the death of blockchain analysis. Conceal brings back the true anonymity of cash.

Conceal is open-source, community driven and truly decentralized. No one owns it, everyone can take part.

## Resources

-   Web: <https://conceal.network>
-   GitHub: <https://github.com/ConcealNetwork>
-   Wiki: <https://conceal.network/wiki>
-   Explorer: <https://explorer.conceal.network>
-   Discord: <https://discord.gg/YbpHVSd>
-   Twitter: <https://twitter.com/ConcealNetwork>
-   Telegram Official (News Feed): <https://t.me/concealnetwork>
-   Telegram User Group (Chat Group): <https://t.me/concealnetworkusers>
-   Medium: <https://medium.com/@ConcealNetwork>
-   Reddit: <https://www.reddit.com/r/ConcealNetwork>
-   Bitcoin Talk: <https://bitcointalk.org/index.php?topic=4515873>
-   Paperwallet: <https://conceal.network/paperwallet>

## Compiling Conceal from source

### System Memory

The build process can consume upto 13GB of memory and may fail if enough resources are not available.
In some build scenarios it may be necessary to increase the size of the SWAP to compensate for less RAM.

For example if you have 8GB of RAM, then your SWAP size should be 5GB

-   Ubuntu / Debian

```bash
sudo fallocate -l 5G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

-   Rasberry Pi OS

```bash
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile
CONF_SWAPSIZE=5120
sudo nano /sbin/dphys-swapfile
#CONF_MAXSWAP=2048
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
```

### Linux

#### Prerequisites

-   You will need the following dependencies to build the Conceal CLI: boost, cmake, git, gcc, g++, python, and make:

##### Ubuntu and Debian Based Systems

```bash
sudo apt update
sudo apt install -y build-essential python3-dev git cmake libboost-all-dev
```

#### Building

-   On Linux Systems:

```bash
git clone https://github.com/ConcealNetwork/conceal-core
cd conceal-core
mkdir build
cd build
cmake ..
make
```

If the build is successful the binaries will be in the `src` folder.

### ARM / Raspberry Pi

Tested on a Raspberry Pi 4 with Raspberry Pi OS (32/64bit) images.

Other ARM CPU/OS combinations should be possible if the CPU supports Neon/AES.

-   Follow the Linux / Ubuntu procedure to build.

### Windows 10

#### Prerequisites

-   Install [Visual Studio 2019 Community Edition](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=16)
    > When installing Visual Studio, you need to install **Desktop development with C++** and the **MSVC v142 - VS 2019 C++ x64/x86 build tools** components. The option to install the v142 build tools can be found by expanding the "**Desktop development with C++**" node on the right. You will need this for the project to build correctly.
-   Install [CMake](https://cmake.org/download/)
-   Install [Boost](https://sourceforge.net/projects/boost/files/boost-binaries/1.78.0/boost_1_78_0-msvc-14.2-64.exe/download), **ensuring** you download the installer for **MSVC 14.2**. 
    > The instructions will be given for Boost 1.78.0. Using a different version should be supported but you will have to change the version where required.

#### Building

-   From the start menu, open 'x64 Native Tools Command Prompt for vs2019' or run "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\Tools\\VsMSBuildCmd.bat" from any command prompt.

```ps
git clone https://github.com/ConcealNetwork/conceal-core
cd conceal-core
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64 -DBOOST_ROOT="c:\local\boost_1_78_0"
msbuild concealX.sln /p:Configuration=Release /m
```

If the build is successful the binaries will be in the `src/Release` folder.

### macOS

#### Prerequisites

-   In order to install prerequisites, [XCode](https://developer.apple.com/xcode/) and [Homebrew](https://brew.sh/) needs to be installed.
    Once both are ready, open Terminal app and run the following command to install additional tools:

```bash
xcode-select --install
```

-   On newer macOS versions (v10.14 and higher) this step is done through Software Update in System Preferences.

-   After that, proceed with installing dependencies:

```bash
brew install git cmake boost
```

#### Building

-   When all dependencies are installed, build Conceal Core binaries:

```bash
git clone https://github.com/ConcealNetwork/conceal-core
cd conceal-core
mkdir build
cd build
cmake ..
make
```

If the build is successful the binaries will be located in the `src` folder.

#### Special Thanks

Special thanks goes out to the developers from Cryptonote, Bytecoin, Ryo, Monero, Forknote, TurtleCoin, Karbo and Masari.


Copyright (c) 2017-2024 Conceal Community, Conceal Network & Conceal Devs
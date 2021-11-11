![image](https://github.com/ConcealNetwork/conceal-imagery/blob/master/logos/splash.png)

# Conceal Core (CLI)
[![Ubuntu 20.04](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu20.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu20.yml)
[![Ubuntu 18.04](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu18.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/ubuntu18.yml)
[![Windows](https://github.com/ConcealNetwork/conceal-core/actions/workflows/windows.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/windows.yml)
[![macOS](https://github.com/ConcealNetwork/conceal-core/actions/workflows/macOS.yml/badge.svg)](https://github.com/ConcealNetwork/conceal-core/actions/workflows/macOS.yml)

Latest Release: v6.5.1

Maintained by Conceal Developers, overseen by Conceal Team and driven by Conceal Community.

## Information
Conceal Network is a secure peer-to-peer privacy framework empowering individuals and organizations to anonymously communicate and interact financially in a decentralized and censorship resistant ecosystem.

Conceal is based on the original Cryptonote protocol and runs on a secure peer-to-peer network technology to operate with no central authority. You control your private keys to your funds, you control your destiny. Conceal Network is accessible by anyone in the world regardless of his/her geographic location or status. Our blockchain is resistant to any kind of analysis. All your CCX transactions and messages are anonymous. Conceal Network provides an instant secure, untraceable and unlinkable way of encrypted communication - crypto messages.

Conceal is open-source, community driven and truly decentralized. No one owns it, everyone can take part.

## Resources
- Web: [https://conceal.network](https://conceal.network/)
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

### System Memory

The build process can consume upto 13GB of memory and may fail if enough resources are not available.
In some build scenarios it may be necessary to increase the size of the SWAP to compensate for less RAM.

For example if you have 8GB of RAM, then your SWAP size should be 5GB

- Ubuntu / Linux
	```bash
	sudo fallocate -l 5G /swapfile
	sudo chmod 600 /swapfile
	sudo mkswap /swapfile
	sudo swapon /swapfile
	```

- Rasberry Pi OS
	```bash
	sudo dphys-swapfile swapoff
	sudo nano /etc/dphys-swapfile
	CONF_SWAPSIZE=5120
	sudo nano /sbin/dphys-swapfile
	#CONF_MAXSWAP=2048
	sudo dphys-swapfile setup
	sudo dphys-swapfile swapon
	```

### Linux / Ubuntu / Debian

#### Prerequisites

- You will need the following dependencies to build the Conceal CLI: boost, cmake, git, gcc, g++, python, and make.
- On Ubuntu:
	```bash
	sudo apt update
	sudo apt-get install -y build-essential python-dev gcc g++ git cmake libboost-all-dev
	```

#### Building
	```bash
	git clone https://github.com/ConcealNetwork/conceal-core
	cd conceal-core
	mkdir build && cd $_
	cmake ..
	make
	```

If the build is successful the binaries will be in the `src` folder.

### ARM / Raspberry Pi

Tested on a Raspberry Pi 4 with Raspberry Pi OS (32/64bit) images.

Other ARM CPU/OS combinations should be possible if the CPU supports Neon/AES.

- Follow the Linux / Ubuntu procedure to build.

### Windows 10

#### Prerequisites

- Install [Visual Studio 2019 Community Edition](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=16)
- Install [CMake](https://cmake.org/download/)
- When installing Visual Studio, you need to install **Desktop development with C++** and the **MSVC v142 - VS 2019 C++ x64/x86 build tools** components. The option to install the v142 build tools can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
- Install [Boost 1.73.0](https://sourceforge.net/projects/boost/files/boost-binaries/1.73.0/boost_1_73_0-msvc-14.2-64.exe/download), **ensuring** you download the installer for **MSVC 14.2**.

#### Building

From the start menu, open 'x64 Native Tools Command Prompt for vs2019' or run "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsMSBuildCmd.bat" from any command prompt.

	```bash
	git clone https://github.com/ConcealNetwork/conceal-core
	cd conceal-core
	mkdir build
	cmake .. -G "Visual Studio 16 2019" -A x64 -DBOOST_LIBRARYDIR="c:\local\boost_1_73_0\lib64-msvc-14.2"
	msbuild concealX.sln /p:Configuration=Release /m
	```

If the build is successful the binaries will be in the `src/Release` folder.

### macOS

#### Prerequisites

In order to install prerequisites, [XCode](https://developer.apple.com/xcode/) and [Homebrew](https://brew.sh/) needs to be installed.
Once both are ready, open Terminal app and run the following command to install additional tools:

	```bash
	xcode-select --install
	```

On newer macOS versions (v10.14 and higher) this step is done through Software Update in System Preferences.

After that, proceed with installing dependencies:

	```bash
	brew install git python cmake gcc boost
	```


#### Building

When all dependencies are installed, build Conceal Core binaries:

	```bash
	git clone https://github.com/ConcealNetwork/conceal-core
	cd conceal-core
	mkdir build && cd $_
	cmake ..
	make
	```

If the build is successful the binaries will be located in `src` directory.

#### Special Thanks
Special thanks goes out to the developers from Cryptonote, Bytecoin, Ryo, Monero, Forknote, TurtleCoin, Karbo and Masari.

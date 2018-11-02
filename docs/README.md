Skip to content
 
Search or jump to…

Pull requests
Issues
Marketplace
Explore
 @cryptokatz Sign out
9
12 5 TheCircleFoundation/conceal-core
 Code  Issues 0  Pull requests 0  Projects 0  Wiki  Insights  Settings
conceal-core/ 
README.md
  or cancel
    
 
1
![image](https://github.com/TheCircleFoundation/conceal-assets/blob/master/splash.png)
2
​
3
# Conceal Core (CLI)
4
Latest Release: v5.0.0
5
Maintained by The Circle Team.
6
​
7
## Information
8
Conceal is a free open source privacy protected peer-to-peer digital cash system that is completely decentralized, without the need for a central server or trusted parties. Users hold the crypto keys to their own money and transact directly with each other, with the help of a P2P network to check for double-spending. Conceal has a decentralized blockchain banking in its core and instant untraceable crypto messages that can be decrypted with recipient`s private key.
9
​
10
## Resources
11
- Web: [conceal.network](https://conceal.network/)
12
- GitHub: [https://github.com/TheCircleFoundation/conceal-core](https://github.com/TheCircleFoundation/conceal-core)
13
- Discord: [https://discord.gg/YbpHVSd](https://discord.gg/YbpHVSd)
14
- Twitter: [https://twitter.com/ConcealNetwork](https://twitter.com/ConcealNetwork)
15
- Medium: [https://medium.com/@ConcealNetwork](https://medium.com/@ConcealNetwork)
16
- Reddit: [https://www.reddit.com/r/ConcealNetwork/](https://www.reddit.com/r/ConcealNetwork/)
17
- Bitcoin Talk: [https://bitcointalk.org/index.php?topic=4515873](https://bitcointalk.org/index.php?topic=4515873)
18
- Paperwallet: [https://paperwallet.conceal.network/](https://paperwallet.conceal.network/)
19
​
20
## Compiling Conceal from source
21
​
22
### Linux / Ubuntu
23
​
24
##### Prerequisites
25
​
26
- You will need the following dependencies to build the Conceal CLI: boost, cmake, git, gcc, g++, python, and make.
27
- On Ubuntu: `sudo apt-get install -y build-essential python-dev gcc g++ git cmake libboost-all-dev`
28
​
29
#### Building
30
​
31
- `git clone https://github.com/TheCircleFoundation/conceal-core`
32
- `cd conceal-core`
33
- `make build-release`
34
​
35
If the build is successful the binaries will be in the src folder.
36
​
37
### Windows 10
38
​
39
##### Prerequisites
40
​
41
- Install [Visual Studio 2017 Community Edition](https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15&page=inlineinstall)
42
- Install [CMake](https://cmake.org/download/)
43
- When installing Visual Studio, you need to install **Desktop development with C++** and the **VC++ v140 toolchain** components. The option to install the v140 toolchain can be found by expanding the "Desktop development with C++" node on the right. You will need this for the project to build correctly.
44
- Install [Boost 1.67.0](https://boost.teeks99.com/bin/1.67.0/), ensuring you download the installer for MSVC 14.1.
45
​
46
##### Building
@cryptokatz
Commit changes

Update README.md

Add an optional extended description…
  Commit directly to the master branch.
  Create a new branch for this commit and start a pull request. Learn more about pull requests.
 
© 2018 GitHub, Inc.
Terms
Privacy
Security
Status
Help
Contact GitHub
Pricing
API
Training
Blog
About
Press h to open a hovercard with more details.

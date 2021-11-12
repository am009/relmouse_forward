# Relative Mouse Forward on Windows

Send relative mouse movements and keyboard key strokes to remote computer. Used with RDP to play games.

https://docs.microsoft.com/en-us/answers/questions/238563/rdp-relative-mouse-movement-support-urgently-neede.html

Based on [GamingAnywhere](https://github.com/chunying/gaminganywhere)

远程桌面软件都用的是绝对鼠标逻辑，减少需要发生的包数目。而FPS游戏需要相对的鼠标移动，导致游戏视角无法移动。因此直接捕获本地鼠标的相对移动发送到远程。

注意要用管理员权限启动。不然无法影响高权限窗口。

### 用法

被控端启动server，设置ip为0.0.0.0 （监听）

控制端设置IP为服务器的IP，启动后出现一个小的白色窗口捕获鼠标，按下`~`键释放鼠标，同时停止发送鼠标移动数据，但是会继续发送键盘数据。

使用时先远程连接目标主机，然后启动本软件，将窗口放到角落，再设置为前台。

缺点：需要设置窗口为前台。另外在RDP还是看不到鼠标。。。

使用UDP协议

TODO 将公用代码移动到dll里，设置依赖然后共享。

TODO try next addr in getaddrinfo ctrladdr list

TODO support tcp

Done 增加命令行选项支持

Done 监听`[::]`时支持ipv6/ipv4双栈。

Done IPv6支持

Done 设置窗口透明

## log

### 自己写一个鼠标移动转发


https://thenumbat.github.io/cpp-course/sdl2/01/vsSetup.html 设置SDL开发 https://lazyfoo.net/tutorials/SDL/01_hello_SDL/windows/msvc2019/index.php 先获取鼠标位置再看怎么转发。

sdl sdk解压到项目里面，增加lib path和include path https://stackoverflow.com/questions/830271/link-to-all-visual-studio-variables 利用变量

http://www.mirrorservice.org/sites/sourceware.org/pub/pthreads-win32/ 同时安装这个库

#### 环境配置
调试-环境： `PATH=%PATH%;$(ProjectDir)sdl_lib\$(PlatformShortName);$(ProjectDir)pthread_dll\$(PlatformShortName)`

VC++目录：

包含目录：`$(ProjectDir)pthread_include`    `$(ProjectDir)sdl_include`

库目录： `$(ProjectDir)sdl_lib\$(PlatformShortName)`   `$(ProjectDir)pthread_lib\$(PlatformShortName)`

C/C++：预处理器：_WINSOCK_DEPRECATED_NO_WARNINGS  _CRT_SECURE_NO_WARNINGS

链接器：输入：SDL2.lib  SDL2main.lib  pthreadVC2.lib  Ws2_32.lib

链接器：常规：附加库目录：`$(ProjectDir)sdl_lib\$(PlatformShortName)`  `$(ProjectDir)pthread_lib\$(PlatformShortName)`

一开始极其卡顿，改client主循环，延时去掉就好了。后面遇到的是鼠标飘到窗口边缘的时候相对移动获取得有问题。这个参照 `https://gist.github.com/DanielGibson/f357c2f86734f8be39c8a758b1f900e3` 加上`SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");`就好了。然后捕获的鼠标相对移动就OK了。



## other log

<!---
兴趣与项目/远程游戏
-->

### 直接使用RDP

远程桌面软件都用的是绝对鼠标逻辑，减少需要发生的包数目。而FPS游戏需要相对的鼠标移动，导致原神的人无法移动。
其他方案是做一个小的软件，直接捕获本地鼠标的相对移动发送到远程。
https://stackoverflow.com/questions/731979/how-to-send-relative-mouse-movement-to-another-pc 这人也想写一个小软件。

https://superuser.com/questions/849918/erratic-mouse-movement-in-3d-games-over-rdp-with-remotefx 跟随这个步骤想要转发本地鼠标，但是我的Galaxy Book2可能是Windows arm的原因，看这里 https://techcommunity.microsoft.com/t5/security-compliance-and-identity/introducing-microsoft-remotefx-usb-redirection-part-3/ba-p/247085 鼠标检查驱动信息里也没有这个驱动，启动它没有效果，启用tsusbflt.sys的Boot启动之后电脑就无法开机了。。。只好手动修复。

### synergy-core

一个共享鼠标的软件，有GUI，还有人Fork：https://github.com/debauchee/barrier 

https://github.com/DEAKSoftware/Synergy-Binaries/releases 这里下载编译好的

问题在于我RDP之后控制的居然是我这边被退掉的锁屏界面的鼠标。。。

https://www.groovypost.com/reviews/sharemouse-vs-synergy-keyboard-mouse-sharing-review/ 另外一个软件


### remote session shadowing + synergy

http://woshub.com/rdp-session-shadow-to-windows-10-user/ 按照这里设置组策略。同时两边设置注册表，然后重启。还是不行。

https://docs.microsoft.com/en-us/troubleshoot/windows-server/remote/shadow-terminal-server-session 官方的设置组策略的说明

https://community.spiceworks.com/how_to/136210-use-mstsc-as-a-remote-viewer-controller debug

既然要用到RPC，那确实防火墙有关系。还是开启File and Printer Sharing试试吧。可以，关闭防火墙之后症状就变成拒绝访问了。。。加上prompt，然后输账号密码就可以了。。。
mstsc /shadow:1 /v:xxx.xxx.xxx.xxx /noConsentPrompt /control
https://www.reddit.com/r/windows/comments/gs2r1p/mstsc_shadow_as_an_rdp_file/ 这人和我一样，他用了窗口消息的方法模拟手动输入。而且这个选项好像不能保存到rdp文件，所以得直接命令行。我好像不能用保存的密码。保存的密码好像和cmdkey命令有关？？但是我不shadow就可以直接连hp.子域名，加上shadow了就必须要手动输入密码了。。。
现在的问题，1是输入密码，2是防火墙设置允许RPC，3是还要开启synergy。
然后就算都开启了，测试synergy还是失败了。。。还是不能玩原神。。。

### UltraVNC

https://forum.ultravnc.net/viewtopic.php?f=4&t=36860 这里开贴说开启Gii就可以，但是 https://github.com/ultravnc/UltraVNC/issues/12 这里说实现得不对，总之现在还是不行。

另外发现只有RDP也连着，且不是最小化的时候才行，不然直接连过去是黑屏的。。。可能是因为只是屏幕捕获吧。

### gaming anywhere
https://gaminganywhere.org/download.html 尝试
直接RDP连接开服务器的问题在于，如果我RDP连过服务器，那边session就退出了。如果用RDP的session，rdp也在传输图像，相当于传输了两遍图像。而且还有一些小问题（IPv6和鼠标指针有时消失）。真不如自己改一个转发鼠标的程序。

event driven当前的问题是：32位DLL的hook失败。

如果我开一个新账号，1是原神需要管理员权限启动，而gaming anywhere不能看到UAC界面。看能不能关了UAC或者开一个管理员权限的powershell来启动原神。
我需要离开之前登录另外一个账号，设置合盖无操作，然后按需要连接过去，在控制台启动原神然后玩。。。
而如果没有提前设置好，除非我rdp设置好session 1…

#### config文件
根据启动报错，两边修改audio-lame.conf里的采样率为48000。但是如果是RDP就需要44100。。。TODO

client和server都改使用x265编码。server.desktop.conf和client.rel.conf。

Windows ARM平板根据直接双击时报错找不到DLL的情况，从那边amd64电脑里的SysWow64文件夹里面复制32位的dll到同目录就可以了，有两个：msvcp100.dll和msvcr100.dll

服务端开启管理员权限的cmd，执行.\ga-server-periodic.exe .\config\server.desktop.conf

平板执行.\ga-client.exe .\config\client.rel.conf rtsp://xxxxxx/desktop

帧率设置在服务端 bin\config\common\video-x264.conf

Event-Driven模式：由于Hook的dll是32位的，不支持64位的原神。。。

平板这边使用rel的时候可以控制原神，而且其实控制的反应挺灵敏的，只是传输图像太慢了。
！！！切换到x264反而快了很多？？可以玩的水平了。。。之前错怪它了
经过测试，在RDP里也可以玩。但是server启动的时候读取的是rdp的屏幕大小。所以连接前设置分辨率为1080p。
研究它是怎么传输相对的鼠标位置的：https://github.com/chunying/gaminganywhere/blob/616db19c0b8101bddd7418a4723170cf83fd9710/ga/client/ga-client.cpp 这里用libsdl的SDL_WaitEvent函数获取鼠标事件。https://github.com/chunying/gaminganywhere/blob/master/ga/module/ctrl-sdl/ctrl-sdl.cpp 真就一个SendInput函数。。。

#### 分辨率问题
原来是GetSystemMetrics(SM_CXSCREEN)出问题了，因为没有设置程序为DPI aware。主机上的server在右键exe的设置里change High DPI settings勾选Override即可。
 
client也要这样设置，这样大小才正常。

其他小问题：
鼠标指针：有时候不知道为什么没有鼠标指针，需要解决鼠标指针的问题。
IPv6：现在还不支持IPv6的连接好像？？我得手动看IPv4地址。

### gaming anywhere 编译 & windows ARM

https://github.com/chunying/gaminganywhere 

https://gaminganywhere.org/doc/quick_start.html HOW TO BUILD on Win32 platform

https://docs.microsoft.com/en-us/windows/win32/directx-sdk--august-2009- directX sdk现在是windows SDK的一部分？？https://github.com/apitrace/apitrace/wiki/DirectX-SDK 

https://gaminganywhere.org/doc/quick_start.html 这里有win32的构建说明

#### 编译

直接执行deps.pkg.win32\install64.cmd。然后到外面开启x64环境，执行nmake /f NMakefile all。

报错发现要注释掉pthread.h里面的320行处的timespec定义。

然后保存说libliveMedia版本不对，看来要重新编译live555

还是要下载https://www.microsoft.com/en-gb/download/confirmation.aspx?id=6812 然后安装到C:\Microsoft DirectX SDK即可。

解决一些符号连接的问题就直接复制。

ga_client.cpp顶上增加如下
```
#ifdef __cplusplus
extern "C" {
#endif
FILE* __cdecl __iob_func(unsigned i) {
    return __acrt_iob_func(i);
}
#ifdef __cplusplus
}
#endif
```

### 旧版本构建

下面是官网直接下载的环境版本遇到的问题：然而Github上已经基本支持64位了，白搞了。

https://github.com/wongdu/refGaming-anywhere/commit/46bf5b8a715fec4dd0035b53851f06370dd92967 这人支持Debug的commit

ntwin32.mak找不到

https://github.com/chunying/gaminganywhere/issues/87 要自己去SDK里下载。。。下载第一个ISO然后执行"E:\Setup\WinSDKBuild\WinSDKBuild_x86.msi"就自动安装了。。。C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1\Include里，放到"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133\include"里去了。

https://www.microsoft.com/en-us/download/confirmation.aspx?id=8442 

deps.win32\include\pthread.h 320行附近的struct timespec定义注释掉。

报错说库的类型是x86和x64不对。nmake /f NMakefile clean清理下。原来构建其他架构的主要问题在库啊。。。

改ga\NMakefile.def里的CXX直接等于cl。因为build之前启动了x64的环境配置bat。

### x264编译

根据docs\BUILD.extra.win32的指示

直接configure说找不到yasm。先安装：启动Msys2的控制台，然后通过/c/目录访问c盘，到

```
cd /c/Users/warren/d/2021/gaminganywhere-0.8.0/deps.pkg.win64/x264-snapshot-20140330-2245
pacman -S mingw-w64-x86_64-yasm
./configure --prefix=../x264bin --enable-shared --extra-ldflags=-Wl,--output-def=libx264.def
```

make的第一步出现奇怪的报错，把makefile里面的@去掉之后把命令复制出来一个个执行没有问题。。。可能是撞到了命令行最大长度然后截断导致只有单独的-号了吧。。。？？还是手动继续执行补全.deps

下面切换到cmd，根据def文件创建导入库lib？。

```
%comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
lib /def:libx264.def
copy libx264.lib ..\x264bin\lib
```
再切换回去
```
Cd ..
pacman -S zip
zip -ur9 x264-20140330-2245.zip x264bin
```
### live555

根据这里docs\BUILD.live555编译。
先复制patch到源码目录里面，然后msys执行：
```
pacman -S patch
patch -p 1 < ./live555-sockbuf-size.diff
```
1.	修改deps.pkg.win64\live\win32config：（路径末尾千万不能留反斜杠！！）
```
TOOLS32	=		C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133
C_COMPILER =		"cl"
rc32 = "rc"
```
2.	编译
```
.\genWindowsMakefiles.cmd
cd UsageEnvironment && nmake -f UsageEnvironment.mak
cd ..\groupsock && nmake -f groupsock.mak
cd ..\liveMedia && nmake -f liveMedia.mak
cd ..\BasicUsageEnvironment && nmake -f BasicUsageEnvironment.mak
```
3.	msys里去复制文件
```
# 假设现在在live文件夹外
mkdir -p live555/include
mkdir -p live555/lib
find live -name '*.lib' -exec cp {} live555/lib/ \;
find live -name '*.obj' -exec rm -f {} \;
find live -name '*.lib' -exec rm -f {} \;
```
4.	继续编译debug版本的，首先注释win32config开头的NODEBUG=1，然后重新执行编译脚本
5.	msys里再次复制文件
```
export LIVELIB='UsageEnvironment groupsock liveMedia BasicUsageEnvironment'
for l in $LIVELIB; do cp live/$l/lib$l.lib live555/lib/lib$l.d.lib; done
unset LIVELIB
find live -name '*.hh' -exec cp -f {} live555/include/ \;
cp live/groupsock/include/NetCommon.h live555/include/
zip -ur9 live.2014.03.25-bin.zip live555
```

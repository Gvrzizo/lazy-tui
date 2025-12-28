# Learning At ZJU Yes TUI Front-End

`lazy-tui`是一个基于 C++ 和 ftxui 库的 [LAZY](https://github.com/YangShu233-Snow/Learning_at_ZJU_third_client) TUI。

## 安装

### Linux/MacOS

首先确保你成功安装了[lazy cli](https://github.com/YangShu233-Snow/Learning_at_ZJU_third_client)，可以在终端直接使用其命令。

#### Arch 及其衍生发行版

可以直接通过 AUR 安装。
```sh
//If you are using yay
yay -S lazy-tui-git
//If you are using paru
paru -S lazy-tui-git
```

#### 其他发行版/MacOS

可以通过 Release 或下面的指令获得构建
```sh
git clone https://github.com/Gvrzizo/lazy-tui.git
cd lazy-tui

mkdir build
cd build

cmake ..

make -j
```
将二进制文件放在环境变量已经设定好的合适位置。之后就可以使用`lazytui`命令。

### Windows

有条件可以自行构建，也可以下载 Release 中的预构建二进制文件。将`lazytui`文件所在路径添加到环境变量`PATH`中即可。

## Todo

- [ ] 多文件提交
- [ ] 课程的目录、资源查看
- [ ] 云盘的管理
- [ ] 更用户友好的界面

# Seafile-Server结构
![seafile-server项目结构](https://img-blog.csdnimg.cn/7db0a32af71f4ebca05069d494bf72bc.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_20,color_FFFFFF,t_70,g_se,x_16)

> 以下内容为编译相关

- [autogen.sh](https://github.com/haiwen/seafile-server/blob/master/autogen.sh)
    
    用于自动生成makefile、configure的脚本。

    完整的工具链详见[Wiki - GNU Autotools](https://en.wikipedia.org/wiki/GNU_Autotools)，流程图如下：

    ![在这里插入图片描述](https://img-blog.csdnimg.cn/e681e4319c9e469d876878c0d5f907ba.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBAY3lrMDYyMA==,size_14,color_FFFFFF,t_70,g_se,x_16#pic_center)


- [configure.ac](https://github.com/haiwen/seafile-server/blob/master/configure.ac)

    编译配置。
    
    可以从中看到编译该项目所需的全部Makefile.am，进而确认哪些目录中的源代码需要被编译。

- [Makefile.am](https://github.com/haiwen/seafile-server/blob/master/Makefile.am)

    编译规则。
    
    该文件与configure.ac结合生成Makefile.in。在随后的过程中通过一系列步骤生成Makefile。

    可以从中看到编译某部分内容所需要的所有源代码，以及该部分源代码的类型（库文件、头文件、源文件等）。

- ## [m4](https://github.com/haiwen/seafile-server/blob/master/m4)

    与autoconf有关。

> 以下内容为开源相关

- [LICENSE.txt](https://github.com/haiwen/seafile-server/blob/master/LICENSE.txt)
    
    开源证书。

- [README.markdown](https://github.com/haiwen/seafile-server/blob/master/README.markdown)
    
    项目介绍与说明。

- [README.testing.md](https://github.com/haiwen/seafile-server/blob/master/README.testing.md)

    测试说明。

- [run_tests.sh](https://github.com/haiwen/seafile-server/blob/master/run_tests.sh)
    
    测试脚本。(调用[ci/run.py](https://github.com/haiwen/seafile-server/blob/master/ci/run.py))

- [updateversion.sh](https://github.com/haiwen/seafile-server/blob/master/updateversion.sh)
    
    升级脚本。

- ## [ci](/https://github.com/haiwen/seafile-server/blob/master/ci)

    持续集成（Continuous integration），利用的是Github Actions。（具体配置在“[.github/workflows](https://github.com/haiwen/seafile-server/blob/master/.github/workflows)”下）

    - [install-deps.sh](https://github.com/haiwen/seafile-server/blob/master/ci/install-deps.sh)

        安装依赖项。

    - [run.py](https://github.com/haiwen/seafile-server/blob/master/ci/run.py)

        自动化安装、自动化测试。

    - [serverctl.py](https://github.com/haiwen/seafile-server/blob/master/ci/serverctl.py)

        被run.py调用，自动化配置seafile-server。

    - [utils.py](https://github.com/haiwen/seafile-server/blob/master/ci/utils.py)

        向前两者封装了所需的实用方法（主要是系统命令）。

    从“[.github/workflows/ci.yml](https://github.com/haiwen/seafile-server/blob/master/.github/workflows/ci.yml)”中可以得知CI的具体流程为：

    1. 进入工作目录
		```shell
		cd $GITHUB_WORKSPACE
		```
    2. 安装依赖项
   		```shell
		./ci/install-deps.sh
		```
    3. 安装并测试。
		```shell
		./ci/run.py
		```

- ## [scripts](https://github.com/haiwen/seafile-server/blob/master/scripts)

    各种用于安装、配置、管理的脚本。

- ## [doc](https://github.com/haiwen/seafile-server/blob/master/doc)
    
    文档。

- ## [tools](https://github.com/haiwen/seafile-server/blob/master/tools)

    目前只有seafile-admin，一个协助安装与管理的工具。

> 以下内容为客户端源代码

- ## [python](https://github.com/haiwen/seafile-server/blob/master/python)

    通过python实现了seafile服务端对应的的RPC客户端。

	此子项目依赖pysearpc，一个Searpc的Python Binding（Python调用C库）。详见【[libsearpc](https://github.com/haiwen/libsearpc)】。

    - [seafile](https://github.com/haiwen/seafile-server/blob/master/python/seafile)

        初步实现了一个RPC客户端。可以连接到指定的RPC服务端。

    - [seaserv](https://github.com/haiwen/seafile-server/blob/master/python/seaserv)

        进一步封装RPC客户端，并加入了异常处理。可以通过环境变量以及配置文件来连接到指定的RPC服务端。

> 以下内容为C语言实现的服务端源代码

- ## [lib](https://github.com/haiwen/seafile-server/blob/master/lib)

    库文件。

- ## [include](https://github.com/haiwen/seafile-server/blob/master/include)

    头文件。

- ## [common](https://github.com/haiwen/seafile-server/blob/master/common)

    通用源文件。

- ## [fuse](https://github.com/haiwen/seafile-server/blob/master/fuse)

    C实现的用户空间文件系统(Filesystem in Userspace)。

- ## [server](https://github.com/haiwen/seafile-server/blob/master/server)

    C实现的服务端核心。分为Seaf API(RPC协议)和Http API(Http协议)。

    其中Seaf API用于向Seahub提供文件访问服务，Http API用于向桌面客户端提供文件同步服务。

> 以下内容为Go实现的服务端源代码

- ## [fileserver](https://github.com/haiwen/seafile-server/blob/master/fileserver)

   Go实现的服务端核心，同样也分为Seaf API和Http API。同时还包含了Go实现的用户空间文件系统。

- ## [controller](https://github.com/haiwen/seafile-server/blob/master/controller)

    启动seafile-server、ccnet-server、seafile-monitor。修复seafile-server和ccnet-server的进程。

> 以下内容为测试脚本

- ## [tests](https://github.com/haiwen/seafile-server/blob/master/tests)

    通过python实现的RPC客户端进行功能测试。
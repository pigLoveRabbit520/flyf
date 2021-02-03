# flyf
简单FTP客户端，取名含义fly+ftp，飞快的ftp

# 常见FTP命令
* `open` - 连接ftp服务器。
* `help`或?- 列出所有可用的FTP命令。
* `cd` - 切除远程计算机上的目录。
* `lcd` - 切换本地计算机上的目录。
* `ls` - 列出当前远程目录中的文件和目录的名称。
* `mkdir` - 在当前远程目录中创建一个新目录。
* `pwd` - 打印远程计算机上的当前工作目录。
* `delete` - 删除当前远程目录中的文件。
* `rmdir` - 删除当前远程目录中的目录。
* `get` - 将一个文件从远程复制到本地计算机。
* `mget` - 将多个文件从远程复制到本地计算机。
* `put` - 将一个文件从本地复制到远程计算机。
* `mput` - 将多个文件从本地复制到远程计算机。


# 已支持的命令
* ls     显示服务端文件列表
* lls    显示本地文件列表
* pwd    显示服务端工作目录
* lpwd   显示本地工作目录
* cd     更改服务端工作目录
* lcd    更改本地工作目录
* get    下载文件
* put    上传文件
* delete 删除服务端文件
* ascii  更改传输模式为ASCII
* binary 更改传输模式为BINARY
* mkdir  服务端新建文件夹
* help   帮助信息
* open   连接服务端
* exit   退出程序

# 编译
make

# 问题总结
* 打开data socket后（passive mode），IIS FTP server对`LIST`（其他要传输数据的命令也一样）返回`125`，vsftpd返回`150`
* close不一定会立即关闭socket发送FIN段（譬如socket被不同进程共享），可以用shutdown解决
* TCP是流式协议，接收端收到的数据不一定等于发送端发送的数据
* FTP会有多行消息，三位数字编码的响应码加上`-`，结束行就是该响应码加上空格


# TODO
* fix bug
* 优化代码结构

# 参考文章
* [在linux下使用FTP命令传输文件](https://www.myfreax.com/transfer-files-using-ftp-command-under-linux/)

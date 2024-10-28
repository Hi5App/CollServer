## Generate protobuf&grpc cpp source code

1. 首先找到vcpkg_installed所在目录，在vcpkg清单模式下该目录会被vcpkg自动放到构建目录下，例如：build/Debug/vcpkg_installed

2. Linux下，执行include/SwcDbmsCommon下的InstallPwsh.sh脚本安装Powershell，记得赋予可执行权限：chmod +x InstallPwsh.sh

3. 在include/SwcDbmsCommon目录下找到GenCpp.ps1脚本文件，命令行执行pwsh进入Powershell命令行

4. 在Powershell命令行终端，执行命令 ./GenCpp.ps1 -ProtobufPath "{你的vcpkg_installed所在路径}/x64-windows/tools/protobuf" -gRPCPath "{你的vcpkg_installed所在路径}/x64-windows/tools/grpc"

例如：

```pwsh
./GenCpp.ps1 -ProtobufPath "../../build/Debug/vcpkg_installed/x64-windows/tools/protobuf" -gRPCPath "../../build/Debug/vcpkg_installed/x64-windows/tools/grpc"
```

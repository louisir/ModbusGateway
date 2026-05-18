# ModbusGateway

中文 | [English](#modbusgateway-english)

ModbusGateway 是一个基于 Qt 的 Modbus 软网关工具，用于在 Modbus TCP 与 Modbus RTU 之间转发请求与响应。程序支持“上位机 TCP -> 下位机 RTU”和“上位机 RTU -> 下位机 TCP”两种模式，可配置串口参数、TCP 监听或目标地址，并显示 TCP/RTU 两侧的实时收发帧。

## 最新版本下载

- 最新版本：`v0.7`
- Windows 便携包：[ModbusGateway-v0.7-win64.zip](https://github.com/louisir/ModbusGateway/releases/download/v0.7/ModbusGateway-v0.7-win64.zip)
- 源码包：[v0.7 source code](https://github.com/louisir/ModbusGateway/archive/refs/tags/v0.7.zip)
- 全部发布版本：[GitHub Releases](https://github.com/louisir/ModbusGateway/releases)

## 功能特性

- 支持“上位机 TCP -> 下位机 RTU”：Modbus TCP 主站访问 Modbus RTU 从站。
- 支持“上位机 RTU -> 下位机 TCP”：Modbus RTU 主站访问 Modbus TCP 从站。
- TCP 侧按 MBAP 长度字段解析帧，支持 TCP 半包和粘包。
- RTU 侧自动追加/校验 CRC16。
- RTU 请求按队列串行发送，避免多个 TCP 请求并发打到同一条 RTU 总线。
- 支持常用功能码：
  - `0x01` 读线圈
  - `0x02` 读离散输入
  - `0x03` 读保持寄存器
  - `0x04` 读输入寄存器
  - `0x05` 写单个线圈
  - `0x06` 写单个保持寄存器
  - `0x0F` 写多个线圈
  - `0x10` 写多个保持寄存器
- 支持 Modbus 异常响应帧。
- RTU 或 TCP 从站响应超时时返回网关异常响应。
- GUI 实时显示 TCP 与 RTU 两侧的收发方向、时间戳和十六进制数据。
- RTU/TCP 日志列表支持单选、多选、框选、`Ctrl+C` 复制和分别清空。

## 当前约束

- “上位机 TCP -> 下位机 RTU”模式下，TCP server 一次只接受一个 TCP client 连接；新的连接会被拒绝。
- “上位机 RTU -> 下位机 TCP”模式下，程序作为 TCP client 连接一个 TCP 从站，同一时间只处理一个 RTU 主站请求。
- RTU/TCP 响应超时时间当前固定为 2000 ms。
- “上位机 RTU -> 下位机 TCP”模式不转发 RTU 广播地址 `0`。
- 工程是 Qt Widgets 应用，使用 qmake 构建。
- 未内置自动化协议测试或串口模拟器。

## 环境要求

- Qt 5.15 或 Qt 6.x
- Qt modules:
  - Core
  - Gui
  - Widgets
  - Network
  - SerialPort
- 支持 C++17 的编译器

本仓库已在 Windows + Qt 6.8.3 + MinGW 环境下构建通过。

## 构建方法

### Qt Creator

1. 用 Qt Creator 打开 `ModbusGateway.pro`。
2. 选择带有 `Qt SerialPort` 模块的 Qt Kit。
3. 执行 Run qmake。
4. Build 或 Run。

### 命令行

Windows 示例：

```powershell
qmake ModbusGateway.pro
mingw32-make -j2
```

如果 `qmake` 或 `mingw32-make` 不在 `PATH` 中，请使用 Qt 安装目录里的完整路径。

## 一键打包

Windows 下可直接运行：

```powershell
.\package-release.bat -Version v0.7 -Clean
```

脚本会执行 release 构建、调用 `windeployqt` 收集 Qt 运行时依赖，并生成：

```text
dist/ModbusGateway-v0.7-win64.zip
```

如果需要同时推送 tag 并创建 GitHub Release，可在已安装并登录 `gh` CLI，或设置 `GH_TOKEN`/`GITHUB_TOKEN` 后运行：

```powershell
.\package-release.bat -Version v0.7 -Clean -Publish
```

发布新版本时，需要同步更新本 README 的“最新版本下载”和“一键打包”示例中的版本号、下载链接和产物文件名。

## 使用方法

1. 启动程序。
2. 在模式 radio 中选择“上位机 TCP -> 下位机 RTU”或“上位机 RTU -> 下位机 TCP”。
3. 在 RTU 配置区选择串口号、波特率、数据位、停止位、校验位和流控制。
4. 在 TCP 配置区填写 IP 和端口。“上位机 TCP -> 下位机 RTU”模式表示本机监听地址；“上位机 RTU -> 下位机 TCP”模式表示目标 TCP 从站地址。默认端口为 `502`。
5. 点击“运行”启动网关。
6. “上位机 TCP -> 下位机 RTU”模式下，将 Modbus TCP 主站连接到程序监听的 IP 和端口。
7. “上位机 RTU -> 下位机 TCP”模式下，将 Modbus RTU 主站连接到所选串口，程序会主动连接配置的 Modbus TCP 从站。
8. 点击“停止”关闭网关。

界面上方列表显示 RTU 侧数据，下方列表显示 TCP 侧数据。`<-` 表示发出，`->` 表示收到。
两个列表都可以单选、多选或框选日志项，选中后按 `Ctrl+C` 复制；RTU/TCP 配置面板内的“清空 RTU 日志”和“清空 TCP 日志”按钮可分别清空对应列表。

## 协议转换说明

上位机 TCP -> 下位机 RTU：

- 解析 Modbus TCP MBAP 头。
- 保留 Unit Identifier、Function Code 和 PDU 数据作为 RTU ADU。
- 自动计算并追加 Modbus RTU CRC16。
- 请求进入 RTU 队列，等待前一个请求响应或超时后再发送下一个请求。
- RTU 响应校验地址、功能码和 CRC16 后，去掉 CRC 并使用对应 TCP 请求的 Transaction Identifier 返回给 TCP 主站。

上位机 RTU -> 下位机 TCP：

- 校验来自串口的 RTU 请求地址、功能码和 CRC16。
- 去掉 RTU CRC，生成 MBAP 头和 Transaction Identifier，发送给配置的 TCP 从站。
- TCP 响应必须匹配当前 Transaction Identifier 和 RTU 请求内容。
- TCP 响应转换为 RTU 响应时自动追加 CRC16，并写回串口。

## 目录结构

```text
.
├── main.cpp
├── mainwindow.*
├── modbus_rtu_worker.*
├── modbus_tcp_worker.*
├── transfer.*
├── modbusrtuwidget.*
├── modbustcpwidget.*
├── modbusassistant.*
├── *.ui
├── resource.qrc
└── res/
```

关键文件：

- `modbus_tcp_worker.*`: TCP server/client、TCP 帧接收、事务 ID 匹配和 TCP 收发。
- `modbus_rtu_worker.*`: 串口收发、RTU CRC、RTU 帧解析、请求队列和超时处理。
- `transfer.*`: Modbus TCP 和 Modbus RTU ADU 的转换。
- `mainwindow.*`: 主界面、启动停止逻辑和日志显示。

## 常见问题

### 程序启动失败

检查串口是否被其他程序占用。“上位机 TCP -> 下位机 RTU”模式还需要检查 TCP 端口是否已被占用；Windows 下监听 `502` 端口可能需要管理员权限。“上位机 RTU -> 下位机 TCP”模式需要确认目标 TCP 从站可以连接。

### TCP 主站连接不上

确认选择了正确的本机 IP 地址，并检查防火墙是否允许程序监听该端口。

### RTU 设备无响应

检查串口号、波特率、数据位、停止位、校验位、485 转换器接线以及从站地址。也可以通过界面的 RTU 收发日志确认请求是否已经发出。

### TCP 从站连接失败

在“上位机 RTU -> 下位机 TCP”模式下，确认 TCP 配置区填写的是目标 TCP 从站 IP 和端口，并检查网络、防火墙和从站服务状态。

### 多个 TCP 主站同时连接

当前实现只支持一个 TCP client。需要多主站场景时，应扩展 TCP client 管理和 RTU 请求调度策略。

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file.

---

# ModbusGateway English

[中文](#modbusgateway) | English

ModbusGateway is a Qt-based Modbus software gateway for forwarding requests and responses between Modbus TCP and Modbus RTU. It supports both `TCP master -> RTU slave` and `RTU master -> TCP slave` modes, with GUI settings for the serial port, TCP listen or target address, and real-time TCP/RTU frame logs.

## Latest Release Download

- Latest version: `v0.7`
- Windows portable package: [ModbusGateway-v0.7-win64.zip](https://github.com/louisir/ModbusGateway/releases/download/v0.7/ModbusGateway-v0.7-win64.zip)
- Source archive: [v0.7 source code](https://github.com/louisir/ModbusGateway/archive/refs/tags/v0.7.zip)
- All releases: [GitHub Releases](https://github.com/louisir/ModbusGateway/releases)

## Features

- `TCP master -> RTU slave`: a Modbus TCP master accesses Modbus RTU slave devices.
- `RTU master -> TCP slave`: a Modbus RTU master accesses a Modbus TCP slave device.
- TCP frame parsing based on the MBAP length field, including TCP packet fragmentation and coalescing.
- Automatic Modbus RTU CRC16 append and validation.
- Serialized RTU request queue to prevent multiple TCP requests from being sent to the same RTU bus at the same time.
- Common function codes:
  - `0x01` Read Coils
  - `0x02` Read Discrete Inputs
  - `0x03` Read Holding Registers
  - `0x04` Read Input Registers
  - `0x05` Write Single Coil
  - `0x06` Write Single Register
  - `0x0F` Write Multiple Coils
  - `0x10` Write Multiple Registers
- Modbus exception response support.
- Gateway exception response on RTU or TCP slave response timeout.
- GUI logs for TCP and RTU frame direction, timestamp, and hexadecimal payload.
- RTU/TCP log lists support single selection, multi-selection, drag selection, `Ctrl+C` copy, and separate clearing.

## Current Limitations

- In `TCP master -> RTU slave` mode, the TCP server accepts only one TCP client at a time. Additional connections are rejected.
- In `RTU master -> TCP slave` mode, the application acts as a TCP client connected to one TCP slave and handles one outstanding RTU master request at a time.
- The RTU/TCP response timeout is currently fixed at 2000 ms.
- `RTU master -> TCP slave` mode does not forward RTU broadcast address `0`.
- The project is a Qt Widgets application built with qmake.
- No built-in automated protocol test suite or serial port simulator is included.

## Requirements

- Qt 5.15 or Qt 6.x
- Qt modules:
  - Core
  - Gui
  - Widgets
  - Network
  - SerialPort
- A compiler with C++17 support

This repository has been verified on Windows with Qt 6.8.3 and MinGW.

## Build

### Qt Creator

1. Open `ModbusGateway.pro` in Qt Creator.
2. Select a Qt Kit that includes the `Qt SerialPort` module.
3. Run qmake.
4. Build or run the project.

### Command Line

Windows example:

```powershell
qmake ModbusGateway.pro
mingw32-make -j2
```

If `qmake` or `mingw32-make` is not available in `PATH`, use the full path from your Qt installation.

## One-Command Packaging

On Windows, run:

```powershell
.\package-release.bat -Version v0.7 -Clean
```

The script builds the release executable, runs `windeployqt` to collect Qt runtime dependencies, and creates:

```text
dist/ModbusGateway-v0.7-win64.zip
```

To also push the tag and create a GitHub Release, install and authenticate the `gh` CLI, or set `GH_TOKEN`/`GITHUB_TOKEN`, then run:

```powershell
.\package-release.bat -Version v0.7 -Clean -Publish
```

When publishing a new version, also update the version number, download links, and artifact filename in this README's "Latest Release Download" and "One-Command Packaging" sections.

## Usage

1. Start the application.
2. Select `上位机 TCP -> 下位机 RTU` or `上位机 RTU -> 下位机 TCP` with the mode radio buttons.
3. Select the serial port, baud rate, data bits, stop bits, parity, and flow control in the RTU panel.
4. Enter the TCP IP and port in the TCP panel. In `TCP master -> RTU slave` mode this is the local listen address; in `RTU master -> TCP slave` mode this is the target TCP slave address. The default port is `502`.
5. Click `运行` to start the gateway.
6. In `TCP master -> RTU slave` mode, connect your Modbus TCP master to the selected listen IP and port.
7. In `RTU master -> TCP slave` mode, connect your Modbus RTU master to the selected serial port. The application connects to the configured Modbus TCP slave.
8. Click `停止` to stop the gateway.

The upper list shows RTU traffic, and the lower list shows TCP traffic. `<-` means sent, and `->` means received.
Both lists support selecting one or more log items, drag selection, and `Ctrl+C` copy. The `清空 RTU 日志` and `清空 TCP 日志` buttons inside the RTU/TCP configuration panels clear the corresponding list.

## Conversion Behavior

TCP master -> RTU slave:

- Parse the Modbus TCP MBAP header.
- Keep Unit Identifier, Function Code, and PDU data as the RTU ADU.
- Calculate and append the Modbus RTU CRC16.
- Queue RTU requests and send the next request only after the previous response or timeout.
- Validate the RTU response address, function code, and CRC16, remove the CRC, and return it to the TCP master with the matching Transaction Identifier.

RTU master -> TCP slave:

- Validate the RTU request address, function code, and CRC16 from the serial side.
- Remove the RTU CRC, generate an MBAP header and Transaction Identifier, and send the request to the configured TCP slave.
- The TCP response must match the current Transaction Identifier and RTU request.
- The TCP response is converted back to RTU with CRC16 and written to the serial port.

## Project Layout

```text
.
├── main.cpp
├── mainwindow.*
├── modbus_rtu_worker.*
├── modbus_tcp_worker.*
├── transfer.*
├── modbusrtuwidget.*
├── modbustcpwidget.*
├── modbusassistant.*
├── *.ui
├── resource.qrc
└── res/
```

Important files:

- `modbus_tcp_worker.*`: TCP server/client, TCP frame reception, transaction ID matching, and TCP I/O.
- `modbus_rtu_worker.*`: Serial I/O, RTU CRC, RTU frame parsing, request queue, and timeout handling.
- `transfer.*`: Conversion between Modbus TCP and Modbus RTU ADUs.
- `mainwindow.*`: Main window, start/stop control, and traffic logs.

## Troubleshooting

### Startup fails

Check whether the serial port is already used by another program. In `TCP master -> RTU slave` mode, also check whether the TCP port is already occupied; on Windows, listening on port `502` may require administrator privileges. In `RTU master -> TCP slave` mode, confirm that the target TCP slave is reachable.

### TCP master cannot connect

Make sure the selected local IP address is correct and that your firewall allows the application to listen on the selected port.

### RTU device does not respond

Check the serial port, baud rate, data bits, stop bits, parity, RS-485 adapter wiring, and slave address. The RTU log in the GUI can help confirm whether requests are being sent.

### TCP slave connection fails

In `RTU master -> TCP slave` mode, make sure the TCP panel contains the target TCP slave IP and port, and check the network, firewall, and slave service status.

### Multiple TCP masters

The current implementation supports only one TCP client. Multi-master support requires extending TCP client management and RTU request scheduling.

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file.

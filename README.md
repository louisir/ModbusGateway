# ModbusGateway

中文 | [English](#modbusgateway-english)

ModbusGateway 是一个基于 Qt 的 Modbus 软网关工具，用于在 Modbus TCP 主站和 Modbus RTU 从站设备之间转发请求与响应。程序提供图形界面，可配置串口参数、TCP 监听地址和端口，并显示 TCP/RTU 两侧的实时收发帧。

## 功能特性

- Modbus TCP 到 Modbus RTU 双向转换。
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
- RTU 响应超时时向 TCP 主站返回网关异常响应。
- GUI 实时显示 TCP 与 RTU 两侧的收发方向、时间戳和十六进制数据。

## 当前约束

- 当前 TCP server 一次只接受一个 TCP client 连接；新的连接会被拒绝。
- RTU 响应超时时间当前固定为 2000 ms。
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
.\package-release.bat -Version v0.4 -Clean
```

脚本会执行 release 构建、调用 `windeployqt` 收集 Qt 运行时依赖，并生成：

```text
dist/ModbusGateway-v0.4-win64.zip
```

如果需要同时推送 tag 并创建 GitHub Release，可在已安装并登录 `gh` CLI，或设置 `GH_TOKEN`/`GITHUB_TOKEN` 后运行：

```powershell
.\package-release.bat -Version v0.4 -Clean -Publish
```

## 使用方法

1. 启动程序。
2. 在 RTU 配置区选择串口号、波特率、数据位、停止位、校验位和流控制。
3. 在 TCP 配置区选择本机监听 IP 和端口，默认端口为 `502`。
4. 点击“运行”启动网关。
5. 将 Modbus TCP 主站连接到程序监听的 IP 和端口。
6. 程序会把 TCP 请求转换为 RTU 帧发往串口，并把 RTU 响应转换回 TCP 响应。
7. 点击“停止”关闭网关。

界面上方列表显示 RTU 侧数据，下方列表显示 TCP 侧数据。`<-` 表示发出，`->` 表示收到。

## 协议转换说明

TCP 到 RTU：

- 解析 Modbus TCP MBAP 头。
- 保留 Unit Identifier、Function Code 和 PDU 数据作为 RTU ADU。
- 自动计算并追加 Modbus RTU CRC16。
- 请求进入 RTU 队列，等待前一个请求响应或超时后再发送下一个请求。

RTU 到 TCP：

- 校验 RTU 地址、功能码和 CRC16。
- 去掉 RTU CRC。
- 生成 MBAP 头。
- 使用对应 TCP 请求的 Transaction Identifier 返回给主站。

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

- `modbus_tcp_worker.*`: TCP server、TCP 帧接收、事务 ID 匹配和 TCP 响应发送。
- `modbus_rtu_worker.*`: 串口收发、RTU CRC、RTU 帧解析、请求队列和超时处理。
- `transfer.*`: Modbus TCP 和 Modbus RTU ADU 的转换。
- `mainwindow.*`: 主界面、启动停止逻辑和日志显示。

## 常见问题

### 程序启动失败

检查串口是否被其他程序占用，或 TCP 端口是否已经被占用。Windows 下监听 `502` 端口可能需要管理员权限。

### TCP 主站连接不上

确认选择了正确的本机 IP 地址，并检查防火墙是否允许程序监听该端口。

### RTU 设备无响应

检查串口号、波特率、数据位、停止位、校验位、485 转换器接线以及从站地址。也可以通过界面的 RTU 收发日志确认请求是否已经发出。

### 多个 TCP 主站同时连接

当前实现只支持一个 TCP client。需要多主站场景时，应扩展 TCP client 管理和 RTU 请求调度策略。

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file.

---

# ModbusGateway English

[中文](#modbusgateway) | English

ModbusGateway is a Qt-based Modbus software gateway. It forwards requests and responses between a Modbus TCP master and Modbus RTU slave devices. The application provides a GUI for serial port settings, TCP listening settings, and real-time TCP/RTU frame logs.

## Features

- Bidirectional Modbus TCP to Modbus RTU conversion.
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
- Gateway exception response on RTU response timeout.
- GUI logs for TCP and RTU frame direction, timestamp, and hexadecimal payload.

## Current Limitations

- The TCP server accepts only one TCP client at a time. Additional connections are rejected.
- The RTU response timeout is currently fixed at 2000 ms.
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
.\package-release.bat -Version v0.4 -Clean
```

The script builds the release executable, runs `windeployqt` to collect Qt runtime dependencies, and creates:

```text
dist/ModbusGateway-v0.4-win64.zip
```

To also push the tag and create a GitHub Release, install and authenticate the `gh` CLI, or set `GH_TOKEN`/`GITHUB_TOKEN`, then run:

```powershell
.\package-release.bat -Version v0.4 -Clean -Publish
```

## Usage

1. Start the application.
2. Select the serial port, baud rate, data bits, stop bits, parity, and flow control in the RTU panel.
3. Select the local TCP listen IP and port in the TCP panel. The default port is `502`.
4. Click `运行` to start the gateway.
5. Connect your Modbus TCP master to the selected IP and port.
6. The application converts TCP requests to RTU frames and converts RTU responses back to TCP responses.
7. Click `停止` to stop the gateway.

The upper list shows RTU traffic, and the lower list shows TCP traffic. `<-` means sent, and `->` means received.

## Conversion Behavior

TCP to RTU:

- Parse the Modbus TCP MBAP header.
- Keep Unit Identifier, Function Code, and PDU data as the RTU ADU.
- Calculate and append the Modbus RTU CRC16.
- Queue RTU requests and send the next request only after the previous response or timeout.

RTU to TCP:

- Validate RTU address, function code, and CRC16.
- Remove the RTU CRC.
- Generate a new MBAP header.
- Return the response with the matching TCP Transaction Identifier.

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

- `modbus_tcp_worker.*`: TCP server, TCP frame reception, transaction ID matching, and TCP response sending.
- `modbus_rtu_worker.*`: Serial I/O, RTU CRC, RTU frame parsing, request queue, and timeout handling.
- `transfer.*`: Conversion between Modbus TCP and Modbus RTU ADUs.
- `mainwindow.*`: Main window, start/stop control, and traffic logs.

## Troubleshooting

### Startup fails

Check whether the serial port is already used by another program or whether the TCP port is already occupied. On Windows, listening on port `502` may require administrator privileges.

### TCP master cannot connect

Make sure the selected local IP address is correct and that your firewall allows the application to listen on the selected port.

### RTU device does not respond

Check the serial port, baud rate, data bits, stop bits, parity, RS-485 adapter wiring, and slave address. The RTU log in the GUI can help confirm whether requests are being sent.

### Multiple TCP masters

The current implementation supports only one TCP client. Multi-master support requires extending TCP client management and RTU request scheduling.

## License

This project is licensed under the terms of the [LICENSE](LICENSE) file.

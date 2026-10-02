```
# OpenYR

[![GPL v3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://opensource.org/license/GPL-3.0)

---

🌐 **Language / 语言**：[English](#english) | [中文](#chinese)

---

# English {#english}

> **Disclaimer**: OpenYR is an independent, open-source, **non-commercial** project created solely for educational and research purposes. It is **not affiliated with, endorsed by, sponsored by, or in any way associated with Electronic Arts Inc. or its licensors**. "Command & Conquer: Yuri's Revenge" and all related titles, trademarks, and intellectual property are the exclusive property of Electronic Arts Inc.
>
> This project is a **reverse-engineering-based reimplementation** of engine behavior. Its implementation is derived from independent analysis of the original game's observable behavior, disassembly research, and publicly available documentation. The source code is **independently written** and does **not** include, copy, or redistribute any proprietary game assets, original source code, or binaries. Users must legally own a copy of the original game to run this engine.
>
> This project is **strictly non-profit**. It does not accept donations, sponsorships, or any form of commercial funding, and it is not intended for any commercial use.

## Overview

**OpenYR** is an open-source, reverse-engineering-based **reimplementation** of an engine compatible with *Command & Conquer: Yuri's Revenge*, initiated and maintained by **sovietianqi**. The project reconstructs the core logic of the original real-time strategy engine through independent analysis of observable game behavior, disassembly research, and publicly available documentation.

OpenYR aims to break free from the constraints of closed-source binaries, providing the community with an open, extensible, and portable engine alternative. This opens up broader technical possibilities for MOD development, toolchain integration, and cross-platform adaptation — all within a strictly non-commercial, educational framework.

## Current Status

> 🚧 **Active Development — Continuously Evolving** — The core architecture and foundational components are gradually taking shape. As a reimplementation project, some behaviors may not yet fully match the original game, and logical discrepancies may exist. Developers are welcome to participate in code reviews and testing feedback to help refine the implementation.

## Core Technical Directions

OpenYR rethinks the engine from the ground up with a comprehensive architectural overhaul. Key technical directions include:

### 1. Modular Architecture Design

The project adopts a modular layered architecture with independently encapsulated core subsystems, including but not limited to:

- **Core**: Engine core infrastructure
- **Game**: Game main loop and state management
- **Combat**: Combat logic and damage calculation
- **AI**: Artificial intelligence decision-making system
- **Audio**: Audio playback and management
- **FileFormats**: Resource file parsing for Mix / INI / SHP / VXL formats

Each module interacts through well-defined interfaces, reducing coupling and facilitating independent testing and replacement.

### 2. Modern Build System

- **CMake** as the build system, supporting multi-platform compilation configurations
- **C++** as the development language, leveraging modern C++ features for improved expressiveness and safety
- Clear dependency management for seamless integration of third-party libraries

### 3. Open Game Logic Extension

- Game logic fully implemented in source code, no longer constrained by hardcoded behaviors
- Provides MOD authors with direct entry points for logic modification, eliminating reliance on DLL injection or hooking techniques
- Facilitates implementation of new mechanics and gameplay that were difficult or impossible to achieve previously

### 4. Cross-Platform Portability

Through an abstraction layer independent of Windows-specific APIs, OpenYR has the potential to be ported to other operating systems (such as Linux and macOS), offering a native experience to a broader player base.

## Technical Architecture

| Component            | Description                                                       |
|----------------------|-------------------------------------------------------------------|
| Language             | C++                                                               |
| Build System         | CMake                                                             |
| Project Nature       | Reverse-engineering-based reimplementation                        |
| Compatibility Target | Command & Conquer: Yuri's Revenge (behavioral compatibility)      |
| License              | GNU General Public License v3.0                                   |
| Commercial Use       | Prohibited — strictly non-profit, educational/research only       |

## Directory Structure

```text
OpenYR/
├── src/
│   ├── AI/            # Artificial Intelligence System
│   ├── Abstract/      # Abstract Base Classes & Interfaces
│   ├── Animations/    # Animation System
│   ├── Audio/         # Audio Engine
│   ├── COM/           # Component Object Model
│   ├── Combat/        # Combat Logic
│   ├── Containers/    # Containers & Data Structures
│   ├── Core/          # Engine Core
│   ├── FileFormats/   # Resource File Format Parsers
│   ├── Game/          # Main Game Logic
│   └── Helpers/       # Utilities & Helper Functions
├── CMakeLists.txt     # CMake Build Configuration
└── README.md          # Project Documentation
```

Build Guide

Prerequisites

· A C++17 (or later) compliant compiler (MSVC / GCC / Clang)
· CMake 3.10 or higher
· Legally obtained original game asset files (for testing and runtime)

Build Steps

```bash
git clone https://github.com/Sovietianqi/OpenYR.git
cd OpenYR
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Detailed build instructions will be supplemented as the project stabilizes.

Contributing & Support

OpenYR is in its early development stages, and community contributions are highly welcomed.

· Bug Reports: Please submit via Issues on this repository
· Code Contributions: Pull Requests are welcome
· Discussions: Feel free to reach out via Issues or email

Note: OpenYR is a strictly non-commercial, educational project. We do not accept donations, sponsorships, or any form of financial support. All contributions must comply with the GPL v3 license and respect the intellectual property rights of Electronic Arts Inc.

---

The project is under rapid development. Design documents, API references, and a comprehensive build guide will be released in subsequent updates.

---

中文 {#chinese}

免责声明：OpenYR 是一个独立、开源、非商业项目，仅用于教育与研究目的。本项目与 Electronic Arts Inc. 及其授权方无任何关联，亦未经其认可、赞助或以任何形式合作。《命令与征服：尤里的复仇》及其所有相关名称、商标及知识产权均为 Electronic Arts Inc. 的独家财产。

本项目是基于逆向工程的重实现。其实现来源于对原版游戏可观察行为的独立分析、反汇编研究以及公开文档。源代码为独立编写，不包含、不复制、不再分发任何专有游戏资源、原版源代码或二进制文件。用户必须合法拥有原版游戏副本方可运行本引擎。

本项目严格非盈利，不接受任何形式的捐赠、赞助或商业资助，亦不用于任何商业用途。

概述

OpenYR 是由 sovietianqi 发起并维护的、面向《命令与征服：尤里的复仇》的开源逆向工程重实现项目。本项目通过对原版游戏可观察行为、反汇编研究与公开文档的独立分析，重建即时战略引擎的核心逻辑。

OpenYR 旨在摆脱封闭源代码二进制的约束，为社区提供一个开放、可扩展、可移植的引擎替代方案，同时为 MOD 开发、工具链集成及跨平台适配提供更广阔的技术空间——所有这些都在严格非商业、教育性的框架内进行。

当前状态

🚧 开发阶段，持续建设中 —— 项目的核心架构与基础组件已逐步搭建完成。作为一个重实现项目，部分行为可能尚未与原版游戏完全一致，且可能存在逻辑偏差。欢迎开发者参与代码审查与测试反馈，共同完善引擎实现。

核心技术方向

OpenYR 从架构设计的源头出发，对引擎进行了全面重构，主要技术方向包括：

1. 模块化架构设计

项目采用模块化分层架构，各核心子系统独立封装，包括但不限于：

· Core：引擎核心基础设施
· Game：游戏主循环与状态管理
· Combat：战斗逻辑与伤害计算
· AI：人工智能决策系统
· Audio：音频播放与管理
· FileFormats：Mix / INI / SHP / VXL 等资源文件解析

各模块间通过清晰的接口交互，降低耦合度，便于独立测试与替换。

2. 现代化构建体系

· 采用 CMake 作为构建系统，支持多平台编译配置
· 使用 C++ 作为开发语言，充分利用现代 C++ 特性提升代码表达力与安全性
· 依赖管理清晰，便于集成第三方库

3. 开放的游戏逻辑扩展

· 游戏逻辑完全由源代码实现，不再受限于硬编码行为
· 为 MOD 作者提供更直接的逻辑修改入口，无需依赖注入或 Hook 手段
· 便于实现此前难以支持的新机制与新玩法

4. 跨平台可移植性

通过独立于 Windows 专有 API 的抽象层设计，OpenYR 具备向其他操作系统（如 Linux、macOS）移植的潜力，为更广泛的玩家群体提供原生体验的可能。

技术架构

组件 说明
开发语言 C++
构建系统 CMake
项目性质 基于逆向工程的重实现
兼容目标 《命令与征服：尤里的复仇》（行为兼容）
许可协议 GNU General Public License v3.0
商业用途 禁止——严格非盈利，仅限教育/研究

目录结构

```text
OpenYR/
├── src/
│   ├── AI/            # 人工智能系统
│   ├── Abstract/      # 抽象基类与接口定义
│   ├── Animations/    # 动画系统
│   ├── Audio/         # 音频引擎
│   ├── COM/           # 组件对象模型
│   ├── Combat/        # 战斗逻辑
│   ├── Containers/    # 容器与数据结构
│   ├── Core/          # 引擎核心
│   ├── FileFormats/   # 资源文件格式解析
│   ├── Game/          # 游戏主逻辑
│   └── Helpers/       # 辅助工具与实用函数
├── CMakeLists.txt     # CMake 构建配置
└── README.md          # 项目说明
```

构建指南

环境要求

· 支持 C++17 或更高版本的编译器（MSVC / GCC / Clang）
· CMake 3.10 或更高版本
· 合法获取的游戏原版资源文件（用于测试与运行）

构建步骤

```bash
git clone https://github.com/Sovietianqi/OpenYR.git
cd OpenYR
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

具体构建细节将在项目稳定后补充完善。

贡献与支持

OpenYR 目前处于早期开发阶段，诚邀社区开发者参与共建。

· 问题反馈：请通过本仓库 Issues 提交
· 代码贡献：欢迎提交 Pull Request
· 讨论交流：欢迎通过 Issues 或邮件与作者沟通

注意：OpenYR 是一个严格非商业的教育性项目。我们不接受任何形式的捐赠、赞助或财务支持。所有贡献必须遵守 GPL v3 许可协议，并尊重 Electronic Arts Inc. 的知识产权。

---

项目仍在快速发展中，后续将陆续补充设计文档、API 参考与完整的构建指南。

```
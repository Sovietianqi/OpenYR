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
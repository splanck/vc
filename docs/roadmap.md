# Development Roadmap

This document outlines the planned phases of the `vc` compiler and the
initial features targeted for early development. It also summarizes the
compatibility goals for NetBSD and other supported systems.

## Planned Compiler Phases

1. **Lexer** – tokenizes the raw source code.
2. **Parser** – builds an abstract syntax tree from the tokens.
3. **Optimizer** – performs optional IR optimizations.
4. **Backend** – generates target-specific code.

These components correspond to the architecture described in
[architecture](architecture.md) and will be implemented incrementally.

## Initial Language Features

The first milestone aims to support a minimal subset of C including:

- basic arithmetic operations
- function definitions and calls

Additional features will be added once the core pipeline is stable.

## Compatibility Goals

`vc` primarily targets NetBSD and is expected to build out of the box on
that platform. The project also strives to remain portable across other
POSIX systems, such as FreeBSD, OpenBSD and Linux. When building on
non-NetBSD hosts, use `PLATFORM=generic` to disable NetBSD specific
extensions as described in [building](building.md).

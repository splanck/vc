# Compiler Architecture

This document describes the high level architecture of **vc**.

## Overview

The compiler is composed of several distinct phases:

1. **Lexical analysis** – the lexer converts the raw source code into a
   stream of tokens.
2. **Parsing** – the parser builds an abstract syntax tree (AST) from the
   token stream.
3. **Semantic analysis** – the AST is checked for correctness and is
   transformed into an intermediate representation (IR).
4. **Optimization** – optional passes that operate on the IR.
5. **Code generation** – the IR is lowered to target specific assembly
   language.

Each phase is implemented in its own module. Later development will
expand on these components.

For build instructions see [building](building.md) and the [README](../README.md).

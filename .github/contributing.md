# Contributing Guide

Contributions are always welcome to TWiLightMenu++! If you have something new to share with us, please, do it.

## Issues Guidelines

Before you submit an issue, check that it meets these guidelines:

- specify the version of TWiLightMenu++ you are using
- specify what console are you using and your entrypoint


## Pull Request Guidelines

Before you submit a pull request from your forked repo, check that it meets these guidelines:

1. Tell us what you've changed.
2. Test your code beforehand.
3. All code must be clear and readable. Indentation is a must and code must follow C variable and name convention.

## Setting up your environment

This fork uses BlocksDS 1.24.0 and Wonderful Toolchain. See the [build instructions](../docs/blocksds-port.md#build-this-fork) and [port notes](../docs/blocksds-port.md). Build with `make -j6 all` and `make package`. Agent regression tests and measurement tools live on the [`diagnostics` branch](https://github.com/Sanrax/TWiLightMenu-Blocks/blob/diagnostics/docs/diagnostics.md) and run manually there.

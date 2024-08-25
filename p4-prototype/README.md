# SwitchV2P P4 Prototype

This directory contains the P4 prototype code for SwitchV2P. This code is not optimized and serves as a proof-of-concept only.

## Directory Structure

- The P4 source code is in the `p4` directory.
- PTF tests are in `ptf-tests`.
- Python control plane scripts are in the `py` directory.
- A Makefile is provided for building and testing.

## Prerequisites

Ensure that Intel P4 Studio is installed and configured. The ICA build tools should be located in `$HOME/tools`, and the `SDE` environment variable must be set. Additionally, to run the tests, the Python library `scapy` should be installed.

## Building the Prototype

To compile the P4 code, run:

```bash
make
```

## Running Tests

To run the tests, follow these steps:

1. Start the simulator by running:

```bash
make model
```

2. Start switchd by running:

```bash
make switchd
```

3. Run the PTF tests with:

```bash
make test
```
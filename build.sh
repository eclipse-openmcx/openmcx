#!/usr/bin/env bash

# Copyright (c) 2020 AVL List GmbH and others
#
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
#
# SPDX-License-Identifier: Apache-2.0

# be strict
set -euo pipefail

# To install the prerequisites on Ubuntu Focal Fossa (20.04 LTS) run:
# apt update
# apt install -y cmake git
# apt install -y build-essential
# apt install -y libxml2-dev zlib1g-dev

mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=`pwd`/../install/
make -j install

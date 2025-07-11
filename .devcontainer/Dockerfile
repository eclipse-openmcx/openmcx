# Copyright (c) 2020 AVL List GmbH and others
#
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
#
# SPDX-License-Identifier: Apache-2.0

ARG VARIANT="focal"
FROM mcr.microsoft.com/vscode/devcontainers/cpp:0-${VARIANT}

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y cmake git && \
    apt-get install -y python-is-python3 && \
    apt-get install -y build-essential && \
    apt-get install -y libxml2-dev zlib1g-dev && \
    apt-get install -y libasio-dev libxerces-c-dev libzip-dev && \
    rm -rf /var/lib/apt/lists/*

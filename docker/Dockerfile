# Copyright (c) 2021 AVL List GmbH and others
#
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
#
# SPDX-License-Identifier: Apache-2.0

FROM ubuntu:20.04
LABEL Name=openmcx Version=1.0.0

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get install -y cmake git && \
    apt-get install -y build-essential && \
    apt-get install -y python-is-python3 python3-numpy && \
    apt-get install -y libxml2-dev zlib1g-dev && \
    apt-get install -y libasio-dev libxerces-c-dev libzip-dev && \
    rm -rf /var/lib/apt/lists/*

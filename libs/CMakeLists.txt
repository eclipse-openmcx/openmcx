################################################################################
# Copyright (c) 2020 AVL List GmbH and others
# 
# This program and the accompanying materials are made available under the
# terms of the Apache Software License 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0.
# 
# SPDX-License-Identifier: Apache-2.0
################################################################################

cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

project(libs VERSION 1.0.0 LANGUAGES C CXX)

add_subdirectory("tarjan")
add_subdirectory("common")
add_subdirectory("util")
add_subdirectory("channel_value")

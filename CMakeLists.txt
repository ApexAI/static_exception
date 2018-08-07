# Copyright 2018 Apex.AI, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.cmake_minimum_required(VERSION 3.5)

cmake_minimum_required(VERSION 3.5)

project(static_exception)
set(CMAKE_CXX_STANDARD 14)

# Uncomment to enable logging:
# add_definitions(-DEXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL)

# Set custom memory pool limits:
# add_definitions(-EXCEPTION_MEMORY__CXX_MAX_EXCEPTION_SIZE 1024)
# add_definitions(-EXCEPTION_MEMORY__CXX_POOL_SIZE 64*128)
# add_definitions(-EXCEPTION_MEMORY__CXX_POOL_ALIGNMENT 8)


add_library(static_exception SHARED src/exception_memory_pool.cpp)
add_subdirectory(test)




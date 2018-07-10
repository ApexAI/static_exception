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
# limitations under the License.

add_subdirectory("/usr/src/gtest" ${CMAKE_BINARY_DIR}/gtest)
enable_testing()
find_package(Boost COMPONENTS timer)
if(${Boost_FOUND})
  add_definitions(-DENABLE_BOOST_TIMER)
endif()
include_directories(${GTEST_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

# We compile some shared library to check if everything works in this.
add_library(some_library SHARED SomeClass.cpp SomeClass.hpp)

add_executable(static_exception_test static_exception_test.cpp )

target_link_libraries(static_exception_test some_library gtest gtest_main dl ${Boost_LIBRARIES})
add_test(StrCompare static_exception_test)
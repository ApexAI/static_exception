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

find_package(GTest REQUIRED)
enable_testing()

include_directories(${GTEST_INCLUDE_DIRS})

# We compile some shared libraries to check if everything works in this context aswell.
add_library(some_other_library SHARED SomeOtherClass.cpp SomeOtherClass.hpp)
add_library(some_library SHARED SomeClass.cpp SomeClass.hpp)

add_executable(static_exception_test static_exception_test.cpp)

target_link_libraries(static_exception_test
    some_library
    some_other_library
    gtest gtest_main
    dl
    static_exception
    pthread)
add_test(StrCompare static_exception_test)

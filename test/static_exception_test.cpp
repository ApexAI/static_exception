// Copyright 2018 Apex.AI, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <malloc.h>
#include <dlfcn.h>
#include <gtest/gtest.h>


#include "SomeClass.hpp"

#define EXCEPTION_MEMORY_USE_STATIC_EXCEPTION

#ifdef EXCEPTION_MEMORY_USE_STATIC_EXCEPTION
#endif

/// Makes malloc terminate if set to false.
static bool g_forbid_malloc;

namespace exception_memory {
namespace __cxx {
extern inline void cxa_free_dependent_exception (void *vptr) noexcept;
extern inline void cxa_free_exception(void *vptr) noexcept;
}
}

extern std::size_t __get_exception_memory_pool_used_segments();

/// Custom malloc to check for memory allocation.
void *malloc(size_t size) {
  static void *(*real_malloc)(size_t) = nullptr;
  if(!real_malloc) {
    real_malloc = (void *(*)(size_t)) dlsym(RTLD_NEXT, "malloc");
  }
  void *p = real_malloc(size);
  if(g_forbid_malloc) {
    fprintf(stderr, "malloc(%d) = %p\n", static_cast<int>(size), p);
    std::terminate();
  }
  return p;
}

/// Some custom exception.
class MyException {
public:
  MyException() noexcept {
    // Creating some dummy data.
    std::size_t counter = 0;
    std::for_each(std::begin(m_dummy_data), std::end(m_dummy_data), [&counter](auto& e){
      e = counter;
      ++counter;
    });
  }
  ~MyException() noexcept {
    // Checking exception data consistency.
    std::size_t counter = 0;
    std::for_each(std::cbegin(m_dummy_data), std::cend(m_dummy_data), [&counter](auto e){
      if (e != counter) {
        std::terminate();
      }
      ++counter;
    });
  }
private:
  std::size_t m_dummy_data[64];
};


void recursive_except(std::size_t max_depth = 64, std::size_t depth = 0)
{
  if(depth > max_depth) {
    return;
  }
  try {
    throw MyException();
  } catch(...) {
    recursive_except(max_depth, depth+1);
  }
}

void check_used_segments(std::size_t expected) {
  auto prev = g_forbid_malloc;
  g_forbid_malloc = false;
  GTEST_ASSERT_EQ(__get_exception_memory_pool_used_segments(), expected);
  g_forbid_malloc = prev;
}

TEST(StaticExceptions, DeepRecursion) {
  check_used_segments(0);

  // use std::clock to measure cpu time
  std::clock_t start = std::clock();
  // use steady_clock (monotonic) to measure interval
  auto t_start = std::chrono::steady_clock::now();

  std::array<std::thread, 128> threads;
  for(auto & elem : threads) {
    elem = std::thread([](){
      sleep(1);
      for (std::size_t i = 0; i < 100; ++i) {
        recursive_except();
      }
    });
  }
  g_forbid_malloc = true;
  for(auto & elem : threads) {
    elem.join();
  }
  check_used_segments(0);
  g_forbid_malloc = false;

  // use std::clock to measure cpu time
  std::clock_t end = std::clock();
  // use steady_clock (monotonic) to measure interval
  auto t_end = std::chrono::steady_clock::now();

  // calculate cpu time (ms)
  auto elapsed_cpu_time = 1000.0 * (end - start) / CLOCKS_PER_SEC;
  
  // calculate interval time from steady_clock times
  auto elapsed_wall_time = std::chrono::duration<double, std::milli>(t_end - t_start);

  std::cout << std::fixed << std::setprecision(2) <<
    "[ CPU TIME USED ] " << elapsed_cpu_time  << " ms\n" <<
    "[ WALL TIME USED ] " << elapsed_wall_time.count() << " ms" << std::endl;
}

TEST(StaticExceptions, ExceptionPtr) {
  check_used_segments(0);
  std::exception_ptr eptr;
  auto t = std::thread([&](){
    g_forbid_malloc = true;
    try {
      check_used_segments(0);
      throw MyException();
    } catch(...) {
      check_used_segments(1);
      eptr = std::current_exception();
    }
    check_used_segments(1);
  });
  t.join();
  try {
    check_used_segments(1);
    std::rethrow_exception(eptr);
  } catch (...) {
    // Here two exceptions are alive, the one stored in the exception pointer and the one thrown
    // before and caught here.
    check_used_segments(2);
  }
  check_used_segments(1);
  eptr = nullptr;
  check_used_segments(0);
  g_forbid_malloc = false;
}

TEST(StaticExceptions, ExceptionTooLarge) {
  class LargeException {
    char data[1024];
  };

  class SmallerException {
    char data[800];
  };
  try {
    ASSERT_DEATH(throw LargeException(), "");
    throw SmallerException();
  } catch(...) {

  }
}

TEST(StaticExceptions, MemoryPoolExhausted) {
  ASSERT_DEATH(recursive_except(64*128), "");
}

TEST(StaticExceptions, MemoryLeak) {
  void* som_mem = new char;
  ASSERT_DEATH(exception_memory::__cxx::cxa_free_exception(som_mem), "");
  ASSERT_DEATH(exception_memory::__cxx::cxa_free_dependent_exception(som_mem), "");
}

TEST(StaticExceptions, SharedLibraryClass) {
  g_forbid_malloc = true;
  SomeClass();
  g_forbid_malloc = false;
}

TEST(StaticExceptions, SharedLibraryFunc) {
  g_forbid_malloc = true;
  ff();
  g_forbid_malloc = false;
}

int main(int argc, char **argv) {
  g_forbid_malloc = false;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}


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

#ifndef EXCEPTION_MEMORY_POOL_HPP
#define EXCEPTION_MEMORY_POOL_HPP

#include <array>
#include <atomic>
#include <cstring>
#include <thread>
#include <cxxabi.h>
// This file is copied over from GCC to provide size information. No logic of it is used.
#include "unwind-cxx.h"

#ifdef __GNUC__
#if __GNUC__==5 && __GNUC_MINOR__==4 && __GNUC_PATCHLEVEL__==0
#else
#error Unsupported GCC version. Required version is __GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__.
#endif
#else
#error Unsupported compiler. Only GCC is supported.
#endif

#ifndef EXCEPTION_MEMORY__CXX_MAX_EXCEPTION_SIZE
/** Maximal supported exception size. Note that the internal exception representation already
 * uses a small header, so the effective available size for the exception object is slightly
 * smaller. If a larger exception is thrown std::terminate is called.
 */
#define EXCEPTION_MEMORY__CXX_MAX_EXCEPTION_SIZE 1024
#endif

#ifndef EXCEPTION_MEMORY__CXX_POOL_SIZE
/** Maximal number of supported exceptions concurrently in flight over all threads. If the number
 *  of exceptions exceeds this limit std::terminate is called.
 */
#define EXCEPTION_MEMORY__CXX_POOL_SIZE 64*128
#endif

#ifndef EXCEPTION_MEMORY__CXX_POOL_ALIGNMENT
/// Alignment of the allocated memory pool blocks.
#define EXCEPTION_MEMORY__CXX_POOL_ALIGNMENT 8
#endif

#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
#include <iostream>
#endif

namespace exception_memory {
namespace __cxx{

/** Overridable function to specify behaviour if the exception memory pool is exhausted. By default
 *  this function calls std::terminate.
 *  \param thrown_size The requested memory size.
 *  \return A pointer to some additional memory.
 */
extern "C" void* exception_memory_pool_exhausted(const size_t thrown_size) {
  std::terminate();
  return nullptr;
}

/** Overridable function to specify behaviour if the thrown exception is too large for the
 *  exception memory pool. By default this function calls std::terminate.
 *  \param thrown_size The requested memory size.
 *  \return A pointer to some additional memory.
 */
extern "C" void* exception_too_large(const size_t thrown_size) {
  std::terminate();
  return nullptr;
}

/** Overridable function to specify behaviour if the memory pool detects an memory leak. By
 *  default this function calls std::terminate.
 */
extern "C" void exception_memory_pool_leak() {
  std::terminate();
}

/// Thread safe exception memory pool.
class ExceptionMemoryPool {
  public:
  static constexpr std::size_t max_exception_size = EXCEPTION_MEMORY__CXX_MAX_EXCEPTION_SIZE;
  static constexpr std::size_t pool_size = EXCEPTION_MEMORY__CXX_POOL_SIZE;
  static constexpr std::size_t alignment = EXCEPTION_MEMORY__CXX_POOL_ALIGNMENT;

  inline ExceptionMemoryPool() noexcept
  {
    (void) start_idx(); // Making sure the hasher is created on startup.

    for (auto &elem : m_pool) {
      elem.second = aligned_alloc(alignment, max_exception_size);
      if (elem.second == nullptr) {
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
        std::cerr << "Could not initalize exception memory pool. Terminating." << std::endl;
#endif
        std::terminate();
      }
    }
  }

  inline ~ExceptionMemoryPool() noexcept {
    for (auto &elem : m_pool) {
      free(elem.second);
    }
  }

  /** Allocates \param thrown_size from the memory pool. If the exception size is to large for the
    * pool to handle exception_too_large is called. If the memory pool is exhausted
    * exception_memory_pool_exhausted is called.
    * \return Pointer to the allocated memory block.
   */
  inline void *allocate(const size_t thrown_size) noexcept {
    if (thrown_size > max_exception_size) {
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
      std::cerr << "Exception too large." << std::endl;
#endif
      return exception_too_large(thrown_size);
    }
    auto idx = inc_idx(start_idx());
    while (idx != start_idx()) {
      auto& elem = m_pool[idx];
      const auto occupied = elem.first.test_and_set();
      if(!occupied) {
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
        std::cout << "Allocate: " << elem.second << std::endl;
#endif
        return elem.second;
      }
      idx = inc_idx(idx);
    }
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
    std::cerr << "Memory pool exhausted." << std::endl;
#endif
    // Callback could provide additional memory.
    return exception_memory_pool_exhausted(thrown_size);
  }
  /** Deallocates \param thrown_object from the pool. If the memory did not originate from this
   *  memory pool exception_memory_pool_leak() is called.
   */
  inline void deallocate(void *thrown_object) noexcept {
    auto idx = inc_idx(start_idx());
    while (idx != start_idx()) {
      auto& elem = m_pool[idx];
      if (elem.second == thrown_object) {
        elem.first.clear();
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
        std::cout << "Free: " << elem.second << std::endl;
#endif
        return;
      }
      idx = inc_idx(idx);
    }
#ifdef EXCEPTION_MEMORY___CXX_LOG_MEMORY_POOL
    std::cerr << "Freeing exception not from this pool. Memory leak present!" << std::endl;
#endif
    exception_memory_pool_leak();
  }

  /** WARNING: This function is not thread safe! Only use it for testing!
   *  \return The number of used segments in the memory pool.
   */
  inline std::size_t used_segments() noexcept {
    std::size_t counter = std::size_t();
    for(auto& elem : m_pool)
    {
      auto orig = elem.first.test_and_set();
      if(orig) {
        ++counter;
      }
      else {
        elem.first.clear();
      }
    }
    return counter;
  }

  /// \returns if \param vptr was allocated from this memory pool.
  inline bool is_allocated_by_this_pool(void *vptr) const noexcept {
    void *ptr = (char *) vptr - sizeof (__cxxabiv1::__cxa_refcounted_exception);
    for (const auto& elem : m_pool) {
      if (ptr == elem.second) {
        return true;
      }
    }
    return false;
  }
  private:
  std::array <std::pair<std::atomic_flag, void *>, pool_size> m_pool;

  /// \return The thread specific start of where to look for a free memory segment.
  std::size_t start_idx() const noexcept  {
    static const auto hasher = std::hash<std::thread::id>();
    static const thread_local std::size_t t =
        hasher(std::this_thread::get_id()) % pool_size; // const (threadsafe) nothrow operation
    return t;
  }

  /// \return \param idx increased by 1 modulo pool_size.
  std::size_t inc_idx(const std::size_t idx) const noexcept {
    if (idx + 1 == pool_size) {
      return 0;
    }
    else {
      return idx + 1;
    }
  }
};

static ExceptionMemoryPool cxx_exception_memory_pool;

/** Helper function which gets memory from the exception memory pool and transforms it into a
 *  format usable by the compiler.
 *  \param thrown_size Requested memory size.
 *  \return Pointer to allocated memory.
 */
extern "C" void * cxa_allocate_exception(size_t thrown_size) noexcept
{
  thrown_size += sizeof (__cxxabiv1::__cxa_refcounted_exception);
  auto ret = cxx_exception_memory_pool.allocate(thrown_size);
  memset (ret, 0, sizeof (__cxxabiv1::__cxa_refcounted_exception));
  return (void *)((char *)ret + sizeof (__cxxabiv1::__cxa_refcounted_exception));
}

/** Helper function which frees memory from the exception memory pool.
 *  \param vptr Pointer to the memory to free.
 */
extern "C" void cxa_free_exception(void *vptr) noexcept
{
  char *ptr = (char *) vptr - sizeof (__cxxabiv1::__cxa_refcounted_exception);
  cxx_exception_memory_pool.deallocate(ptr);
}

/** Helper function which gets memory for an dependent exception (used by std::exception_ptr)
 *  from the exception memory pool and transforms it into a format usable by the compiler.
 *  \return vptr Pointer to allocated memory.
 */
extern "C" void* cxa_allocate_dependent_exception() noexcept
{
  void * ret = cxx_exception_memory_pool.allocate(sizeof (__cxxabiv1::__cxa_dependent_exception));
  memset (ret, 0, sizeof (__cxxabiv1::__cxa_dependent_exception));
  return ret;
}

/** Helper function which frees memory for an dependent exception (used by std::exception_ptr)
 *  from the exception memory pool and transforms it into a format usable by the compiler.
 *  \param vptr Pointer to allocated memory.
 */
extern "C" void cxa_free_dependent_exception (void *vptr) noexcept
{
  cxx_exception_memory_pool.deallocate(vptr);
}

}
}

// Override the compiler functions
extern "C" void * __cxa_allocate_exception(size_t thrown_size)
{
  return exception_memory::__cxx::cxa_allocate_exception(thrown_size);
}

extern "C" void __cxa_free_exception(void *thrown_object)
{
  exception_memory::__cxx::cxa_free_exception(thrown_object);
}

extern "C" __cxxabiv1::__cxa_dependent_exception * __cxa_allocate_dependent_exception () {
  return static_cast<__cxxabiv1::__cxa_dependent_exception*>(exception_memory::__cxx::cxa_allocate_dependent_exception());
}

extern "C" void __cxa_free_dependent_exception (__cxxabiv1::__cxa_dependent_exception * dependent_exception) {
  exception_memory::__cxx::cxa_free_dependent_exception(dependent_exception);
}

#endif // EXCEPTION_MEMORY_POOL_HPP

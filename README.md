# Introduction

Avoiding dynamic memory allocations is ciritical in safety relevant systems,
but throwing an exception dynamically allocates memory in GCC. This header
only library replaces this allocation scheme with a thread safe memory pool.

# Usage

You need to include `exception_memory_pool.hpp` in every file you intend
to throw an exception in. You do not need to add this include to libraries
to make it work.

# Configuration

The resource limits of memory pool can be configured using compiler
defines:

```
# Set custom memory pool limits:
add_definitions(-EXCEPTION_MEMORY__CXX_MAX_EXCEPTION_SIZE 1024)
add_definitions(-EXCEPTION_MEMORY__CXX_POOL_SIZE 64*128)
add_definitions(-EXCEPTION_MEMORY__CXX_POOL_ALIGNMENT 8)
```

Errors can be handled by overwriting error specific callback functions.
By default these call `std::terminate`:

```cpp
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
```

# Limitations

* Exceptions thrown during library initalization might still be allocated
dynamically. This is usually not a problem as static memory is only required
during steady time.

* Standard exceptions such `std::runtime_error` will still allocate memory
for their internal error string if thrown. This library does not solve
this issue.
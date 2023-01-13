#ifndef H_memorysafety
#define H_memorysafety
//---------------------------------------------------------------------------
// C++ memory safety runtime
// (c) 2023 Thomas Neumann
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------
namespace memorysafety {
//---------------------------------------------------------------------------
/// Assert that the object A is still valid (i.e., no dependencies are violated)
void validate(const void* A) noexcept;
/// Register a dependency between an object A and the existence of an object B. A cannot be used after B has been destroyed
void add_dependency(const void* A, const void* B) noexcept;
/// Register a dependency between an object A and the content of an object B. A cannot be used after B has been modified
void add_content_dependency(const void* A, const void* B) noexcept;
/// Mark the object B as modified
void mark_modified(const void* B) noexcept;
/// Mark the object B as destroyed and release associated memory. Note that every object A or B that has been an argument to add.*dependency must be destroyed at some point
void mark_destroyed(const void* B) noexcept;
/// Reset all dependencies of A and make it valid again
void reset(const void* A) noexcept;
/// Mark an object A invalid if the other object B is invalid
void propagate_invalid(const void* A, const void* B) noexcept;
/// Like propagate_invalid, but pass over content dependencies, too
void propagate_content(const void* A, const void* B) noexcept;
//---------------------------------------------------------------------------
/// Change the violation handler. By default the program is terminated, but that is not very convenient for tests
void set_violation_handler(void (*handler)(const void* obj)) noexcept;
//---------------------------------------------------------------------------
/// Report that an assertion failed
void assert_spatial_failed() noexcept;
/// Helper for spatial asserts
inline void assert_spatial(bool cond) noexcept {
   if (!cond) assert_spatial_failed();
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
#endif

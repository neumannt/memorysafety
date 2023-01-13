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
//---------------------------------------------------------------------------
/// Change the violation handler. By default the program is terminated, but that is not very convenient for tests
void set_violation_handler(void (*handler)(const void* obj)) noexcept;
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
#endif

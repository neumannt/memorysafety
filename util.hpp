#ifndef H_util
#define H_util
//---------------------------------------------------------------------------
// C++ memory safety runtime
// (c) 2023 Thomas Neumann
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------
#include "memorysafety.hpp"
#include <functional>
#include <iosfwd>
#include <utility>
//---------------------------------------------------------------------------
// Utility classes that provide memory-safe abstractions
//
// NOTICE: We cannot get foolproof safety without compiler support, in particular
// the code relies upon the fact that short-term references are valid. For
// potentially invalid references use ref_wrapper defined below to get the
// intended behavior that the compiler should provide automatically.
//---------------------------------------------------------------------------
namespace detail {
template <class T>
constexpr T& ref_wrapper_fun(T& t) noexcept { return t; }
template <class T>
void ref_wrapper_fun(T&&) = delete;
}
//---------------------------------------------------------------------------
/// A reference wrapper modeled after std::reference_wrapper but with memory safety checks
template <class T>
class ref_wrapper {
   public:
   // types
   typedef T type;

   // construct/copy/destroy
   template <class U, class = decltype(detail::ref_wrapper_fun<T>(std::declval<U>()), std::enable_if_t<!std::is_same_v<ref_wrapper, std::remove_cvref_t<U>>>())>
   constexpr ref_wrapper(U&& u) noexcept(noexcept(detail::ref_wrapper_fun<T>(std::forward<U>(u))))
      : _ptr(std::addressof(detail::ref_wrapper_fun<T>(std::forward<U>(u)))) {
      // The reference depends on the target pointer
      memorysafety::add_dependency(this, _ptr);
   }
   ref_wrapper(const ref_wrapper& o) noexcept
      : _ptr(o._ptr) {
      // Copy the dependency from the other reference and propagate invalid states
      memorysafety::propagate_invalid(this, &o);
      memorysafety::add_dependency(this, _ptr);
   }
   ~ref_wrapper() {
      // The reference was destroyed
      memorysafety::mark_destroyed(this);
   }

   // assignment
   ref_wrapper& operator=(const ref_wrapper& o) noexcept {
      if (this != &o) {
         // Clear all existing dependencies make valid again
         memorysafety::reset(this);

         // Copy the dependency from the other reference and propagate invalid states
         _ptr = o._ptr;
         memorysafety::propagate_invalid(this, &o);
         memorysafety::add_dependency(this, _ptr);
      }
      return *this;
   }

   // access
   constexpr operator T&() const noexcept {
      memorysafety::validate(this);
      return *_ptr;
   }
   constexpr T& get() const noexcept {
      memorysafety::validate(this);
      return *_ptr;
   }

   template <class... ArgTypes>
   constexpr std::invoke_result_t<T&, ArgTypes...>
   operator()(ArgTypes&&... args) const {
      return std::invoke(get(), std::forward<ArgTypes>(args)...);
   }

   private:
   T* _ptr;
};
//---------------------------------------------------------------------------
/// A reference wrapper for objects that depend on an outer object being unmodified
template <class T>
class inner_ref_wrapper {
   public:
   // types
   typedef T type;

   // construct/copy/destroy
   template <class U, class = decltype(detail::ref_wrapper_fun<T>(std::declval<U>()), std::enable_if_t<!std::is_same_v<inner_ref_wrapper, std::remove_cvref_t<U>>>())>
   constexpr inner_ref_wrapper(const void* outer, U&& u) noexcept(noexcept(detail::ref_wrapper_fun<T>(std::forward<U>(u))))
      : _ptr(std::addressof(detail::ref_wrapper_fun<T>(std::forward<U>(u)))) {
      // The reference depends on the outer object
      memorysafety::add_content_dependency(this, outer);
   }
   inner_ref_wrapper(const inner_ref_wrapper& o) noexcept
      : _ptr(o._ptr) {
      // Copy the dependency from the other reference and propagate invalid states
      memorysafety::propagate_content(this, &o);
   }
   ~inner_ref_wrapper() {
      // The reference was destroyed
      memorysafety::mark_destroyed(this);
   }

   // assignment
   inner_ref_wrapper& operator=(const inner_ref_wrapper& o) noexcept {
      if (this != &o) {
         // Clear all existing dependencies make valid again
         memorysafety::reset(this);

         // Copy the dependency from the other reference and propagate invalid states
         _ptr = o._ptr;
         memorysafety::propagate_content(this, &o);
      }
      return *this;
   }

   // access
   constexpr operator T&() const noexcept {
      memorysafety::validate(this);
      return *_ptr;
   }
   constexpr T& get() const noexcept {
      memorysafety::validate(this);
      return *_ptr;
   }

   template <class... ArgTypes>
   constexpr std::invoke_result_t<T&, ArgTypes...>
   operator()(ArgTypes&&... args) const {
      return std::invoke(get(), std::forward<ArgTypes>(args)...);
   }

   private:
   T* _ptr;
};
//---------------------------------------------------------------------------
/// A simple string implementation that demonstrates safety primitives
class ms_string {
   public:
   using value_type = char;
   using size_type = unsigned long;
   using difference_type = long;
   using reference = inner_ref_wrapper<char>;
   using const_reference = inner_ref_wrapper<const char>;

   static constexpr size_type npos = ~static_cast<size_type>(0);

   /// An iterator
   class iterator {
      private:
      char *iter, *limit;

      friend class ms_string;

      iterator(ms_string* outer, char* iter, char* limit) noexcept : iter(iter), limit(limit) {
         memorysafety::add_content_dependency(this, outer);
      }

      public:
      constexpr iterator() noexcept : iter(nullptr), limit(nullptr) {}
      iterator(const iterator& o) noexcept : iter(o.iter), limit(o.limit) { memorysafety::propagate_content(this, &o); }
      ~iterator() { memorysafety::mark_destroyed(this); }

      iterator& operator=(const iterator& o) noexcept {
         if (this != &o) {
            memorysafety::reset(this);
            iter = o.iter;
            limit = o.limit;
            memorysafety::propagate_content(this, &o);
         }
         return *this;
      }

      iterator& operator++() {
         memorysafety::assert_spatial(iter != limit);
         ++iter;
         return *this;
      }
      iterator& operator+=(long step) {
         memorysafety::assert_spatial((step >= 0) && (step < (limit - iter)));
         iter += step;
         return *this;
      }
      iterator operator+(long step) const {
         iterator res = *this;
         res += step;
         return res;
      }
      char& operator*() const {
         memorysafety::assert_spatial(iter < limit);
         memorysafety::validate(this);
         return *iter;
      }

      bool operator==(const iterator& o) const { return iter == o.iter; }
      bool operator!=(const iterator& o) const { return iter != o.iter; }
      bool operator<(const iterator& o) const { return iter < o.iter; }
      bool operator<=(const iterator& o) const { return iter <= o.iter; }
      bool operator>(const iterator& o) const { return iter > o.iter; }
      bool operator>=(const iterator& o) const { return iter >= o.iter; }
   };
   /// A const iterator
   class const_iterator {
      private:
      const char *iter, *limit;

      friend class ms_string;

      const_iterator(const ms_string* outer, const char* iter, const char* limit) noexcept : iter(iter), limit(limit) {
         memorysafety::add_content_dependency(this, outer);
      }

      public:
      constexpr const_iterator() noexcept : iter(nullptr), limit(nullptr) {}
      const_iterator(const const_iterator& o) noexcept : iter(o.iter), limit(o.limit) { memorysafety::propagate_content(this, &o); }
      ~const_iterator() { memorysafety::mark_destroyed(this); }

      const_iterator& operator=(const const_iterator& o) noexcept {
         if (this != &o) {
            memorysafety::reset(this);
            iter = o.iter;
            limit = o.limit;
            memorysafety::propagate_content(this, &o);
         }
         return *this;
      }

      const_iterator& operator++() {
         memorysafety::assert_spatial(iter != limit);
         ++iter;
         return *this;
      }
      const_iterator& operator+=(long step) {
         memorysafety::assert_spatial((step >= 0) && (step < (limit - iter)));
         iter += step;
         return *this;
      }
      const_iterator operator+(long step) const {
         const_iterator res = *this;
         res += step;
         return res;
      }
      char operator*() const {
         memorysafety::assert_spatial(iter < limit);
         memorysafety::validate(this);
         return *iter;
      }

      bool operator==(const iterator& o) const { return iter == o.iter; }
      bool operator!=(const iterator& o) const { return iter != o.iter; }
      bool operator<(const iterator& o) const { return iter < o.iter; }
      bool operator<=(const iterator& o) const { return iter <= o.iter; }
      bool operator>(const iterator& o) const { return iter > o.iter; }
      bool operator>=(const iterator& o) const { return iter >= o.iter; }
   };

   /// The data
   char* _ptr;
   /// Size and capacity
   size_type _size, _capacity;

   public:
   /// Constructor
   constexpr ms_string() : _ptr(nullptr), _size(0), _capacity(0) {}
   /// Constructor from a C string
   ms_string(const char* cstr) {
      size_type len = 0;
      while (cstr[len]) ++len;
      if (len) {
         _ptr = new char[len];
         for (size_type index = 0; index < len; ++index)
            _ptr[index] = cstr[index];
      } else {
         _ptr = nullptr;
      }
      _size = _capacity = len;
   }
   /// Move constructor
   ms_string(ms_string&& o) noexcept
      : _ptr(o._ptr), _size(o._size), _capacity(o._capacity) {
      memorysafety::mark_modified(&o);
      o._ptr = nullptr;
      o._size = o._capacity = 0;
   }
   /// Copy constructor
   ms_string(const ms_string& o)
      : _size(o._size), _capacity(o._size) {
      if (_size) {
         _ptr = new char[_size];
         for (size_type index = 0; index < _size; ++index)
            _ptr[index] = o._ptr[index];
      } else {
         _ptr = nullptr;
      }
   }
   /// Destructor
   ~ms_string() {
      memorysafety::mark_destroyed(this);
      delete[] _ptr;
   }

   /// Assignment
   ms_string& operator=(const ms_string& o) {
      if (this != &o) {
         memorysafety::mark_modified(this);
         delete[] _ptr;
         _size = _capacity = o._size;
         if (_size) {
            _ptr = new char[_size];
            for (size_type index = 0; index < _size; ++index)
               _ptr[index] = o._ptr[index];
         } else {
            _ptr = nullptr;
         }
      }
      return *this;
   }
   /// Assignment
   ms_string& operator=(ms_string&& o) {
      if (this != &o) {
         memorysafety::mark_modified(this);
         memorysafety::mark_modified(&o);
         delete[] _ptr;
         _ptr = o._ptr;
         _size = o._size;
         _capacity = o._capacity;
         o._ptr = nullptr;
         o._size = o._capacity = 0;
      }
      return *this;
   }

   /// Access
   reference operator[](size_type pos) {
      memorysafety::assert_spatial(pos < _size);
      return reference(this, _ptr[pos]);
   }
   /// Access
   const_reference operator[](size_type pos) const {
      memorysafety::assert_spatial(pos < _size);
      return const_reference(this, _ptr[pos]);
   }
   /// Access
   reference front() {
      memorysafety::assert_spatial(_size > 0);
      return reference(this, _ptr[0]);
   }
   /// Access
   const_reference front() const {
      memorysafety::assert_spatial(_size > 0);
      return const_reference(this, _ptr[0]);
   }
   /// Access
   reference back() {
      memorysafety::assert_spatial(_size > 0);
      return reference(this, _ptr[_size - 1]);
   }
   /// Access
   const_reference back() const {
      memorysafety::assert_spatial(_size > 0);
      return const_reference(this, _ptr[_size - 1]);
   }
   /// Access the raw data. TODO result is currently unsafe, we need a checking wrapper here
   const char* data() const { return _ptr; }

   /// Iterator
   iterator begin() { return iterator(this, _ptr, _ptr + _size); }
   /// Iterator
   const_iterator begin() const { return const_iterator(this, _ptr, _ptr + _size); }
   /// Iterator
   const_iterator cbegin() { return const_iterator(this, _ptr, _ptr + _size); }
   /// Iterator
   iterator end() { return iterator(this, _ptr + _size, _ptr + _size); }
   /// Iterator
   const_iterator end() const { return const_iterator(this, _ptr + _size, _ptr + _size); }
   /// Iterator
   const_iterator cend() { return const_iterator(this, _ptr + _size, _ptr + _size); }

   /// Empty?
   bool empty() const { return !_size; }
   /// Size
   size_type size() const { return _size; }
   /// Size
   size_type length() const { return _size; }
   /// Make sure we have enough space
   void reserve(size_type nc) {
      memorysafety::mark_modified(this);
      if (nc > _capacity) {
         size_type nc2 = _capacity + (_capacity / 8);
         if (nc2 > nc) nc = nc2;
         char* np = new char[nc];
         for (size_type index = 0; index < _size; ++index) np[index] = _ptr[index];
         delete[] _ptr;
         _ptr = np;
         _capacity = nc;
      }
   }

   // Clear the contents
   void clear() {
      memorysafety::mark_modified(this);
      _size = 0;
   }
   /// Erase characters
   ms_string& erase(size_type index = 0, size_type count = npos) {
      memorysafety::mark_modified(this);
      if (index < _size) {
         if (count < (_size - index)) {
            for (size_type index = 0, limit = _size - index - count; index != limit; ++index)
               _ptr[index] = _ptr[index + count];
            _size -= count;
         } else {
            _size = index;
         }
      }
      return *this;
   }
   /// Erase a character
   iterator erase(iterator iter) {
      memorysafety::validate(&iter);
      memorysafety::assert_spatial((iter.iter >= _ptr) && (iter.iter <= _ptr + _size));
      size_type pos = iter.iter - _ptr;
      erase(pos, 1);
      return iterator(this, _ptr + ((pos < _size) ? pos : _size), _ptr + _size);
   }
   /// Erase a range of characters
   iterator erase(iterator first, iterator last) {
      memorysafety::validate(&first);
      memorysafety::validate(&last);
      memorysafety::assert_spatial((first.iter >= _ptr) && (last.iter >= first.iter) && (last.iter <= _ptr + _size));
      size_type pos = first.iter - _ptr, count = last.iter - first.iter;
      erase(pos, count);
      return iterator(this, _ptr + ((pos < _size) ? pos : _size), _ptr + _size);
   }

   /// Append a character
   void push_back(char c) {
      // Reserve marks as modified
      reserve(_size + 1);

      // Check for numeric overflows
      memorysafety::assert_spatial(_size < _capacity);

      _ptr[_size++] = c;
   }
   /// Append
   ms_string& operator+=(char c) {
      push_back(c);
      return *this;
   }
   /// Append
   ms_string& operator+=(const ms_string& o) {
      memorysafety::assert_spatial(this != &o);

      reserve(_size + o._size);

      // Check for numeric overflows
      memorysafety::assert_spatial((_size < _capacity) && (o._size < _capacity));

      for (size_type index = 0; index != o._size; ++index)
         _ptr[_size++] = o._ptr[index];
      return *this;
   }

   /// Change the string size
   void resize(size_type ns, char c = '\0') {
      memorysafety::mark_modified(this);

      if (ns < _size) {
         _size = ns;
      } else if (ns > _size) {
         reserve(ns);
         memorysafety::assert_spatial(ns <= _capacity);
         while (_size < ns) _ptr[_size++] = c;
      }
   }

   /// Swap the content
   void swap(ms_string& o) noexcept {
      if (this != &o) {
         memorysafety::mark_modified(this);
         memorysafety::mark_modified(&o);

         {
            auto tmp = _ptr;
            _ptr = o._ptr;
            o._ptr = tmp;
         }
         {
            auto tmp = _size;
            _size = o._size;
            o._size = tmp;
         }
         {
            auto tmp = _capacity;
            _capacity = o._capacity;
            o._capacity = tmp;
         }
      }
   }
};
//---------------------------------------------------------------------------
#endif

#include "memorysafety.hpp"
#include <iostream>
#include <unordered_map>
//---------------------------------------------------------------------------
namespace memorysafety {
//---------------------------------------------------------------------------
namespace {
//---------------------------------------------------------------------------
using ViolationHandler = void (*)(const void*);
//---------------------------------------------------------------------------
static void defaultHandler(const void* A) {
   // Print an error message and terminate the program by default
   std::cerr << "violating memory safety dependency on object " << A << std::endl;
   std::terminate();
}
//---------------------------------------------------------------------------
static constinit ViolationHandler violationHandler = defaultHandler;
//---------------------------------------------------------------------------
/// Memory safety logic
class MemorySafety {
   struct Dependency;

   /// Information about an object
   struct Object {
      /// The dependencies of the object itself
      Dependency* dependencies = nullptr;
      /// The incoming dependencies
      Dependency* incoming[2] = {nullptr, nullptr};
      /// Is the object still valid?
      bool isValid = true;

      Object() noexcept = default;
      Object(const Object&) = delete;
      Object(Object&&) = delete;

      /// Invalidate objects that depend on this object
      void invalidateIncoming(bool contentOnly) noexcept;
      /// Invalidate an object, dropping all dependencies
      void invalidate() noexcept;
      /// Add a dependency
      void addDependency(Object* target, bool content) noexcept;

      /// Splay a dependency node
      void splay(Dependency* dep) noexcept;
      /// Splay rotation
      void leftRotate(Dependency* dep) noexcept;
      /// Splay rotation
      void rightRotate(Dependency* dep) noexcept;
   };
   /// A dependency. This is part of two intrusive data structures, one lookup
   /// tree for finding an existing dependencies and a double linked list
   /// for invalidating dependencies
   struct Dependency {
      /// The object that depends on another
      Object* A;
      /// The object that is dependent on
      Object* B;
      /// Content dependency?
      bool content;

      /// The tree structure for finding dependencies
      Dependency *parent, *left, *right;
      /// The double linked list for incoming dependencies
      Dependency *prev, *next;

      /// Add to dependency chain
      void link() noexcept;
      /// Remove from dependency chain
      void unlink() noexcept;
   };

   /// The lookup tables
   std::unordered_map<const void*, Object> lookup;
   /// Initialized flag
   bool initialized;

   public:
   /// Constructor
   MemorySafety();
   /// Destructor
   ~MemorySafety();

   /// Validate an object
   void validate(const void* A) const noexcept;
   /// Add a dependency on the existence of B
   void addDependency(const void* A, const void* B) noexcept;
   /// Add a dependency on the content of B
   void addContentDependency(const void* A, const void* B) noexcept;
   /// Mark an object as modified
   void markModified(const void* B) noexcept;
   /// Mark an object as destroy
   void markDestroyed(const void* B) noexcept;

   /// Is the object initialized? This only works because global objects are zero initialized
   bool isAvailable() const noexcept { return initialized; }
};
//---------------------------------------------------------------------------
void MemorySafety::Dependency::link() noexcept
// Add to dependency chain
{
   prev = nullptr;
   next = B->incoming[content];
   if (next) next->prev = this;
   B->incoming[content] = this;
}
//---------------------------------------------------------------------------
void MemorySafety::Dependency::unlink() noexcept
// Remove from dependency chain
{
   if (prev) {
      prev->next = next;
   } else {
      B->incoming[content] = next;
   }
   if (next) next->prev = prev;

   prev = next = nullptr;
}
//---------------------------------------------------------------------------
void MemorySafety::Object::invalidateIncoming(bool contentOnly) noexcept
// Invalidate objects that depend on this object
{
   // Invalidate everything that depends on our content
   while (incoming[1]) incoming[1]->A->invalidate();

   // Invalidate everything that depends on our existence
   if (!contentOnly) {
      while (incoming[0]) incoming[0]->A->invalidate();
   }
}
//---------------------------------------------------------------------------
void MemorySafety::Object::invalidate() noexcept
// Invalidate an object, dropping all dependencies
{
   if (isValid) {
      isValid = false;
      invalidateIncoming(true);
   }

   // Drop all dependencies
   auto d = dependencies;
   dependencies = nullptr;
   while (d) {
      if (d->left) {
         auto n = d->left;
         d->left = n->right;
         n->right = d;
         d = n;
      } else {
         auto n = d;
         d = d->right;
         n->unlink();
         delete n;
      }
   }
}
//---------------------------------------------------------------------------
void MemorySafety::Object::addDependency(Object* target, bool content) noexcept
// Add a dependency
{
   // Check if it already exists
   Dependency *iter = dependencies, *parent = nullptr;
   while (iter) {
      parent = iter;
      if (target < iter->B) {
      } else if (target > iter->B) {
      } else {
         // Existing dependency found, upgrade if needed
         if (content > iter->content) {
            iter->unlink();
            iter->content = true;
            iter->link();
         }
         splay(iter);
         return;
      }
   }

   // Create a new dependency
   Dependency* d = new Dependency();
   d->A = this;
   d->B = target;
   d->content = content;

   d->parent = parent;
   d->left = d->right = d->prev = d->next = nullptr;

   // Add to tree
   if (!parent) {
      dependencies = d;
   } else if (target < parent->B) {
      parent->left = d;
   } else {
      parent->right = d;
   }

   // Add to dependency chain
   d->link();

   splay(d);
}
//---------------------------------------------------------------------------
void MemorySafety::Object::splay(Dependency* dep) noexcept
// Splay a dependency node
{
   while (dep->parent) {
      if (!dep->parent->parent) {
         if (dep->parent->left == dep)
            rightRotate(dep->parent);
         else
            leftRotate(dep->parent);
      } else if ((dep->parent->left == dep) && (dep->parent->parent->left == dep->parent)) {
         rightRotate(dep->parent->parent);
         rightRotate(dep->parent);
      } else if ((dep->parent->right == dep) && (dep->parent->parent->right == dep->parent)) {
         leftRotate(dep->parent->parent);
         leftRotate(dep->parent);
      } else if ((dep->parent->left == dep) && (dep->parent->parent->right == dep->parent)) {
         rightRotate(dep->parent);
         leftRotate(dep->parent);
      } else {
         leftRotate(dep->parent);
         rightRotate(dep->parent);
      }
   }
}
//---------------------------------------------------------------------------
void MemorySafety::Object::leftRotate(Dependency* dep) noexcept
// Splay rotation
{
   auto o = dep->right;
   if (o) {
      dep->right = o->left;
      if (o->left) o->left->parent = dep;
      o->parent = dep->parent;
   }

   if (!dep->parent) {
      dependencies = o;
   } else if (dep == dep->parent->left) {
      dep->parent->left = o;
   } else {
      dep->parent->right = o;
   }
   if (o) o->left = dep;
   dep->parent = o;
}
//---------------------------------------------------------------------------
void MemorySafety::Object::rightRotate(Dependency* dep) noexcept
// Splay rotation
{
   auto o = dep->left;
   if (o) {
      dep->left = o->right;
      if (o->right) o->right->parent = dep;
      o->parent = dep->parent;
   }
   if (!dep->parent) {
      dependencies = o;
   } else if (dep == dep->parent->left) {
      dep->parent->left = o;
   } else {
      dep->parent->right = o;
   }
   if (o) o->right = dep;
   dep->parent = o;
}
//---------------------------------------------------------------------------
/// The memory safety logic
static MemorySafety logic;
//---------------------------------------------------------------------------
MemorySafety::MemorySafety() {
   initialized = true;
}
//---------------------------------------------------------------------------
MemorySafety::~MemorySafety() {
   // Release all remaining dependencies
   for (auto& e : lookup)
      e.second.invalidate();
   lookup.clear();

   initialized = false;
}
//---------------------------------------------------------------------------
void MemorySafety::validate(const void* A) const noexcept
// Validate an object
{
   auto iter = lookup.find(A);
   if (iter != lookup.end()) {
      if (!iter->second.isValid) {
         violationHandler(A);
      }
   }
}
//---------------------------------------------------------------------------
void MemorySafety::addDependency(const void* A, const void* B) noexcept
/// Add a dependency on the existence of B
{
   auto& a = lookup[A];

   // Stop operating in invalid objects
   if (!a.isValid) return;

   auto& b = lookup[B];
   a.addDependency(&b, false);
}
//---------------------------------------------------------------------------
void MemorySafety::addContentDependency(const void* A, const void* B) noexcept
/// Add a dependency on the content of B
{
   auto& a = lookup[A];

   // Stop operating in invalid objects
   if (!a.isValid) return;

   auto& b = lookup[B];

   // Is B invalid? Than we are immediately invalid, too
   if (!b.isValid) {
      a.invalidate();
      return;
   }

   a.addDependency(&b, true);
}
//---------------------------------------------------------------------------
void MemorySafety::markModified(const void* B) noexcept
// Mark an object as modified
{
   auto iter = lookup.find(B);
   if (iter != lookup.end()) {
      // Invalidate everything that depends on the content
      iter->second.invalidateIncoming(true);
   }
}
//---------------------------------------------------------------------------
void MemorySafety::markDestroyed(const void* B) noexcept
// Mark an object as destroyed
{
   auto iter = lookup.find(B);
   if (iter != lookup.end()) {
      // Invalidate all dependencies
      iter->second.invalidateIncoming(false);

      lookup.erase(iter);
   }
}
//---------------------------------------------------------------------------
}
//---------------------------------------------------------------------------
/// Assert that the object A is still valid (i.e., no dependencies are violated)
void validate(const void* A) noexcept {
   if (logic.isAvailable()) logic.validate(A);
}
//---------------------------------------------------------------------------
/// Register a dependency between an object A and an object B. A cannot be used after B has been destroyed
void add_dependency(const void* A, const void* B) noexcept {
   if (logic.isAvailable()) logic.addDependency(A, B);
}
//---------------------------------------------------------------------------
/// Register a dependency between an object A and an object B. A cannot be used after B has been modified
void add_content_dependency(const void* A, const void* B) noexcept {
   if (logic.isAvailable()) logic.addContentDependency(A, B);
}
//---------------------------------------------------------------------------
/// Mark the object B as modified
void mark_modified(const void* B) noexcept {
   if (logic.isAvailable()) logic.markModified(B);
}
//---------------------------------------------------------------------------
/// Mark the object B as destroyed and release associated memory. Note that every object A or B that has been an argument to add.*dependency must be destroyed at some point
void mark_destroyed(const void* B) noexcept {
   if (logic.isAvailable()) logic.markDestroyed(B);
}
//---------------------------------------------------------------------------
/// Change the violation handler. By default the program is terminated, but that is not very convenient for tests
void set_violation_handler(void (*handler)(const void*)) noexcept {
   violationHandler = handler ? handler : defaultHandler;
}
//---------------------------------------------------------------------------
}

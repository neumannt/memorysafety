#include "util.hpp"
//---------------------------------------------------------------------------
// C++ memory safety demo
// (c) 2023 Thomas Neumann
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------
int main() {
   ms_string test("Hello ");
   auto iter = test.begin();
   [[maybe_unused]] char c = *iter;
   auto iter2 = iter;
   test+=" World!";
   iter = iter2;
   [[maybe_unused]] char c2 = *iter; // this fails
}

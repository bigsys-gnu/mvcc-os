#pragma once
#include <new>

struct mvrlu_node {
  int value;
  mvrlu_node *next;

  mvrlu_node(int val): value(val), next(nullptr) {}

  static void* operator new(unsigned long nbytes, const std::nothrow_t&) noexcept;
  static void* operator new(unsigned long nbytes);
  static void operator delete(void *p, const std::nothrow_t&) noexcept;
  static void operator delete(void *p);
};

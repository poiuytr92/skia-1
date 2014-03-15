/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkAtomics_sync_DEFINED
#define SkAtomics_sync_DEFINED

/** GCC/Clang __sync based atomics. */

#include <stdint.h>

int32_t sk_atomic_inc(int32_t* addr);

int32_t sk_atomic_add(int32_t* addr, int32_t inc);

int32_t sk_atomic_dec(int32_t* addr);

static inline __attribute__((always_inline)) void sk_membar_acquire__after_atomic_dec() { }

int32_t sk_atomic_conditional_inc(int32_t* addr);

bool sk_atomic_cas(int32_t* addr,
                   int32_t before,
                   int32_t after);

static inline __attribute__((always_inline)) void sk_membar_acquire__after_atomic_conditional_inc() { }

#endif

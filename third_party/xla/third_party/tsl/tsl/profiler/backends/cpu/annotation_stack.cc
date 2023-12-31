/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tsl/profiler/backends/cpu/annotation_stack.h"

#include <atomic>

#include "tsl/platform/types.h"

namespace tsl {
namespace profiler {
namespace internal {

#ifdef _WIN32
#define DECL_DLL_EXPORT __declspec(dllexport)
#else
#define DECL_DLL_EXPORT
#endif
// DLL imported variables cannot be initialized on Windows. This file is
// included only on DLL exports.
DECL_DLL_EXPORT std::atomic<int> g_annotation_enabled(0);

// g_annotation_enabled implementation must be lock-free for faster execution of
// the ScopedAnnotation API. This can be commented (if compilation is failing)
// but execution might be slow (even when tracing is disabled).
static_assert(ATOMIC_INT_LOCK_FREE == 2, "Assumed atomic<int> was lock free");

}  // namespace internal

/*static*/ string* AnnotationStack::ThreadAnnotationStack() {
  static thread_local string annotation_stack;
  return &annotation_stack;
}

}  // namespace profiler
}  // namespace tsl

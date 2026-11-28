## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

if (WIN32)
  set(gb_base_internal_SOURCE
    win_aligned_memory.cc
    win_local_time.cc
    win_platform.cc win_platform.h
    win_process_memory.cc
    win_sleep.cc
    win_stdio_file.cc
  )
  set(gb_base_internal_TEST_SOURCE
    local_time_test.cc
    process_memory_test.cc
    win_manifest_test.cc
    win_platform_test.cc
  )
elseif (EMSCRIPTEN)
  set(gb_base_internal_SOURCE
    gen_aligned_memory.cc
    gen_process_memory.cc
    gen_stdio_file.cc
    web_local_time.cc
    web_sleep.cc
  )

  # local_time_test.cc checks against std::chrono::current_zone(), which
  # Emscripten's standard library doesn't have.
  set(gb_base_internal_TEST_SOURCE
    web_local_time_test.cc
  )
else()
  set(gb_base_internal_SOURCE
    gen_aligned_memory.cc
    gen_local_time.cc
    gen_sleep.cc
    gen_stdio_file.cc
  )
  set(gb_base_internal_TEST_SOURCE
    local_time_test.cc
  )

  # Process memory is read from /proc, which is Linux's alone.
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND gb_base_internal_SOURCE linux_process_memory.cc)
    list(APPEND gb_base_internal_TEST_SOURCE process_memory_test.cc)
  else()
    list(APPEND gb_base_internal_SOURCE gen_process_memory.cc)
  endif()
endif()

set(gb_base_SOURCE
  access_token.h
  aligned_memory.h
  allocator.cc allocator.h
  callback_scope.h
  clock.h
  context.cc context.h
  context_builder.h
  embedded_data.h
  fake_clock.h
  flags.h
  function_hook.h
  init_logging.cc init_logging.h
  local_time.h
  log_file.cc log_file.h
  platform_requirements.cc platform_requirements.h
  process_memory.h
  scoped_call.h
  sleep.h
  stdio_file.h
  string_util.h
  type_info.h
  unicode.cc unicode.h
  validated_context.cc validated_context.h
  weak_ptr.h
  ${gb_base_internal_SOURCE}
)

set(gb_base_TEST_SOURCE
  allocator_test.cc
  callback_scope_test.cc
  clock_test.cc
  context_test.cc
  embedded_data_test.cc
  flags_test.cc
  function_hook_test.cc
  init_logging_test.cc
  log_file_test.cc
  platform_requirements_test.cc
  scoped_call_test.cc
  sleep_test.cc
  type_info_test.cc
  unicode_test.cc
  validated_context_test.cc
  weak_ptr_test.cc
  ${gb_base_internal_TEST_SOURCE}
)
set(gb_base_TEST_DEPS
  gb_test
)
# MSVC, rather than a compiler ID check, as clang-cl needs /bigobj too.
if (MSVC)
  set_source_files_properties(validated_context_test.cc
    PROPERTIES COMPILE_FLAGS /bigobj
  )
endif()

set(gb_base_DEPS
  absl::any_invocable
  absl::base
  absl::check
  absl::flat_hash_map
  absl::log
  absl::log_entry
  absl::log_initialize
  absl::log_severity
  absl::log_sink
  absl::log_sink_registry
  absl::memory
  absl::span
  absl::str_format
  absl::strings
  absl::synchronization
  absl::time
)

gb_add_library(gb_base)

target_include_directories(gb_base PRIVATE "${GB_THIRD_PARTY_DIR}/utfcpp/source")
if (GB_BUILD_TESTS)
  gb_embed_data(gb_base_test embedded_data_test.bin
    gb/base/embedded_data_test_bytes.h gb::kTestBytes)
  gb_embed_data(gb_base_test embedded_data_test_empty.txt
    gb/base/embedded_data_test_empty.h kGbTestEmpty)
  gb_embed_data(gb_base_test embedded_data_test.txt
    gb/base/embedded_data_test_text.h gb::embedded_test::kText)
  target_include_directories(gb_base_test PRIVATE "${GB_THIRD_PARTY_DIR}/utfcpp/source")
endif()
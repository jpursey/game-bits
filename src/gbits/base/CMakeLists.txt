set(gbits_base_SOURCE
  context.cc context.h
  context_builder.h
  context_type.h
  validated_context.cc validated_context.h
)

set(gbits_base_TEST_SOURCE
  context_test.cc
  validated_context_test.cc
)
set_source_files_properties(validated_context_test.cc
  PROPERTIES COMPILE_FLAGS /bigobj
)

set(gbits_base_DEPS
  absl::flat_hash_map
  absl::memory
  absl::strings
)

set(gbits_base_LIBS
  glog
)

gbits_add_library(gbits_base)

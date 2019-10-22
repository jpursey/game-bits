set(base_SOURCE
	context.cc context.h
	context_type.h
)

set(base_TEST_SOURCE
	context_test.cc
)

set(base_INCLUDES
	${GBITS_ABSL_INCLUDE}
)

set(base_LIBS
	${GBITS_ABSL_LIB}
)

add_gbits_library(base)

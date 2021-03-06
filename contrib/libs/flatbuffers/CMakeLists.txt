set(SRCS
	dummy.cpp
	flatbuffers/flatbuffers.h
	flatbuffers/flexbuffers.h
	flatbuffers/hash.h
	flatbuffers/idl.h
	flatbuffers/util.h
)

engine_add_library(
	LIB flatbuffers
	SRCS ${SRCS}
)
if (NOT FLATBUFFERS_FOUND)
	target_include_directories(flatbuffers PUBLIC .)
endif()
set(COMPILER_SRCS
	compiler/src/compiler/cpp_generator.cc
	compiler/src/compiler/cpp_generator.h
	compiler/src/compiler/go_generator.cc
	compiler/src/compiler/go_generator.h
	compiler/code_generators.cpp
	compiler/flatc.cpp
	compiler/flatc_main.cpp
	compiler/idl_gen_cpp.cpp
	compiler/idl_gen_fbs.cpp
	compiler/idl_gen_general.cpp
	compiler/idl_gen_go.cpp
	compiler/idl_gen_grpc.cpp
	compiler/idl_gen_js.cpp
	compiler/idl_gen_json_schema.cpp
	compiler/idl_gen_php.cpp
	compiler/idl_gen_python.cpp
	compiler/idl_gen_text.cpp
	compiler/idl_parser.cpp
	compiler/reflection.cpp
	compiler/util.cpp
)
engine_add_executable(TARGET flatc SRCS ${COMPILER_SRCS})
target_link_libraries(flatc flatbuffers)
target_include_directories(flatc PRIVATE compiler/)

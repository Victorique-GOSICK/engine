set(SRCS
	IMGUIApp.cpp IMGUIApp.h
	IMGUI.cpp IMGUI.h
	IMGUIInternal.h
	IMGUIAddons.h
)
set(LIB imgui)
add_library(${LIB} ${SRCS})
generate_shaders(${LIB} texture)
engine_target_link_libraries(TARGET ${LIB} DEPENDENCIES core io video dearimgui)
set_target_properties(${LIB} PROPERTIES FOLDER ${LIB})
target_include_directories(${LIB} PUBLIC .)

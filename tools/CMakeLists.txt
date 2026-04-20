# Standalone offline tools. No iPlug2 linkage — pure C++17 stdlib.
add_executable(render_ir render_ir.cpp)
set_target_properties(render_ir PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
if(NOT MSVC)
  target_compile_options(render_ir PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

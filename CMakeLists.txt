cmake_minimum_required(VERSION 3.10)
add_definitions(-DUNICODE -D_UNICODE)

project("world")

# MSVC 핫 리로드 설정
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# src/ 내부 모든 cpp 파일 재귀적으로 찾기
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# 실행 파일 생성
add_executable(world WIN32
    ${SRC_FILES}

    "external/inih/ini.c"
) 

# C++20 설정
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET world PROPERTY CXX_STANDARD 20)
endif() 

# includes/ 전체 포함
target_include_directories(world PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/includes"
)

# soloud
cmake_policy(PUSH)
cmake_policy(VERSION 3.10)
set(SOLOUD_BACKEND_SDL2 OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_WASAPI ON CACHE BOOL "" FORCE)
add_subdirectory(external/soloud/contrib)
cmake_policy(POP)

target_link_libraries(world PRIVATE soloud)
target_include_directories(world PRIVATE
    external/soloud/include
    external/inih/
)
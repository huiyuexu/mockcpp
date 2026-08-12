ENABLE_LANGUAGE(ASM)
FIND_PACKAGE(Threads REQUIRED)

ADD_LIBRARY(mockcpp-aarch64-cross-so-target SHARED
  CrossSoTarget.cpp
  CrossSoArchitectureTargets.S)
TARGET_LINK_LIBRARIES(mockcpp-aarch64-cross-so-target Threads::Threads)
SET_TARGET_PROPERTIES(mockcpp-aarch64-cross-so-target PROPERTIES
  COMPILE_FLAGS "-O0 -fplt -fno-lto"
  LINK_FLAGS "-Wl,-Bsymbolic-functions")

ADD_LIBRARY(mockcpp-aarch64-cross-so-target-now SHARED
  CrossSoTarget.cpp
  CrossSoArchitectureTargets.S)
TARGET_LINK_LIBRARIES(mockcpp-aarch64-cross-so-target-now Threads::Threads)
SET_TARGET_PROPERTIES(mockcpp-aarch64-cross-so-target-now PROPERTIES
  COMPILE_FLAGS "-O0 -fplt -fno-lto"
  LINK_FLAGS "-Wl,-Bsymbolic-functions,-z,now")

FUNCTION(ADD_AARCH64_CROSS_SO_REGRESSION target binding_flag helper_target)
   ADD_EXECUTABLE(${target} CrossSoRegression.cpp)
   TARGET_LINK_LIBRARIES(${target}
     mockcpp
     ${helper_target}
    Threads::Threads
    ${CMAKE_DL_LIBS})
  SET_TARGET_PROPERTIES(${target} PROPERTIES
    COMPILE_FLAGS "-O0 -fno-pic -fno-pie -fplt -fno-lto"
    LINK_FLAGS "-no-pie -Wl,${binding_flag}"
    BUILD_RPATH "$ORIGIN")
ENDFUNCTION()

ADD_AARCH64_CROSS_SO_REGRESSION(
  mockcpp-aarch64-cross-so-lazy
  "-z,lazy"
  mockcpp-aarch64-cross-so-target)
ADD_AARCH64_CROSS_SO_REGRESSION(
  mockcpp-aarch64-cross-so-now
  "-z,now"
  mockcpp-aarch64-cross-so-target-now)

ENABLE_LANGUAGE(ASM)

ADD_LIBRARY(mockcpp-aarch64-cross-so-target SHARED
  CrossSoTarget.cpp
  CrossSoArchitectureTargets.S)
SET_TARGET_PROPERTIES(mockcpp-aarch64-cross-so-target PROPERTIES
  COMPILE_FLAGS "-O0 -fno-lto"
  LINK_FLAGS "-Wl,-Bsymbolic-functions")

FUNCTION(ADD_AARCH64_CROSS_SO_REGRESSION target binding_flag)
  ADD_EXECUTABLE(${target} CrossSoRegression.cpp)
  TARGET_LINK_LIBRARIES(${target}
    mockcpp
    mockcpp-aarch64-cross-so-target
    ${CMAKE_DL_LIBS})
  SET_TARGET_PROPERTIES(${target} PROPERTIES
    COMPILE_FLAGS "-O0 -fno-pic -fno-pie -fplt -fno-lto"
    LINK_FLAGS "-no-pie -Wl,${binding_flag}"
    BUILD_RPATH "$ORIGIN")
ENDFUNCTION()

ADD_AARCH64_CROSS_SO_REGRESSION(
  mockcpp-aarch64-cross-so-lazy "-z,lazy")
ADD_AARCH64_CROSS_SO_REGRESSION(
  mockcpp-aarch64-cross-so-now "-z,now")

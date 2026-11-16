# Opt-in GPL-aware build; source remains separate from the default project.
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(rubberband_source
  URL https://codeload.github.com/breakfastquay/rubberband/tar.gz/e4296ac80b1170018a110bc326fd0d45a0eb27d6
  URL_HASH SHA256=32770ab6a395badb6a5ee3443bf5a0cfd5cac07f332af8e03fd6dcd61f5b3759)
FetchContent_MakeAvailable(rubberband_source)
add_library(pitch_rubberband STATIC ${rubberband_source_SOURCE_DIR}/single/RubberBandSingle.cpp)
set_target_properties(pitch_rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pitch_rubberband PUBLIC ${rubberband_source_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pitch_rubberband PUBLIC pitch_core)
if(APPLE)
  target_link_libraries(pitch_rubberband PUBLIC "-framework Accelerate")
endif()
if(BUILD_TESTING)
  add_executable(rubberband_smoke ${PROJECT_SOURCE_DIR}/tests/rubberband_smoke.cpp)
  target_link_libraries(rubberband_smoke PRIVATE pitch_rubberband)
  add_test(NAME rubberband_smoke COMMAND rubberband_smoke)
  add_executable(rubberband_live_smoke ${PROJECT_SOURCE_DIR}/tests/rubberband_live_smoke.cpp)
  target_link_libraries(rubberband_live_smoke PRIVATE pitch_rubberband)
  add_test(NAME rubberband_live_smoke COMMAND rubberband_live_smoke)
endif()
if(PITCH_BUILD_SC)
  add_library(RubberBandShifts MODULE RubberBandShifts.cpp RubberBandLiveShift.cpp)
  target_link_libraries(RubberBandShifts PRIVATE pitch_rubberband)
  target_include_directories(RubberBandShifts PRIVATE ${SC_SOURCE_DIR}/include/plugin_interface
    ${SC_SOURCE_DIR}/include/common ${SC_SOURCE_DIR}/common)
  set_target_properties(RubberBandShifts PROPERTIES PREFIX "")
  if(APPLE OR WIN32)
    set_target_properties(RubberBandShifts PROPERTIES SUFFIX ".scx")
  endif()
  install(TARGETS RubberBandShifts LIBRARY DESTINATION RubberBandShifts COMPONENT RubberBandShifts
    RUNTIME DESTINATION RubberBandShifts COMPONENT RubberBandShifts)
  install(FILES RubberBandShifts.sc DESTINATION RubberBandShifts/Classes COMPONENT RubberBandShifts)
  install(FILES ${PROJECT_SOURCE_DIR}/sc/HelpSource/Classes/RubberBandR2Shift.schelp
    ${PROJECT_SOURCE_DIR}/sc/HelpSource/Classes/RubberBandR3Shift.schelp
    ${PROJECT_SOURCE_DIR}/sc/HelpSource/Classes/RubberBandLiveShift.schelp
    DESTINATION RubberBandShifts/HelpSource/Classes COMPONENT RubberBandShifts)
  install(FILES ${rubberband_source_SOURCE_DIR}/COPYING README.md DESTINATION RubberBandShifts/licenses COMPONENT RubberBandShifts)
endif()

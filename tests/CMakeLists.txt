add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")

mark_as_advanced(
    BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
    gmock_build_tests gtest_build_samples gtest_build_tests
    gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

set_target_properties(gtest PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

find_package(yaml-cpp)
FIND_PACKAGE( Boost 1.55 COMPONENTS REQUIRED )

find_library(GPIOD_LIBRARY NAMES libgpiod.so)
if(NOT GPIOD_LIBRARY)
  message(FATAL_ERROR "gpiod library not found.")
endif()
find_library(GPIODCXX_LIBRARY NAMES libgpiodcxx.so)
if(NOT GPIODCXX_LIBRARY)
  message(FATAL_ERROR "gpiodcxx library not found.")
endif()
find_path(GPIOD_INCLUDE_DIR gpiod.hpp)
if(NOT GPIOD_INCLUDE_DIR)
  message(FATAL_ERROR "gpiod headers not found.")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

macro(package_add_test_with_libraries TESTNAME FILES LIBRARIES)
    add_executable(${TESTNAME} ${FILES})
    include_directories(../src/ ${YAML_CPP_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${GPIOD_INCLUDE_DIR})
    target_link_libraries(${TESTNAME} gtest gmock gtest_main ${LIBRARIES})
    gtest_discover_tests(${TESTNAME}
        WORKING_DIRECTORY ${TEST_WORKING_DIRECTORY}
        PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()
set(FILES main.cpp Signal_test.cpp StateMachine_test.cpp  Logic_test.cpp VoltageRegulator_test.cpp
		../src/Config.cpp ../src/StateMachine.cpp ../src/GpioInput.cpp ../src/GpioOutput.cpp ../src/Signal.cpp
  		../src/SignalProvider.cpp ../src/Logic.cpp ../src/LogicInput.cpp ../src/NullInput.cpp ../src/NullOutput.cpp
		../src/VoltageRegulator.cpp ../src/ACPIStates.cpp ../src/Dbus.cpp ../src/SysFsWatcher.cpp)
set(LIBRARIES ${YAML_CPP_LIBRARIES} ${GPIODCXX_LIBRARY} Threads::Threads)
if(SDBUSPLUSPLUS_FOUND)
	set(LIBRARIES ${YAML_CPP_LIBRARIES} ${GPIODCXX_LIBRARY} Threads::Threads sdbusplus -lsystemd)
endif()
include_directories(../src ${YAML_CPP_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${GPIOD_INCLUDE_DIR})

package_add_test_with_libraries(test_all "${FILES}" "${LIBRARIES}")

set(FILES main.cpp LogicRuntimeTest.cpp
		../src/Config.cpp ../src/StateMachine.cpp ../src/GpioInput.cpp ../src/GpioOutput.cpp ../src/Signal.cpp
  		../src/SignalProvider.cpp ../src/Logic.cpp ../src/LogicInput.cpp ../src/NullInput.cpp ../src/NullOutput.cpp
		../src/VoltageRegulator.cpp ../src/ACPIStates.cpp ../src/Dbus.cpp ../src/SysFsWatcher.cpp)
set(LIBRARIES ${YAML_CPP_LIBRARIES} ${GPIODCXX_LIBRARY} Threads::Threads)
if(SDBUSPLUSPLUS_FOUND)
	set(LIBRARIES ${YAML_CPP_LIBRARIES} ${GPIODCXX_LIBRARY} Threads::Threads sdbusplus -lsystemd)
endif()

include_directories(../src ${YAML_CPP_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${GPIOD_INCLUDE_DIR})

package_add_test_with_libraries(test_waveform "${FILES}" "${LIBRARIES}")



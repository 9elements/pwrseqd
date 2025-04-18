cmake_minimum_required(VERSION 3.5)
project(pwrseqd)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
	# Update submodules as needed
	option(GIT_SUBMODULE "Check submodules during build" ON)
		if(GIT_SUBMODULE)
			message(STATUS "Submodule update")
			execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
					WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
					RESULT_VARIABLE GIT_SUBMOD_RESULT)
		if(NOT GIT_SUBMOD_RESULT EQUAL "0")
			message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
		endif()
	endif()
endif()
find_package(PkgConfig REQUIRED)

find_package(NL REQUIRED)
if(NL_FOUND)
	include_directories(${NL_INCLUDE_DIRS})
	link_directories(${NL_LIBRARY_DIRS})
endif()

find_package(yaml-cpp)
FIND_PACKAGE( Boost 1.55 COMPONENTS  REQUIRED )

pkg_check_modules(LIBGPIODCXX libgpiodcxx>=1.5)
if(LIBGPIODCXX_FOUND)
        add_definitions(-DWITH_GPIOD_PULLUPS)
endif()
if(NOT LIBGPIODCXX_FOUND)
	pkg_check_modules(LIBGPIODCXX REQUIRED libgpiodcxx)
endif()
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)
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
pkg_check_modules(PHOSPHOR_LOGGING phosphor-logging)
if(PHOSPHOR_LOGGING_FOUND)
	include_directories(${LOGGING_INCLUDE_DIRS})
	link_directories(${LOGGING_LIBRARY_DIRS})
	add_definitions(-DWITH_PHOSPHOR_LOGGING=1)
endif()
pkg_check_modules(SDBUSPLUSPLUS sdbusplus)
if(SDBUSPLUSPLUS_FOUND)
	include_directories(${SDBUSPLUSPLUS_INCLUDE_DIRS})
	link_directories(${SDBUSPLUSPLUS_LIBRARY_DIRS})
	find_program(SDBUSPLUSPLUS sdbus++)
	add_definitions(-DWITH_SDBUSPLUSPLUS=1)
endif()
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
	enable_testing()
	include(GoogleTest)
	add_subdirectory(tests)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # enforces standard compliance

include_directories(src/ ${YAML_CPP_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} ${GPIOD_INCLUDE_DIR} extern/inotify-cpp/src/include)# extern/googletest/googletest/include)

add_executable(${CMAKE_PROJECT_NAME} src/Main.cpp src/Logging.cpp src/Config.cpp src/StateMachine.cpp src/GpioInput.cpp src/GpioOutput.cpp src/Signal.cpp
	src/SignalProvider.cpp src/Logic.cpp src/LogicInput.cpp src/NullInput.cpp src/NullOutput.cpp src/VoltageRegulator.cpp src/LED.cpp
	src/ACPIStates.cpp src/Dbus.cpp src/InventoryItemPresence.cpp src/VoltageRegulatorSysfs.cpp src/Netlink.cpp src/NullVoltageRegulator.cpp
	src/LogOutput.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME} yaml-cpp )
target_link_libraries(${CMAKE_PROJECT_NAME} ${GPIODCXX_LIBRARY})
target_link_libraries(${CMAKE_PROJECT_NAME} Threads::Threads)
if(SDBUSPLUSPLUS_FOUND)
	target_link_libraries(${CMAKE_PROJECT_NAME} sdbusplus)
	target_link_libraries(${CMAKE_PROJECT_NAME} -lsystemd)
endif()
if(PHOSPHOR_LOGGING_FOUND)
	target_link_libraries(${CMAKE_PROJECT_NAME} phosphor_logging)
endif()
if(NL_FOUND)
	target_link_libraries(${CMAKE_PROJECT_NAME} ${NL_LIBRARIES})
endif()

set(
  SERVICE_FILES
  ${PROJECT_SOURCE_DIR}/service_files/chassis-system-reset.service
  ${PROJECT_SOURCE_DIR}/service_files/chassis-system-reset.target
  ${PROJECT_SOURCE_DIR}/service_files/xyz.openbmc_project.Chassis.Control.Power@.service
  )

set(
  CONFIG_FILE_GEN3
  ${PROJECT_SOURCE_DIR}/config/genesis3/pch.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/states.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/chassis.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/cpu0_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/cpu1_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/cpu2_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/cpu3_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/rulerdrive.yaml
  ${PROJECT_SOURCE_DIR}/config/genesis3/nic.yaml
  )

set(
  CONFIG_FILE_SBP1
  ${PROJECT_SOURCE_DIR}/config/sbp1/pch.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/states.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/chassis.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/cpu0_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/cpu1_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/cpu2_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/cpu3_pwr.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rulerdrive.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/nic.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd01.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd02.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd03.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd04.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd05.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd06.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd07.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd08.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd09.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd10.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd11.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd12.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd13.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd14.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd15.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd16.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd17.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd18.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd19.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd20.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd21.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd22.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd23.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd24.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd25.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd26.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd27.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd28.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd29.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd30.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd31.yaml
  ${PROJECT_SOURCE_DIR}/config/sbp1/rssd32.yaml
  )

install(FILES ${SERVICE_FILES} DESTINATION /usr/lib/systemd/system/)
install(FILES ${CONFIG_FILE_GEN3} DESTINATION /usr/share/pwrseqd/genesis3/)
install(FILES ${CONFIG_FILE_SBP1} DESTINATION /usr/share/pwrseqd/sbp1/)
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION bin)

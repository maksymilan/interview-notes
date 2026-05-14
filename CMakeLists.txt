cmake_minimum_required(VERSION 3.16)
project(interview_notes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(dag_task_scheduler_lib
	questions/01_dag_task_scheduler/dag_task_scheduler.cpp
	utils/utils.cpp
)

target_include_directories(dag_task_scheduler_lib PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(dag_task_scheduler_lib PUBLIC cxx_std_20)

add_library(smart_pointer_lib
	questions/02_smart_pointer/smart_pointer.cpp
)

target_compile_features(smart_pointer_lib PUBLIC cxx_std_20)

if(BUILD_TESTING)
	include(FetchContent)

	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
	)
	FetchContent_MakeAvailable(googletest)

	add_executable(dag_task_scheduler_test
		test/dag_task_scheduler_test.cpp
		utils/utils.cpp
	)

	target_include_directories(dag_task_scheduler_test PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
	)

	target_link_libraries(dag_task_scheduler_test PRIVATE
		GTest::gtest_main
	)

	include(GoogleTest)
	gtest_discover_tests(dag_task_scheduler_test)
endif()

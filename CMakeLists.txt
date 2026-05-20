cmake_minimum_required(VERSION 3.16)
project(interview_notes LANGUAGES C CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
option(INTERVIEW_NOTES_BUILD_GTEST "Build tests that require fetching GoogleTest" OFF)

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

add_executable(smart_pointer_demo
	questions/02_smart_pointer/smart_pointer.cpp
)

target_compile_features(smart_pointer_demo PUBLIC cxx_std_20)

add_library(spsc_queue_lib
	questions/03_SPSC/lockfree_queue_SPSC.cpp
)

target_compile_features(spsc_queue_lib PUBLIC cxx_std_20)

add_executable(spsc_queue_correctness
	test/spsc_queue_correctness.cpp
)

target_include_directories(spsc_queue_correctness PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(spsc_queue_correctness PUBLIC cxx_std_20)

add_executable(spsc_queue_benchmark
	test/spsc_queue_benchmark.cpp
)

target_include_directories(spsc_queue_benchmark PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(spsc_queue_benchmark PUBLIC cxx_std_20)

add_executable(shared_memory_player
	questions/05_shared_memory/shared_memory_player.c
)

target_compile_features(shared_memory_player PUBLIC c_std_11)
target_link_libraries(shared_memory_player PRIVATE Threads::Threads)

if(BUILD_TESTING)
	add_executable(shared_memory_player_test
		test/shared_memory_player_test.c
	)

	target_compile_features(shared_memory_player_test PUBLIC c_std_11)
	target_link_libraries(shared_memory_player_test PRIVATE Threads::Threads)

	add_test(NAME shared_memory_player_test COMMAND shared_memory_player_test)

	if(INTERVIEW_NOTES_BUILD_GTEST)
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
endif()

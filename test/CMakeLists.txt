# GTest
find_package(GTest QUIET)

if (NOT GTEST_FOUND)
    fetchcontent_declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.11.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "Always use msvcrt.dll")
    fetchcontent_makeavailable(googletest)
    set(GTEST_BOTH_LIBRARIES GTest::gtest GTest::gtest_main)
endif ()

link_libraries(GTest::gtest GTest::gtest_main)

add_compile_options(${VARIANT_COMPILE_OPTIONS})
add_link_options(${VARIANT_LINK_OPTIONS})

set(VARIANT_UNIT_TESTS
        variant_smoke_test
        visitor_smoke_test
        )

set(VARIANT_TEST_SOURCES ${VARIANT_UNIT_TESTS})

foreach (TEST_SOURCE ${VARIANT_TEST_SOURCES})
    string(REPLACE "/" "_" TEST_NAME ${TEST_SOURCE})
    add_executable(${TEST_NAME} ${TEST_SOURCE}.cpp)
    target_include_directories(${TEST_NAME}
            PRIVATE ${VARIANT_SOURCE_DIR}/include
            )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach ()

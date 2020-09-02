cmake_minimum_required(VERSION 3.15)
project(horizon_callable VERSION 0.0.0 LANGUAGES CXX C)

include(HorizonCommon)


include_directories(include/)

set(HEADER
        )

set(SOURCE
        )

set(HORIZON_CALLABLE_COMPILE_SOURCE ${HEADER} ${SOURCE})


add_library(horizon_callable ${HORIZON_CALLABLE_COMPILE_SOURCE})
target_link_libraries(horizon_callable )
target_include_directories(horizon_callable PUBLIC include/)
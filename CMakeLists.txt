cmake_minimum_required(VERSION 3.15)
project(horizon_callable VERSION 0.0.0 LANGUAGES CXX C)

# set dependent source files
set(HEADER
        include/callable/Job.hpp
        include/callable/CallbackManager.hpp)

set(SOURCE
        source/CallbackManager.cpp)

MakeHorizonModule(COMPILE_SOURCE ${HEADER} ${SOURCE}
        PUBLIC_DEPENDENCIES horizon::preprocessor)


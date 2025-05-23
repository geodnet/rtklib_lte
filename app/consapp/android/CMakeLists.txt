cmake_minimum_required(VERSION 3.16)
project(srtkpp)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DTRACE -DENAGLO -DENAQZS -DENAGAL -DENACMP -DENAIRN -DNFREQ=7 -DNEXOBS=3)
include_directories(./)
include_directories(../../../src)
include_directories(../../../src/rcv)
add_executable(androidRTKPP
    androidRTKPP.cpp
    androidRTK.h
    androidRTK.cpp
    ../../../src/rtklib.h   
    ../../../src/rtkcmn.c
    ../../../src/rinex.c   
    ../../../src/rtkpos.c   
    ../../../src/postpos.c  
    ../../../src/solution.c 
    ../../../src/lambda.c   
    ../../../src/sbas.c     
    ../../../src/preceph.c  
    ../../../src/pntpos.c   
    ../../../src/geoid.c
    ../../../src/ephemeris.c
    ../../../src/options.c  
    ../../../src/ppp.c      
    ../../../src/ppp_ar.c   
    ../../../src/rtcm.c     
    ../../../src/rtcm2.c    
    ../../../src/rtcm3.c    
    ../../../src/rtcm3e.c   
    ../../../src/ionex.c    
    ../../../src/tides.c   
    )

target_link_libraries(androidRTKPP m)

# cmake --build . --config Release

cmake_minimum_required(VERSION 3.16)
project(rtklib)

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DTRACE -DENAGLO -DENAQZS -DENAGAL -DENACMP -DENAIRN -DENALEO -DNFREQ=7 -DNEXOBS=3)
include_directories(app/consapp/convbin)
include_directories(app/consapp/rnx2rtkp)
include_directories(src)
include_directories(src/rcv)

add_executable(convbin
    app/consapp/convbin/convbin.c
    src/rtklib.h   
    src/rtkcmn.c 
    src/rtcm.c 
    src/rtcm2.c 
    src/rtcm3.c 
    src/rtcm3e.c 
    src/preceph.c 
    src/sbas.c 
    src/ephemeris.c 
    src/rcvraw.c 
    src/rinex.c
    src/ionex.c
    src/convrnx.c
    src/pntpos.c
    src/geoid.c
    src/rcv/binex.c 
    src/rcv/crescent.c 
    src/rcv/javad.c 
    src/rcv/novatel.c 
    src/rcv/nvs.c 
    src/rcv/rt17.c 
    src/rcv/septentrio.c 
    src/rcv/skytraq.c 
    src/rcv/ss2.c 
    src/rcv/ublox.c
    )

add_executable(rnx2rtkp
    app/consapp/rnx2rtkp/rnx2rtkp.c
    src/rtklib.h   
    src/rtkcmn.c
    src/rinex.c   
    src/rtkpos.c   
    src/postpos.c  
    src/solution.c 
    src/lambda.c   
    src/sbas.c     
    src/preceph.c  
    src/pntpos.c   
    src/geoid.c
    src/ephemeris.c
    src/options.c  
    src/ppp.c      
    src/ppp_ar.c   
    src/rtcm.c     
    src/rtcm2.c    
    src/rtcm3.c    
    src/rtcm3e.c   
    src/ionex.c    
    src/tides.c   
    )

if(WIN32)
    target_link_libraries(convbin  Winmm.lib)
    target_link_libraries(rnx2rtkp Winmm.lib)
else()
    target_link_libraries(convbin  m)
    target_link_libraries(rnx2rtkp m)
endif()

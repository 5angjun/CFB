add_library(SANGJUN_driver SHARED driver.c)

set_target_properties(SANGJUN_driver PROPERTIES
    OUTPUT_NAME "SANGJUN"
    SUFFIX ".sys"
    LINK_FLAGS "/SUBSYSTEM:NATIVE /ENTRY:DriverEntry"
    INTERPROCEDURAL_OPTIMIZATION FALSE
)

target_compile_definitions(SANGJUN_driver PRIVATE _AMD64_)

target_include_directories(SANGJUN_driver PRIVATE
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/km"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um"
)

target_link_directories(SANGJUN_driver PRIVATE 
    "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/km/x64"
)

target_link_libraries(SANGJUN_driver PRIVATE ntoskrnl.lib)

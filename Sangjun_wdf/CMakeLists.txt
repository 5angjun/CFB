add_library(SANGJUN_wdf_driver SHARED driver_wdf.c)

set_target_properties(SANGJUN_wdf_driver PROPERTIES
    OUTPUT_NAME "SANGJUN_WDF"
    SUFFIX ".sys"
    LINK_FLAGS "/SUBSYSTEM:NATIVE /ENTRY:DriverEntry"
)

target_compile_definitions(SANGJUN_wdf_driver PRIVATE _AMD64_)

target_include_directories(SANGJUN_wdf_driver PRIVATE
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/km"
    "C:/Program Files (x86)/Windows Kits/10/Include/wdf/kmdf/1.33"
)

target_link_directories(SANGJUN_wdf_driver PRIVATE
    "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/km/x64" # ✅ 반드시 있어야 함
    "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
    "C:/Program Files (x86)/Windows Kits/10/Lib/wdf/kmdf/x64/1.33"
)

target_link_libraries(SANGJUN_wdf_driver PRIVATE
    ntoskrnl.lib           # ✅ 커널 심볼들
    wdfdriverentry.lib     # ✅ WDF 드라이버 진입점
    wdfldr.lib             # ✅ WDF 로더
)

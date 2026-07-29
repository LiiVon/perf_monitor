include("D:/LZ_Projects/forQT/monitor_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/monitor_UI-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/LZ_Projects/forQT/monitor_UI/build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/monitor_UI.exe"
    GENERATE_QT_CONF
)

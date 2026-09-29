cmake_minimum_required(VERSION 3.24)
project(jiit-firewall VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Network Xml Test)

add_library(jiitcore
    cpp/include/jiit/network.h cpp/include/jiit/sophos.h cpp/include/jiit/xray.h cpp/include/jiit/service.h cpp/include/jiit/dbus_adaptor.h
    cpp/src/config.cpp cpp/src/network.cpp cpp/src/sophos.cpp cpp/src/wallet.cpp
    cpp/src/xray.cpp cpp/src/service.cpp cpp/src/dbus_adaptor.cpp)
target_include_directories(jiitcore PUBLIC cpp/include)
target_link_libraries(jiitcore PUBLIC Qt6::Core Qt6::DBus Qt6::Network Qt6::Xml)
target_compile_options(jiitcore PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jiit-firewall-daemon cpp/src/daemon_main.cpp)
target_link_libraries(jiit-firewall-daemon PRIVATE jiitcore)
enable_testing()
add_executable(jiit-firewall-tests cpp/tests/test_core.cpp)
target_link_libraries(jiit-firewall-tests PRIVATE jiitcore Qt6::Test)
add_test(NAME jiit-firewall-tests COMMAND jiit-firewall-tests)

include(GNUInstallDirs)
install(TARGETS jiit-firewall-daemon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES systemd/jiit-firewall.service DESTINATION ${CMAKE_INSTALL_DATADIR}/systemd/user)
install(FILES dbus/org.jiit.FirewallManager.service DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES dbus/org.jiit.FirewallManager.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/jiit-firewall/dbus)
install(DIRECTORY package/ DESTINATION ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/com.gaurishhs.wifi)

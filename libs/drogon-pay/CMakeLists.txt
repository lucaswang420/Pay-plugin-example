# ##############################################################################
# drogon-pay: the reusable Drogon payment plugin library (services, channels,
# ORM models, utils, HTTP handlers). Routes are registered programmatically by
# PayPlugin (no ADD_METHOD_TO), so the library is safe to consume as a STATIC
# library without symbol-drop concerns. Exported as DrogonPay::DrogonPay via
# find_package(DrogonPay).
# ##############################################################################
add_library(drogon-pay STATIC
    src/ChannelRegistry.cc
    src/PayPlugin.cc
    src/channels/AlipayChannel.cc
    src/channels/WechatChannel.cc
    src/handlers/AuthCheck.cc
    src/handlers/CallbackHandlers.cc
    src/handlers/PayAuthMetrics.cc
    src/handlers/PayHandlers.cc
    src/handlers/PayMetricsHandlers.cc
    src/models/PayCallback.cc
    src/models/PayIdempotency.cc
    src/models/PayLedger.cc
    src/models/PayOrder.cc
    src/models/PayPayment.cc
    src/models/PayRefund.cc
    src/services/CallbackService.cc
    src/services/IdempotencyService.cc
    src/services/PaymentService.cc
    src/services/ReconciliationService.cc
    src/services/RefundService.cc
    src/utils/PayUtils.cc
)

# Alias kept for in-tree consumers (example host, tests) that predate the
# formal library name.
add_library(pay_core ALIAS drogon-pay)
add_library(DrogonPay::DrogonPay ALIAS drogon-pay)

# include/  : public API surface (drogon_pay/*.h) — the only headers hosts see
#             and the only headers installed.
# src/      : internal headers; BUILD_INTERFACE-only so the white-box test
#             suite can include services/channels/handlers directly, while
#             installed consumers never see them.
target_include_directories(drogon-pay PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(drogon-pay PUBLIC Drogon::Drogon OpenSSL::Crypto OpenSSL::SSL)

# The generated ORM model .cc files include orm_compat.h directly (root-cause
# fix for the old /FI /-include force-include hack). Keep the codecvt
# deprecation silencers PUBLIC so any consumer compiling against the models
# builds warning-clean.
if(MSVC)
    target_compile_options(drogon-pay PUBLIC /utf-8)
    target_compile_definitions(drogon-pay PUBLIC
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_definitions(drogon-pay PUBLIC
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
endif()

# Elevated warnings for the library target only (PRIVATE: never leaks to
# consumers). Advisory bar for new code; deliberately not warnings-as-errors
# so pre-existing warnings don't break the build.
target_compile_options(drogon-pay PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ------------------------------------------------------------------------------
# Install & export: find_package(DrogonPay) -> DrogonPay::DrogonPay
# ------------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS drogon-pay
    EXPORT DrogonPayTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/drogon_pay
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT DrogonPayTargets
    FILE DrogonPayTargets.cmake
    NAMESPACE DrogonPay::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DrogonPay
)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/DrogonPayConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonPayConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DrogonPay
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonPayConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonPayConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DrogonPayConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DrogonPay
)

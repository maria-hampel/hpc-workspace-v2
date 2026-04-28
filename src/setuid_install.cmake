file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin/ws_allocate" _WS_SUID_ALLOCATE)
execute_process(
    COMMAND chown "${_WS_CHOWN_OWNER}:${_WS_CHOWN_GROUP}" "${_WS_SUID_ALLOCATE}"
    RESULT_VARIABLE _WS_CHOWN_ALLOCATE_RC)
if(_WS_CHOWN_ALLOCATE_RC)
    message(WARNING "chown failed for ws_allocate")
endif()
execute_process(
    COMMAND chmod u+s "${_WS_SUID_ALLOCATE}"
    RESULT_VARIABLE _WS_SUILD_ALLOCATE_RC)
if(_WS_SUILD_ALLOCATE_RC)
    message(WARNING "chmod u+s failed for ws_allocate")
endif()
if(NOT _WS_SUILD_ALLOCATE_RC)
    message(STATUS "setuid bit applied to ws_allocate")
endif()

file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin/ws_release" _WS_SUID_RELEASE)
execute_process(
    COMMAND chown "${_WS_CHOWN_OWNER}:${_WS_CHOWN_GROUP}" "${_WS_SUID_RELEASE}"
    RESULT_VARIABLE _WS_CHOWN_RELEASE_RC)
if(_WS_CHOWN_RELEASE_RC)
    message(WARNING "chown failed for ws_release")
endif()
execute_process(
    COMMAND chmod u+s "${_WS_SUID_RELEASE}"
    RESULT_VARIABLE _WS_SUILD_RELEASE_RC)
if(_WS_SUILD_RELEASE_RC)
    message(WARNING "chmod u+s failed for ws_release")
endif()
if(NOT _WS_SUILD_RELEASE_RC)
    message(STATUS "setuid bit applied to ws_release")
endif()

file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin/ws_restore" _WS_SUID_RESTORE)
execute_process(
    COMMAND chown "${_WS_CHOWN_OWNER}:${_WS_CHOWN_GROUP}" "${_WS_SUID_RESTORE}"
    RESULT_VARIABLE _WS_CHOWN_RESTORE_RC)
if(_WS_CHOWN_RESTORE_RC)
    message(WARNING "chown failed for ws_restore")
endif()
execute_process(
    COMMAND chmod u+s "${_WS_SUID_RESTORE}"
    RESULT_VARIABLE _WS_SUILD_RESTORE_RC)
if(_WS_SUILD_RESTORE_RC)
    message(WARNING "chmod u+s failed for ws_restore")
endif()
if(NOT _WS_SUILD_RESTORE_RC)
    message(STATUS "setuid bit applied to ws_restore")
endif()

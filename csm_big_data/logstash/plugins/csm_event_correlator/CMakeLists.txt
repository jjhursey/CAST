#================================================================================
#
#    csm_big_data/Logstash/plugins/grok_dynamic/CMakeLists.txt
#
#    © Copyright IBM Corporation 2015-2018. All Rights Reserved
#
#    This program is licensed under the terms of the Eclipse Public License
#    v1.0 as published by the Eclipse Foundation and available at
#    http://www.eclipse.org/legal/epl-v10.html
#
#    U.S. Government Users Restricted Rights:  Use, duplication or disclosure
#    restricted by GSA ADP Schedule Contract with IBM Corp.
#
#================================================================================


execute_process(
    COMMAND bash  "-c" "pwd; ./build.sh"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

set(SUBDIR logstash/plugins/)

# Files that should not be executable when shipped
file(GLOB INSTALL_FILES
    "logstash-filter-csm-event-correlator*.zip"
)

install(FILES ${INSTALL_FILES} COMPONENT ${LOGSTASH_RPM_NAME} DESTINATION ${BDS_BASE_NAME}/${SUBDIR})

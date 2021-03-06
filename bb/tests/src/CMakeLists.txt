function(BUILD_TEST testname)
  add_executable(${testname} ${ARGV})
  target_link_libraries(${testname} bbAPI)
  install(TARGETS ${testname} COMPONENT burstbuffer-tests DESTINATION bb/tests/bin)
endfunction()

function(INSTALL_SCRIPT testname)
  install(PROGRAMS ${testname} COMPONENT burstbuffer-tests DESTINATION bb/tests/bin)
endfunction()

find_path(MPI_BIN NAMES mpicc
          PATHS /opt/ibm/spectrum_mpi/bin /lib64/openmpi/bin)

if(MPI_BIN)
  message("Using MPI found in ${MPI_BIN}")
  set(MPI_C_COMPILER ${MPI_BIN}/mpicc)
  set(MPI_CXX_COMPILER ${MPI_BIN}/mpicxx)
  find_package(MPI REQUIRED)

  function(BUILD_MPI_TEST testname)
    add_executable(${testname} ${ARGV})
    target_include_directories(${testname} PRIVATE ${MPI_C_INCLUDE_PATH})
    target_link_libraries(${testname} ${MPI_C_LIBRARIES} bbAPI)
    target_compile_definitions(${testname} PRIVATE -DUSE_MPI=1)
    install(TARGETS ${testname} COMPONENT burstbuffer-tests DESTINATION bb/tests/bin)
  endfunction()
else()
  function(BUILD_MPI_TEST testname)
    message("MPI not configured, skipping test ${testname}")
  endfunction()
endif(MPI_BIN)


add_executable(export_layout_test export_layout_test.c)
install(FILES test_basic_xfer.c COMPONENT burstbuffer-tests DESTINATION bb/tests/src)
install(TARGETS export_layout_test COMPONENT burstbuffer-tests DESTINATION bb/tests/bin)


INSTALL_SCRIPT(verify_block.pl)
INSTALL_SCRIPT(stagein.pl)
INSTALL_SCRIPT(stageout1.pl)
INSTALL_SCRIPT(stageout2.pl)
INSTALL_SCRIPT(launch1.pl)
INSTALL_SCRIPT(launch2.pl)
INSTALL_SCRIPT(md5sum_cwd.sh)
INSTALL_SCRIPT(stageout_initiate.pl)
INSTALL_SCRIPT(accesscheck.sh)
BUILD_TEST(test_c_api test_c_api.c)
BUILD_TEST(transfer_list  transfer_list.c)
BUILD_TEST(parmchecktransfer  parmchecktransfer.c)
BUILD_TEST(parmcheckGetVersion  parmcheckGetVersion.c)
BUILD_TEST(addfiles4  addfiles4.c)
BUILD_TEST(test_GetDeviceUsage test_GetDeviceUsage.c)
BUILD_TEST(test_GetUsage test_GetUsage.c)
BUILD_TEST(initLoopSafety initLoopSafety.c)
BUILD_TEST(earlyInit earlyInit.c)
BUILD_TEST(initThreadSafety initThreadSafety.c)
BUILD_TEST(termThreadSafety termThreadSafety.c)
BUILD_MPI_TEST(test_basic_xfer test_basic_xfer.c)
BUILD_MPI_TEST(cancel_transfer  cancel_transfer.c)
BUILD_MPI_TEST(genTransfer  genTransfer.c)
BUILD_MPI_TEST(changeOwner  changeOwner.c)
BUILD_MPI_TEST(changeMode  changeMode.c)
BUILD_MPI_TEST(creatDir  creatDir.c)
BUILD_MPI_TEST(bb_lv  bb_lv.c)
BUILD_MPI_TEST(stLVCreate  stLVCreate.c)
BUILD_MPI_TEST(resizeLV  resizeLV.c)
BUILD_MPI_TEST(bb_init  bb_init.c)
BUILD_MPI_TEST(test_ver  test_ver.c)
BUILD_MPI_TEST(test_addfiles  test_addfiles.c)
BUILD_MPI_TEST(transfer_rate  transfer_rate.c)
BUILD_MPI_TEST(setUsageLimit  setUsageLimit.c)
BUILD_MPI_TEST(getLastErr  getLastErr.c)
BUILD_MPI_TEST(test_ckpt_mpi test_ckpt_mpi.c app_common.c)
BUILD_MPI_TEST(xfer_cancel xfer_cancel.cc)
BUILD_MPI_TEST(getListBBSvrs getListBBSvrs.c)
BUILD_MPI_TEST(setBBSvr setBBSvr.c)
BUILD_MPI_TEST(getTransferInfo getTransferInfo.c)
BUILD_MPI_TEST(get_version1 get_version1.c)
BUILD_MPI_TEST(get_version2 get_version2.c)
BUILD_MPI_TEST(getTransferHandle2 getTransferHandle2.c)
BUILD_MPI_TEST(getTransferHandle3 getTransferHandle3.c)
BUILD_MPI_TEST(getTransferHandle4 getTransferHandle4.c)
BUILD_MPI_TEST(test_handle_perf test_handle_perf.c)
BUILD_MPI_TEST(test_spanner_perf test_spanner_perf.c)
BUILD_MPI_TEST(test_basic_cancel test_basic_cancel.c)

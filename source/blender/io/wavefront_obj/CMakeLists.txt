# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  exporter
  importer
  ../common
  ../../bmesh/intern
  ../../editors/include
  ../../makesrna
)

set(INC_SYS
  ../../../../extern/fast_float
)

set(SRC
  IO_wavefront_obj.cc
  exporter/obj_export_file_writer.cc
  exporter/obj_export_mesh.cc
  exporter/obj_export_mtl.cc
  exporter/obj_export_nurbs.cc
  exporter/obj_exporter.cc
  importer/importer_mesh_utils.cc
  importer/obj_import_file_reader.cc
  importer/obj_import_mesh.cc
  importer/obj_import_mtl.cc
  importer/obj_import_nurbs.cc
  importer/obj_importer.cc

  IO_wavefront_obj.hh
  exporter/obj_export_file_writer.hh
  exporter/obj_export_io.hh
  exporter/obj_export_mesh.hh
  exporter/obj_export_mtl.hh
  exporter/obj_export_nurbs.hh
  exporter/obj_exporter.hh
  importer/importer_mesh_utils.hh
  importer/obj_import_file_reader.hh
  importer/obj_import_mesh.hh
  importer/obj_import_mtl.hh
  importer/obj_import_nurbs.hh
  importer/obj_import_objects.hh
  importer/obj_importer.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_io_common
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::nodes
  PRIVATE bf::windowmanager
)

blender_add_lib(bf_io_wavefront_obj "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/obj_exporter_tests.cc
    tests/obj_importer_tests.cc
    tests/obj_mtl_parser_tests.cc
    tests/obj_nurbs_io_tests.cc
  )

  set(TEST_INC
    ${INC}

    ../../blenloader
    ../../../../tests/gtests
  )

  set(TEST_LIB
    ${LIB}

    bf_blenloader_test_util
    bf_io_wavefront_obj
  )

  blender_add_test_suite_lib(io_wavefront "${TEST_SRC}" "${TEST_INC}" "${INC_SYS}" "${TEST_LIB}")
endif()

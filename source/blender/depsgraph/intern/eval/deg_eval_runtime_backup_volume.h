/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

struct Volume;
struct VolumeGridVector;

namespace blender::deg {

struct Depsgraph;

/* Backup of volume datablocks runtime data. */
class VolumeBackup {
 public:
  VolumeBackup(const Depsgraph *depsgraph);

  void init_from_volume(Volume *volume);
  void restore_to_volume(Volume *volume);

  VolumeGridVector *grids;
  char filepath[/*FILE_MAX*/ 1024];
};

}  // namespace blender::deg

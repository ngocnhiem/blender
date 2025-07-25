/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_node_types.h"

#include "NOD_common.hh"
#include "node_common.h"
#include "node_composite_util.hh"

#include "BKE_node.hh"

#include "RNA_access.hh"

static void register_node_type_cmp_group()
{
  static blender::bke::bNodeType ntype;

  /* NOTE: Cannot use sh_node_type_base for node group, because it would map the node type
   * to the shared NODE_GROUP integer type id. */
  blender::bke::node_type_base_custom(
      ntype, "CompositorNodeGroup", "Group", "GROUP", NODE_CLASS_GROUP);
  ntype.enum_name_legacy = "GROUP";
  ntype.type_legacy = NODE_GROUP;
  ntype.poll = cmp_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.ui_class = node_group_ui_class;
  ntype.ui_description_fn = node_group_ui_description;
  ntype.rna_ext.srna = RNA_struct_find("CompositorNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  blender::bke::node_type_size(
      ntype, GROUP_NODE_DEFAULT_WIDTH, GROUP_NODE_MIN_WIDTH, GROUP_NODE_MAX_WIDTH);
  ntype.labelfunc = node_group_label;
  ntype.declare = blender::nodes::node_group_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_group)

void register_node_type_cmp_custom_group(blender::bke::bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = cmp_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }
  ntype->declare = blender::nodes::node_group_declare;
}

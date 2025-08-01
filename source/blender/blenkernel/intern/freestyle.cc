/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_freestyle_types.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "BKE_freestyle.h"
#include "BKE_lib_id.hh"
#include "BKE_linestyle.h"

/* Function declarations. */
static FreestyleLineSet *alloc_lineset();
static void copy_lineset(FreestyleLineSet *new_lineset, FreestyleLineSet *lineset, const int flag);
static FreestyleModuleConfig *alloc_module();
static void copy_module(FreestyleModuleConfig *new_module, FreestyleModuleConfig *module);

void BKE_freestyle_config_init(FreestyleConfig *config)
{
  config->mode = FREESTYLE_CONTROL_EDITOR_MODE;

  BLI_listbase_clear(&config->modules);
  config->flags = 0;
  config->sphere_radius = 0.1f;
  config->dkr_epsilon = 0.0f;
  config->crease_angle = DEG2RADF(134.43f);

  BLI_listbase_clear(&config->linesets);
}

void BKE_freestyle_config_free(FreestyleConfig *config, const bool do_id_user)
{
  LISTBASE_FOREACH (FreestyleLineSet *, lineset, &config->linesets) {
    if (lineset->group) {
      if (do_id_user) {
        id_us_min(&lineset->group->id);
      }
      lineset->group = nullptr;
    }
    if (lineset->linestyle) {
      if (do_id_user) {
        id_us_min(&lineset->linestyle->id);
      }
      lineset->linestyle = nullptr;
    }
  }
  BLI_freelistN(&config->linesets);
  BLI_freelistN(&config->modules);
}

void BKE_freestyle_config_copy(FreestyleConfig *new_config,
                               const FreestyleConfig *config,
                               const int flag)
{
  FreestyleLineSet *new_lineset;
  FreestyleModuleConfig *new_module;

  new_config->mode = config->mode;
  new_config->flags = config->flags;
  new_config->sphere_radius = config->sphere_radius;
  new_config->dkr_epsilon = config->dkr_epsilon;
  new_config->crease_angle = config->crease_angle;

  BLI_listbase_clear(&new_config->linesets);
  LISTBASE_FOREACH (FreestyleLineSet *, lineset, &config->linesets) {
    new_lineset = alloc_lineset();
    copy_lineset(new_lineset, lineset, flag);
    BLI_addtail(&new_config->linesets, (void *)new_lineset);
  }

  BLI_listbase_clear(&new_config->modules);
  LISTBASE_FOREACH (FreestyleModuleConfig *, module, &config->modules) {
    new_module = alloc_module();
    copy_module(new_module, module);
    BLI_addtail(&new_config->modules, (void *)new_module);
  }
}

static void copy_lineset(FreestyleLineSet *new_lineset, FreestyleLineSet *lineset, const int flag)
{
  new_lineset->linestyle = lineset->linestyle;
  new_lineset->flags = lineset->flags;
  new_lineset->selection = lineset->selection;
  new_lineset->qi = lineset->qi;
  new_lineset->qi_start = lineset->qi_start;
  new_lineset->qi_end = lineset->qi_end;
  new_lineset->edge_types = lineset->edge_types;
  new_lineset->exclude_edge_types = lineset->exclude_edge_types;
  new_lineset->group = lineset->group;
  STRNCPY_UTF8(new_lineset->name, lineset->name);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)new_lineset->linestyle);
    id_us_plus((ID *)new_lineset->group);
  }
}

static FreestyleModuleConfig *alloc_module()
{
  return MEM_callocN<FreestyleModuleConfig>("style module configuration");
}

FreestyleModuleConfig *BKE_freestyle_module_add(FreestyleConfig *config)
{
  FreestyleModuleConfig *module_conf = alloc_module();
  BLI_addtail(&config->modules, (void *)module_conf);
  module_conf->script = nullptr;
  module_conf->is_displayed = 1;
  return module_conf;
}

static void copy_module(FreestyleModuleConfig *new_module, FreestyleModuleConfig *module)
{
  new_module->script = module->script;
  new_module->is_displayed = module->is_displayed;
}

bool BKE_freestyle_module_delete(FreestyleConfig *config, FreestyleModuleConfig *module_conf)
{
  if (BLI_findindex(&config->modules, module_conf) == -1) {
    return false;
  }
  BLI_freelinkN(&config->modules, module_conf);
  return true;
}

bool BKE_freestyle_module_move(FreestyleConfig *config,
                               FreestyleModuleConfig *module_conf,
                               int direction)
{
  return ((BLI_findindex(&config->modules, module_conf) > -1) &&
          (BLI_listbase_link_move(&config->modules, module_conf, direction) == true));
}

void BKE_freestyle_lineset_unique_name(FreestyleConfig *config, FreestyleLineSet *lineset)
{
  BLI_uniquename(&config->linesets,
                 lineset,
                 "FreestyleLineSet",
                 '.',
                 offsetof(FreestyleLineSet, name),
                 sizeof(lineset->name));
}

static FreestyleLineSet *alloc_lineset()
{
  return MEM_callocN<FreestyleLineSet>("Freestyle line set");
}

FreestyleLineSet *BKE_freestyle_lineset_add(Main *bmain, FreestyleConfig *config, const char *name)
{
  int lineset_index = BLI_listbase_count(&config->linesets);

  FreestyleLineSet *lineset = alloc_lineset();
  BLI_addtail(&config->linesets, (void *)lineset);
  BKE_freestyle_lineset_set_active_index(config, lineset_index);

  lineset->linestyle = BKE_linestyle_new(bmain, DATA_("LineStyle"));
  lineset->flags |= FREESTYLE_LINESET_ENABLED;
  lineset->selection = FREESTYLE_SEL_VISIBILITY | FREESTYLE_SEL_EDGE_TYPES |
                       FREESTYLE_SEL_IMAGE_BORDER;
  lineset->qi = FREESTYLE_QI_VISIBLE;
  lineset->qi_start = 0;
  lineset->qi_end = 100;
  lineset->edge_types = FREESTYLE_FE_SILHOUETTE | FREESTYLE_FE_BORDER | FREESTYLE_FE_CREASE;
  lineset->exclude_edge_types = 0;
  lineset->group = nullptr;
  if (name) {
    STRNCPY_UTF8(lineset->name, name);
  }
  else if (lineset_index > 0) {
    SNPRINTF_UTF8(lineset->name, DATA_("LineSet %i"), lineset_index + 1);
  }
  else {
    STRNCPY_UTF8(lineset->name, DATA_("LineSet"));
  }
  BKE_freestyle_lineset_unique_name(config, lineset);

  return lineset;
}

bool BKE_freestyle_lineset_delete(FreestyleConfig *config, FreestyleLineSet *lineset)
{
  if (BLI_findindex(&config->linesets, lineset) == -1) {
    return false;
  }
  if (lineset->group) {
    id_us_min(&lineset->group->id);
  }
  if (lineset->linestyle) {
    id_us_min(&lineset->linestyle->id);
  }
  BLI_remlink(&config->linesets, lineset);
  MEM_freeN(lineset);
  BKE_freestyle_lineset_set_active_index(config, 0);
  return true;
}

FreestyleLineSet *BKE_freestyle_lineset_get_active(FreestyleConfig *config)
{
  LISTBASE_FOREACH (FreestyleLineSet *, lineset, &config->linesets) {
    if (lineset->flags & FREESTYLE_LINESET_CURRENT) {
      return lineset;
    }
  }
  return nullptr;
}

short BKE_freestyle_lineset_get_active_index(FreestyleConfig *config)
{
  FreestyleLineSet *lineset;
  short i;

  for (lineset = (FreestyleLineSet *)config->linesets.first, i = 0; lineset;
       lineset = lineset->next, i++)
  {
    if (lineset->flags & FREESTYLE_LINESET_CURRENT) {
      return i;
    }
  }
  return 0;
}

void BKE_freestyle_lineset_set_active_index(FreestyleConfig *config, short index)
{
  FreestyleLineSet *lineset;
  short i;

  for (lineset = (FreestyleLineSet *)config->linesets.first, i = 0; lineset;
       lineset = lineset->next, i++)
  {
    if (i == index) {
      lineset->flags |= FREESTYLE_LINESET_CURRENT;
    }
    else {
      lineset->flags &= ~FREESTYLE_LINESET_CURRENT;
    }
  }
}

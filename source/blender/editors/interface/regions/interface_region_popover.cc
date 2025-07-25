/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Pop-Over Region
 *
 * \note This is very close to `interface_region_menu_popup.cc`.
 *
 * We could even merge them, however menu logic is already over-loaded.
 * PopOver's have the following differences.
 *
 * - UI is not constrained to a list.
 * - Pressing a button won't close the pop-over.
 * - Different draw style (to show this is has different behavior from a menu).
 * - #PanelType are used instead of #MenuType.
 * - No menu flipping support.
 * - No moving the menu to fit the mouse cursor.
 * - No key accelerators to access menu items
 *   (if we add support they would work differently).
 * - No arrow key navigation.
 * - No menu memory.
 * - No title.
 */

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"

#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_layout.hh"

#include "interface_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Popup Menu with Callback or String
 * \{ */

struct uiPopover {
  uiBlock *block;
  uiLayout *layout;
  uiBut *but;
  ARegion *butregion;

  /* Needed for keymap removal. */
  wmWindow *window;
  wmKeyMap *keymap;
  wmEventHandler_Keymap *keymap_handler;

  uiPopoverCreateFunc popover_func;
  const PanelType *panel_type;

  /* Size in pixels (ui scale applied). */
  int ui_size_x;

#ifdef USE_UI_POPOVER_ONCE
  bool is_once;
#endif
};

/**
 * \param region: Optional, the region the block will be placed in. Must be set if the popover is
 *                supposed to support refreshing.
 */
static void ui_popover_create_block(bContext *C,
                                    ARegion *region,
                                    uiPopover *pup,
                                    blender::wm::OpCallContext opcontext)
{
  BLI_assert(pup->ui_size_x != 0);

  const uiStyle *style = UI_style_get_dpi();

  pup->block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);

  UI_block_flag_enable(pup->block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_POPOVER);
#ifdef USE_UI_POPOVER_ONCE
  if (pup->is_once) {
    UI_block_flag_enable(pup->block, UI_BLOCK_POPOVER_ONCE);
  }
#endif

  pup->layout = &blender::ui::block_layout(pup->block,
                                           blender::ui::LayoutDirection::Vertical,
                                           blender::ui::LayoutType::Panel,
                                           0,
                                           0,
                                           pup->ui_size_x,
                                           0,
                                           0,
                                           style);

  pup->layout->operator_context_set(opcontext);

  if (pup->but) {
    if (pup->but->context) {
      pup->layout->context_copy(pup->but->context);
    }
  }
}

static uiBlock *ui_block_func_POPOVER(bContext *C, uiPopupBlockHandle *handle, void *arg_pup)
{
  uiPopover *pup = static_cast<uiPopover *>(arg_pup);

  /* Create UI block and layout now if it wasn't done between begin/end. */
  if (!pup->layout) {
    ui_popover_create_block(C, handle->region, pup, blender::wm::OpCallContext::InvokeRegionWin);

    if (pup->popover_func) {
      pup->block->handle = handle;
      pup->popover_func(C, pup->layout, const_cast<PanelType *>(pup->panel_type));
      pup->block->handle = nullptr;
    }

    pup->layout = nullptr;
  }

  /* Setup and resolve UI layout for block. */
  uiBlock *block = pup->block;

  /* in some cases we create the block before the region,
   * so we set it delayed here if necessary */
  if (BLI_findindex(&handle->region->runtime->uiblocks, block) == -1) {
    UI_block_region_set(block, handle->region);
  }

  blender::ui::block_layout_resolve(block);
  UI_block_direction_set(block, UI_DIR_DOWN | UI_DIR_CENTER_X);

  const int block_margin = U.widget_unit / 2;

  if (pup->but) {
    /* For a header menu we set the direction automatic. */
    block->minbounds = BLI_rctf_size_x(&pup->but->rect);
    UI_block_bounds_set_normal(block, block_margin);

    /* If menu slides out of other menu, override direction. */
    const bool slideout = ui_block_is_menu(pup->but->block);
    if (slideout) {
      UI_block_direction_set(block, UI_DIR_RIGHT);
    }

    /* Store the button location for positioning the popover arrow hint. */
    if (!handle->refresh) {
      float center[2] = {BLI_rctf_cent_x(&pup->but->rect), BLI_rctf_cent_y(&pup->but->rect)};
      ui_block_to_window_fl(handle->ctx_region, pup->but->block, &center[0], &center[1]);
      /* These variables aren't used for popovers,
       * we could add new variables if there is a conflict. */
      block->bounds_offset[0] = int(center[0]);
      block->bounds_offset[1] = int(center[1]);
      copy_v2_v2_int(handle->prev_bounds_offset, block->bounds_offset);
    }
    else {
      copy_v2_v2_int(block->bounds_offset, handle->prev_bounds_offset);
    }

    if (!slideout) {
      ARegion *region = CTX_wm_region(C);

      if (region && region->panels.first) {
        /* For regions with panels, prefer to open to top so we can
         * see the values of the buttons below changing. */
        UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
      }
      /* Prefer popover from header to be positioned into the editor. */
      else if (region) {
        if (RGN_TYPE_IS_HEADER_ANY(region->regiontype)) {
          if (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_BOTTOM) {
            UI_block_direction_set(block, UI_DIR_UP | UI_DIR_CENTER_X);
          }
        }
      }
    }

    /* Estimated a maximum size so we don't go off-screen for low height
     * areas near the bottom of the window on refreshes. */
    handle->max_size_y = UI_UNIT_Y * 16.0f;
  }
  else if (pup->panel_type &&
           (pup->panel_type->offset_units_xy.x || pup->panel_type->offset_units_xy.y))
  {
    UI_block_flag_enable(block, UI_BLOCK_LOOP);
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
    UI_block_direction_set(block, block->direction);
    block->minbounds = UI_MENU_WIDTH_MIN;

    const int bounds_offset[2] = {
        int(pup->panel_type->offset_units_xy.x * UI_UNIT_X),
        int(pup->panel_type->offset_units_xy.y * UI_UNIT_Y),
    };
    UI_block_bounds_set_popup(block, block_margin, bounds_offset);
  }
  else {
    /* Not attached to a button. */
    int bounds_offset[2] = {0, 0};
    UI_block_flag_enable(block, UI_BLOCK_LOOP);
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
    UI_block_direction_set(block, block->direction);
    block->minbounds = UI_MENU_WIDTH_MIN;

    if (!handle->refresh) {
      uiBut *but = nullptr;
      uiBut *but_first = nullptr;
      for (const std::unique_ptr<uiBut> &but_iter : block->buttons) {
        if ((but_first == nullptr) && ui_but_is_editable(but_iter.get())) {
          but_first = but_iter.get();
        }
        if (but_iter->flag & (UI_SELECT | UI_SELECT_DRAW)) {
          but = but_iter.get();
          break;
        }
      }

      if (but) {
        bounds_offset[0] = -(but->rect.xmin + 0.8f * BLI_rctf_size_x(&but->rect));
        bounds_offset[1] = -BLI_rctf_cent_y(&but->rect);
      }
      else {
        bounds_offset[0] = -(pup->ui_size_x / 2);
        bounds_offset[1] = but_first ? -BLI_rctf_cent_y(&but_first->rect) : (UI_UNIT_Y / 2);
      }
      copy_v2_v2_int(handle->prev_bounds_offset, bounds_offset);
    }
    else {
      copy_v2_v2_int(bounds_offset, handle->prev_bounds_offset);
    }

    UI_block_bounds_set_popup(block, block_margin, bounds_offset);
  }

  return block;
}

static void ui_block_free_func_POPOVER(void *arg_pup)
{
  uiPopover *pup = static_cast<uiPopover *>(arg_pup);
  if (pup->keymap != nullptr) {
    wmWindow *window = pup->window;
    WM_event_remove_keymap_handler(&window->modalhandlers, pup->keymap);
  }
  MEM_delete(pup);
}

uiPopupBlockHandle *ui_popover_panel_create(bContext *C,
                                            ARegion *butregion,
                                            uiBut *but,
                                            uiPopoverCreateFunc popover_func,
                                            const PanelType *panel_type)
{
  wmWindow *window = CTX_wm_window(C);
  const uiStyle *style = UI_style_get_dpi();

  /* Create popover, buttons are created from callback. */
  uiPopover *pup = MEM_new<uiPopover>(__func__);
  pup->but = but;

  /* FIXME: maybe one day we want non panel popovers? */
  {
    const int ui_units_x = (panel_type->ui_units_x == 0) ? UI_POPOVER_WIDTH_UNITS :
                                                           panel_type->ui_units_x;
    /* Scale width by changes to Text Style point size. */
    pup->ui_size_x = ui_units_x * U.widget_unit * (style->widget.points / UI_DEFAULT_TEXT_POINTS);
  }

  pup->popover_func = popover_func;
  pup->panel_type = panel_type;

#ifdef USE_UI_POPOVER_ONCE
  {
    /* Ideally this would be passed in. */
    const wmEvent *event = window->eventstate;
    pup->is_once = (event->type == LEFTMOUSE) && (event->val == KM_PRESS);
  }
#endif

  /* Create popup block. */
  uiPopupBlockHandle *handle = ui_popup_block_create(
      C, butregion, but, nullptr, ui_block_func_POPOVER, pup, ui_block_free_func_POPOVER, true);

  /* Add handlers. If attached to a button, the button will already
   * add a modal handler and pass on events. */
  if (!but) {
    UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
    WM_event_add_mousemove(window);
    handle->popup = true;
  }

  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Standard Popover Panels
 * \{ */

wmOperatorStatus UI_popover_panel_invoke(bContext *C,
                                         const char *idname,
                                         bool keep_open,
                                         ReportList *reports)
{
  uiLayout *layout;
  PanelType *pt = WM_paneltype_find(idname, true);
  if (pt == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "Panel \"%s\" not found", idname);
    return OPERATOR_CANCELLED;
  }

  if (pt->poll && (pt->poll(C, pt) == false)) {
    /* cancel but allow event to pass through, just like operators do */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  uiBlock *block = nullptr;
  if (keep_open) {
    uiPopupBlockHandle *handle = ui_popover_panel_create(
        C, nullptr, nullptr, ui_item_paneltype_func, pt);
    uiPopover *pup = static_cast<uiPopover *>(handle->popup_create_vars.arg);
    block = pup->block;
  }
  else {
    uiPopover *pup = UI_popover_begin(C, U.widget_unit * pt->ui_units_x, false);
    layout = UI_popover_layout(pup);
    UI_paneltype_draw(C, pt, layout);
    UI_popover_end(C, pup, nullptr);
    block = pup->block;
  }

  if (block) {
    uiPopupBlockHandle *handle = block->handle;
    UI_block_active_only_flagged_buttons(C, handle->region, block);
  }
  return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Popup Menu API with begin & end
 * \{ */

uiPopover *UI_popover_begin(bContext *C, int ui_menu_width, bool from_active_button)
{
  uiPopover *pup = MEM_new<uiPopover>(__func__);
  if (ui_menu_width == 0) {
    ui_menu_width = U.widget_unit * UI_POPOVER_WIDTH_UNITS;
  }
  pup->ui_size_x = ui_menu_width;

  ARegion *butregion = nullptr;
  uiBut *but = nullptr;

  if (from_active_button) {
    butregion = CTX_wm_region(C);
    but = UI_region_active_but_get(butregion);
    if (but == nullptr) {
      butregion = nullptr;
    }
  }

  pup->but = but;
  pup->butregion = butregion;

  /* Operator context default same as menus, change if needed. */
  ui_popover_create_block(C, nullptr, pup, blender::wm::OpCallContext::ExecRegionWin);

  /* Create in advance so we can let buttons point to #uiPopupBlockHandle::retvalue
   * (and other return values) already. */
  pup->block->handle = MEM_new<uiPopupBlockHandle>(__func__);

  return pup;
}

static void popover_keymap_fn(wmKeyMap * /*keymap*/, wmKeyMapItem * /*kmi*/, void *user_data)
{
  uiPopover *pup = static_cast<uiPopover *>(user_data);
  pup->block->handle->menuretval = UI_RETURN_OK;
}

void UI_popover_end(bContext *C, uiPopover *pup, wmKeyMap *keymap)
{
  wmWindow *window = CTX_wm_window(C);

  if (keymap) {
    /* Add so we get keymaps shown in the buttons. */
    UI_block_flag_enable(pup->block, UI_BLOCK_SHOW_SHORTCUT_ALWAYS);
    pup->keymap = keymap;
    pup->keymap_handler = WM_event_add_keymap_handler_priority(&window->modalhandlers, keymap, 0);
    WM_event_set_keymap_handler_post_callback(pup->keymap_handler, popover_keymap_fn, pup);
  }

  /* Create popup block. No refresh support since the buttons were created
   * between begin/end and we have no callback to recreate them. */
  uiPopupBlockHandle *handle = ui_popup_block_create(C,
                                                     pup->butregion,
                                                     pup->but,
                                                     nullptr,
                                                     ui_block_func_POPOVER,
                                                     pup,
                                                     ui_block_free_func_POPOVER,
                                                     false);

  /* Add handlers. */
  UI_popup_handlers_add(C, &window->modalhandlers, handle, 0);
  WM_event_add_mousemove(window);
  handle->popup = true;

  /* Re-add so it gets priority. */
  if (keymap) {
    BLI_remlink(&window->modalhandlers, pup->keymap_handler);
    BLI_addhead(&window->modalhandlers, pup->keymap_handler);
  }

  pup->window = window;

  /* TODO(@ideasman42): we may want to make this configurable.
   * The begin/end type of calling popups doesn't allow 'can_refresh' to be set.
   * For now close this style of popovers when accessed. */
  UI_block_flag_disable(pup->block, UI_BLOCK_KEEP_OPEN);
}

uiLayout *UI_popover_layout(uiPopover *pup)
{
  return pup->layout;
}

#ifdef USE_UI_POPOVER_ONCE
void UI_popover_once_clear(uiPopover *pup)
{
  pup->is_once = false;
}
#endif

/** \} */

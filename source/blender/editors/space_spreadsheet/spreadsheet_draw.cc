/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_rect.h"

#include "ED_spreadsheet.hh"

#include "spreadsheet_column.hh"
#include "spreadsheet_draw.hh"
#include "spreadsheet_intern.hh"

#define CELL_RIGHT_PADDING (2.0f * UI_SCALE_FAC)

namespace blender::ed::spreadsheet {

SpreadsheetDrawer::SpreadsheetDrawer()
{
  left_column_width = UI_UNIT_X * 2;
  top_row_height = UI_UNIT_Y * 1.1f;
  row_height = UI_UNIT_Y;
}

SpreadsheetDrawer::~SpreadsheetDrawer() = default;

void SpreadsheetDrawer::draw_top_row_cell(int /*column_index*/,
                                          const CellDrawParams & /*params*/) const
{
}

void SpreadsheetDrawer::draw_left_column_cell(int /*row_index*/,
                                              const CellDrawParams & /*params*/) const
{
}

void SpreadsheetDrawer::draw_content_cell(int /*row_index*/,
                                          int /*column_index*/,
                                          const CellDrawParams & /*params*/) const
{
}

int SpreadsheetDrawer::column_width(int /*column_index*/) const
{
  return 5 * UI_UNIT_X;
}

static void draw_index_column_background(const uint pos,
                                         const ARegion *region,
                                         const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, 11);
  immRectf(pos, 0, region->winy - drawer.top_row_height, drawer.left_column_width, 0);
}

static void draw_alternating_row_overlay(const uint pos,
                                         const int scroll_offset_y,
                                         const ARegion *region,
                                         const SpreadsheetDrawer &drawer)
{
  immUniformThemeColor(TH_ROW_ALTERNATE);
  GPU_blend(GPU_BLEND_ALPHA);
  BLI_assert(drawer.row_height > 0);
  const int row_pair_height = drawer.row_height * 2;
  const int row_top_y = region->winy - drawer.top_row_height - scroll_offset_y % row_pair_height;
  for (const int i : IndexRange(region->winy / row_pair_height + 1)) {
    int x_left = 0;
    int x_right = region->winx;
    int y_top = row_top_y - i * row_pair_height - drawer.row_height;
    int y_bottom = y_top - drawer.row_height;
    y_top = std::min(y_top, region->winy - drawer.top_row_height);
    y_bottom = std::min(y_bottom, region->winy - drawer.top_row_height);
    immRectf(pos, x_left, y_top, x_right, y_bottom);
  }
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_top_row_background(const uint pos,
                                    const ARegion *region,
                                    const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, 11);
  immRectf(pos, 0, region->winy, region->winx, region->winy - drawer.top_row_height);
}

static void draw_separator_lines(const uint pos,
                                 const int scroll_offset_x,
                                 const ARegion *region,
                                 const SpreadsheetDrawer &drawer)
{
  immUniformThemeColorShade(TH_BACK, -11);

  immBeginAtMost(GPU_PRIM_LINES, drawer.tot_columns * 2 + 4);

  /* Left column line. */
  immVertex2f(pos, drawer.left_column_width, region->winy);
  immVertex2f(pos, drawer.left_column_width, 0);

  /* Top row line. */
  immVertex2f(pos, 0, region->winy - drawer.top_row_height);
  immVertex2f(pos, region->winx, region->winy - drawer.top_row_height);

  /* Column separator lines. */
  int line_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    line_x += column_width;
    if (line_x >= drawer.left_column_width) {
      immVertex2f(pos, line_x, region->winy);
      immVertex2f(pos, line_x, 0);
    }
  }
  immEnd();
}

static void get_visible_rows(const SpreadsheetDrawer &drawer,
                             const ARegion *region,
                             const int scroll_offset_y,
                             int *r_first_row,
                             int *r_max_visible_rows)
{
  *r_first_row = -scroll_offset_y / drawer.row_height;
  *r_max_visible_rows = region->winy / drawer.row_height + 1;
}

static void draw_left_column_content(const int scroll_offset_y,
                                     const bContext *C,
                                     ARegion *region,
                                     const SpreadsheetDrawer &drawer)
{
  int old_scissor[4];
  GPU_scissor_get(old_scissor);

  GPU_scissor(0, 0, drawer.left_column_width, region->winy - drawer.top_row_height);

  uiBlock *left_column_block = UI_block_begin(C, region, __func__, ui::EmbossType::None);
  int first_row, max_visible_rows;
  get_visible_rows(drawer, region, scroll_offset_y, &first_row, &max_visible_rows);
  for (const int row_index : IndexRange(first_row, max_visible_rows)) {
    if (row_index >= drawer.tot_rows) {
      break;
    }
    CellDrawParams params;
    params.block = left_column_block;
    params.xmin = 0;
    params.ymin = region->winy - drawer.top_row_height - (row_index + 1) * drawer.row_height -
                  scroll_offset_y;
    params.width = drawer.left_column_width - CELL_RIGHT_PADDING;
    params.height = drawer.row_height;
    drawer.draw_left_column_cell(row_index, params);
  }

  UI_block_end(C, left_column_block);
  UI_block_draw(C, left_column_block);

  GPU_scissor(UNPACK4(old_scissor));
}

static void draw_top_row_content(const bContext *C,
                                 ARegion *region,
                                 const SpreadsheetDrawer &drawer,
                                 const int scroll_offset_x)
{
  int old_scissor[4];
  GPU_scissor_get(old_scissor);

  GPU_scissor(drawer.left_column_width + 1,
              region->winy - drawer.top_row_height,
              region->winx - drawer.left_column_width,
              drawer.top_row_height);

  uiBlock *first_row_block = UI_block_begin(C, region, __func__, ui::EmbossType::None);

  int left_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    const int right_x = left_x + column_width;

    CellDrawParams params;
    params.block = first_row_block;
    params.xmin = left_x;
    params.ymin = region->winy - drawer.top_row_height;
    params.width = column_width - CELL_RIGHT_PADDING;
    params.height = drawer.top_row_height;
    drawer.draw_top_row_cell(column_index, params);

    left_x = right_x;
  }

  UI_block_end(C, first_row_block);
  UI_block_draw(C, first_row_block);

  GPU_scissor(UNPACK4(old_scissor));
}

static void draw_cell_contents(const bContext *C,
                               ARegion *region,
                               const SpreadsheetDrawer &drawer,
                               const int scroll_offset_x,
                               const int scroll_offset_y)
{
  int old_scissor[4];
  GPU_scissor_get(old_scissor);

  GPU_scissor(drawer.left_column_width + 1,
              0,
              region->winx - drawer.left_column_width,
              region->winy - drawer.top_row_height);

  uiBlock *cells_block = UI_block_begin(C, region, __func__, ui::EmbossType::None);

  int first_row, max_visible_rows;
  get_visible_rows(drawer, region, scroll_offset_y, &first_row, &max_visible_rows);

  int left_x = drawer.left_column_width - scroll_offset_x;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    const int column_width = drawer.column_width(column_index);
    const int right_x = left_x + column_width;

    if (right_x >= drawer.left_column_width && left_x <= region->winx) {
      for (const int row_index : IndexRange(first_row, max_visible_rows)) {
        if (row_index >= drawer.tot_rows) {
          break;
        }

        CellDrawParams params;
        params.block = cells_block;
        params.xmin = left_x;
        params.ymin = region->winy - drawer.top_row_height - (row_index + 1) * drawer.row_height -
                      scroll_offset_y;
        params.width = column_width - CELL_RIGHT_PADDING;
        params.height = drawer.row_height;
        drawer.draw_content_cell(row_index, column_index, params);
      }
    }

    left_x = right_x;
  }

  UI_block_end(C, cells_block);
  UI_block_draw(C, cells_block);

  GPU_scissor(UNPACK4(old_scissor));
}

static void update_view2d_tot_rect(const SpreadsheetDrawer &drawer,
                                   ARegion *region,
                                   const int row_amount)
{
  int column_width_sum = 0;
  for (const int column_index : IndexRange(drawer.tot_columns)) {
    column_width_sum += drawer.column_width(column_index);
  }
  /* Adding some padding avoids issues where the right most column overlaps with other region
   * elements like its border or the icon to open the sidebar. */
  const int right_padding = UI_UNIT_X * 0.5f;

  UI_view2d_totRect_set(&region->v2d,
                        column_width_sum + drawer.left_column_width + right_padding,
                        row_amount * drawer.row_height + drawer.top_row_height);
}

static void draw_column_reorder_source(const uint pos,
                                       const ARegion &region,
                                       const SpaceSpreadsheet &sspreadsheet,
                                       const int scroll_offset_x)
{
  const ReorderColumnVisualizationData &data =
      *sspreadsheet.runtime->reorder_column_visualization_data;
  const SpreadsheetTable &table = *get_active_table(sspreadsheet);
  const SpreadsheetColumn &moving_column = *table.columns[data.old_index];

  rctf rect;
  rect.xmin = moving_column.runtime->left_x - scroll_offset_x;
  rect.xmax = moving_column.runtime->right_x - scroll_offset_x;
  rect.ymin = 0;
  rect.ymax = region.winy;

  immUniformThemeColorShadeAlpha(TH_BACK, -20, -128);
  GPU_blend(GPU_BLEND_ALPHA);
  immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_column_reorder_destination(const ARegion &region,
                                            const SpaceSpreadsheet &sspreadsheet,
                                            const SpreadsheetDrawer &drawer,
                                            const int scroll_offset_x)
{
  const ReorderColumnVisualizationData &data =
      *sspreadsheet.runtime->reorder_column_visualization_data;
  const SpreadsheetTable &table = *get_active_table(sspreadsheet);
  const SpreadsheetColumn &moving_column = *table.columns[data.old_index];
  const SpreadsheetColumn &insert_column = *table.columns[data.new_index];

  {
    /* Draw column that is moved. */
    ColorTheme4f color;
    UI_GetThemeColorShade4fv(TH_BACK, -20, color);
    color.a = 0.3f;
    rctf offset_column_rect;
    offset_column_rect.xmin = moving_column.runtime->left_x + data.current_offset_x_px -
                              scroll_offset_x;
    offset_column_rect.xmax = offset_column_rect.xmin +
                              moving_column.width * SPREADSHEET_WIDTH_UNIT;
    offset_column_rect.ymin = 0;
    offset_column_rect.ymax = region.winy;
    UI_draw_roundbox_4fv(&offset_column_rect, true, 0, color);
  }
  {
    /* Draw indicator where the column is inserted. */
    ColorTheme4f color;
    UI_GetThemeColorShade4fv(TH_TEXT, 20, color);
    color.a = 0.6f;
    const int insert_column_x = data.new_index <= data.old_index ? insert_column.runtime->left_x :
                                                                   insert_column.runtime->right_x;
    const int width = UI_UNIT_X * 0.1f;
    rctf insert_rect;
    insert_rect.xmin = insert_column_x - width / 2 - scroll_offset_x;
    insert_rect.xmax = insert_rect.xmin + width;
    insert_rect.ymin = 0;
    insert_rect.ymax = region.winy;

    /* Don't draw on top of index column. */
    const int left_bound = drawer.left_column_width - width / 2;
    insert_rect.xmin = std::max<float>(insert_rect.xmin, left_bound);
    insert_rect.xmax = std::max<float>(insert_rect.xmax, left_bound);

    UI_draw_roundbox_4fv(&insert_rect, true, 0, color);
  }
}

void draw_spreadsheet_in_region(const bContext *C,
                                ARegion *region,
                                const SpreadsheetDrawer &drawer)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(C);

  update_view2d_tot_rect(drawer, region, drawer.tot_rows);

  UI_ThemeClearColor(TH_BACK);

  View2D *v2d = &region->v2d;
  const int scroll_offset_y = v2d->cur.ymax;
  const int scroll_offset_x = v2d->cur.xmin;
  bool is_reordering_columns = sspreadsheet.runtime->reorder_column_visualization_data.has_value();

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  draw_index_column_background(pos, region, drawer);
  draw_alternating_row_overlay(pos, scroll_offset_y, region, drawer);
  draw_top_row_background(pos, region, drawer);
  if (is_reordering_columns) {
    draw_column_reorder_source(pos, *region, sspreadsheet, scroll_offset_x);
  }
  draw_separator_lines(pos, scroll_offset_x, region, drawer);

  immUnbindProgram();

  draw_left_column_content(scroll_offset_y, C, region, drawer);
  draw_top_row_content(C, region, drawer, scroll_offset_x);
  draw_cell_contents(C, region, drawer, scroll_offset_x, scroll_offset_y);

  if (is_reordering_columns) {
    draw_column_reorder_destination(*region, sspreadsheet, drawer, scroll_offset_x);
  }

  rcti scroller_mask;
  BLI_rcti_init(&scroller_mask,
                drawer.left_column_width,
                region->winx,
                0,
                region->winy - drawer.top_row_height);
  UI_view2d_scrollers_draw(v2d, &scroller_mask);
}

}  // namespace blender::ed::spreadsheet

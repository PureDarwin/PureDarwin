/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "radeon_puredarwin.h"
#include "util/format/u_format.h"
#include <radeon_surface.h>

static unsigned cik_get_macro_tile_index(const struct radeon_surf *surf)
{
   unsigned index, tileb;

   tileb = 8 * 8 * surf->bpe;
   tileb = MIN2(surf->u.legacy.tile_split, tileb);

   for (index = 0; tileb > 64; index++)
      tileb >>= 1;

   assert(index < 16);
   return index;
}

#define   G_009910_MICRO_TILE_MODE(x)          (((x) >> 0) & 0x03)
#define   G_009910_MICRO_TILE_MODE_NEW(x)      (((x) >> 22) & 0x07)

static void set_micro_tile_mode(struct radeon_surf *surf,
                                struct radeon_info *info)
{
   uint32_t tile_mode;

   if (info->gfx_level < GFX6) {
      surf->micro_tile_mode = 0;
      return;
   }

   tile_mode = info->si_tile_mode_array[surf->u.legacy.tiling_index[0]];

   if (info->gfx_level >= GFX7)
      surf->micro_tile_mode = G_009910_MICRO_TILE_MODE_NEW(tile_mode);
   else
      surf->micro_tile_mode = G_009910_MICRO_TILE_MODE(tile_mode);
}

static void surf_level_winsys_to_drm(struct radeon_surface_level *level_drm,
                                     const struct legacy_surf_level *level_ws,
                                     unsigned bpe)
{
   level_drm->offset = (uint64_t)level_ws->offset_256B * 256;
   level_drm->slice_size = (uint64_t)level_ws->slice_size_dw * 4;
   level_drm->nblk_x = level_ws->nblk_x;
   level_drm->nblk_y = level_ws->nblk_y;
   level_drm->pitch_bytes = level_ws->nblk_x * bpe;
   level_drm->mode = level_ws->mode;
}

static void surf_level_drm_to_winsys(struct legacy_surf_level *level_ws,
                                     const struct radeon_surface_level *level_drm,
                                     unsigned bpe)
{
   level_ws->offset_256B = level_drm->offset / 256;
   level_ws->slice_size_dw = level_drm->slice_size / 4;
   level_ws->nblk_x = level_drm->nblk_x;
   level_ws->nblk_y = level_drm->nblk_y;
   level_ws->mode = level_drm->mode;
   assert(level_drm->nblk_x * bpe == level_drm->pitch_bytes);
}

static void surf_winsys_to_drm(struct radeon_surface *surf_drm,
                               const struct pipe_resource *tex,
                               unsigned flags, unsigned bpe,
                               enum radeon_surf_mode mode,
                               const struct radeon_surf *surf_ws)
{
   int i;

   memset(surf_drm, 0, sizeof(*surf_drm));

   surf_drm->npix_x = tex->width0;
   surf_drm->npix_y = tex->height0;
   surf_drm->npix_z = tex->depth0;
   surf_drm->blk_w = util_format_get_blockwidth(tex->format);
   surf_drm->blk_h = util_format_get_blockheight(tex->format);
   surf_drm->blk_d = 1;
   surf_drm->array_size = 1;
   surf_drm->last_level = tex->last_level;
   surf_drm->bpe = bpe;
   surf_drm->nsamples = tex->nr_samples ? tex->nr_samples : 1;

   surf_drm->flags = flags;
   surf_drm->flags = RADEON_SURF_CLR(surf_drm->flags, TYPE);
   surf_drm->flags = RADEON_SURF_CLR(surf_drm->flags, MODE);
   surf_drm->flags |= RADEON_SURF_SET(mode, MODE) |
                      RADEON_SURF_HAS_SBUFFER_MIPTREE |
                      RADEON_SURF_HAS_TILE_MODE_INDEX;

   switch (tex->target) {
   case PIPE_TEXTURE_1D:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D, TYPE);
      break;
   case PIPE_TEXTURE_RECT:
   case PIPE_TEXTURE_2D:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D, TYPE);
      break;
   case PIPE_TEXTURE_3D:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_3D, TYPE);
      break;
   case PIPE_TEXTURE_1D_ARRAY:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D_ARRAY, TYPE);
      surf_drm->array_size = tex->array_size;
      break;
   case PIPE_TEXTURE_CUBE_ARRAY: // cube array layout like 2d array
      assert(tex->array_size % 6 == 0);
      FALLTHROUGH;
   case PIPE_TEXTURE_2D_ARRAY:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D_ARRAY, TYPE);
      surf_drm->array_size = tex->array_size;
      break;
   case PIPE_TEXTURE_CUBE:
      surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_CUBEMAP, TYPE);
      break;
   case PIPE_BUFFER:
   default:
      assert(0);
   }

   surf_drm->bo_size = surf_ws->surf_size;
   surf_drm->bo_alignment = 1 << surf_ws->surf_alignment_log2;

   surf_drm->bankw = surf_ws->u.legacy.bankw;
   surf_drm->bankh = surf_ws->u.legacy.bankh;
   surf_drm->mtilea = surf_ws->u.legacy.mtilea;
   surf_drm->tile_split = surf_ws->u.legacy.tile_split;

   for (i = 0; i <= surf_drm->last_level; i++) {
      surf_level_winsys_to_drm(&surf_drm->level[i], &surf_ws->u.legacy.level[i],
                               bpe * surf_drm->nsamples);

      surf_drm->tiling_index[i] = surf_ws->u.legacy.tiling_index[i];
   }

   if (flags & RADEON_SURF_SBUFFER) {
      surf_drm->stencil_tile_split = surf_ws->u.legacy.stencil_tile_split;

      for (i = 0; i <= surf_drm->last_level; i++) {
         surf_level_winsys_to_drm(&surf_drm->stencil_level[i],
                                  &surf_ws->u.legacy.zs.stencil_level[i],
                                  surf_drm->nsamples);
         surf_drm->stencil_tiling_index[i] = surf_ws->u.legacy.zs.stencil_tiling_index[i];
      }
   }
}

static void surf_drm_to_winsys(struct pd_winsys *ws,
                               struct radeon_surf *surf_ws,
                               const struct radeon_surface *surf_drm)
{
   int i;

   memset(surf_ws, 0, sizeof(*surf_ws));

   surf_ws->blk_w = surf_drm->blk_w;
   surf_ws->blk_h = surf_drm->blk_h;
   surf_ws->bpe = surf_drm->bpe;
   surf_ws->is_linear = surf_drm->level[0].mode <= RADEON_SURF_MODE_LINEAR_ALIGNED;
   surf_ws->has_stencil = !!(surf_drm->flags & RADEON_SURF_SBUFFER);
   surf_ws->flags = surf_drm->flags;

   surf_ws->surf_size = surf_drm->bo_size;
   surf_ws->surf_alignment_log2 = util_logbase2(surf_drm->bo_alignment);

   surf_ws->u.legacy.bankw = surf_drm->bankw;
   surf_ws->u.legacy.bankh = surf_drm->bankh;
   surf_ws->u.legacy.mtilea = surf_drm->mtilea;
   surf_ws->u.legacy.tile_split = surf_drm->tile_split;

   surf_ws->u.legacy.macro_tile_index = cik_get_macro_tile_index(surf_ws);

   for (i = 0; i <= surf_drm->last_level; i++) {
      surf_level_drm_to_winsys(&surf_ws->u.legacy.level[i], &surf_drm->level[i],
                               surf_drm->bpe * surf_drm->nsamples);
      surf_ws->u.legacy.tiling_index[i] = surf_drm->tiling_index[i];
   }

   if (surf_ws->flags & RADEON_SURF_SBUFFER) {
      surf_ws->u.legacy.stencil_tile_split = surf_drm->stencil_tile_split;

      for (i = 0; i <= surf_drm->last_level; i++) {
         surf_level_drm_to_winsys(&surf_ws->u.legacy.zs.stencil_level[i],
                                  &surf_drm->stencil_level[i],
                                  surf_drm->nsamples);
         surf_ws->u.legacy.zs.stencil_tiling_index[i] = surf_drm->stencil_tiling_index[i];
      }
   }

   set_micro_tile_mode(surf_ws, &ws->info);
   surf_ws->is_displayable = surf_ws->is_linear ||
                             surf_ws->micro_tile_mode == RADEON_MICRO_MODE_DISPLAY ||
                             surf_ws->micro_tile_mode == RADEON_MICRO_MODE_RENDER;
}


int pd_surface_init(struct radeon_winsys *rws, const struct radeon_info *info,
    const struct pipe_resource *tex, uint64_t flags, unsigned bpe,
    enum radeon_surf_mode mode, struct radeon_surf *surf) {
    struct pd_winsys *ws=(void *)rws;
    struct radeon_surface layout;
    surf_winsys_to_drm(&layout,tex,flags,bpe,mode,surf);
    int result=radeon_surface_best(ws->surf_man,&layout);
    if (!result) result=radeon_surface_init(ws->surf_man,&layout);
    if (!result) surf_drm_to_winsys(ws,surf,&layout);
    return result;
}

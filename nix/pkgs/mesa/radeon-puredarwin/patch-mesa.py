from pathlib import Path
import re
p = Path('src/gallium/meson.build')
s = p.read_text().replace("subdir('winsys/radeon/drm')", "subdir('winsys/radeon/puredarwin')")
p.write_text(s)
p = Path('src/gallium/winsys/radeon/puredarwin/radeon_surface.c')
s = p.read_text().replace('#include <xf86drm.h>', 'extern int pd_surface_query(int, unsigned, uint32_t *);')
a=s.index('static int radeon_get_value('); b=s.index('static int radeon_get_family', a)
s=s[:a]+'''static int radeon_get_value(int conn, unsigned req, uint32_t *value)
{ return pd_surface_query(conn, req, value); }

'''+s[b:]
# RV730's 1D and 2D layouts are handled here without a Linux DRM version query.
s=s.replace('    drmVersionPtr version;\n','')
s=re.sub(r'    version = drmGetVersion\(surf_man->fd\);.*?    drmFreeVersion\(version\);', '    surf_man->hw_info.allow_2d = 1;', s, flags=re.S)
p.write_text(s)
for name in ['inline_sw_helper.h','sw_helper.h']:
    p=Path('src/gallium/auxiliary/target-helpers')/name
    s=p.read_text()
    s='#ifdef GALLIUM_R600\n#include "radeon/puredarwin/radeon_puredarwin.h"\n#endif\n'+s
    needle='   struct pipe_screen *screen = NULL;'
    s=s.replace(needle,needle+'''
#if defined(GALLIUM_R600)
   if (strcmp(driver, "r600") == 0)
      screen = radeon_puredarwin_screen_create(winsys, config);
#endif
''')
    s=s.replace('      "llvmpipe",', '''#if defined(GALLIUM_R600)
      (sw_vk || only_sw) ? "" : "r600",
#endif
      "llvmpipe",''')
    if name == 'inline_sw_helper.h':
        s=s.replace('radeon_puredarwin_screen_create(winsys, config)', 'radeon_puredarwin_screen_create(winsys, NULL)')
    p.write_text(s)
# linux's entry point remains a stub, swrast DRI creates our native device.
p=Path('src/gallium/auxiliary/target-helpers/drm_helper.h')
s=p.read_text().replace('#if defined(GALLIUM_R600)', '#if defined(GALLIUM_R600) && !defined(__APPLE__)')
p.write_text(s)
# r600's screen setup otherwise calls the no-libdrm stub and dereferences NULL.
p=Path('src/gallium/drivers/r600/r600_pipe.c')
s=p.read_text()
a=s.index('\tdrmVersionPtr version = drmGetVersion(')
b=s.index('\n\treturn drm_version;',a)
s=s[:a]+'''\tconst unsigned drm_version = r600_version_simple(rscreen->b.info.drm_major,
\t\t\t\t\t rscreen->b.info.drm_minor,
\t\t\t\t\t rscreen->b.info.drm_patchlevel);
'''+s[b:]
s=s.replace('caps->query_timestamp = rscreen->b.info.clock_crystal_freq != 0;', 'caps->query_timestamp = false;')
p.write_text(s)
p=Path('src/gallium/drivers/r600/r600_pipe_common.c')
s=p.read_text()
s=re.sub(r'snprintf\(rscreen->renderer_string,.*?kernel_version\);', '''snprintf(rscreen->renderer_string, sizeof(rscreen->renderer_string),
                 "%s (PureDarwin IOKit%s)", chip_name, kernel_version);''', s, flags=re.S)
p.write_text(s)

# importing a local scanout BO must retain R700's BO-relative address mode.
p=Path('src/gallium/drivers/r600/r600_texture.c')
s=p.read_text().replace('resource->gpu_address = rscreen->ws->buffer_get_virtual_address(resource->buf);',
    'resource->gpu_address = rscreen->info.r600_has_virtual_memory ? rscreen->ws->buffer_get_virtual_address(resource->buf) : 0;')
p.write_text(s)
p=Path('src/gallium/targets/dri/meson.build')
s=p.read_text()
assert "files('dri_target.c')" in s, "Mesa DRI source list changed"
s=s.replace("files('dri_target.c')", "files('../../winsys/radeon/puredarwin/radeon_present.c', 'dri_target.c')")
p.write_text(s)

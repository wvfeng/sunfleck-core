#ifndef SUNFLECK_WEB_INSPECTOR_H
#define SUNFLECK_WEB_INSPECTOR_H

#include <sunfleck/ui.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind the opened app to the optional Wasm inspector. Clear before destroying it. */
#ifdef SUNFLECK_WEB_DEBUG
void sunfleck_web_debug_bind(ui_app* app);
#else
static inline void sunfleck_web_debug_bind(ui_app* app) { (void)app; }
#endif

#ifdef __cplusplus
}
#endif

#endif

  globalThis.skiaEvaluationBridge = {
    fontVersion: () => module._skia_web_font_version(),
    features: () => {
      const font = module.FS.readFile('/sunfleck/font.ttf');
      const pointer = module._sf_alloc(font.length);
      if(!pointer) throw new Error('Feature font allocation failed');
      try { module.HEAPU8.set(font,pointer); return module._skia_web_features(pointer,font.length); }
      finally { module._sf_free(pointer); }
    },
    init: () => module._skia_web_init(), lost: () => module._skia_web_lost(),
    restore: () => module._skia_web_restore(), close: () => module._skia_web_close(),
    present: (bytes, width, height) => {
      const pointer = module._sf_alloc(bytes.length);
      if (!pointer) throw new Error('Skia command allocation failed');
      try {
        module.HEAPU8.set(bytes, pointer);
        return module._skia_web_present(pointer, bytes.length, width, height);
      } finally { module._sf_free(pointer); }
    },
  };

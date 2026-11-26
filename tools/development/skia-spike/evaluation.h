#pragma once
/* Evaluation-only ABI. This is not part of the Sunfleck SDK. */
#ifdef _WIN32
#ifdef SKIA_EVALUATION_EXPORTS
#define SKIA_EVAL_API __declspec(dllexport)
#else
#define SKIA_EVAL_API __declspec(dllimport)
#endif
#else
#define SKIA_EVAL_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
SKIA_EVAL_API unsigned skia_evaluation_version(void);
SKIA_EVAL_API int skia_evaluation_render(const char* output_directory, unsigned dpi);
SKIA_EVAL_API const char* skia_evaluation_error(void);
#ifdef __cplusplus
}
#endif

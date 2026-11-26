#pragma once
#include "text_format.h"
#include <vector>
#include <span>

struct StyledRun {
    uint32_t start=0, length=0;
    TextFormat format;
    bool operator==(const StyledRun&) const = default;
};
inline thread_local std::span<const StyledRun> current_text_runs;
struct InlineTextScope {
    std::span<const StyledRun> previous;
    explicit InlineTextScope(std::span<const StyledRun> runs):previous(current_text_runs) { current_text_runs=runs; }
    ~InlineTextScope() { current_text_runs=previous; }
};
inline bool is_inline(ui_node_kind kind) { return kind==UI_RUN || kind==UI_SPAN; }
inline bool inline_container(ui_node_kind kind) { return kind==UI_TEXT_BLOCK || kind==UI_SPAN; }

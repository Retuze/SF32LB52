#pragma once
#include "framework/animation/value_animator.hpp"
#include <stdint.h>

namespace litho {

// Edge the page slides from (enter) or toward (exit).
enum class SlideEdge : uint8_t {
    None   = 0,
    Left   = 1,
    Right  = 2,
    Top    = 3,
    Bottom = 4,
};

// Animation applied to one Activity root (enter or exit layer).
struct LayerAnim {
    SlideEdge slide = SlideEdge::None;
    bool      fade  = false;
    // Scale reserved for next iteration (View scale not wired yet).
    bool      scale = false;

    LayerAnim& withFade()  { fade = true; return *this; }
    LayerAnim& withScale() { scale = true; return *this; }
};

struct TransitionSpec {
    LayerAnim    enter;
    LayerAnim    exit;
    uint16_t     durationMs = 450;
    Interpolator ease       = Interpolator::ACCELERATE_DECELERATE;

    TransitionSpec& withFade() {
        enter.fade = true;
        exit.fade  = true;
        return *this;
    }
    TransitionSpec& setDuration(uint16_t ms) {
        durationMs = ms;
        return *this;
    }
    TransitionSpec& setEase(Interpolator i) {
        ease = i;
        return *this;
    }

    bool hasEnter() const {
        return enter.slide != SlideEdge::None || enter.fade || enter.scale;
    }
    bool hasExit() const {
        return exit.slide != SlideEdge::None || exit.fade || exit.scale;
    }
    bool isNone() const { return !hasEnter() && !hasExit(); }

    // ── presets ──────────────────────────────────────────────
    static TransitionSpec none() { return {}; }

    static TransitionSpec slideFromRight() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Right;
        return s;
    }
    static TransitionSpec slideFromLeft() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Left;
        return s;
    }
    static TransitionSpec slideFromTop() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Top;
        return s;
    }
    static TransitionSpec slideFromBottom() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Bottom;
        return s;
    }
    static TransitionSpec fade() {
        TransitionSpec s;
        s.enter.fade = true;
        s.exit.fade  = true;
        return s;
    }
    // Push: new enters from edge, old exits toward the opposite edge
    static TransitionSpec pushFromRight() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Right;
        s.exit.slide  = SlideEdge::Left;
        return s;
    }
    static TransitionSpec pushFromLeft() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Left;
        s.exit.slide  = SlideEdge::Right;
        return s;
    }
    static TransitionSpec pushFromTop() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Top;
        s.exit.slide  = SlideEdge::Bottom;
        return s;
    }
    static TransitionSpec pushFromBottom() {
        TransitionSpec s;
        s.enter.slide = SlideEdge::Bottom;
        s.exit.slide  = SlideEdge::Top;
        return s;
    }
};

} // namespace litho

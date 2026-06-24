#pragma once
#include "framework/intent/bundle.hpp"

namespace litho {

struct Intent {
    const char* target = nullptr;   // target Activity name (explicit)
    Bundle      extras;             // key-value payload

    Intent() = default;
    explicit Intent(const char* t) : target(t) {}

    Intent& putInt(const char* k, int v)     { extras.putInt(k, v);     return *this; }
    Intent& putFloat(const char* k, float v) { extras.putFloat(k, v);   return *this; }
    Intent& putString(const char* k, const char* v) { extras.putString(k, v); return *this; }

    int         getInt(const char* k, int d=0) const     { return extras.getInt(k, d); }
    float       getFloat(const char* k, float d=0) const { return extras.getFloat(k, d); }
    const char* getString(const char* k, const char* d="") const { return extras.getString(k, d); }
};

} // namespace litho

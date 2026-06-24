#pragma once
#include <stdint.h>

namespace litho {

// Key-value container for Intent extras.
// Strings are copied via malloc — caller can pass stack/transient strings safely.
// Max 8 entries total; overflows are silently dropped.

class Bundle {
public:
    static constexpr int kMaxEntries = 8;

    Bundle()  = default;
    ~Bundle() { clear(); }

    // Bundle owns strdup'd strings — no shallow copy.
    Bundle(const Bundle&) = delete;
    Bundle& operator=(const Bundle&) = delete;

    void putInt(const char* key, int value);
    void putFloat(const char* key, float value);
    void putString(const char* key, const char* value);

    int         getInt(const char* key, int def = 0) const;
    float       getFloat(const char* key, float def = 0) const;
    const char* getString(const char* key, const char* def = "") const;

    void clear();

private:
    enum Type { NONE, INT, FLOAT, STRING };

    struct Entry {
        const char* key  = nullptr;   // caller-owned, never freed
        union { int i; float f; const char* s; };
        Type type = NONE;
        bool ownsString = false;      // true → s was malloc'd, free on clear
    };

    Entry    mEntries[kMaxEntries];
    uint16_t mCount = 0;

    const Entry* find(const char* key, Type type) const;
};

} // namespace litho

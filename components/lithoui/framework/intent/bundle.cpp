#include "framework/intent/bundle.hpp"
#include <stdlib.h>
#include <string.h>

namespace litho {

void Bundle::putInt(const char* key, int value) {
    if (mCount >= kMaxEntries) return;
    Entry& e = mEntries[mCount++];
    e.key  = key;
    e.i    = value;
    e.type = INT;
}

void Bundle::putFloat(const char* key, float value) {
    if (mCount >= kMaxEntries) return;
    Entry& e = mEntries[mCount++];
    e.key  = key;
    e.f    = value;
    e.type = FLOAT;
}

void Bundle::putString(const char* key, const char* value) {
    if (mCount >= kMaxEntries) return;
    Entry& e = mEntries[mCount++];
    e.key = key;
    // Copy — caller can safely free or reuse the buffer.
    e.s = strdup(value);
    e.ownsString = true;
    e.type = STRING;
}

void Bundle::clear() {
    for (int i = 0; i < mCount; i++) {
        if (mEntries[i].ownsString && mEntries[i].s) {
            free((void*)mEntries[i].s);
        }
    }
    mCount = 0;
}

const Bundle::Entry* Bundle::find(const char* key, Type type) const {
    for (int i = 0; i < mCount; i++) {
        if (mEntries[i].type == type && strcmp(mEntries[i].key, key) == 0)
            return &mEntries[i];
    }
    return nullptr;
}

int Bundle::getInt(const char* key, int def) const {
    const Entry* e = find(key, INT);
    return e ? e->i : def;
}

float Bundle::getFloat(const char* key, float def) const {
    const Entry* e = find(key, FLOAT);
    return e ? e->f : def;
}

const char* Bundle::getString(const char* key, const char* def) const {
    const Entry* e = find(key, STRING);
    return e ? e->s : def;
}

} // namespace litho

#pragma once
#include <stdint.h>

namespace litho {

class Object {
public:
    virtual ~Object() = default;

    uint32_t id() const { return mId; }
    void     setId(uint32_t id) { mId = id; }

protected:
    uint32_t mId = 0;
};

} // namespace litho

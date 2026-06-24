/**
 * @file cxx_runtime.cpp
 * @brief Minimal C++ runtime for bare-metal (no exceptions, no RTTI).
 */
#include <stddef.h>

extern "C" {
void* malloc(size_t size);
void  free(void* ptr);
}

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C" void __cxa_pure_virtual() {
    while (1) {}
}

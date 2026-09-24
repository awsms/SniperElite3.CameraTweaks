#pragma once
#include <cstdint>

class FovControl {
public:
    void Init();
    bool Apply(float scale, bool enabled);
    bool Available() const { return available; }
    const char* Status() const { return status; }
private:
    std::uintptr_t address = 0;
    float original = 0.0f;
    float lastWritten = 0.0f;
    bool available = false;
    const char* status = "FOV is not initialized.";
};

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace sparkbot {
struct Vector { float x = 0; float y = 0; };
inline Vector RemoteDrive(float x,float y) {
    if(!std::isfinite(x)||!std::isfinite(y))return {x,y}; // Motion rejects invalid input.
    x=std::clamp(x,-1.f,1.f);y=std::clamp(y,-1.f,1.f);
    float radius=std::hypot(x,y);
    if(radius<.045f)return {};
    // Keep both tracks moving in forward/reverse arcs. Blend into a pivot only
    // near the horizontal axis; use exactly the same curve on both sides.
    float drive=std::min(1.f,std::abs(y)/(radius*.35f));
    float throttle=std::copysign(radius*drive,y);
    float turn=x*(1.f-.55f*drive);
    float peak=std::max(std::abs(throttle+turn),std::abs(throttle-turn));
    // Legacy chassis PWM uses 8192*speed/100 (13 bits): avoid its 100% overflow.
    float scale=std::min(radius,.98f)/peak;
    return {turn*scale,throttle*scale};
}

// A command lease is renewed only by a fresh command, never by a status poll.
class Motion {
    Vector value_{};
    int64_t expires_ = 0;
    uint32_t owner_ = 0;
    uint32_t sequence_ = 0;
    bool sequence_seen_ = false;
    bool emergency_ = false;
public:
    bool Move(uint32_t owner, uint32_t sequence, float x, float y, int64_t now,
              int duration_ms = 350) {
        if (emergency_ || !std::isfinite(x) || !std::isfinite(y) || !owner ||
            duration_ms < 1 || duration_ms > 1500) return false;
        if (owner_ != owner && now < expires_) return false;
        if (owner_ == owner && sequence_seen_ && sequence <= sequence_) return false;
        owner_ = owner; sequence_ = sequence; sequence_seen_ = true;
        value_ = {std::clamp(x, -1.0f, 1.0f), std::clamp(y, -1.0f, 1.0f)};
        expires_ = now + duration_ms;
        return true;
    }
    Vector At(int64_t now) const { return emergency_ || now >= expires_ ? Vector{} : value_; }
    void Stop() { value_ = {}; expires_ = 0; }
    void Disconnect(uint32_t owner) { if (owner_ == owner) Stop(); }
    void EmergencyStop() { Stop(); emergency_ = true; }
    void Arm() { Stop(); emergency_ = false; }
    bool IsEmergency() const { return emergency_; }
};

enum class Direction { Left, Right, Up, Down };
class Game2048 {
    std::array<uint32_t, 16> cells_{};
    uint32_t score_ = 0;
    static int Index(Direction d, int row, int col) {
        switch (d) {
            case Direction::Left: return row * 4 + col;
            case Direction::Right: return row * 4 + 3 - col;
            case Direction::Up: return col * 4 + row;
            case Direction::Down: return (3 - col) * 4 + row;
        }
        return 0;
    }
public:
    void SetBoard(std::array<uint32_t, 16> cells) { cells_ = cells; score_ = 0; }
    const std::array<uint32_t, 16>& Cells() const { return cells_; }
    uint32_t Score() const { return score_; }
    bool Slide(Direction d) {
        auto before = cells_;
        for (int row = 0; row < 4; ++row) {
            uint32_t packed[4]{}; int count = 0;
            for (int col = 0; col < 4; ++col) {
                uint32_t value = cells_[Index(d, row, col)];
                if (value) packed[count++] = value;
            }
            uint32_t merged[4]{}; int out = 0;
            for (int i = 0; i < count; ++i) {
                if (i + 1 < count && packed[i] == packed[i+1]) {
                    merged[out] = packed[i] * 2;
                    score_ += merged[out++]; ++i;
                } else merged[out++] = packed[i];
            }
            for (int col = 0; col < 4; ++col) cells_[Index(d, row, col)] = merged[col];
        }
        return cells_ != before;
    }
    bool Spawn(uint32_t random) {
        int empty[16], count = 0;
        for (int i=0; i<16; ++i) if (!cells_[i]) empty[count++] = i;
        if (!count) return false;
        cells_[empty[random % count]] = ((random / 16) % 10 == 0) ? 4 : 2;
        return true;
    }
    bool GameOver() const {
        for (int i=0; i<16; ++i) {
            if (!cells_[i]) return false;
            if (i % 4 != 3 && cells_[i] == cells_[i+1]) return false;
            if (i < 12 && cells_[i] == cells_[i+4]) return false;
        }
        return true;
    }
};
}

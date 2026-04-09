#pragma once

namespace BitmaskHelpers {

// Returns true if any bit in `bitmask` is set in `value`. `bitmask` may
// contain multiple bits — they are tested together with `&`. `bitmask=0`
// is a sentinel meaning "not a bitmask widget" and always returns false.
inline bool getBit(int value, int bitmask) {
    if (bitmask == 0) return false;
    return (value & bitmask) != 0;
}

// Returns a new int with all bits in `bitmask` set or cleared together,
// preserving every other bit in `value`. Idempotent for bits already in
// the requested state. `bitmask=0` is a sentinel meaning "not a bitmask
// widget" and returns `value` unchanged.
inline int setBit(int value, int bitmask, bool on) {
    if (bitmask == 0) return value;
    return on ? (value | bitmask) : (value & ~bitmask);
}

} // namespace BitmaskHelpers

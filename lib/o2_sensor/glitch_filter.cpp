#include "glitch_filter.h"

#include <algorithm>

namespace {
float median_of_3(float a, float b, float c) {
  return std::max(std::min(a, b), std::min(std::max(a, b), c));
}
}  // namespace

void GlitchFilter::push(float value) {
  buf_[next_index_] = value;
  next_index_ = (next_index_ + 1) % 3;
  if (count_ < 3) {
    ++count_;
  }
}

float GlitchFilter::value() const {
  if (count_ < 3) {
    // Dernier echantillon pousse = celui juste avant next_index_.
    int last_index = (next_index_ + 3 - 1) % 3;
    return buf_[last_index];
  }
  return median_of_3(buf_[0], buf_[1], buf_[2]);
}

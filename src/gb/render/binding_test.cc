// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/binding.h"

#include "gb/render/render_types.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

Binding MakeTextureBinding(int index) {
  return Binding()
      .SetShaders(ShaderType::kFragment)
      .SetLocation(BindingSet::kMaterial, index)
      .SetTexture();
}

TEST(BindingTest, IndexRange) {
  EXPECT_TRUE(MakeTextureBinding(0).IsValid());
  EXPECT_TRUE(MakeTextureBinding(kMaxBindingIndex).IsValid());
  EXPECT_FALSE(MakeTextureBinding(-1).IsValid());
  EXPECT_FALSE(MakeTextureBinding(kMaxBindingIndex + 1).IsValid());
}

TEST(BindingTest, Combine) {
  Binding binding = MakeTextureBinding(1);
  Binding other = Binding()
                      .SetShaders(ShaderType::kVertex)
                      .SetLocation(BindingSet::kMaterial, 1)
                      .SetTexture();
  other.volatility = DataVolatility::kPerFrame;
  EXPECT_TRUE(binding.Combine(other));
  EXPECT_EQ(binding.shader_types, kAllShaderTypes);
  EXPECT_EQ(binding.volatility, DataVolatility::kPerFrame);

  EXPECT_FALSE(binding.Combine(MakeTextureBinding(2)));
  EXPECT_FALSE(binding.Combine(Binding(binding).SetTextureArray()));
}

}  // namespace
}  // namespace gb

// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/binding.h"

#include "gb/base/type_info.h"
#include "gb/render/render_test.h"
#include "gb/render/render_types.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class BindingTest : public RenderTest {};

Binding MakeTextureBinding(int index) {
  return Binding()
      .SetShaders(ShaderType::kFragment)
      .SetLocation(BindingSet::kMaterial, index)
      .SetTexture();
}

TEST_F(BindingTest, IndexRange) {
  EXPECT_TRUE(MakeTextureBinding(0).IsValid());
  EXPECT_TRUE(MakeTextureBinding(kMaxBindingIndex).IsValid());
  EXPECT_FALSE(MakeTextureBinding(-1).IsValid());
  EXPECT_FALSE(MakeTextureBinding(kMaxBindingIndex + 1).IsValid());
}

TEST_F(BindingTest, Combine) {
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

TEST_F(BindingTest, ConstantsComparedByTypeKey) {
  const RenderDataType vector2_type(GetAccessToken(), "vector2",
                                    TypeKey::Get<Vector2>(), sizeof(Vector2));
  const RenderDataType other_vector2_type(GetAccessToken(), "other_vector2",
                                          TypeKey::Get<Vector2>(),
                                          sizeof(Vector2));
  const RenderDataType vector3_type(GetAccessToken(), "vector3",
                                    TypeKey::Get<Vector3>(), sizeof(Vector3));
  const Binding binding = Binding()
                              .SetShaders(ShaderType::kVertex)
                              .SetLocation(BindingSet::kMaterial, 0)
                              .SetConstants(&vector2_type);

  const Binding same_key = Binding(binding).SetConstants(&other_vector2_type);
  EXPECT_TRUE(binding.IsCompatible(same_key));
  EXPECT_EQ(binding, same_key);

  const Binding other_key = Binding(binding).SetConstants(&vector3_type);
  EXPECT_FALSE(binding.IsCompatible(other_key));
  EXPECT_NE(binding, other_key);

  const Binding no_type = Binding(binding).SetConstants(nullptr);
  EXPECT_FALSE(binding.IsCompatible(no_type));
  EXPECT_NE(binding, no_type);
  EXPECT_TRUE(no_type.IsCompatible(no_type));
}

}  // namespace
}  // namespace gb

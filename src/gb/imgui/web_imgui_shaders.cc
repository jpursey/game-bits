// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cstdint>
#include <string_view>

#include "absl/types/span.h"
#include "gb/imgui/imgui_shaders.h"

namespace gb::internal {

namespace {

// WGSL converted from assets/shaders/imgui_shader.vert and .frag as
// gb_embed_shader converts a shader in a web build (see
// CMake/GameBitsTargetCommands.cmake), with trailing spaces removed:
//
//   glslc imgui_shader.vert -o vert.spv
//   spirv-opt --split-combined-image-sampler vert.spv -o vert.split.spv
//   naga --validate 0 vert.split.spv vert.raw.wgsl
//   cmake -DINPUT=vert.raw.wgsl -DOUTPUT=vert.bound.wgsl
//     -P CMake/GameBitsWgslBindings.cmake   (one command)
//   naga vert.bound.wgsl vert.wgsl
//
// and the same for imgui_shader.frag. Change both together with the SPIR-V
// in gen_imgui_shaders.cc.
constexpr std::string_view kImGuiVertexShaderWgsl = R"wgsl(struct gl_PerVertex {
    @builtin(position) gl_Position: vec4<f32>,
    gl_PointSize: f32,
    gl_ClipDistance: array<f32, 1>,
    gl_CullDistance: array<f32, 1>,
}

struct ViewConstants {
    kScale: vec2<f32>,
    kOffset: vec2<f32>,
}

struct VertexOutput {
    @builtin(position) gl_Position: vec4<f32>,
    @location(0) member: vec4<f32>,
    @location(1) member_1_: vec2<f32>,
}

var<private> unnamed: gl_PerVertex = gl_PerVertex(vec4<f32>(0f, 0f, 0f, 1f), 1f, array<f32, 1>(), array<f32, 1>());
var<private> inPosition_1_: vec2<f32>;
@group(0) @binding(0)
var<uniform> unnamed_1_: ViewConstants;
var<private> outColor: vec4<f32>;
var<private> inColor_1_: vec4<f32>;
var<private> outUv: vec2<f32>;
var<private> inUv_1_: vec2<f32>;

fn main_1_() {
    let _e12_ = inPosition_1_;
    let _e14_ = unnamed_1_.kScale;
    let _e17_ = unnamed_1_.kOffset;
    let _e18_ = ((_e12_ * _e14_) + _e17_);
    unnamed.gl_Position = vec4<f32>(_e18_.x, _e18_.y, 0f, 1f);
    let _e23_ = inColor_1_;
    outColor = _e23_;
    let _e24_ = inUv_1_;
    outUv = _e24_;
    return;
}

@vertex
fn main(@location(0) inPosition: vec2<f32>, @location(2) inColor: vec4<f32>, @location(1) inUv: vec2<f32>) -> VertexOutput {
    inPosition_1_ = inPosition;
    inColor_1_ = inColor;
    inUv_1_ = inUv;
    main_1_();
    let _e11_ = unnamed.gl_Position.y;
    unnamed.gl_Position.y = -(_e11_);
    let _e13_ = unnamed.gl_Position;
    let _e14_1 = outColor;
    let _e15_ = outUv;
    return VertexOutput(_e13_, _e14_1, _e15_);
}
)wgsl";

constexpr std::string_view kImGuiFragmentShaderWgsl = R"wgsl(struct SamplerInfo {
    m_index: f32,
}

@group(1) @binding(4)
var<uniform> unnamed: SamplerInfo;
var<private> outColor: vec4<f32>;
var<private> inColor_1_: vec4<f32>;
@group(1) @binding(1)
var m_sampler_sampler: sampler;
@group(1) @binding(0)
var m_sampler_image: texture_2d<f32>;
var<private> inUv_1_: vec2<f32>;
@group(1) @binding(3)
var m_sampler_array_sampler: sampler;
@group(1) @binding(2)
var m_sampler_array_image: texture_2d_array<f32>;

fn main_1_() {
    let _e11_ = unnamed.m_index;
    if (_e11_ < 0f) {
        let _e13_ = inColor_1_;
        let _e14_ = inUv_1_;
        let _e15_ = textureSample(m_sampler_image, m_sampler_sampler, _e14_);
        outColor = (_e13_ * _e15_);
    } else {
        let _e17_ = inColor_1_;
        let _e18_ = inUv_1_;
        let _e20_ = unnamed.m_index;
        let _e23_ = vec3<f32>(_e18_.x, _e18_.y, _e20_);
        let _e29_ = textureSample(m_sampler_array_image, m_sampler_array_sampler, vec2<f32>(_e23_.x, _e23_.y), i32(_e23_.z));
        outColor = (_e17_ * _e29_);
    }
    return;
}

@fragment
fn main(@location(0) inColor: vec4<f32>, @location(1) inUv: vec2<f32>) -> @location(0) vec4<f32> {
    inColor_1_ = inColor;
    inUv_1_ = inUv;
    main_1_();
    let _e5_ = outColor;
    return _e5_;
}
)wgsl";

absl::Span<const uint8_t> ToBytes(std::string_view code) {
  return {reinterpret_cast<const uint8_t*>(code.data()), code.size()};
}

}  // namespace

absl::Span<const uint8_t> GetImGuiVertexShaderCode() {
  return ToBytes(kImGuiVertexShaderWgsl);
}

absl::Span<const uint8_t> GetImGuiFragmentShaderCode() {
  return ToBytes(kImGuiFragmentShaderWgsl);
}

}  // namespace gb::internal

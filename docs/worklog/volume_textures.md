# Volume textures and storage buffers

Shaders can read three new kinds of data. A volume texture is a 3D texture,
sampled with filtering, and updated a region at a time. Terra Diem keeps a
small volume of light for each chunk of its world. A storage buffer is an array
of structured data, sized when created and updated in ranges. It holds per-draw
data that is too large or too variable for a constants type, such as the
per-draw data of *Shared mesh buffers and indirect draws*. Every kind of
texture also gets a texel format: one, two, or four 8-bit channels, or four
4-bit channels packed in 16 bits, besides today's RGBA8. Small volumes are
often one or two bytes per texel.

Existing code is unchanged: textures and texture arrays default to today's
format, and nothing else about them changes. `Binding::constants_type` is
renamed `data_type`, which no project reads, and a `RenderBackend` gains pure
virtuals, which no project implements.

## Behavior

### Texture formats

```
// The format of each texel in a texture. kRgba8Srgb is color (the shader reads
// it converted from sRGB to linear); the rest are read as stored, with each
// channel scaled to [0, 1].
enum class TextureFormat : uint8_t {
  kRgba8Srgb,  // Four 8-bit channels, sRGB color. 4 bytes (today's format).
  kRgba8,      // Four 8-bit channels. 4 bytes.
  kRg8,        // Two 8-bit channels. 2 bytes.
  kR8,         // One 8-bit channel. 1 byte.
  kRgba4,      // Four 4-bit channels in a uint16_t: red in the top 4 bits,
               // then green, blue, and alpha. 2 bytes.
};

// Returns the size of one texel in bytes.
int GetTexelSize(TextureFormat format);
```

- `RenderSystem::CreateTexture()` and `CreateTextureArray()` take a
  `TextureFormat` after the `SamplerOptions`, defaulting to `kRgba8Srgb`.
  `Texture::GetFormat()` and `TextureArray::GetFormat()` return it.
- The byte-based `Set()`, `SetRegion()`, and `Get()` take texels in the
  texture's format, and sizes are checked against its texel size.
- `Texture::Get(out_texels, size_in_bytes)` reads a whole texture in any
  format, failing for `kStaticWrite` as `TextureArray::Get()` does. Saving a
  texture reads it this way, as `Edit()` only works for 4-byte formats.
- `Clear()` and `ClearRegion()` take a `Pixel`, converted to the format: its
  first channels for `kR8` and `kRg8`, and the top 4 bits of each for `kRgba4`.
- The `Pixel` and `uint32_t` overloads of `Set()`, `SetRegion()`, and `Get()`,
  and `Texture::Edit()` (whose `ImageView` is of `Pixel`s), work only for the
  two 4-byte formats. For the others they fail, logging an error.
- `RenderSystem::CanFilter(TextureFormat)` says whether the backend can sample
  a format with filtering. Creating a texture with `SamplerOptions::filter`
  set, in a format it can't filter, fails.
- Mipmaps work for every format.
- Saved textures and texture arrays keep their format, and ones saved before
  load as `kRgba8Srgb`.

### Volume textures

```
// A volume texture is a 3D image accessible by the graphics card, in any
// TextureFormat. It is bound to shaders through binding data (as a sampler3D
// in GLSL), and changed depending on its data volatility.
class VolumeTexture : public Resource {
 public:
  DataVolatility GetVolatility() const;
  TextureFormat GetFormat() const;
  int GetWidth() const;
  int GetHeight() const;
  int GetDepth() const;
  const SamplerOptions& GetSamplerOptions() const;

  // Clears the whole volume, or a region of it, with a pixel converted to the
  // format (see TextureFormat).
  bool Clear(Pixel pixel = {0, 0, 0, 0});
  bool ClearRegion(int x, int y, int z, int width, int height, int depth,
                   Pixel pixel = {0, 0, 0, 0});

  // Replaces the whole volume, or a region of it, with texels in the format,
  // ordered x first, then y, then z. A region partly out of bounds is clipped.
  bool Set(const void* texels, int64_t size_in_bytes);
  bool SetRegion(int x, int y, int z, int width, int height, int depth,
                 const void* texels, int64_t size_in_bytes);

  // Reads the whole volume. Fails for kStaticWrite volatility.
  bool Get(void* out_texels, int64_t size_in_bytes) const;
};
```

- `RenderSystem::CreateVolumeTexture(volatility, width, height, depth, options,
  format)`, returning a `ResourcePtr<VolumeTexture>`, or into a
  `ResourceSet`, as for textures. Each side is at most
  `kMaxVolumeTextureSize` (2048), and the total at most
  `kMaxVolumeTextureTexels` (512M, as for texture arrays).
- Volume textures have no mipmaps. Creating one with `SamplerOptions::mipmap`
  set fails, logging an error, so a caller passes
  `SamplerOptions().SetMipmap(false)`. `tile_size` must be zero. Sampler
  options given in a load context apply to every texture loaded with it, so
  loading a volume ignores their `mipmap` and `tile_size`.
- A `kStaticWrite` volume can't apply a partial update while it is in use,
  just as a `kStaticWrite` texture can't.
- `Binding::SetVolumeTexture()`, and `BindingData::SetVolumeTexture()` and
  `GetVolumeTexture()`, as for textures.
- `RenderSystem::SaveVolumeTexture(name, volume, volatility)` saves a
  readable volume (not `kStaticWrite`), with the volatility it is to have when
  loaded, as `SaveTextureArray()` does. It loads through the resource system,
  by name or as a dependency, like any other render resource.

### Storage buffers

```
// A storage buffer is an array of structured values that shaders read, all of
// one type registered with RenderSystem::RegisterConstantsType(). It is bound
// to shaders through binding data (as a readonly buffer block in GLSL), and
// changed depending on its data volatility.
class StorageBuffer : public Resource {
 public:
  DataVolatility GetVolatility() const;
  const RenderDataType* GetType() const;
  int GetCount() const;

  // Sets every value to zero.
  bool Clear();

  // Writes values.size() values, starting at index. Type must be the buffer's
  // type, and the values must fit.
  template <typename Type>
  bool Set(int index, absl::Span<const Type> values);

  // Reads values.size() values, starting at index. Fails for kStaticWrite
  // volatility.
  template <typename Type>
  bool Get(int index, absl::Span<Type> values) const;
};
```

- `RenderSystem::CreateStorageBuffer(type, volatility, count)`, returning a
  `ResourcePtr<StorageBuffer>`, or into a `ResourceSet`. A new buffer is all
  zeros. Its total size is at most `kMaxStorageBufferBytes` (128 MiB, the
  least any Vulkan device supports).
- The type's C++ layout must match GLSL's `std430` layout, which a type used
  for constants (`std140`) usually also does.
- `Binding::SetStorageBuffer(type)`, and `BindingData::SetStorageBuffer()` and
  `GetStorageBuffer()`. Binding a buffer of a different type is a
  `RENDER_ASSERT` failure, as for constants.
- `RenderSystem::SaveStorageBuffer(name, buffer, volatility)` saves a
  readable buffer (not `kStaticWrite`), with the volatility it is to have when
  loaded. It is saved with its type's name and its values as bytes, as
  materials save constants. Loading it fails, logging an error, if no type of
  that name is registered, or the type's size no longer matches the values.

### Saving material types and materials

Material types and materials save volume texture and storage buffer bindings
(a storage buffer's with its type's name, as for constants), and what is bound
to them by resource ID, as for textures. Loading a material loads the volume
textures and storage buffers it uses, so authored content can carry them.

## Design

### Names

- `TextureFormat`, not `PixelFormat`, as `Pixel` is always RGBA8 (`gb_image`).
  `TexelFormat` was the alternative. Its values follow Vulkan's format names
  (`kRgba8Srgb` for `VK_FORMAT_R8G8B8A8_SRGB`).
- `VolumeTexture`, beside `Texture` and `TextureArray`. `Texture3d` was the
  alternative, but reads poorly beside them.
- `StorageBuffer`, the Vulkan and OpenGL term (Direct3D says "structured
  buffer"). It is distinct from `RenderBuffer`, the internal vertex and index
  buffer it is built on, and from constants.
- Regions in a volume are still "regions", as for textures, not "boxes".
- Storage buffer types are registered with `RegisterConstantsType()`. Renaming
  it to cover both would break every project that uses it, for little gain.

### Texture formats (gb_render)

- `TextureFormat` and `GetTexelSize()` live in `render_types.h`.
- `Texture`, `TextureArray`, and `VolumeTexture` keep their format, and do the
  `Pixel` conversion for `Clear()` themselves, so a backend only ever sees
  texels in the format.
- `RenderBackend::CanFilter(RenderInternal, TextureFormat)` is a new pure
  virtual. The test backend can filter every format, with a setting to say it
  can't, for tests.
- `TextureChunk` and `TextureArrayChunk` gain `format : TextureFormat =
  Rgba8Srgb` and `texels : [ubyte]`. They are written with `texels`, and read
  from `pixels` if there are no `texels`, so old files still load.

### Texture formats (gb_render_vulkan)

| Format       | Vulkan format                                     |
| ------------ | ------------------------------------------------- |
| `kRgba8Srgb` | `R8G8B8A8_SRGB`                                   |
| `kRgba8`     | `R8G8B8A8_UNORM`                                  |
| `kRg8`       | `R8G8_UNORM`                                      |
| `kR8`        | `R8_UNORM`                                        |
| `kRgba4`     | `B4G4R4A4_UNORM_PACK16`, with a swizzled view     |

- `R4G4B4A4_UNORM_PACK16` has our bit order, but Vulkan doesn't require
  devices to support it, while it requires `B4G4R4A4_UNORM_PACK16` (sampled
  and filtered). That format's bits are blue, green, red, alpha from the top,
  so the image view swaps red and blue, and the shader reads ours.
- `CanFilter()` asks the device whether the format's Vulkan format has
  `eSampledImageFilterLinear` with optimal tiling.
- Mipmaps resize with `stb_image_resize`: sRGB-aware for `kRgba8Srgb` (as
  before), and linear with the format's channel count for the 8-bit formats.
  `kRgba4` is unpacked to RGBA8, resized, and packed again. The mip level
  count, each level's offset, and their generation are free functions in
  `gb/render/mipmaps.h`, as nothing in them is Vulkan's. `VulkanTexture` and
  `VulkanTextureArray` share them, and so can any other backend.
- `VulkanTexture` and `VulkanTextureArray` do their host buffer arithmetic in
  texels of the format's size, rather than `Pixel`s. Each
  `VulkanRenderState::ImageUpdate` gives the offset of its region's first
  texel, so the backend needs no texel size.
- `GetVulkanTextureFormat()` returns the Vulkan format with the view's
  component mapping, so no image can take one without the other.

### Volume textures (gb_render)

- `VolumeTexture` is a `Resource`, like `Texture`: it clips and validates in
  the base class, and a backend implements `DoClear()`, `DoSet()`, and
  `DoGet()` on regions already clipped. There is no `Edit()`, as `ImageView`
  is 2D.
- `BindingType::kVolumeTexture`, with `fbs::BindingType::VolumeTexture`.
- `TestVolumeTexture` keeps its texels in memory and counts operations, as
  `TestTexture` does.
- `VolumeTextureChunk` (volatility, format, width, height, depth, texels, and
  sampler options), with its own chunk type, read and written as
  `TextureArrayChunk` is. `BindingDataEntry` gains a `volume_texture_id`, so
  materials refer to one by resource ID.
- `RenderBackend::CreateVolumeTexture()` is a new pure virtual.

### Volume textures (gb_render_vulkan)

The three volatility strategies in `VulkanTexture` (`kStaticWrite` uploads
from a temporary host buffer, `kStaticReadWrite` keeps one host buffer,
`kPerFrame` keeps an image per frame in flight) are the same for a volume,
with regions in three dimensions. So they move out of `VulkanTexture` into a
`VulkanTexels` object that works on 3D regions (a 2D texture's regions have a
z of zero and a depth of one), which `VulkanTexture` and `VulkanVolumeTexture`
each own. `VulkanTexels` also generates a 2D texture's mipmaps, as it holds
the host buffers they are generated in, and has the sampler options.

```
// Holds the texels of a texture: its images, and the host buffers its data
// volatility needs. It queues the uploads of the regions written each frame,
// generating mipmaps when the texture has them.
class VulkanTexels {
 public:
  // A 2D texture, with mipmaps if options asks for them.
  static std::unique_ptr<VulkanTexels> CreateTexture(
      VulkanBackend* backend, DataVolatility volatility, TextureFormat format,
      int width, int height, const SamplerOptions& options);

  // A 3D texture, which has no mipmaps.
  static std::unique_ptr<VulkanTexels> CreateVolume(
      VulkanBackend* backend, DataVolatility volatility, TextureFormat format,
      int width, int height, int depth);

  const ImageHandle& GetImageHandle(int frame) const;

  bool Clear(const Region& region, const void* texel);

  // The source's rows are stride texels apart, and its slices slice_height
  // rows apart, as a clipped region keeps the caller's layout.
  bool Set(const Region& region, const void* texels, int stride,
           int slice_height);
  bool Get(void* out_texels) const;
  void* EditBegin();
  void EditEnd(bool modified);

  void OnRender(VulkanRenderState* state);
};
```

- Each strategy implements `PrepareWrite(region)`, which marks the region to
  upload and returns the host texels to write it to, so `Clear()` and `Set()`
  are written once for every strategy.
- An `e3D` view type makes a `VulkanImage` 3D, with
  `VulkanImage::Options::SetDepth()` giving its depth, so a volume one texel
  deep is still 3D. It asserts that a 3D image has one layer, and any other
  image a depth of one.
- `VulkanRenderState::ImageUpdate` gives its region as a `vk::Offset3D` and
  `vk::Extent3D`, and the copy's buffer image height is the image's, so a
  region's slices are a whole slice apart in the host buffer.
- `VulkanBindingData` writes a volume texture's descriptor as a combined image
  sampler, as for textures, rewriting it when its image handle changes.
- `VulkanImage::Create()` asks the device for the limits on images of its
  format, type, tiling, and usage (`getImageFormatProperties()`), and fails if
  the image is larger than they allow, in extent, mip levels, layers, samples,
  or memory. Vulkan only requires devices to support 3D images 256 texels on a
  side, below `kMaxVolumeTextureSize`, and 256 array layers, below
  `kMaxTextureArrayCount`, so this covers texture arrays too.

### Storage buffers (gb_render)

- `StorageBuffer` is a final `Resource` that owns a `RenderBuffer`, which the
  backend creates with `RenderBackend::CreateStorageBuffer(volatility,
  value_size, count)`, as `Mesh` owns its vertex and index buffers. The test
  backend's `TestRenderBuffer` serves for storage buffers.
- `RenderBuffer` gains writes to part of the buffer: `DoSet()` takes an
  offset. *Shared mesh buffers and indirect draws* needs the same for meshes.
- `RenderBuffer::Get()` reads a range of values, which `StorageBuffer::Get()`
  and saving use. Reading through `Edit()` instead could copy a whole host
  buffer that a transfer is still using.
- `BindingType::kStorageBuffer`, with `fbs::BindingType::StorageBuffer`, and
  `Binding::constants_type` holds the storage buffer's type (renamed
  `data_type`, as it no longer describes only constants). This breaks the
  public `Binding` struct, but no project reads the field.
- `StorageBufferChunk` (volatility, the type's name, the count, and the values
  as bytes), with its own chunk type. `BindingDataEntry` gains a
  `storage_buffer_id`.

### Storage buffers (gb_render_vulkan)

- `VulkanBufferType::kStorage`, created with `eStorageBuffer` usage, by the
  same three volatility strategies as other `VulkanRenderBuffer`s.
- Writes to part of a buffer copy only the range written, from the host
  buffer to the device buffer.
- The descriptor is `eStorageBuffer`, over the whole buffer, rewritten when its
  buffer handle changes.
- `VulkanBindingData` keeps the `StorageBuffer`, and binds its `RenderBuffer`
  through `BindingData::GetRenderBuffer()`, a protected static any backend's
  binding data can use. `BindingData` joins `RenderInternal`'s friends for it,
  so `gb_render` names no backend class.

**Brittleness:**
- A storage buffer's type must match the shader's `std430` block. Nothing can
  check a C++ struct against GLSL, so `RegisterConstantsType()`'s comment says
  so, as it does for `std140` today.
- A saved storage buffer holds its values as bytes in its type's C++ layout,
  as saved constants do. Loading checks the type's name and size, but a type
  whose fields change without changing its size loads the old bytes as the
  new fields. Saved data is re-exported when its type changes.
- Volume textures need `SetMipmap(false)`, as `SamplerOptions` defaults to
  mipmaps. Failing loudly is better than silently differing from the options
  given.

### Confirmed on a device

- `kR8` and `kRgba4` volumes draw with filtering between slices. Any format or
  size a device can't support fails to create, rather than failing when drawn.
- `B4G4R4A4_UNORM_PACK16` with a swizzled view reads back as our bit order,
  and `stb_image_resize` resizes 1 and 2 channel 8-bit images.
- `std430` arrays of a struct with a `vec4` have no padding the C++ struct
  lacks.

### Building blocks

Pieces other features can use:
- `TextureFormat` and `GetTexelSize()` (`render_types.h`), for render targets
  in these formats.
- `gb/render/mipmaps.h`: mip level counts, offsets, and generation for any
  format, with nothing Vulkan's in it.
- `VulkanTexels`: the three volatility strategies for any image, on 3D
  regions.
- `RenderBuffer::SetRange()` and `Get()`, writing and reading part of a
  buffer, which mesh pools need.
- `BindingData::GetRenderBuffer()`, for any backend's binding data to bind a
  storage buffer.
- `VulkanImage::Create()` failing for images larger than the device allows.

## Checks

- Unit tests in `gb_render_test` cover formats, volume textures, storage
  buffers, partial buffer writes, and saving each of them, against the test
  backend. `mipmaps_test.cc` covers mipmaps in every format.
- `examples/render-check` draws what only the Vulkan backend can show, and a
  session checks it with `tools\run-program.ps1` screenshots:
  - A row of textures, one per format, each a gradient, mipmapped where drawn
    small. `kR8` is red only, `kRg8` red and green, and `kRgba4` matches
    `kRgba8` but banded.
  - `kR8` and `kRgba4` volumes, a different pattern in each slice, sampled by a
    quad moving through them, which blends smoothly between slices.
  - Two rows of quads, `kStaticReadWrite` and `kPerFrame` storage buffers,
    each quad colored from its own index. The last color changes every second,
    so frames a second apart differ only in that quad.
  - Keys for the texture checks a script can't click: P paints a square each
    frame, and 1, 2, and 3 press the `kStaticWrite` buttons.

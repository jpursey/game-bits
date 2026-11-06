// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MESH_POOL_BUFFERS_H_
#define GB_RENDER_MESH_POOL_BUFFERS_H_

#include <cstdint>

namespace gb {

// This class defines the vertex and index buffers of a MeshPool, for a specific
// graphics API.
//
// This is an internal class called by MeshPool to access the underlying
// graphics API and GPU. The pool decides where each mesh goes, and the buffers
// only hold the data.
//
// Derived classes should assume that all method arguments are already valid. No
// additional checking is required, outside of limits that are specific to the
// derived class implementation or underlying graphics API or GPU.
//
// This class and all derived classes must be thread-safe.
class MeshPoolBuffers {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  MeshPoolBuffers(const MeshPoolBuffers&) = delete;
  MeshPoolBuffers& operator=(const MeshPoolBuffers&) = delete;
  virtual ~MeshPoolBuffers() = default;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Copies a mesh's `vertex_count` vertices into the buffers from
  // `vertex_offset`, and its `index_count` indices from `index_offset`. The
  // data is copied before this returns, so the caller may free it.
  //
  // The mesh is drawn with this data from the next frame that is ended, or, if
  // that frame is already ending as this is called, the one after.
  //
  // The ranges are within the capacities of the last call to Grow() (or of
  // creation), and are either new or were freed with a ticket CanReuse()
  // accepted.
  //
  // Returns false if the write could not be made.
  virtual bool WriteMesh(int vertex_offset, const void* vertices,
                         int vertex_count, int index_offset,
                         const void* indices, int index_count) = 0;

  // Makes the buffers hold at least `vertex_capacity` vertices and
  // `index_capacity` indices, keeping their contents, before the next frame
  // that is ended draws. Neither capacity is less than before, and writes
  // after this returns may use the new capacities.
  //
  // The pool holds a lock that every thread creating a mesh needs while this
  // runs, so it should do as little as it can, leaving the copying to when the
  // frame ends.
  //
  // Returns false if the buffers could not grow, in which case they keep their
  // capacities.
  virtual bool Grow(int vertex_capacity, int index_capacity) = 0;

  // Returns a ticket for space the pool frees now, which frames in flight may
  // still draw, and writes not yet copied may still change. Tickets never
  // decrease.
  virtual uint64_t GetReuseTicket() const = 0;

  // Returns true if space freed with the ticket can be written again: no frame
  // that may draw it is in flight, and no write to it is still to be copied.
  // Once a ticket can be reused, so can every earlier one.
  virtual bool CanReuse(uint64_t ticket) const = 0;

 protected:
  MeshPoolBuffers() = default;
};

}  // namespace gb

#endif  // GB_RENDER_MESH_POOL_BUFFERS_H_

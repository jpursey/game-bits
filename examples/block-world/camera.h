#ifndef CAMERA_H_
#define CAMERA_H_

#include "gb/render/render_types.h"
#include "glm/glm.hpp"

class Camera final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  Camera();
  Camera(const Camera&) = default;
  Camera(Camera&&) = default;
  Camera& operator=(const Camera&) = default;
  Camera& operator=(Camera&&) = default;
  ~Camera() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Position of the camera in world space.
  const glm::vec3& GetPosition() const { return position_; }
  void SetPosition(const glm::vec3& position);

  // Direction the camera is looking toward in world space.
  const glm::vec3& GetDirection() const { return direction_; }
  void SetDirection(const glm::vec3& direction);

  // View distance for clipping.
  float GetViewDistance() const { return view_distance_; }
  void SetViewDistance(float view_distance) { view_distance_ = view_distance; }

  float GetNearDistance() const { return 0.1f; }

  // Vertical field of view in radians.
  float GetFieldOfView() const { return fov_; }
  void SetFieldOfView(float fov) { fov_ = fov; }

  // The strafe direction is horizontal and perpendicular to the direction.
  const glm::vec3& GetStrafe() const { return strafe_; }

  // The up direction is perpendicular to the direction and "up" relative to the
  // view.
  const glm::vec3& GetViewUp() const { return view_up_; }

  // View matrix for this camera.
  const glm::mat4& GetView() const { return view_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Generate Projection matrix for this camera.
  glm::mat4 CreateProjection(const gb::FrameDimensions& size) const;

  // Generate ray through screen position from camera.
  glm::vec3 CreateScreenRay(const gb::FrameDimensions& size, int screen_x,
                            int screen_y) const;

  // Helper to draw direct editing UI
  void DrawGui(const char* title = nullptr);

 private:
  void UpdateView();
  void UpdateStrafe();

  // Properties
  glm::vec3 position_ = glm::vec3(0, 0, 0);
  glm::vec3 direction_ = glm::vec3(1, 0, 0);
  float view_distance_ = 100.0f;
  float fov_ = glm::radians(45.0f);

  // Derived properties
  glm::vec3 strafe_;   // Horizontal strafe direction.
  glm::vec3 view_up_;  // Up direction relative to camera view.
  glm::mat4 view_;
};

#endif  // CAMERA_H_

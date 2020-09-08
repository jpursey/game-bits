#ifndef CAMERA_H_
#define CAMERA_H_

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

  float GetViewDistance() const { return view_distance_; }
  void SetViewDistance(float view_distance) { view_distance_ = view_distance; }

  // The strafe direction is perpendicular to the direction.
  //
  // This is updated when direction is updated.
  const glm::vec3& GetStrafe() const { return strafe_; }

  // View matrix for this camera. This is updated
  //
  // This is updated when position or direction is updated.
  const glm::mat4& GetView() const { return view_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Helper to draw direct editing UI
  void DrawGui(const char* title = nullptr);

 private:
  void UpdateView();
  void UpdateStrafe();

  glm::vec3 position_ = glm::vec3(0, 0, 0);
  glm::vec3 direction_ = glm::vec3(1, 0, 0);
  float view_distance_ = 100.0f;
  glm::vec3 strafe_;
  glm::mat4 view_;
};

#endif  // CAMERA_H_

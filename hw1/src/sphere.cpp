#include "sphere.h"

#include <Eigen/Dense>

#include "cloth.h"
#include "configs.h"

namespace {
void generateVertices(std::vector<GLfloat>& vertices, std::vector<GLuint>& indices) {
  // See http://www.songho.ca/opengl/gl_sphere.html#sphere if you don't know how to create a sphere.
  vertices.reserve(8 * (sphereStack + 1) * (sphereSlice + 1));
  indices.reserve(6 * sphereStack * sphereSlice);

  float x, y, z, xy;  //  position

  float sectorStep = static_cast<float>(EIGEN_PI * 2 / sphereSlice);
  float stackStep = static_cast<float>(EIGEN_PI / sphereStack);
  float sectorAngle, stackAngle;

  for (int i = 0; i <= sphereStack; ++i) {
    stackAngle = static_cast<float>(EIGEN_PI / 2 - i * stackStep);  // [pi/2, -pi/2]
    xy = cosf(stackAngle);                                          // r * cos(u)
    z = sinf(stackAngle);                                           // r * sin(u)

    for (int j = 0; j <= sphereSlice; ++j) {
      sectorAngle = j * sectorStep;  // [0, 2pi]

      x = xy * cosf(sectorAngle);  // r * cos(u) * cos(v)
      y = xy * sinf(sectorAngle);  // r * cos(u) * sin(v)
      vertices.insert(vertices.end(), {x, y, z, x, y, z});
    }
  }

  unsigned int k1, k2;  // EBO index
  for (int i = 0; i < sphereStack; ++i) {
    k1 = i * (sphereSlice + 1);  // beginning of current sphereStack
    k2 = k1 + sphereSlice + 1;   // beginning of next sphereStack
    for (int j = 0; j < sphereSlice; ++j, ++k1, ++k2) {
      if (i != 0) {
        indices.insert(indices.end(), {k1, k2, k1 + 1});
      }
      // k1+1 => k2 => k2+1
      if (i != (sphereStack - 1)) {
        indices.insert(indices.end(), {k1 + 1, k2, k2 + 1});
      }
    }
  }
}
}  // namespace

Spheres& Spheres::initSpheres() {
  static Spheres spheres;
  return spheres;
}

void Spheres::addSphere(const Eigen::Ref<const Eigen::Vector4f>& position, float size) {
  if (sphereCount == _particles.getCapacity()) {
    _particles.resize(sphereCount * 2);
    _radius.resize(sphereCount * 2);
    offsets.allocate(8 * sphereCount * sizeof(float));
    sizes.allocate(2 * sphereCount * sizeof(float));
  }
  _radius[sphereCount] = size;
  _particles.position(sphereCount) = position;
  _particles.velocity(sphereCount).setZero();
  _particles.acceleration(sphereCount).setZero();
  _particles.mass(sphereCount) = sphereDensity * size * size * size;

  sizes.load(0, _radius.size() * sizeof(float), _radius.data());
  ++sphereCount;
}

Spheres::Spheres() : Shape(1, 1), sphereCount(0), _radius(1, 0.0f) {
  offsets.allocate(4 * sizeof(float));
  sizes.allocate(sizeof(float));

  std::vector<GLfloat> vertices;
  std::vector<GLuint> indices;
  generateVertices(vertices, indices);

  vbo.allocate_load(vertices.size() * sizeof(GLfloat), vertices.data());
  ebo.allocate_load(indices.size() * sizeof(GLuint), indices.data());

  vao.bind();
  vbo.bind();
  ebo.bind();

  vao.enable(0);
  vao.setAttributePointer(0, 3, 6, 0);
  glVertexAttribDivisor(0, 0);
  vao.enable(1);
  vao.setAttributePointer(1, 3, 6, 3);
  glVertexAttribDivisor(1, 0);
  offsets.bind();
  vao.enable(2);
  vao.setAttributePointer(2, 3, 4, 0);
  glVertexAttribDivisor(2, 1);
  sizes.bind();
  vao.enable(3);
  vao.setAttributePointer(3, 1, 1, 0);
  glVertexAttribDivisor(3, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spheres::draw() const {
  vao.bind();
  offsets.load(0, 4 * sphereCount * sizeof(GLfloat), _particles.getPositionData());
  GLsizei indexCount = static_cast<GLsizei>(ebo.size() / sizeof(GLuint));
  glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr, sphereCount);
  glBindVertexArray(0);
}

void Spheres::collide(Shape* shape) { shape->collide(this); }
void Spheres::collide(Cloth* cloth) {
  constexpr float coefRestitution = 0.0f;
  // TODO: Collide with particle (Simple approach to handle softbody collision)
  //   1. Detect collision.
  //   2. If collided, update impulse directly to particles' velocity
  //   3. (Bonus) You can add friction, which updates particles' acceleration a = F / m
  // Note:
  //   1. There are `sphereCount` spheres.
  //   2. There are `particlesPerEdge * particlesPerEdge` particles.
  //   3. See TODOs in Cloth::computeSpringForce if you don't know how to access data.
  // Hint:
  //   1. You can simply push particles back to prevent penetration.
  //     Sample code is provided here:
  //       Eigen::Vector4f correction = penetration * normal * 0.15f;
  //       _particles.position(i) += correction;
  //       _particles.position(j) -= correction;

  // Write code here!
  for (int j = 0; j < sphereCount; j++) {
    for (int i = 0; i < particlesPerEdge * particlesPerEdge; i++) {
      if ((cloth->particles().position(i) - _particles.position(j)).norm() <= _radius[j]) {
        Eigen::Vector4f vec = cloth->particles().position(i) - _particles.position(j);
        Eigen::Vector4f v1 = vec.normalized().dot(_particles.velocity(j)) * vec.normalized();
        Eigen::Vector4f v2 = vec.normalized().dot(cloth->particles().velocity(i)) * vec.normalized();
        float m1 = _particles.mass(j), m2 = cloth->particles().mass(i);
        Eigen::Vector4f v1_after = (m1 * v1 + m2 * v2) / (m1 + m2);
        Eigen::Vector4f v2_after = (m1 * v1 + m2 * v2) / (m1 + m2);
        _particles.velocity(j) += -v1 + v1_after;
        cloth->particles().velocity(i) += -v2 + v2_after;

        float normal_force_value = ((v1_after - v1) / deltaTime * _particles.mass(j)).norm();  //移動所造成的摩擦力
        v1 = (_particles.velocity(j) - v1).normalized();
        v2 = (cloth->particles().velocity(i) - v2).normalized();
        Eigen::Vector4f move_friction_1 = (v2 - v1) * normal_force_value * frictionCoef;
        Eigen::Vector4f move_friction_2 = (v1 - v2) * normal_force_value * frictionCoef;
        _particles.velocity(j) += deltaTime * move_friction_1 * _particles.inverseMass(j);
        cloth->particles().velocity(i) += deltaTime * move_friction_2 * cloth->particles().inverseMass(i);
        
        Eigen::Vector4f rotate_direction_1 = _particles.rotation(j).cross3(vec.normalized()).normalized();  //旋轉造成的摩擦力
        Eigen::Vector4f rotate_friction_1 =
            (- rotate_direction_1) * normal_force_value * frictionCoef;
        Eigen::Vector4f rotate_friction_2 = rotate_direction_1 * normal_force_value * frictionCoef;
         _particles.velocity(j) += deltaTime * rotate_direction_1 * _particles.inverseMass(j);
        cloth->particles().velocity(i) += deltaTime * rotate_friction_2 * cloth->particles().inverseMass(i);

        float I1 = (float)2 / 5 * _particles.mass(j) * _radius[j] * _radius[j];
        _particles.rotation(j) += (vec.normalized().cross3(move_friction_1 + rotate_direction_1) / I1) * deltaTime;

        float penetration = _radius[j] - abs((cloth->particles().position(i) - _particles.position(j)).norm());
        auto correction = penetration * vec.normalized() * 0.15;
        cloth->particles().position(i) += correction;
        _particles.position(j) -= correction;
      }
    }
  }
}
void Spheres::collide() {
  constexpr float coefRestitution = 0.8f;
  // TODO: Collide with another sphere (Rigidbody collision)
  //   1. Detect collision.
  //   2. If collided, update impulse directly to particles' velocity
  //   3. (Bonus) You can add friction, which updates particles' acceleration a = F / m
  // Note:
  //   1. There are `sphereCount` spheres.
  //   2. You may not want to calculate one sphere twice (a <-> b and b <-> a)
  //   3. See TODOs in Cloth::computeSpringForce if you don't know how to access data.
  // Hint:
  //   1. You can simply push particles back to prevent penetration.

  // Write code here!
  for (int j = 0; j < sphereCount; j++) {
    for (int i = j+1; i < sphereCount; i++) {
      if ((_particles.position(i) - _particles.position(j)).norm() <= _radius[j]+_radius[i]) {
        Eigen::Vector4f vec = _particles.position(i) - _particles.position(j);
        Eigen::Vector4f v1 = vec.normalized().dot(_particles.velocity(j)) * vec.normalized();
        Eigen::Vector4f v2 = vec.normalized().dot(_particles.velocity(i)) * vec.normalized();
        float m1 = _particles.mass(j), m2 = _particles.mass(i);
        Eigen::Vector4f v1_after = (m1 * v1 + m2 * v2 + m2 * coefRestitution * (v2 - v1)) / (m1 + m2);
        Eigen::Vector4f v2_after = (m1 * v1 + m2 * v2 + m1 * coefRestitution * (v1 - v2)) / (m1 + m2);
        _particles.velocity(j) += -v1 + v1_after;
        _particles.velocity(i) += -v2 + v2_after;

        float normal_force_value = ((v1_after - v1) / deltaTime * _particles.mass(j)).norm();  //移動所造成的摩擦力
        v1 = (_particles.velocity(j) - v1).normalized();
        v2 = (_particles.velocity(i) - v2).normalized();
        Eigen::Vector4f move_friction_1 = (v2 - v1) * normal_force_value * frictionCoef;
        Eigen::Vector4f move_friction_2 = (v1 - v2) * normal_force_value * frictionCoef;
        _particles.velocity(j) += deltaTime * move_friction_1 * _particles.inverseMass(j);
        _particles.velocity(i) += deltaTime * move_friction_2 * _particles.inverseMass(i);

        Eigen::Vector4f rotate_direction_1 = _particles.rotation(j).cross3(vec.normalized()).normalized();  //旋轉造成的摩擦力
        Eigen::Vector4f rotate_direction_2 = _particles.rotation(i).cross3(vec.normalized()).normalized();
        Eigen::Vector4f rotate_friction_1 =
            (rotate_direction_2 - rotate_direction_1) * normal_force_value * frictionCoef;
        Eigen::Vector4f rotate_friction_2 =
            (rotate_direction_1 - rotate_direction_1) * normal_force_value * frictionCoef;
        _particles.velocity(j) += deltaTime * rotate_direction_1 * _particles.inverseMass(j);
        _particles.velocity(i) += deltaTime * rotate_direction_2 * _particles.inverseMass(i);

        float I1 = (float)2 / 5 * _particles.mass(j) * _radius[j] * _radius[j];
        float I2 = (float)2 / 5 * _particles.mass(i) * _radius[i] * _radius[i];
        _particles.rotation(j) += (vec.normalized().cross3(move_friction_1 + rotate_direction_1) / I1) * deltaTime;
        _particles.rotation(i) += (vec.normalized().cross3(move_friction_2 + rotate_direction_2) / I2) * deltaTime;

        float penetration = _radius[j] + _radius[i] - abs((_particles.position(i) - _particles.position(j)).norm());
        auto correction = penetration * vec.normalized() * 0.15;
        _particles.position(i) += correction;
        _particles.position(j) -= correction;
      }
    }
  }
}

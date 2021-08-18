#pragma once
#include <memory>
#include <set>
#include <glm/glm/glm.hpp>

namespace egg
{
	struct DrawCall;
	struct DrawData;
	class EggLight;
	class EggMesh;
	class EggMaterial;
	class Camera;
	enum class DrawFlags;

	/**
	 * TODO:
	 * Builder for draw call objects.
	 * Hold their own state.
	 * One mesh, multiple draw calls.
	 * Material switch inside draw call.
	 * Set to keep track of duplicate materials.
	 * 
	 */
}
#include "api/EggRenderer.h"
#include "Renderer.h"

namespace egg
{
    std::unique_ptr<EggRenderer> EggRenderer::CreateInstance(const RendererSettings& a_Settings)
    {
		//Make an instance of the vulkan renderer.
		auto ptr = std::make_unique<Renderer>();
		return std::move(ptr);
    }
}

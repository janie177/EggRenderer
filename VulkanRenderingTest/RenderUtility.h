#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>

/*
 * Read a file and store the contents in the given output buffer as chars.
 */
class RenderUtility
{
public:
	static bool ReadFile(const std::string& a_File, std::vector<char>& a_Output)
	{
	    std::ifstream fileStream(a_File, std::ios::binary | std::ios::ate); //Ate will start the file pointer at the back, so tellg will return the size of the file.
	    if (fileStream.is_open())
	    {
	        const auto size = fileStream.tellg();
	        fileStream.seekg(0); //Reset to start of the file.
	        a_Output.resize(size);
	        fileStream.read(&a_Output[0], size);
	        fileStream.close();
	        return true;
	    }
	    return false;
	}

	/*
	 * Load a Spir-V shader from file and compile it.
	 */
	static bool CreateShaderModuleFromSpirV(const std::string& a_File, VkShaderModule& a_Output, const VkDevice& a_Device)
	{
	    std::vector<char> byteCode;
	    if (!ReadFile(a_File, byteCode))
	    {
	        printf("Could not load shader %s from Spir-V file.\n", a_File.c_str());
	        return false;
	    }

	    VkShaderModuleCreateInfo shaderCreateInfo{};
	    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	    shaderCreateInfo.codeSize = byteCode.size();
	    shaderCreateInfo.pNext = nullptr;
	    shaderCreateInfo.flags = 0;
	    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());
	    if (vkCreateShaderModule(a_Device, &shaderCreateInfo, nullptr, &a_Output) != VK_SUCCESS)
	    {
	        printf("Could not convert Spir-V to shader module for file %s!\n", a_File.c_str());
	        return false;
	    }

	    return true;
	}
};
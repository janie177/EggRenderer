@echo off
if exist "%cd%/shaders/output" (
echo "Deleting existing Spir-V files..."
@RD /S /Q "%cd%/shaders/output"
)

echo "Compiling glsl to Spir-V in output folder..."
mkdir "%cd%/shaders/output"
for %%i in (shaders/*.vert shaders/*.frag) do (
"%VULKAN_SDK%\Bin\glslangValidator.exe" -V "shaders/%%~i" -o "shaders/output/%%~i.spv"
)

pause
exit 0
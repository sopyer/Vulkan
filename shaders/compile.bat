for /r %%f in (*.vert;*.frag) do %VULKAN_SDK%\Bin32\glslangValidator.exe -V %%f -o %%f.spv
pause
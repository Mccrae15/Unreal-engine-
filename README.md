NVIDIA Unreal Engine 4 Fork
===========================

Welcome to NVIDIA's Unreal Engine 4 GitHub fork.  This fork contains many branches with [GameWorks and VRWorks integrations](https://developer.nvidia.com/nvidia-gameworks-and-ue4):

* [Blast](https://developer.nvidia.com/blast) - [4.17 branch](https://github.com/NvPhysX/UnrealEngine/tree/Blast-4.17), [4.18 branch](https://github.com/NvPhysX/UnrealEngine/tree/Blast-4.18)
* [CataclysmDemo](https://developer.nvidia.com/cataclysm-flip-solver-gpu-particles) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/CataclysmDemo-4.15)
* [FleX](https://developer.nvidia.com/flex) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/FleX-4.17.1)
* [Flow](https://developer.nvidia.com/nvidia-flow) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/NvFlow-4.17)
* [HairWorks](https://developer.nvidia.com/hairworks) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/HairWorks)
* [HBAO+](http://www.geforce.com/hardware/technology/hbao-plus) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/HBAO+)
* [Turbulence](https://developer.nvidia.com/turbulence) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/Turbulence-4.13)
* [TXAA](https://www.geforce.com/hardware/technology/txaa) - [4.16 branch](https://github.com/NvPhysX/UnrealEngine/tree/TXAA3-4.16)
* [Volumetric Lighting](https://developer.nvidia.com/VolumetricLighting) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/VolumetricLighting-4.16)
* [VR Funhouse](https://developer.nvidia.com/vr-funhouse-mod-kit) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/VRFunhouse-4.11)
* [VRWorks](https://developer.nvidia.com/vrworks) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/VRWorks-Graphics-4.15)
* [VXGI](https://developer.nvidia.com/vxgi) - [4.17 branch](https://github.com/NvPhysX/UnrealEngine/tree/VXGI-4.17) [4.18 branch](https://github.com/NvPhysX/UnrealEngine/tree/VXGI-4.18)
* [WaveWorks](https://developer.nvidia.com/waveworks) - [branch](https://github.com/NvPhysX/UnrealEngine/tree/WaveWorks)

If you need to file an issue, please note which branch you are using.

Many of the NVIDIA branches contain binaries (NVAPI, APEX, or PhysX) that are normally fetched by GitDependencies (called by Setup.bat).  When Setup.bat asks to overwrite them, the best answer is "No", otherwise you will need revert the changes to those binaries using Git.

If you are staring at this empty NVIDIA-README branch and are wondering where to find the Unreal Engine 4 source, see a [quick tutorial about git branches](https://www.atlassian.com/git/tutorials/using-branches).  Note, you can view all of NVIDIA's integration branches with the `git branch -r` command.


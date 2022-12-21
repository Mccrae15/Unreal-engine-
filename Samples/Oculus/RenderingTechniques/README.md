# Rendering Techniques

To change which map is launched, adjust the default map in the project settings, or use the Launch button directly within the editor rather than using the Project Launcher.

### Cascaded Shadows

This map demonstrates how you can have dynamic shadows, including on dynamic objects, using Unreal Engine's cascaded shadow map functionality.

The sliders in the Cascaded Shadows map can be dragged using the triggers on the controller. Some settings are not exposed in the in-game GUI and need to be adjusted directly on the light. The light's rotation is controlled by a Blueprint script attached to the light.

Lights must currently be movable for cascaded shadows to work; stationary lights will display correctly in the editor but will not be shadowed on the device. Note also that shadows may disappear with certain camera/light direction combinations if the far distance is set too far on the light.

### Color Grading

This map demonstrates how to use look-up tables (LUTs) to adjust the look of your scene.

Color grading requires that the patch in the "ColorGradingLUT UE4 Patch" folder is applied to the UE4 source code. With the patch applied, color grading LUTs can be applied as described in the [UE4 support document on LUTs](https://docs.unrealengine.com/en-US/Engine/Rendering/PostProcessEffects/UsingLUTs/index.html).

Note that by default the patch computes the LUT only once rather than rebuilding it every frame. If you have changing or animated postprocessing conditions, this can be changed by adjusting:

```
if (!Views[0].GetTonemappingLUT())
{
    // Rather than regenerating the LUT every frame on mobile, we assume that it is constant to save GPU processing.
    FRDGBuilder GraphBuilder(RHICmdList);
    AddCombineLUTPass(GraphBuilder, Views[0]);
    GraphBuilder.Execute();
}
```

to remove the `if` statement; that is:

```
FRDGBuilder GraphBuilder(RHICmdList);
AddCombineLUTPass(GraphBuilder, Views[0]);
GraphBuilder.Execute();
```

### Distance Field Baked Shadows

Signed Distance Field shadows are an Unreal Engine feature for hard-edged, high-quality precomputed shadows for stationary or static lights and objects. 

For precomputed distance field shadows to work, the light type must be set to stationary and "Support Distance Field Shadows" must be enabled within the Project Settings' Mobile Shader Permutation Reduction section.

### Portals

PortalsMap demonstrates two different ways of rendering portals in VR. 

The first is using a static parallax-corrected cubemap captured at the location of the portal being looked through. The PortalSampleCubemapMaterial applied to that portal applies parallax correction using the DistanceLimitedReflections node; this makes it appear as if all of the scene is on the surface of a sphere when viewed through the portal. The benefits of this technique are that it can be prebaked and is therefore fairly inexpensive and can be high resolution; however, it also has an obvious warping effect, and objects lack the correct depth when viewed in stereo.

The second method uses stereo render targets to render the scene from the perspective of what each eye would see through the portal. This achieves a very convincing effect, albeit at a higher performance cost. The implementation is contained within the Portals plugin, and the functionality is split between the PortalCaptureActorBP blueprint, the MF_ScreenAlignedUV_VR material function, and the PortalSampleMaterial material. There are also some helper C++ methods contained in the PortalCaptureActor class.

### Text Rendering

The TextRendering map demonstrates the effects of different texture filtering settings in combination with high-contrast textures like text, and also shows how VR stereo layers can be used to render high-quality text or textures.

Note that when rendering text rendering with transparency the text should not be premultiplied with the alpha channel.
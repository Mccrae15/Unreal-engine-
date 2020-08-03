# Locomotion and Interactions

This sample demonstrates a range of different locomotion and interaction types for VR.

## Controls

- The triggers and the X and A buttons are the action buttons.
- Press the Y or B buttons on the controllers to bring up or dismiss the locomotion method selection UI, then use the action buttons to select a method from the drop down menu.
- Press the side grip to grab an object near the controller.

## Locomotion Methods

- Point and Teleport: Move the thumbstick away from the neutral position on either controller and point where you want to go. An arc and indicator on the ground will show your destination and orientation after teleporting. Rotate the stick to change your target orientation.
When the stick returns to neutral you will be teleported to the destination. 

- Point and Teleport (with third-person avatar): This operates identically to point and teleport, with the addition of a third-person avatar that walks to the destination.

- Stepped Translation and Rotation: Push the left stick forwards or backwards to jump forwards or backwards in increments, and push the right stick left or right to rotate in 35 degree intervals. 

- Grab and Drag: Hold down the action buttons or triggers and move the controllers over the ground to drag yourself in a direction.

- Arm Swinging: Hold down the action buttons or triggers and swing your arms to move forward in the direction the controllers are pointing.

- Dual Stick Walking: The left stick controls translation and the right stick controls rotation as in a standard first-person non-VR game. This type of movement can be uncomfortable in VR, so it is recommended that you only try it for short periods of time or without the headset fully covering your vision until you are accustomed to it.

These locomotion methods are implemented within the Event Graph of MotionControllerPawn.

## Interactions

- Any blue cube in the scene can be picked up and thrown using the grab button on either controller. This interaction is implemented in BP_PickupCube.

- The gun can be picked up by grabbing with one hand. Then, bring, the other hand near the barrel and grab to control the aim. The gun will stretch based on the distance between your hands. This interaction is implemented in Gun/Blueprints/BP_PickupTwoHandedAim.

- The blue pole can be picked up by grabbing with one hand; that hand will control the position and orientation of the pole. Grabbing the pole with a second hand will activate two-handed mode; the first hand controls the position while the pole is aligned along the vector between the two hands. This interaction is implemented in BP_PickupTwoHandedCylinder.

- The bow can be picked up by grabbing with one hand. Grabbing with a second hand will begin drawing an arrow; release the second hand to fire the arrow. This interaction is implemented in BowAndArrow/Blueprints/BP_Bow.

All interactions work in conjunction with the PickupActorInterface, which is coordinated by BP_MotionController and MotionControllerPawn.
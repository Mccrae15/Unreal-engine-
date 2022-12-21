# Android Permissions & Platform Specs Sample
A sample that demonstrates how to request Android permission and fetch OS specs and states.  
![alt text](AndroidPermissions/ReadmeMedia/sample_screenshot.png)

  
## Project Overview
The project has two UMG UIs.  One for handling permission and the other to display OS relates states and specs.  The UMGs derive from custom BP classes created in C++ ("PermissionsUserWidget" and "PlatformSpecsUserWidget").  
  
## Permissions
Handling permissions is done in the "PermissionsUserWidget" class implementation.  It exposes some BP functionalities to request permissions and holds some internal BP viewable variable states.  These states are binded to the UI widgets to display whether a specific permission is enabled and to disable the request buttons if a permission is already enabled.  The buttons simply trigger the C++ functions to request permissions as shown below for the microphone permission:  
![alt text](AndroidPermissions/ReadmeMedia/umg_button_bp.png)  
  
The C++ logic makes use of the "UAndroidPermissionFunctionLibrary" to fetch and request permissions.  It's important that the project settings are properly setup to have the permissions that are needed for the application added to the manifest file.  This is done in the project settings as shown below:
![alt text](AndroidPermissions/ReadmeMedia/editor_permissions_settings.png)  
  
## Platform Specs & States
The "PlatformSpecsUserWidget" class makes use of UE4's "FAndroidMisc" to get OS specific states and specifications.  The widget class just holds state variables that are then binded to the UI's widgets.  Additionally, for every tick, dynamic states are updated (such as volume and battery states).
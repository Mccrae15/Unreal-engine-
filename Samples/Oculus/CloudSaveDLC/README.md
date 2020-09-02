# Cloud Save & DLC Sample
This sample demonstrates how to make use of the Cloud Save (v2) API and how to manage purchasing and downloading DLC content. 
  
## Project Overview
The project has two UMG UIs: one for demonstrating how to manage DLC and the other to demonstrate how to make use of cloud saves.

## Setup
Since the application makes use of platform content, you'll need to setup the Oculus Quest application from the developer portal. **Note that you will need to have passed concept review before you will be able to create a new Quest app in the developer portal!**

1. Create a new Quest application
2. Gather the application ID from the portal as shown below  
![App ID on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/app_id.png)  
3. Replace the app id in the "RiftAppId" & "GearVRAppID" fields in both "CloudSaveDLC\Config\DefaultEngine.ini" and "CloudSaveDLC\Config\Android\AndroidEngine.ini"
5. Set up the cloud save, DLC asset, and in-app-purchase functionality as detailed below. 
4. Once all settings are properly setup, you will need to push a build (to the ALPHA channel for example) before the platform features can be used. Make sure to add yourself as a user in the build channel. 
![Add User screen on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/add_user.png)

### Cloud Save
Enable cloud saves in the portal as shown below:
![Cloud Save checkbox on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/enable_cloud_save.png)

### DLC Assets
Downloadable asset files can only be tested through builds uploaded to the Oculus Store and downloaded from there onto a test device. To upload a build, follow the instructions described in the [Asset Files, DLC, and Expansion Files for Android Apps](https://developer.oculus.com/documentation/unreal/ps-add-ons/) documentation, using the `upload-quest-build` subcommand and passing the folder containing the DLC asset (in this case, a text file named DLCAsset.txt) as the `--assets-dir` parameter. Any users in the build channel can then download the app from the Oculus Store and test the DLC functionality.

### In-App Purchases
In-app purchases must be defined in the Oculus dashboard as described in the [Add Purchases to Your Oculus App](https://developer.oculus.com/documentation/unreal/ps-iap) documentation. For this app, a TSV file containing the SKUs `EXAMPLECON` and `EXAMPLEDUR` (for the consumable and durable purchase, respectively) should be uploaded.
![In-App Purchase submission on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/upload_iap_info.png)

To test the in-app purchases, test users must be used along with the mock credit cards detailed in [the documentation](https://developer.oculus.com/documentation/unreal/ps-iap/). Using a real credit card will result in that card being charged.

![Test users on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/test_user.png)

## Cloud Save
The application maintains a counter for which its value gets loaded/saved from the cloud. Once saved, you should be able to see the file being synced on the portal as shown below:  
![Cloud Save checkbox on the Oculus Dashboard](CloudSaveDLC/ReadmeMedia/cloud_save_list.png)   
Note that the application ensures that `WRITE_EXTERNAL_STORAGE` permission is enabled before enabling the user to load/save. This is a requirement as described in the [cloud storage documentation](https://developer.oculus.com/documentation/unreal/ps-cloud-storage/). The UI makes calls to C++ BP functionalities which can be found in the `UCloudSaveWidget` class implementation.

## DLC Assets
The application can download a text file as a DLC asset and display the downloaded text. The functionality is implemented in the `DLCWidget` class, corresponding to the `DLCWidgetUI` Blueprint type. Note that the `READ_EXTERNAL_STORAGE` and `INTERNET` permissions are required for DLC asset support, and are acquired in `UDLCWidget::AcquirePermissions()`.

## In-App Purchases
The application can display information about two in-app purchase SKUs: `EXAMPLECON` and `EXAMPLEDUR`. It checks on startup in `UDLCWidget::FetchPurchasedProducts()` for existing purchases, and then allows the user to launch the purchase flow for unpurchased products. `EXAMPLECON` is always purchasable since it is consumed immediately after purchase.

The implementation is split between `UDLCWidget::FetchPurchasedProducts()` and `UDLCWidget::FetchProductDetails(ovrPurchaseArrayHandle)` for retrieving details about the available in-app purchases and populating the list view, `UOVRProduct` for storing information about each individual product and handling product purchases, and `UDLCListEntryWidget` for allowing the Blueprint list element to query information about the `UOVRProduct`.
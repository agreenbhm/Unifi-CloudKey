# Unifi Network Server 8.x for Original Cloud Key (UCKv1)
TLDR - the instructions are at the bottom of the page.

### WARNING - this only supports the wireless controller (UNS), not any other apps that the Cloud Key might support (like the NVR app).  It's possible it *might* work alongside other apps, but it has not been tested.

### What is this?
This repository contains the necessary components to self-host the latest version of Unifi Network Server on the old Unifi Cloud Key (which is no longer supported).

### Background
Ubiquiti abandoned the original Cloud Key (UCK) and does not provide updates past v7.2 for the Unifi Network Server (the wireless controller).  Newer access points require a newer version of UNS to be adopted into the system (I found this out with a U7 Pro), requiring you to either self-host the newer server on other hardware, or to upgrade to one of their newer Cloud Keys.

### Problems
They claim that newer versions of the UNS software require 64-bit hardware, which the UCK does not have (it's ARMv7, ie 32-bit).  I decided to find out why.  It turns out there are 3 things preventing it from installing on 32-bit hardware: 1) they no longer include a 32-bit ARM native library within the package, 2) they hardcoded the startup function to only run on 64-bit hardware, and 3) they hardcoded something into the UNS .deb package to explicitly not run on the UCK.

### Solutions

1. The 64-bit native libraries included in newer versions of UNS match the hashes of the libraries from the older version that's on the UCK, meaning nothing there has changed.  That being the case, since the 32-bit libraries are available on the UCK, you can copy the 32-bit files into the newer version of UNS and they work just fine.

2. There is a single "if" statement during program launch that checks the CPU architecture and returns false if it's 32-bit.  That was easily patched by editing the Java bytecode to always return true.

3. By unpacking the .deb, removing the model check, and repacking, this control can easily be bypassed.

### Deployment Approach
The goal of this project was to allow running the latest UNS on the UCK with minimal modification to the UCK.  I found two ways to get things running:

1. Upgrade the device from Debian 8 to Debian 11, which can be accomplished by setting the apt repos to the next distro version and then upgrading. This is tedious, as you need to go from Debian 8 -> 9 -> 10 -> 11.  This takes several hours, though it did work and was fully functional best I could tell.  Upgrading to Debian 12 rendered the device unbootable though.

2. Utilize Docker to containerize the application and database, allowing minimal modification to the host system (which hopefully improves long-term stability) while allowing the app to be run in a newer version of Debian with all dependencies, without conflicting with the host.  This is the approach I decided was best, though if you want to do without Docker #1 seems to work just fine.

The solution implemented updates the apt repos on the UCK to ones that work in 2024, installs Docker, downloads UNS and MongoDB, builds the container image, and then launches the database and UNS in two separate containers (sharing the same image).  In addition, a few modifications are made to the host system such as disabling the original UNS and MongoDB services, making a file every boot to tell the webui that UNS is installed, and changing commands that are run to start/stop/determine version of UNS from the UCK webui (since now it is containerized).  Everything is backed up to a file/folder with the extension ".old" before making any modifications, so reversing changes, while a manual process, is relatively simple.  If at any point you want to easily revert the entire device you can factory-reset it without issue (but make sure you've backed up UNS first).

### Instructions

0. BACK UP UNIFI NETWORK SERVER FIRST.  I cannot stress this enough.  While this script *should* work without issue and leave old data on-disk, the data is NOT migrated to the new instance.  Also, if this script fails horribly it could leave your device in a state requiring a factory-reset, so just back up your UNS data via the webui first.  You'll need it later to restore anyway.  If you skip this step and lose all your data you have no one to blame but yourself!

1. SSH into your UCK using the username and password you log into the device's webui with.  Note: this is NOT necessarily the same you use to log into the UNS webui; this is the one you log into the device maintenance page with.

2. Download a .tar.gz version of this repository.  The stock UCK does not have "git" or "unzip" on there, so you need to download the .tar.gz via wget and unpack.  Then switch to that directory.
    * `wget https://github.com/agreenbhm/Unifi-CloudKey/archive/refs/heads/main.tar.gz`
    * `tar xzvf Unifi-CloudKey-main.tar.gz`
    * `cd Unifi-CloudKey-main`

3. Ensure the main script is executable via:
    * `chmod +x ./provision_cloudkey.sh`

4. Run the script as root.  It will take a while (perhaps between 30-60 minutes), as the container needs to build from scratch.  Errors are not necessarily fatal, but keep an eye on the output and if you run into problems make sure you post that to the issue you raise.
    * `sudo ./provision_cloudkey.sh`

5. When finished, you will need to reboot.  It'll tell you to do so. Give it 5 minutes though before rebooting to let the app finish the initial launch, just to be safe.
    * `sudo reboot`

6. After the device reboots, it may take some time for the app to launch.  Give it roughly 5 or so minutes to fully boot.

7. Navigate to the device via your web browser.  Port 80 should redirect to port 443 (the standard SSL device management page).  If all is well, you should see the option for logging into the UNS or device maintenance apps.  Choose UNS.

8. Restore your backup to UNS (or setup the controller from scratch).

9. Done!

### Upgrading
Run the "upgrade_cloudkey.sh" script.  It does not currently check for the installed version vs available version, so only run if you know there's an upgrade of UNS available.

1. `./upgrade_cloudkey.sh`

Or, to do so manually:

1. Navigate to the folder this repository is located and pull the latest version via git.
    - `cd <repo_folder>; git pull`

2. Update the local image, either by rebuilding it locally or pulling it down:
    - Rebuild the container image
      - `docker build -t agreenbhm/unifi-cloudkey:latest --network host .`
    - or, pull the latest version from Docker Hub
      - `docker pull --platform linux/armhf agreenbhm/unifi-cloudkey:latest`

3. Re-deploy with docker-compose
    - `docker-compose up -d`

### Testing
There is also a "provision_vm.sh" script in the repo which is only meant for test purposes on non-UCK hardware.  If you want to play with it or help with development, set up an aarch64/ARM64 VM running Debian 12, download the repo, and then run "provision_vm.sh".  This will provision the VM to look like the UCK (the device management webui is installed), and then build/install arm/armhf/ARM32 containers within Docker.  When all is done you will be able to navigate to the webui for the UCK on your VM and test as if you had a native UCK.

NOTE: you MUST run this on a platform that supports arm/armhf/ARM32 (which Apple Silicon Macs do not).  So either on a VM on a native aarch64 machine, or an emulated aarch64 VM on MacOS.  I did the latter using QEMU/UTM, with the "virtualize" option OFF (since native virtualization does not support armhf on Apple Silicon).

### Issues
If you encounter bugs post an issue.  Make sure to include necessary log output and a comprehensive description, including the environment and steps taken.

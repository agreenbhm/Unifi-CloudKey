# Unifi Network Server 8.x for Original Cloud Key (UCKv1)

### What is this?
This repository contains the necessary components to self-host the latest version of Unifi Network Server on the old Unifi Cloud Key (which is no longer supported).

### Background
Ubiquiti abandoned the original Cloud Key (UCK) and does not provide updates past v7.2 for the Unifi Network Server (the wireless controller).  Newer access points require a newer version of UNS to be adopted into the system (I found this out with a U7 Pro), requiring you to either self-host the newer server on other hardware, or to upgrade to one of their newer Cloud Keys.

### Problems
They claim that newer versions of the UNS software require 64-bit hardware, which the UCK does not have (it's ARMv7, ie 32-bit).  I decided to find out why.  It turns out there are 3 things preventing it from installing on 32-bit hardware: 1) they no longer include a 32-bit ARM native library within the package, 2) they hardcoded the startup function to only run on 64-bit hardware, and 3) they hardcoded something into the UNS .deb package to explicitly not run on the UCK.

### Solutions

1. The 64-bit native libraries included in newer versions of UNS match the hashes of the libraries from the older version that's on the UCK, meaning nothing there has changed.  That being the case, since the 32-bit libraries are available on the UCK, you can copy the 32-bit files into the newer version of UNS and they work just fine.

2. There is a single "if" statement that checks the CPU architecture and returns false if it's 32-bit.  That was easily patched by editing the Java bytecode to always return true.

3. By unpacking the .deb, removing the model check, and repacking, this control can easily be bypassed.
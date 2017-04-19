Nvidia Tegra TK-1 (Kernel 3.10.40-gc017b03)

For your convenience, we made the script that automated the steps described below.
Just run RunMe.sh. After reboot run the script one more time to build, install and load the DUO driver.

Pre-requisites:
---------------

1) Install the following pre-requisites:
	>sudo apt-add-repository universe
	>sudo apt-get update
	# GCC compiler & debugger
	>sudo apt-get install build-essential gdb libncurses5-dev
	# CMake, Qt5 & QtCreator, OpenCV
	>sudo apt-get install cmake qt5-default qtcreator libopencv-dev

2) You must build and load the DUO kernel module before using the DUO camera.

   DUO LKM Build Process:
   ----------------------
   Because DUO uses 1024 byte bulk packet size, we need to patch the kernel to allow for this.

   1. Download and configure kernel source:
	>cd ~/Documents
	>mkdir Kernel
	>cd Kernel
	# Download the kernel source (latest release is R21.4)
	>wget http://developer.download.nvidia.com/embedded/L4T/r21_Release_v4.0/source/kernel_src.tbz2
	# Extract the kernel source
	>tar -xvf kernel_src.tbz2
	>cd kernel
	# Copy the current config
	>zcat /proc/config.gz > .config
	# Make sure the config is ok (this shouldn't ask questions, if the kernel versions match)
	>make menuconfig
	# Go to: "General setup -> Local version" and set your version name eg.: -duo3d
	# Save and exit

   2. Edit the file drivers/usb/host/xhci-mem.c
	>gedit drivers/usb/host/xhci-mem.c

   3. Comment out the following lines of code (lines 1439 & 1440):
        1439:	if (usb_endpoint_xfer_bulk(&ep->desc))
	1440:		max_packet = 512;

   4. Save the file

   5. Compile the kernel (around 5 minutes):
	# Compile the kernel and the kernel modules
	>make -j4
	>make modules
	# Install the modules (/lib/modules/<your-kernel-version>/)
	>sudo make modules_install
	# Copy newly built kernel zImage into /boot directory
	# (You may want to backup or rename your previous kernel zImage file before this)
	>sudo cp arch/arm/boot/zImage /boot/zImage

   6. Reboot and verify the version of the kernel
	>uname -r

   7. Build DUO Linux Kernel Module
	>cd ~/Documents/DUO3D-ARM-v1.0.60.10/DUODriver
	# The make file will use your newly built kernel headers (~/Documents/Kernel/kernel)
	>make

   8. Install DUO Kernel Module
	>sudo ./InstallDriver

   9. Load DUO Kernel Module
	>sudo ./LoadDriver

   10. Unload DUO Kernel Module
	>sudo ./UnloadDriver

3) Plug in DUO device and verify that the node 'duo0' appears in /dev directory.

4) Use DUO with the DUODashboard.


Development with DUO SDK:
-------------------------

Assuming you have all pre-requisites installed (in step 1), you can now view, 
modify and build DUO code samples in ~/Documents/CL-DUO3D-ARM-1.0.60.10/DUOSDK/Samples directory.
For more information, please take a look at README.txt file in each directory.


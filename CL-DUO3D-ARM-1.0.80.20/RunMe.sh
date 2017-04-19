#bin/bash
if [ ! -f step1 ]; 
then
  # Step 1
  sudo apt-add-repository universe
  sudo apt-get update
  # GCC compiler & debugger
  sudo apt-get install build-essential gdb libncurses5-dev
  # CMake, Qt5 & QtCreator, OpenCV
  sudo apt-get install cmake nano qt5-default qtcreator
  # Build kernel
  cd ~/Documents
  mkdir Kernel
  cd Kernel
  # Download the kernel source (latest release is R21.4)
  wget http://developer.download.nvidia.com/embedded/L4T/r21_Release_v4.0/source/kernel_src.tbz2
  #wget http://developer.download.nvidia.com/embedded/L4T/r21_Release_v3.0/sources/kernel_src.tbz2
  # Extract the kernel source
  tar -xvf kernel_src.tbz2
  cd kernel
  # Copy the current config
  zcat /proc/config.gz > .config
  # Make sure the config is ok (this shouldn't ask questions, if the kernel versions match)
  make menuconfig
  # Go to: "General setup -> Local version" and set your version name eg.: -duo3d
  # Save and exit
  # Comment out the following lines of code (lines 1439 & 1440):
  # 1439:	if (usb_endpoint_xfer_bulk(&ep->desc))
  # 1440:		max_packet = 512;
  nano drivers/usb/host/xhci-mem.c
  # Compile the kernel and the kernel modules
  make -j4
  make modules
  # Install the modules (/lib/modules/<your-kernel-version>/)
  sudo make modules_install
  # Backup previous kernel zImage file
  sudo mv /boot/zImage /boot/zImage.backup
  # Copy newly built kernel zImage into /boot directory
  sudo cp arch/arm/boot/zImage /boot/zImage

  echo "Step one complete" > ~/Documents/CL-DUO3D-ARM-1.0.35.226/step1
  # Reboot
  sudo reboot
else
  # Step 2
  uname -r
  # Build DUO Linux Kernel Module
  cd DUODriver
  # The make file will use your newly built kernel headers (~/Documents/Kernel/kernel)
  make
  # Install & load the driver
  sudo ./InstallDriver
  sudo ./LoadDriver
  # Display DUO node
  ls /dev/duo*
  rm ../step1
fi


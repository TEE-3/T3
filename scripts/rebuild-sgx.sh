# reinstall sgx-driver
cd ..

sudo rm -rf linux-sgx

git clone https://github.com/intel/linux-sgx-driver.git

cd linux-sgx-driver
#git checkout sgx2
dpkg-query -s linux-headers-$(uname -r)
sudo apt-get install linux-headers-$(uname -r)
make
sudo mkdir -p "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
sudo cp isgx.ko "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
sudo sh -c "cat /etc/modules | grep -Fxq isgx || echo isgx >> /etc/modules"    
sudo /sbin/depmod
sudo /sbin/modprobe isgx
cd ..
rm -rf linux-sgx-driver

#reinstall sgx-sdk
sudo apt-get install build-essential ocaml ocamlbuild automake autoconf libtool wget python libssl-dev libjsonrpccpp-dev libjsonrpccpp-tools nasm

git clone https://github.com/intel/linux-sgx.git

# checkout to 2.1 version

cd linux-sgx
git checkout sgx_2.1.3 

./download_prebuilt.sh
make
make sdk_install_pkg
make psw_install_pkg

cd linux/installer/bin
sudo ./sgx_linux_x64_sdk_*
sudo ./sgx_linux_x64_psw_*
cd ../../../..

rm -rf linux-sgx
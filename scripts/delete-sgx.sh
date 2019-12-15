# remove driver
sudo /sbin/modprobe -r isgx
sudo rm -rf "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"
sudo /sbin/depmod
sudo /bin/sed -i '/^isgx$/d' /etc/modules
# rm sgx
sudo /opt/intel/sgxsdk/uninstall.sh
sudo /opt/intel/sgxpsw/uninstall.sh

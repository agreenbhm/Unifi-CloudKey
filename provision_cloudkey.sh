#!/bin/bash

if [ "$EUID" -ne 0 ]
then
	echo "Please re-run with root privileges."
	exit
fi

mode="$1"

# Info messages in yellow
info() {
    command echo -e "\e[1;33m$@\e[0m"
}

# Success messages in green
success() {
    command echo -e "\e[1;32m$@\e[0m\n\n"
}

# Error messages in red
error() {
    echo -e "\e[1;31m$@\e[0m" >&2
}
exec 2> >(while read -r line; do error "$line"; done)

# Shutting off pre-installed Unifi and MongoDB
info "Disabling stock Unifi and database..."
systemctl disable unifi && systemctl stop unifi
systemctl disable mongodb && systemctl stop mongodb
success "Done"

# Skip setting repos if provisioning in VM mode
if [ "$mode" != "vm" ]
then
	# Existing Debian repo lists are outdated and don't work; replacing with working ones and adding additional ones
	info "Backing up repo lists and installing new ones..."
	mv /etc/apt/sources.list /etc/apt/sources.list.old
	mv /etc/apt/sources.list.d /etc/apt/sources.list.d.old
	echo 'deb https://download.docker.com/linux/debian jessie stable' > /etc/apt/sources.list
	#echo 'deb [ arch=amd64,arm64 ] https://www.ui.com/downloads/unifi/debian stable ubiquiti' >> /etc/apt/sources.list
	echo 'deb http://archive.debian.org/debian-security jessie/updates main contrib non-free' >> /etc/apt/sources.list
	echo 'deb http://archive.debian.org/debian/ jessie main contrib non-free' >> /etc/apt/sources.list
	success "Done"
fi

info "Updating apt and installing Docker and dependencies..."
# Fixing missing /dev/tty[0-9] requirement of some dependency.
# This doesn't do anything, but fixes install/config errors.
touch /dev/tty0
# Moving Docker dir to /srv/ partition to account for limited userdata space
systemctl stop docker
mkdir -p /srv/var/lib/docker
if [ -d "/var/lib/docker" ]
then
	mv /var/lib/docker /srv/var/lib/
fi
ln -sf /srv/var/lib/docker /var/lib/docker
apt update
if [ "$mode" != "vm" ]
then
	apt -yf install docker-ce
	success "Done"
	# There's some weird issue introduced with Docker 18.06.2 to address a CVE which
	# breaks Docker on this device.  18.06.1 works though.  Installing latest from repo and then
	# downgrading from .deb allows installation of dependencies.
	info "Downgrading Docker to working version and installing docker-compose..."
	wget https://download.docker.com/linux/debian/dists/jessie/pool/stable/armhf/docker-ce_18.06.1~ce~3-0~debian_armhf.deb
	dpkg -i docker-ce_18.06.1~ce~3-0~debian_armhf.deb
	wget https://download.docker.com/linux/debian/dists/buster/pool/stable/armhf/docker-compose-plugin_2.6.0~debian-buster_armhf.deb
	dpkg -i docker-compose-plugin_2.6.0~debian-buster_armhf.deb
	ln -s /usr/libexec/docker/cli-plugins/docker-compose /usr/bin/
else
	apt -yf install docker.io docker-compose
fi
success "Done"

# There's some weird issue introduced with Docker 18.06.2 to address a CVE which 
# breaks Docker on this device.  18.06.1 works though.  Installing latest from repo and then 
# downgrading from .deb allows installation of dependencies.
#info "Downgrading Docker to working version and installing docker-compose..."
#wget https://download.docker.com/linux/debian/dists/jessie/pool/stable/armhf/docker-ce_18.06.1~ce~3-0~debian_armhf.deb
#dpkg -i docker-ce_18.06.1~ce~3-0~debian_armhf.deb
#wget https://download.docker.com/linux/debian/dists/buster/pool/stable/armhf/docker-compose-plugin_2.6.0~debian-buster_armhf.deb
#dpkg -i docker-compose-plugin_2.6.0~debian-buster_armhf.deb
#ln -s /usr/libexec/docker/cli-plugins/docker-compose /usr/bin/
#success "Done"


# Checking Unifi version via the MongoDB container so version is still available if the Unifi 
# container is stopped, which is necessary for the webui.
# Changing service start/stop functions to call Docker instead of the host's service.
# Known Issue - the Maintenance page might show the service as stopped for an extended length
# of time after clicking 'Start' as it takes a while for the container to start.
info "Making changes to host system webui to support new Unifi version in Docker..."
usermod -a -G docker www-data
cp /usr/share/cloudkey-webui/www/common.inc /usr/share/cloudkey-webui/www/common.inc.bak
cp /usr/share/cloudkey-webui/www/settings.inc /usr/share/cloudkey-webui/www/settings.inc.bak
cp /usr/share/cloudkey-webui/www/api.inc /usr/share/cloudkey-webui/www/api.inc.bak
#sed -i "s/\$cmd = 'dpkg-query/\$cmd = 'docker exec -t unifi dpkg-query/g" /usr/share/cloudkey-webui/www/common.inc
sed -i "s/\$cmd = 'dpkg-query/\$cmd = 'docker exec -t mongodb dpkg-query/g" /usr/share/cloudkey-webui/www/common.inc
echo '@reboot /usr/bin/touch /var/run/unifi_runtime.cfg' >> /var/spool/cron/crontabs/root
# if [ ! -f "/etc/rc.local" ]
# then
# 	echo -e '#!/bin/sh\n\nexit 0' > /etc/rc.local
#  	chmod +x /etc/rc.local
# fi
#sed -i 's/^exit 0$/touch \/var\/run\/unifi_runtime.cfg\nexit 0/g' /etc/rc.local
sed -i "s/.*CMD_SERVICE_UNIFI.*/define('CMD_SERVICE_UNIFI', '\/usr\/bin\/docker ');/g" /usr/share/cloudkey-webui/www/settings.inc
sed -i "s/exec(CMD_SERVICE_UNIFI . ' start', \$out, \$rc);/exec(CMD_SERVICE_UNIFI . ' start unifi', \$out, \$rc);/g" /usr/share/cloudkey-webui/www/api.inc
sed -i "s/exec(CMD_SERVICE_UNIFI . ' stop', \$out, \$rc);/exec(CMD_SERVICE_UNIFI . ' stop unifi', \$out, \$rc);/g" /usr/share/cloudkey-webui/www/api.inc
success "Done"

info "Building Docker image - this will probably take a while..."
docker build -t agreenbhm/unifi-cloudkey:8.4.59 --network host .
success "Done"

info "Starting containers..."
docker-compose up -d
success "Finished!"

error "Need to reboot. Please do so manually."

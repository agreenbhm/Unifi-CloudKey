#!/bin/bash

echo "Disabling stock Unifi and database..."
systemctl disable unifi && systemctl stop unifi
systemctl disable mongodb && systemctl stop mongodb
echo "Done"

echo "Backing up repo lists and installing new ones..."
mv /etc/apt/sources.list /etc/apt/sources.list.old
mv /etc/apt/sources.list.d /etc/apt/sources.list.d.old
echo 'deb https://download.docker.com/linux/debian jessie stable' > /etc/apt/sources.list
echo 'deb [ arch=amd64,arm64 ] https://www.ui.com/downloads/unifi/debian stable ubiquiti' >> /etc/apt/sources.list
echo 'deb http://archive.debian.org/debian-security jessie/updates main contrib non-free' >> /etc/apt/sources.list
echo 'deb http://archive.debian.org/debian/ jessie main contrib non-free' >> /etc/apt/sources.list
echo "Done"

echo "Updating apt and installing Docker and dependencies..."
touch /dev/tty0
apt update && apt -yf install docker-ce
echo "Done"

echo "Downgrading Docker to working version and installing docker-compose..."
wget https://download.docker.com/linux/debian/dists/jessie/pool/stable/armhf/docker-ce_18.06.1~ce~3-0~debian_armhf.deb
dpkg -i docker-ce_18.06.1~ce~3-0~debian_armhf.deb
wget https://download.docker.com/linux/debian/dists/buster/pool/stable/armhf/docker-compose-plugin_2.6.0~debian-buster_armhf.deb
dpkg -i docker-compose-plugin_2.6.0~debian-buster_armhf.deb
ln -s /usr/libexec/docker/cli-plugins/docker-compose /usr/bin/
echo "Done"

echo "Making changes to host system webui to support new Unifi version in Docker..."
usermod -a -G docker www-data
cp /usr/share/cloudkey-webui/www/common.inc /usr/share/cloudkey-webui/www/common.inc.bak
cp /usr/share/cloudkey-webui/www/settings.inc /usr/share/cloudkey-webui/www/settings.inc.bak
cp /usr/share/cloudkey-webui/www/api.inc /usr/share/cloudkey-webui/www/api.inc.bak

#checking version via the mongodb container so version is still available if the unifi package is stopped
# Known Issue - the Maintenance page might show the service as stopped for an extended length of time after clicking 'Start' as it takes a while for the container to start
#sed -i "s/\$cmd = 'dpkg-query/\$cmd = 'docker exec -t unifi dpkg-query/g" /usr/share/cloudkey-webui/www/common.inc
sed -i "s/\$cmd = 'dpkg-query/\$cmd = 'docker exec -t mongodb dpkg-query/g" /usr/share/cloudkey-webui/www/common.inc
sed -i 's/^exit 0$/touch \/var\/run\/unifi_runtime.cfg\nexit 0/g' /etc/rc.local
sed -i "s/.*CMD_SERVICE_UNIFI.*/define('CMD_SERVICE_UNIFI', '\/usr\/bin\/docker ');/g" /usr/share/cloudkey-webui/www/settings.inc
sed -i "s/exec(CMD_SERVICE_UNIFI . ' start', \$out, \$rc);/exec(CMD_SERVICE_UNIFI . ' start unifi', \$out, \$rc);/g" /usr/share/cloudkey-webui/www/api.inc
sed -i "s/exec(CMD_SERVICE_UNIFI . ' stop', \$out, \$rc);/exec(CMD_SERVICE_UNIFI . ' stop unifi', \$out, \$rc);/g" /usr/share/cloudkey-webui/www/api.inc

echo "Done"

echo "Building Docker image - this will probably take a while..."
docker build -t agreenbhm/unifi-cloudkey:8.4.59 --network host .
echo "Done"

echo "Starting containers..."
docker-compose up -d
echo "Finished!"
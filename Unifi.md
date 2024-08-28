* Update sources.list files to include:
  
	`deb https://download.docker.com/linux/debian jessie stable`
	`deb [ arch=amd64,arm64 ] https://www.ui.com/downloads/unifi/debian stable ubiquiti`
	`deb http://archive.debian.org/debian-security jessie/updates main contrib non-free
	`deb http://archive.debian.org/debian/ jessie main contrib non-free`
- Enable Unifi (via file)
	- `touch /var/run/unifi_runtime.cfg`
- Fix Unifi Version check
	- `usermod -a -G docker www-data`
	- `sed -i "s/\$cmd = 'dpkg-query/\$cmd = 'docker exec -t unifi dpkg-query/g" /usr/share/cloudkey-webui/www/common.inc`
 - Install Docker
	* `apt update && apt install docker-ce`
* Disable Unifi
	* `systemctl disable unifi && systemctl stop unifi`
* Disable Mongodb
	* `systemctl disable mongodb && systemctl stop mongodb`
	
* Downgrade Docker (to fix bug)
	* `wget https://download.docker.com/linux/debian/dists/jessie/pool/stable/armhf/docker-ce_18.06.1~ce~3-0~debian_armhf.deb`
	* `dpkg -i docker-ce_18.06.1~ce~3-0~debian_armhf.deb
	
* Setup MongoDB Container
	* `docker pull debian:buster
	* `docker run -it --name mongodb --network host --hostname mongodb debian:buster /bin/bash`
	
* Configure MongoDB Container
	* `apt update`
	* `echo 'deb [trusted=yes] https://andrewharle.github.io/REPO/debian buster experimental' > /etc/apt/sources.list.d/mongodb.list`
	*  `apt install nano ca-certificates && apt update && apt install mongodb`
	* `echo 'storageEngine=mmapv1' >> /etc/mongodb.conf`
	* `/etc/init.d/mongodb start`
	* 

* Setup Unifi Container
	* `docker pull debian:bullseye
	* `docker run -it --name unifi --network host --hostname unifi debian:bullseye /bin/bash`
- Configure Unfi Container
	- `apt update && apt install nano ca-certificates openjdk-17-jdk`
	- `echo 'deb [trusted=yes] http://www.ubnt.com/downloads/unifi/debian cloudkey-stable ubiquiti' > /etc/apt/sources.list.d/ubnt-unifi.list`
- Run Custom Docker Container
	- `docker run -it --name unifi --hostname unifi --network host agreenbhm/unifi-cloudkey:8.4.59 /entrypoint.sh`
	- 
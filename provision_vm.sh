#!/bin/bash

if [ "$EUID" -ne 0 ]
then
	echo "Please re-run with root privileges."
	exit
fi

dpkg --add-architecture armhf
apt update
apt install -yf nginx php-fpm libc6:armhf libstdc++6:armhf libpam0g:armhf libevent-2.1-7:armhf zlib1g:armhf libselinux1:armhf libcrypt1:armhf libpam-modules:armhf
ln -s /lib/arm-linux-gnueabihf/libevent-2.1.so.7 /lib/arm-linux-gnueabihf/libevent-2.0.so.5
wget https://ftp.debian.org/debian/pool/main/g/glibc/multiarch-support_2.28-10+deb10u1_arm64.deb
wget https://snapshot.debian.org/archive/debian/20120314T034457Z/pool/main/o/openssl/libssl1.0.0_1.0.0h-1_armhf.deb
dpkg -i multiarch-support_2.28-10+deb10u1_arm64.deb
dpkg -i libssl1.0.0_1.0.0h-1_armhf.deb
mv -f /etc/nginx /etc/nginx.old
cp -R cloudkey_stock/etc/* /etc/
cp -R cloudkey_stock/usr/* /usr/
cp cloudkey_stock/sbin/* /sbin/
chmod +x /sbin/ubnt-systool /sbin/chpasswd /sbin/pwcheck /sbin/ubnt-tools /sbin/ubnt-systemhub
systemctl enable ubnt-systemhub
systemctl start ubnt-systemhub
chmod 755 /etc/ssl/private
sed -i 's/fastcgi_pass unix:\/var\/run\/php5-fpm.sock;/fastcgi_pass unix:\/var\/run\/php\/php-fpm.sock;/g'  /etc/nginx/sites-enabled/cloudkey-webui
sed -i 's/\$trline{0}/\$trline\[0\]/g' /usr/share/cloudkey-webui/www/common.inc
mkdir -p /run/php/{sessions,tmp}
chown -R www-data:www-data /run/php/{sessions,tmp}
systemctl restart nginx

if [ "$1" == "test" ]
then
	exit 0
fi

./provision_cloudkey.sh vm

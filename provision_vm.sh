#!/bin/bash

if [ "$EUID" -ne 0 ]
then
	echo "Please re-run with root privileges."
	exit
fi

dpkg --add-architecture armhf
apt update
apt install -yf nginx php-fpm libc6:armhf libstdc++6:armhf
mv -f /etc/nginx /etc/nginx.old
cp -R cloudkey_stock/etc/* /etc/
cp -R cloudkey_stock/usr/* /usr/
cp cloudkey_stock/sbin/* /sbin/
chmod +x /sbin/ubnt-systool /sbin/chpasswd /sbin/pwcheck
chmod 755 /etc/ssl/private
sed -i 's/fastcgi_pass unix:\/var\/run\/php5-fpm.sock;/fastcgi_pass unix:\/var\/run\/php\/php-fpm.sock;/g'  /etc/nginx/sites-enabled/cloudkey-webui
sed -i 's/\$trline{0}/\$trline\[0\]/g' /usr/share/cloudkey-webui/www/common.inc
systemctl restart nginx

if [ "$1" == "test" ]
then
	exit 0
fi

./provision_cloudkey.sh vm

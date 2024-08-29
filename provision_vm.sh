#!/bin/bash

if [ "$EUID" -ne 0 ]
then
	echo "Please re-run with root privileges."
	exit
fi

apt update
apt install -yf nginx php-fpm
mv /etc/nginx /etc/nginx.old
cp -R cloudkey_stock/etc/* /etc/
cp -R cloudkey_stock/usr/* /usr/
chmod 755 /etc/ssl/private
sed -i 's/fastcgi_pass unix:\/var\/run\/php5-fpm.sock;/fastcgi_pass unix:\/var\/run\/php\/php-fpm.sock;/g'  /etc/nginx/sites-enabled/cloudkey-webui
systemctl restart nginx

./provision_cloudkey.sh vm

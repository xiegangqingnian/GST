#!/bin/bash
echo "正在安装 php7.2..."
rpm -Uvh https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
rpm -Uvh https://mirror.webtatic.com/yum/el7/webtatic-release.rpm

yum install -y php72w php72w-cli php72w-opcache php72w-pecl-apcu php72w-pecl-redis php72w-pecl-mongodb php72w-xml php72w-intl php72w-mbstring

echo "正在安装composer..."
cd /work
curl -sS https://getcomposer.org/installer | php
mv composer.phar  /usr/local/bin/composer

echo "done."
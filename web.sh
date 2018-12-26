#!/bin/bash
echo "正在停止浏览器前端服务..."
pkill ng$
sleep 0.5
echo "正在停止浏览器后台服务..."
systemctl stop nginx
sleep 1s

rm -f /work/wwwlogs/*
rm -f /work/gst_install/tracker/trackerapi/var/logs/*
rm -f /work/gst_install/tracker/trackerapi/var/cache/dev/*
rm -f /work/gst_install/tracker/trackerapi/var/cache/prod/*

echo "正在启动浏览器后台服务..."
systemctl start nginx

echo "正在启动浏览器前端服务..."
cd /work/gst_install/tracker/frontend
nohup ng serve --host 0.0.0.0 --port 4200 >/dev/null 2>&1 &
sleep 3s

systemctl status nginx
ps -al

netstat -lntp
#!/bin/bash
echo "正在停止区块nodeos服务..."
pkill nodgst
sleep 0.5
echo "正在停止钱包keosd服务..."
pkill kgstd

echo "正在停止浏览器前端服务..."
pkill ng$
sleep 0.5
echo "正在停止浏览器后台服务..."
systemctl stop nginx
sleep 1s

rm -rf /work/wwwlogs/*
rm -rf /work/gst_install/tracker/trackerapi/var/logs/*
rm -rf /work/gst_install/tracker/trackerapi/var/cache/dev/*
rm -rf /work/gst_install/tracker/trackerapi/var/cache/prod/*

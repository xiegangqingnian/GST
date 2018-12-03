#!/bin/bash
cd /work/gst_install/monitor/EOS-Network-monitor/netmon-frontend
echo "正在停止浏览器前端服务..."
pkill node$
sleep 0.5
cd /work/gst_install/monitor/EOS-Network-monitor/netmon-backend
echo "正在停止浏览器后台服务..."
pm2 kill
sleep 3s
npm install
echo "正在启动浏览器后台服务..."
pm2 start ecosystem.config.js

echo "正在启动浏览器前端服务..."
cd /work/gst_install/monitor/EOS-Network-monitor/netmon-frontend
n stable
yarn
yarn build
nohup yarn start >/dev/null 2>&1 &


sleep 2s

netstat -lntp


#!/bin/bash
echo "正在停止浏览器服务..."
pkill node$
cd /work/monitor-work/EOS-Network-monitor/netmon-backend
pm2 kill
sleep 0.5
echo "正在停止区块nodeos服务..."
pkill nodeos
sleep 0.5
echo "正在停止钱包keosd服务..."
pkill keosd

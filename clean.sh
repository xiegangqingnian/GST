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
echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[35m 正在清除区块数据...\033[0m"
rm -rf /work/gst_install/nodgst/data/*
rm -f /work/gst_install/wallet/wallet.log

ret=$(ps -ef |grep mongod|grep -v grep|wc -l)
if [ $ret -lt 2 ];then
   echo "the mongod service is ERR"
   systemctl start mongod
fi
mongo 127.0.0.1:27017/gstdb  --eval 'db.dropDatabase();'
echo -e "\033[31m----------------------------------------------------------------------\033[0m"
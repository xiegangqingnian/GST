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
sleep 3s

cd /work/eos_install
nohup /work/eos_install/eos/build/bin/nodeos --data-dir=/work/eos_install/nodeos/data \
--config-dir=/work/eos_install/nodeos/config --max-transaction-time=1000 > /work/eos_install/nodeos/data/bpnode.log 2>&1 &

#nohup /work/eos_install/eos/build/bin/keosd --data-dir=/work/eos_install/wallet \
#--config-dir=/work/eos_install/wallet > /work/eos_install/wallet/wallet.log 2>&1 &

alias cleos='cleos -u http://127.0.0.1:8888 --wallet-url http://127.0.0.1:8900'

echo "nodeos service restart..."
sleep 2s

echo "PW5Jbrw443s1toFjCxVP9EuiL7nSDHGX2Hm26LoKQu4U4oTxiyqoB" | cleos wallet unlock

cleos wallet list

echo "钱包中的私钥列表..."
echo "PW5Jbrw443s1toFjCxVP9EuiL7nSDHGX2Hm26LoKQu4U4oTxiyqoB" | cleos wallet private_keys

echo "公钥PW5Jbrw44* 对应的帐户列表..."
cleos get accounts EOS4zx1a8BcodmZQ5jW4Csj3ussy1tj9AxwhDp2Tx3GoSqkzQfgkA

echo "eosio account info:"
cleos get account eosio

echo "account eosio currency balance:"
cleos get currency balance eosio.token eosio EOS

echo "EOS currency stats:"
cleos get currency stats eosio.token EOS

echo "account bp1 info:"
cleos get account bp1
sleep 1s

echo "account voter1 info:"
cleos get account voter2
sleep 1s

echo "account bp1 info:"
cleos get account bp1
sleep 1s

echo "当前区块信息..."
cleos get info

 
cd /work/monitor-work/EOS-Network-monitor/netmon-backend
#npm install
echo "正在启动浏览器后台服务..."
pm2 start ecosystem.config.js

echo "正在启动浏览器前端服务..."
cd /work/monitor-work/EOS-Network-monitor/netmon-frontend
#yarn prebuild
#yarn build
nohup yarn start >/dev/null 2>&1 &

cd /work/eos_install

netstat -lntp

echo "done..."
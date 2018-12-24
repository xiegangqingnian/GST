#!/bin/bash
echo "正在停止区块nodgst服务..."
pkill nodgst
sleep 0.5
echo "正在停止钱包kgstd服务..."
pkill kgstd
sleep 3s

echo "正在清除区块数据..."
rm -rf /work/gst_install/nodgst/data/*


chown -R mongod:mongod /work/monitor-work/mongo
chmod -R 777 /work/monitor-work/mongo

echo "Initialization config.ini"
\cp -f /work/gst_install/nodgst/config/config_init.ini /work/gst_install/nodgst/config/config.ini

nohup /work/gst_install/gst/build/bin/nodgst --data-dir=/work/gst_install/nodgst/data \
--config-dir=/work/gst_install/nodgst/config \
--genesis-json=/work/gst_install/nodgst/config/genesis.json  --max-transaction-time=1000 > /work/gst_install/nodgst/data/bpnode.log 2>&1 &

#nohup /work/gst_install/gst/build/bin/kgstd --data-dir=/work/gst_install/wallet \
#--config-dir=/work/gst_install/wallet > /work/gst_install/wallet/wallet.log 2>&1 &

alias clgst='clgst -u http://127.0.0.1:8888 --wallet-url http://127.0.0.1:8900'

echo "nodeo service start..."

echo "PW5JXmhmMC76zubANLoY9zA9uvvHr7aS5JTUBfRenFv1EAm6ADUvW" | clgst wallet unlock

clgst wallet list

clgst wallet import --private-key 5JfxbuzqBVTc1SsHQ9RFY4aExuNtGZapGyCe6uapdZAPwKcBAAz
clgst wallet import --private-key 5KarHtfxsJY9Ei8kmZpFo5GeA3rEWmC6Qe9UubkKfvLchE5MbtE
clgst wallet import --private-key 5JF6qVx2otj5d6TqzGxpQQgoULj9r2GCZH5NPw16PwPS3BGFFEv
clgst wallet import --private-key 5K4SqTVrWVauwM6je8pWxi9aAwoqtwnByqZcmZnVSaAc666793X

sleep 2s

echo "creating system account..."

clgst create account gstio gstio.bpay GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.msig GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.names GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.ram GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.ramfee GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.saving GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.stake GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.token GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.vpay GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.fee GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo "正在安装合约..."
cd /work/gst_install/gst

echo "1. 安装gstio.bios合约..."
clgst set contract gstio build/contracts/gstio.bios -p gstio
 

echo "2. 安装gstio.token合约..."
clgst set contract gstio.token build/contracts/gstio.token -p gstio.token

echo "3. 安装gstio.msig合约..."
clgst set contract gstio.msig build/contracts/gstio.msig -p gstio.msig

echo "4. 发行代币 GST 发行人gstio ..." 
clgst push action gstio.token create '["gstio", "1000000000.0000 GST"]' -p gstio.token

echo "5. 将代币GST资产打入gstio账户 ..."
clgst push action gstio.token issue '["gstio", "1000000000.0000 GST", "issue message"]' -p gstio

echo "6. 安装gstio.system 合约..."
clgst set contract gstio build/contracts/gstio.system -p gstio -x 1000
 

echo "gstio account info:"
clgst get account gstio

sleep 2s

echo "create account voter1 with --stake-net \"20000 GST\" --stake-cpu \"20000 GST\" --buy-ram \"20000 GST\""
clgst system newaccount gstio voter1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "20000 GST" --stake-cpu "20000 GST" --buy-ram "20000 GST"

echo "create account voter2/voter3/voter4/voter5 with --stake-net \"2000 GST\" --stake-cpu \"2000 GST\" --buy-ram \"2000 GST\""
clgst system newaccount gstio voter2 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "2000 GST" --stake-cpu "2000 GST" --buy-ram "2000 GST"
clgst system newaccount gstio voter3 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "2000 GST" --stake-cpu "2000 GST" --buy-ram "2000 GST"
clgst system newaccount gstio voter4 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "2000 GST" --stake-cpu "2000 GST" --buy-ram "2000 GST"
clgst system newaccount gstio voter5 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "2000 GST" --stake-cpu "2000 GST" --buy-ram "2000 GST"

echo "create account bp1 with --stake-net \"20000 GST\" --stake-cpu \"20000 GST\" --buy-ram \"20000 GST\""
clgst system newaccount gstio bp1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU --stake-net "20000 GST" --stake-cpu "20000 GST" --buy-ram "20000 GST"

echo "将bp1注册为超级节点..."
clgst system regproducer bp1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz

echo "account gstio currency balance:"
clgst get currency balance gstio.token gstio GST

echo "GST currency stats:"
clgst get currency stats gstio.token GST

echo "将150000000.0000 GST 从gstio转账到voter1..."
clgst transfer gstio voter1 "150000000.0000 GST" -p gstio

echo "将10000.0000 GST 从gstio转账到voter2..."
clgst transfer gstio voter2 "10000.0000 GST" -p gstio

echo "将10000.0000 GST 从gstio转账到bp1..."
clgst transfer gstio bp1 "10000.0000 GST" -p gstio

echo "voter1 抵押 150000000.0000 GST for cpu, 0 GST for net..."
clgst system delegatebw voter1 voter1 "150000000.0000 GST" "0 GST"

echo "voter2 抵押 1000.0000 GST for cpu, 1000 GST for net..."
clgst system delegatebw voter2 voter2 "1000.0000 GST" "1000.0000 GST"


echo "voter1 投票选举节点 bp1..."
clgst system voteproducer prods voter1 bp1

echo "voter2 投票选举节点 bp1..."
clgst system voteproducer prods voter2 bp1

echo "account voter1 info:"
clgst get account voter1

echo "account voter2 info:"
clgst get account voter2

echo "account bp1 info:"
clgst get account bp1

echo "等待区块信息更新 5s..."
sleep 5s

echo "当前区块信息..."
clgst get info

sleep 3s

echo "正在重新启动区块nodgst服务..."
pkill nodgst
pkill kgstd

sleep 3s

echo "Initialization config.ini"
\cp -f /work/gst_install/nodgst/config/config_produce.ini /work/gst_install/nodgst/config/config.ini

nohup /work/gst_install/gst/build/bin/nodgst --data-dir=/work/gst_install/nodgst/data \
--config-dir=/work/gst_install/nodgst/config --max-transaction-time=1000 > /work/gst_install/nodgst/data/bpnode.log 2>&1 &

echo "区块nodgst服务已重启..."

cd /work/gst_install

sleep 2s

netstat -lntp

echo "done..."

#!/bin/bash
echo -e "\033[35m 正在停止区块nodgst服务...\033[0m"
pkill nodgst
sleep 0.5
echo -e "\033[35m 正在停止钱包kgstd服务...\033[0m"
pkill kgstd
sleep 3s

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

chown -R mongod:mongod /work/gst_install/mongo
chmod -R 777 /work/gst_install/mongo

echo "Initialization config.ini"
\cp -f /work/gst_install/nodgst/config/config_init.ini /work/gst_install/nodgst/config/config.ini

echo -e "\033[32m 正在启动区块nodgst服务...\033[0m"
nohup /work/gst_install/gst/build/bin/nodgst --data-dir=/work/gst_install/nodgst/data \
--config-dir=/work/gst_install/nodgst/config \
--genesis-json=/work/gst_install/nodgst/config/genesis.json  --max-transaction-time=3000 > /work/gst_install/nodgst/data/bpnode.log 2>&1 &

echo -e "\033[32m 正在启动钱包服务...\033[0m"
nohup /work/gst_install/gst/build/bin/kgstd --data-dir=/work/gst_install/wallet \
--config-dir=/work/gst_install/wallet > /work/gst_install/wallet/wallet.log 2>&1 &

alias clgst='clgst -u http://127.0.0.1:8888 --wallet-url http://127.0.0.1:8900'

echo -e "\033[32m nodeo service start...\033[0m"

echo "PW5Ka7m2J2wLAhA3wbJPQ9XhFMc5KVLVE2qYTETr6cEvwqyrTGJAw" | clgst wallet unlock

clgst wallet list

clgst wallet import --private-key 5JfxbuzqBVTc1SsHQ9RFY4aExuNtGZapGyCe6uapdZAPwKcBAAz
clgst wallet import --private-key 5KarHtfxsJY9Ei8kmZpFo5GeA3rEWmC6Qe9UubkKfvLchE5MbtE
clgst wallet import --private-key 5JF6qVx2otj5d6TqzGxpQQgoULj9r2GCZH5NPw16PwPS3BGFFEv
clgst wallet import --private-key 5K4SqTVrWVauwM6je8pWxi9aAwoqtwnByqZcmZnVSaAc666793X

sleep 3s


STALE_NOD=$(netstat -ln|grep -o 8888)
if [ "$STALE_NOD" == "" ]; then
      cat /work/gst_install/nodgst/data/bpnode.log | grep -B 1 ^error

      rm -rf /work/gst_install/nodgst/data/*
      mongo 127.0.0.1:27017/gstdb  --eval 'db.dropDatabase();'

      nohup /work/gst_install/gst/build/bin/nodgst --data-dir=/work/gst_install/nodgst/data \
	--config-dir=/work/gst_install/nodgst/config --max-transaction-time=3000 > /work/gst_install/nodgst/data/bpnode.log 2>&1 &
	sleep 2s
fi

echo -e "\033[32m 正在创建系统帐户...\033[0m"

clgst create account gstio gstio.bpay GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.msig GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.names GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.ram GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.ramfee GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.fee GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.saving GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.stake GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.token GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst create account gstio gstio.vpay GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[32m 正在安装合约...\033[0m"
cd /work/gst_install/gst

echo -e "\033[32m 1. 安装gstio.bios合约...\033[0m"
clgst set contract gstio build/contracts/gstio.bios -p gstio
 

echo -e "\033[32m 2. 安装gstio.token合约...\033[0m"
clgst set contract gstio.token build/contracts/gstio.token -p gstio.token

echo -e "\033[32m 3. 安装gstio.msig合约...\033[0m"
clgst set contract gstio.msig build/contracts/gstio.msig -p gstio.msig

echo -e "\033[32m 4. 发行代币 GST 发行人gstio ...\033[0m"
clgst push action gstio.token create '["gstio", "1000000000.0000 GST"]' -p gstio.token

echo -e "\033[32m 5. 将代币GST资产打入gstio账户 ...\033[0m"
clgst push action gstio.token issue '["gstio", "1000000000.0000 GST", "issue message"]' -p gstio

echo -e "\033[32m 6. 安装gstio.system 合约...\033[0m"
clgst set contract gstio build/contracts/gstio.system -p gstio -x 1000
 

echo "gstio account info:"
clgst get account gstio

sleep 2s

echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[32m create account voter1 \033[0m"
echo -e "\033[31m----------------------------------------------------------------------\033[0m"
clgst system newaccount gstio voter1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[32m create account voter2/voter3/voter4/voter5 \033[0m"
echo -e "\033[31m----------------------------------------------------------------------\033[0m"

clgst system newaccount gstio voter2 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst system newaccount gstio voter3 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst system newaccount gstio voter4 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU
clgst system newaccount gstio voter5 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[35m create account bp1\033[0m"
clgst system newaccount gstio bp1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[35m create account bp2\033[0m"
clgst system newaccount gstio bp2 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[35m create account bp3\033[0m"
clgst system newaccount gstio bp3 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU

echo -e "\033[35m create account bp4\033[0m"
clgst system newaccount gstio bp4 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz GST659CGCoztnkD5cswR6eeLHZS4Le1E5QJzX6zt7e45j8MEwVaGU


echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[35m 将bp1注册为超级节点...\033[0m"
clgst system regproducer bp1 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz

echo -e "\033[35m 将bp2注册为超级节点...\033[0m"
clgst system regproducer bp2 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz

echo -e "\033[35m 将bp3注册为超级节点...\033[0m"
clgst system regproducer bp3 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz

echo -e "\033[35m 将bp4注册为超级节点...\033[0m"
clgst system regproducer bp4 GST8MfTEtHsMU1AGL4LYbYx3eiU9iVK3K6WXUEoJHkieVAbj9gHDz
echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo "account gstio currency balance:"
clgst get currency balance gstio.token gstio GST

echo "GST currency stats:"
clgst get currency stats gstio.token GST

echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[32m gstio转账到投票用户 \033[0m"
echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo -e "\033[32m 将151000000.0000 GST 从gstio转账到voter1...\033[0m"
clgst transfer gstio voter1 "151000000.0000 GST" -p gstio

echo -e "\033[32m 将120000.0000 GST 从gstio转账到voter2...\033[0m"
clgst transfer gstio voter2 "120000.0000 GST" -p gstio

echo -e "\033[32m 将120000.0000 GST 从gstio转账到voter3...\033[0m"
clgst transfer gstio voter3 "120000.0000 GST" -p gstio

echo -e "\033[32m 将120000.0000 GST 从gstio转账到voter4...\033[0m"
clgst transfer gstio voter4 "120000.0000 GST" -p gstio

echo -e "\033[32m 将120000.0000 GST 从gstio转账到voter5...\033[0m"
clgst transfer gstio voter5 "120000.0000 GST" -p gstio

echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo -e "\033[32m 将12000.0000 GST 从gstio转账到bp1...\033[0m"
clgst transfer gstio bp1 "12000.0000 GST" -p gstio

echo -e "\033[32m 将12000.0000 GST 从gstio转账到bp2...\033[0m"
clgst transfer gstio bp2 "12000.0000 GST" -p gstio

echo -e "\033[32m 将12000.0000 GST 从gstio转账到bp3...\033[0m"
clgst transfer gstio bp3 "12000.0000 GST" -p gstio

echo -e "\033[32m 将12000.0000 GST 从gstio转账到bp4...\033[0m"
clgst transfer gstio bp4 "12000.0000 GST" -p gstio

echo -e "\033[31m----------------------------------------------------------------------\033[0m"


echo -e "\033[32m voter1 抵押 150000000.0000 GST for cpu, 0 GST for net...\033[0m"
clgst system delegatebw voter1 voter1 "150000000.0000 GST" "0 GST"

echo -e "\033[32m voter2 抵押 90000.0000 GST for cpu, 10000.0000 GST for net...\033[0m"
clgst system delegatebw voter2 voter2 "90000.0000 GST" "10000.0000 GST"

echo -e "\033[32m voter3 抵押 90000.0000 GST for cpu, 10000.0000 GST for net...\033[0m"
clgst system delegatebw voter3 voter3 "90000.0000 GST" "10000.0000 GST"

echo -e "\033[32m voter4 抵押 90000.0000 GST for cpu, 10000.0000 GST for net...\033[0m"
clgst system delegatebw voter4 voter4 "90000.0000 GST" "10000.0000 GST"

echo -e "\033[32m voter5 抵押 90000.0000 GST for cpu, 10000.0000 GST for net...\033[0m"
clgst system delegatebw voter5 voter5 "90000.0000 GST" "10000.0000 GST"

echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo -e "\033[32m voter1 投票选举节点 bp1...\033[0m"
clgst system voteproducer prods voter1 bp1


echo "account voter1 info:"
clgst get account voter1

echo "account voter2 info:"
clgst get account voter2

echo "account bp1 info:"
clgst get account bp1

echo -e "\033[35m 等待区块信息更新 2s...\033[0m"
sleep 2s

echo "当前区块信息..."
clgst get info

sleep 2s

echo -e "\033[35m 正在重新启动区块nodgst服务...\033[0m"
pkill nodgst
pkill kgstd

sleep 2s

echo "Initialization config.ini"
\cp -f /work/gst_install/nodgst/config/config_produce.ini /work/gst_install/nodgst/config/config.ini

nohup /work/gst_install/gst/build/bin/nodgst --data-dir=/work/gst_install/nodgst/data \
--config-dir=/work/gst_install/nodgst/config --max-transaction-time=3000 > /work/gst_install/nodgst/data/bpnode.log 2>&1 &
echo -e "\033[35m 区块nodgst服务已重启...\033[0m"

nohup /work/gst_install/gst/build/bin/kgstd --data-dir=/work/gst_install/wallet \
--config-dir=/work/gst_install/wallet > /work/gst_install/wallet/wallet.log 2>&1 &
echo -e "\033[35m 钱包服务已重启...\033[0m"

cd /work/gst_install

sleep 1s

echo "PW5Ka7m2J2wLAhA3wbJPQ9XhFMc5KVLVE2qYTETr6cEvwqyrTGJAw" | clgst wallet unlock

sleep 1s
clgst wallet list

netstat -lntp

echo "done..."
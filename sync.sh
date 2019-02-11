#!/bin/bash
echo "synchronizing..."
\cp -f /data/zjw/work/gst_install/*.sh /data/lcz/work/gst_install/
\cp -f /data/zjw/work/gst_install/gst/build/bin/* /data/lcz/work/gst_install/gst/build/bin/


rm -f /data/lcz/work/gst_install/sync.sh
rm -f /data/lcz/work/gst_install/restart1.sh
rm -f /data/lcz/work/gst_install/mg.sh

rm -f /data/lcz/work/gst_install/wallet/*.wallet
\cp -f /data/zjw/work/gst_install/wallet/default.wallet /data/lcz/work/gst_install/wallet


\cp -f /data/zjw/work/gst_install/nodgst/config/*.ini /data/lcz/work/gst_install/nodgst/config
\cp -f /data/zjw/work/gst_install/nodgst/config/genesis.json /data/lcz/work/gst_install/nodgst/config


 
bps=("bp2" "bp3" "bp4")
bpips=("192.168.105.205" "192.168.105.206" "192.168.105.207")

# ${#users[@]} 返回数组的大小
for ((i=0; i<${#bps[@]};i++));
do
	echo -e "\033[31m----------------------------------------------------------------------\033[0m"
	echo "do  ${bps[i]}"
	echo -e "\033[31m----------------------------------------------------------------------\033[0m"
        \cp -f /data/zjw/work/gst_install/*.sh /data/bps/${bps[i]}/work/gst_install/
	\cp -f /data/zjw/work/gst_install/gst/build/bin/* /data/bps/${bps[i]}/work/gst_install/gst/build/bin/
	rm -f /data/bps/${bps[i]}/work/gst_install/sync.sh
	rm -f /data/bps/${bps[i]}/work/gst_install/restart1.sh
	rm -f /data/bps/${bps[i]}/work/gst_install/mg.sh
	rm -f /data/bps/${bps[i]}/work/gst_install/wallet/*.wallet
	\cp -f /data/zjw/work/gst_install/wallet/default.wallet /data/bps/${bps[i]}/work/gst_install/wallet
	
	\cp -f /data/zjw/work/gst_install/nodgst/config/genesis.json /data/bps/${bps[i]}/work/gst_install/nodgst/config


	\cp -f /data/zjw/work/gst_install/nodgst/config/config_template.ini /data/bps/${bps[i]}/work/gst_install/nodgst/config/config.ini
 
	echo -e "\nproducer-name = ${bps[i]}" >> /data/bps/${bps[i]}/work/gst_install/nodgst/config/config.ini
	docker exec -it centos_${bps[i]} /bin/bash -c "/work/gst_install/bpstart.sh"
done

echo -e "\033[31m----------------------------------------------------------------------\033[0m"
echo -e "\033[32m 执行投票选举节点...\033[0m"
docker exec -it centos_bzjw /bin/bash -c "/work/gst_install/vote.sh"
echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo "done..."
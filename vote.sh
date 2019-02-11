#!/bin/bash
echo -e "\033[31m----------------------------------------------------------------------\033[0m"

echo -e "\033[32m voter2 投票选举节点 bp2...\033[0m"
clgst system voteproducer prods voter2 bp2

echo -e "\033[32m voter3 投票选举节点 bp3...\033[0m"
clgst system voteproducer prods voter3 bp3

echo -e "\033[32m voter4 投票选举节点 bp4...\033[0m"
clgst system voteproducer prods voter4 bp4
 
echo -e "\033[31m 投票完成. \033[0m"
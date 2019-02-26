#!/bin/bash

echo "PW5Ka7m2J2wLAhA3wbJPQ9XhFMc5KVLVE2qYTETr6cEvwqyrTGJAw" | clgst wallet unlock

cd  /work/gst_install/gst

clgst set contract gstio build/contracts/gstio.bios -p gstio


clgst create account gstio test1 GST6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV 

 clgst create account gstio test2 GST6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

 clgst create account gstio test3 GST6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
 
clgst push action gstio setprods '{"schedule":[{"producer_name":"test1","block_signing_key":"GST6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"}]}' -p gstio

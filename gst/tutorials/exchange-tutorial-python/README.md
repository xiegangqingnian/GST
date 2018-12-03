The following steps must be taken for the example script to work.

0. Create wallet
0. Create account for gstio.token
0. Create account for scott
0. Create account for exchange
0. Set token contract on gstio.token
0. Create GST token
0. Issue initial tokens to scott

**Note**:
Deleting the `transactions.txt` file will prevent replay from working.


### Create wallet
`clgst wallet create`

### Create account steps
`clgst create key`

`clgst create key`

`clgst wallet import  --private-key <private key from step 1>`

`clgst wallet import  --private-key <private key from step 2>`

`clgst create account gstio <account_name> <public key from step 1> <public key from step 2>`

### Set contract steps
`clgst set contract gstio.token /contracts/gstio.token -p gstio.token@active`

### Create GST token steps
`clgst push action gstio.token create '{"issuer": "gstio.token", "maximum_supply": "100000.0000 GST", "can_freeze": 1, "can_recall": 1, "can_whitelist": 1}' -p gstio.token@active`

### Issue token steps
`clgst push action gstio.token issue '{"to": "scott", "quantity": "900.0000 GST", "memo": "testing"}' -p gstio.token@active`

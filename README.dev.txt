I (Lukas) use RPC wallet and daemon most of the time and now as we test aliases and I am finishing the development, we have a lot of sync problems. 

There is some help: 
Before running daemon, remove folder ./citicash/lmdb usually containing at least data.mdb and lock.mdb, so the whole blockchain is loaded correctly from scratch on next run. 

After running RPC wallet, call rescan_blockchain so the wallet data are most up to date. 
curl -X POST <address>:<port>/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"rescan_blockchain" -H 'Content-Type: application/json'
Quit the RPC wallet with stop_wallet so the data are saved correctly. 
curl -X POST <address>:<port>/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"stop_wallet"}' -H 'Content-Type: application/json'

I hope I soon finally finish saving the alias data into lmdb database and those problems will fade away and I remove this file. 
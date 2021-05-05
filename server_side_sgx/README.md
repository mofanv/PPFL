# Secure aggregation using SGX

This is an application providing secure aggregation, on a server orchestrating federated learning, using SGX based on [OpenEnclave SDK](https://github.com/openenclave/openenclave).

### More info:
1) The data transmission between server and clients is using `scp`. 
2) We simplify the communication and agreement between server and client by `ssh` from the server to the client directly. 
3) Weights of layers being run in TEEs are encrypted/decrypted by hand-coded symmetric key (AES-CBC (128bit)) in both server SGX and client TrustZone. 

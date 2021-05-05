This is PPFL client side, which runs several layers of a Deep Neural Network (DNN) model in TrustZone.

This application is atop of our previous framework [DarkneTZ](https://github.com/mofanv/darknetz), a work published at MobiSys20. [See here for more details.](https://arxiv.org/abs/2004.05703)


# Prerequisites
**Required System**: Ubuntu-based distributions

**TrustZone-enabled device is needed**. Raspberry Pi 3, HiKey Board, ARM Juno board, etc. Check this [List](https://optee.readthedocs.io/en/latest/building/devices/index.html#device-specific) for more info.

# Setup
## (1) Set up OP-TEE
1) Follow **step1** ~ **step5** in "**Get and build the solution**" to build the OP-TEE solution.
https://optee.readthedocs.io/en/latest/building/gits/build.html#get-and-build-the-solution

2) Keep follow **step6** ~ **step7** in the above link to flash the devices. Note that this step is device-specific.


3) Follow **step8** ~ **step9** to test whether OP-TEE works or not. Run:
```
tee-supplicant -d
xtest
```

Note: you may face OP-TEE related problem/errors during setup. If this happens, you may want to raise issues under [their project pages](https://github.com/OP-TEE/optee_os).

## (2) Build DarkneTZ
1) clone codes and datasets
```
git clone https://github.com/mofanv/darknetz.git
git clone https://github.com/mofanv/tz_datasets.git
```
Let `$PATH_OPTEE$` be the path of OPTEE, `$PATH_darknetz$` be the path of darknetz, and `$PATH_tz_datasets$` be the path of tz_datasets. Note that this `darknetz` is form the source. You can also clone it from this client-side code.

2) copy DarkneTZ to example dir
```
mkdir $PATH_OPTEE$/optee_examples/darknetz
cp -a $PATH_darknetz$/. $PATH_OPTEE$/optee_examples/darknetz/
```

3) copy datasets to root dir
```
cp -a $PATH_tz_datasets$/. $PATH_OPTEE$/out-br/target/root/
```

4) rebuild the OP-TEE

to run `make flash` to flash the OP-TEE with `darknetz` to your device.

5) after you boot your devices, test by the command 
```
darknetp
```
Note: It is NOT `darknetz` here for the command.

You should get the output:
 ```
# usage: ./darknetp <function>
 ```
Great! You have set up the client side successfully.

# More Tests on Training Models

1) To train a model from scratch 
```
darknetp classifier train -pp_start 4 -pp_end 10 cfg/mnist.dataset cfg/mnist_lenet.cfg
```
You can choose the partition point of layers in the TEE by adjusting the argument `-pp_start` and `-pp_end`. Any sequence layers (first, middle, or last layers) can be put inside the TEE.

Note: you may suffer from insufficient secure memory problems when you run this command. The preconfigured secure memory of darknetz is `TA_STACK_SIZE = 1*1024*1024` and `TA_DATA_SIZE = 10*1024*1024` in `ta/user_ta_header_defines.h` file. However, the maximum secure memory size can differ from different devices (typical size is 16 MiB) and maybe not enough. When this happens, a easy way is to configure the needed secure memory to be smaller.

When everything is ready, you will see output from the Normal World like this:
```
# Prepare session with the TA
# Begin darknet
# mnist_lenet
# 1
layer     filters    size              input                output
    0 conv      6  5 x 5 / 1    28 x  28 x   3   ->    28 x  28 x   6  0.001 BFLOPs
    1 max          2 x 2 / 2    28 x  28 x   6   ->    14 x  14 x   6
    2 conv      6  5 x 5 / 1    14 x  14 x   6   ->    14 x  14 x   6  0.000 BFLOPs
    3 max          2 x 2 / 2    14 x  14 x   6   ->     7 x   7 x   6
    4 connected_TA                          294  ->   120
    5 dropout_TA    p = 0.80                120  ->   120
    6 connected_TA                          120  ->    84
    7 dropout_TA    p = 0.80                 84  ->    84
    8 connected_TA                           84  ->    10
    9 softmax_TA                                       10
   10 cost_TA                                          10
# Learning Rate: 0.01, Momentum: 0.9, Decay: 5e-05
# 1000
# 28 28
# Loaded: 0.197170 seconds
# 1, 0.050: 0.000000, 0.000000 avg, 0.009999 rate, 3.669898 seconds, 50 images
# Loaded: 0.000447 seconds
# 2, 0.100: 0.000000, 0.000000 avg, 0.009998 rate, 3.651714 seconds, 100 images
...
```

Layers with `_TA` are running in the TrustZone. When the last layer is inside the TEE, you will not see the loss from Normal World. The training loss is calculated based on outputs of the model which belong to the last layer in the TrustZone, so it can only be seen from the Secure World. That is, the output from the Secure World is like this:
```
# I/TA:  loss = 1.62141, avg loss = 1.62540 from the TA
# I/TA:  loss = 1.58659, avg loss = 1.61783 from the TA
# I/TA:  loss = 1.57328, avg loss = 1.59886 from the TA
# I/TA:  loss = 1.52641, avg loss = 1.57889 from the TA
...
```


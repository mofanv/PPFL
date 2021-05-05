#include "darknet.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

// openenclave library
#include <openenclave/host.h>
#include "../secure_aggregation_u.h"
#include "parser.h"

int debug=1;

// This is the function that the enclave will call back into to
// print a message.
void host_secure_aggregation()
{
    fprintf(stdout, "Enclave called into host to print: Secure Aggregation!\n");
}


void os_secure_aggregation(
    bool last_one,
    size_t num_p,
    float* input_buf,
    float* output_buf,
    size_t buf_size)
{
   // add input_buf to aggregated buf
   for(size_t i=0; i<buf_size; i++)
   {
       output_buf[i] += input_buf[i];
   }

   // the last one then averaging
   if(last_one)
   {
       for(size_t i=0; i<buf_size; i++){
           output_buf[i] = output_buf[i] / num_p;
       }
   }
}


void fed_averaging_ree(int pp1, int pp2, network **nets, size_t num_p)
{
    size_t size_bias;
    size_t size_weight;

    for(int i = 0; i < nets[0]->n; ++i){

        if(i <= pp1 || i > pp2){
            layer l = nets[0]->layers[i];

            // layer type
            if(l.type == CONVOLUTIONAL){
                size_bias = l.n;
                size_weight = l.nweights;

            }else if(l.type == CONNECTED){
                size_bias = l.outputs;
                size_weight = l.outputs*l.inputs;
            }else{
                continue;
            }

            printf("Aggregating layer %d ... in OS:", i);

            for (size_t j=1; j<num_p; j++){
                os_secure_aggregation(j==num_p-1, num_p, nets[j]->layers[i].biases, l.biases, size_bias);
            }
            printf(" biases finished...");

            if (debug==1){
                printf("\nnet 0: ");
                for(size_t z=0; z<5; z++)
                {
                    printf("%f, ", l.weights[z]);
                }
            }

            for (size_t j=1; j<num_p; j++){
                os_secure_aggregation(j==num_p-1, num_p, nets[j]->layers[i].weights, l.weights, size_weight);

                if (debug==1){
                    printf("\nnet %zu: ", j);
                    for(size_t z=0; z<5; z++)
                    {
                        printf("%f, ", nets[j]->layers[i].weights[z]);
                    }
                }
            }

            if (debug==1){
                printf("\nnet a: ");
                for(size_t z=0; z<5; z++)
                {
                    printf("%f, ", l.weights[z]);
                }
            }
            printf(" weights finished\n");
        }
    }
}


void fed_averaging_tee(int pp1, int pp2, network **nets, size_t num_p)
{
    // define enclave
    oe_result_t result;
    int ret = 1;
    oe_enclave_t* enclave = NULL;
    uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;

    // Create the enclave
    char* hc_argv="./enclave/secure_aggregation_enc.signed";
    result = oe_create_secure_aggregation_enclave(
        hc_argv, OE_ENCLAVE_TYPE_AUTO, flags, NULL, 0, &enclave); //argv[1]
    if (result != OE_OK)
    {
        fprintf(
            stderr,
            "oe_create_secure_aggregation_enclave(): result=%u (%s)\n",
            result,
            oe_result_str(result));
        exit(0);
    }
    size_t size_bias;
    size_t size_weight;
    for(int i = 0; i < nets[0]->n; ++i){
        if(pp1 < i && i <= pp2){
            layer l = nets[0]->layers[i];

            // layer type
            if(l.type == CONVOLUTIONAL){
                size_bias = l.n;
                size_weight = l.nweights;

            }else if(l.type == CONNECTED){
                size_bias = l.outputs;
                size_weight = l.outputs*l.inputs;
            }else{
                continue;
            }

            printf("Aggregating layer %d ... inside SGX:", i);
            // biases, size=l.n
            enclave_initialize(enclave, l.biases, size_bias);

            for (size_t j=1; j<num_p; j++){

                result = enclave_secure_aggregation(enclave, j==num_p-1, nets[j]->layers[i].biases, l.biases, size_bias);
            }

            printf(" biases finished...");
            // weights, size=l.nweights
            enclave_initialize(enclave, l.weights, size_weight);

            if (debug==1){
                printf("\nnet 0: ");
                for(size_t z=0; z<5; z++)
                {
                    printf("%f, ", l.weights[z]);
                }
            }

            for (size_t j=1; j<num_p; j++){
                result = enclave_secure_aggregation(enclave, j==num_p-1, nets[j]->layers[i].weights, l.weights, size_weight);

                if (debug==1){
                    printf("\nnet %zu: ", j);
                    for(size_t z=0; z<5; z++)
                    {
                        printf("%f, ", nets[j]->layers[i].weights[z]);
                    }
                }
            }

            if (debug==1){
                printf("\nnet a: ");
                for(size_t z=0; z<5; z++)
                {
                    printf("%f, ", l.weights[z]);
                }
            }
            printf(" weights finished\n");
        }
    }
}


void model_aggregation(int pp1, int pp2, int frozen_bool, char *cfgfile, char *weightfolder)
{

    struct dirent *de;  // Pointer for directory entry
    char *base = basecfg(cfgfile);

    // opendir() returns a pointer of DIR type.
    DIR *dr_c = opendir(weightfolder);

    // count number of files
    size_t count_files = 0;
    while ((de = readdir(dr_c)) != NULL){
        count_files++;
    }
    count_files = count_files-2;
    if(count_files == 1){
        printf("Number of participants is 1, no need of FL\n");
        exit(0);
    }else{
        printf("Number of participants: %zu\n", count_files);
    }


    // load weights to seperate network
    network **nets = calloc(count_files, sizeof(network*));
    char file_buf[256];
    int indx=0;

    DIR *dr = opendir(weightfolder);
    while ((de = readdir(dr)) != NULL){
        if (!strcmp (de->d_name, "."))
            continue;
        if (!strcmp (de->d_name, ".."))
            continue;

        sprintf(file_buf, "%s%s", weightfolder, de->d_name);
        nets[indx] = load_network(cfgfile, file_buf, 0);
        printf("load weights of p-%i: %s\n", indx+1, file_buf);

        indx++;

    }
    closedir(dr);


    // average on each layer
    if(frozen_bool==0){
        fed_averaging_ree(pp1, pp2, nets, count_files);
    }
    fed_averaging_tee(pp1, pp2, nets, count_files);

    // save averaged network (in nets[0])
    char buff[256];
    sprintf(buff, "%s%s_averaged.weights", weightfolder, base);
    save_weights(nets[0], buff);

    //free_network(net);
    for(int i=0; i<count_files; i++){
        free_network(nets[i]);
    }
}

void weight_disclose(int pp1, int pp2, network *net)
{
    // define enclave
    oe_result_t result;
    int ret = 1;
    oe_enclave_t* enclave = NULL;
    uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;

    // Create the enclave
    char* hc_argv="./enclave/secure_aggregation_enc.signed";
    result = oe_create_secure_aggregation_enclave(
        hc_argv, OE_ENCLAVE_TYPE_AUTO, flags, NULL, 0, &enclave); //argv[1]
    if (result != OE_OK)
    {
        fprintf(
            stderr,
            "oe_create_weight_disclose_enclave(): result=%u (%s)\n",
            result,
            oe_result_str(result));
        exit(0);
    }
    size_t size_bias;
    size_t size_weight;
    for(int i = 0; i < net->n; ++i){
        if(pp1 < i && i <= pp2){
            layer l = net->layers[i];

            // layer type
            if(l.type == CONVOLUTIONAL){
                size_bias = l.n;
                size_weight = l.nweights;

            }else if(l.type == CONNECTED){
                size_bias = l.outputs;
                size_weight = l.outputs*l.inputs;
            }else{
                continue;
            }

            printf("Decrypting layer %d ... inside SGX:", i);

            enclave_weights_decryption(enclave, l.biases, size_bias);
            enclave_weights_decryption(enclave, l.weights, size_weight);

            printf("  finished\n");
        }
    }
}


void weight_close(int pp1, int pp2, network *net)
{
    // define enclave
    oe_result_t result;
    int ret = 1;
    oe_enclave_t* enclave = NULL;
    uint32_t flags = OE_ENCLAVE_FLAG_DEBUG;

    // Create the enclave
    char* hc_argv="./enclave/secure_aggregation_enc.signed";
    result = oe_create_secure_aggregation_enclave(
        hc_argv, OE_ENCLAVE_TYPE_AUTO, flags, NULL, 0, &enclave); //argv[1]
    if (result != OE_OK)
    {
        fprintf(
            stderr,
            "oe_create_weight_disclose_enclave(): result=%u (%s)\n",
            result,
            oe_result_str(result));
        exit(0);
    }
    size_t size_bias;
    size_t size_weight;
    for(int i = 0; i < net->n; ++i){
        if(pp1 < i && i <= pp2){
            layer l = net->layers[i];

            // layer type
            if(l.type == CONVOLUTIONAL){
                size_bias = l.n;
                size_weight = l.nweights;

            }else if(l.type == CONNECTED){
                size_bias = l.outputs;
                size_weight = l.outputs*l.inputs;
            }else{
                continue;
            }

            printf("Encrypting layer %d ... inside SGX:", i);

            enclave_weights_encryption(enclave, l.biases, size_bias);
            enclave_weights_encryption(enclave, l.weights, size_weight);

            printf("  finished\n");
        }
    }
}

void layer_finalization(int pp1, int pp2, int pp3, int pp4, int index_layer, char *cfgfile1, char *weightfile1, char *cfgfile2, char *weightfolder2)
{

    char *base = basecfg(cfgfile1);

    // load weights to seperate network
    network *net;
    int indx=0;

    frozen_bool = 1; // only load tee weights
    sepa_save_bool = 1;

    net = load_network(cfgfile1, weightfile1, 0);
    printf("load weights of p-%i: %s\n", indx+1, weightfile1);

    // disclose the previous layer's weights from tee (decryption)
    weight_disclose(pp1, pp2, net);

    // save network
    char buff1[256];
    sprintf(buff1, "%s%s_trained.weights_layer%d", weightfolder2, base, index_layer);
    save_weights(net, buff1);
    free_network(net);

    partition_point1 = 999;
    partition_point1 = 999;
    sepa_save_bool = 0;

    // load all trained layers weights
    char file_buf[256];
    sprintf(file_buf, "%s%s_trained.weights_layer", weightfolder2, base);

    network *net_con;
    net_con = load_network_layerwise(cfgfile2, file_buf, index_layer);
    if (strcmp(cfgfile1, cfgfile2) != 0){ //0 is equal
        weight_close(pp3, pp4, net_con);
    }

    // save averaged network (in nets[0])
    char buff2[256];
    sprintf(buff2, "%s%s_trained.weights", weightfolder2, base);

    save_weights(net_con, buff2);
    free_network(net_con);
}


void server(int argc, char **argv)
{
    if(argc < 4){
        fprintf(stderr, "usage: %s %s [train/test/valid] [cfg] [weights (optional)]\n", argv[0], argv[1]);
        return;
    }

    // partition point of DNN
    int pp_start = find_int_arg(argc, argv, "-pp_start", 999);

    if(pp_start == 999){ // when using pp_start_f for forzen first layers outside TEE
        pp_start = find_int_arg(argc, argv, "-pp_start_f", 999);
        frozen_bool = 1;
    }

    int pp_end = find_int_arg(argc, argv, "-pp_end", 999);

    int pp1 = pp_start - 1;
    int pp2 = pp_end;

    partition_point1 = pp1;
    partition_point2 = pp2;

    sepa_save_bool = find_int_arg(argc, argv, "-ss", 0);
    // 0 no Separate
    // 1 separate load and save
    // 2 separate load but not save

    int index_layer = find_int_arg(argc, argv, "-idx", 1);

    int pp_start2 = find_int_arg(argc, argv, "-pp_start2", 999);
    int pp_end2 = find_int_arg(argc, argv, "-pp_end2", 999);

    int pp3 = pp_start2 - 1;
    int pp4 = pp_end2;

    int clear = find_arg(argc, argv, "-clear");

    char *cfg = argv[3];
    char *weights_folder = argv[4];

    char *cfg2 = argv[5];
    char *weights_folder2 = argv[6];

    if(0==strcmp(argv[2], "model_aggregation")) model_aggregation(pp1, pp2, frozen_bool, cfg, weights_folder);
    if(0==strcmp(argv[2], "layer_finalization")) layer_finalization(pp1, pp2, pp3, pp4, index_layer, cfg, weights_folder, cfg2, weights_folder2);
}

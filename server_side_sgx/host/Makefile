# Copyright (c) Open Enclave SDK contributors.
# Licensed under the MIT License.

include ../../config.mk

CFLAGS=$(shell pkg-config oehost-$(COMPILER) --cflags)
LDFLAGS=$(shell pkg-config oehost-$(COMPILER) --libs)
INCDIR=$(shell pkg-config oehost-$(COMPILER) --variable=includedir)

COMMON= -Iinclude/ -Isrc/
MPFLAGS= -lm -pthread
OBJSCR = src/
EXECSCR = examples/

OBJ=gemm.o utils.o cuda.o deconvolutional_layer.o convolutional_layer.o list.o image.o activations.o im2col.o col2im.o blas.o crop_layer.o dropout_layer.o maxpool_layer.o softmax_layer.o data.o matrix.o network.o connected_layer.o cost_layer.o parser.o option_list.o detection_layer.o route_layer.o upsample_layer.o box.o normalization_layer.o avgpool_layer.o layer.o local_layer.o shortcut_layer.o logistic_layer.o activation_layer.o rnn_layer.o gru_layer.o crnn_layer.o demo.o batchnorm_layer.o region_layer.o reorg_layer.o tree.o  lstm_layer.o l2norm_layer.o yolo_layer.o iseg_layer.o

CFILE=gemm.c utils.c cuda.c deconvolutional_layer.c convolutional_layer.c list.c image.c activations.c im2col.c col2im.c blas.c crop_layer.c dropout_layer.c maxpool_layer.c softmax_layer.c data.c matrix.c network.c connected_layer.c cost_layer.c parser.c option_list.c detection_layer.c route_layer.c upsample_layer.c box.c normalization_layer.c avgpool_layer.c layer.c local_layer.c shortcut_layer.c logistic_layer.c activation_layer.c rnn_layer.c gru_layer.c crnn_layer.c demo.c batchnorm_layer.c region_layer.c reorg_layer.c tree.c  lstm_layer.c l2norm_layer.c yolo_layer.c iseg_layer.c

EXECOBJA=captcha.o lsd.o super.o art.o tag.o cifar.o go.o rnn.o segmenter.o regressor.o classifier.o server.o coco.o yolo.o detector.o nightmare.o instance-segmenter.o darknet.o

EXECFILE=captcha.c lsd.c super.c art.c tag.c cifar.c go.c rnn.c segmenter.c regressor.c classifier.c server.c coco.c yolo.c detector.c nightmare.c instance-segmenter.c darknet.c

CFILES = $(addprefix $(OBJSCR), $(CFILE))
EXECFILES = $(addprefix $(EXECSCR), $(EXECFILE))

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	oeedger8r ../secure_aggregation.edl --untrusted \
		--search-path $(INCDIR) \
		--search-path $(INCDIR)/openenclave/edl/sgx

	$(CC) -g -c $(COMMON) $(CFLAGS) $(CFILES)
	$(CC) -g -c $(COMMON) $(CFLAGS) $(EXECFILES)
	$(CC) -g -c $(CFLAGS) secure_aggregation_u.c
	$(CC) -o secure_aggregation_host secure_aggregation_u.o $(OBJ) $(EXECOBJA) $(MPFLAGS) $(LDFLAGS) 

clean:
	rm -f secure_aggregation_host secure_aggregation_u.o secure_aggregation_u.c secure_aggregation_u.h secure_aggregation_args.h $(OBJ) $(EXECOBJA)

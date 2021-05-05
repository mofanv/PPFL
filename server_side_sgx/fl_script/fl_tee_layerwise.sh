#!/bin/bash
set -e
. /opt/openenclave/share/openenclave/openenclaverc

IP_CLIENT_1=192.168.0.22
PASSWORD_CLIENT_1=fl-tee-system

NUM_ROUNDS=2
NUM_CLIENTS=2

PP_START_LIST=(0 1 2 4)
PP_END_LIST=(4 5 6 7)
NUM_LAYER=4

DATASET=mnist
MODEL=greedy-cnn

SERVER_APP_DIR="/media/vincent/program/my_oesamples/secure-aggregation-SGX/"
SERVER_AVERAGED_DIR="./results/${DATASET}/averaged_layerwise/"
SERVER_UPDATES_DIR="./results/${DATASET}/client_updates_layerwise/"
SERVER_TRAINED_DIR="./results/${DATASET}/trained_layerwise/"

CLIENT_MODEL_DIR="/root/models/${DATASET}/"
DM="${DATASET}_${MODEL}"

STARTER="./results/${DATASET}/${DM}.weights"
# sshpass -p fl-tee-system ssh root@192.168.0.22 darknetp classifier train -pp_start 0 -pp_end 0 cfg/mnist.dataset cfg/greedy-cnn-layer1.cfg
# sshpass -p fl-tee-system scp root@192.168.0.22:/root/models/mnist/greedy-cnn-layer1.weights /media/vincent/program/my_oesamples/secure-aggregation-SGX/results/mnist/mnist_greedy-cnn.weights


echo "============= initialization ============="
cd ${SERVER_APP_DIR}

rm -rf ${SERVER_TRAINED_DIR}
mkdir ${SERVER_TRAINED_DIR}

for((l=1;l<=${NUM_LAYER};l++))
do
	echo "============= layer ${l} ============="
	rm -rf ${SERVER_UPDATES_DIR}
	mkdir ${SERVER_UPDATES_DIR}

	rm -rf ${SERVER_AVERAGED_DIR}
	mkdir ${SERVER_AVERAGED_DIR}

	if [ ${l} == 1 ]
	then
		cp ${STARTER} "${SERVER_AVERAGED_DIR}${DM}_averaged_r0.weights"
	else
		cp "${SERVER_TRAINED_DIR}${DM}_trained.weights" "${SERVER_AVERAGED_DIR}${DM}_averaged_r0.weights"
	fi

	lp=$((l-1))
	PP_START=${PP_START_LIST[${lp}]}
	PP_END=${PP_END_LIST[${lp}]}
	PP_START2=${PP_START_LIST[${l}]}
	PP_END2=${PP_END_LIST[${l}]}

	for ((r=1;r<=${NUM_ROUNDS};r++))
	do
		echo "============= round ${r} ============="
		for ((c=1;c<=${NUM_CLIENTS};c++))
		do
			echo "============= copy weights server -> client ${c} ============="
			rp=$((r-1))
			time sshpass -p ${PASSWORD_CLIENT_1} scp "${SERVER_AVERAGED_DIR}${DM}_averaged_r${rp}.weights" "root@${IP_CLIENT_1}:${CLIENT_MODEL_DIR}${DM}_global.weights"
			filesize=$(stat --format=%s "${SERVER_AVERAGED_DIR}${DM}_averaged_r${rp}.weights")
			echo "tee weights: ${filesize} Bytes"
			sleep 3s

			echo "============= ssh to the client and local training ============="
			# select net cfg for layer
			if [ ${l} == ${NUM_LAYER} ]
			then
				sshpass -p ${PASSWORD_CLIENT_1} ssh "root@${IP_CLIENT_1}" cp "cfg/${MODEL}-aux.cfg" "cfg/${DM}.cfg"
			else
				sshpass -p ${PASSWORD_CLIENT_1} ssh "root@${IP_CLIENT_1}" cp "cfg/${MODEL}-layer${l}.cfg" "cfg/${DM}.cfg"
			fi
			# training with TEEs (for ss, only support tee)
			time sshpass -p ${PASSWORD_CLIENT_1} ssh "root@${IP_CLIENT_1}" darknetp classifier train -pp_start_f ${PP_START} -pp_end ${PP_END} -ss 2 "cfg/${DATASET}.dataset" "cfg/${DM}.cfg" "${CLIENT_MODEL_DIR}${DM}_global.weights"
			sleep 3s

			echo "============= copy weights client ${c} -> server ============="
			rm -rf "${SERVER_UPDATES_DIR}${DM}_c${c}.weights"
			mkdir "${SERVER_UPDATES_DIR}${DM}_c${c}.weights"

			time sshpass -p ${PASSWORD_CLIENT_1} scp "root@${IP_CLIENT_1}:${CLIENT_MODEL_DIR}${DM}.weights_tee" "${SERVER_UPDATES_DIR}${DM}_c${c}.weights/_tee"
			filesize=$(stat --format=%s "${SERVER_UPDATES_DIR}${DM}_c${c}.weights/_tee")
			echo "tee weights: ${filesize} Bytes"
			sleep 3s
		done

		echo "============= averaging ============="
		# select net cfg for layer
		if [ ${l} == ${NUM_LAYER} ]
		then
			cp "cfg/${MODEL}-aux.cfg" "cfg/${DM}.cfg"
		else
			cp "cfg/${MODEL}-layer${l}.cfg" "cfg/${DM}.cfg"
		fi

		# for the last round, finishing layer and save as trained
		if [ ${r} == ${NUM_ROUNDS} ]
		then
			time host/secure_aggregation_host server model_aggregation -pp_start_f ${PP_START} -pp_end ${PP_END} -ss 1 "cfg/${DM}.cfg" ${SERVER_UPDATES_DIR}
			mkdir "${SERVER_TRAINED_DIR}${MODEL}_layer${l}.weights"
			mv "${SERVER_UPDATES_DIR}${DM}_averaged.weights_tee" "${SERVER_TRAINED_DIR}${MODEL}_layer${l}.weights/_tee"
		else
			time host/secure_aggregation_host server model_aggregation -pp_start_f ${PP_START} -pp_end ${PP_END} -ss 2 "cfg/${DM}.cfg" ${SERVER_UPDATES_DIR}
			mv "${SERVER_UPDATES_DIR}${DM}_averaged.weights" "${SERVER_AVERAGED_DIR}${DM}_averaged_r${r}.weights"
		fi
	done


	# next cfg
	ln=$((l+1))
	echo "============= build model for training next layer ${ln} (if existed) ============="
	if [ ${ln} -gt ${NUM_LAYER} ]
	then
		NEXT_CFG="cfg/${DM}.cfg"
	elif [ ${ln} == ${NUM_LAYER} ]
	then
		NEXT_CFG="cfg/${MODEL}-aux.cfg"
	else
		NEXT_CFG="cfg/${MODEL}-layer${ln}.cfg"
	fi

	time host/secure_aggregation_host server layer_finalization -pp_start_f ${PP_START} -pp_end ${PP_END} -pp_start2 ${PP_START2} -pp_end2 ${PP_END2} -idx ${l} "cfg/${DM}.cfg" "${SERVER_TRAINED_DIR}${MODEL}_layer${l}.weights" ${NEXT_CFG} "${SERVER_TRAINED_DIR}"

done

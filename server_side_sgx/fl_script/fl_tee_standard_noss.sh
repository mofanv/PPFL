#!/bin/bash
set -e
. /opt/openenclave/share/openenclave/openenclaverc

IP_CLIENT_1=192.168.0.22
PASSWORD_CLIENT_1=fl-tee-system

NUM_ROUNDS=2
NUM_CLIENTS=2

PP_START=6
PP_END=8

DATASET=mnist
MODEL=lenet

SERVER_APP_DIR="/media/vincent/program/my_oesamples/secure-aggregation-SGX/"
SERVER_AVERAGED_DIR="./results/${DATASET}/averaged_standard_noss/"
SERVER_UPDATES_DIR="./results/${DATASET}/client_updates_standard_noss/"
CLIENT_MODEL_DIR="/root/models/${DATASET}/"
DM="${DATASET}_${MODEL}"

STARTER="./results/${DATASET}/${DM}_pp${PP_START}${PP_END}.weights"


echo "============= initialization ============="
cd ${SERVER_APP_DIR}

rm -rf ${SERVER_UPDATES_DIR}
mkdir ${SERVER_UPDATES_DIR}

rm -rf ${SERVER_AVERAGED_DIR}
mkdir ${SERVER_AVERAGED_DIR}

cp ${STARTER} "${SERVER_AVERAGED_DIR}${DM}_averaged_r0.weights"

for ((r=1;r<=NUM_ROUNDS;r++))
do
	echo "============= round ${r} ============="
	for ((c=1;c<=NUM_CLIENTS;c++))
	do
		echo "============= copy weights server -> client ${c} ============="
		rp=$((r-1))
		time sshpass -p ${PASSWORD_CLIENT_1} scp "${SERVER_AVERAGED_DIR}${DM}_averaged_r${rp}.weights" "root@${IP_CLIENT_1}:${CLIENT_MODEL_DIR}${DM}_global.weights"
		filesize=$(stat --format=%s "${SERVER_AVERAGED_DIR}${DM}_averaged_r${rp}.weights")
		echo "${filesize} Bytes"
		sleep 3s

		echo "============= ssh to the client and local training ============="
		if [ ${PP_START} == 999 ]
		then
			# training without TEEs
			time sshpass -p ${PASSWORD_CLIENT_1} ssh "root@${IP_CLIENT_1}" darknetp classifier train "cfg/${DATASET}.dataset" "cfg/${DM}.cfg" "${CLIENT_MODEL_DIR}${DM}_global.weights"
		else
			# training with TEEs
			time sshpass -p ${PASSWORD_CLIENT_1} ssh "root@${IP_CLIENT_1}" darknetp classifier train -pp_start ${PP_START} -pp_end ${PP_END} "cfg/${DATASET}.dataset" "cfg/${DM}.cfg" "${CLIENT_MODEL_DIR}${DM}_global.weights"
		fi
		sleep 3s

		echo "============= copy weights client ${c} -> server ============="
		time sshpass -p ${PASSWORD_CLIENT_1} scp "root@${IP_CLIENT_1}:${CLIENT_MODEL_DIR}${DM}.weights" "${SERVER_UPDATES_DIR}${DM}_c${c}.weights"
		filesize=$(stat --format=%s "${SERVER_UPDATES_DIR}${DM}_c${c}.weights")
		echo "${filesize} Bytes"
		sleep 3s
	done

	echo "============= averaging ============="
	time host/secure_aggregation_host server model_aggregation -pp_start ${PP_START} -pp_end ${PP_END} "cfg/${DM}.cfg" ${SERVER_UPDATES_DIR}
	mv "${SERVER_UPDATES_DIR}${DM}_averaged.weights" "${SERVER_AVERAGED_DIR}${DM}_averaged_r${r}.weights"
done

#!/bin/bash

function is_au() {
	if [[ "${AUTO_UPDATE}" != "1" ]]; then
		echo "Did you build the OTA with -au?"
		read -p "(y/n): " yn
		case $yn in
			[Yy]* ) echo "Cool." ;;
			[Nn]* ) echo; echo "bro"; exit 1;;
			* ) echo "that is not a y or an n."; exit 1;;
		esac
	fi
}

function usage() {
	echo "./build/upload.sh <server_user> <server_ip> <increment> <ssh_key_path (optional)> <ssh_port (optional)>"
}

trap ctrl_c INT

function ctrl_c() {
    exit 1
}

SERVER_USER="${1}"
SERVER_IP="${2}"
INCREMENT="${3}"
SSH_KEY_PATH="${4}"
SSH_PORT="${5}"


if [[ "${SERVER_IP}" == "" ]]; then
	echo "server IP is null"
	usage
	exit 1
fi

if [[ "${INCREMENT}" == "" ]]; then
	echo "increment is null"
	usage
	exit 1
fi

if [[ "${SERVER_USER}" == "" ]]; then
	echo "server user is null"
	usage
	exit 1
fi

SSH_ARGUMENT_LIST=""

if [[ "${SSH_KEY_PATH}" != "" ]]; then
	if [[ -f "${SSH_KEY_PATH}" ]]; then
		SSH_ARGUMENT_LIST="-i ${SSH_KEY_PATH}"
	else
		echo "${SSH_KEY_PATH} does not exist"
		exit 1
	fi
fi

SCP_ARGUMENT_LIST=""

if [[ "${SSH_PORT}" != "" ]]; then
	SCP_ARGUMENT_LIST="${SSH_ARGUMENT_LIST} -P ${SSH_PORT}"
	SSH_ARGUMENT_LIST="${SSH_ARGUMENT_LIST} -p ${SSH_PORT}"
fi

if [[ ! -f "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}d.ota" ]]; then
	echo "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}d.ota" does not exist.
	exit 1
fi

if [[ ! -f "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}oskr.ota" ]]; then
        echo "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}oskr.ota" does not exist.
        exit 1
fi

if [[ $(ssh ${SSH_ARGUMENT_LIST} -o PasswordAuthentication=no $SERVER_USER@$SERVER_IP "uname -a") != *"Linux"* ]]; then
	echo "Something is wrong with the SSH communication."
	exit 1
fi

function sshServ() {
	ssh ${SSH_ARGUMENT_LIST} -o PasswordAuthentication=no ${SERVER_USER}@${SERVER_IP} "$@"
}

is_au
sshServ mkdir -p /wire/otas/full
sshServ touch /wire/otas/dnar
scp ${SCP_ARGUMENT_LIST} "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}d.ota" "${SERVER_USER}@${SERVER_IP}:/wire/otas/full/dev/$(cat ANKI_VERSION).${INCREMENT}.ota"
scp ${SCP_ARGUMENT_LIST} "_build/vicos-$(cat ANKI_VERSION).${INCREMENT}oskr.ota" "${SERVER_USER}@${SERVER_IP}:/wire/otas/full/oskr/$(cat ANKI_VERSION).${INCREMENT}.ota"
sshServ "echo $(cat ANKI_VERSION).${INCREMENT} > /wire/otas/latest"
sshServ rm /wire/otas/dnar

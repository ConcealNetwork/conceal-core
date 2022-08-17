#!/bin/bash
SYSTEM_RAMSIZE="$(( $(free | awk 'FNR == 2 {print $2}') / 1000000 ))"
SWAPSIZE="$(( (13 - $SYSTEM_RAMSIZE) * 1024 ))"

[ -f "/etc/apt/sources.list.d/raspi.list" ] && isRPI=1 || isRPI=0
[ "$EUID" != "0" ] && printf "Please run this as root: \x1b[1;95msudo $0\x1b[0m\n" && exit 1

[ "$SYSTEM_RAMSIZE" -gt 13 ] && \
	printf "A swap file isn't required as there is enough memory is installed\n" && exit

[ -f "/swapfile" ] && printf "\x1b[1;7;91m WARNING \x1b[0m /swapfile already exists - " && \
	printf "overwrite? [Y/N] " && read overwrite

case $overwrite in
	"y"|"Y") printf "Swapping-off /swapfile\n" && swapoff /swapfile && rm /swapfile;;
	"n"|"N") printf "Couldn't create swapfile\n" && exit 1;;
esac

case $isRPI in
	1)
		dphys-swapfile swapoff
		sed -i "s/CONF_SWAPSIZE=.*/CONF_SWAPSIZE=$SWAPSIZE/" /etc/dphys-swapfile
		dphys-swapfile setup
		dphys-swapfile swapon
		;;
	0)
		[ -f "/swapfile" ] && printf "\x1b[1;7;91m WARNING \x1b[0m /swapfile already exists - " && \
			printf "overwrite? [Y/N] " && read overwrite

		case $overwrite in
			"y"|"Y") printf "Swapping-off /swapfile\n" && swapoff /swapfile && rm /swapfile;;
			"n"|"N") printf "Couldn't create swapfile\n" && exit 1;;
		esac

		fallocate -l ${SWAPSIZE}M /swapfile; chmod 0600 /swapfile; mkswap /swapfile; swapon /swapfile; exit 0;;
esac

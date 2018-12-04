	OS_VER=$( grep VERSION_ID /etc/os-release | cut -d'=' -f2 | sed 's/[^0-9\.]//gI' \
	| cut -d'.' -f1 )

	MEM_MEG=$( free -m | sed -n 2p | tr -s ' ' | cut -d\  -f2 )
	CPU_SPEED=$( lscpu | grep "MHz" | tr -s ' ' | cut -d\  -f3 | cut -d'.' -f1 )
	CPU_CORE=$( lscpu -pCPU | grep -v "#" | wc -l )
	MEM_GIG=$(( ((MEM_MEG / 1000) / 2) ))
	JOBS=$(( MEM_GIG > CPU_CORE ? CPU_CORE : MEM_GIG ))

	DISK_INSTALL=$( df -h . | tail -1 | tr -s ' ' | cut -d\  -f1 )
	DISK_TOTAL_KB=$( df . | tail -1 | awk '{print $2}' )
	DISK_AVAIL_KB=$( df . | tail -1 | awk '{print $4}' )
	DISK_TOTAL=$(( DISK_TOTAL_KB / 1048576 ))
	DISK_AVAIL=$(( DISK_AVAIL_KB / 1048576 ))

	printf "\\n\\tOS name: %s\\n" "${OS_NAME}"
	printf "\\tOS Version: %s\\n" "${OS_VER}"
	printf "\\tCPU speed: %sMhz\\n" "${CPU_SPEED}"
	printf "\\tCPU cores: %s\\n" "${CPU_CORE}"
	printf "\\tPhysical Memory: %s Mgb\\n" "${MEM_MEG}"
	printf "\\tDisk install: %s\\n" "${DISK_INSTALL}"
	printf "\\tDisk space total: %sG\\n" "${DISK_TOTAL%.*}"
	printf "\\tDisk space available: %sG\\n" "${DISK_AVAIL%.*}"
	printf "\\tConcurrent Jobs (make -j): ${JOBS}\\n"

	if [ "${MEM_MEG}" -lt 7000 ]; then
		printf "\\n\\tYour system must have 7 or more Gigabytes of physical memory installed.\\n"
		printf "\\tExiting now.\\n\\n"
		exit 1;
	fi

	if [ "${OS_VER}" -lt 7 ]; then
		printf "\\n\\tYou must be running Centos 7 or higher to install GSTIO.\\n"
		printf "\\tExiting now.\\n\\n"
		exit 1;
	fi

	if [ "${DISK_AVAIL%.*}" -lt "${DISK_MIN}" ]; then
		printf "\\n\\tYou must have at least %sGB of available storage to install GSTIO.\\n" "${DISK_MIN}"
		printf "\\tExiting now.\\n\\n"
		exit 1;
	fi

	printf "\\n"

       

	function print_instructions()
	{
		printf "\\t%s -f %s &\\n" "$( command -v mongod )" "${MONGOD_CONF}"
		printf "\\tsource /opt/rh/python33/enable\\n"
		printf '\texport PATH=${HOME}/opt/mongodb/bin:$PATH\n'
		printf "\\tcd %s; make test\\n\\n" "${BUILD_DIR}"
		return 0
	}

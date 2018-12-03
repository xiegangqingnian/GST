#! /bin/bash

binaries=(clgst
          gstio-abigen
          gstio-launcher
          gstio-s2wasm
          gstio-wast2wasm
          gstiocpp
          kgstd
          nodgst
          gstio-applesdemo)

if [ -d "/usr/local/gstio" ]; then
   printf "\tDo you wish to remove this install? (requires sudo)\n"
   select yn in "Yes" "No"; do
      case $yn in
         [Yy]* )
            if [ "$(id -u)" -ne 0 ]; then
               printf "\n\tThis requires sudo, please run ./gstio_uninstall.sh with sudo\n\n"
               exit -1
            fi

            pushd /usr/local &> /dev/null
            rm -rf gstio
            pushd bin &> /dev/null
            for binary in ${binaries[@]}; do
               rm ${binary}
            done
            # Handle cleanup of directories created from installation
            if [ "$1" == "--full" ]; then
               if [ -d ~/Library/Application\ Support/gstio ]; then rm -rf ~/Library/Application\ Support/gstio; fi # Mac OS
               if [ -d ~/.local/share/gstio ]; then rm -rf ~/.local/share/gstio; fi # Linux
            fi
            popd &> /dev/null
            break;;
         [Nn]* )
            printf "\tAborting uninstall\n\n"
            exit -1;;
      esac
   done
fi

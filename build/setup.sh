#!/bin/bash
#set -x
set -o nounset
ALLMEDIAVERSION="AllStream 0.1.1"
CURRENT_PATH=`pwd`
cd ${CURRENT_PATH}/..
export ALLSTREAM_ROOT=$PWD
export PREFIX_ROOT=/home/allstream/
export THIRD_ROOT=${ALLSTREAM_ROOT}/3rd_party/
export EXTEND_ROOT=${ALLSTREAM_ROOT}/extend/linux64/

find=`env|grep PKG_CONFIG_PATH`    
if [ "find${find}" == "find" ]; then    
    export PKG_CONFIG_PATH=${EXTEND_ROOT}/lib/pkgconfig/
else
    export PKG_CONFIG_PATH=${EXTEND_ROOT}/lib/pkgconfig/:${PKG_CONFIG_PATH}
fi

find=`env|grep PATH`
if [ "find${find}" == "find" ]; then    
    export PATH=${EXTEND_ROOT}/bin/
else
    export PATH=${EXTEND_ROOT}/bin/:${PATH}
fi
echo "------------------------------------------------------------------------------"
echo " PKG_CONFIG_PATH: ${PKG_CONFIG_PATH}"
echo " PATH ${PATH}"
echo " ALLSTREAM_ROOT exported as ${ALLSTREAM_ROOT}"
echo "------------------------------------------------------------------------------"

#
# Sets QUIT variable so script will finish.
#
quit()
{
    QUIT=$1
}

download_3rd()
{
    if [ ! -f ${THIRD_ROOT}/3rd.list ]; then
        echo "there is no 3rd package list\n"
        return 1
    fi
    cat ${THIRD_ROOT}/3rd.list|while read LINE
    do
        name=`echo "${LINE}"|awk -F '|' '{print $1}'`
        url=`echo "${LINE}"|awk -F '|' '{print $2}'`
        package=`echo "${LINE}"|awk -F '|' '{print $3}'`
        if [ ! -f ${THIRD_ROOT}/${package} ]; then
            echo "begin:download :${name}..................."
            wget --no-check-certificate ${url}?raw=true -O ${THIRD_ROOT}/${package}
            echo "end:download :${name}....................."
        fi     
    done
    return 0
}

build_EasyAACEncoder()
{
    module_pack="EasyAACEncoder-master.zip"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the EasyAACEncoder package from server\n"
        wget https://github.com/EasyDarwin/EasyAACEncoder/archive/master.zip -O ${module_pack}
    fi
    unzip ${module_pack}
    
    cd EasyAACEncoder*
    cd src/
    chmod +x Buildit
    ./Buildit x64
    
    if [ 0 -ne ${?} ]; then
        echo "build EasyAACEncoder fail!\n"
        return 1
    fi
    
    mkdir -p ${EXTEND_ROOT}/include/
    mkdir -p ${EXTEND_ROOT}/lib/
    
    cp EasyAACEncoderAPI.h ${EXTEND_ROOT}/include/
    cp x64/libEasyAACEncoder.a ${EXTEND_ROOT}/lib/
    
    return 0
}

build_c_care()
{
    module_pack="c-ares-1.12.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the osip2 package from server\n"
        wget https://c-ares.haxx.se/download/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd c-ares*
    
    ./configure --prefix=${EXTEND_ROOT}
                
    if [ 0 -ne ${?} ]; then
        echo "configure c-ares fail!\n"
        return 1
    fi
    
    make&&make install
    
    if [ 0 -ne ${?} ]; then
        echo "make c-ares fail!\n"
        return 1
    fi
    
    return 0
}

build_libevent()
{
    module_pack="libevent-2.1.8-stable.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the osip2 package from server\n"
        wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz
    fi
    tar -zxvf ${module_pack}
    
    cd libevent*
    
    ./configure --prefix=${EXTEND_ROOT}
                
    if [ 0 -ne ${?} ]; then
        echo "configurelibevent fail!\n"
        return 1
    fi
    
    make&&make install
    
    if [ 0 -ne ${?} ]; then
        echo "make libevent fail!\n"
        return 1
    fi
    
    return 0
}

build_osip()
{
    module_pack="libosip2-5.0.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the osip2 package from server\n"
        wget http://ftp.gnu.org/gnu/osip/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd libosip2*
    
    ./configure --prefix=${EXTEND_ROOT}
                
    if [ 0 -ne ${?} ]; then
        echo "configure libosip2 fail!\n"
        return 1
    fi
    
    make&&make install
    
    if [ 0 -ne ${?} ]; then
        echo "make libosip2 fail!\n"
        return 1
    fi
    
    return 0
}

build_exosip()
{
    module_pack="libexosip2-5.0.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the pcre package from server\n"
        wget http://www.antisip.com/download/exosip2/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd libexosip2*
    ./configure --prefix=${EXTEND_ROOT} 
                
    if [ 0 -ne ${?} ]; then
        echo "configure libexosip2 fail!\n"
        return 1
    fi
                
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build libexosip2 fail!\n"
        return 1
    fi
    
    return 0
}

build_ortp()
{
    module_pack="ortp-0.25.0.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the ortp package from server\n"
        wget http://download.savannah.nongnu.org/releases/linphone/ortp/sources/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd ortp*
    ./configure --prefix=${EXTEND_ROOT} 
                
    if [ 0 -ne ${?} ]; then
        echo "configure ortp fail!\n"
        return 1
    fi
                
    make && make install
    
    if [ 0 -ne ${?} ]; then
        echo "build ortp fail!\n"
        return 1
    fi
    
    return 0
}

build_openssl()
{
    module_pack="openssl-0.9.8w.tar.gz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the openssl package from server\n"
        wget https://www.openssl.org/source/old/0.9.x/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd openssl*
                    
    if [ 0 -ne ${?} ]; then
        echo "get openssl fail!\n"
        return 1
    fi
    
    ./config shared --prefix=${PREFIX_ROOT}
    if [ 0 -ne ${?} ]; then
        echo "config openssl fail!\n"
        return 1
    fi
    
    make clean
    
    make
    if [ 0 -ne ${?} ]; then
        echo "make openssl fail!\n"
        return 1
    fi
    make test
    if [ 0 -ne ${?} ]; then
        echo "make test openssl fail!\n"
        return 1
    fi
    make install_sw
    if [ 0 -ne ${?} ]; then
        echo "make install openssl fail!\n"
        return 1
    fi
    
    return 0
}

build_rtmpdump()
{
    module_pack="rtmpdump-2.3.tgz"
    cd ${THIRD_ROOT}
    if [ ! -f ${THIRD_ROOT}${module_pack} ]; then
        echo "start get the rtmpdump package from server\n"
        wget http://rtmpdump.mplayerhq.hu/download/${module_pack}
    fi
    tar -zxvf ${module_pack}
    
    cd *rtmpdump*
    #./configure --prefix=${PREFIX_ROOT} 
    PREFIX_ROOT_SED=$(echo ${PREFIX_ROOT} |sed -e 's/\//\\\//g')
    sed -i "s/prefix\=\/usr\/local/prefix\=${PREFIX_ROOT_SED}/" Makefile 
    if [ 0 -ne ${?} ]; then
        echo "configure rtmpdump fail!\n"
        return 1
    fi
    
    sed -i "s/LIB_OPENSSL=-lssl -lcrypto/LIB_OPENSSL=-lssl -lcrypto -ldl/" Makefile 
    if [ 0 -ne ${?} ]; then
        echo "configure rtmpdump fail!\n"
        return 1
    fi
    
    sed -i "s/prefix\=\/usr\/local/prefix\=${PREFIX_ROOT_SED}/" ./librtmp/Makefile 
    if [ 0 -ne ${?} ]; then
        echo "configure librtmp fail!\n"
        return 1
    fi
    
    C_INCLUDE_PATH=${PREFIX_ROOT}/include/:${C_INCLUDE_PATH:=/usr/local/include/}
    export C_INCLUDE_PATH 
    CPLUS_INCLUDE_PATH=${PREFIX_ROOT}/include/:${CPLUS_INCLUDE_PATH:=/usr/local/include/}
    export CPLUS_INCLUDE_PATH
    LIBRARY_PATH=${PREFIX_ROOT}/lib:${LIBRARY_PATH:=/usr/local/lib/}
    export LIBRARY_PATH 
                
    make SHARED=yes && make install SHARED=yes
    
    if [ 0 -ne ${?} ]; then
        echo "build rtmpdump fail!\n"
        return 1
    fi
    
    return 0
}
build_extend_modules()
{
    download_3rd
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_EasyAACEncoder
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_c_care
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_libevent
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_osip
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_exosip
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_ortp
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_openssl
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    build_rtmpdump
    if [ 0 -ne ${?} ]; then
        return 1
    fi
    return 0
}


build_allstream_module()
{
    cd ${ALLSTREAM_ROOT}
    
    chmod +x genMakefiles
    
    ./genMakefiles linux
    
    make&&make install
    
    if [ 0 -ne ${?} ]; then
       echo "make the allmedia fail!\n"
       return 1
    fi
    echo "make the allmedia success!\n"
    return 0
}

build_all_stream()
{
        
    build_extend_modules
    if [ 0 -ne ${?} ]; then
        return
    fi 
    build_allstream_module
    if [ 0 -ne ${?} ]; then
        return
    fi
    echo "make the all modules success!\n"
    cd ${ALLSTREAM_ROOT}
}

all_allstream_func()
{
        TITLE="Setup the allstream module"

        TEXT[1]="rebuild all module"
        FUNC[1]="build_all_stream"
        
        TEXT[2]="build the allstream module"
        FUNC[2]="build_allstream_module"
}

STEPS[1]="all_allstream_func"

QUIT=0

while [ "$QUIT" == "0" ]; do
    OPTION_NUM=1
    if [ ! -x "`which wget 2>/dev/null`" ]; then
        echo "Need to install wget."
        break 
    fi
    for s in $(seq ${#STEPS[@]}) ; do
        ${STEPS[s]}

        echo "----------------------------------------------------------"
        echo " Step $s: ${TITLE}"
        echo "----------------------------------------------------------"

        for i in $(seq ${#TEXT[@]}) ; do
            echo "[$OPTION_NUM] ${TEXT[i]}"
            OPTIONS[$OPTION_NUM]=${FUNC[i]}
            let "OPTION_NUM+=1"
        done

        # Clear TEXT and FUNC arrays before next step
        unset TEXT
        unset FUNC

        echo ""
    done

    echo "[$OPTION_NUM] Exit Script"
    OPTIONS[$OPTION_NUM]="quit"
    echo ""
    echo -n "Option: "
    read our_entry
    echo ""
    ${OPTIONS[our_entry]} ${our_entry}
    echo
done

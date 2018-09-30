# Prerequisites

### APT packages
```sh
sudo apt-get install build-essential autogen autoconf libtool
sudo apt-get install libelfg0-dev
sudo apt-get install libpqxx3-dev
sudo apt-get install libglib2.0-dev
```

### gcc-5 and binutils from PPA
```sh
#sudo apt-get install python-software-properties
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install gcc-5 g++-5

sudo add-apt-repository ppa:spvkgn/binutils
sudo apt-get update
sudo apt-get install binutils
```

### Create and setup /usr/local/taint
```sh
[ ! -d /usr/local/taint ] && sudo mkdir /usr/local/taint \
&& sudo chown theia:theia /usr/local/taint

stat --format='%U:%G' /usr/local/taint | [ `xargs` == 'theia:theia' ] \
|| sudo chown -R theia:theia /usr/local/taint
```

### Boost, avrocpp, rdkafka, schema, api-bindings, pin via Theia repo
You can install these prerequisites from the Theia repo, as follows:
```sh
sudo apt-get install theia-taint-api-bindings
sudo apt-get install theia-taint-avrocpp
sudo apt-get install theia-taint-boost
sudo apt-get install theia-taint-dwarf
sudo apt-get install theia-taint-librdkafka
sudo apt-get install theia-taint-pin
sudo apt-get install theia-taint-schema
```

If you are missing the Theia repo, then create the file `/etc/apt/sources.list.d/theia.list` with the following contents:
```apt
deb [ trusted=yes arch=amd64 ]  ssh://repo@theia-vm1/var/go/.aptly/public/precise precise main
```

You also need to be able to reach `theia-vm` which you can setup using the usual magic in `~/.ssh/config`

### Boost, avrocpp, rdkafka, schema, api-bindings, pin via source
If you need to build the various dependencies from original source instead of using packages, then follow the individual steps for each package below:

* Boost
    * Download boost
    * build and install
        ```sh
        tar -xf boost_1_54_0.tar.gz
        cd boost_1_54_0/
        ./bootstrap.sh --prefix=/usr/local/taint
        ./b2 threading=multi link=shared hardcode-dll-paths=true --layout=tagged
        ./b2 threading=multi link=shared hardcode-dll-paths=true --layout=tagged install
        ```

* Avro-cpp
    * Depends on boost being installed
    * Download avro-cpp
    * build and install
        ```sh
        tar -xf avro-cpp-1.8.2.tar.gz
        cd avro-cpp-1.8.2/
        [ -d build ] && rm -r build
        mkdir -p build && cd build \
        && cmake -G "Unix Makefiles" \
            -D CMAKE_BUILD_TYPE=Debug \
            -D CMAKE_INSTALL_PREFIX=/usr/local/taint \
            -D CMAKE_INSTALL_RPATH=/usr/local/taint/lib \
            -D CMAKE_PREFIX_PATH=/usr/local/taint \
            ..
        make && make install
        ```

* rdkafka
    * Pull code from github
        ```sh
        git clone https://github.com/edenhill/librdkafka.git
        cd librdkafka
        git checkout -b v0.9.4
        ```
    * Since rdkafka's source doesn't support building in a separate build directory, you may need to reset things when doing a rebuild. Reset as follows:
        ```sh
        rm -r *
        git reset --hard
        ```
    * actual build steps:
        ```sh
        CFLAGS="-Wl,-rpath=/usr/local/taint/lib" \
        CXXFLAGS="-Wl,-rpath=/usr/local/taint/lib" \
        LDFLAGS="-Wl,-rpath=/usr/local/taint/lib" \
        ./configure --disable-dependency-tracking --prefix=/usr/local/taint
        make
        make install
        ```

* serialization-schema
    ```sh
    git clone git@tc.gtisc.gatech.edu:ta3-serialization-schema
    cd ta3-serialization-schema
    git checkout develop
    PREFIX=/usr/local/taint
    TC=${PREFIX}/include/tc_schema
    [ ! -d ${TC} ] && \
    sudo mkdir -p ${TC} && \
    sudo chown -R theia ${TC}
    mvn clean exec:java
    mvn install -Dcpp-output-dir=${PREFIX}/include
    PATH=${PREFIX}/bin:$PATH \
    mvn compile -Pcpp -Dcpp-output-dir=${PREFIX}/include
    cp avro/* ${TC}
    ```
* api-bindings
    * Depends on serialization-schema being installed
        ```sh
        git clone git@tc.gtisc.gatech.edu:ta3-api-bindings-cpp
        cd ta3-api-bindings-cpp
        git checkout develop
        sed \
        -i.orig \
        -e 's#\(-lboost_log\)\([^_-]\)#\1-mt\2#' \
        -e 's#\(-lboost_log_setup\)\([^_-]\)#\1-mt\2#' \
        -e '/^CONFIGURE_CXXFLAGS/ '\
        's#\(CONFIGURE_CXXFLAGS=\).*'\
        '#\1"-Wno-deprecated-declarations -pedantic -std=c++11"#' \
        configure.ac
        ./autogen.sh
        [ -d build ] && rm -r build
        mkdir build && cd build
        CC=/usr/bin/gcc-5 CXX=/usr/bin/g++-5 \
        CFLAGS="-I/usr/local/taint/include \
        CXXFLAGS="-I/usr/local/taint/include -Wl,-rpath=/usr/local/taint/lib"\
        CPPFLAGS="-I/usr/local/taint/include -Wl,-rpath=/usr/local/taint/lib"\
        LDFLAGS="-L/usr/local/taint/lib -Wl,-rpath=/usr/local/taint/lib"\
        PKG_CONFIG_PATH=/usr/local/taint/lib/pkgconfig \
        ../configure --prefix=/usr/local/taint
        make
        make install
        ```

* libdwarf and pin
    * Rebuilding libdwarf is normally not necessary. A prebuilt version is included in this repo and can be copied into place along with pin's libs, includes, and binary.
        ```sh
        #from top-level of repo
        cd taint64
        cp -r pin/lib /usr/local/taint
        cp -r pin/include /usr/local/taint
        cp -r pin/bin /usr/local/taint
        ```
    * If you still want to recompile libdwarf:
        ```sh
        [ -d dwarf-20130207 ] && rm -r dwarf-20130207
        tar -xzf libdwarf-20130207.tar.gz
        cd dwarf-20130207
        ./configure --enable-shared --prefix=/usr/local/taint
        make
        cp libdwarf/libdwarf.so /usr/local/taint/lib/
        ```

# Build and install libdft_dta64
```sh
#from top-level of repo
cd taint64/taint64
./build.sh
./install.sh
```

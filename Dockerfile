# daemon runs in the background
# run something like tail /var/log/Conceald/current to see the status
# be sure to run with volumes, ie:
# docker run -v $(pwd)/Conceald:/var/lib/Conceald -v $(pwd)/wallet:/home/Conceal --rm -ti Conceal:0.2.2
ARG base_image_version=0.10.0
FROM phusion/baseimage:$base_image_version

ADD https://github.com/just-containers/s6-overlay/releases/download/v1.21.2.2/s6-overlay-amd64.tar.gz /tmp/
RUN tar xzf /tmp/s6-overlay-amd64.tar.gz -C /

ADD https://github.com/just-containers/socklog-overlay/releases/download/v2.1.0-0/socklog-overlay-amd64.tar.gz /tmp/
RUN tar xzf /tmp/socklog-overlay-amd64.tar.gz -C /

ARG Conceal_VERSION=v0.4.4
ENV Conceal_VERSION=${Conceal_VERSION}

# install build dependencies
# checkout the latest tag
# build and install
RUN apt-get update && \
    apt-get install -y \
      build-essential \
      python-dev \
      gcc-4.9 \
      g++-4.9 \
      git cmake \
      libboost1.58-all-dev \
      librocksdb-dev && \
    git clone https://github.com/TheCircleFoundation/Conceal/Conceal.git /src/Conceal && \
    cd /src/Conceal && \
    git checkout $Conceal_VERSION && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_CXX_FLAGS="-g0 -Os -fPIC -std=gnu++11" .. && \
    make -j$(nproc) && \
    mkdir -p /usr/local/bin && \
    cp src/Conceald /usr/local/bin/Conceald && \
    cp src/walletd /usr/local/bin/walletd && \
    cp src/simplewallet /usr/local/bin/simplewallet && \
    cp src/miner /usr/local/bin/miner && \
    strip /usr/local/bin/Conceald && \
    strip /usr/local/bin/walletd && \
    strip /usr/local/bin/simplewallet && \
    strip /usr/local/bin/miner && \
    cd / && \
    rm -rf /src/Conceal && \
    apt-get remove -y build-essential python-dev gcc-4.9 g++-4.9 git cmake libboost1.58-all-dev librocksdb-dev && \
    apt-get autoremove -y && \
    apt-get install -y  \
      libboost-system1.58.0 \
      libboost-filesystem1.58.0 \
      libboost-thread1.58.0 \
      libboost-date-time1.58.0 \
      libboost-chrono1.58.0 \
      libboost-regex1.58.0 \
      libboost-serialization1.58.0 \
      libboost-program-options1.58.0 \
      libicu55

# setup the Conceald service
RUN useradd -r -s /usr/sbin/nologin -m -d /var/lib/Conceald Conceald && \
    useradd -s /bin/bash -m -d /home/Conceal Conceal && \
    mkdir -p /etc/services.d/Conceald/log && \
    mkdir -p /var/log/Conceald && \
    echo "#!/usr/bin/execlineb" > /etc/services.d/Conceald/run && \
    echo "fdmove -c 2 1" >> /etc/services.d/Conceald/run && \
    echo "cd /var/lib/Conceald" >> /etc/services.d/Conceald/run && \
    echo "export HOME /var/lib/Conceald" >> /etc/services.d/Conceald/run && \
    echo "s6-setuidgid Conceald /usr/local/bin/Conceald" >> /etc/services.d/Conceald/run && \
    chmod +x /etc/services.d/Conceald/run && \
    chown nobody:nogroup /var/log/Conceald && \
    echo "#!/usr/bin/execlineb" > /etc/services.d/Conceald/log/run && \
    echo "s6-setuidgid nobody" >> /etc/services.d/Conceald/log/run && \
    echo "s6-log -bp -- n20 s1000000 /var/log/Conceald" >> /etc/services.d/Conceald/log/run && \
    chmod +x /etc/services.d/Conceald/log/run && \
    echo "/var/lib/Conceald true Conceald 0644 0755" > /etc/fix-attrs.d/Conceald-home && \
    echo "/home/Conceal true Conceal 0644 0755" > /etc/fix-attrs.d/Conceal-home && \
    echo "/var/log/Conceald true nobody 0644 0755" > /etc/fix-attrs.d/Conceald-logs

VOLUME ["/var/lib/Conceald", "/home/Conceal","/var/log/Conceald"]

ENTRYPOINT ["/init"]
CMD ["/usr/bin/execlineb", "-P", "-c", "emptyenv cd /home/Conceal export HOME /home/Conceal s6-setuidgid Conceal /bin/bash"]

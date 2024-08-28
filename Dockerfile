#FROM --platform=linux/armhf debian:bullseye
FROM debian:bullseye

RUN apt update
RUN echo "deb [trusted=yes] https://andrewharle.github.io/REPO/debian buster experimental" > /etc/apt/sources.list.d/100-andrewharle_repo.list
RUN apt install -yf nano ca-certificates openjdk-17-jdk-headless wget procps net-tools
RUN apt update

WORKDIR /tmp/

RUN wget http://launchpadlibrarian.net/473026110/libboost-system1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN wget http://launchpadlibrarian.net/473026093/libboost-filesystem1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN wget http://launchpadlibrarian.net/473026096/libboost-iostreams1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN wget http://launchpadlibrarian.net/473026103/libboost-program-options1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN dpkg -i libboost-system1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN dpkg -i libboost-filesystem1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN dpkg -i libboost-iostreams1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN dpkg -i libboost-program-options1.67.0_1.67.0-17ubuntu8_armhf.deb
RUN apt install -yf mongodb

RUN echo "port = 27117" >> /etc/mongodb.conf
RUN echo "storageEngine=mmapv1" >> /etc/mongodb.conf

COPY mongodb /etc/init.d/mongodb
RUN chmod +x /etc/init.d/mongodb
RUN mkdir /run/mongodb; chown mongodb:mongodb /run/mongodb/

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh

RUN echo 'deb [ trusted=yes arch=amd64,arm64 ] https://www.ui.com/downloads/unifi/debian stable ubiquiti' > /etc/apt/sources.list.d/100-ubnt-unifi.list
RUN apt update && rm -rf /tmp/*
RUN apt download unifi
COPY ace.jar /tmp/
COPY repack_unifi.sh /tmp/
RUN chmod +x /tmp/repack_unifi.sh
RUN /tmp/repack_unifi.sh /tmp/unifi*.deb /tmp/ace.jar
RUN apt install -yf /tmp/*-modified.deb
RUN rm -rf /tmp/*
COPY armv7 /usr/lib/unifi/lib/native/Linux/armv7/
COPY etc_default_unifi /etc/default/unifi
COPY unifi /etc/init.d/unifi
RUN chmod +x /etc/init.d/unifi

WORKDIR /
#ENTRYPOINT [ "/entrypoint.sh" ]

FROM --platform=linux/armhf debian:bullseye
#FROM debian:bullseye

WORKDIR /tmp/

RUN apt update; \
 echo "deb [trusted=yes] https://andrewharle.github.io/REPO/debian buster experimental" > /etc/apt/sources.list.d/100-andrewharle_repo.list; \
 apt install -yf nano ca-certificates openjdk-17-jdk-headless wget procps net-tools netcat; \
 apt update

RUN wget http://launchpadlibrarian.net/473026110/libboost-system1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 wget http://launchpadlibrarian.net/473026093/libboost-filesystem1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 wget http://launchpadlibrarian.net/473026096/libboost-iostreams1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 wget http://launchpadlibrarian.net/473026103/libboost-program-options1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 dpkg -i libboost-system1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 dpkg -i libboost-filesystem1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 dpkg -i libboost-iostreams1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 dpkg -i libboost-program-options1.67.0_1.67.0-17ubuntu8_armhf.deb; \
 apt install -yf mongodb; \
 echo "port = 27117" >> /etc/mongodb.conf; \
 echo "storageEngine=mmapv1" >> /etc/mongodb.conf

COPY mongodb /etc/init.d/mongodb
RUN chmod +x /etc/init.d/mongodb; \
 mkdir /run/mongodb; \
 ln -sf /dev/stdout /var/log/mongodb/mongodb.log; \
 echo 'deb [ trusted=yes arch=amd64,arm64 ] https://www.ui.com/downloads/unifi/debian stable ubiquiti' > /etc/apt/sources.list.d/100-ubnt-unifi.list; \
 apt update; \
 rm -rf /tmp/*; \
 apt download unifi

COPY ace.jar /tmp/
COPY repack_unifi.sh /tmp/

RUN chmod +x /tmp/repack_unifi.sh; \
 /tmp/repack_unifi.sh /tmp/unifi*.deb /tmp/ace.jar; \
 apt install -yf /tmp/*-modified.deb; \
 rm -rf /tmp/*

COPY armv7 /usr/lib/unifi/lib/native/Linux/armv7/
COPY etc_default_unifi /etc/default/unifi
COPY unifi /etc/init.d/unifi

RUN chmod +x /etc/init.d/unifi; \
 mkdir -p /var/log/unifi; \
 ln -sf /dev/stdout /var/log/unifi/server.log; \
 ln -sf /dev/stdout /var/log/unifi/startup.log

#COPY entrypoint.sh /
#RUN chmod +x /entrypoint.sh

WORKDIR /

# The bundled snappy-java Linux/arm and Linux/armv7 libraries use the soft-float
# ABI. Debian's ARMhf JVM needs the distribution's hard-float JNI build.
# JAVA_TOOL_OPTIONS also reaches JVMs started by the application during restart.
ENV JAVA_TOOL_OPTIONS="-Dorg.xerial.snappy.lib.path=/usr/lib/arm-linux-gnueabihf/jni"
COPY tools/SnappyNativeSmoke.java /tmp/SnappyNativeSmoke.java
RUN apt-get update && \
 apt-get install -y --no-install-recommends libsnappy-jni && \
 mkdir -p /tmp/snappy-smoke && \
 javac -cp '/usr/lib/unifi/lib/*:/usr/lib/unifi/lib/local/*' -d /tmp/snappy-smoke /tmp/SnappyNativeSmoke.java && \
 java -cp '/tmp/snappy-smoke:/usr/lib/unifi/lib/*:/usr/lib/unifi/lib/local/*' SnappyNativeSmoke && \
 rm -rf /tmp/snappy-smoke /tmp/SnappyNativeSmoke.java /var/lib/apt/lists/*
#ENTRYPOINT [ "/entrypoint.sh" ]

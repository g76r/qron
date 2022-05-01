#FROM debian:11.3-slim
FROM g76r/qt6-runner:20220501
LABEL description="qron free scheduler"
LABEL org.opencontainers.image.authors="Grégoire Barbier <devel@g76r.eu>"

# server-side Qt, RDBMS and graphviz dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update \
 && apt-get -y install libqt5core5a libqt5network5 libqt5sql5-psql libqt5sql5-sqlite graphviz

# debuging and convenience utilities used on testing server
RUN apt-get update \
 && apt-get -y install gdb vim tshark

# qron
WORKDIR /opt/qron/qrond
ENV LD_LIBRARY_PATH=/opt/qron/qrond/lib:"$LD_LIBRARY_PATH"
ENTRYPOINT [ "/opt/qron/qrond/bin/qrond" ]
CMD [ "--config-file", "qron.conf" ]
ADD qrond-linux-amd64.tar.bz2 /opt/qron
RUN adduser --disabled-password --gecos "" qron \
 && chown -R qron:qron /opt/qron \
 && apt-get update \
 && apt-get -y install bzip2
USER qron:qron
EXPOSE 8086/tcp

# run attached using e.g.
# docker run -p 8086:8086/tcp g76r/qrond:master
# run detached using e.g.
# docker run --restart unless-stopped -d -p 8086:8086/tcp -e TZ=Europe/Paris --name qron g76r/qrond:master --config-file massive2c.conf


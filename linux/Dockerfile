FROM debian:11
RUN apt-get -y update && \
    apt-get -y install libqt5core5a libqt5network5 libqt5sql5-psql libqt5sql5-sqlite graphviz bzip2 && \
    apt-get clean
COPY qrond-linux-amd64.tar.bz2 /
RUN apt-get -y install bzip2 gdb && \
    adduser --disabled-password --gecos "" qron && \
    mkdir -p /opt/qron && \
    cd /opt/qron && tar xf /qrond-linux-amd64.tar.bz2 && \
    chown -R qron:qron /opt/qron && \
    ls -lR /opt/qron
USER qron:qron
WORKDIR /opt/qron/qrond
ENTRYPOINT [ "/opt/qron/qrond/bin/qrond" ]
CMD [ "--config-file", "qron.conf" ]
ENV LD_LIBRARY_PATH=/opt/qron/qrond/lib

# run attached using e.g.
# docker run -p 8086:8086/tcp g76r/qrond:master
# run detached using e.g.
# docker run --restart unless-stopped -d -p 8086:8086/tcp -e TZ=Europe/Paris --name qron g76r/qrond:master --config-file massive2c.conf


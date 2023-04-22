FROM g76r/qt6-runner:20230403
LABEL description="qron free scheduler"
LABEL org.opencontainers.image.authors="Grégoire Barbier <devel@g76r.eu>"

# server-side Qt, RDBMS and graphviz dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt update -y && apt upgrade -y && apt install -y graphviz gdb vim tshark bzip2 && apt clean -y

# qron
WORKDIR /opt/qron/qrond
ENV LD_LIBRARY_PATH=/opt/qron/qrond/lib:"$LD_LIBRARY_PATH"
ENTRYPOINT [ "/opt/qron/qrond/bin/qrond" ]
CMD [ "--config-file", "qron.conf" ]
ADD qrond-linux-amd64.tar.bz2 /opt/qron
RUN adduser --disabled-password --gecos "" qron \
 && chown -R qron:qron /opt/qron
USER qron:qron
EXPOSE 8086/tcp

# run attached using e.g.
# docker run -p 8086:8086/tcp g76r/qrond:master
# run detached using e.g.
# docker run --restart unless-stopped -d -p 8086:8086/tcp -e TZ=Europe/Paris --name qron g76r/qrond:master --config-file massive2c.conf


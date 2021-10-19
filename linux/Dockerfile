FROM g76r/sid-qt-ss:20200910
COPY qrond /opt/qron
USER nobody:nogroup
WORKDIR /opt/qron
ENTRYPOINT [ "/opt/qron/bin/qrond" ]
CMD [ "--config-file", "qron.conf" ]
ENV LD_LIBRARY_PATH=/opt/qron/lib

# run attached using e.g.
# docker run -p 8086:8086/tcp g76r/qrond:200911
# run detached using e.g.
# docker run --restart unless-stopped -d -p 8086:8086/tcp -e TZ=Europe/Paris --name qron qrond --config-file massive2c.conf


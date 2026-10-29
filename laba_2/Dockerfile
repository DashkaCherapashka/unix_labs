FROM alpine:3.20

RUN apk add --no-cache util-linux coreutils

COPY worker.sh /usr/local/bin/worker.sh
RUN chmod +x /usr/local/bin/worker.sh

VOLUME ["/data"]

ENTRYPOINT ["/usr/local/bin/worker.sh"]

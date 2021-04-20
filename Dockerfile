FROM gcc:9.3.0

RUN apt update && \
    apt install -y \
        autoconf \
        automake \
        libtool \
        libgmp3-dev \
        make \
        time 

COPY build_all.sh /opt/
COPY src/GRAPES /opt/GRAPES
COPY src/meddly /opt/meddly 
COPY src/ /opt/src/

RUN cd /opt/ && ./build_all.sh 

ENTRYPOINT [ "/opt/src/grapes_dd" ]


FROM gcc:9.3.0

RUN apt update && \
    apt install -y \
        autoconf \
        automake \
        libtool \
        libgmp3-dev \
        make \
        time 

#install meddly
COPY src/meddly /opt/meddly 
WORKDIR /opt/meddly
RUN ./autogen.sh && \
    ./configure CXXFLAGS="-O3" --without-gmp && \
    make && \
    make install && \ 
    make clean 

#install grapeslib 
COPY src/GRAPES /opt/GRAPES
WORKDIR /opt/GRAPES
RUN make -B && \
    make clean 

#grapes-dd stuff 
COPY src/ /opt/src/
WORKDIR /opt/src 
RUN make all && \
    make clean && \
    cp grapes_dd orders /usr/bin

# ENTRYPOINT [ "/opt/src/grapes_dd" ]


#ENTRYPOINT [ "grapes_dd" ]



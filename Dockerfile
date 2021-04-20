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
RUN ./autogen.sh && ./configure CXXFLAGS="-O3" --without-gmp && make && make install 

#install grapeslib 
COPY src/GRAPES /opt/GRAPES
WORKDIR /opt/GRAPES
RUN make -B 

#grapes-dd stuff 
COPY src/ /opt/src/
WORKDIR /opt/src 
RUN make all && cp grapes_dd entropy orders /usr/bin


ENTRYPOINT [ "grapes_dd" ]

# COPY build_all.sh /opt/
# COPY src/GRAPES /opt/GRAPES
# COPY src/meddly /opt/meddly 
# COPY src/ /opt/src/

# RUN cd /opt/ && ./build_all.sh 

# ENTRYPOINT [ "/opt/src/grapes_dd" ]


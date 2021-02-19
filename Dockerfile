FROM gcc:6.3.0

RUN apt update && \
    apt install -y \
        autoconf \
        automake \
        libtool \
        libgmp3-dev \
        make \
        time 

#download and compile Meddly Library v0.15.0
RUN mkdir /opt/Meddly
WORKDIR /opt/Meddly
RUN wget https://github.com/asminer/meddly/archive/v0.15.0.tar.gz && \
    tar xf v0.15.0.tar.gz  && \
    cd meddly-0.15.0 && \
    ./autogen.sh && ./configure CXXFLAGS="-O3" --without-gmp --prefix=/usr/local && make && make install 

#compile GRAPES 
COPY src/GRAPES /opt/GRAPES
WORKDIR /opt/GRAPES
RUN make -B && make grapes && mv grapes /usr/local/bin/


#compile GRAPES-DD
COPY src/*.cpp src/*.hpp src/Makefile /opt/indexing/
COPY docker/* /opt/
WORKDIR /opt/indexing
RUN ln -s /opt/GRAPES /opt/indexing/GRAPES && \
    make grapes_dd && \ 
    mv grapes_dd /usr/local/bin/ && \
    cd .. && g++ -o mdd4grapes mdd4grapes.cpp && \
    mv mdd4grapes /usr/local/bin/ && \
    mv start.sh /usr/local/bin/

WORKDIR /

ENTRYPOINT [ "/usr/local/bin/start.sh" ]
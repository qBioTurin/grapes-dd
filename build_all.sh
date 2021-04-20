#! /bin/bash

cd src 

#build meddly 
cd meddly
./autogen.sh || exit 1 
./configure CXXFLAGS="-O3" --without-gmp || exit 1 
make || exit 1 
make install || exit 1 
cd .. 

#build GRAPESLib 
cd GRAPES
make -B || exit 1 
make || exit 1 
cd ..  

#build GRAPES-DD 
make grapes_dd || exit 1 

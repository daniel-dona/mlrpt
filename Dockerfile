FROM ubuntu:20.04

RUN apt-get update
RUN apt-get upgrade -y
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN apt-get install git cmake wget libpython2-dev python-numpy swig g++ rtl-sdr librtlsdr-dev pkgconf libturbojpeg0-dev libconfig-dev ca-certificates -y

RUN mkdir /app


# Soapy lib

WORKDIR /app
#Pending fix to use the latest version from GitHub
RUN wget https://github.com/pothosware/SoapySDR/archive/soapy-sdr-0.7.2.tar.gz
RUN tar -xvf soapy-sdr-0.7.2.tar.gz
RUN mkdir /app/SoapySDR-soapy-sdr-0.7.2/build
WORKDIR /app/SoapySDR-soapy-sdr-0.7.2/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr ..
RUN make -j4
RUN make install
RUN ldconfig

# Soapy rtl-sdr support

WORKDIR /app
RUN git clone https://github.com/pothosware/SoapyRTLSDR.git
RUN mkdir /app/SoapyRTLSDR/build
WORKDIR /app/SoapyRTLSDR/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr ..
RUN make -j4
RUN make install

# mlrpt

WORKDIR /app
RUN git clone https://github.com/dvdesolve/mlrpt
RUN mkdir /app/mlrpt/build
WORKDIR /app/mlrpt/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/usr ..
RUN make -j4
RUN make install

WORKDIR /app/mlrpt

ENTRYPOINT ["/usr/bin/mlrpt"]

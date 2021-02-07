FROM ubuntu:20.04

RUN apt-get update
RUN apt-get upgrade -y
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN apt-get install git nano cmake wget libpython2-dev python-numpy swig g++ -y

RUN mkdir /app


# Soapy lib

WORKDIR /app
RUN wget https://github.com/pothosware/SoapySDR/archive/soapy-sdr-0.7.2.tar.gz
RUN tar -xvf soapy-sdr-0.7.2.tar.gz
WORKDIR /app/SoapySDR-soapy-sdr-0.7.2
RUN mkdir build
WORKDIR /app/SoapySDR-soapy-sdr-0.7.2/build
RUN cmake ..
RUN make -j4
RUN make install
RUN ldconfig
RUN SoapySDRUtil --info

# Soapy rtl-sdr support

WORKDIR /app
RUN apt-get install rtl-sdr librtlsdr-dev -y
RUN git clone https://github.com/pothosware/SoapyRTLSDR.git
WORKDIR /app/SoapyRTLSDR
RUN mkdir build
WORKDIR /app/SoapyRTLSDR/build
RUN cmake ..
RUN make
RUN make install

# mlrpt

WORKDIR /app
RUN apt-get install pkgconf libturbojpeg0-dev libconfig-dev -y
RUN git clone https://github.com/dvdesolve/mlrpt
RUN mkdir /app/mlrpt/build
WORKDIR /app/mlrpt/build


RUN cmake ..
RUN make -j4
RUN make install



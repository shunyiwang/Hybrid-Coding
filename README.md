# Hybrid-Coding
Hybrid-Coding Based Content Access Control for Information-Centric Networking

### Prerequisites
- Install prerequisites (tested with Ubuntu 16.04)
```
Install ndn-cxx
sudo apt-get install build-essential libcrypto++-dev libsqlite3-dev libboost-all-dev libssl-dev
sudo apt-get install doxygen graphviz python-sphinx python-pip
sudo pip install sphinxcontrib-doxylink sphinxcontrib-googleanalytics
./waf configure
./waf
sudo ./waf install
echo /usr/local/lib | sudo tee /etc/ld.so.conf.d/ndn-cxx.conf
sudo ldconfig
```

```
Install NFD
sudo apt-get install pkg-config
sudo apt-get install libpcap-dev
sudo apt-get install doxygen graphviz python-sphinx
./waf configure
./waf
sudo ./waf install
sudo cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf
```

### Compile
- Complie consumer producer etc.
```
./encode/encode.sh # Divide the original file into original blocks and encodes them into encoded blocks.
./upload/compile_ep.sh # CP uploads all encoded blocks to the server; User downloads the required encoded blocks from the ICN network. 
./decode/decode.sh # Decodes the original content according to the decoding information, and performs an integrity check through MD5.
```

### Test
- Test in a real NDN environment. The network topology is described in our paper.
```
a. nfd-start
b. ./encode/encode
c. ./upload/consumer
d. ./upload/producer
e. ./decode/decode
```






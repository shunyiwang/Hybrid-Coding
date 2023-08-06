# Hybrid-Coding
Install ndn-cxx
sudo apt-get install build-essential libcrypto++-dev libsqlite3-dev libboost-all-dev libssl-dev
sudo apt-get install doxygen graphviz python-sphinx python-pip
sudo pip install sphinxcontrib-doxylink sphinxcontrib-googleanalytics
./waf configure
./waf
sudo ./waf install
echo /usr/local/lib | sudo tee /etc/ld.so.conf.d/ndn-cxx.conf
sudo ldconfig

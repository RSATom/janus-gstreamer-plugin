# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/bionic64"

  config.vm.provider "virtualbox" do |vb|
     vb.gui = false
  end

  config.vm.provision "shell", inline: <<-SHELL
    # Permit anyone to start the GUI
    sudo sed -i 's/allowed_users=.*$/allowed_users=anybody/' /etc/X11/Xwrapper.config

    apt-get update
    sudo apt install -y build-essential git
    sudo apt install -y libmicrohttpd-dev libjansson-dev libnice-dev \
        libssl-dev libsrtp2-dev libsofia-sip-ua-dev libglib2.0-dev \
        libopus-dev libogg-dev libcurl4-openssl-dev liblua5.3-dev \
        pkg-config gengetopt libtool automake
    sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
    #sudo apt install -y xubuntu-desktop mc

    sudo mkdir /opt/janus -p
    sudo chown vagrant:vagrant /opt/janus
  SHELL

  config.vm.provision "shell", privileged: false, inline: <<-SHELL
    mkdir git -p && cd git
    git clone https://github.com/meetecho/janus-gateway.git
    cd janus-gateway && sh ./autogen.sh && ./configure --prefix=/opt/janus && make && make install

    cd ~/git
    git clone https://github.com/RSATom/janus-gstreamer-plugin.git
    cd janus-gstreamer-plugin && ./autogen.sh && \
        ./configure --prefix=/opt/janus CXXFLAGS="-g -Og" && make && make install
  SHELL
end

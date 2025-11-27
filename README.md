# OAI 5G SA Environment (CN5G + gNB + nrUE)

This repository provides a minimal and reproducible setup for running **OpenAirInterface 5G Standalone (SA)**, including:

- **OAI 5G Core (CN5G)**
- **OAI gNB**
- **OAI nrUE**

The installation and usage instructions summarize the official OAI tutorials while adapting paths and commands to the directory structure of this repository.

---

## 📦 Installation

Minimum hardware requirements:
- Laptop/Desktop/Server for OAI CN5G and OAI gNB
    - Operating System: [Ubuntu 24.04 LTS](https://releases.ubuntu.com/24.04/ubuntu-24.04.2-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 32 GB
    - 
### 1. Install & Set Up OAI CN5G  
The OAI 5G Core is deployed using Docker Compose.

**Key steps include:**
- Install Docker Engine & Docker Compose  
- Pull OAI CN5G Docker images  
- Configure and launch the 5G core components  
  (AMF, SMF, UPF, NRF, AUSF, MongoDB, WebUI)


## 1.1 Clone this repo:
```cd ~
git clone https://github.com/seyeon4302/oai-project.git

```
## 1.2 OAI CN5G pre-requisites

```bash
sudo apt install -y git net-tools putty

# https://docs.docker.com/engine/install/ubuntu/
sudo apt update
sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Add your username to the docker group, otherwise you will have to run in sudo mode.
sudo usermod -a -G docker $(whoami)
reboot
```

## 1.3 Pull OAI CN5G docker images

```bash
cd ~/oai-project/oai-cn5g
docker compose pull
```

This repository follows the official directory and compose structure.
Reference: [OAI CN5G Tutorial] https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_CN5G.md
---

### 2. Install & Build OAI gNB and OAI nrUE  


Both gNB and nrUE are built from the `openairinterface5g` source.

## 2.1 OAI gNB and OAI nrUE pre-requisites

### Build UHD from source
```bash
# https://files.ettus.com/manual/page_build_guide.html
sudo apt install -y autoconf automake build-essential ccache cmake cpufrequtils doxygen ethtool g++ git inetutils-tools libboost-all-dev libncurses-dev libusb-1.0-0 libusb-1.0-0-dev libusb-dev python3-dev python3-mako python3-numpy python3-requests python3-scipy python3-setuptools python3-ruamel.yaml

git clone https://github.com/EttusResearch/uhd.git ~/uhd
cd ~/uhd
git checkout v4.8.0.0
cd host
mkdir build
cd build
cmake ../
make -j $(nproc)
make test # This step is optional
sudo make install
sudo ldconfig
sudo uhd_images_downloader
```

## 2.2 Build OAI gNB and OAI nrUE

```bash
# Get openairinterface5g source code
cd ~/oai-project/openairinterface5g


# Install OAI dependencies
cd ~/oai-project/openairinterface5g/cmake_targets
./build_oai -I

# nrscope dependencies
sudo apt install -y libforms-dev libforms-bin

# Build OAI gNB
cd ~/oai-project/openairinterface5g/cmake_targets
./build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope" -C
```



## 🚀 Usage Scenarios

### 1. Run OAI CN5G (Terminal 1)
```cd ~/oai-project/oai-cn5g
docker compose up -d
```

### 2. Run OAI gNB (Terminal 2)
```cd ~/oai-project/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem \
    -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf \
    --gNBs.[0].min_rxtxtime 6 \
    --rfsim
```

### 3. Run OAI nrUE (Terminal 3)
```cd ~/oai-project/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-uesoftmodem \
    -r 106 \
    --numerology 1 \
    --band 78 \
    -C 3619200000 \
    --uicc0.imsi 001010000000001 \
    --rfsim \
    --rfsimulator.serveraddr 127.0.0.1 \
    --rfsimulator.serverport 4043 \
    --rfsimulator.IQfile /tmp/rfsim1.iqs
```

Reference: [OAI gNB/nrUE Tutorial] https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md

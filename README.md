# OAI 5G SA Environment (CN5G + gNB + nrUE)

This repository provides a minimal and reproducible setup for running **OpenAirInterface 5G Standalone (SA)**, including:

- **OAI 5G Core (CN5G)**
- **OAI gNB**
- **OAI nrUE**

The installation and usage instructions summarize the official OAI tutorials while adapting paths and commands to the directory structure of this repository.

---

## 📦 Installation

### 1. Install & Set Up OAI CN5G  
Refer to the official OAI documentation:  
🔗 https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_CN5G.md

The OAI 5G Core is deployed using Docker Compose.

**Key steps include:**
- Install Docker Engine & Docker Compose  
- Pull OAI CN5G Docker images  
- Configure and launch the 5G core components  
  (AMF, SMF, UPF, NRF, AUSF, MongoDB, WebUI)

This repository follows the official directory and compose structure.

---

### 2. Install & Build OAI gNB and OAI nrUE  
Refer to the official tutorial:  
🔗 https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md

Both gNB and nrUE are built from the `openairinterface5g` source.

**Example build command:**
```bash
cd openairinterface5g/cmake_targets
./build_oai -c -w USRP
```

## 🚀 Usage Scenarios

### 1. Run OAI CN5G
```cd oai-project/oai-cn5g
docker compose up -d
```

### 2. Run OAI gNB
```cd oai-project/openairinterface5g/cmake_targets/ran_build/build
sudo ./nr-softmodem \
    -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf \
    --gNBs.[0].min_rxtxtime 6 \
    --rfsim
```

### 3. Run OAI nrUE
```cd oai-project/openairinterface5g/cmake_targets/ran_build/build
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


## 📄 References

[OAI CN5G Tutorial] https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_CN5G.md

[OAI gNB/nrUE Tutorial] https://github.com/OPENAIRINTERFACE/openairinterface5g/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md

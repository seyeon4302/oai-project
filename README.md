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

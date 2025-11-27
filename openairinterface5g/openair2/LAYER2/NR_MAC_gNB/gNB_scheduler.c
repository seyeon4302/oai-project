/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file gNB_scheduler.c
 * \brief gNB scheduler top level function operates on per subframe basis
 * \author  Navid Nikaein and Raymond Knopp, WEI-TAI CHEN
 * \date 2010 - 2014, 2018
 * \email: navid.nikaein@eurecom.fr, kroempa@gmail.com
 * \version 0.5
 * \company Eurecom, NTUST
 * @ingroup _mac
 */

#include "assertions.h"

#include "NR_MAC_gNB/mac_proto.h"

#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/vcd_signal_dumper.h"
#include "UTIL/OPT/opt.h"

#include "openair2/X2AP/x2ap_eNB.h"

#include "nr_pdcp/nr_pdcp_oai_api.h"

#include "intertask_interface.h"

#include "executables/softmodem-common.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "executables/nr-softmodem.h"

#include <errno.h>
#include <string.h>

/* ===================== PRB Utilization (MAC-only) ===================== */

#define UTIL_WIN_SLOTS 100
#define NR_MAX_MODULES 16

/* 모듈ID별 누적자 (스케줄러는 sched_lock 잡고 도니 파일정적이면 충분) */
static uint64_t prb_used_acc[NR_MAX_MODULES] = {0};
static uint64_t prb_tot_acc [NR_MAX_MODULES] = {0};
static uint32_t prb_win_cnt [NR_MAX_MODULES] = {0};
static float    dl_prb_util_avg[NR_MAX_MODULES] = {0.f};  /* 0.0 ~ 1.0 */

/* PHY에서 읽어가게 하는 간단 getter (PDSCH 진폭 스케일링 등에 활용)
 * 나중에 nr_dlsch.c 등에서: extern float oai_mac_get_dl_prb_util_avg(module_id_t mod_id);
 */
float oai_mac_get_dl_prb_util_avg(module_id_t mod_id)
{
  if (mod_id >= NR_MAX_MODULES) return 0.f;
  return dl_prb_util_avg[mod_id];
}

/* ===================================================================== */

uint8_t nr_get_rv(int rel_round)
{
  const uint8_t nr_rv_round_map[4] = {0, 2, 3, 1};
  AssertFatal(rel_round < 4, "Invalid index %d for rv\n", rel_round);
  return nr_rv_round_map[rel_round];
}

void clear_nr_nfapi_information(gNB_MAC_INST *gNB,
                                int CC_idP,
                                frame_t frameP,
                                slot_t slotP,
                                nfapi_nr_dl_tti_request_t *DL_req,
                                nfapi_nr_tx_data_request_t *TX_req,
                                nfapi_nr_ul_dci_request_t *UL_dci_req)
{
  /* called below and in simulators, so we assume a lock but don't require it */
  const int num_slots = gNB->frame_structure.numb_slots_frame;
  UL_tti_req_ahead_initialization(gNB, num_slots, CC_idP, frameP, slotP);

  nfapi_nr_dl_tti_pdcch_pdu_rel15_t **pdcch = (nfapi_nr_dl_tti_pdcch_pdu_rel15_t **)gNB->pdcch_pdu_idx[CC_idP];

  gNB->pdu_index[CC_idP] = 0;

  DL_req[CC_idP].SFN = frameP;
  DL_req[CC_idP].Slot = slotP;
  DL_req[CC_idP].dl_tti_request_body.nPDUs             = 0;
  DL_req[CC_idP].dl_tti_request_body.nGroup = 0;
  memset(pdcch, 0, sizeof(*pdcch) * MAX_NUM_CORESET);

  UL_dci_req[CC_idP].SFN = frameP;
  UL_dci_req[CC_idP].Slot = slotP;
  UL_dci_req[CC_idP].numPdus = 0;

  /* advance last round's future UL_tti_req to be ahead of current frame/slot */
  const int size = gNB->UL_tti_req_ahead_size;
  const int prev_slot = frameP * num_slots + slotP + size - 1;
  nfapi_nr_ul_tti_request_t *future_ul_tti_req = &gNB->UL_tti_req_ahead[CC_idP][prev_slot % size];
  future_ul_tti_req->SFN = (prev_slot / num_slots) % 1024;
  LOG_D(NR_MAC, "%d.%d UL_tti_req_ahead SFN.slot = %d.%d for index %d \n", frameP, slotP, future_ul_tti_req->SFN, future_ul_tti_req->Slot, prev_slot % size);
  /* future_ul_tti_req->Slot is fixed! */
  for (int i = 0; i < future_ul_tti_req->n_pdus; i++) {
    future_ul_tti_req->pdus_list[i].pdu_type = 0;
    future_ul_tti_req->pdus_list[i].pdu_size = 0;
  }
  future_ul_tti_req->n_pdus = 0;
  future_ul_tti_req->n_ulsch = 0;
  future_ul_tti_req->n_ulcch = 0;
  future_ul_tti_req->n_group = 0;

  TX_req[CC_idP].Number_of_PDUs = 0;
}

static void clear_beam_information(NR_beam_info_t *beam_info, int frame, int slot, int slots_per_frame)
{
  // for now we use the same logic of UL_tti_req_ahead
  // reset after 1 frame with the exception of 15kHz
  if (beam_info->beam_mode == NO_BEAM_MODE)
    return;
  // initialization done only once
  AssertFatal(beam_info->beam_allocation_size >= 0, "Beam information not initialized\n");
  int idx_to_clear = (frame * slots_per_frame + slot) / beam_info->beam_duration;
  idx_to_clear = (idx_to_clear + beam_info->beam_allocation_size - 1) % beam_info->beam_allocation_size;
  if (slot % beam_info->beam_duration == 0) {
    // resetting previous period allocation
    LOG_D(NR_MAC, "%d.%d Clear beam information for index %d\n", frame, slot, idx_to_clear);
    for (int i = 0; i < beam_info->beams_per_period; i++)
      beam_info->beam_allocation[i][idx_to_clear] = -1;
  }
}

/* the structure nfapi_nr_ul_tti_request_t is very big, let's copy only what is necessary */
static void copy_ul_tti_req(nfapi_nr_ul_tti_request_t *to, nfapi_nr_ul_tti_request_t *from)
{
  int i;

  to->header = from->header;
  to->SFN = from->SFN;
  to->Slot = from->Slot;
  to->n_pdus = from->n_pdus;
  to->rach_present = from->rach_present;
  to->n_ulsch = from->n_ulsch;
  to->n_ulcch = from->n_ulcch;
  to->n_group = from->n_group;

  for (i = 0; i < from->n_pdus; i++) {
    to->pdus_list[i].pdu_type = from->pdus_list[i].pdu_type;
    to->pdus_list[i].pdu_size = from->pdus_list[i].pdu_size;

    switch (from->pdus_list[i].pdu_type) {
      case NFAPI_NR_UL_CONFIG_PRACH_PDU_TYPE:
        to->pdus_list[i].prach_pdu = from->pdus_list[i].prach_pdu;
        break;
      case NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE:
        to->pdus_list[i].pusch_pdu = from->pdus_list[i].pusch_pdu;
        break;
      case NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE:
        to->pdus_list[i].pucch_pdu = from->pdus_list[i].pucch_pdu;
        break;
      case NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE:
        to->pdus_list[i].srs_pdu = from->pdus_list[i].srs_pdu;
        break;
    }
  }

  for (i = 0; i < from->n_group; i++)
    to->groups_list[i] = from->groups_list[i];
}

void gNB_dlsch_ulsch_scheduler(module_id_t module_idP, frame_t frame, slot_t slot, NR_Sched_Rsp_t *sched_info)
{
  protocol_ctxt_t ctxt = {0};
  PROTOCOL_CTXT_SET_BY_MODULE_ID(&ctxt, module_idP, ENB_FLAG_YES, NOT_A_RNTI, frame, slot, module_idP);

  gNB_MAC_INST *gNB = RC.nrmac[module_idP];
  NR_COMMON_channels_t *cc = gNB->common_channels;
  NR_ServingCellConfigCommon_t *scc = cc->ServingCellConfigCommon;

  NR_SCHED_LOCK(&gNB->sched_lock);
  int slots_frame = gNB->frame_structure.numb_slots_frame;
  clear_beam_information(&gNB->beam_info, frame, slot, slots_frame);

  gNB->frame = frame;
  start_meas(&gNB->gNB_scheduler);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_gNB_DLSCH_ULSCH_SCHEDULER, VCD_FUNCTION_IN);

  for (int CC_id = 0; CC_id < MAX_NUM_CCs; CC_id++) {
    int num_beams = 1;
    if (gNB->beam_info.beam_mode != NO_BEAM_MODE)
      num_beams = gNB->beam_info.beams_per_period;
    /* clear vrb_maps */
    for (int i = 0; i < num_beams; i++)
      memset(cc[CC_id].vrb_map[i], 0, sizeof(uint16_t) * MAX_BWP_SIZE);
    /* clear last scheduled slot's content (only)! */
    const int size = gNB->vrb_map_UL_size;
    const int prev_slot = frame * slots_frame + slot + size - 1;
    for (int i = 0; i < num_beams; i++) {
      uint16_t *vrb_map_UL = cc[CC_id].vrb_map_UL[i];
      memcpy(&vrb_map_UL[prev_slot % size * MAX_BWP_SIZE], &gNB->ulprbbl, sizeof(uint16_t) * MAX_BWP_SIZE);
    }
    clear_nr_nfapi_information(gNB, CC_id, frame, slot, &sched_info->DL_req, &sched_info->TX_req, &sched_info->UL_dci_req);
  }

  bool wait_prach_completed = gNB->num_scheduled_prach_rx >= NUM_PRACH_RX_FOR_NOISE_ESTIMATE;
  if ((wait_prach_completed || get_softmodem_params()->phy_test) && (slot == 0) && (frame & 127) == 0) {
    char stats_output[32656] = {0};
    dump_mac_stats(gNB, stats_output, sizeof(stats_output), true);
    LOG_I(NR_MAC, "Frame.Slot %d.%d\n%s\n", frame, slot, stats_output);
  }

  nr_measgap_scheduling(gNB, frame, slot);
  nr_mac_update_timers(module_idP, frame, slot);

  if (wait_prach_completed || get_softmodem_params()->phy_test) {
    /* schedule MIB */
    schedule_nr_mib(module_idP, frame, slot, &sched_info->DL_req);

    /* schedule SIBs */
    if (IS_SA_MODE(get_softmodem_params())) {
      schedule_nr_sib1(module_idP, frame, slot, &sched_info->DL_req, &sched_info->TX_req);
      schedule_nr_other_sib(module_idP, frame, slot, &sched_info->DL_req, &sched_info->TX_req);
    }
  }

  /* schedule PRACH if not in phy_test mode */
  if (get_softmodem_params()->phy_test == 0) {
    const int n_slots_ahead = slots_frame - cc->prach_len + get_NTN_Koffset(scc);
    const frame_t f = (frame + (slot + n_slots_ahead) / slots_frame) % 1024;
    const slot_t s = (slot + n_slots_ahead) % slots_frame;
    schedule_nr_prach(module_idP, f, s);
  }

  /* Schedule CSI-RS transmission */
  nr_csirs_scheduling(module_idP, frame, slot, &sched_info->DL_req);

  /* CSI measurement reporting */
  nr_csi_meas_reporting(module_idP, frame, slot);

  nr_schedule_srs(module_idP, frame, slot);

  /* Random Access */
  if (get_softmodem_params()->phy_test == 0) {
    nr_schedule_RA(module_idP, frame, slot, &sched_info->UL_dci_req, &sched_info->DL_req, &sched_info->TX_req);
  }

  /* UL DCI/PUSCH */
  start_meas(&gNB->schedule_ulsch);
  nr_schedule_ulsch(module_idP, frame, slot, &sched_info->UL_dci_req);
  stop_meas(&gNB->schedule_ulsch);

  /* DL DCI/PDSCH */
  start_meas(&gNB->schedule_dlsch);
  nr_schedule_ue_spec(module_idP, frame, slot, &sched_info->DL_req, &sched_info->TX_req);
  stop_meas(&gNB->schedule_dlsch);

  /* ===================== PRB UTILIZATION (DL) ===================== */
  /* cc[CC0].vrb_map[beam][rb] 에 UE 할당 흔적(!=0)이 표시됨 */
  int used_prbs_slot = 0;
  const int CC0 = 0; /* MAX_NUM_CCs==1 가정 */
  const int num_beams_now = (gNB->beam_info.beam_mode != NO_BEAM_MODE) ? gNB->beam_info.beams_per_period : 1;

  for (int b = 0; b < num_beams_now; b++) {
    for (int rb = 0; rb < MAX_BWP_SIZE; rb++) {
      if (cc[CC0].vrb_map[b][rb] != 0)
        used_prbs_slot++;
    }
  }
  /* tot_prbs_slot: 활성 BWP를 정확히 모르면 MAX_BWP_SIZE로 단순화 */
  const int tot_prbs_slot = MAX_BWP_SIZE;

  /* 슬롯 스냅샷 로그 (시끄러우면 LOG_D → LOG_T/OFF 로 바꿔도 됨) */
  float util_inst = (tot_prbs_slot ? (float)used_prbs_slot / (float)tot_prbs_slot : 0.f);
  if (util_inst < 0.f) util_inst = 0.f; else if (util_inst > 1.f) util_inst = 1.f;
  LOG_D(NR_MAC, "[PWR] %d.%d snapshot: used=%d total=%d util=%.3f\n",
        frame, slot, used_prbs_slot, tot_prbs_slot, util_inst);

  /* 누적 → 윈도우 평균 */
  prb_used_acc[module_idP] += (uint64_t)used_prbs_slot;
  prb_tot_acc [module_idP] += (uint64_t)tot_prbs_slot;
  prb_win_cnt [module_idP]++;

  if (prb_win_cnt[module_idP] >= UTIL_WIN_SLOTS) {
    float util = (prb_tot_acc[module_idP] ? (float)prb_used_acc[module_idP] / (float)prb_tot_acc[module_idP] : 0.f);
    if      (util < 0.f) util = 0.f;
    else if (util > 1.f) util = 1.f;
    dl_prb_util_avg[module_idP] = util;

    /* reset window */
    prb_used_acc[module_idP] = 0;
    prb_tot_acc [module_idP] = 0;
    prb_win_cnt [module_idP] = 0;

    /* 윈도우 평균 로그 */
    LOG_I(NR_MAC, "[PWR] DL PRB util (avg over %d slots) = %.3f\n", UTIL_WIN_SLOTS, util);
  }
  /* =============================================================== */

  nr_sr_reporting(gNB, frame, slot);
  nr_schedule_pucch(gNB, frame, slot);

  /* TODO: copy from gNB->UL_tti_req_ahead[0][current_index], ie. CC_id == 0, only 1 CC supported */
  AssertFatal(MAX_NUM_CCs == 1, "only 1 CC supported\n");
  const int current_index = ul_buffer_index(frame, slot, slots_frame, gNB->UL_tti_req_ahead_size);
  copy_ul_tti_req(&sched_info->UL_tti_req, &gNB->UL_tti_req_ahead[0][current_index]);

  stop_meas(&gNB->gNB_scheduler);
  NR_SCHED_UNLOCK(&gNB->sched_lock);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_gNB_DLSCH_ULSCH_SCHEDULER, VCD_FUNCTION_OUT);
}

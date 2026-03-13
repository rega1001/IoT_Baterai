#include "KirimDataMQTT.h"

// Fungsi untuk mengirim data BMS ke MQTT
void kirimBmsStatus(const char* topic_prefix) {
  char pesan[10];
  char timestamp[25];
  char topic[50];

  // Mengambil data dari objek jkBms (di-extern dari main)
  BmsSysData bms = jkBms.getData();
  
  // Mengupdate pylonEncoder (di-extern dari main)
  pylonEncoder.updateData(bms);

  // Mengambil waktu (di-extern dari main)
  getWaktuSaatIni(timestamp, sizeof(timestamp));
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "timestamp");
  comms.getMqttClient().publish(topic, timestamp);

  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_voltage_v);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_total");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_current_a);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "arus");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%u", bms.soc_percent);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "soc");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%u", bms.avg_cycle_count);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "cycle");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.battery_type);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "bat_type");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_balance);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_bal");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_charge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_charge");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_discharge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_discharge");
  comms.getMqttClient().publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%u", bms.alarm_message);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "alarm");
  comms.getMqttClient().publish(topic, pesan);

  // Looping untuk tegangan per sel
  for (int i = 0; i < 24; i++) {
      char pesanVcell[10];
      snprintf(topic, sizeof(topic), "%sv_sel_%d", topic_prefix, i + 1);
      snprintf(pesanVcell, sizeof(pesanVcell), "%.3f", bms.cell_voltages[i]);
      comms.getMqttClient().publish(topic, pesanVcell);
      delay(5); 
  }
}

void kirimDataInverter(const char* topic_prefix) {
    char pesan[10];
    char timestamp[25];
    char topic[50];

    // Mengambil data dari objek tgproInverter (di-extern dari main)
    TgproData data = tgproInverter.getData();

    // Mengambil waktu (di-extern dari main)
    getWaktuSaatIni(timestamp, sizeof(timestamp));
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "timestamp");
    comms.getMqttClient().publish(topic, timestamp);

    snprintf(pesan, sizeof(pesan), "%.2f", data.V_Grid);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_grid");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.Hz_Grid);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "hz_grid");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.W_Grid);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "w_grid");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.V_Out);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_out");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.Hz_Out);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "hz_out");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.W_Out);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "w_out");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.VA_Out);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "va_out");
    comms.getMqttClient().publish(topic, pesan);
    
    snprintf(pesan, sizeof(pesan), "%.2f", data.V_Batt);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_batt");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.I_Batt);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "i_batt");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.W_Batt);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "w_batt");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%u", data.SOC_Batt);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "soc_batt");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.V_PV);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_pv");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%.2f", data.I_PV);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "i_pv");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.W_PV);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "w_pv");
    comms.getMqttClient().publish(topic, pesan);

    snprintf(pesan, sizeof(pesan), "%d", data.W_Ch_PV);
    snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "w_ch_pv");
    comms.getMqttClient().publish(topic, pesan);

    // Tambahkan pengiriman data lainnya sesuai kebutuhan
}
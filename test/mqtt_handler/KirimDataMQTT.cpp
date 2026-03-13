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
  client.publish(topic, timestamp);

  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_voltage_v);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "v_total");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%.2f", bms.total_current_a);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "arus");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%.1f", bms.soc_percent);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "soc");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%u", bms.avg_cycle_count);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "cycle");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.battery_type);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "bat_type");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_balance);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_bal");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_charge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_charge");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%d", bms.switch_discharge);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "sw_discharge");
  client.publish(topic, pesan);

  snprintf(pesan, sizeof(pesan), "%u", bms.alarm_message);
  snprintf(topic, sizeof(topic), "%s%s", topic_prefix, "alarm");
  client.publish(topic, pesan);

  // Looping untuk tegangan per sel
  for (int i = 0; i < 24; i++) {
      char pesanVcell[10];
      snprintf(topic, sizeof(topic), "%sv_sel_%d", topic_prefix, i + 1);
      snprintf(pesanVcell, sizeof(pesanVcell), "%.3f", bms.cell_voltages[i]);
      client.publish(topic, pesanVcell);
      delay(5); 
  }
}
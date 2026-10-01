#include "App_a7680c.h"
#include "sl_iostream.h"
#include "sl_iostream_uart.h"
#include "sl_iostream_init_eusart_instances.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define A7680C_CMD_DELAY_MS   500
#define A7680C_SMS_DELAY_MS   8000
#define A7680C_BOOT_DELAY_MS  5000
#define A7680C_RX_BUF_SIZE    512
#define A7680C_CLBS_TIMEOUT_MS 30000  // Tang len 30s cho dich vu LBS


#define MQTT_HEALTH_TOPIC "health/data"

// ── Parse tọa độ dạng chuỗi thành coord_t (tránh lỗi printf double) ──
static coord_t parse_coord(const char *s)
{
  coord_t c = { 0, 0, false };

  if (!s) return c;

  while (*s == ' ' || *s == '\r' || *s == '\n') s++;

  if      (*s == '-') { c.negative = true; s++; }
  else if (*s == '+') { s++; }

  while (*s >= '0' && *s <= '9') {
    c.integer = c.integer * 10 + (*s - '0');
    s++;
  }

  if (*s == '.') {
    s++;
    uint32_t multiplier = 100000;
    while (*s >= '0' && *s <= '9' && multiplier > 0) {
      c.decimal += (uint32_t)(*s - '0') * multiplier;
      multiplier /= 10;
      s++;
    }
  }

  return c;
}

// ── In coord_t ra chuỗi ──
static void coord_to_str(coord_t c, char *buf, size_t buf_size)
{
  if (c.negative) {
    snprintf(buf, buf_size, "-%ld.%06lu",
             (long)c.integer, (unsigned long)c.decimal);
  } else {
    snprintf(buf, buf_size, "%ld.%06lu",
             (long)c.integer, (unsigned long)c.decimal);
  }
}

// ── Đọc response từ A7680C (không kiểm tra keyword) ──
static void a7680c_read_response(void)
{
  char     buf[A7680C_RX_BUF_SIZE] = { 0 };
  size_t   total      = 0;
  size_t   bytes_read = 0;
  uint32_t timeout    = 0;

  while (timeout < 3000 && total < (A7680C_RX_BUF_SIZE - 1)) {
    bytes_read = 0;
    sl_iostream_read(sl_iostream_A7680C_handle,
                     &buf[total], 1, &bytes_read);
    if (bytes_read > 0) {
      total  += bytes_read;
      timeout = 0;
    } else {
      sl_sleeptimer_delay_millisecond(10);
      timeout += 10;
    }
  }

  buf[total] = '\0';
  if (total > 0) {
    app_log("[A7680C] Response: %s\n", buf);
  } else {
    app_log("[A7680C] Response: (KHONG CO PHAN HOI - TIMEOUT)\n");
  }
}

// ── Flush toàn bộ dữ liệu còn trong RX buffer ──
static void a7680c_flush_rx(void)
{
  char     flush_buf[64] = { 0 };
  size_t   fr = 0;
  uint32_t ft = 0;

  while (ft < 500) {
    fr = 0;
    sl_iostream_read(sl_iostream_A7680C_handle,
                     flush_buf, sizeof(flush_buf) - 1, &fr);
    if (fr == 0) {
      sl_sleeptimer_delay_millisecond(10);
      ft += 10;
    } else {
      ft = 0;
    }
  }
}

// ── Gửi AT command, chờ response, kiểm tra keyword ──
static bool a7680c_send_cmd_check_timeout(const char *cmd,
                                           const char *expect,
                                           uint32_t    timeout_ms)
{
  char     buf[A7680C_RX_BUF_SIZE] = { 0 };
  size_t   total      = 0;
  size_t   bytes_read = 0;
  uint32_t timeout    = 0;

  app_log("[A7680C] CMD: %s\n", cmd);
  sl_iostream_write(sl_iostream_A7680C_handle, cmd, strlen(cmd));
  sl_iostream_write(sl_iostream_A7680C_handle, "\r\n", 2);
  sl_sleeptimer_delay_millisecond(A7680C_CMD_DELAY_MS);

  while (timeout < timeout_ms && total < (A7680C_RX_BUF_SIZE - 1)) {
    bytes_read = 0;
    sl_iostream_read(sl_iostream_A7680C_handle,
                     &buf[total], 1, &bytes_read);
    if (bytes_read > 0) {
      total  += bytes_read;
      timeout = 0;
      buf[total] = '\0';

      if (expect != NULL && strstr(buf, expect) != NULL) {
        //buf[total] = '\0';
        app_log("[A7680C] Response: %s\n", buf);
        app_log("[A7680C] [OK] Tim thay: \"%s\"\n", expect);
        return true;
      }
    } else {
      sl_sleeptimer_delay_millisecond(10);
      timeout += 10;
    }
  }

  buf[total] = '\0';
  if (total > 0) {
    app_log("[A7680C] Response: %s\n", buf);
  } else {
    app_log("[A7680C] [WARN] Khong co phan hoi cho: %s\n", cmd);
  }

  if (expect != NULL) {
    bool found = (strstr(buf, expect) != NULL);
    if (found) {
      app_log("[A7680C] [OK] Tim thay: \"%s\"\n", expect);
    } else {
      app_log("[A7680C] [WARN] Khong tim thay: \"%s\"\n", expect);
    }
    return found;
  }

  return (total > 0);
}

// ── Wrapper dùng timeout mặc định 3000ms ──
static bool a7680c_send_cmd_check(const char *cmd, const char *expect)
{
  return a7680c_send_cmd_check_timeout(cmd, expect, 3000);
}

// ── Gửi AT command và đọc response (không kiểm tra keyword) ──
static void a7680c_send_cmd(const char *cmd)
{
  app_log("[A7680C] CMD: %s\n", cmd);
  sl_iostream_write(sl_iostream_A7680C_handle, cmd, strlen(cmd));
  sl_iostream_write(sl_iostream_A7680C_handle, "\r\n", 2);
  sl_sleeptimer_delay_millisecond(A7680C_CMD_DELAY_MS);
  a7680c_read_response();
}

// ── Gửi nội dung SMS + Ctrl+Z ──
static void a7680c_send_data(const char *data)
{
  app_log("[A7680C] DATA: %s\n", data);
  sl_iostream_write(sl_iostream_A7680C_handle, data, strlen(data));
  uint8_t ctrl_z = 0x1A;
  sl_iostream_write(sl_iostream_A7680C_handle, &ctrl_z, 1);
  sl_sleeptimer_delay_millisecond(A7680C_SMS_DELAY_MS);
  a7680c_read_response();
}

// ── Kiểm tra đăng ký mạng ──
static bool a7680c_check_reg(const char *at_cmd, const char *label)
{
  char     buf[A7680C_RX_BUF_SIZE] = { 0 };
  size_t   total = 0, bytes_read = 0;
  uint32_t timeout = 0;

  app_log("[A7680C] CMD: %s\n", at_cmd);
  sl_iostream_write(sl_iostream_A7680C_handle, at_cmd, strlen(at_cmd));
  sl_iostream_write(sl_iostream_A7680C_handle, "\r\n", 2);
  sl_sleeptimer_delay_millisecond(A7680C_CMD_DELAY_MS);

  while (timeout < 3000 && total < (A7680C_RX_BUF_SIZE - 1)) {
    bytes_read = 0;
    sl_iostream_read(sl_iostream_A7680C_handle,
                     &buf[total], 1, &bytes_read);
    if (bytes_read > 0) { total += bytes_read; timeout = 0; }
    else { sl_sleeptimer_delay_millisecond(10); timeout += 10; }
  }

  buf[total] = '\0';
  app_log("[A7680C] Response: %s\n", buf);

  bool ok = (strstr(buf, ",1") != NULL || strstr(buf, ",5") != NULL);
  app_log("[A7680C] %s: %s\n", label,
          ok ? "[OK] Da dang ky" : "[WARN] Chua dang ky");
  return ok;
}

// ── Xóa toàn bộ SMS trong SIM ──
static void a7680c_delete_all_sms(void)
{
  app_log("[A7680C] Xoa toan bo SMS trong SIM (AT+CMGD=1,4)...\n");
  a7680c_flush_rx();

  if (a7680c_send_cmd_check_timeout("AT+CMGD=1,4", "OK", 10000)) {
    app_log("[A7680C] [OK] Xoa SMS thanh cong\n");
  } else {
    app_log("[A7680C] [WARN] Xoa SMS that bai (SIM co the trong)\n");
  }
  sl_sleeptimer_delay_millisecond(1000);
}

// ── Lấy vị trí Cell-ID qua AT+CLBS=1,1 ──
// FIX 1: Them AT+CGATT=1 truoc khi goi CLBS — dich vu LBS can data attach
// FIX 2: Doc bo sung sau khi thay "+CLBS:" — tranh cat mat du lieu URC den sau
// FIX 3: Dung coord_t/parse_coord/coord_to_str — tranh loi printf("%.6f") tren EFR32
// FIX 4: Timeout tang len 30s — dich vu LBS co the mat thoi gian tra ve server
sl_status_t App_a7680c_get_cell_location(char *loc_buf, size_t buf_size)
{
  if (loc_buf == NULL || buf_size == 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Gia tri mac dinh neu that bai
  snprintf(loc_buf, buf_size, "Vi tri: Khong xac dinh");

  // ── FIX 1: Dam bao da attach GPRS/data truoc khi query CLBS ──
  app_log("[A7680C] Dam bao GPRS da attach (AT+CGATT=1)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd_check_timeout("AT+CGATT=1", "OK", 10000);
  sl_sleeptimer_delay_millisecond(2000);

  char     buf[A7680C_RX_BUF_SIZE] = { 0 };
  size_t   total = 0, bytes_read = 0;
  uint32_t timeout = 0;

  app_log("[A7680C] Lay vi tri Cell-ID (AT+CLBS=1,1)...\n");
  a7680c_flush_rx();

  sl_iostream_write(sl_iostream_A7680C_handle, "AT+CLBS=1,1", 11);
  sl_iostream_write(sl_iostream_A7680C_handle, "\r\n", 2);
  sl_sleeptimer_delay_millisecond(A7680C_CMD_DELAY_MS);

  bool clbs_found = false;

  while (timeout < A7680C_CLBS_TIMEOUT_MS && total < (A7680C_RX_BUF_SIZE - 1)) {
    bytes_read = 0;
    sl_iostream_read(sl_iostream_A7680C_handle,
                     &buf[total], 1, &bytes_read);
    if (bytes_read > 0) {
      total  += bytes_read;
      timeout = 0;
      buf[total] = '\0';

      if (strstr(buf, "ERROR") != NULL) {
        app_log("[A7680C] Cell-ID response: %s\n", buf);
        app_log("[A7680C] [FAIL] CLBS tra ve ERROR!\n");
        return SL_STATUS_FAIL;
      }

      // ── FIX 2: Khi thay "+CLBS:" voi so ngay sau, doc bo sung ──
      // tranh cat mat phan du lieu con lai (URC den theo tung phan)
      char *clbs_check = strstr(buf, "+CLBS:");
      if (clbs_check && !clbs_found) {
        char *after = clbs_check + 6;
        while (*after == ' ') after++;
        if (*after >= '0' && *after <= '9') {
          clbs_found = true;
          sl_sleeptimer_delay_millisecond(500);
          bytes_read = 0;
          sl_iostream_read(sl_iostream_A7680C_handle,
                           &buf[total], A7680C_RX_BUF_SIZE - total - 1,
                           &bytes_read);
          total += bytes_read;
          buf[total] = '\0';
          break;
        }
      }
    } else {
      sl_sleeptimer_delay_millisecond(10);
      timeout += 10;
    }
  }

  buf[total] = '\0';
  app_log("[A7680C] Cell-ID response: %s\n", buf);

  char *clbs = strstr(buf, "+CLBS:");
  if (clbs == NULL) {
    app_log("[A7680C] [WARN] Khong tim thay +CLBS trong response\n");
    return SL_STATUS_FAIL;
  }

  char *data = clbs + 6;
  while (*data == ' ') data++;

  int  err = 0;
  char lat_str[20] = { 0 };
  char lon_str[20] = { 0 };
  int  acc         = 0;

  // ── FIX: Dung sscanf gon hon, it loi hon so voi parse thu cong bang strchr ──
  int parsed = sscanf(data, "%d,%19[^,],%19[^,],%d",
                      &err, lat_str, lon_str, &acc);

  app_log("[A7680C] CLBS parse: err=%d lat=[%s] lon=[%s] acc=%d parsed=%d\n",
          err, lat_str, lon_str, acc, parsed);

  if (parsed < 3) {
    app_log("[A7680C] [FAIL] Parse CLBS that bai!\n");
    return SL_STATUS_FAIL;
  }

  if (err != 0) {
    app_log("[A7680C] [FAIL] CLBS error code: %d\n", err);
    return SL_STATUS_FAIL;
  }

  // ── FIX 3: Dung coord_t de tranh loi printf("%.6f") tren EFR32 ──
  coord_t lat = parse_coord(lat_str);
  coord_t lon = parse_coord(lon_str);

  char lat_buf[20] = { 0 };
  char lon_buf[20] = { 0 };
  coord_to_str(lat, lat_buf, sizeof(lat_buf));
  coord_to_str(lon, lon_buf, sizeof(lon_buf));

  app_log("[A7680C] Latitude : %s\n", lat_buf);
  app_log("[A7680C] Longitude: %s\n", lon_buf);
  app_log("[A7680C] Do chinh xac: ~%d met\n", acc);

  // Tao link Google Maps
  snprintf(loc_buf, buf_size,
           "https://maps.google.com/?q=%s,%s",
           lat_buf, lon_buf);

  app_log("[A7680C] [OK] Vi tri: %s\n", loc_buf);
  return SL_STATUS_OK;
}

// ── Init A7680C ──
sl_status_t App_a7680c_init(void)
{
  app_log("[A7680C] Initializing...\n");

  sl_sleeptimer_delay_millisecond(A7680C_BOOT_DELAY_MS);

  app_log("[A7680C] Flush URC sau boot...\n");
  a7680c_flush_rx();
  sl_sleeptimer_delay_millisecond(500);

  // ── Buoc 1: Kiem tra AT ──
  app_log("[A7680C] [1/7] Kiem tra AT (thu toi da 5 lan)...\n");
  bool at_ok = false;
  for (int i = 0; i < 5; i++) {
    app_log("[A7680C] AT lan thu %d...\n", i + 1);
    a7680c_flush_rx();
    if (a7680c_send_cmd_check_timeout("AT", "OK", 5000)) {
      at_ok = true;
      break;
    }
    sl_sleeptimer_delay_millisecond(1000);
  }

  if (!at_ok) {
    app_log("[A7680C] FAIL: Module khong phan hoi AT sau 5 lan thu!\n");
    app_log("[A7680C]  - Kiem tra ket noi UART TX/RX\n");
    app_log("[A7680C]  - Kiem tra nguon cap module\n");
    return SL_STATUS_FAIL;
  }

  // ── Buoc 2: Tat echo ──
  app_log("[A7680C] [2/7] Tat echo (ATE0)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd("ATE0");
  sl_sleeptimer_delay_millisecond(500);

  // ── Buoc 3: Kiem tra SIM ──
  app_log("[A7680C] [3/7] Kiem tra SIM (AT+CPIN?)...\n");
  bool sim_ok = false;
  for (int i = 0; i < 3; i++) {
    app_log("[A7680C] CPIN lan thu %d...\n", i + 1);
    a7680c_flush_rx();
    sl_sleeptimer_delay_millisecond(300);
    if (a7680c_send_cmd_check_timeout("AT+CPIN?", "READY", 5000)) {
      sim_ok = true;
      break;
    }
    sl_sleeptimer_delay_millisecond(2000);
  }

  if (!sim_ok) {
    app_log("[A7680C] FAIL: SIM chua san sang!\n");
    app_log("[A7680C]  - Kiem tra SIM da cam chua\n");
    app_log("[A7680C]  - SIM co bi khoa PIN khong\n");
    return SL_STATUS_FAIL;
  }

  // ── Buoc 4: Kiem tra tin hieu ──
  app_log("[A7680C] [4/7] Kiem tra tin hieu (AT+CSQ)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd_check("AT+CSQ", "+CSQ:");

  // ── Buoc 5-6: Kiem tra mang GSM/LTE — thu toi da 10 lan, cach 2s ──
  app_log("[A7680C] [5-6/7] Kiem tra mang (thu toi da 10 lan)...\n");
  bool creg_ok  = false;
  bool cereg_ok = false;

  for (int i = 0; i < 10; i++) {
    app_log("[A7680C] Kiem tra mang lan thu %d...\n", i + 1);

    a7680c_flush_rx();
    creg_ok = a7680c_check_reg("AT+CREG?", "CREG");

    a7680c_flush_rx();
    cereg_ok = a7680c_check_reg("AT+CEREG?", "CEREG");

    if (creg_ok || cereg_ok) {
      break;
    }

    sl_sleeptimer_delay_millisecond(2000);
  }

  if (!creg_ok && !cereg_ok) {
    app_log("[A7680C] FAIL: Khong co mang (ca GSM va LTE) sau 10 lan thu!\n");
    app_log("[A7680C]  - Kiem tra anten da gan chua?\n");
    app_log("[A7680C]  - SIM co ho tro vung nay khong?\n");
    app_log("[A7680C]  - Kiem tra vi tri co song yeu khong?\n");
    return SL_STATUS_FAIL;
  }

  // ── Buoc 7: Kiem tra nha mang + dat che do SMS ──
  app_log("[A7680C] [7/7] Kiem tra nha mang (AT+COPS?)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd_check("AT+COPS?", "+COPS:");

  app_log("[A7680C] Dat che do SMS text (AT+CMGF=1)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd_check("AT+CMGF=1", "OK");

  app_log("[A7680C] Dat bo ma GSM (AT+CSCS)...\n");
  a7680c_flush_rx();
  a7680c_send_cmd_check("AT+CSCS=\"GSM\"", "OK");

  // Xoa SMS cu tranh +SMS FULL
  a7680c_delete_all_sms();

  app_log("[A7680C] Init OK!\n");
  return SL_STATUS_OK;
}

// ── Gửi SMS ──
sl_status_t App_a7680c_send_sms(const char *phone_number,
                                  const char *message)
{
  if (phone_number == NULL || message == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  char cmd_buf[64] = { 0 };

  app_log("[A7680C] Gui SMS den: %s\n", phone_number);
  app_log("[A7680C] Noi dung: %s\n", message);

  a7680c_flush_rx();
  if (!a7680c_send_cmd_check("AT+CMGF=1", "OK")) {
    app_log("[A7680C] FAIL: Khong dat duoc che do SMS text!\n");
    return SL_STATUS_FAIL;
  }
  sl_sleeptimer_delay_millisecond(300);

  a7680c_flush_rx();
  if (!a7680c_send_cmd_check("AT+CSCS=\"GSM\"", "OK")) {
    app_log("[A7680C] WARN: Khong dat duoc bo ma GSM.\n");
  }
  sl_sleeptimer_delay_millisecond(300);

  a7680c_flush_rx();

  snprintf(cmd_buf, sizeof(cmd_buf), "AT+CMGS=\"%s\"", phone_number);
  if (!a7680c_send_cmd_check_timeout(cmd_buf, ">", 5000)) {
    app_log("[A7680C] FAIL: Khong nhan duoc dau \">\" tu module!\n");
    app_log("[A7680C]  - Co the chua co mang\n");
    app_log("[A7680C]  - SIM bi loi hoac het tien\n");
    app_log("[A7680C]  - Module bi treo, thu reset\n");
    return SL_STATUS_FAIL;
  }

  sl_sleeptimer_delay_millisecond(500);

  a7680c_send_data(message);

  // Xoa SMS vua gui tranh day SIM
  app_log("[A7680C] Xoa SMS da gui de giai phong bo nho SIM...\n");
  sl_sleeptimer_delay_millisecond(2000);
  a7680c_flush_rx();
  a7680c_send_cmd_check_timeout("AT+CMGD=1,4", "OK", 15000);

  app_log("[A7680C] SMS da gui xong!\n");
  return SL_STATUS_OK;
}














// ============================================================
// MQTT TEST - A7680C -> broker.hivemq.com
// ============================================================

#define MQTT_BROKER_HOST       "broker.hivemq.com"
#define MQTT_BROKER_PORT       1883
#define MQTT_CLIENT_ID         "BRD2608A_HealthMonitor_001"

#define MQTT_TEST_TOPIC        "health/test"
#define MQTT_TEST_PAYLOAD      "Hello MQTT"

static bool mqtt_started = false;
static bool mqtt_connected = false;


// ------------------------------------------------------------
// Start MQTT service
// ------------------------------------------------------------
static bool a7680c_mqtt_start(void)
{
  app_log("\n[A7680C][MQTT] Start MQTT service...\n");

  a7680c_flush_rx();

  if (!a7680c_send_cmd_check_timeout(
        "AT+CMQTTSTART",
        "+CMQTTSTART: 0",
        15000)) {

    app_log("[A7680C][MQTT] FAIL: CMQTTSTART\n");
    return false;
  }

  mqtt_started = true;

  app_log("[A7680C][MQTT] MQTT service STARTED\n");

  return true;
}


// ------------------------------------------------------------
// Acquire MQTT client
// ------------------------------------------------------------
static bool a7680c_mqtt_acquire(void)
{
  char cmd[128];

  app_log("[A7680C][MQTT] Acquire MQTT client...\n");

  a7680c_flush_rx();

  snprintf(cmd,
           sizeof(cmd),
           "AT+CMQTTACCQ=0,\"%s\",0",
           MQTT_CLIENT_ID);

  if (!a7680c_send_cmd_check_timeout(
        cmd,
        "OK",
        5000)) {

    app_log("[A7680C][MQTT] FAIL: CMQTTACCQ\n");
    return false;
  }

  app_log("[A7680C][MQTT] Client acquired\n");

  return true;
}


// ------------------------------------------------------------
// Connect to HiveMQ
// ------------------------------------------------------------
static bool a7680c_mqtt_connect(void)
{
  char cmd[128];

  app_log("[A7680C][MQTT] Connect HiveMQ...\n");

  a7680c_flush_rx();

  snprintf(cmd,
           sizeof(cmd),
           "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,0",
           MQTT_BROKER_HOST,
           MQTT_BROKER_PORT);

  if (!a7680c_send_cmd_check_timeout(
        cmd,
        "+CMQTTCONNECT: 0,0",
        30000)) {

    app_log("[A7680C][MQTT] FAIL: MQTT CONNECT\n");
    return false;
  }

  mqtt_connected = true;

  app_log("[A7680C][MQTT] CONNECTED TO HiveMQ\n");

  return true;
}


// ------------------------------------------------------------
// Publish MQTT message
// ------------------------------------------------------------
static bool a7680c_mqtt_publish(
    const char *topic,
    const char *payload)
{
  char cmd[256];

  if (!mqtt_connected) {
    app_log("[A7680C][MQTT] ERROR: MQTT not connected\n");
    return false;
  }

  // ----------------------------------------------------------
  // 1. Send topic length
  // ----------------------------------------------------------

  app_log("[A7680C][MQTT] Topic: %s\n", topic);

  a7680c_flush_rx();

  snprintf(cmd,
           sizeof(cmd),
           "AT+CMQTTTOPIC=0,%u",
           (unsigned int)strlen(topic));

  if (!a7680c_send_cmd_check_timeout(
        cmd,
        ">",
        5000)) {

    app_log("[A7680C][MQTT] FAIL: CMQTTTOPIC\n");
    return false;
  }

  // Send topic
  sl_iostream_write(
      sl_iostream_A7680C_handle,
      topic,
      strlen(topic));

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // 2. Send payload length
  // ----------------------------------------------------------

  app_log("[A7680C][MQTT] Payload: %s\n", payload);

  a7680c_flush_rx();

  snprintf(cmd,
           sizeof(cmd),
           "AT+CMQTTPAYLOAD=0,%u",
           (unsigned int)strlen(payload));

  if (!a7680c_send_cmd_check_timeout(
        cmd,
        ">",
        5000)) {

    app_log("[A7680C][MQTT] FAIL: CMQTTPAYLOAD\n");
    return false;
  }

  // Send payload
  sl_iostream_write(
      sl_iostream_A7680C_handle,
      payload,
      strlen(payload));

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // 3. Publish
  // ----------------------------------------------------------

  a7680c_flush_rx();

  if (!a7680c_send_cmd_check_timeout(
        "AT+CMQTTPUB=0,0,60",
        "+CMQTTPUB: 0,0",
        10000)) {

    app_log("[A7680C][MQTT] FAIL: CMQTTPUB\n");
    return false;
  }

  app_log("[A7680C][MQTT] PUBLISH SUCCESS\n");

  return true;
}


// ------------------------------------------------------------
// Full MQTT test
// ------------------------------------------------------------
sl_status_t App_a7680c_mqtt_test(void)
{
  app_log("\n");
  app_log("========================================\n");
  app_log(" A7680C MQTT TEST\n");
  app_log(" HiveMQ\n");
  app_log("========================================\n");

  // ----------------------------------------------------------
  // Make sure data attach
  // ----------------------------------------------------------

  app_log("[MQTT] Checking packet domain attach...\n");

  a7680c_flush_rx();

  if (!a7680c_send_cmd_check_timeout(
        "AT+CGATT=1",
        "OK",
        10000)) {

    app_log("[MQTT] FAIL: CGATT\n");
    return SL_STATUS_FAIL;
  }

  sl_sleeptimer_delay_millisecond(2000);


  // ----------------------------------------------------------
  // Start MQTT
  // ----------------------------------------------------------

  if (!a7680c_mqtt_start()) {
    return SL_STATUS_FAIL;
  }


  // ----------------------------------------------------------
  // Acquire client
  // ----------------------------------------------------------

  if (!a7680c_mqtt_acquire()) {
    return SL_STATUS_FAIL;
  }


  // ----------------------------------------------------------
  // Connect HiveMQ
  // ----------------------------------------------------------

  if (!a7680c_mqtt_connect()) {
    return SL_STATUS_FAIL;
  }


  // ----------------------------------------------------------
  // Publish test message
  // ----------------------------------------------------------

  if (!a7680c_mqtt_publish(
        MQTT_TEST_TOPIC,
        MQTT_TEST_PAYLOAD)) {

    return SL_STATUS_FAIL;
  }


  app_log("\n");
  app_log("========================================\n");
  app_log(" MQTT TEST SUCCESS\n");
  app_log(" Broker : %s\n", MQTT_BROKER_HOST);
  app_log(" Port   : %d\n", MQTT_BROKER_PORT);
  app_log(" Topic  : %s\n", MQTT_TEST_TOPIC);
  app_log(" Data   : %s\n", MQTT_TEST_PAYLOAD);
  app_log("========================================\n");

  return SL_STATUS_OK;
}

sl_status_t App_a7680c_mqtt_publish_health(
    float temperature,
    float humidity,
    int voc,
    int heart_rate,
    int spo2,
    float body_temperature,
    int co2,
    float pm2_5,
    int fall)
{
    char payload[512];

    snprintf(payload,
             sizeof(payload),
             "{\"temperature\":%.2f,"
             "\"humidity\":%.2f,"
             "\"voc\":%d,"
             "\"heart_rate\":%d,"
             "\"spo2\":%d,"
             "\"body_temperature\":%.2f,"
             "\"co2\":%d,"
             "\"pm2_5\":%.2f,"
             "\"fall\":%d}",
             temperature,
             humidity,
             voc,
             heart_rate,
             spo2,
             body_temperature,
             co2,
             pm2_5,
             fall);

    app_log("[MQTT] Health JSON: %s\n", payload);

    if (a7680c_mqtt_publish(MQTT_HEALTH_TOPIC, payload)) {
        app_log("[MQTT] Health data published OK\n");
        return SL_STATUS_OK;
    }

    app_log("[MQTT] Health data publish FAILED\n");
    return SL_STATUS_FAIL;
}









/*#include "App_a7680c.h"

#include "sl_iostream.h"
#include "sl_iostream_uart.h"
#include "sl_iostream_init_eusart_instances.h"
#include "sl_sleeptimer.h"
#include "app_log.h"

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// GENERAL CONFIG
// ============================================================

#define A7680C_CMD_DELAY_MS        100
#define A7680C_SMS_DELAY_MS        8000
#define A7680C_BOOT_DELAY_MS       5000

#define A7680C_RX_BUF_SIZE         256

#define A7680C_CLBS_TIMEOUT_MS     30000

#define MQTT_HEALTH_TOPIC          "health/data"

// ============================================================
// MQTT CONFIG
// ============================================================

#define MQTT_BROKER_HOST           "broker.hivemq.com"
#define MQTT_BROKER_PORT           1883

#define MQTT_CLIENT_ID             "BRD2608A"

#define MQTT_TEST_TOPIC            "health/test"
#define MQTT_TEST_PAYLOAD          "Hello MQTT"

// ============================================================
// MQTT STATE
// ============================================================

static bool mqtt_started   = false;
static bool mqtt_connected = false;


// ============================================================
// PARSE COORDINATE
// ============================================================

static coord_t parse_coord(const char *s)
{
  coord_t c = { 0, 0, false };

  if (s == NULL) {
    return c;
  }

  while (*s == ' ' || *s == '\r' || *s == '\n') {
    s++;
  }

  if (*s == '-') {
    c.negative = true;
    s++;
  }
  else if (*s == '+') {
    s++;
  }

  while (*s >= '0' && *s <= '9') {
    c.integer = c.integer * 10 + (*s - '0');
    s++;
  }

  if (*s == '.') {
    s++;

    uint32_t multiplier = 100000;

    while (*s >= '0' &&
           *s <= '9' &&
           multiplier > 0) {

      c.decimal +=
          (uint32_t)(*s - '0') * multiplier;

      multiplier /= 10;
      s++;
    }
  }

  return c;
}


// ============================================================
// COORDINATE TO STRING
// ============================================================

static void coord_to_str(coord_t c,
                         char *buf,
                         size_t buf_size)
{
  if (buf == NULL || buf_size == 0) {
    return;
  }

  if (c.negative) {

    snprintf(buf,
             buf_size,
             "-%ld.%06lu",
             (long)c.integer,
             (unsigned long)c.decimal);

  }
  else {

    snprintf(buf,
             buf_size,
             "%ld.%06lu",
             (long)c.integer,
             (unsigned long)c.decimal);
  }
}


// ============================================================
// READ RESPONSE ONLY
// ============================================================

static void a7680c_read_response(void)
{
  char buf[A7680C_RX_BUF_SIZE];

  memset(buf, 0, sizeof(buf));

  size_t total = 0;
  size_t bytes_read = 0;

  uint32_t idle_time = 0;

  while (idle_time < 3000) {

    bytes_read = 0;

    sl_iostream_read(
        sl_iostream_A7680C_handle,
        &buf[total],
        1,
        &bytes_read);

    if (bytes_read > 0) {

      if (total < sizeof(buf) - 1) {

        total++;
        buf[total] = '\0';
      }

      idle_time = 0;

    }
    else {

      sl_sleeptimer_delay_millisecond(10);

      idle_time += 10;
    }
  }

  buf[total] = '\0';

  if (total > 0) {

    app_log(
        "[A7680C] Response: %s\n",
        buf);

  }
  else {

    app_log(
        "[A7680C] Response: "
        "(KHONG CO PHAN HOI - TIMEOUT)\n");
  }
}




static void a7680c_flush_rx(void)
{
    char flush_buf[128];
    size_t bytes_read = 0;



    app_log("[A7680C] Flush RX...\n");

    while (1) {
        bytes_read = 0;

        sl_iostream_read(
            sl_iostream_A7680C_handle,
            flush_buf,
            sizeof(flush_buf),
            &bytes_read);

        if (bytes_read == 0) {
            break;
        }


    }


    sl_sleeptimer_delay_millisecond(20);


    while (1) {
        bytes_read = 0;

        sl_iostream_read(
            sl_iostream_A7680C_handle,
            flush_buf,
            sizeof(flush_buf),
            &bytes_read);

        if (bytes_read == 0) {
            break;
        }
    }

    app_log("[A7680C] RX flushed.\n");
}
// ============================================================
// SEND AT COMMAND + WAIT FOR EXPECT
//
// Rolling buffer.
//
// Không để URC làm đầy buffer và kết thúc vòng đọc.
// ============================================================

static bool a7680c_send_cmd_check_timeout(
    const char *cmd,
    const char *expect,
    uint32_t timeout_ms)
{
  char rx[256];
  memset(rx, 0, sizeof(rx));

  size_t rx_len = 0;
  size_t bytes_read = 0;

  uint64_t start_tick;
  uint64_t now_tick;
  uint64_t elapsed_ms;

  if (cmd == NULL) {
    return false;
  }

  app_log(
      "\n[A7680C] CMD: %s\n",
      cmd);

  // ==========================================================
  // SEND COMMAND
  // ==========================================================

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      cmd,
      strlen(cmd));

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "\r\n",
      2);

  // Cho modem bắt đầu xử lý
  sl_sleeptimer_delay_millisecond(
      A7680C_CMD_DELAY_MS);

  // ==========================================================
  // START REAL TIMEOUT
  // ==========================================================

  start_tick = sl_sleeptimer_get_tick_count64();

  // ==========================================================
  // READ RESPONSE
  // ==========================================================

  while (1) {

    // --------------------------------------------------------
    // REAL WALL-CLOCK TIMEOUT
    // --------------------------------------------------------

    now_tick = sl_sleeptimer_get_tick_count64();

    if (sl_sleeptimer_tick64_to_ms(
            now_tick - start_tick,
            &elapsed_ms) != SL_STATUS_OK) {

      elapsed_ms = timeout_ms;
    }

    if (elapsed_ms >= timeout_ms) {
      break;
    }

    // --------------------------------------------------------
    // READ ONE BYTE
    // --------------------------------------------------------

    char ch = 0;

    bytes_read = 0;

    sl_iostream_read(
        sl_iostream_A7680C_handle,
        &ch,
        1,
        &bytes_read);

    // --------------------------------------------------------
    // NO DATA
    // --------------------------------------------------------

    if (bytes_read == 0) {

      sl_sleeptimer_delay_millisecond(5);

      continue;
    }

    // --------------------------------------------------------
    // GOT DATA
    // --------------------------------------------------------

    app_log("%c", ch);

    // --------------------------------------------------------
    // ADD TO RX BUFFER
    // --------------------------------------------------------

    if (rx_len < sizeof(rx) - 1) {

      rx[rx_len++] = ch;
      rx[rx_len] = '\0';

    } else {



      memmove(
          rx,
          rx + 1,
          sizeof(rx) - 2);

      rx[sizeof(rx) - 2] = ch;
      rx[sizeof(rx) - 1] = '\0';
    }

    // ========================================================
    // CHECK ERROR
    // ========================================================

    if (strstr(rx, "\r\nERROR\r\n") != NULL ||
        strstr(rx, "\nERROR\r\n") != NULL ||
        strstr(rx, "+CME ERROR") != NULL) {

      app_log(
          "\n[A7680C] ERROR response for: %s\n",
          cmd);

      app_log(
          "[A7680C] RX BUFFER:\n%s\n",
          rx);

      return false;
    }

    // ========================================================
    // CHECK EXPECT
    // ========================================================

    if (expect != NULL &&
        strlen(expect) > 0) {




      if (strcmp(expect, "READY") == 0) {

        if (strstr(rx, "+CPIN: READY") != NULL) {

          app_log(
              "\n[A7680C] RESPONSE FOUND: +CPIN: READY\n");

          app_log(
              "[A7680C] RX BUFFER:\n%s\n",
              rx);

          return true;
        }
      }



      else {

        if (strstr(rx, expect) != NULL) {

          app_log(
              "\n[A7680C] RESPONSE FOUND: %s\n",
              expect);

          app_log(
              "[A7680C] RX BUFFER:\n%s\n",
              rx);

          return true;
        }
      }
    }
  }

  // ==========================================================
  // TIMEOUT
  // ==========================================================

  rx[sizeof(rx) - 1] = '\0';

  app_log(
      "\n[A7680C] TIMEOUT: %s\n",
      cmd);

  if (rx_len > 0) {

    app_log(
        "[A7680C] RX BUFFER:\n%s\n",
        rx);

  } else {

    app_log(
        "[A7680C] RX: NO DATA\n");
  }

  if (expect != NULL) {

    app_log(
        "[A7680C] EXPECT NOT FOUND: %s\n",
        expect);
  }

  return false;
}
// ============================================================
// DEFAULT COMMAND CHECK
// ============================================================

static bool a7680c_send_cmd_check(
    const char *cmd,
    const char *expect)
{
  return a7680c_send_cmd_check_timeout(
      cmd,
      expect,
      5000);
}


// ============================================================
// SEND COMMAND WITHOUT CHECK
// ============================================================

static void a7680c_send_cmd(const char *cmd)
{
  if (cmd == NULL) {
    return;
  }

  app_log(
      "[A7680C] CMD: %s\n",
      cmd);

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      cmd,
      strlen(cmd));

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "\r\n",
      2);

  sl_sleeptimer_delay_millisecond(
      A7680C_CMD_DELAY_MS);

  a7680c_read_response();
}


// ============================================================
// SEND SMS DATA + CTRL-Z
// ============================================================

static void a7680c_send_data(const char *data)
{
  if (data == NULL) {
    return;
  }

  app_log(
      "[A7680C] DATA: %s\n",
      data);

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      data,
      strlen(data));

  uint8_t ctrl_z = 0x1A;

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      &ctrl_z,
      1);

  sl_sleeptimer_delay_millisecond(
      A7680C_SMS_DELAY_MS);

  a7680c_read_response();
}


// ============================================================
// CHECK NETWORK REGISTRATION
// ============================================================

static bool a7680c_check_reg(
    const char *at_cmd,
    const char *label)
{
  char buf[A7680C_RX_BUF_SIZE];

  memset(buf, 0, sizeof(buf));

  size_t total = 0;
  size_t bytes_read = 0;

  uint32_t elapsed = 0;

  app_log(
      "[A7680C] CMD: %s\n",
      at_cmd);

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      at_cmd,
      strlen(at_cmd));

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "\r\n",
      2);

  sl_sleeptimer_delay_millisecond(300);

  while (elapsed < 3000) {

    char ch = 0;

    bytes_read = 0;

    sl_iostream_read(
        sl_iostream_A7680C_handle,
        &ch,
        1,
        &bytes_read);

    if (bytes_read > 0) {

      if (total < sizeof(buf) - 1) {

        buf[total++] = ch;

        buf[total] = '\0';
      }

      elapsed = 0;

    }
    else {

      sl_sleeptimer_delay_millisecond(10);

      elapsed += 10;
    }
  }

  buf[total] = '\0';

  app_log(
      "[A7680C] Response: %s\n",
      buf);

  bool ok = false;



  if (strstr(buf, ",1") != NULL ||
      strstr(buf, ",5") != NULL) {

    ok = true;
  }

  app_log(
      "[A7680C] %s: %s\n",
      label,
      ok
        ? "[OK] Da dang ky"
        : "[WARN] Chua dang ky");

  return ok;
}


// ============================================================
// CHECK PACKET ATTACH
//
// AT+CGATT?
//
// +CGATT: 1
// => attached
//
// +CGATT: 0
// => not attached
// ============================================================

static bool a7680c_check_packet_attach(void)
{
  char buf[128];

  memset(buf, 0, sizeof(buf));

  size_t total = 0;
  size_t bytes_read = 0;

  uint32_t elapsed = 0;

  app_log(
      "[LTE] Checking packet attach: AT+CGATT?\n");

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "AT+CGATT?",
      strlen("AT+CGATT?"));

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "\r\n",
      2);

  sl_sleeptimer_delay_millisecond(100);

  while (elapsed < 5000) {

    char ch = 0;

    bytes_read = 0;

    sl_iostream_read(
        sl_iostream_A7680C_handle,
        &ch,
        1,
        &bytes_read);

    if (bytes_read > 0) {

      if (total < sizeof(buf) - 1) {

        buf[total++] = ch;

        buf[total] = '\0';
      }

      elapsed = 0;

      // ------------------------------------------------------
      // ATTACHED
      // ------------------------------------------------------

      if (strstr(buf, "+CGATT: 1") != NULL) {

        app_log(
            "[LTE] CGATT STATUS = 1\n");

        return true;
      }

      // ------------------------------------------------------
      // NOT ATTACHED
      // ------------------------------------------------------

      if (strstr(buf, "+CGATT: 0") != NULL) {

        app_log(
            "[LTE] CGATT STATUS = 0\n");

        return false;
      }

    }
    else {

      sl_sleeptimer_delay_millisecond(10);

      elapsed += 10;
    }
  }

  app_log(
      "[LTE] AT+CGATT? TIMEOUT\n");

  if (total > 0) {

    app_log(
        "[LTE] Response: %s\n",
        buf);
  }

  return false;
}


// ============================================================
// DELETE ALL SMS
// ============================================================

static void a7680c_delete_all_sms(void)
{
  app_log(
      "[A7680C] Xoa toan bo SMS trong SIM "
      "(AT+CMGD=1,4)...\n");

  a7680c_flush_rx();

  if (a7680c_send_cmd_check_timeout(
          "AT+CMGD=1,4",
          "OK",
          10000)) {

    app_log(
        "[A7680C] [OK] Xoa SMS thanh cong\n");

  }
  else {

    app_log(
        "[A7680C] [WARN] Xoa SMS that bai\n");
  }

  sl_sleeptimer_delay_millisecond(1000);
}


// ============================================================
// CELL-ID LOCATION
// ============================================================

sl_status_t App_a7680c_get_cell_location(
    char *loc_buf,
    size_t buf_size)
{
  if (loc_buf == NULL ||
      buf_size == 0) {

    return SL_STATUS_INVALID_PARAMETER;
  }

  snprintf(
      loc_buf,
      buf_size,
      "Vi tri: Khong xac dinh");

  // ----------------------------------------------------------
  // DATA ATTACH
  // ----------------------------------------------------------

  app_log(
      "\n[A7680C] Dam bao data attach "
      "(AT+CGATT=1)...\n");

  a7680c_flush_rx();

  bool attach_command_ok =
      a7680c_send_cmd_check_timeout(
          "AT+CGATT=1",
          "OK",
          10000);

  if (!attach_command_ok) {

    app_log(
        "[A7680C] CGATT command khong tra OK.\n");


    a7680c_flush_rx();

    if (!a7680c_check_packet_attach()) {

      app_log(
          "[A7680C] [FAIL] Packet data chua attach.\n");

      return SL_STATUS_FAIL;
    }

    app_log(
        "[A7680C] Packet data da attach.\n");
  }

  sl_sleeptimer_delay_millisecond(2000);

  // ----------------------------------------------------------
  // CLBS
  // ----------------------------------------------------------

  char buf[A7680C_RX_BUF_SIZE];

  memset(buf, 0, sizeof(buf));

  size_t total = 0;
  size_t bytes_read = 0;

  uint32_t elapsed = 0;

  app_log(
      "[A7680C] Lay vi tri Cell-ID "
      "(AT+CLBS=1,1)...\n");

  a7680c_flush_rx();

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "AT+CLBS=1,1",
      strlen("AT+CLBS=1,1"));

  sl_iostream_write(
      sl_iostream_A7680C_handle,
      "\r\n",
      2);

  sl_sleeptimer_delay_millisecond(300);

  while (elapsed < A7680C_CLBS_TIMEOUT_MS) {

    char ch = 0;

    bytes_read = 0;

    sl_iostream_read(
        sl_iostream_A7680C_handle,
        &ch,
        1,
        &bytes_read);

    if (bytes_read > 0) {

      if (total < sizeof(buf) - 1) {

        buf[total++] = ch;

        buf[total] = '\0';

      }
      else {


      }

      elapsed = 0;

      if (strstr(buf, "ERROR") != NULL) {

        app_log(
            "[A7680C] Cell-ID response: %s\n",
            buf);

        app_log(
            "[A7680C] [FAIL] CLBS tra ve ERROR!\n");

        return SL_STATUS_FAIL;
      }

      if (strstr(buf, "+CLBS:") != NULL) {


        sl_sleeptimer_delay_millisecond(500);

        break;
      }

    }
    else {

      sl_sleeptimer_delay_millisecond(10);

      elapsed += 10;
    }
  }

  buf[total] = '\0';

  app_log(
      "[A7680C] Cell-ID response: %s\n",
      buf);

  char *clbs = strstr(
      buf,
      "+CLBS:");

  if (clbs == NULL) {

    app_log(
        "[A7680C] [WARN] Khong tim thay +CLBS "
        "trong response\n");

    return SL_STATUS_FAIL;
  }

  char *data = clbs + 6;

  while (*data == ' ') {
    data++;
  }

  int err = 0;

  char lat_str[20] = { 0 };
  char lon_str[20] = { 0 };

  int acc = 0;

  int parsed = sscanf(
      data,
      "%d,%19[^,],%19[^,],%d",
      &err,
      lat_str,
      lon_str,
      &acc);

  app_log(
      "[A7680C] CLBS parse: "
      "err=%d lat=[%s] lon=[%s] "
      "acc=%d parsed=%d\n",
      err,
      lat_str,
      lon_str,
      acc,
      parsed);

  if (parsed < 3) {

    app_log(
        "[A7680C] [FAIL] Parse CLBS that bai!\n");

    return SL_STATUS_FAIL;
  }

  if (err != 0) {

    app_log(
        "[A7680C] [FAIL] CLBS error code: %d\n",
        err);

    return SL_STATUS_FAIL;
  }

  coord_t lat = parse_coord(lat_str);
  coord_t lon = parse_coord(lon_str);

  char lat_buf[20] = { 0 };
  char lon_buf[20] = { 0 };

  coord_to_str(
      lat,
      lat_buf,
      sizeof(lat_buf));

  coord_to_str(
      lon,
      lon_buf,
      sizeof(lon_buf));

  app_log(
      "[A7680C] Latitude : %s\n",
      lat_buf);

  app_log(
      "[A7680C] Longitude: %s\n",
      lon_buf);

  app_log(
      "[A7680C] Do chinh xac: ~%d met\n",
      acc);

  snprintf(
      loc_buf,
      buf_size,
      "https://maps.google.com/?q=%s,%s",
      lat_buf,
      lon_buf);

  app_log(
      "[A7680C] [OK] Vi tri: %s\n",
      loc_buf);

  return SL_STATUS_OK;
}


// ============================================================
// A7680C INIT
// ============================================================

sl_status_t App_a7680c_init(void)
{
  app_log(
      "\n[A7680C] Initializing...\n");

  // ----------------------------------------------------------
  // BOOT DELAY
  // ----------------------------------------------------------

  sl_sleeptimer_delay_millisecond(
      A7680C_BOOT_DELAY_MS);

  app_log(
      "[A7680C] Flush URC sau boot...\n");

  a7680c_flush_rx();

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // STEP 1 - AT
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [1/7] Kiem tra AT "
      "(thu toi da 5 lan)...\n");

  bool at_ok = false;

  for (int i = 0; i < 5; i++) {

    app_log(
        "[A7680C] AT lan thu %d...\n",
        i + 1);

    a7680c_flush_rx();

    if (a7680c_send_cmd_check_timeout(
            "AT",
            "OK",
            5000)) {

      at_ok = true;

      break;
    }

    sl_sleeptimer_delay_millisecond(1000);
  }

  if (!at_ok) {

    app_log(
        "[A7680C] FAIL: Module khong phan hoi "
        "AT sau 5 lan thu!\n");

    app_log(
        "[A7680C] - Kiem tra UART TX/RX\n");

    app_log(
        "[A7680C] - Kiem tra nguon module\n");

    return SL_STATUS_FAIL;
  }

  // ----------------------------------------------------------
  // STEP 2 - ATE0
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [2/7] Tat echo (ATE0)...\n");

  a7680c_flush_rx();

  a7680c_send_cmd("ATE0");

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // STEP 3 - SIM
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [3/7] Kiem tra SIM "
      "(AT+CPIN?)...\n");

  bool sim_ok = false;

  for (int i = 0; i < 3; i++) {

    app_log(
        "[A7680C] CPIN lan thu %d...\n",
        i + 1);

    a7680c_flush_rx();

    sl_sleeptimer_delay_millisecond(300);

    if (a7680c_send_cmd_check_timeout(
            "AT+CPIN?",
            "READY",
            5000)) {

      sim_ok = true;

      break;
    }

    sl_sleeptimer_delay_millisecond(2000);
  }

  if (!sim_ok) {

    app_log(
        "[A7680C] FAIL: SIM chua san sang!\n");

    app_log(
        "[A7680C] - Kiem tra SIM\n");

    app_log(
        "[A7680C] - Kiem tra PIN SIM\n");

    return SL_STATUS_FAIL;
  }

  // ----------------------------------------------------------
  // STEP 4 - SIGNAL
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [4/7] Kiem tra tin hieu "
      "(AT+CSQ)...\n");

  a7680c_flush_rx();

  a7680c_send_cmd_check(
      "AT+CSQ",
      "+CSQ:");

  // ----------------------------------------------------------
  // STEP 5/6 - NETWORK
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [5-6/7] Kiem tra mang "
      "(thu toi da 10 lan)...\n");

  bool creg_ok = false;
  bool cereg_ok = false;

  for (int i = 0; i < 10; i++) {

    app_log(
        "[A7680C] Kiem tra mang lan thu %d...\n",
        i + 1);

    a7680c_flush_rx();

    creg_ok =
        a7680c_check_reg(
            "AT+CREG?",
            "CREG");

    a7680c_flush_rx();

    cereg_ok =
        a7680c_check_reg(
            "AT+CEREG?",
            "CEREG");



    if (cereg_ok) {
      break;
    }

    if (creg_ok) {
      break;
    }

    sl_sleeptimer_delay_millisecond(2000);
  }

  if (!creg_ok && !cereg_ok) {

    app_log(
        "[A7680C] FAIL: Khong co mang "
        "sau 10 lan thu!\n");

    app_log(
        "[A7680C] - Kiem tra anten\n");

    app_log(
        "[A7680C] - Kiem tra SIM\n");

    return SL_STATUS_FAIL;
  }

  // ----------------------------------------------------------
  // STEP 7 - OPERATOR
  // ----------------------------------------------------------

  app_log(
      "[A7680C] [7/7] Kiem tra nha mang "
      "(AT+COPS?)...\n");

  a7680c_flush_rx();

  a7680c_send_cmd_check(
      "AT+COPS?",
      "+COPS:");

  // ----------------------------------------------------------
  // SMS MODE
  // ----------------------------------------------------------

  app_log(
      "[A7680C] Dat che do SMS text "
      "(AT+CMGF=1)...\n");

  a7680c_flush_rx();

  a7680c_send_cmd_check(
      "AT+CMGF=1",
      "OK");

  app_log(
      "[A7680C] Dat bo ma GSM "
      "(AT+CSCS)...\n");

  a7680c_flush_rx();

  a7680c_send_cmd_check(
      "AT+CSCS=\"GSM\"",
      "OK");

  // ----------------------------------------------------------
  // DELETE SMS
  // ----------------------------------------------------------

  a7680c_delete_all_sms();

  app_log(
      "\n[A7680C] Init OK!\n");

  return SL_STATUS_OK;
}


// ============================================================
// SEND SMS
// ============================================================

sl_status_t App_a7680c_send_sms(
    const char *phone_number,
    const char *message)
{
  if (phone_number == NULL ||
      message == NULL) {

    return SL_STATUS_INVALID_PARAMETER;
  }

  char cmd_buf[64] = { 0 };

  app_log(
      "[A7680C] Gui SMS den: %s\n",
      phone_number);

  app_log(
      "[A7680C] Noi dung: %s\n",
      message);

  // ----------------------------------------------------------
  // SMS TEXT MODE
  // ----------------------------------------------------------

  a7680c_flush_rx();

  if (!a7680c_send_cmd_check(
          "AT+CMGF=1",
          "OK")) {

    app_log(
        "[A7680C] FAIL: Khong dat SMS text!\n");

    return SL_STATUS_FAIL;
  }

  sl_sleeptimer_delay_millisecond(300);

  // ----------------------------------------------------------
  // GSM CHARSET
  // ----------------------------------------------------------

  a7680c_flush_rx();

  if (!a7680c_send_cmd_check(
          "AT+CSCS=\"GSM\"",
          "OK")) {

    app_log(
        "[A7680C] WARN: Khong dat GSM charset.\n");
  }

  sl_sleeptimer_delay_millisecond(300);

  // ----------------------------------------------------------
  // CMGS
  // ----------------------------------------------------------

  a7680c_flush_rx();

  snprintf(
      cmd_buf,
      sizeof(cmd_buf),
      "AT+CMGS=\"%s\"",
      phone_number);

  if (!a7680c_send_cmd_check_timeout(
          cmd_buf,
          ">",
          5000)) {

    app_log(
        "[A7680C] FAIL: "
        "Khong nhan duoc dau \">\"!\n");

    return SL_STATUS_FAIL;
  }

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // SEND SMS DATA
  // ----------------------------------------------------------

  a7680c_send_data(message);

  // ----------------------------------------------------------
  // DELETE SMS
  // ----------------------------------------------------------

  app_log(
      "[A7680C] Xoa SMS vua gui...\n");

  sl_sleeptimer_delay_millisecond(2000);

  a7680c_flush_rx();

  a7680c_send_cmd_check_timeout(
      "AT+CMGD=1,4",
      "OK",
      15000);

  app_log(
      "[A7680C] SMS da gui xong!\n");

  return SL_STATUS_OK;
}


// ============================================================
// MQTT CLEANUP
//
// Dùng khi MQTT service/client cũ còn tồn tại.
// ============================================================

static void a7680c_mqtt_cleanup(void)
{
  app_log(
      "[MQTT] Cleanup old MQTT session...\n");

  mqtt_connected = false;

  // ----------------------------------------------------------
  // DISCONNECT
  // ----------------------------------------------------------

  a7680c_flush_rx();

  a7680c_send_cmd_check_timeout(
      "AT+CMQTTDISC=0,60",
      "+CMQTTDISC:",
      5000);

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // RELEASE CLIENT
  // ----------------------------------------------------------

  a7680c_flush_rx();

  a7680c_send_cmd_check_timeout(
      "AT+CMQTTREL=0",
      "OK",
      5000);

  sl_sleeptimer_delay_millisecond(500);

  // ----------------------------------------------------------
  // STOP MQTT SERVICE
  // ----------------------------------------------------------

  a7680c_flush_rx();

  a7680c_send_cmd_check_timeout(
      "AT+CMQTTSTOP",
      "+CMQTTSTOP: 0",
      12000);

  mqtt_started = false;

  sl_sleeptimer_delay_millisecond(1000);

  app_log(
      "[MQTT] Cleanup finished.\n");
}


// ============================================================
// MQTT START
// ============================================================

static bool a7680c_mqtt_start(void)
{
    app_log(
        "\n[A7680C][MQTT] Start MQTT service...\n");

    mqtt_started = false;
    mqtt_connected = false;

    // ==========================================================
    // CHỜ MODEM ỔN ĐỊNH SAU KHI ATTACH / NETWORK
    // ==========================================================

    app_log(
        "[MQTT] Waiting modem network to stabilize...\n");

    sl_sleeptimer_delay_millisecond(3000);

    // ==========================================================
    // FLUSH URC CŨ
    // ==========================================================

    a7680c_flush_rx();

    // ==========================================================
    // START MQTT
    // ==========================================================

    app_log(
        "[MQTT] Sending AT+CMQTTSTART...\n");

    if (a7680c_send_cmd_check_timeout(
            "AT+CMQTTSTART",
            "OK",
            30000)) {

        sl_sleeptimer_delay_millisecond(1500);

        a7680c_flush_rx();

        mqtt_started = true;

        app_log(
            "[A7680C][MQTT] "
            "MQTT START SUCCESS\n");

        return true;
    }

    // ==========================================================
    // FIRST START FAILED
    // ==========================================================

    app_log(
        "[A7680C][MQTT] "
        "START failed on first attempt.\n");

    app_log(
        "[MQTT] Wait 5 seconds before retry...\n");

    sl_sleeptimer_delay_millisecond(5000);

    a7680c_flush_rx();

    // ==========================================================
    // RETRY START
    // ==========================================================

    app_log(
        "[MQTT] Retry AT+CMQTTSTART...\n");

    if (a7680c_send_cmd_check_timeout(
            "AT+CMQTTSTART",
            "OK",
            30000)) {

        sl_sleeptimer_delay_millisecond(1500);

        a7680c_flush_rx();

        mqtt_started = true;

        app_log(
            "[A7680C][MQTT] "
            "MQTT START SUCCESS after retry\n");

        return true;
    }

    // ==========================================================
    // FINAL FAIL
    // ==========================================================

    app_log(
        "[A7680C][MQTT] "
        "MQTT START FAILED\n");

    mqtt_started = false;
    mqtt_connected = false;

    return false;
}


// ============================================================
// MQTT ACQUIRE CLIENT
// ============================================================

static bool a7680c_mqtt_acquire(void)
{
    char rx[256];
    memset(rx, 0, sizeof(rx));

    size_t total = 0;
    size_t bytes_read = 0;
    uint32_t elapsed = 0;

    app_log("\n========================================\n");
    app_log("[MQTT] CMQTTACCQ\n");
    app_log("========================================\n");

    // ========================================================
    // CHECK MQTT SERVICE
    // ========================================================

    if (!mqtt_started) {
        app_log("[MQTT] ERROR: MQTT service chua start\n");
        return false;
    }

    // ========================================================
    // KHONG GOI ATE0 O DAY
    //
    // ATE0 da duoc thuc hien trong qua trinh init A7680C.
    // Goi lai ATE0 tai day co the lam nhiễu UART do URC.
    // ========================================================

    // Don RX truoc khi gui CMQTTACCQ
    a7680c_flush_rx();

    // ========================================================
    // SEND CMQTTACCQ
    // ========================================================

    const char *cmd =
        "AT+CMQTTACCQ=0,\"BRD2608A\",0";

    app_log("[MQTT] SEND: %s\n", cmd);

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        cmd,
        strlen(cmd));

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        "\r\n",
        2);

    // ========================================================
    // READ RESPONSE
    // ========================================================

    while (elapsed < 10000) {

        char ch = 0;
        bytes_read = 0;

        sl_iostream_read(
            sl_iostream_A7680C_handle,
            &ch,
            1,
            &bytes_read);

        if (bytes_read > 0) {

            // ------------------------------------------------
            // SAVE RESPONSE
            // ------------------------------------------------

            if (total < sizeof(rx) - 1) {
                rx[total++] = ch;
                rx[total] = '\0';
            }

            // In response ra log
            app_log("%c", ch);

            // ------------------------------------------------
            // SUCCESS
            // ------------------------------------------------

            if (strstr(rx, "\r\nOK\r\n") != NULL ||
                strstr(rx, "\nOK\r\n") != NULL ||
                strstr(rx, "OK\r\n") != NULL) {

                app_log(
                    "\n[MQTT] CMQTTACCQ SUCCESS\n");

                app_log(
                    "[MQTT] RX = %s\n",
                    rx);

                return true;
            }

            // ------------------------------------------------
            // ERROR
            // ------------------------------------------------

            if (strstr(rx, "+CME ERROR") != NULL ||
                strstr(rx, "+CMQTTACCQ:") != NULL ||
                strstr(rx, "\r\nERROR\r\n") != NULL ||
                strstr(rx, "\nERROR\r\n") != NULL) {

                app_log(
                    "\n[MQTT] CMQTTACCQ ERROR\n");

                app_log(
                    "[MQTT] RX = %s\n",
                    rx);

                return false;
            }

        } else {

            sl_sleeptimer_delay_millisecond(10);
            elapsed += 10;
        }
    }

    // ========================================================
    // TIMEOUT
    // ========================================================

    rx[sizeof(rx) - 1] = '\0';

    app_log(
        "\n[MQTT] CMQTTACCQ TIMEOUT\n");

    app_log(
        "[MQTT] FINAL RX = %s\n",
        rx);

    return false;
}
// ============================================================
// MQTT CONNECT
// ============================================================

static bool a7680c_wait_ok(uint32_t timeout_ms)
{
    char rx[256];
    size_t total = 0;
    uint32_t elapsed = 0;

    memset(rx, 0, sizeof(rx));

    while (elapsed < timeout_ms) {

        char ch = 0;
        size_t bytes_read = 0;

        sl_iostream_read(
            sl_iostream_A7680C_handle,
            &ch,
            1,
            &bytes_read
        );

        if (bytes_read > 0) {

            app_log("%c", ch);

            if (total < sizeof(rx) - 1) {
                rx[total++] = ch;
                rx[total] = '\0';
            }


            if (strstr(rx, "\r\nOK\r\n") != NULL ||
                strstr(rx, "\nOK\r\n") != NULL ||
                strcmp(rx, "OK\r\n") == 0 ||
                strstr(rx, "\nOK\n") != NULL) {

                app_log("\n[MQTT] OK FOUND\n");

                return true;
            }

            if (strstr(rx, "ERROR") != NULL ||
                strstr(rx, "+CME ERROR") != NULL) {

                app_log("\n[MQTT] ERROR FOUND\n");
                app_log("[MQTT] RX = %s\n", rx);

                return false;
            }

        } else {

            sl_sleeptimer_delay_millisecond(10);
            elapsed += 10;
        }
    }

    app_log("\n[MQTT] WAIT OK TIMEOUT\n");
    app_log("[MQTT] RX = %s\n", rx);

    return false;
}


static bool a7680c_mqtt_connect(void)
{
  char cmd[160];

  if (!mqtt_started) {

    app_log(
        "[A7680C][MQTT] ERROR: "
        "MQTT service chua start\n");

    return false;
  }

  app_log(
      "[A7680C][MQTT] "
      "Connect HiveMQ...\n");

  a7680c_flush_rx();

  snprintf(
      cmd,
      sizeof(cmd),
      "AT+CMQTTCONNECT=0,"
      "\"tcp://%s:%d\",60,0",
      MQTT_BROKER_HOST,
      MQTT_BROKER_PORT);

  if (!a7680c_send_cmd_check_timeout(
          cmd,
          "+CMQTTCONNECT: 0,0",
          30000)) {

    app_log(
        "[A7680C][MQTT] "
        "FAIL: MQTT CONNECT\n");

    mqtt_connected = false;

    return false;
  }

  mqtt_connected = true;

  app_log(
      "[A7680C][MQTT] "
      "CONNECTED TO HiveMQ\n");

  return true;
}


// ============================================================
// MQTT PUBLISH
// ============================================================

static bool a7680c_mqtt_wait_prompt(uint32_t timeout_ms)
{
    char rx[256];
    size_t total = 0;
    size_t bytes_read = 0;

    uint64_t start_tick;
    uint64_t now_tick;
    uint64_t elapsed_ms;

    memset(rx, 0, sizeof(rx));

    app_log("[MQTT] Waiting for prompt > ...\n");

    start_tick = sl_sleeptimer_get_tick_count64();

    while (1) {

        now_tick = sl_sleeptimer_get_tick_count64();

        if (sl_sleeptimer_tick64_to_ms(
                now_tick - start_tick,
                &elapsed_ms) != SL_STATUS_OK) {

            elapsed_ms = timeout_ms;
        }

        if (elapsed_ms >= timeout_ms) {
            break;
        }

        char ch = 0;
        bytes_read = 0;

        sl_iostream_read(
            sl_iostream_A7680C_handle,
            &ch,
            1,
            &bytes_read);

        if (bytes_read == 0) {
            sl_sleeptimer_delay_millisecond(5);
            continue;
        }


        app_log("%c", ch);


        if (total < sizeof(rx) - 1) {

            rx[total++] = ch;
            rx[total] = '\0';

        } else {

            memmove(
                rx,
                rx + 1,
                sizeof(rx) - 2);

            rx[sizeof(rx) - 2] = ch;
            rx[sizeof(rx) - 1] = '\0';
        }


        if (ch == '>') {

            app_log(
                "\n[MQTT] PROMPT '>' FOUND\n");

            return true;
        }


        if (strstr(rx, "\r\nERROR\r\n") != NULL ||
            strstr(rx, "\nERROR\r\n") != NULL ||
            strstr(rx, "+CME ERROR") != NULL) {

            app_log(
                "\n[MQTT] PROMPT ERROR\n");

            app_log(
                "[MQTT] RX = %s\n",
                rx);

            return false;
        }
    }

    app_log(
        "\n[MQTT] PROMPT TIMEOUT\n");

    if (total > 0) {

        app_log(
            "[MQTT] RX = %s\n",
            rx);

    } else {

        app_log(
            "[MQTT] RX = NO DATA\n");
    }

    return false;
}


static bool a7680c_mqtt_publish(
    const char *topic,
    const char *payload
)
{

  if (!mqtt_connected) {
      app_log(
          "[MQTT] PUBLISH BLOCKED: "
          "MQTT chua connected.\n");

      return false;
  }


  if (topic == NULL || payload == NULL) {
      app_log(
          "[MQTT] PUBLISH BLOCKED: "
          "topic/payload NULL.\n");

      return false;
  }
    char cmd[128];
    size_t topic_len;
    size_t payload_len;

    topic_len = strlen(topic);
    payload_len = strlen(payload);

    app_log("\n========================================\n");
    app_log("[MQTT] PUBLISH\n");
    app_log("========================================\n");

    app_log("[MQTT] Topic   = %s\n", topic);
    app_log("[MQTT] Payload = %s\n", payload);




    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CMQTTTOPIC=0,%u",
        (unsigned int)topic_len
    );

    app_log("[MQTT] STEP 1: %s\n", cmd);


    a7680c_flush_rx();


    sl_iostream_write(
        sl_iostream_A7680C_handle,
        cmd,
        strlen(cmd)
    );

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        "\r\n",
        2
    );


    sl_sleeptimer_delay_millisecond(50);

    if (!a7680c_mqtt_wait_prompt(10000)) {

        app_log("[MQTT] FAIL: CMQTTTOPIC prompt\n");

        return false;
    }



    app_log("[MQTT] Sending topic: %s\n", topic);

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        topic,
        topic_len
    );


    sl_iostream_write(
        sl_iostream_A7680C_handle,
        "\r\n",
        2
    );


    if (!a7680c_wait_ok(10000)) {

        app_log("[MQTT] FAIL: CMQTTTOPIC data\n");

        return false;
    }

    app_log("[MQTT] CMQTTTOPIC OK\n");




    snprintf(
        cmd,
        sizeof(cmd),
        "AT+CMQTTPAYLOAD=0,%u",
        (unsigned int)payload_len
    );

    app_log("[MQTT] STEP 2: %s\n", cmd);


    a7680c_flush_rx();


    sl_iostream_write(
        sl_iostream_A7680C_handle,
        cmd,
        strlen(cmd)
    );

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        "\r\n",
        2
    );


    sl_sleeptimer_delay_millisecond(50);

    if (!a7680c_mqtt_wait_prompt(10000)) {

        app_log("[MQTT] FAIL: CMQTTPAYLOAD prompt\n");

        return false;
    }



    app_log("[MQTT] Sending payload...\n");

    sl_iostream_write(
        sl_iostream_A7680C_handle,
        payload,
        payload_len
    );


    sl_iostream_write(
        sl_iostream_A7680C_handle,
        "\r\n",
        2
    );


    if (!a7680c_wait_ok(10000)) {

        app_log("[MQTT] FAIL: CMQTTPAYLOAD data\n");

        return false;
    }

    app_log("[MQTT] CMQTTPAYLOAD OK\n");




    app_log("[MQTT] STEP 3: CMQTTPUB\n");

    if (!a7680c_send_cmd_check_timeout(
            "AT+CMQTTPUB=0,0,60,0",
            "+CMQTTPUB: 0,0",
            10000)) {

        app_log("[MQTT] FAIL: CMQTTPUB\n");

        return false;
    }
    sl_sleeptimer_delay_millisecond(500);



    app_log("\n========================================\n");
    app_log("[MQTT] PUBLISH SUCCESS!\n");
    app_log("========================================\n");

    return true;
}

// ============================================================
// MQTT TEST
// ============================================================

sl_status_t App_a7680c_mqtt_test(void)
{
  app_log("\n");

  app_log(
      "========================================\n");

  app_log(
      " A7680C MQTT TEST\n");

  app_log(
      " HiveMQ\n");

  app_log(
      "========================================\n");

  // ==========================================================
  // RESET STATE
  // ==========================================================

  mqtt_started = false;
  mqtt_connected = false;

  // ==========================================================
  // PACKET DATA ATTACH
  // ==========================================================

  app_log(
      "\n[MQTT] Checking packet domain attach...\n");

  a7680c_flush_rx();


  bool cgatt_command_ok =
      a7680c_send_cmd_check_timeout(
          "AT+CGATT=1",
          "OK",
          15000);

  if (cgatt_command_ok) {

    app_log(
        "[LTE] AT+CGATT=1 -> OK\n");

  }
  else {



    app_log(
        "[LTE] CGATT command "
        "did not return OK.\n");

    app_log(
        "[LTE] Checking actual "
        "attach state...\n");

    a7680c_flush_rx();

    if (!a7680c_check_packet_attach()) {

      app_log(
          "[LTE] Packet data "
          "is NOT attached.\n");

      app_log(
          "[MQTT] STOP: "
          "LTE data unavailable.\n");

      return SL_STATUS_FAIL;
    }

    app_log(
        "[LTE] Packet data "
        "is already attached.\n");
  }

  sl_sleeptimer_delay_millisecond(2000);

  // ==========================================================
  // START MQTT
  // ==========================================================

  if (!a7680c_mqtt_start()) {

    app_log(
        "[MQTT] FAIL: MQTT START\n");

    return SL_STATUS_FAIL;
  }

  // ==========================================================
  // ACQUIRE
  // ==========================================================

  if (!a7680c_mqtt_acquire()) {

    app_log(
        "[MQTT] FAIL: MQTT ACQUIRE\n");

    return SL_STATUS_FAIL;
  }

  // ==========================================================
  // CONNECT
  // ==========================================================

  if (!a7680c_mqtt_connect()) {

    app_log(
        "[MQTT] FAIL: MQTT CONNECT\n");

    return SL_STATUS_FAIL;
  }

  // ==========================================================
  // TEST PUBLISH
  // ==========================================================

  if (!a7680c_mqtt_publish(
          MQTT_TEST_TOPIC,
          MQTT_TEST_PAYLOAD)) {

    app_log(
        "[MQTT] FAIL: MQTT PUBLISH\n");

    return SL_STATUS_FAIL;
  }

  // ==========================================================
  // SUCCESS
  // ==========================================================

  app_log("\n");

  app_log(
      "========================================\n");

  app_log(
      " MQTT TEST SUCCESS\n");

  app_log(
      " Broker : %s\n",
      MQTT_BROKER_HOST);

  app_log(
      " Port   : %d\n",
      MQTT_BROKER_PORT);

  app_log(
      " Topic  : %s\n",
      MQTT_TEST_TOPIC);

  app_log(
      " Data   : %s\n",
      MQTT_TEST_PAYLOAD);

  app_log(
      "========================================\n");

  return SL_STATUS_OK;
}


// ============================================================
// MQTT HEALTH DATA
// ============================================================

sl_status_t App_a7680c_mqtt_publish_health(
    float temperature,
    float humidity,
    int voc,
    int heart_rate,
    int spo2,
    float body_temperature,
    int co2,
    float pm2_5,
    int fall)
{
  char payload[512];

  // ==========================================================
  // CREATE JSON
  // ==========================================================

  int len = snprintf(
      payload,
      sizeof(payload),

      "{"
      "\"temperature\":%.2f,"
      "\"humidity\":%.2f,"
      "\"voc\":%d,"
      "\"heart_rate\":%d,"
      "\"spo2\":%d,"
      "\"body_temperature\":%.2f,"
      "\"co2\":%d,"
      "\"pm2_5\":%.2f,"
      "\"fall\":%d"
      "}",

      temperature,
      humidity,
      voc,
      heart_rate,
      spo2,
      body_temperature,
      co2,
      pm2_5,
      fall);

  // ==========================================================
  // CHECK JSON BUFFER
  // ==========================================================

  if (len < 0 ||
      (size_t)len >= sizeof(payload)) {

    app_log(
        "[MQTT] ERROR: "
        "Health JSON too long\n");

    return SL_STATUS_FAIL;
  }

  app_log(
      "[MQTT] Health JSON: %s\n",
      payload);

  // ==========================================================
  // PUBLISH
  // ==========================================================

  if (a7680c_mqtt_publish(
          MQTT_HEALTH_TOPIC,
          payload)) {

    app_log(
        "[MQTT] "
        "Health data published OK\n");

    return SL_STATUS_OK;
  }

  app_log(
      "[MQTT] "
      "Health data publish FAILED\n");

  return SL_STATUS_FAIL;
}*/


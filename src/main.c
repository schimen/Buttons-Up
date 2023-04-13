#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/types.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#define SW0_NODE DT_ALIAS(sw0)
#define VOLUME_UP_KEY 0x80       // For iPhone and Android cameras
#define KEYBOARD_RETURN_KEY 0x58 // For some other android cameras

LOG_MODULE_REGISTER(buttons_up);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static void connected(struct bt_conn *conn, uint8_t err) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (err) {
    LOG_ERR("Failed to connect to %s (%u)", addr, err);
    return;
  }

  LOG_INF("Connected %s", addr);

  if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
    LOG_ERR("Failed to set security\n");
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  LOG_INF("Disconnected from %s (reason 0x%02x)", addr, reason);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  if (!err) {
    LOG_INF("Security changed: %s level %u", addr, level);
  } else {
    LOG_ERR("Security failed: %s level %u err %d", addr, level, err);
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

struct hids_info {
  uint16_t version; // Version number of USB HID specification
  uint16_t code;    // Country HID Device hardware is localized for
  uint8_t flags;
} __packed;

struct hids_report {
  uint8_t id;   // Report ID
  uint8_t type; // Report Type
} __packed;

static struct hids_info info = {
    .version = 0x0111,
    .code = 0x0002,
    .flags = BIT(1), // Normally Connectable
};

static struct hids_report input = {
    .id = 0x01,
    .type = 0x01, // Input
};

static uint8_t report_map[] = {
    0x05, 0x01, // Usage Page (Generic Desktop)
    0x09, 0x06, // Usage (Keyboard)
    0xA1, 0x01, // Collection (Application)
    // Modifier byte
    0x05, 0x07, //   Usage Page (Key Codes)
    0x19, 0xE0, //   Usage Minimum (Keyboard Left Control)
    0x29, 0xE7, //   Usage Maximum (Keyboard Right GUI)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x08, //   Report Count (8)
    0x81, 0x02, //   Input (Data, Variable, Absolute)
    // Reserved byte
    0x95, 0x01, //   Report Count (1)
    0x75, 0x08, //   Report Size (8)
    0x81, 0x01, //   Input (Constant)
    // Key array (2 bytes)
    0x95, 0x02, //   Report Count (2)
    0x75, 0x08, //   Report Size (8)
    0x15, 0x00, //   Logical Minimum (No event indicated)
    0x25, 0x81, //   Logical Maximum (Keyboard Volume Down)
    0x05, 0x07, //   Usage Page (Key Codes)
    0x19, 0x00, //   Usage Minimum (No event indicated)
    0x29, 0x81, //   Usage Maximum (Keyboard Volume Down)
    0x81, 0x00, //   Input (Data, Array)
    0xC0,       // End collection
};

uint8_t notify_enabled = 0;

static ssize_t read_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
                           sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr, void *buf,
                           uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                           sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
  notify_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t read_input_report(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

// Define HID service
BT_GATT_SERVICE_DEFINE(
    button_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_info, NULL, &info),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_report_map, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_input_report, NULL,
                           NULL),
    BT_GATT_CCC(input_ccc_changed,
                BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ, read_report,
                       NULL, &input));

static void bt_ready(int err) {
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }
  err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
  if (err) {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return;
  }
}

void button_loop() {
  const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
  gpio_pin_configure_dt(&sw0, GPIO_INPUT);

  for (;;) {
    if (notify_enabled == 1) {
      uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0};
      if (gpio_pin_get_dt(&sw0)) {
        report[2] = VOLUME_UP_KEY;
        report[3] = KEYBOARD_RETURN_KEY;
      }
      bt_gatt_notify(NULL, &button_service.attrs[5], report, sizeof(report));
    }
    k_msleep(100);
  }
}

void main(void) {
  int err = bt_enable(bt_ready);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)\n", err);
    return;
  }
  button_loop();
}

#ifndef HIDS_BUTTON
#define HIDS_BUTTON

#include <zephyr/kernel.h>
#include <stdbool.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

 // Key for taking picture on iPhone and Android cameras
#define VOLUME_DOWN_KEY 0x81

struct hids_info {
  uint16_t version; // Version number of USB HID specification
  uint16_t code;    // Country HID Device hardware is localized for
  uint8_t flags;
} __packed;

struct hids_report {
  uint8_t id;   // Report ID
  uint8_t type; // Report Type
} __packed;

int notify_volume_down(struct bt_conn *conn);

int notify_no_key(struct bt_conn *conn);

bool notify_enabled();

#endif
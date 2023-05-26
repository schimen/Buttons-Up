#include "hids_button.h"

static struct hids_info info = {
    .version = 0x0111,
    .code = 0x0002,
    .flags = BIT(1), // Normally Connectable
};

static struct hids_report input = {
    .id = 0x01,
    .type = 0x01, // Input
};

static uint8_t ctrl_point;

static const uint8_t report_map[] = {
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

static bool notify_enabled_internal = false;

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

static ssize_t read_input_report(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr, void *buf,
                                 uint16_t len, uint16_t offset) {
  return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled_internal = value == BT_GATT_CCC_NOTIFY;
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;
	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	memcpy(value + offset, buf, len);
	return len;
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
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ, read_report,
                       NULL, &input),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point));


int notify_volume_down(struct bt_conn *conn) {
    const uint8_t report[] = {0, 0, VOLUME_DOWN_KEY, 0, 0, 0, 0, 0};
    return bt_gatt_notify(conn, &button_service.attrs[5], report, sizeof(report));
}

int notify_no_key(struct bt_conn *conn) {
    const uint8_t report[8] = { 0 };
    return bt_gatt_notify(conn, &button_service.attrs[5], report, sizeof(report));
}

bool notify_enabled() {
    return notify_enabled_internal;
}


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

#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <hal/nrf_gpio.h>

#include "hids_button.h"

#define BUTTON_TIMEOUT 2000

LOG_MODULE_REGISTER(buttons_up);

const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

const struct bt_data ad[] = {
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

void turn_system_off() {
  // Blink led
  gpio_pin_set_dt(&led0, 1);
  for (int i = 0; i < 5; i++) {
    k_msleep(100);
    gpio_pin_toggle_dt(&led0);
  }
  // Configure to generate PORT event (wakeup) on button press
	nrf_gpio_cfg_input(NRF_DT_GPIOS_TO_PSEL(DT_ALIAS(sw0), gpios),
			   NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_sense_set(NRF_DT_GPIOS_TO_PSEL(DT_ALIAS(sw0), gpios),
			       NRF_GPIO_PIN_SENSE_LOW);
  // Turn off
  pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
  k_msleep(1000); // 1 Second delay for turning off
}

void button_loop() {
  int64_t uptime = k_uptime_get();
  for (;;) {
    if (notify_enabled()) {
      if (gpio_pin_get_dt(&sw0)) {
        gpio_pin_set_dt(&led0, 1);
        if (k_uptime_get() - uptime > BUTTON_TIMEOUT) {
          break;
        }
        notify_volume_down(NULL);
      }
      else {
        uptime = k_uptime_get();
        notify_no_key(NULL);
      }
    }
    k_msleep(100);
    gpio_pin_set_dt(&led0, 0);
  }
  turn_system_off();
}

int gpio_init() {
  int err;
  err = gpio_pin_configure_dt(&sw0, GPIO_INPUT);
  if (err) {
    LOG_ERR("Error %d: failed to configure %s pin %d", err, sw0.port->name, sw0.pin);
    return err;
  }
  // Init led0
  err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
  if (err) {
    LOG_ERR(
      "Error %d: failed to configure %s pin %d",
      err, led0.port->name, led0.pin
    );
  }
  return err;
}

void main(void) {
  int err = bt_enable(bt_ready);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)\n", err);
    return;
  }
  gpio_init();
  button_loop();
}

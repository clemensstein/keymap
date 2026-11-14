/*
 * Toucan2 (linke Haelfte): 2M-Funkmodus nur zur rechten Haelfte.
 *
 * Die Verbindung zum PC soll auf 1M bleiben, weil 2M mit dem Intel/Realtek-
 * Bluetooth-Chip unter Windows Probleme gemacht hat. Zwischen den Haelften
 * (beide nRF52840) halbiert 2M die Funkzeit pro Paket, dadurch fallen
 * seltener Funkfenster aus.
 *
 * ZMK erzwingt CONFIG_BT_AUTO_PHY_UPDATE (select in app/src/split/Kconfig).
 * Zephyr fordert damit direkt nach dem Verbindungsaufbau auf jeder Verbindung
 * 2M an, auch zum PC. Deshalb wird hier nach jedem PHY-Wechsel nachgesteuert:
 * PC-Verbindung zurueck auf 1M, Verbindung zur rechten Haelfte auf 2M.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Hoechstens so viele Korrekturen pro Verbindung, falls die Gegenseite ablehnt */
#define MAX_PHY_FIXES 3

static uint8_t phy_fixes[CONFIG_BT_MAX_CONN];

static bool is_split_link(struct bt_conn *conn) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) != 0 || info.type != BT_CONN_TYPE_LE) {
        return false;
    }
    /* Central = Verbindung zur rechten Haelfte, Peripheral = Verbindung zum PC */
    return info.role == BT_CONN_ROLE_CENTRAL;
}

static void request_phy(struct bt_conn *conn, bool split) {
    uint8_t *fixes = &phy_fixes[bt_conn_index(conn)];

    if (*fixes >= MAX_PHY_FIXES) {
        return;
    }
    (*fixes)++;

    int ret = bt_conn_le_phy_update(conn, split ? BT_CONN_LE_PHY_PARAM_2M
                                                : BT_CONN_LE_PHY_PARAM_1M);
    if (ret) {
        LOG_WRN("PHY-Update (%s) fehlgeschlagen: %d", split ? "Split 2M" : "PC 1M", ret);
    }
}

static void toucan_phy_connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        return;
    }
    phy_fixes[bt_conn_index(conn)] = 0;
    /* Bevorzugung je Verbindung setzen; das automatische 2M von Zephyr folgt
     * danach und wird in toucan_phy_updated() korrigiert. */
    request_phy(conn, is_split_link(conn));
}

static void toucan_phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *phy) {
    bool split = is_split_link(conn);
    uint8_t want = split ? BT_GAP_LE_PHY_2M : BT_GAP_LE_PHY_1M;

    LOG_INF("PHY %s: tx %u rx %u", split ? "Split" : "PC", phy->tx_phy, phy->rx_phy);
    if (phy->tx_phy != want || phy->rx_phy != want) {
        request_phy(conn, split);
    }
}

BT_CONN_CB_DEFINE(toucan_phy_conn_cb) = {
    .connected = toucan_phy_connected,
    .le_phy_updated = toucan_phy_updated,
};

/*
 * Standard-PHY 1M fuer vom PC angestossene Wechsel. Laeuft nach zmk_ble_init
 * (CONFIG_ZMK_BLE_INIT_PRIORITY = 50), das Bluetooth synchron startet.
 */
static int toucan_phy_default_1m(void) {
    if (!bt_is_ready()) {
        LOG_WRN("Bluetooth nicht bereit, Standard-PHY bleibt unveraendert");
        return 0;
    }

    struct net_buf *buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_DEFAULT_PHY,
                                            sizeof(struct bt_hci_cp_le_set_default_phy));
    if (buf == NULL) {
        LOG_WRN("Kein HCI-Puffer fuer Standard-PHY");
        return 0;
    }

    struct bt_hci_cp_le_set_default_phy *cp = net_buf_add(buf, sizeof(*cp));
    cp->all_phys = 0;
    cp->tx_phys = BT_HCI_LE_PHY_PREFER_1M;
    cp->rx_phys = BT_HCI_LE_PHY_PREFER_1M;

    int ret = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_DEFAULT_PHY, buf, NULL);
    if (ret) {
        LOG_WRN("Standard-PHY 1M setzen fehlgeschlagen: %d", ret);
    }
    return 0;
}

SYS_INIT(toucan_phy_default_1m, APPLICATION, 60);

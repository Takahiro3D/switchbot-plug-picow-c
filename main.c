/**
 * switchbot_ble_scan - SwitchBot Plug Mini BLE Advertisement Scanner
 *
 * Pico W (BTstack) でプラグミニのアドバタイズパケットをパッシブスキャンし、
 * USB CDC シリアルに消費電力等を出力する。
 *
 * Wi-Fi は一切使用しない（BLE専用動作）。
 *
 * 出力例:
 *   [SCAN] MAC=XX:XX:XX:XX:XX:XX RSSI=-55 power=12.3W ON  overload=0
 *   [RAW ] mfr_data[14]: 69 09 XX XX XX XX XX XX 01 6A 80 00 00 7B ...
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

// ----------------------------------------------------------------
// フィルタ設定
// ----------------------------------------------------------------

#define DEBUG_ENABLE    0  // 1: 詳細ログ出力, 0: 消費電力データのみ出力

// 特定デバイスのみ表示したい場合は MAC を設定（すべてゼロ = フィルタなし）
// 例: static const uint8_t TARGET_MAC[6] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
static const uint8_t TARGET_MAC[6] = {0x0C, 0x4E, 0xA0, 0x36, 0x08, 0x46};
#define FILTER_BY_MAC   1   // 1にすると TARGET_MAC のみ表示

#define ADTYPE_MANUFACTURER_SPECIFIC  0xFF

// SwitchBot社のCompany ID (Bluetooth SIG登録値 0x0969)
#define SWITCHBOT_COMPANY_ID  0x0969

// PlugMini のDevice Type バイト値
#define PLUGMINI_JP  0x6A   // 'j'
#define PLUGMINI_US  0x67   // 'g'

// ----------------------------------------------------------------
// BTstack コールバック
// ----------------------------------------------------------------

static btstack_packet_callback_registration_t hci_event_callback_registration;

// ----------------------------------------------------------------
// AD Structure の抽出
// ----------------------------------------------------------------

/**
 * Manufacturer Specific Data (Type=0xFF) を探してポインタを返す。
 * 見つからなければ NULL を返す。
 *
 * BLE アドバタイズパケットは複数の AD Structure の連結:
 *   [length][type][data ...]
 * length は type+data のバイト数。
 */
static inline const uint8_t *find_mfr_data(const uint8_t *adv_data, uint8_t adv_len,
                                     uint8_t *out_mfr_len) {
    uint8_t pos = 0;
    while (pos < adv_len) {
        uint8_t ad_len  = adv_data[pos];
        if (ad_len == 0 || pos + ad_len >= adv_len) break;
        uint8_t ad_type = adv_data[pos + 1];
        if (ad_type == ADTYPE_MANUFACTURER_SPECIFIC) {
            *out_mfr_len = ad_len - 1;            // type バイトを除いたデータ長
            return &adv_data[pos + 2];             // data の先頭
        }
        pos += 1 + ad_len;
    }
    return NULL;
}

/**
 * Manufacturer Specific Data をパースしてシリアル出力する。
 *
 * SwitchBot プラグミニ のフォーマット（公式BLE API仕様より）:
 *   Byte 0-1: Company ID (big-endian, 0x0969)
 *   Byte 2-7: BLE MAC (big-endian, 6 bytes)
 *   Byte 8  : シーケンス番号 (1~255)
 *   Byte 9  : Device state (0x00 - power off 0x80 - power on)
 *   Byte 10 : ステータスフラグ
 *               bit0: delay
 *               bit1: timer
 *               bit2: syncUtcTime
 *   Byte 11 : rssi signal wifi
 *   Byte 12 : 消費電力 上位バイト (MSB, bit7=overload flag(15A超), bit6-0=有効)
 *   Byte 13 : 消費電力 下位バイト (LSB)
 *   → currentPower(W) = ((data[12] & 0x7F) << 8 | data[13]) / 10.0
 */
static inline void parse_and_print(const uint8_t *mac, int8_t rssi,
                             const uint8_t *mfr, uint8_t mfr_len) {
    static uint8_t last_sequence = 0x00;  // 最初はどのシーケンスとも異なる値

    if (mfr_len < 4) return;

    // Company ID チェック (big-endian)
    uint16_t company_id = ((uint16_t)mfr[1] << 8) | mfr[0];
    if (company_id != SWITCHBOT_COMPANY_ID) return;

#if DEBUG_ENABLE
    // SwitchBot製品のRAWダンプを全出力（オフセット調査用）
    printf("[RAW MFR] MAC=%02X:%02X:%02X:%02X:%02X:%02X len=%d:",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mfr_len);
    for (int i = 0; i < mfr_len; i++) {
        printf(" %02X", mfr[i]);
    }
    printf("\n");
#endif

    if (mfr_len == 14) {
        const uint8_t sequence = mfr[8];
        if(sequence != last_sequence) {
            last_sequence = sequence;
            const uint16_t power_raw = ((mfr[12] & 0x7F) << 8) | mfr[13];
            printf("Sequence: 0x%02X, Power: %.1f W\n", sequence, (float)power_raw / 10.0);
        }
    }
}



/**
 * BTstack HCI イベントハンドラ
 * - BTSTACK_EVENT_STATE: BTstack起動完了 → スキャン開始
 * - GAP_EVENT_ADVERTISING_REPORT: アドバタイズ受信 → パース・出力
 */
static inline void hci_packet_handler(uint8_t packet_type, uint16_t channel,
                                uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {

    case BTSTACK_EVENT_STATE:
        // BTstack が HCI_STATE_WORKING になったらスキャン開始
        // ※ hci_power_control(HCI_POWER_ON) の後、非同期でここに来る
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
        printf("[INFO] BTstack up, starting passive scan...\n");

        // 第1引数: 0 = passive scan（SCAN_REQを送らない）
        //          1 = active scan （SCAN_REQを送りSCAN_RSPも受信する）
        // SwitchBotプラグミニはアドバタイズ本体に電力値を含むのでpassiveで十分
        gap_set_scan_parameters(0,      // 0=passive, 1=active
                                0x30,   // scan interval (×0.625ms = 30ms)
                                0x30);  // scan window   (×0.625ms = 30ms)
        gap_start_scan();
        break;

    case GAP_EVENT_ADVERTISING_REPORT: {
        static uint32_t total_adv = 0;
        total_adv++;

        // アドバタイズレポートから各フィールドを取得
        bd_addr_t addr;
        gap_event_advertising_report_get_address(packet, addr);
#if FILTER_BY_MAC
        if (memcmp(addr, TARGET_MAC, 6) != 0) break;
#endif

        int8_t rssi = (int8_t)gap_event_advertising_report_get_rssi(packet);

        uint8_t        adv_len  = gap_event_advertising_report_get_data_length(packet);
        const uint8_t *adv_data = gap_event_advertising_report_get_data(packet);

#if DEBUG_ENABLE
        printf("[DBG] total_adv=%lu latest MAC=%02X:%02X:%02X:%02X:%02X:%02X RSSI=%d len=%d:",
               total_adv,
               addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
               rssi, adv_len);
        for (int i = 0; i < adv_len; i++) {
            printf(" %02X", adv_data[i]);
        }
        printf("\n");
#endif
        // Manufacturer Specific Data を探す
        uint8_t        mfr_len = 0;
        const uint8_t *mfr     = find_mfr_data(adv_data, adv_len, &mfr_len);
        if (!mfr) break;

        parse_and_print(addr, rssi, mfr, mfr_len);
        break;
    }

    default:
        break;
    }
}

// ----------------------------------------------------------------
// エントリポイント
// ----------------------------------------------------------------

int main(void) {
    stdio_init_all();
    sleep_ms(2000);  // USB CDC シリアル接続待ち

    printf("=== SwitchBot Plug Mini BLE Scanner (Pico W) ===\n");
    printf("Scanning for advertisements...\n\n");

    // CYW43 (Wi-Fi/BT チップ) を BLE 専用モードで初期化
    // btstack_cyw43_init() は内部で cyw43_arch_init() も兼ねる
    if (cyw43_arch_init()) {
        printf("[ERROR] CYW43 init failed\n");
        return 1;
    }

    // BTstack のイベントループ設定
    l2cap_init();
    sm_init();

    // HCI パケットハンドラ登録
    // スキャン開始は BTSTACK_EVENT_STATE (HCI_STATE_WORKING) のタイミングで行う
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // BT 起動（非同期。起動完了は BTSTACK_EVENT_STATE イベントで通知される）
    hci_power_control(HCI_POWER_ON);

    // BTstack のイベントループ（ここから戻らない）
    btstack_run_loop_execute();

    return 0;
}
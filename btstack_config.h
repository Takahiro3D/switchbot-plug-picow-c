/**
 * btstack_config.h
 * BTstack のコンパイル時設定。
 *
 * pico_btstack_ble をリンクすると ENABLE_BLE が自動定義される。
 * それ以外に BTstack の各モジュールが要求する定義をここで補完する。
 * パッシブスキャン専用のため、接続・ペアリング関連は最小値に設定。
 */

#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// -------------------------------------------------------
// LE ロール設定
// hci.c が両ロールを想定したコードをコンパイルするため
// PERIPHERAL も定義しないとビルドエラーになる（SDK 2.2.0）
// -------------------------------------------------------
#define ENABLE_LE_CENTRAL           // スキャン（セントラル）動作
#define ENABLE_LE_PERIPHERAL        // hci.c のコンパイルエラー回避に必要

// -------------------------------------------------------
// ATT データベース (att_db_util.c が要求)
// HAVE_MALLOC を定義するか MAX_ATT_DB_SIZE を定義する。
// GATT サーバーは使わないが定義だけ必要。
// -------------------------------------------------------
#define MAX_ATT_DB_SIZE             512

// -------------------------------------------------------
// LE デバイスDB (le_device_db_memory.c が要求)
// ペアリングしないので 1 で十分。
// -------------------------------------------------------
#define MAX_NR_LE_DEVICE_DB_ENTRIES     1

// -------------------------------------------------------
// LE デバイスDB TLV (le_device_db_tlv.c が要求)
// Flash への永続化。使わないが定義は必要。
// -------------------------------------------------------
#define NVM_NUM_DEVICE_DB_ENTRIES       1

// -------------------------------------------------------
// セキュリティマネージャ (sm.c)
// -------------------------------------------------------
#define MAX_NR_SM_LOOKUP_ENTRIES        1

// -------------------------------------------------------
// HCI / L2CAP バッファ
// -------------------------------------------------------
#define HCI_ACL_PAYLOAD_SIZE            255
#define MAX_NR_CONTROLLER_ACL_BUFFERS   4
#define MAX_NR_LE_CONNECTIONS           1

// -------------------------------------------------------
// CYW43 HCI トランスポート固有設定 (btstack_hci_transport_cyw43.c が要求)
// HCI_OUTGOING_PRE_BUFFER_SIZE >= 4 (CYW43_PACKET_HEADER_SIZE)
// HCI_ACL_CHUNK_SIZE_ALIGNMENT は4の倍数
// -------------------------------------------------------
#define HCI_OUTGOING_PRE_BUFFER_SIZE    4
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT    4

// -------------------------------------------------------
// HCI ダンプ (hci_dump_embedded_stdout.c が要求)
// -------------------------------------------------------
#define ENABLE_PRINTF_HEXDUMP

// -------------------------------------------------------
// ログ出力
// -------------------------------------------------------
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
// #define ENABLE_LOG_DEBUG  // 詳細すぎる場合はコメントアウトのまま

#endif /* BTSTACK_CONFIG_H */
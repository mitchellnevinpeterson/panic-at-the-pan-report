#ifndef PEER_LINK_H
#define PEER_LINK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PEER_TIMEOUT_MS 600u
#define PEER_SYNC_TIMEOUT_MS 1200u
#define PEER_EVENT_CAPACITY 8u
typedef enum { PEER_CONTROLLER, PEER_SENSOR } PeerRole;
typedef enum { PEER_INPUT, PEER_SENSOR_READING, PEER_CONTROL, PEER_STATUS } PeerEventKind;
/* Controller -> sensor, repeated in every heartbeat. audio_enabled = sizzle on,
 * audio_zone = CookZone_t (0..3). Final-game LCD fields: lcd_mode = CookLcdMode_t
 * (0 attract, 1 waiting, 2 zone), burnt, burn_seq (0..3, change = burnt cue),
 * target_cm (0 = none), stirs (0..255) and flips (0..63, larger values are sent
 * as 63): the current ingredient's counts for the LCD. */
typedef struct {
    bool lamp; uint8_t audio_zone; bool audio_enabled; bool start_lamp;
    uint8_t lcd_mode; bool burnt; uint8_t burn_seq; uint8_t target_cm;
    uint8_t stirs, flips;
} PeerControl;
typedef struct {
    PeerEventKind kind;
    uint32_t epoch, time_ms;
    uint8_t input_id;
    uint16_t distance_mm;
    bool valid, synchronized;
    PeerControl control;
} PeerEvent;
typedef uint32_t (*PeerNowFn)(void *);
/* Must atomically enqueue the entire frame or return false without sending bytes. */
typedef bool (*PeerSendFn)(const uint8_t *, size_t, void *);
typedef void (*PeerEventFn)(const PeerEvent *, void *);
typedef struct { uint32_t time, born; uint16_t seq; uint8_t id; } PeerPending;
typedef struct {
    PeerRole role;
    PeerNowFn now;
    PeerSendFn send;
    PeerEventFn event;
    void *user;
    uint32_t epoch, last_rx, last_tx, map_local, map_remote, last_retry;
    uint32_t challenge, faults, ready_after, input_after;
    uint32_t valid_frames, bad_frames; /* Complete-frame CRC/version diagnostics. */
    uint16_t next_seq, received_seq;
    uint8_t rx[23], rx_count, count, head;
    bool synchronized, have_epoch, escaping, discard, ready_seen;
    PeerControl control;
    PeerPending pending[PEER_EVENT_CAPACITY];
} PeerLink;
void PeerLink_Init(PeerLink *, PeerRole, uint32_t initial_epoch,
                   PeerNowFn, PeerSendFn, PeerEventFn, void *);
void PeerLink_ReceiveByte(PeerLink *, uint8_t);
void PeerLink_Service(PeerLink *);
bool PeerLink_IsSynchronized(const PeerLink *);
/* Controller only: use at every round boundary, before admitting new input. */
void PeerLink_ResetSession(PeerLink *);
/* RX overflow, peripheral failure, or application queue overflow: fail closed. */
void PeerLink_Fault(PeerLink *);
void PeerLink_SendControl(PeerLink *, const PeerControl *);
bool PeerLink_SendSensor(PeerLink *, uint16_t mm, bool valid, uint32_t local_detection_ms);
/* Input IDs: 0..4 ingredient ON, 5 Finish, 6 pan-flip limit switch, 7 Start;
 * 8..12 ingredient OFF (activity only). */
bool PeerLink_SendInput(PeerLink *, uint8_t input_id, uint32_t local_detection_ms);
#endif
